---
name: rt1180-build
description: Build, sign and check the MIMXRT1180-EVK firmware of this project (standalone LED example, MCUboot variant, bootloader + update-test images) and run the host unit tests. Use whenever firmware has to be compiled, a build fails, or build outputs must be verified.
---

# RT1180 firmware build

Target: MIMXRT1180-EVK, MIMXRT1189 (CM33 + CM7), MCUXpresso SDK 26.09.00, Arm GNU Toolchain 14.2. Read `docs/toolchains/MCUXPRESSO_SDK.md` for background; this skill is the short operational path.

## Environment (Windows dev PC)

The toolchain and ninja are not on PATH. Set them in the same PowerShell command as the build:

```powershell
$env:ARMGCC_DIR = "$env:USERPROFILE\.mcuxpressotools\arm-gnu-toolchain-14.2.rel1-mingw-w64-x86_64-arm-none-eabi"
$env:Path = "$env:USERPROFILE\.mcuxpressotools\ninja-1.12.1;" + $env:Path
```

The SDK workspace is `../mcuxpresso-sdk`. If `west update_board` fails with "untracked working tree files would be overwritten", a previous clone was interrupted: tell the user and rename that repository folder aside; never delete it without asking.

## Commands

| Goal | Command | Output |
|---|---|---|
| Host unit tests | `python scripts/host_tests.py` | `build/host-local` |
| LED example, direct ROM boot | `.\scripts\build_rt1180.ps1` | `build/rt1180-led-toggle` |
| LED example for MCUboot slot | `.\scripts\build_rt1180.ps1 -McuBoot [-Version X.Y.Z] [-TogglePeriodUs N] [-NoConfirm]` | `build/rt1180-led-mcuboot` |
| MCUboot + signed v1/v2/v3 test set | `.\.venv\Scripts\python.exe scripts\rt1180_mcuboot.py build` | `build/rt1180-update-test` |

On Linux/macOS use `bash scripts/build_rt1180.sh [--mcuboot ...]` and `.venv/bin/python`. Capture long build output to a file in the scratchpad and grep it for `error:|undefined reference|FAILED|CMake Error`. The jsonschema warnings from the SDK are harmless.

## Known pitfalls

- **Windows path limit**: the Arm compiler cannot write `-fstack-usage` files beyond 260 characters. The symptom is `cannot open ... .su for writing`. Keep build directory names short or set `RT1180_BUILD_DIR`.
- `armgcc.cmake:24` error: `ARMGCC_DIR` is not set.
- A manual `west build` needs the `.venv\Scripts` folder on PATH (`update_board` and the SDK start `west` and `python` by name).

## Checking outputs

Use `arm-none-eabi-objdump -h` / `nm` from `$env:ARMGCC_DIR\bin`:

- MCUboot app: `.interrupts` at `0x28040400`, nothing at `0x28000000`; `core1_image_start..end` present (CM7 embedded); `mflash_drv_init` in ITCM (`0x0FFE....`).
- Bootloader: `.fcb` at `0x28000400`, `.container` at `0x28001000`.
- Log region: `LittleFS_ctx.start_addr = 0x7e0000`, `LITTLEFS_BLOCK_COUNT 32` in `primary/mcux_config.h`.
- Signed images: `imgtool verify -k <sdk>/middleware/mcuboot_opensource/boot/nxp_mcux_sdk/keys/sign-ecdsa-p256-priv.pem <file>`; secondary-slot test images end with the MCUboot magic `77 c2 95 f3 ... 79 80`.

Report what was built, which checks passed, and anything not verified (hardware behaviour is never proven by a build).
