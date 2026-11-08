# ADR-0009: Factory data protection

- Status: Proposed
- Date: 2026-11-08

## Context
Some factory-provisioned information must never change during product life.

## Decision
Separate secrets/root-of-trust from non-secret factory records. Prefer OTP/eFuse or a secure element for secrets. Use hardware-enforced permanent protection for NOR records only if the exact device and provisioning flow support it.

## Consequences
A read-only software API is not sufficient evidence of immutability.

## Validation
Define factory provisioning, verify lock permanence and perform post-provisioning readback tests.
