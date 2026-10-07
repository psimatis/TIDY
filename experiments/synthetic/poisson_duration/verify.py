#!/usr/bin/env python3
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

from synthetic_lib import load_frame, verify

CONTEXT = {"Dataset", "Extent", "Lambda", "Domain"}

if __name__ == "__main__":
    verify(load_frame(HERE / "logs", CONTEXT, "Lambda"), "Lambda")
