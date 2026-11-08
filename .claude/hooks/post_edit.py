#!/usr/bin/env python3
"""PostToolUse hook: format edited C sources and re-run the host unit tests.

- *.c / *.h anywhere in the project: clang-format -i with the nearest .clang-format
  (skipped silently when clang-format is not installed; see requirements-dev.txt).
- firmware/, platform/, tests/ or CMakeLists.txt: scripts/host_tests.py --quiet. A failure
  exits with code 2, which feeds the build/test output back to Claude.
"""

import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

HOST_TESTED = ("firmware/", "platform/", "tests/", "CMakeLists.txt")


def main() -> None:
    try:
        payload = json.load(sys.stdin)
    except ValueError:
        return
    path = (payload.get("tool_input") or {}).get("file_path", "")
    root = Path(os.environ.get("CLAUDE_PROJECT_DIR", os.getcwd())).resolve()
    try:
        relative = Path(path).resolve().relative_to(root).as_posix()
    except ValueError:
        return  # outside the project

    if relative.endswith((".c", ".h")):
        venv = root / ".venv" / ("Scripts/clang-format.exe" if os.name == "nt" else "bin/clang-format")
        formatter = str(venv) if venv.is_file() else shutil.which("clang-format")
        if formatter:
            subprocess.run([formatter, "-i", "--style=file", path], cwd=root, check=False)

    if relative.startswith(HOST_TESTED):
        result = subprocess.run([sys.executable, str(root / "scripts" / "host_tests.py"), "--quiet"],
                                cwd=root, capture_output=True, text=True)
        if result.returncode != 0:
            sys.stderr.write(f"Host unit tests failed after editing {relative}:\n{result.stderr}{result.stdout}")
            sys.exit(2)


if __name__ == "__main__":
    main()
