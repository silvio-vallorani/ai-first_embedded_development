# Logging

The firmware uses a Zephyr-style logging service (ADR-0015). Every record is printed on the debug UART and stored in flash as a line of a text file.

## Using it in code

```c
#include "aied_log.h"

LOG_MODULE_REGISTER(motor, LOG_LEVEL_INF); /* once per .c file */

LOG_ERR("overcurrent %u mA", current_ma);
LOG_WRN("retry %d", attempt);
LOG_INF("started");
LOG_DBG("not stored: below this module's level");
```

- Levels: `LOG_LEVEL_NONE`, `LOG_LEVEL_ERR`, `LOG_LEVEL_WRN`, `LOG_LEVEL_INF`, `LOG_LEVEL_DBG` (same values as Zephyr).
- A record is kept when its level is at or below both the module level and `AIED_LOG_MAX_LEVEL` (default `LOG_LEVEL_DBG`). Lower `AIED_LOG_MAX_LEVEL` at build time and the compiler removes the more verbose calls.
- Do not end messages with `\n`: the service always ends each record with exactly one newline. Line breaks inside a message become spaces.
- Do not log from interrupts with high timing requirements: the console backend writes to the UART synchronously. Flash writes never happen in the logging call.
- Security-relevant events (image state and confirmation, log access, anything touching keys or the update path) use `LOG_SEC_ERR/WRN/INF` instead: they are recorded in the shared `sec` module whatever file they come from, so `grep "> sec:"` finds them all.
- Do not include the NXP `fsl_component_log.h` in the same file: it defines the same `LOG_*` macros, and `aied_log.h` stops the build if both are present.

## Record format

```text
[00:00:01.234,567] <inf> app: AIED dual-core LED toggle 2.0.0 (toggle period 250000 us)
```

The timestamp is `hours:minutes:seconds.milliseconds,microseconds` since reset. Each boot starts with:

```text
[00:00:00.004,120] <inf> log: boot 12, firmware 2.0.0
```

The boot number is stored in flash, so lines from different power cycles can be told apart. `, log storage formatted` is appended when the log region held no valid file system.

## Storage

| Item | Value |
|---|---|
| Region | QSPI offset 0x7E0000 (CPU 0x287E0000), 128 KiB |
| File system | littlefs (SDK), 32 blocks of 4 KiB |
| Files | `log.NNNNNNNN.txt`, a new file every 16 KiB, the 4 newest kept |
| Boot counter | `boot.cnt` |

Records go to a 2-KiB RAM buffer first and are written to flash by `aied_log_port_poll()` in the main loop. Records still in RAM at a sudden reset are lost. A full buffer drops new records and writes `<wrn> log: N messages dropped`.

## Retrieving the logs

**From running firmware (UART)**: the firmware answers the `logdump` command on the debug UART (115200 8N1) with all files, oldest first:

```sh
.venv/Scripts/python scripts/rt1180_logs.py uart --port COM5
```

You can also type `logdump` and press Enter in a serial terminal.

**From a board that no longer runs (debugger)**: the probe reads the log region and the files are extracted on the PC. The J-Link (default) reads through the XIP window, which needs any bootable image so that the ROM configures FlexSPI; `--probe-tool linkserver` reads the NOR directly. This needs littlefs-python, which exists for Python 3.12–3.14 only:

```sh
py -3.14 -m pip install littlefs-python==0.19.0
py -3.14 scripts/rt1180_logs.py flash
```

Both commands write the files unchanged into `build/logs/<method>-<date-time>/`.

## Erasing the logs

Type `logerase` and press Enter in a serial terminal. The firmware erases every block of the log region (not only the file-system metadata), formats it and answers `=== AIED LOG ERASE DONE ===`. The boot counter restarts at 1; the first record is `<wrn> sec: log storage erased on request over UART`.

## Release builds

`logdump` and `logerase` are unauthenticated. Build release images with `AIED_LOG_PORT_COMMANDS=0` (for example `mcux_add_macro(CC "-DAIED_LOG_PORT_COMMANDS=0")`), which removes both commands; the debugger retrieval keeps working.

## Porting

`platform/log/mcux/aied_log_port.c` connects the service to the MCUXpresso SDK: time source (DWT cycle counter + SysTick), console (`PUTCHAR`), littlefs mount and the `logdump`/`logerase` commands. `platform/storage/littlefs` implements the file-system interface used by the file backend. The core (`firmware/services/log`) has no hardware dependency and is unit-tested on the host.
