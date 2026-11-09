# ADR-0001: Multi-board architecture

- Status: Accepted
- Date: 2026-11-08

## Context
The firmware must support the MIMXRT1180-EVK and custom MIMXRT1189 boards without coupling application code to a single PCB.

## Decision
Separate MCU support, board support and build targets. A target identifies board + core + configuration.

## Consequences
Board pinmux, clocks, flash geometry, boot straps and memory maps remain board-owned. Common MCU capabilities remain in the MCU layer.

## Validation
Add a second custom-board configuration and ensure shared services compile unchanged.

2026-11-09: the AM254-5 custom board (`boards/am254-5`, `docs/boards/AM254-5.md`) is the second board. It reuses the SDK EVK board support where the hardware matches and owns its LED definition and CM7 pin mux; the application and shared services compile unchanged for both boards. Hardware validation pending.
