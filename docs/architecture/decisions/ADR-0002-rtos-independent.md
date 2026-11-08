# ADR-0002: RTOS-independent architecture

- Status: Accepted
- Date: 2026-11-08

## Context
The product must not require a specific RTOS on both cores.

## Decision
Use a stable OSAL. Start with a bare-metal backend; add FreeRTOS or another backend without changing application services.

## Consequences
No RTOS headers or native primitives may leak into services or applications.

## Validation
Compile service-level tests with the host backend and at least one embedded backend when integrated.
