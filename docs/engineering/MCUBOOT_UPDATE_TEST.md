# MCUboot update test (MIMXRT1180-EVK, phase 1)

This procedure validates the phase-1 MCUboot integration (ADR-0014) on the EVK: MCUboot boots the dual-core LED application, updates it, confirms it, and reverts an unconfirmed image. The LED toggle period tells the images apart.

| Image | Version | LED toggles every | Confirms itself | Programmed into |
|---|---|---:|---|---|
| v1 | 1.0.0 | 1 s | yes | primary slot 0x28040000 |
| v2 | 2.0.0 | 250 ms | yes | secondary slot 0x28240000 (test update) |
| v3 | 3.0.0 | 100 ms | **no** | secondary slot 0x28240000 (rollback test) |

## Prerequisites

- MIMXRT1180-EVK connected through the on-board MCU-Link USB port, boot configuration as delivered (FlexSPI NOR boot), lifecycle OEM Open.
- NXP LinkServer installed (found automatically in `C:\nxp\LinkServer_*`, on `PATH`, or through `LINKSERVER`).
- Serial terminal on the MCU-Link VCOM port: 115200 baud, 8N1, no flow control.
- SDK setup completed (`scripts/setup_sdk.*`), `ARMGCC_DIR` set, `cmake` and `ninja` on `PATH`.

## Build

```sh
.venv/Scripts/python scripts/rt1180_mcuboot.py build     # Windows
.venv/bin/python scripts/rt1180_mcuboot.py build         # Linux/macOS
```

The command builds MCUboot and the three application images, signs them with the NXP development key and verifies the signatures. The output is in `build/rt1180-update-test/`, and `images.txt` lists every file with its address. CI builds the same set and publishes it as the `rt1180-mcuboot-update-test` artifact.

## Test steps

Keep the serial terminal open for the whole test. Record the console output of every step.

### T1: MCUboot boots v1

```sh
.venv/Scripts/python scripts/rt1180_mcuboot.py flash initial
```

This erases the whole flash, programs MCUboot and programs v1 into the primary slot. The board resets after programming.

Pass:
- The console shows the MCUboot banner, then `AIED dual-core LED toggle 1.0.0 (toggle period 1000000 us)`, `CM7 ready` and `MCUboot: image state ..., nothing to confirm`.
- The green user LED toggles every 1 s.

### T2: update to v2, confirmed

```sh
.venv/Scripts/python scripts/rt1180_mcuboot.py flash v2
```

The board does not reset after programming. Press RESET.

Pass:
- MCUboot reports a test swap, then the console shows version `2.0.0`, `CM7 ready` and `MCUboot: test image confirmed (permanent)`.
- The LED toggles every 250 ms.
- Press RESET again: MCUboot does not swap, the console shows `2.0.0` and `nothing to confirm`, and the LED still toggles every 250 ms. The update is permanent.

### T3: unconfirmed v3 is reverted

```sh
.venv/Scripts/python scripts/rt1180_mcuboot.py flash v3
```

Press RESET.

Pass:
- MCUboot reports a test swap, then the console shows `3.0.0` and `confirmation disabled in this build`. The LED toggles every 100 ms.
- Press RESET again: MCUboot reverts the swap, the console shows `2.0.0`, and the LED toggles every 250 ms again.

## If a step fails

Stop and keep the evidence: console log, LinkServer output, and the step number. Typical causes:

- No console output and no LED after T1: the Boot ROM did not start MCUboot. Check the boot configuration and the lifecycle.
- MCUboot starts but reports an invalid image: signature or slot address mismatch. Compare with `images.txt`.
- The application starts but `CM7 ready timeout` is printed: CM7 image copy or start failed, a pair-level failure. MCUboot reverts an unconfirmed test image at the next reset.

## Not covered yet

- Power loss and reset injected during the swap (next test after this procedure passes).
- Update reception over UART (xmodem), without a debugger.
- Anti-rollback, production signing keys, recovery stage and redundant MCUboot (phase 2).
