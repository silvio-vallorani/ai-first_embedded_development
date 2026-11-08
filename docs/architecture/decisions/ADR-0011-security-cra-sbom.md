# ADR-0011: Security, CRA and SBOM

- Status: Accepted
- Date: 2026-11-08

## Context
The product lifecycle requires vulnerability handling, component inventory and release evidence.

## Decision
Maintain SBOM, vulnerability-management, security-update and CRA evidence workflows. Generate SPDX or CycloneDX SBOMs and preserve build provenance.

## Consequences
CI can provide evidence and checks but does not by itself establish CRA compliance. Production signing keys must not be exposed to generic hosted runners.

## Validation
Review the compliance workflow against the product's actual market, release lifecycle and legal assessment.
