
#!/usr/bin/env python3
"""Run NEBULA CTest suite."""

from __future__ import annotations

import argparse
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", default="build")
    parser.add_argument("--config", default="Release")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    ctest = shutil.which("ctest")
    if not ctest:
        print("[ERROR] CTest was not found. Install CMake.")
        return 1

    build_dir = (ROOT / args.build_dir).resolve()
    if not build_dir.is_dir():
        print("[ERROR] Build directory does not exist.")
        print("Run scripts/build.py first.")
        return 1

    command = [
        ctest,
        "--test-dir", str(build_dir),
        "-C", args.config,
        "--output-on-failure",
    ]

    if args.verbose:
        command.append("--verbose")

    print("[TEST]", " ".join(command), flush=True)
    result = subprocess.run(command, cwd=ROOT)

    if result.returncode == 0:
        print("[TEST] All registered tests passed.")
    else:
        print("[TEST] One or more tests failed.")

    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
