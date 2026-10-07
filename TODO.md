# TODO

Open work on PFV and its build, as of 2026-10-06. Move items to "Done" (with
the date) when they land.

## Before the first commit

- [ ] **Decide how `fileversions/` is tracked.** It's a separate local git
  repo (one commit, no remote) with uncommitted changes, including the
  2026-10-05 fixes and `pfv_app.py` / `pfv_startup.py`. The top-level repo
  can't build from a fresh clone until this is settled. Options: make it a
  submodule (it needs a remote), or merge it into this repo.
- [ ] **Commit the `core` changes** on core's `build-migration` branch, then
  bump the `core` submodule here: hidden files left out of the staged scripts
  folder, app.json `python.packages`, and the BUILD_MIGRATION /
  `build_migration/pfv.md` docs.
- [ ] Commit this repo: app files (`app.json`, `build.py`, `CMakeLists.txt`,
  `src/`, `README.md`, `TODO.md`, `.gitignore`), `web-license` submodule.
  `tracking/` is untracked too: commit it separately, or leave it out.

## Test the C++ GUI by hand

Only the repo view and history have been checked in the running app.

- [ ] Workspace step: Open, Convert, New, switching via the recent list,
  reopening the last workspace at startup.
- [ ] Repo links: link existing local repo, create new local repo, link S3,
  Remove, switching between links.
- [ ] Checkout (default destination, a specific version), Abandon, Sync
  (modified, conflict, repo moved), Delete, Rename, Undelete.
- [ ] Workspace view: untracked files, right-click → Commit as new file.
- [ ] Script Editor: `pfvgui.mainWindow()`, `pfv_app.dispatch(...)`.
- [ ] S3 against a real bucket: env/profile credentials, typed keys.
- [ ] Then delete the old PySide GUI (`fileversions/pfv_gui_*.py`).

## Build and release

- [ ] `-debug` preset: build, `--python-check`, and check whether boto3
  (pure Python) works under `python_d.exe`.
- [ ] `python build.py package`: installer builds and passes its silent
  install test.
- [ ] Sentry: create project `pfv`, put its DSN in app.json `sentry.dsn`,
  test with `--sentry-check`, then add `--sentry-check` to `package.verify`.
- [ ] Licensing: set up a `pfv` license product and write `licenseConfig.h`;
  test `--license-check`, then add it to `package.verify`.
- [ ] Confirm the app name `PFV` and slug `pfv` before the first release (the
  license is stored under the name), and create product `pfv` on the files
  API for `build.py release`.
- [ ] Don't ship stray files from the scripts folder: `foo/` (test data),
  `docs/`, `demo_setup.py`, `localtest.py`, the old `pfv_gui_*.py`. E.g. move
  them out of `fileversions/`, or give `peel_add_app` an exclude list.
- [ ] Windows version resource on `PFV.exe` (core's list, BUILD_MIGRATION §0).

## PFV library and backend

- [ ] **Credential profiles don't work in the app.** `pfv_credentials` needs
  `cryptography` (not in app.json `python.packages`) and asks for its
  passphrase with `getpass` on a console the GUI doesn't have. Needs a
  passphrase callback the GUI can answer with a dialog, plus the package.
- [ ] The S3 dialog's *AWS profile* is saved as `profile` in `session.json`,
  but `PFVSession.storage_kwargs()` only reads back `cred_profile`, `region`
  and `endpoint_url`, so the profile is lost when the workspace is reopened.
- [ ] Checkout records written before the `make_record` fix (2026-10-05) have
  an absolute path as the vdir. Either a one-off migration in `pfv_state`, or
  tell users to check those files out again.
- [ ] Automated tests for `pfv_app.dispatch()` (temp workspace + local repo;
  the smoke test from the port is a starting point).
- [ ] The Redis state backend needs `redis` in `python.packages` if the app
  should support it.

## Feature ideas (not started)

- [ ] Lock / unlock from the GUI (`pfv.lock` / `pfv.unlock`).
- [ ] Rename with history migration (`pfv_deletion.migrate_renamed`), not just
  the marker.
- [ ] Context menu on the history table (checkout this version).
- [ ] Filter box for the file tree.

## Done

- 2026-10-05: C++ GUI port of the PySide GUI on core's build system
  (`PYTHON SENTRY LICENSE SCRIPT_EDITOR`, shiboken module `pfvgui`).
- 2026-10-05: fixed `make_record` storing absolute vdirs, "Commit as new
  file" without `init`, the empty Date column, S3 keys saved in plain text.
- 2026-10-05: staged scripts folder skips hidden entries (core).
- 2026-10-06: app.json `python.packages` (core), boto3 for S3.
- 2026-10-06: docs updated for the C++ app.
