#!/usr/bin/env python3
"""Retrieve the persistent log files of the MIMXRT1189 boards (AM254-4, MIMXRT1180-EVK; ADR-0015).

    python scripts/rt1180_logs.py uart --port COM5      # running firmware, `logdump` command
    python scripts/rt1180_logs.py flash                 # debugger dump of the log region
    python scripts/rt1180_logs.py selftest              # check the parsers on the host

The files are written to build/logs/<method>-<date-time>/ unchanged (text, one record per line).
`uart` needs pyserial; `flash` needs littlefs-python, which has wheels for Python 3.12-3.14.
`flash` uses the SEGGER J-Link by default (scripts/probe.py, ADR-0017). The J-Link reads through
the XIP window, so the ROM must have configured FlexSPI (any bootable image); otherwise use
--probe-tool linkserver, whose flash driver reads the NOR directly.
"""

from __future__ import annotations

import argparse
import datetime
import re
import sys
import tempfile
import time
from pathlib import Path

import probe

ROOT = Path(__file__).resolve().parents[1]
LOGS_DIR = ROOT / "build" / "logs"

# Log region (examples/dual_core_led_toggle/log/primary.conf): QSPI offset 0x7E0000.
REGION_ADDRESS = 0x287E0000
BLOCK_SIZE = 4096
BLOCK_COUNT = 32
READ_SIZE = 16
PROG_SIZE = 256
LOOKAHEAD_SIZE = 16

FILE_BEGIN = re.compile(r"^=== FILE (\S+) (\d+) ===$")
FILE_END = "=== END FILE ==="
DUMP_DONE = re.compile(r"^=== AIED LOG DUMP (END|INCOMPLETE|UNAVAILABLE) ===$")


def parse_uart_dump(lines: list[str]) -> tuple[dict[str, str], str]:
    """Parse the framed `logdump` output into {file name: text} and the final status."""
    files: dict[str, str] = {}
    current: str | None = None
    expected = 0
    status = "MISSING"
    for raw in lines:
        line = raw.rstrip("\r\n")
        match = FILE_BEGIN.match(line)
        done = DUMP_DONE.match(line)
        if current is None and match:
            current, expected = match.group(1), int(match.group(2))
            files[current] = ""
        elif current is not None and line == FILE_END:
            if len(files[current].encode("utf-8")) != expected:
                status = "SIZE MISMATCH"
            current = None
        elif current is not None:
            files[current] += line + "\n"
        elif done:
            status = done.group(1) if status == "MISSING" else status
    if current is not None:
        status = "TRUNCATED"
    return files, status


def extract_littlefs(image: bytes) -> dict[str, str]:
    """Read every file of the log region image with littlefs-python."""
    try:
        from littlefs import LittleFS
    except ImportError:
        sys.exit("Error: littlefs-python is missing. Install it in Python 3.12-3.14: "
                 "py -3.14 -m pip install littlefs-python==0.19.0")
    fs = LittleFS(block_size=BLOCK_SIZE, block_count=BLOCK_COUNT, read_size=READ_SIZE,
                  prog_size=PROG_SIZE, lookahead_size=LOOKAHEAD_SIZE, mount=False)
    fs.context.buffer = bytearray(image)
    fs.mount()
    files = {}
    for name in sorted(fs.listdir("/")):
        with fs.open(name, "rb") as handle:
            files[name] = handle.read().decode("utf-8", errors="replace")
    return files


def save(files: dict[str, str], method: str) -> Path:
    """Write the retrieved files into a new timestamped folder."""
    folder = LOGS_DIR / f"{method}-{datetime.datetime.now():%Y%m%d-%H%M%S}"
    folder.mkdir(parents=True)
    for name, text in files.items():
        (folder / Path(name).name).write_text(text, encoding="utf-8", newline="\n")
    return folder


def command_uart(args: argparse.Namespace) -> None:
    """Send `logdump` to the running firmware and capture the framed output."""
    try:
        import serial
    except ImportError:
        sys.exit("Error: pyserial is missing; run scripts/setup_sdk to update the environment.")
    lines: list[str] = []
    with serial.Serial(args.port, args.baud, timeout=0.5) as port:
        port.reset_input_buffer()
        port.write(b"logdump\r\n")
        deadline = time.monotonic() + args.timeout
        while time.monotonic() < deadline:
            raw = port.readline()
            if not raw:
                continue
            line = raw.decode("utf-8", errors="replace")
            lines.append(line)
            if DUMP_DONE.match(line.rstrip("\r\n")):
                break
    files, status = parse_uart_dump(lines)
    report(files, status, "uart")


def command_flash(args: argparse.Namespace) -> None:
    """Dump the log region through the debugger and extract the files on the host."""
    size = BLOCK_SIZE * BLOCK_COUNT
    with tempfile.TemporaryDirectory() as tmp:
        image_path = Path(tmp) / "log-region.bin"
        probe.dump(REGION_ADDRESS, size, image_path, tool=args.probe_tool, serial=args.probe)
        image = image_path.read_bytes()
    if len(image) != size:
        sys.exit(f"Error: expected {size} bytes from the log region, got {len(image)}.")
    report(extract_littlefs(image), "END", "flash")


def report(files: dict[str, str], status: str, method: str) -> None:
    """Save the files and summarise the result."""
    logs = {name: text for name, text in files.items() if name.startswith("log.")}
    if not logs:
        sys.exit(f"Error: no log files retrieved (status {status}).")
    folder = save(logs, method)
    lines = sum(text.count("\n") for text in logs.values())
    print(f"{len(logs)} file(s), {lines} record(s), status {status}: {folder}")
    if status != "END":
        sys.exit(f"Warning: dump status {status}; the files may be incomplete.")


def command_selftest(_: argparse.Namespace) -> None:
    """Check both parsers without hardware."""
    record = "[00:00:01.000,000] <inf> app: hello\n"
    dump = ["noise\r\n", "=== AIED LOG DUMP BEGIN boot 3 ===\r\n",
            f"=== FILE log.00000001.txt {len(record)} ===\r\n", record.replace("\n", "\r\n"),
            "=== END FILE ===\r\n", "=== AIED LOG DUMP END ===\r\n"]
    files, status = parse_uart_dump(dump)
    assert status == "END" and files == {"log.00000001.txt": record}, (status, files)
    assert parse_uart_dump(dump[:4])[1] == "TRUNCATED"
    assert parse_uart_dump(dump[:3] + ["x\r\n"] + dump[4:])[1] == "SIZE MISMATCH"
    print("UART dump parser: OK")
    try:
        from littlefs import LittleFS
    except ImportError:
        print("littlefs-python not installed: flash extraction not tested")
        return
    fs = LittleFS(block_size=BLOCK_SIZE, block_count=BLOCK_COUNT, read_size=READ_SIZE,
                  prog_size=PROG_SIZE, lookahead_size=LOOKAHEAD_SIZE)
    with fs.open("log.00000002.txt", "wb") as handle:  # binary, as the firmware writes it
        handle.write(record.encode("utf-8"))
    with fs.open("boot.cnt", "wb") as handle:
        handle.write(b"\x03\x00\x00\x00")
    extracted = extract_littlefs(bytes(fs.context.buffer))
    assert extracted["log.00000002.txt"] == record, extracted
    print("littlefs extraction: OK")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="command", required=True)
    uart = sub.add_parser("uart", help="retrieve logs from running firmware over the debug UART")
    uart.add_argument("--port", required=True, help="serial port of the MCU-Link VCOM (e.g. COM5)")
    uart.add_argument("--baud", type=int, default=115200)
    uart.add_argument("--timeout", type=float, default=60.0, help="seconds to wait for the dump")
    uart.set_defaults(func=command_uart)
    flash = sub.add_parser("flash", help="dump the log region through the SWD probe (firmware not needed)")
    flash.add_argument("--probe-tool", choices=probe.TOOLS, help="jlink (default, or PROBE_TOOL) or linkserver")
    flash.add_argument("--probe", help="probe serial number (J-Link) or serial/#index (LinkServer)")
    flash.set_defaults(func=command_flash)
    sub.add_parser("selftest", help="check the parsers without hardware").set_defaults(func=command_selftest)
    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
