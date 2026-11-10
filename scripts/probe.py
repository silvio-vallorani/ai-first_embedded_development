#!/usr/bin/env python3
"""SWD probe backends for programming and reading the MIMXRT1189 boards (ADR-0017).

SEGGER J-Link is the reference probe (default); NXP LinkServer is kept as an alternative.
Selection: PROBE_TOOL=jlink|linkserver, or --probe-tool on the calling scripts.

    python scripts/probe.py selftest      # check the generated J-Link command files

Environment:
    JLINK_INSTALL_DIR   J-Link installation (default: PATH, then C:/Program Files/SEGGER/JLink*)
    JLINK_DEVICE        J-Link device name (default MIMXRT1189xxx8_M33, as in the SDK board runner)
    JLINK_SPEED         SWD speed in kHz (default 1000)
    JLINK_ATTEMPTS      J-Link sessions tried before failing (default 3)
    LINKSERVER          LinkServer executable; LINKSERVER_DEVICE device/board string
"""

from __future__ import annotations

import glob
import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SDK_ROOT = Path(os.environ.get("MCUXPRESSO_SDK", ROOT.parent / "mcuxpresso-sdk")).resolve()
# NXP's CM33 connect/reset script for the RT1180 (ROM trap, caches); SoC-specific, valid for every MIMXRT1189 board.
JLINK_SCRIPT = SDK_ROOT / "mcuxsdk" / "examples" / "_boards" / "evkmimxrt1180" / "jlinkscript" / "evkmimxrt1180_cm33.jlinkscript"
JLINK_DEVICE = os.environ.get("JLINK_DEVICE", "MIMXRT1189xxx8_M33")
# 1 MHz: on the AM254-4 (Tag-Connect, J-Link V10) 4 MHz connects failed intermittently, 1 MHz never did.
JLINK_SPEED = os.environ.get("JLINK_SPEED", "1000")
JLINK_ATTEMPTS = int(os.environ.get("JLINK_ATTEMPTS", "3"))
# The EVK flash configuration also fits the AM254-4 (same W25Q128JW on FlexSPI1 port A).
LINKSERVER_DEVICE = os.environ.get("LINKSERVER_DEVICE", "MIMXRT1189:MIMXRT1180-EVK")
TOOLS = ("jlink", "linkserver")


def default_tool() -> str:
    tool = os.environ.get("PROBE_TOOL", "jlink")
    if tool not in TOOLS:
        sys.exit(f"Error: PROBE_TOOL must be one of {', '.join(TOOLS)}.")
    return tool


def _find(env: str, names: list[str], patterns: list[str], hint: str) -> str:
    candidates = [os.environ.get(env, "")] + [shutil.which(name) or "" for name in names]
    for pattern in patterns:
        candidates += sorted(glob.glob(pattern), reverse=True)
    for candidate in candidates:
        if candidate and Path(candidate).is_file():
            return candidate
    sys.exit(f"Error: {hint}")


def jlink_exe() -> str:
    """SEGGER J-Link Commander; PATH is used only for a SEGGER path (the JDK ships an unrelated jlink.exe)."""
    install = os.environ.get("JLINK_INSTALL_DIR", "")
    candidates = [os.environ.get("JLINK_EXE", "")]
    if install:
        candidates += [f"{install}/JLink.exe", f"{install}/JLinkExe"]
    for pattern in ("C:/Program Files/SEGGER/JLink*/JLink.exe", "C:/Program Files (x86)/SEGGER/JLink*/JLink.exe",
                    "/opt/SEGGER/JLink*/JLinkExe", "/Applications/SEGGER/JLink*/JLinkExe"):
        candidates += sorted(glob.glob(pattern), reverse=True)
    candidates += [path for path in (shutil.which("JLinkExe"), shutil.which("JLink.exe")) if path and "SEGGER" in path]
    for candidate in candidates:
        if candidate and Path(candidate).is_file():
            return candidate
    sys.exit("Error: SEGGER J-Link Commander not found. Install the J-Link software or set JLINK_INSTALL_DIR.")


def linkserver_exe() -> str:
    return _find("LINKSERVER", ["LinkServer"], ["C:/nxp/LinkServer_*/LinkServer.exe", "/usr/local/LinkServer*/LinkServer"],
                 "LinkServer not found. Install NXP LinkServer or set LINKSERVER.")


def jlink_program_commands(items: list[tuple[Path, int | None]], erase_all: bool, reset: bool) -> list[str]:
    """J-Link Commander commands that program ELF/HEX (own addresses) or BIN (given address) files."""
    # Without EnableEraseAllFlashBanks, "erase" skips the external QSPI NOR (seen on hardware).
    commands = ["r", "h"] + (["exec EnableEraseAllFlashBanks", "erase"] if erase_all else [])
    for path, address in items:
        commands.append(f"loadfile \"{path}\"" + ("" if address is None else f" 0x{address:08X}"))
    return commands + (["r", "g"] if reset else []) + ["exit"]


def jlink_dump_commands(address: int, size: int, output: Path) -> list[str]:
    """Attach without reset (XIP already configured by the boot), halt, save, resume."""
    return ["h", f"savebin \"{output}\" 0x{address:08X} 0x{size:X}", "g", "exit"]


def _run(command: list[str]) -> None:
    print("+", " ".join(str(part) for part in command), flush=True)
    if subprocess.run([str(part) for part in command]).returncode != 0:
        sys.exit("Error: the probe command failed (see its output above).")


def _jlink(commands: list[str], serial: str | None) -> None:
    if not JLINK_SCRIPT.is_file():
        sys.exit(f"Error: {JLINK_SCRIPT} not found. Run scripts/setup_sdk first.")
    with tempfile.TemporaryDirectory() as tmp:
        command_file = Path(tmp) / "commands.jlink"
        command_file.write_text("\n".join(commands) + "\n", encoding="utf-8")
        print("J-Link commands:", "; ".join(commands), flush=True)
        command = [jlink_exe(), "-NoGui", "1", "-ExitOnError", "1", "-AutoConnect", "1",
                   "-Device", JLINK_DEVICE, "-If", "SWD", "-Speed", JLINK_SPEED,
                   "-JLinkScriptFile", JLINK_SCRIPT] + (["-USB", serial] if serial else []) + ["-CommandFile", command_file]
        # Whole-session retry: connects intermittently fail with "Failed to power up DAP". Program
        # sessions restart from a reset and dumps only read, so repeating a session is safe.
        for attempt in range(1, JLINK_ATTEMPTS + 1):
            print("+", " ".join(str(part) for part in command), flush=True)
            if subprocess.run([str(part) for part in command]).returncode == 0:
                return
            print(f"J-Link session failed (attempt {attempt}/{JLINK_ATTEMPTS}).", flush=True)
            time.sleep(3)
        sys.exit("Error: the J-Link session failed (see its output above).")


def program(items: list[tuple[Path, int | None]], erase_all: bool = False, reset: bool = True,
            tool: str | None = None, serial: str | None = None) -> None:
    """Program files in one session; erase the whole flash first if asked, reset at the end if asked."""
    tool = tool or default_tool()
    if tool == "jlink":
        _jlink(jlink_program_commands(items, erase_all, reset), serial)
        return
    base = [linkserver_exe(), "flash"] + (["--probe", serial] if serial else []) + [LINKSERVER_DEVICE, "load"]
    for index, (path, address) in enumerate(items):
        last = index == len(items) - 1
        options = (["--erase-all"] if erase_all and index == 0 else []) + ([] if last and reset else ["--no-reset"])
        _run(base + options + [f"{path}" if address is None else f"{path}:0x{address:08X}"])


def dump(address: int, size: int, output: Path, tool: str | None = None, serial: str | None = None) -> None:
    """Read size bytes from address into output."""
    tool = tool or default_tool()
    if tool == "jlink":
        _jlink(jlink_dump_commands(address, size, output), serial)
        return
    _run([linkserver_exe(), "flash"] + (["--probe", serial] if serial else [])
         + [LINKSERVER_DEVICE, "dump", hex(address), hex(size), str(output)])


def selftest() -> None:
    # A JDK jlink on PATH must never be taken for J-Link Commander.
    with tempfile.TemporaryDirectory() as tmp:
        for name in ("JLink.exe", "JLinkExe"):
            fake = Path(tmp) / name
            fake.write_text("")
            fake.chmod(0o755)
        saved = os.environ.get("PATH", "")
        os.environ["PATH"] = tmp
        try:
            found = jlink_exe()
        except SystemExit:
            found = ""  # no SEGGER installation on this host: also correct
        finally:
            os.environ["PATH"] = saved
        assert not found.startswith(tmp), found
    elf, binary = Path("boot.elf"), Path("app.bin")
    assert jlink_program_commands([(elf, None), (binary, 0x28040000)], erase_all=True, reset=True) == [
        "r", "h", "exec EnableEraseAllFlashBanks", "erase", 'loadfile "boot.elf"', 'loadfile "app.bin" 0x28040000', "r", "g", "exit"]
    assert jlink_program_commands([(binary, 0x28240000)], erase_all=False, reset=False) == [
        "r", "h", 'loadfile "app.bin" 0x28240000', "exit"]
    assert jlink_dump_commands(0x287E0000, 0x20000, Path("d.bin")) == [
        "h", 'savebin "d.bin" 0x287E0000 0x20000', "g", "exit"]
    print("probe command generation: OK")


if __name__ == "__main__":
    if sys.argv[1:] == ["selftest"]:
        selftest()
    else:
        sys.exit(__doc__)
