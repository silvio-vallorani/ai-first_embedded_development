# ADR-0008: Release and debug version policy

- Status: Accepted
- Date: 2026-11-08

## Context
Production devices must resist unauthorized rollback while engineering devices need downgrade capability.

## Decision
Production releases are upgrade-only using a hardware-backed monotonic counter or equivalent. Debug downgrade requires a separate authorization policy/key.

## Consequences
Manifest version alone is not the anti-rollback root. Provisioning and lifecycle state are part of security design.

## Validation
Test counter advancement, interrupted updates and debug authorization on target hardware.
