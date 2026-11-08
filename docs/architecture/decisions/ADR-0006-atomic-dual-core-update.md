# ADR-0006: Atomic dual-core firmware update

- Status: Accepted requirement; mechanism under implementation
- Date: 2026-11-08

## Context
CM33 and CM7 firmware are mutually compatible parts of one release.

## Decision
Treat the two images as one release pair. Validate both before activation; commit or roll back both as one transaction.

## Consequences
Manifest, journal and boot state are pair-level. Neither core may run while a pair transition is incomplete.

## Validation
Inject power loss at every swap/journal transition and verify recovery on hardware.
