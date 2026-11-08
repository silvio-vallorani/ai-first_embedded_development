# ADR-0010: NXP SDK and build system

- Status: Accepted
- Date: 2026-11-08

## Context
Builds must be reproducible across developers and CI. The reference hardware is the MIMXRT1180-EVK populated with MIMXRT1189, and the initial application exercises both Cortex-M33 and Cortex-M7 cores.

## Decision
- Use NXP MCUXpresso SDK **26.09.00**, pinned to manifest tag `v26.09.00`.
- Use the NXP-validated Arm GNU Toolchain **14.2.x** baseline for this SDK release.
- Manage the SDK's modular repositories with West; do not vendor the complete SDK into this repository.
- Use the NXP SysBuild flow for multicore examples so the primary and secondary images, startup, linker configuration and image packaging remain consistent.
- Use CMake + Ninja for project-level builds and keep host unit tests independent of the target SDK.
- Do not track moving `main` branches for reproducible builds.

## Version selection rationale
SDK 26.09.00 is the latest non-preview quarterly release available at the decision date. NXP also lists 26.06.00-LTS; that is the long-term-maintenance alternative when maintenance horizon is more important than using the newest quarterly baseline. The 26.12.00-pvw1 preview is not selected because preview releases are not at full release quality.

## Consequences
- The exact SDK manifest tag and toolchain baseline are recorded in `sdk/MCUXPRESSO_SDK_VERSION` and `docs/toolchains/MCUXPRESSO_SDK.md`.
- Board/core/config presets must match the selected SDK release.
- The RT1180-EVK multicore SysBuild project remains the source of truth for startup, linker scripts, secondary-core image placement and packaging.
- Updating the SDK requires an explicit review of component revisions, build output and hardware validation.

## Validation
Rebuild from a clean environment and record the SDK manifest revision, compiler version and generated artifact metadata. The initial dual-core example is not considered validated until both images build and the application is exercised on the EVK.
