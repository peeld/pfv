# PFV — Next Chat Handoff (2026-10-06)

## Where things are

The PySide6 GUI has been replaced by a **C++ Qt app with embedded Python**,
built with core's `build.py` system (`core/docs/todo/BUILD_MIGRATION.md`). The
app lives at the root of the `pfv` repo; this folder (`pfv-public/`) is
its Python scripts folder.

- GUI: `src/` (C++), described in `docs/GUI.md` and `docs/DEVELOPMENT.md`.
- Backend: `pfv_app.py` (`dispatch(name, json)`), the only Python the GUI calls.
- Startup/self-check: `pfv_startup.py` (`python build.py run -- --python-check`).
- Python packages (boto3): root `app.json` `python.packages`.
- The repo/workspace view toggle, Workspace view and "Commit as new file"
  (the feature this file used to hand off) are done, in the C++ app.

The old PySide GUI (`pfv_gui_*.py`) was deleted on
2026-10-06. GUI work happens in C++ in `src/` at the repo root.

## Fixed during the port (2026-10-05)

- `pfv_state.make_record()` stored `vdir.resolve()`, an absolute path, as the
  vdir, which broke status/sync/checkin/abandon. Checkouts made before the fix
  must be checked out again.
- "Commit as new file" now runs `pfv.init()` before `pfv.commit()`.
- The Date column reads the `timestamp` meta key that `commit()` writes.
- S3 access keys typed into the dialog are no longer saved to `session.json`.

## What's next

`TODO.md` at the repo root is the list. Start there.

## Key files to read at the start of the next chat

- `TODO.md` (repo root)
- `docs/DEVELOPMENT.md` — architecture and how to add a command
- `src/mainwindow.cpp` — the window (for GUI work)
- `pfv-public/pfv_app.py` — the backend facade (for backend work)
- `docs/DOCS.md` — core library reference if you need API details
