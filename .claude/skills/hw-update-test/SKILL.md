---
name: hw-update-test
description: Run the MCUboot update test on a connected MIMXRT1180-EVK (T1 boot v1, T2 confirmed update to v2, T3 rollback of unconfirmed v3, T4 persistent log check). Flashes the board, so only the user starts it.
disable-model-invocation: true
---

# MCUboot update test on the EVK

Follow `docs/engineering/MCUBOOT_UPDATE_TEST.md` exactly; it defines the pass criteria. This skill adds the procedure for running it with the user.

## Before starting

1. Ask the user to confirm that the EVK is connected through MCU-Link and that no other tool holds the probe or the VCOM port.
2. Check the probe: `C:\nxp\LinkServer_*\LinkServer.exe probes`. Find the VCOM port: `Get-PnpDevice -PresentOnly | ? FriendlyName -match 'VCOM|MCU-Link'`.
3. Make sure `build/rt1180-update-test/images.txt` exists and is newer than the last source change. Otherwise build first (skill `rt1180-build`).
4. Warn the user that T1 erases the whole flash, including the persistent logs.

## Running each step

For every step T1–T3:

1. Run the flash command (`.venv\Scripts\python.exe scripts\rt1180_mcuboot.py flash initial|v2|v3`) and show the LinkServer result.
2. Capture the UART console while the user presses RESET, for example with a short pyserial read of 10–15 s on the VCOM port at 115200 baud.
3. Ask the user what the LED does (toggle period). The LED is the evidence for the CM7 side, which has no console.
4. Compare console and LED with the pass criteria. Mark the step PASS or FAIL. Never assume a result you did not observe.

T4: close the console capture, then run `.venv\Scripts\python.exe scripts\rt1180_logs.py uart --port <COMx>`. Check the boot records against the expected sequence, then repeat after a power cycle.

## On failure

Stop at the first FAIL. Keep the console log, LinkServer output and step number, and use the "If a step fails" table of the test document. Do not reflash or change firmware to work around the failure without the user's agreement.

## Report

Give a table of T1–T4 with PASS/FAIL and the evidence (console lines, LED period, log files), followed by anything not checked.
