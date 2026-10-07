# tools

| Folder | What |
|---|---|
| repo root, `src/` | **PFV** (Peel File Versions): C++ Qt GUI with embedded Python |
| `python/` | the app's own Python: `pfv_app.py` (the backend the GUI calls), `pfv_startup.py` |
| `pfv-public/` | the PFV Python library and CLI (public submodule) |
| `docs/` | private docs: the app, its build, planning |
| `tests/` | `pfv_app` tests: `python -m unittest discover tests` |
| `tracking/` | Fleet PM tool: Django server + `pmtool` client (separate, not part of the app build) |
| `core/` | shared submodule: build system (`pd`), `PeelApp.cmake`, shared C++ |
| `web-license/` | licensing submodule (`peel_add_app(... LICENSE)`) |

Open work is in [`TODO.md`](TODO.md).

## PFV

A versioning tool for large binary files: a workspace (a local folder) is
linked to one or more repos (a local folder or S3), files are checked out,
edited and synced back as new versions. The library is documented in
`pfv-public/README.md`, the app in `docs/GUI.md`.

The GUI is C++ (`src/`). All PFV work happens in the embedded Python, through
one facade, `python/pfv_app.py` (`dispatch(name, json) -> json`), called
from `src/pfvBackend.cpp` on background threads. Architecture and how to add
a feature: `docs/DEVELOPMENT.md`.

`python/` and `pfv-public/` are both on the embedded Python's `sys.path`
(`python/` first) and are staged together into one `scripts/` folder, so a
file name must not be in both (configure fails if it is). Code that only
makes sense inside the app goes in `python/`; the library, and docs on using
and extending it, go in `pfv-public/`, which is public.

The app is built like every app on `core`'s build system: `app.json` +
`build.py` + `core/cmake/PeelApp.cmake`, with
[`core/docs/todo/BUILD_MIGRATION.md`](core/docs/todo/BUILD_MIGRATION.md) as the
reference and `core/docs/todo/build_migration/pfv.md` as this app's notes.

### Set up a PC (once)

```
git submodule update --init
cd core\python
python -m pd.machine discover --write
cd ..\..
python build.py doctor
```

`PEEL_CREDENTIALS` (or `credentials.json` in the repo root, gitignored) is
needed to download the build packages the first time.

### Build and run

```
python build.py configure                 # downloads Qt/Python/packages, pip-installs app.json python.packages
python build.py build
python build.py run                       # [-- <storage> <work tree>]
python build.py run -- --python-check     # embedded Python self-check
python build.py stage                     # self-contained app folder in build/<preset>/stage
python build.py run --staged -- --python-check
python build.py package                   # NSIS installer + portable zip, both verified
```

`python build.py list` shows the presets: `qtbld-6.11.1` (default),
`qtbld-6.11.1-debug`, and `official-6.9.0`, which builds but has no Python
and so no PFV backend.

### app.json

- `name` / `slug` / `target`: `PFV` / `pfv` / `PFV`. The name is where the
  license is stored, so it can't change after the first release.
- `qt.qtbld`: the Qt + Python + PySide build the app uses.
- `packages`: prebuilt C++ packages (sentry-native, OpenSSL).
- `python.packages`: pip packages for the embedded Python (`boto3` for S3),
  installed per preset into `build/<preset>/python-packages` and shipped in
  the stage's `python/Lib/site-packages`. Pin exact versions.
- `sentry`: project `pfv`, DSN still empty, so Sentry is off.
- `package.verify`: what the installer test runs.

`licenseConfig.h` (the license secret) and `credentials.json` are never
committed. Without `licenseConfig.h` the app builds with licensing off.
