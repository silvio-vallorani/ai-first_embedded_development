# ADR-0015: Logging service and persistent log region

- Status: Accepted; hardware validation pending
- Date: 2026-11-08

## Context
The firmware needs Zephyr-style logging: records are printed and also stored in non-volatile flash as text files, so they can be retrieved after a problem. Each record needs a timestamp and must end with a newline.

The MCUXpresso SDK provides a log component (`components/log`). It does not print the module name, adds a line ending only when the format string contains `\n` (and then writes CRLF), inserts ANSI colour codes by default, silently truncates at 128 characters, and depends on SDK headers, so it cannot be tested on the host. The SDK also provides littlefs 2.11 with an `mflash` block-device adapter for the RT1180 FlexSPI NOR.

## Decision
- **Log core**: an own C11 service in `firmware/services/log` with the Zephyr API (`LOG_MODULE_REGISTER(name, level)`, `LOG_ERR/WRN/INF/DBG`, levels with Zephyr's values), filtered at compile time (`AIED_LOG_MAX_LEVEL`) and per module.
- **Record format**: exactly one line per record, `[hh:mm:ss.mmm,uuu] <lvl> module: message\n`, at most 160 bytes. Line breaks inside the message become spaces; a longer message is truncated and ends with `...`.
- **Backends**: the console is immediate and writes in the caller's context, with CRLF on the terminal. The file backend is deferred: records are buffered in a 2-KiB RAM ring and written by `aied_log_process()` from the main loop, never from an interrupt. When the ring is full, new records are dropped and counted, and a `<wrn> log: N messages dropped` record is written later.
- **Persistence**: littlefs from the SDK on the NXP `mflash` adapter, with static buffers (no heap). Files are `log.NNNNNNNN.txt` (16 KiB each, 4 kept, oldest removed), plus `boot.cnt` with the boot counter.
- **Flash region**: QSPI offset 0x7E0000 (CPU 0x287E0000), 128 KiB = 32 blocks of 4 KiB. This is the "Persistent logs" region of the target `MEMORY_MAP.md`, so it stays valid in phase 2. In the phase-1 layout (ADR-0014) the region is outside MCUboot and both slots.
- **Timestamp**: uptime since boot from the CM33 DWT cycle counter, extended to 64 bits by SysTick every 10 ms, so it stays correct while `mflash` keeps interrupts masked during erase or program. Every boot writes `<inf> log: boot N, firmware X.Y.Z`, with N persistent, so sessions can be told apart. A real-time clock can replace the time source later without changing the core.
- **Retrieval**: the UART command `logdump` on the running firmware prints all files in framed form. The debugger path reads the region with LinkServer and extracts the files on the PC with littlefs-python (`scripts/rt1180_logs.py`), which works even when the firmware no longer runs.
- **Security events** (2026-11-09, CRA plan phase A): image state, confirmation and log access are recorded through `LOG_SEC_ERR/WRN/INF` in a shared `sec` module, so they can be filtered as `<lvl> sec:` regardless of the calling file.
- **Secure erasure** (2026-11-09): the UART command `logerase` erases every block of the log region, then formats it; `lfs_format` alone would leave old records readable in unused blocks. The boot counter restarts at 1 and the first record of the new file system is `<wrn> sec: log storage erased on request over UART`.
- **Code placement** (2026-11-10): `mflash` reprograms the NOR the CPU executes from, so `mflash_drv.c` and `fsl_flexspi.c` must run from ITCM. The MCUboot application linker already does this; standalone (direct ROM boot) builds use NXP's bootloader XIP linker (`ota_examples/mcuboot_opensource/linker/MIMXRT1189xxxxx_cm33_flexspi_nor.ld`, the standard CM33 XIP linker plus exactly that relocation) through `examples/dual_core_led_toggle/standalone/primary.cmake`. Without it the log initialisation corrupts its own instruction fetches (seen on the AM254-4).
- **Release builds**: `AIED_LOG_PORT_COMMANDS=0` removes both UART commands. They are unauthenticated, so release builds must disable them until service access is authenticated (CRA plan phase E); debugger retrieval keeps working.

## Consequences
- Only CM33 writes the persistent log. CM7 has no logging yet; CM33 records the CM7 events it sees. Forwarding CM7 records needs an inter-core channel (rpmsg), which is a later step.
- Flash erase and program run with interrupts masked (an `mflash` requirement while executing in place), for up to a few hundred milliseconds per 4-KiB erase. The main loop is non-blocking, so this only delays work and never loses a toggle.
- Records still in the RAM ring at a reset or power loss are lost. littlefs keeps every committed file consistent after power loss.
- At up to 64 KiB of text the region holds roughly the last 1000 records. littlefs needs the remaining space for copy-on-write.
- `rt1180_mcuboot.py flash initial` erases the whole flash, logs included.
- littlefs-python has no wheel for Python 3.15; debugger extraction needs Python 3.12–3.14.
- With `AIED_LOG_PORT_COMMANDS=0` the end user has no way to erase the logs until an authenticated service interface exists; the CRA requirement to remove data then relies on the debugger or a factory reset (open in the CRA plan).

## Validation
Host unit tests cover formatting, filtering, deferral, overflow, rotation, the boot counter, write failures and the dump. On the EVK: check boot records, log survival across reset and power cycle, rotation after more than 16 KiB, `logdump` and `logerase` over UART (after `logerase`, a debugger dump of the region must show only the new file system), debugger extraction, and log writes during an MCUboot update.
