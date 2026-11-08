# ADR-0007: Immutable recovery and redundant MCUboot

- Status: Accepted architectural target; Boot ROM support assumed, not verified
- Date: 2026-11-08

## Context
The active MCUboot image must be updateable without making power loss during erase/program unrecoverable.

## Decision
Use an immutable recovery first stage and redundant MCUboot copies. The recovery stage selects a valid bootloader copy or enters a recovery path.

## Consequences
The current memory map reserves 256 KiB for the recovery-stage concept and 512 KiB each for MCUboot A/B. These are planning budgets, not a proven ROM chain.

## Validation
Confirm the Boot ROM image format, boot source selection and fallback behavior on the exact RT1189 silicon/EVK. If unsupported, revise the architecture before production.
