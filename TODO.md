# TODO

Open work on PFV and its build, as of 2026-10-06. Move items to "Done" (with
the date) when they land.

## Commit the move of `pfv_app.py` / `pfv_startup.py` to `python/`

- [ ] core (`build-migration`): `PYTHON_SCRIPTS` as a list,
  `PEEL_APP_PYTHON_DIRS`, the name-clash and empty-folder checks, `doctor`
  checking every submodule, docs. Push, then bump `core` here.
- [ ] pfv-public: the two files removed, README rows and the app-packages
  paragraph removed, `demo_setup.py` points at the CLI. Push, then bump it here.
- [ ] This repo: `python/`, `CMakeLists.txt`, `src/` comments, `tests/`,
  `README.md`, `docs/`, `TODO.md`, and both submodule bumps, in one commit.

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
- [ ] Sentry: create project `pfv`, put its DSN in app.json `sentry.dsn`,
  test with `--sentry-check`, then add `--sentry-check` to `package.verify`.
- [ ] Licensing: set up a `pfv` license product and write `licenseConfig.h`;
  test `--license-check`, then add it to `package.verify`.
- [ ] Confirm the app name `PFV` and slug `pfv` before the first release (the
  license is stored under the name), and create product `pfv` on the files
  API for `build.py release`. `release --dry-run` stops at the
  `PEEL_CREDENTIALS` check, before any build.
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
- 2026-10-06: `python build.py package`: installer and portable zip build and
  pass their verify (install, `--python-check`, uninstall).
- 2026-10-06: `pfv_app.py` and `pfv_startup.py` moved out of the public
  library into `python/`; core's `PYTHON_SCRIPTS` takes several folders.
