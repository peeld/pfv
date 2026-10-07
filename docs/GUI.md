# PFV GUI

The PFV app: a C++ Qt GUI (`src/` at the repo root) with the PFV library
running in its embedded Python. It replaces the old PySide6 GUI, which has
been removed.

## Build and launch

From the repo root (one-time machine setup is in the root `README.md`):

```bash
python build.py configure            # fetches Qt/Python, pip-installs app.json python.packages
python build.py build
python build.py run                                       # reopens the last workspace
python build.py run -- C:/repos/project C:/work/project   # [storage] [work_tree]
python build.py run -- --python-check                     # check the embedded Python and exit
```

The S3 backend's `boto3` comes with the app (app.json `python.packages`),
so there's nothing to pip-install by hand.

## Interface

```
┌ File  View  Help ───────────────────────────────────────────────────┐
│ 1 Workspace  [recent workspaces ▾] [Open…] [Convert…] [New…]        │
│   C:\work\project                                                   │
│ 2 Repo Links [demo-repo ▾] [+ Add Link ▾] [Remove]                  │
│   C:\repos\project                                                  │
│                                             [Refresh] [🔄 Sync]     │
├─────────────────────────────────────┬───────────────────────────────┤
│ [Repo View] [Workspace View]        │ Version History               │
│ File        Version Size  Date Stat │ Ver │Type  │Lock│Del│Tag│Meta │
│ ▾ textures                          │  2  │binary│    │   │   │Al: …│
│     hero.psd   2   2.9 KB  …    ✓   │  1  │binary│    │   │   │Al: …│
│   scene.ma     1   3.9 KB  … ✎ Mod. │ [Checkout] [Abandon] [Delete] │
│                                     │ [Rename] [Undelete]           │
├─────────────────────────────────────┴───────────────────────────────┤
│ Log (timestamped; failures include the Python traceback)  [Clear]   │
└─────────────────────────────────────────────────────────────────────┘
```

- **Step 1, Workspace:** pick a recent workspace (from `~/.pfvconfig`), open
  an existing one, convert any folder into one (writes `.pfv/session.json`),
  or create a new blank one. The app reopens the last workspace at startup.
- **Step 2, Repo Links:** the repos in the workspace's `session.json`. *Add
  Link* links an existing local repo, creates a new local repo, or links an
  S3 repo. The first link added becomes the default. *Remove* only removes
  the link; no files are deleted.
- **Repo View** lists every versioned file (vdir) in the connected repo.
  vdirs with `/` show as folders. **Workspace View** lists the files under the
  work tree, with *In repo?*. Right-click an untracked file for **Commit as
  new file…**, which creates the vdir (named by its path in the work tree)
  and commits version 1.
- **Version History** shows the selected file's versions. The latest is blue,
  locked versions amber and deleted versions red.
- **Menus:** File (Open Workspace, Refresh F5, Exit), View (Script Editor,
  Ctrl+Shift+E), Help (About, License…, Send crash reports, when those are
  built in).

Lists and history load in the background, so a slow repo (S3) doesn't freeze
the window.

**Status indicators**

| Symbol | Meaning | Action |
|--------|---------|--------|
| ✓ | Up to date | — |
| ✎ Modified | Local changes | Click Sync |
| ⚠ CONFLICT | Both sides changed | Resolve, then sync |
| ↻ Repo changed | Repo updated since checkout | Checkout new version if needed |
| 🔒 | File locked | Wait or contact file holder |

## Workflow

**Checkout:** download a version to work on.
1. Select a file, click **Checkout**, pick a version (`latest`, `HEAD` or a
   number). The destination defaults to the file's place in the work tree.
2. The checkout is tracked in the workspace's `.pfv/` state, with the SHA at
   checkout, so later syncs can detect conflicts.

**Sync:** commit modified files back to the repo.
1. Click **🔄 Sync**.
2. PFV checks every tracked checkout. If any file is in conflict, nothing is
   committed and the conflicts are reported. Otherwise each modified file is
   checked in as a new version. Files whose repo moved on are skipped.

**Abandon:** select a checked-out file and click **Abandon Checkout**. This
forgets its checkout records without committing (the file itself is left
alone).

## Deletion and Renaming

Deletion and renaming are non-destructive: PFV creates a marker version (`.deleted` or `.renamed`) so history is fully preserved.

### Delete a file

**GUI:** Select file → click **🗑️ Delete** → enter message/reason → confirm.

**API:**
```python
from pfv_deletion import mark_deleted
new_ver = mark_deleted("archive.zip", storage=store, author="alice",
                       message="Project archived", reason="Completed Q1")
```

The new version carries a `.deleted` marker. The file appears in the history table with `Deleted: YES`. Previous versions remain accessible.

### Restore (undelete)

**GUI:** Select a deleted file → click **↶ Undelete** → optionally enter the version to restore from → confirm.

**API:**
```python
from pfv_deletion import undelete
new_ver = undelete("archive.zip", storage=store, author="bob",
                   message="Restoring for reprocessing",
                   restore_version=3)   # omit to auto-select last good version
```

Copies the content from the specified (or last good) version and creates a new non-deleted version.

### Rename a file

**GUI:** Select file → click **✏️ Rename** → enter new name and message → confirm.

**API — mark only:**
```python
from pfv_deletion import mark_renamed
new_ver = mark_renamed("old_project.mp4", "new_project.mp4",
                       storage=store, author="charlie",
                       message="Conform to naming standard")
```

**API — mark + migrate history to new vdir:**
```python
from pfv_deletion import migrate_renamed
new_ver = migrate_renamed("old_project.mp4", "new_project.mp4",
                          storage=store, author="charlie",
                          message="Renaming project")
```

`migrate_renamed` marks the old vdir as renamed, creates the new vdir, and copies the latest version content into it.

### Query helpers

```python
from pfv_deletion import (is_deleted, is_renamed, get_renamed_to,
                           list_deleted_files, list_renamed_files)

is_deleted("archive.zip", storage=store)          # → bool
is_renamed("old_project.mp4", storage=store)      # → bool
get_renamed_to("old_project.mp4", storage=store)  # → "new_project.mp4" or None
list_deleted_files(storage=store)                 # → [{vdir, author, deleted_at, reason, ...}]
list_renamed_files(storage=store)                 # → [{old_vdir, new_name, author, renamed_at, ...}]
```

### Repository layout after operations

```
# After mark_deleted:
archive.zip/
├── latest → 5
├── 1 … 4          (history preserved)
├── 5.deleted      (empty marker file)
└── 5.meta         {"deleted": true, "deleted_by": "alice", ...}

# After migrate_renamed:
old_project.mp4/
├── latest → 3
└── 3.renamed      (contains "new_project.mp4")

new_project.mp4/
├── latest → 1
└── 1              (copied from old v2)
```


## S3 repos

**+ Add Link ▾ → Link remote S3 repo…** asks for bucket, prefix, region,
endpoint (for S3-compatible stores) and optionally credentials. Leave the
credentials blank to use the environment or `~/.aws` (the standard boto3
chain). Access keys typed into the dialog are used for that session only and
are **not** saved to `session.json`, so with typed keys you'll be asked again
next time. For a saved setup, use an AWS profile or a PFV credential profile
(`cred_profile` in `session.json`, from `pfv_credentials.CredentialStore`).

Note: the dialog's *AWS profile* field is saved as `profile`, which
`PFVSession.storage_kwargs()` doesn't read back yet (see `TODO.md`).

## Scripting

View > Script Editor (Ctrl+Shift+E) runs Python inside the app:

```python
import pfvgui, pfv_app, json
win = pfvgui.mainWindow()          # the C++ main window
win.workTree(), win.storageLocation(), win.currentVdir()
win.openWorkspace(r"C:\work\project")
win.refresh()
json.loads(pfv_app.dispatch("history", json.dumps({"vdir": "scene.ma"})))
```

`pfv`, `pfv_storage` and the other modules can be imported directly too.

## Troubleshooting

| Problem | Cause | Fix |
|-------|-------|-----|
| "Storage Error" when connecting | Path doesn't exist, no permissions, or S3 credentials | Check the path or credentials; the log has the traceback |
| "No repo linked to this workspace yet" | Workspace has no repo links | Add one under step 2 |
| "Error loading vdir …" in the log | Malformed vdir (no `latest`) | That vdir is skipped; fix it in storage |
| Sync: "Found N conflicted file(s)" | Repo advanced and the file changed locally | Checkout the new version, re-apply changes |
| "Python failed to start" in the log | Broken build or stage | `python build.py run -- --python-check` |
| "S3Backend requires boto3" | Built without app.json `python.packages` | `python build.py configure` |
| "Cannot undelete: no previous version" | Only version is the deletion marker | No restorable content exists |
| Sync/abandon don't see a file checked out before 2026-10-06 | Old checkout records stored an absolute path as the vdir (fixed in `pfv_state.make_record`) | Check the file out again |

## Architecture

See `DEVELOPMENT.md`. In short: C++ widgets (`src/`) call
`pfv-public/pfv_app.py`'s `dispatch(name, json)` through `src/pfvBackend.cpp`;
`pfv_app` calls the PFV library; storage I/O goes through
`pfv_storage.StorageBackend`, checkout tracking through
`pfv_state.StateBackend`.
