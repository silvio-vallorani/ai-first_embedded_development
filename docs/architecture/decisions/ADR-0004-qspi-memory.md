# ADR-0004: QSPI NOR memory architecture

- Status: Accepted (logical map provisional)
- Date: 2026-11-08

## Context
The baseline uses a 128-Mbit (16-MiB) QSPI NOR. The EVK software/hardware references identify a Winbond W25Q128JW-family part.

## Decision
Reserve the first 8 MiB for boot, paired application slots and system data. Reserve the second 8 MiB for future sector replacement/redundancy. Use 4-KiB erase sectors as the logical allocation unit.

## Consequences
The proposed map is documented in `MEMORY_MAP.md`. NOR does not automatically remap defective sectors. Boot ROM/XIP compatibility and actual board BOM remain to be confirmed.

## Validation
Read JEDEC ID and SFDP on hardware; confirm fitted part, geometry, erase commands, protection features and board revision.
