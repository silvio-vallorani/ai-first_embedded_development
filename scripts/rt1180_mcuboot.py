#!/usr/bin/env python3
"""Build, sign and flash the MCUboot update-test images for MIMXRT1189 boards (ADR-0014).

Run with the project virtual environment, for example:

    .venv/Scripts/python scripts/rt1180_mcuboot.py build      (Windows)
    .venv/bin/python scripts/rt1180_mcuboot.py build          (Linux/macOS)
    .venv/Scripts/python scripts/rt1180_mcuboot.py --board am254-4 build
    .venv/Scripts/python scripts/rt1180_mcuboot.py --board am254-4 flash initial
    .venv/Scripts/python scripts/rt1180_mcuboot.py check-release <image.bin>   (release guard, ADR-0011)

--board selects the board under boards/ (default mimxrt1180_evk). The bootloader is the same for
both boards (it only uses the debug UART and FlexSPI1 port A, wired identically). flash uses the
SEGGER J-Link by default (ADR-0017); --probe-tool linkserver selects NXP LinkServer (see
scripts/probe.py for the environment variables). The update-test procedure is described in
docs/engineering/MCUBOOT_UPDATE_TEST.md.
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

import probe

ROOT = Path(__file__).resolve().parents[1]
SDK_ROOT = Path(os.environ.get("MCUXPRESSO_SDK", ROOT.parent / "mcuxpresso-sdk")).resolve()
MCUBOOT_DIR = SDK_ROOT / "mcuxsdk" / "middleware" / "mcuboot_opensource"
IMGTOOL = MCUBOOT_DIR / "scripts" / "imgtool.py"
# shortcut: NXP's public development key (ADR-0014), replace with a protected key before production.
SIGNING_KEY = MCUBOOT_DIR / "boot" / "nxp_mcux_sdk" / "keys" / "sign-ecdsa-p256-priv.pem"

BOARD = "mimxrt1180_evk"  # set from --board in main()
OUTPUT_DIR = ROOT / "build" / "rt1180-update-test"  # per board, set in main()
BOOTLOADER_BUILD_DIR = ROOT / "build" / "rt1180-mcuboot"
BOOTLOADER_ELF = BOOTLOADER_BUILD_DIR / "mcuboot_opensource_cm33.elf"
APP_BIN_NAME = "led_toggle_primary_core_cm33.bin"

# NXP reference layout for evkmimxrt1180 (ota_examples/mcuboot_opensource/flash_partitioning.h).
PRIMARY_SLOT = 0x28040000
SECONDARY_SLOT = 0x28240000
SLOT_SIZE = 0x200000
HEADER_SIZE = 0x400


def build_prefix() -> str:
    """Short build-directory prefix of the selected board (Windows path-length limit)."""
    return "rt1180" if BOARD == "mimxrt1180_evk" else BOARD


@dataclass(frozen=True)
class TestImage:
    """One application image of the update test."""

    name: str
    version: str
    toggle_period_us: int
    confirm: bool
    slot: str  # "primary": programmed directly; "secondary": test update for MCUboot

    @property
    def file_name(self) -> str:
        suffix = "signed.bin" if self.slot == "primary" else "signed-test.bin"
        return f"led-toggle-{self.version}{'' if self.confirm else '-noconfirm'}.{suffix}"


IMAGES = (
    TestImage("v1", "1.0.0", 1_000_000, True, "primary"),
    TestImage("v2", "2.0.0", 250_000, True, "secondary"),
    TestImage("v3", "3.0.0", 100_000, False, "secondary"),
)


def run(command: list[str], **kwargs) -> None:
    """Run a command, echoing it, and stop on failure."""
    print("+", " ".join(str(part) for part in command), flush=True)
    subprocess.run([str(part) for part in command], check=True, **kwargs)


def require(path: Path, hint: str) -> None:
    """Stop with a helpful message when a required file is missing."""
    if not path.exists():
        sys.exit(f"Error: {path} not found. {hint}")


def venv_tool(name: str) -> Path:
    """Return an executable from the project virtual environment."""
    scripts = ROOT / ".venv" / ("Scripts" if os.name == "nt" else "bin")
    return scripts / (f"{name}.exe" if os.name == "nt" else name)


def build_bootloader() -> None:
    """Build NXP's MCUboot port (ota_examples/mcuboot_opensource) for the CM33."""
    if not os.environ.get("ARMGCC_DIR"):
        sys.exit("Error: set ARMGCC_DIR to the Arm GNU Toolchain 14.2.x installation root.")
    west = venv_tool("west")
    require(west, "Run scripts/setup_sdk first.")
    run(
        [
            west, "build", "-b", "evkmimxrt1180",
            "mcuxsdk/examples/ota_examples/mcuboot_opensource",
            "--config", "flexspi_nor_debug", "--toolchain", "armgcc", "-Dcore_id=cm33",
            f"-DPython3_EXECUTABLE={venv_tool('python')}",
            "-p", "always", "-d", BOOTLOADER_BUILD_DIR,
        ],
        cwd=SDK_ROOT,
    )
    require(BOOTLOADER_ELF, "The bootloader build did not produce the expected ELF.")


def build_app(image: TestImage) -> Path:
    """Build one MCUboot application variant and return its raw binary."""
    build_dir = ROOT / "build" / f"{build_prefix()}-ut-{image.name}"
    env = dict(os.environ, RT1180_BUILD_DIR=str(build_dir))
    if os.name == "nt":
        command = [
            "powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass",
            "-File", ROOT / "scripts" / "build_rt1180.ps1", "-Board", BOARD, "-McuBoot",
            "-Version", image.version, "-TogglePeriodUs", str(image.toggle_period_us),
        ] + ([] if image.confirm else ["-NoConfirm"])
    else:
        command = [
            "bash", ROOT / "scripts" / "build_rt1180.sh", "--board", BOARD, "--mcuboot",
            "--version", image.version, "--toggle-period-us", str(image.toggle_period_us),
        ] + ([] if image.confirm else ["--no-confirm"])
    run(command, env=env)
    binary = build_dir / "primary" / APP_BIN_NAME
    require(binary, "The application build did not produce the expected binary.")
    return binary


def imgtool(*args: object) -> None:
    """Run the SDK's imgtool with the project Python."""
    run([sys.executable, IMGTOOL, *args])


def sign(image: TestImage, binary: Path) -> Path:
    """Sign a raw application binary; secondary-slot images request a test swap."""
    output = OUTPUT_DIR / image.file_name
    args = [
        "sign", "--key", SIGNING_KEY, "--align", "4", "--version", image.version,
        "--slot-size", hex(SLOT_SIZE), "--header-size", hex(HEADER_SIZE), "--pad-header",
    ]
    if image.slot == "secondary":
        args.append("--pad")  # Full slot with trailer magic: MCUboot performs a test swap.
    imgtool(*args, binary, output)
    imgtool("verify", "--key", SIGNING_KEY, output)
    return output


def command_build(_: argparse.Namespace) -> None:
    """Build the bootloader and all update-test images into build/rt1180-update-test."""
    for path, hint in ((IMGTOOL, "Run scripts/setup_sdk first."), (SIGNING_KEY, "")):
        require(path, hint)
    if OUTPUT_DIR.exists():
        shutil.rmtree(OUTPUT_DIR)
    OUTPUT_DIR.mkdir(parents=True)

    build_bootloader()
    shutil.copy2(BOOTLOADER_ELF, OUTPUT_DIR / BOOTLOADER_ELF.name)
    lines = [
        f"MCUboot update-test images for {BOARD} (ADR-0014, NXP reference layout)",
        f"bootloader  {BOOTLOADER_ELF.name}  ELF, programmed from 0x28000000",
    ]
    for image in IMAGES:
        signed = sign(image, build_app(image))
        address = PRIMARY_SLOT if image.slot == "primary" else SECONDARY_SLOT
        lines.append(
            f"{image.name}  {signed.name}  {image.slot} slot 0x{address:08X}, version {image.version}, "
            f"toggle {image.toggle_period_us} us, confirm {'yes' if image.confirm else 'NO'}"
        )
    (OUTPUT_DIR / "images.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines))


def command_check_release(args: argparse.Namespace) -> None:
    """Reject release candidates that NXP's public development key validates (ADR-0011)."""
    require(IMGTOOL, "Run scripts/setup_sdk first.")
    rejected = 0
    for path in args.images:
        result = subprocess.run([sys.executable, str(IMGTOOL), "verify", "--key", str(SIGNING_KEY), str(path)],
                                capture_output=True, text=True)
        output = (result.stdout + result.stderr).strip()
        if result.returncode == 0:
            reason = "signed with the public NXP development key"
        elif "No signature found for the given key" not in output:
            reason = "not a valid signed MCUboot image: " + (output.splitlines() or ["no output"])[-1]
        else:
            print(f"ok        {path}: not signed with the development key")
            continue
        print(f"REJECTED  {path}: {reason}")
        rejected += 1
    if rejected:
        sys.exit(f"{rejected} image(s) rejected as release candidates.")


def command_flash(args: argparse.Namespace) -> None:
    """Program one step of the update test through the SWD probe (J-Link by default)."""
    images = {image.name: image for image in IMAGES}
    if args.step == "initial":
        bootloader = OUTPUT_DIR / BOOTLOADER_ELF.name
        v1 = OUTPUT_DIR / images["v1"].file_name
        require(bootloader, "Run the build command first.")
        require(v1, "Run the build command first.")
        # Erase everything once so no stale image or trailer remains in either slot.
        probe.program([(bootloader, None), (v1, PRIMARY_SLOT)], erase_all=True, reset=True,
                      tool=args.probe_tool, serial=args.probe)
    else:
        image = images[args.step]
        signed = OUTPUT_DIR / image.file_name
        require(signed, "Run the build command first.")
        probe.program([(signed, SECONDARY_SLOT)], reset=False, tool=args.probe_tool, serial=args.probe)
        print("Secondary slot programmed. Press the board RESET button to let MCUboot perform the update.")


def main() -> None:
    global BOARD, OUTPUT_DIR
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--board", default=BOARD, help="board under boards/ (mimxrt1180_evk or am254-4)")
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("build", help="build bootloader and signed v1/v2/v3 images").set_defaults(func=command_build)
    flash = sub.add_parser("flash", help="program one step of the update test through the SWD probe")
    flash.add_argument(
        "step",
        choices=("initial", "v2", "v3"),
        help="initial: erase all + MCUboot + v1 in primary slot; v2/v3: test image in secondary slot",
    )
    flash.add_argument("--probe-tool", choices=probe.TOOLS, help="jlink (default, or PROBE_TOOL) or linkserver")
    flash.add_argument("--probe", help="probe serial number (J-Link) or serial/#index (LinkServer)")
    flash.set_defaults(func=command_flash)
    check = sub.add_parser("check-release", help="fail if an image is signed with the public development key")
    check.add_argument("images", nargs="+", type=Path)
    check.set_defaults(func=command_check_release)
    args = parser.parse_args()
    if not (ROOT / "boards" / args.board / "include" / "aied_board.h").is_file():
        sys.exit(f"Error: unknown board {args.board} (expected boards/<name>/include/aied_board.h).")
    BOARD = args.board
    OUTPUT_DIR = ROOT / "build" / f"{build_prefix()}-update-test"
    args.func(args)


if __name__ == "__main__":
    main()
