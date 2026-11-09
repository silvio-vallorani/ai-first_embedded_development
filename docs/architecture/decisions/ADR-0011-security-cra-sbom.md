# ADR-0011: Security, CRA and SBOM

- Status: Accepted
- Date: 2026-11-08

## Context
The product lifecycle requires vulnerability handling, component inventory and release evidence.

## Decision
Maintain SBOM, vulnerability-management, security-update and CRA evidence workflows. Generate SPDX or CycloneDX SBOMs and preserve build provenance.

### Implementation (2026-11-09, CRA plan phase A)

The gap analysis and plan are in `docs/compliance/CRA_GAP_ANALYSIS.md`.

- **SBOM**: SPDX 2.3 JSON from `scripts/generate_sbom.py`, built from `compile_commands.json` of the bootloader and the release image. It lists every SDK repository whose sources or headers are used (with its git commit), the linked third-party components, the toolchain and the pinned Python tools. Third-party versions are read from the sources because NXP's SBOMs can lag. A newly linked SDK repository stops the generator until it is classified. The SBOM is validated with `spdx-tools` in CI.
- **Vulnerability scanning**: grype scans the SBOM on every RT1189 build and weekly. Fixable high and critical findings fail the build.
- **Provenance**: GitHub build provenance attestations cover the signed images, the bootloader and the SBOM.
- **CI supply chain**: GitHub Actions pinned by commit SHA, workflow tokens read-only by default, Dependabot for actions and pip, gitleaks secret scanning, mkdocs pinned.
- **Release guard**: `scripts/rt1180_mcuboot.py check-release` rejects any image that NXP's public development key validates, and any image that is not a valid signed MCUboot image. CI proves both directions.
- **Disclosure and support**: `SECURITY.md` (GitHub private vulnerability reporting, 5-year support period, CRA Art. 14 reporting), `LICENSE` (BSD-3-Clause), `docs/compliance/` (vulnerability handling, risk assessment, Annex VII index).

## Consequences
CI can provide evidence and checks but does not by itself establish CRA compliance. Production signing keys must not be exposed to generic hosted runners.

## Validation
Review the compliance workflow against the product's actual market, release lifecycle and legal assessment.
