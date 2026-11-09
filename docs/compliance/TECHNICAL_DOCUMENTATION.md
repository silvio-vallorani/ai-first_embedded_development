# Technical documentation index (CRA Annex VII)

Where each element of the CRA technical documentation lives. Items marked *open* are produced for the final product.

| Annex VII item | Location | Status |
|---|---|---|
| 1. General description: intended purpose, versions, user information (Annex II) | Product documentation | Open: product not defined |
| 2(a). Design and development, system architecture | [Architecture](../architecture/ARCHITECTURE.md), [Memory map](../architecture/MEMORY_MAP.md), [Boot flow](../architecture/BOOT_FLOW.md), [Firmware update](../architecture/FIRMWARE_UPDATE.md), [ADRs](../architecture/decisions/README.md) | Available for the current phase |
| 2(b). Vulnerability handling, SBOM, coordinated disclosure, contact, secure update distribution | [Vulnerability handling](VULNERABILITY_HANDLING.md), `SECURITY.md`, `scripts/generate_sbom.py` | Available; update distribution open (plan phase F) |
| 2(c). Production and monitoring processes | CI workflows (`.github/workflows/`), [Unit testing](../engineering/UNIT_TESTING.md) | Partial: production programming and provisioning open (phase E) |
| 3. Cybersecurity risk assessment | [Risk assessment](RISK_ASSESSMENT.md) | Initial version |
| 4. Support period determination | `SECURITY.md`: 5 years from release | Set; confirm against the product's expected use |
| 5. Harmonised standards / common specifications applied | — | Open: select when harmonised standards for the CRA are published (e.g. EN 18031 family, IEC 62443-4-2 as reference) |
| 6. Test reports | CI results, [MCUboot update test](../engineering/MCUBOOT_UPDATE_TEST.md) | Partial: security HIL tests open (phases C–E) |
| 7. EU declaration of conformity | — | Open |
| 8. SBOM (on request of market surveillance) | `rt1180-sbom` CI artifact, release evidence | Available per build |

## Product classification

To be decided for the final product. The MIMXRT1189 is itself an "important" product (Annex III, microcontrollers with security-related functionalities); that obligation lies with NXP as its manufacturer. The classification of the end product depends on its function: if it is not listed in Annex III or IV, it is a default-category product and the internal-control procedure (module A) applies.

## Plan and status

[CRA gap analysis and compliance plan](CRA_GAP_ANALYSIS.md).
