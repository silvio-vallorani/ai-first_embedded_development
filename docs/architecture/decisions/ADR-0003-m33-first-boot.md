# ADR-0003: CM33-first dual-core boot

- Status: Accepted (hardware validation pending)
- Date: 2026-11-08

## Context
The RT1189 boot architecture uses CM33 as the first application core; CM33 configures system/resource policy before releasing CM7.

## Decision
CM33 owns paired-image validation, security/resource setup and CM7 startup.

## Consequences
CM7 must not start before pair validation and required TRDC/memory setup.

## Validation
Verify Boot ROM behavior, TRDC configuration, TCM ECC initialization and CM7 reset/start sequencing on the EVK.
