# ADR-0006: Atomic dual-core firmware update

- Status: Accepted requirement; mechanism under implementation
- Date: 2026-11-08

## Context
CM33 and CM7 firmware are mutually compatible parts of one release.

## Decision
Treat the two images as one release pair. Validate both before activation; commit or roll back both as one transaction.

## Consequences
Manifest, journal and boot state are pair-level. Neither core may run while a pair transition is incomplete.

The pair transaction is the journal state machine described in `FIRMWARE_UPDATE.md`: `PREPARED → SWAPPING → TRIAL → COMMITTING → COMMITTED`, with `REVERTING → REVERTED` when verification fails or the probation budget is exhausted. Once `COMMITTING` is persisted the commit is completed, never reverted, because the commit may already have advanced the anti-rollback counter. Sector swaps must be idempotent per direction so that resume after power loss never undoes a completed sector.

## Validation
Inject power loss at every swap/journal transition and verify recovery on hardware.
