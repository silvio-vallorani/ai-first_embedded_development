# ADR-0005: CM7 TCM-first execution

- Status: Proposed
- Date: 2026-11-08

## Context
CM7 is the performance-critical core.

## Decision
Place real-time code in ITCM and critical data/stack in DTCM where feasible. Use OCRAM and external memory for less latency-sensitive content. QSPI XIP remains optional.

## Consequences
Linker sections and memory-budget reports are required. Exact TCM sizes and usable budgets must come from the selected silicon configuration and linker/runtime overhead.

## Validation
Measure worst-case execution latency and verify ECC initialization and TRDC permissions.
