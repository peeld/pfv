# TODO

Open work on PFV and its build, as of 2026-10-06. Move items to "Done" (with
the date) when they land.

## Publish

- [ ] **Push, so a fresh clone builds.** `pfv-public` `main` is 1 commit
  ahead of `origin` (the commit this repo points at), and this repo's
  `main` has never been pushed (`origin/main` is gone).
- [ ] Commit and push core's `PYTHON_SCRIPTS_EXCLUDE` change
  (`cmake/PeelApp.cmake`, `build-migration`), then bump `core` here
  together with the `CMakeLists.txt` that uses it.
- [ ] Commit `pfv-public`'s `storage_kwargs()` profile fix and bump it here.
- [ ] `pfv-public` has an untracked `docs/` and no `.gitignore`
  (`__pycache__/` shows up): commit or ignore.

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
- [ ] Windows version resource on `PFV.exe` (core's list, BUILD_MIGRATION §0).

## PFV library and backend

- [ ] **Credential profiles don't work in the app.** `pfv_credentials` needs
  `cryptography` (not in app.json `python.packages`) and asks for its
  passphrase with `getpass` on a console the GUI doesn't have. Needs a
  passphrase callback the GUI can answer with a dialog, plus the package.
- [ ] Checkout records written before the `make_record` fix (2026-10-05) have
  an absolute path as the vdir. Either a one-off migration in `pfv_state`, or
  tell users to check those files out again. A migration would have to guess:
  the old code resolved the vdir against an unknown cwd, so the only way back
  is matching the path's tail against the vdirs in storage.
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
- 2026-10-06: deleted the old PySide GUI (`pfv_gui_*.py`).
- 2026-10-06: S3 *AWS profile* survives reopening the workspace
  (`storage_kwargs()` reads back `profile`).
- 2026-10-06: `tests/test_pfv_app.py`, unittest suite for
  `pfv_app.dispatch()` (`python -m unittest discover tests`).
- 2026-10-06: `peel_add_app(... PYTHON_SCRIPTS_EXCLUDE ...)` (core); `docs/`,
  `demo_setup.py`, `localtest.py` no longer staged.
