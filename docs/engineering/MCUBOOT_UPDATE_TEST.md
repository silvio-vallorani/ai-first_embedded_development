# MCUboot update test (phase 1)

This procedure validates the phase-1 MCUboot integration (ADR-0014) on the AM254-5 custom board (the target board) or the MIMXRT1180-EVK: MCUboot boots the dual-core LED application, updates it, confirms it, and reverts an unconfirmed image. The LED toggle period tells the images apart.

| Image | Version | LED toggles every | Confirms itself | Programmed into |
|---|---|---:|---|---|
| v1 | 1.0.0 | 1 s | yes | primary slot 0x28040000 |
| v2 | 2.0.0 | 250 ms | yes | secondary slot 0x28240000 (test update) |
| v3 | 3.0.0 | 100 ms | **no** | secondary slot 0x28240000 (rollback test) |

## Board selection

Every command below takes `--board am254-5` before the subcommand (default: `mimxrt1180_evk`), for example `rt1180_mcuboot.py --board am254-5 flash initial`. The output directory is then `build/am254-5-update-test/` and the CI artifact `am254-5-mcuboot-update-test`. The LED is the board user LED: DL1 (green) on the AM254-5, the green user LED on the EVK. The bootloader and the flash layout are the same on both boards.

## Prerequisites

- **AM254-5:** SWD probe (for example MCU-Link) on the Tag-Connect J1. The console UART (LPUART1, 3.3 V) is on J1 pin 8 (MCU TX) and pin 7 (MCU RX); use the probe VCOM if it carries these pins, otherwise a 3.3-V USB-UART. DIP switch SW1 set to boot from FlexSPI NOR (check the setting against `docs/boards/AM254-5.md`), lifecycle OEM Open.
- **EVK:** connected through the on-board MCU-Link USB port, boot configuration as delivered (FlexSPI NOR boot), lifecycle OEM Open.
- LinkServer uses the EVK device configuration for both boards (same NOR on FlexSPI1 port A); set `LINKSERVER_DEVICE` to override it.
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

This erases the whole flash (persistent logs included), programs MCUboot and programs v1 into the primary slot. The board resets after programming.

Pass:
- The console shows the MCUboot banner, then log records (format in `LOGGING.md`): `<inf> log: boot 1, firmware 1.0.0, log storage formatted`, `<inf> app: AIED dual-core LED toggle 1.0.0 (toggle period 1000000 us)`, `<inf> app: CM7 ready` and `<inf> app: MCUboot: image state ..., nothing to confirm`.
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

### T4: the persistent log records the whole sequence

Close the serial terminal (it holds the port), then:

```sh
.venv/Scripts/python scripts/rt1180_logs.py uart --port COM5      # your MCU-Link VCOM port
```

Pass:
- The command reports status `END` and writes the files to `build/logs/uart-.../`.
- The files contain, in order, the boot records `boot 1, firmware 1.0.0`, then `2.0.0` with `test image confirmed`, `2.0.0`, `3.0.0` with `confirmation disabled`, and `2.0.0` again. Every line starts with a timestamp and ends with a newline.
- Power-cycle the board (unplug USB), then repeat the command: the same records are still there, followed by a new boot record.
- Optional, debugger path: `py -3.14 scripts/rt1180_logs.py flash` produces the same files.

## If a step fails

Stop and keep the evidence: console log, LinkServer output, and the step number. Typical causes:

- No console output and no LED after T1: the Boot ROM did not start MCUboot. Check the boot configuration and the lifecycle.
- MCUboot starts but reports an invalid image: signature or slot address mismatch. Compare with `images.txt`.
- The application starts but `CM7 ready timeout` is printed: CM7 image copy or start failed, a pair-level failure. MCUboot reverts an unconfirmed test image at the next reset.

## Not covered yet

- Power loss and reset injected during the swap (next test after this procedure passes).
- Update reception over UART (xmodem), without a debugger.
- Anti-rollback, production signing keys, recovery stage and redundant MCUboot (phase 2).
