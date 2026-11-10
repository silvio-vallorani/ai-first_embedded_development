# Architecture overview

## Scope

The template targets multiple boards based on the MIMXRT1189 family, including the MIMXRT1180-EVK and custom boards. The MCU, board, and build target are separate concepts.

## Layering

1. Application
2. Services
3. OS Abstraction Layer (OSAL)
4. HAL
5. Board Support Package (BSP)
6. MCU support
7. NXP MCUXpresso SDK

Applications must not depend directly on NXP SDK types or RTOS primitives. Hardware access is injected through narrow interfaces to make services testable on a host.

## Targets

Examples: `mimxrt1180_evk_m33`, `mimxrt1180_evk_m7`, `am254-4_m33`, `am254-4_m7`. A target is a board + core + configuration tuple.

## Core responsibilities

- CM33 is the first application core after the SoC Boot ROM. It configures resource/security policy (including TRDC as required), validates the paired release and controls CM7 startup.
- CM7 is performance-oriented. Critical code/data should be placed in ITCM/DTCM where budgets allow; OCRAM and external memory serve less latency-sensitive content. QSPI XIP is a supported option, not a mandatory placement for critical code.
- Do not infer that CM33 is automatically a secure enclave. Security depends on actual boot configuration, TrustZone, TRDC, provisioning, fuses and lifecycle state.

## Language policy

C11 for startup, interrupts, drivers, HAL, OSAL and boot code. C++17 may be used by applications and services. Python is used for build/release/documentation automation. Public C APIs must remain C++-linkage compatible where required.

## Build policy

CMake + Ninja, versioned NXP SDK manifests/releases, reproducible toolchain, and presets per board/core/configuration. The first scaffold only builds the host-testable boot logic; target cross-compilation is a subsequent integration step.
