# ADR-0013: Slot swap and 50-percent sector reserve

- Status: Accepted direction; implementation and hardware validation pending
- Date: 2026-11-08

## Context
The 128-Mbit NOR is divided equally between operational data and a sector-replacement reserve. The user prefers slot swap so active images always execute from the same logical addresses.

## Decision
Allocate 8 MiB to boot/application/system partitions and 8 MiB to a future spare-sector pool. Slot 1 and Slot 2 are each 3 MiB and each contain a 1-MiB CM33 image area plus a 2-MiB CM7 image area. A 4-KiB scratch sector and pair-level journal are budgeted inside the metadata region.

## Consequences
A resumable sector swap and durable journal are required. The host code in this initial scaffold tests orchestration only; the low-level flash algorithm is not implemented. The spare pool is not automatically usable by Boot ROM or direct XIP.

## Validation
Measure final signed images; verify erase/program geometry and power-cut behavior; validate Boot ROM/MCUboot chain; design and qualify sector remapping separately.
