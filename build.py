#!/usr/bin/env python3
"""Build entry point for this app. All logic lives in core/python/pd/project.py;
this file only finds it. Run `python build.py --help`.

First time on a machine:
    git submodule update --init
    python -m pd.machine discover --write   (from core/python, or via: python build.py doctor)
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "core" / "python"))

from pd import project  # noqa: E402

if __name__ == "__main__":
    sys.exit(project.main(ROOT))
