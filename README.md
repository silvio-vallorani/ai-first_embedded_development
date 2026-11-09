# AI-First Embedded Development Template

A multi-board, dual-core embedded firmware template for NXP i.MX RT1189-class devices.

## Baseline

- MCU: NXP MIMXRT1189 (Cortex-M33 + Cortex-M7).
- Target board: AM254-5 custom board ([board page](docs/boards/AM254-5.md)); reference board: MIMXRT1180-EVK.
- MCUXpresso SDK: **26.09.00** (pinned release; see [SDK setup](docs/toolchains/MCUXPRESSO_SDK.md)).
- Arm GNU Toolchain: **14.2.x**, as validated by NXP for this SDK release.
- External boot/update storage: 128-Mbit (16-MiB) Winbond W25Q128JW-family QSPI NOR reference device; exact ordering code and board revision must be confirmed against the populated board BOM.
- C11 platform and boot code; C++17 available to application/services; CMake + Ninja; host unit tests with CTest.
- OS-independent services through OSAL; bare-metal backend is the initial baseline.
- Release image is an inseparable CM33 + CM7 pair.
- Slot swap is the selected conceptual update strategy: the active release is restored to fixed logical execution addresses.

## Repository status

This is an initial architectural and host-testable software scaffold plus a first dual-core application source example. It is not a production bootloader, flash driver, or validated RT1189 board port. Hardware-dependent operations are deliberately behind interfaces. The dual-core LED example is built using the NXP-generated SysBuild project template.

Phase 1 of the MCUboot integration (ADR-0014) uses NXP's MCUboot port and reference flash layout. The CM33 + CM7 pair is one signed image, updated, confirmed or reverted as a unit. Images are signed with NXP's public development key, which provides no security. See the [MCUboot update test](docs/engineering/MCUBOOT_UPDATE_TEST.md).

## Open in VS Code

1. Clone this repository and open its root folder in VS Code.
2. Install the recommended extensions when prompted.
3. Install the host prerequisites: Git, Python 3.8+, CMake and Ninja.
4. Install Arm GNU Toolchain 14.2.x and set `ARMGCC_DIR` to its installation directory. Add its `bin` directory to `PATH`.
5. Run **Terminal → Run Task → Setup MCUXpresso SDK** once. The task creates the project-local `.venv`, installs the pinned West dependency from `requirements.txt`, and initializes the SDK in the sibling directory `../mcuxpresso-sdk` by default.
6. Run **Terminal → Run Task → Build RT1180 dual-core firmware** whenever you want to rebuild. The build task uses West from `.venv` automatically; manual activation is not required.

The SDK can be placed elsewhere by setting `MCUXPRESSO_SDK` to the workspace directory containing `.west`. On Windows, use the PowerShell scripts; on Linux/macOS, use the Bash scripts. The setup script does not install the Arm toolchain, CMake, Ninja or Git. The `.venv` directory is ignored by Git and is not shared with the SDK workspace.

The embedded build creates a guarded, generated overlay under the SDK workspace, based on NXP's `hello_world` multicore SysBuild template. The source files in this repository remain the authoritative application sources. The generated overlay is never allowed to overwrite an unmarked directory.

## Build and test on host

Use the existing CMake presets for host-side code:

```sh
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug
```

A Release host configuration is also available through `host-release`.

## Embedded SDK setup

See [MCUXpresso SDK setup](docs/toolchains/MCUXPRESSO_SDK.md) for prerequisites, toolchain environment variables, manual commands and troubleshooting.

## Documentation

See [Architecture](docs/architecture/ARCHITECTURE.md), [Memory map](docs/architecture/MEMORY_MAP.md), [Boot flow](docs/architecture/BOOT_FLOW.md), [Firmware update](docs/architecture/FIRMWARE_UPDATE.md), and [Unit testing](docs/engineering/UNIT_TESTING.md).

## Security and compliance

The project is not yet CRA compliant; the gap analysis and the plan are in [CRA gap analysis](docs/compliance/CRA_GAP_ANALYSIS.md). Every RT1189 CI build produces an SPDX SBOM and a vulnerability report (`rt1180-sbom` artifact). Report vulnerabilities as described in [SECURITY.md](SECURITY.md). Licensed under [BSD-3-Clause](LICENSE); NXP SDK components keep their own licenses.

## Decision records

Architectural decisions are maintained in `docs/architecture/decisions/`. An accepted ADR records a design decision, not proof of hardware validation.
