#!/usr/bin/env python3
"""Build and run the host unit tests with whatever C compiler this machine has.

Order: $CC, gcc, clang, cc, then zig cc from the project .venv (requirements-dev.txt).
The build directory is build/host-local (separate from the CMake presets).

    python scripts/host_tests.py            # build + test, full output
    python scripts/host_tests.py --quiet    # print only on failure (used by the Claude hook)
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = ROOT / "build" / "host-local"
TOOLS_DIR = ROOT / "build" / "host-tools"
VENV_BIN = ROOT / ".venv" / ("Scripts" if os.name == "nt" else "bin")


def find_tool(name: str) -> str | None:
    """Find a tool on PATH or in the project .venv."""
    local = VENV_BIN / (f"{name}.exe" if os.name == "nt" else name)
    return shutil.which(name) or (str(local) if local.is_file() else None)


def zig_wrappers() -> dict[str, str]:
    """Create cc/ar/ranlib wrappers around `python -m ziglang` (zig bundles clang + libc)."""
    python = VENV_BIN / ("python.exe" if os.name == "nt" else "python")
    probe = subprocess.run([str(python), "-m", "ziglang", "version"], capture_output=True, text=True)
    if probe.returncode != 0:
        return {}
    TOOLS_DIR.mkdir(parents=True, exist_ok=True)
    tools = {}
    for tool, args in (("cc", "cc"), ("ar", "ar"), ("ranlib", "ranlib")):
        if os.name == "nt":
            path = TOOLS_DIR / f"zig-{tool}.cmd"
            extra = " -target x86_64-windows-gnu" if tool == "cc" else ""
            path.write_text(f'@"{python}" -m ziglang {args}{extra} %*\n', encoding="utf-8")
        else:
            path = TOOLS_DIR / f"zig-{tool}"
            path.write_text(f'#!/bin/sh\nexec "{python}" -m ziglang {args} "$@"\n', encoding="utf-8")
            path.chmod(0o755)
        tools[tool] = str(path)
    return tools


def configure_args() -> list[str]:
    """CMake cache arguments selecting the compiler."""
    if os.environ.get("CC"):
        return []
    for compiler in ("gcc", "clang", "cc"):
        if shutil.which(compiler):
            return [f"-DCMAKE_C_COMPILER={shutil.which(compiler)}"]
    zig = zig_wrappers()
    if not zig:
        sys.exit("Error: no host C compiler. Install gcc/clang or run: "
                 ".venv python -m pip install -r requirements-dev.txt")
    return [f"-DCMAKE_C_COMPILER={zig['cc']}", f"-DCMAKE_AR={zig['ar']}", f"-DCMAKE_RANLIB={zig['ranlib']}"]


def run(command: list[str], quiet: bool) -> None:
    """Run a step; on failure print its output and stop with exit code 2."""
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
    if not quiet:
        print(result.stdout, end="")
    if result.returncode != 0:
        print(result.stdout + result.stderr, file=sys.stderr)
        sys.exit(2)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--quiet", action="store_true", help="print only on failure")
    args = parser.parse_args()

    cmake = find_tool("cmake")
    ninja = find_tool("ninja")
    if not cmake or not ninja:
        sys.exit("Error: cmake and ninja are required (ninja: requirements-dev.txt).")
    if not (BUILD_DIR / "CMakeCache.txt").is_file():
        run([cmake, "-S", ".", "-B", str(BUILD_DIR), "-G", "Ninja", f"-DCMAKE_MAKE_PROGRAM={ninja}",
             "-DCMAKE_BUILD_TYPE=Debug"] + configure_args(), args.quiet)
    run([cmake, "--build", str(BUILD_DIR)], args.quiet)
    run([find_tool("ctest") or "ctest", "--test-dir", str(BUILD_DIR), "--output-on-failure"], args.quiet)
    if args.quiet:
        print("host unit tests passed")


if __name__ == "__main__":
    main()
