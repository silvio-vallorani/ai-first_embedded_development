# AI-First Embedded Development Template

A multi-board, dual-core embedded firmware template for NXP i.MX RT1189-class devices.

## Baseline

- MCU: NXP MIMXRT1189 (Cortex-M33 + Cortex-M7).
- Reference board: MIMXRT1180-EVK.
- External boot/update storage: 128-Mbit (16-MiB) Winbond W25Q128JW-family QSPI NOR reference device; exact ordering code and board revision must be confirmed against the populated board BOM.
- C11 platform and boot code; C++17 available to application/services; CMake + Ninja; host unit tests with CTest.
- OS-independent services through OSAL; bare-metal backend is the initial baseline.
- Release image is an inseparable CM33 + CM7 pair.
- Slot swap is the selected conceptual update strategy: the active release is restored to fixed logical execution addresses.

## Repository status

This is an initial architectural and host-testable software scaffold. It is not a production bootloader, signed-image implementation, flash driver, or validated RT1189 board port. Hardware-dependent operations are deliberately behind interfaces.

## Build and test on host

```sh
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug
```

See [Architecture](docs/architecture/ARCHITECTURE.md), [Memory map](docs/architecture/MEMORY_MAP.md), [Boot flow](docs/architecture/BOOT_FLOW.md), [Firmware update](docs/architecture/FIRMWARE_UPDATE.md), and [Unit testing](docs/engineering/UNIT_TESTING.md).

## Decision records

Architectural decisions are maintained in `docs/architecture/decisions/`. An accepted ADR records a design decision, not proof of hardware validation.
