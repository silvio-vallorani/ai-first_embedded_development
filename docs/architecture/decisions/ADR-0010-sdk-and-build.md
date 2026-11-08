# ADR-0010: NXP SDK and build system

- Status: Accepted
- Date: 2026-11-08

## Context
Builds must be reproducible across developers and CI.

## Decision
Use versioned MCUXpresso SDK manifests/releases with West, CMake and Ninja. Pin toolchain and SDK revisions; do not track moving `main` branches.

## Consequences
Board/core/config presets and SDK provenance must be recorded.

## Validation
Rebuild from a clean environment and compare artifact metadata.
