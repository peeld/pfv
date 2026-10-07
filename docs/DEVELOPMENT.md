# PFV GUI — Developer Guide

## Architecture

```
C++ Qt GUI (src/, repo root)                      embedded CPython (qtbld 3.14)
┌───────────────────────────────┐                 ┌──────────────────────────────┐
│ MainWindow  (mainwindow.*)    │                 │ pfv_app.py   dispatch(name,  │
│ FileTreeModel (fileTreeModel.*)│  JSON in/out    │              args_json)      │
│ dialogs.*                     │ ──────────────▶ │   │                          │
│ PfvBackend::call / callAsync  │  GIL per call   │   ▼                          │
│   (pfvBackend.*)              │                 │ pfv · pfv_session · pfv_config│
└───────────────────────────────┘                 │ pfv_deletion · pfv_storage   │
        ▲  pfvgui module (shiboken,               │ pfv_state                    │
        │  src/bindings.*)                        └──────────────────────────────┘
        └── pfv_startup.py, Script Editor ─────────────────┘
```

- **The GUI is C++ only.** It never holds a Python object. Every PFV
  operation is `PfvBackend::call("<command>", QJsonObject)`, which takes the
  GIL, calls `pfv_app.dispatch()` and parses the JSON reply into a
  `PfvResult` (`ok`, `value`, `error`, `traceback`).
- **`pfv_app.py` is the only Python the GUI calls.** It holds the current
  work tree and storage, turns PFV objects into plain dicts, and returns raw
  values (bytes, ISO dates, epoch times); the GUI formats them. `dispatch()`
  never raises: failures come back as `{"ok": false, "error", "traceback"}`.
- **Threads:** `PfvBackend::callAsync()` / `runAsync()` run on a private
  QThreadPool and deliver the result on the GUI thread (or drop it if the
  receiver is gone). The repo view, workspace view, history and sync are
  async; short actions (checkout, delete, ...) call synchronously behind a
  wait cursor. `~MainWindow` waits for the pool before Python shuts down.
  Each `pfv_app` command reads the current storage once at the start, so a
  command already running keeps its storage if the GUI connects elsewhere.
- **Stale replies:** list and history loads carry a request counter
  (`m_listRequest`, `m_historyRequest`); a reply for an older request is
  ignored.
- **Startup** (`src/main.cpp`): Sentry → license → `appPythonStart()` (registers
  the `pfvgui` module, starts Python, runs `pfv_startup.install()`) →
  `MainWindow::startup()`. Shutdown: window deleted (waits for the pool) →
  `appPythonStop()` → Sentry stopped.
- **Build:** `peel_add_app(PFV PYTHON PYTHON_SCRIPTS pfv-public SENTRY LICENSE
  SCRIPT_EDITOR ...)` in the root `CMakeLists.txt`; see
  `core/docs/todo/BUILD_MIGRATION.md`. `pfv-public/` is the scripts folder,
  so the CLI and the app run the same modules.

### Files

| File | What |
|---|---|
| `src/main.cpp` | startup/shutdown order, `--python-check` / `--license-check` / `--sentry-check`, dark stylesheet |
| `src/mainwindow.*` | the window: workspace/repo steps, views, history, actions, sync, log |
| `src/fileTreeModel.*` | one model, two modes (Repo: 5 columns, Workspace: 4); splits `/` paths into folders |
| `src/dialogs.*` | Checkout, Delete, Rename, Undelete, NewFolder (workspace / local repo), S3 |
| `src/pfvBackend.*` | the C++ → `pfv_app` bridge |
| `src/appPython.*`, `src/bindings.*` | Python startup, the shiboken `pfvgui` module |
| `pfv-public/pfv_app.py` | backend facade, the command table `_COMMANDS` |
| `pfv-public/pfv_startup.py` | `install()` at startup, `check()` for `--python-check` |

## Adding a Feature

### 1. Add a backend command

Write a function in `pfv_app.py` that takes JSON-able keyword arguments and
returns JSON-able data, then add it to `_COMMANDS`:

```python
def lock(vdir: str, reason: str = "") -> dict:
    _, storage = _need_storage()
    pfv.lock(vdir, storage=storage, holder=_author(), reason=reason or None)
    return {"locked": True}
```

Test it without the GUI, with any Python that has the PFV modules:

```python
import json, pfv_app
pfv_app.dispatch("open_workspace", json.dumps({"path": r"C:\work\project"}))
pfv_app.dispatch("connect_repo", "{}")
print(pfv_app.dispatch("lock", json.dumps({"vdir": "scene.ma"})))
```

### 2. Call it from the GUI

Synchronous (quick operations):

```cpp
const PfvResult r = PfvBackend::call(QStringLiteral("lock"), args({{"vdir", m_currentVdir}}));
if (!r.ok) {
    reportError(tr("Lock Error"), r);   // logs the traceback, shows a message box
    return;
}
refresh();
```

Asynchronous (anything that may be slow, e.g. walks storage):

```cpp
PfvBackend::callAsync(QStringLiteral("repo_view"), {}, this, [this](const PfvResult &r) {
    // runs on the GUI thread; skipped if `this` was deleted
});
```

Several calls in a row with progress, as `syncFiles()` does: `runAsync()`
with a work lambda (pool thread, plain `PfvBackend::call()`s) and a done
lambda (GUI thread); post progress with `QMetaObject::invokeMethod(...,
Qt::QueuedConnection)`.

### 3. Optionally expose it to Python

Public `MainWindow` methods with plain Qt types are wrapped into `pfvgui`
automatically (shiboken parses `src/mainwindow.h`). Keep implementation
details private, and forward-declare types the header doesn't need.

## Adding a Storage Backend

1. Subclass `StorageBackend` in `pfv_storage.py` and implement the eight abstract methods (`exists`, `read_bytes`, `write_bytes`, `delete`, `list_keys`, `download_to`, `upload_from`, `copy`).
2. Add a URL scheme to `open_storage()`.
3. If it needs a third-party package, add it to `python.packages` in the
   repo root's `app.json` (pinned), so the build installs and ships it.

The only required contract: `write_bytes(key, data)` followed by `read_bytes(key)` returns the same bytes.

## Adding a State Backend

1. Subclass `StateBackend` in `pfv_state.py` and implement `upsert`, `get`, `remove`, `list_all`.
2. Add a case to `open_backend()`.

`CheckoutRecord.vdir` is the logical vdir name (a storage key such as
`textures/hero.psd`), never a filesystem path.

## Testing

- **Backend:** drive `pfv_app.dispatch()` from plain Python against a temp
  workspace and local repo (see the snippet above; `init_workspace`,
  `open_workspace`, `connect`, `commit_new`, `checkout`, `sync_plan`,
  `checkin`, ...). No Qt needed.
- **Embedded Python and packaging:** `python build.py run -- --python-check`
  and, after `python build.py stage`, `python build.py run --staged --
  --python-check`. It fails if `pfv_app` can't answer, `install()` didn't run,
  or a second Qt/OpenSSL is loaded, and it reports boto3.
- **GUI:** `demo_setup.py` creates a test repository; open it with
  `python build.py run -- <repo> <work tree>`.

There are no automated tests yet (see `TODO.md`).

## Debugging

- Every failed command logs its Python traceback in the log panel.
- The Script Editor (Ctrl+Shift+E) runs Python in the app's interpreter:
  inspect `pfv_app._storage`, `pfv_app._work_tree`, or call
  `pfv_app.dispatch(...)` directly.
- C++: open `build/<preset>/PFV.slnx` (or the folder, with the generated
  `CMakeUserPresets.json`) in Visual Studio; F5 finds Qt and Python.
