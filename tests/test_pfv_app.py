"""
Tests for pfv_app.dispatch(), the JSON boundary the C++ GUI calls.

Each test gets a temp workspace and a local repo, and ~/.pfvconfig is
redirected to the temp dir. Run from the repo root:

    python -m unittest discover tests
"""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path[:0] = [str(ROOT / "python"), str(ROOT / "pfv-public")]  # as in the app

import pfv_app  # noqa: E402
import pfv_config  # noqa: E402


def call(cmd: str, /, **args):
    """dispatch() and return the result, failing the test on an error reply."""
    reply = json.loads(pfv_app.dispatch(cmd, json.dumps(args)))
    if not reply["ok"]:
        raise AssertionError(f"{cmd} failed: {reply['error']}\n{reply['traceback']}")
    return reply["result"]


def call_error(cmd: str, /, **args) -> str:
    """dispatch() and return the error message, failing if the call succeeded."""
    reply = json.loads(pfv_app.dispatch(cmd, json.dumps(args)))
    if reply["ok"]:
        raise AssertionError(f"{cmd} unexpectedly succeeded: {reply['result']}")
    return reply["error"]


class DispatchTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.tmp = Path(tmp.name)
        self.ws = self.tmp / "ws"
        self.repo = self.tmp / "repo"
        self.repo.mkdir()

        patcher = mock.patch.object(pfv_config, "DEFAULT_CONFIG_PATH", self.tmp / ".pfvconfig")
        patcher.start()
        self.addCleanup(patcher.stop)

        # pfv_app keeps module-level state; reset it between tests
        pfv_app._work_tree, pfv_app._storage, pfv_app._location = None, None, ""

    def open_and_link(self):
        call("init_workspace", path=str(self.ws))
        call("open_workspace", path=str(self.ws))
        call("add_repo", name="local", location=str(self.repo))
        call("connect_repo")

    def commit_file(self, rel: str, data: bytes) -> int:
        path = self.ws / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        return call("commit_new", rel_path=rel)["version"]

    # -- boundary --------------------------------------------------------

    def test_unknown_command(self):
        self.assertIn("Unknown PFV command", call_error("nope"))

    def test_bad_arguments(self):
        reply = json.loads(pfv_app.dispatch("history", '{"wrong": 1}'))
        self.assertFalse(reply["ok"])
        self.assertIn("traceback", reply)

    def test_empty_args(self):
        self.assertIn("deletion", call("info"))
        self.assertEqual(call("info"), json.loads(pfv_app.dispatch("info", ""))["result"])

    def test_needs_workspace_and_repo(self):
        self.assertIn("No workspace open", call_error("add_repo", name="x", location="y"))
        self.assertIn("No repo connected", call_error("repo_view"))

    # -- workspaces and repo links ---------------------------------------

    def test_workspace_lifecycle(self):
        self.assertEqual(call("init_workspace", path=str(self.ws)), {"created": True})
        self.assertEqual(call("init_workspace", path=str(self.ws)), {"created": False})
        opened = call("open_workspace", path=str(self.ws))
        self.assertEqual(Path(opened["path"]), self.ws.resolve())
        self.assertEqual([Path(w["path"]) for w in call("workspaces")], [self.ws.resolve()])
        self.assertEqual(call("repos"), [])
        self.assertEqual(call("connect_repo"), {"location": None})

    def test_repo_links(self):
        call("init_workspace", path=str(self.ws))
        call("open_workspace", path=str(self.ws))
        call("add_repo", name="a", location=str(self.repo))
        call("add_repo", name="b", location=str(self.tmp / "other"))
        links = {r["name"]: r for r in call("repos")}
        self.assertTrue(links["a"]["default"])      # first link is the default
        self.assertFalse(links["b"]["default"])

        self.assertEqual(call("remove_repo", name="a"), {"removed": True})
        self.assertEqual([(r["name"], r["default"]) for r in call("repos")], [("b", True)])

    def test_s3_link_keeps_profile_drops_secrets(self):
        call("init_workspace", path=str(self.ws))
        call("open_workspace", path=str(self.ws))
        call("add_repo", name="s3", location="s3://bucket/prefix", kwargs={
            "region": "us-west-2",
            "profile": "studio",
            "aws_access_key_id": "AKIAEXAMPLE",
            "aws_secret_access_key": "secret",
        })
        text = (self.ws / ".pfv" / "session.json").read_text(encoding="utf-8")
        self.assertNotIn("AKIAEXAMPLE", text)
        self.assertNotIn("secret", text)

        from pfv_session import PFVSession
        location, kwargs = PFVSession.load(self.ws).storage_kwargs("s3")
        self.assertEqual(location, "s3://bucket/prefix")
        self.assertEqual(kwargs, {"region": "us-west-2", "profile": "studio"})

    # -- views and actions -----------------------------------------------

    def test_commit_new_and_views(self):
        self.open_and_link()
        (self.ws / "notes.txt").write_text("untracked")
        (self.ws / ".hidden").write_text("skipped")
        self.assertEqual(self.commit_file("art/a.psd", b"one"), 1)

        files = {f["rel_path"]: f["in_repo"] for f in call("workspace_view")["files"]}
        self.assertEqual(files, {"art/a.psd": True, "notes.txt": False})

        view = call("repo_view")
        self.assertEqual(view["errors"], [])
        self.assertEqual([(f["vdir"], f["version"]) for f in view["files"]], [("art/a.psd", 1)])
        self.assertEqual(view["files"][0]["size"], 3)

        hist = call("history", vdir="art/a.psd")
        self.assertEqual(hist["latest"], 1)
        self.assertEqual([s["version"] for s in hist["slots"]], [1])

    def test_commit_new_missing_file(self):
        self.open_and_link()
        self.assertIn("Cannot find", call_error("commit_new", rel_path="missing.txt"))

    def test_checkout_modify_sync(self):
        self.open_and_link()
        self.commit_file("a.txt", b"v1")
        dest = self.ws / "out" / "a.txt"
        result = call("checkout", vdir="a.txt", version="latest", dest=str(dest))
        self.assertEqual(result["version"], 1)
        self.assertEqual(dest.read_bytes(), b"v1")

        self.assertEqual(call("sync_plan"), {"tracked": 1, "conflicts": [], "modified": [], "repo_moved": []})

        dest.write_bytes(b"v2")
        plan = call("sync_plan")
        self.assertEqual([Path(p) for p in plan["modified"]], [dest])
        self.assertEqual(call("checkin", dest=plan["modified"][0]), {"version": 2})
        self.assertEqual(call("history", vdir="a.txt")["latest"], 2)
        self.assertEqual(call("sync_plan")["modified"], [])

    def test_checkout_specific_version(self):
        self.open_and_link()
        self.commit_file("a.txt", b"v1")
        dest = self.ws / "a.txt"
        call("checkout", vdir="a.txt", version="latest", dest=str(dest))
        dest.write_bytes(b"v2")
        call("checkin", dest=str(dest))

        old = self.ws / "old" / "a.txt"
        self.assertEqual(call("checkout", vdir="a.txt", version="1", dest=str(old))["version"], 1)
        self.assertEqual(old.read_bytes(), b"v1")

    def test_conflict(self):
        self.open_and_link()
        self.commit_file("a.txt", b"v1")
        first = self.ws / "one" / "a.txt"
        second = self.ws / "two" / "a.txt"
        call("checkout", vdir="a.txt", version="latest", dest=str(first))
        call("checkout", vdir="a.txt", version="latest", dest=str(second))
        first.write_bytes(b"from one")
        call("checkin", dest=str(first))
        second.write_bytes(b"from two")

        plan = call("sync_plan")
        self.assertEqual([Path(p) for p in plan["conflicts"]], [second])
        self.assertEqual(plan["modified"], [])

    def test_abandon(self):
        self.open_and_link()
        self.commit_file("a.txt", b"v1")
        dest = self.ws / "out" / "a.txt"
        call("checkout", vdir="a.txt", version="latest", dest=str(dest))
        self.assertEqual([Path(p) for p in call("abandon", vdir="a.txt")["dests"]], [dest])
        self.assertEqual(call("sync_plan")["tracked"], 0)

    @unittest.skipUnless(pfv_app.HAS_DELETION_SUPPORT, "pfv_deletion not available")
    def test_delete_undelete_rename(self):
        self.open_and_link()
        self.commit_file("a.txt", b"v1")
        self.assertEqual(call("is_deleted", vdir="a.txt"), {"deleted": False})
        call("delete", vdir="a.txt", message="gone")
        self.assertEqual(call("is_deleted", vdir="a.txt"), {"deleted": True})
        call("undelete", vdir="a.txt", message="back")
        self.assertEqual(call("is_deleted", vdir="a.txt"), {"deleted": False})
        self.assertIsInstance(call("rename", vdir="a.txt", new_name="b.txt", message="mv")["version"], int)


if __name__ == "__main__":
    unittest.main()
