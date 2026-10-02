"""Punto de entrada: python main.py --mode {sequential,threading,multiprocessing} ..."""

import multiprocessing
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "src"))

from cpubench.cli import main  # noqa: E402

if __name__ == "__main__":
    multiprocessing.freeze_support()
    sys.exit(main())
