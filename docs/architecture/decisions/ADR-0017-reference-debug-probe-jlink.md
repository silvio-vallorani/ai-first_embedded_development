# ADR-0017: SEGGER J-Link as the reference SWD probe

- Status: Accepted; hardware validation pending (AM254-4)
- Date: 2026-11-09

## Context

The phase-1 tools (`rt1180_mcuboot.py flash`, `rt1180_logs.py flash`) drove NXP LinkServer with the EVK's on-board MCU-Link. The target board is now the AM254-4, which has no on-board probe: SWD (and the console UART) come out on the Tag-Connect J1. The project standardises on the SEGGER J-Link (J-Link software V9.84 installed on the development PC).

Facts:

- **The SDK already supports the J-Link for the RT1189.** Its J-Link runner for the evkmimxrt1180 board uses device `MIMXRT1189xxx8_M33` / `MIMXRT1189xxx8_M7` and NXP's `jlinkscript` files. The CM33 script handles the ROM trap after reset and the cache setup; it is SoC-specific, so it is valid on the AM254-4 as well.
- **External flash is programmed through J-Link's own loader.** The J-Link programs the QSPI NOR through SEGGER's RT1180 flash loader for FlexSPI1 port A. The AM254-4 wiring and device are identical to the EVK, so the loader is expected to work; this is confirmed at bring-up.
- **The J-Link has no USB VCOM on the 10-pin Tag-Connect.** The console UART on J1 pins 7/8 needs a separate 3.3-V USB-UART. SEGGER RTT through the probe is an alternative console channel.
- **Debug authentication works with the J-Link.** SPSDK `nxpdebugmbox` supports RT118x debug authentication with a J-Link, through `spsdk[jlink]` or pyOCD. SPSDK 3.13 requires `cryptography<50`, which conflicts with the project's signing tools (`cryptography==50.0.2`). Provisioning tools therefore get their own virtual environment.
- **J-Link licence.** J-Link EDU models are for non-commercial use only. A commercial project needs a J-Link BASE, PLUS, PRO or ULTRA+.
- **Probe in use (measured on the AM254-4, 2026-11-09).** J-Link hardware V10.10, S/N 600112700, firmware of 2023-01-30, which J-Link Configurator reports as the latest for this hardware. The J-Link software warns that this firmware "does not handle I/D-cache correctly" on ARMv8-M and later. This is a limit of the probe generation, not of a missing update. At 4 MHz SWD the connection failed intermittently ("Failed to power up DAP"); at 1 MHz it was stable for the whole update test.

## Decision

- **J-Link is the default probe of every project tool.** `scripts/probe.py` implements a J-Link backend (J-Link Commander command files: `loadfile`, `exec EnableEraseAllFlashBanks` + `erase`, `savebin`, run with the NXP CM33 `jlinkscript`) and keeps LinkServer as an alternative (`--probe-tool linkserver`, or `PROBE_TOOL`).
- **Connection defaults.** SWD at 1 MHz (`JLINK_SPEED`). A failed J-Link session is retried up to 3 times (`JLINK_ATTEMPTS`). Program sessions restart from a reset and dumps only read, so a retry is safe.
- **The tool looks only for SEGGER's J-Link Commander.** It searches `JLINK_INSTALL_DIR`, then the SEGGER install folders. A `jlink` on PATH is used only if its path contains `SEGGER`, because the JDK ships an unrelated `jlink.exe`.
- **J-Link MCP server for AI-assisted debugging.** The interactive server is `jlink-mcp` (npm, MIT), pinned to version 0.8.0 and registered in **project scope** in `.mcp.json` (device `MIMXRT1189xxx8_M33`, SWD, 1 MHz). Claude Code asks each developer once to approve it. It is launched through `node`, which starts `npx` in a shell, so the same entry works on Windows, Linux and macOS (on Windows a bare `npx` command would need a `cmd /c` wrapper). The server finds the J-Link installation in the standard SEGGER folders of each OS; `JLINK_INSTALL_DIR` in the environment overrides that. The `.claude/settings.json` permissions always ask before its flashing, erase, memory-write, raw-command, reset and GDB tools.
- **VS Code extension.** The same server is distributed as a VS Code extension, `Klievan.jlink-mcp` (publisher not domain-verified). It is listed in `.vscode/extensions.json`, and `.vscode/settings.json` sets `jlinkMcp.*` to the RT1189 values (device `MIMXRT1189xxx8_M33`, SWD, 1 MHz). The extension serves VS Code's own agents. Claude Code reads its MCP servers from its own configuration, so for Claude Code the server is registered in `.mcp.json` instead.
- **Separate provisioning environment.** Debug authentication and lifecycle/fuse operations (ADR-0016 S3, S7, phase E) use SPSDK with the J-Link, in a dedicated virtual environment.

## Consequences

- `rt1180_mcuboot.py flash` and `rt1180_logs.py flash` work with a J-Link without options. `rt1180_logs.py flash` with the J-Link reads through the XIP window, so the ROM must have configured FlexSPI (any bootable image). If nothing boots, LinkServer's flash driver is the fallback.
- **Whole-chip erase.** `flash initial` erases the whole NOR with the J-Link `erase` command (whole chip, 16 MiB), as LinkServer's `--erase-all` did.
- **Console.** The update test needs a 3.3-V USB-UART on J1 (or RTT, once added) for the console.
- **Cache-sensitive debugging is unreliable with the V10 probe.** Flashing, erasing, memory dumps and register reads are unaffected; they were all used for the AM254-4 update test. Interactive debugging with caches enabled may show stale code or data, through breakpoints, stepping or variable reads. This concerns the CM33 XCACHE and, above all, the CM7 I/D caches. For that work, use a J-Link model whose firmware handles ARMv8-M/ARMv7-M caches, or disable the caches in the debug build.
- **MCP server limits.** The MCP server is a third-party, single-maintainer project without npm provenance. It runs only on the developer's PC, at the pinned version. Its tools cannot pass the NXP `jlinkscript`; if connect or reset misbehaves on the RT1189 without it, the project scripts remain the reference path.

## Validation

At AM254-4 bring-up: connect, read the NOR JEDEC ID, run `flash initial`/`v2`/`v3` (MCUBOOT_UPDATE_TEST), `rt1180_logs.py flash`, and an MCP session (connect, halt, read memory, RTT once available). Record in `docs/boards/AM254-4.md` any J-Link setting that differs from the defaults.
