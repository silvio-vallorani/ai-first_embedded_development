# Architecture Decision Records

Statuses: Proposed, Accepted, Deprecated, Superseded. Accepted means the architectural direction is agreed; it does not imply hardware validation. Update ADRs in the same change as implementation.

| ADR | Decision | Status |
|---|---|---|
| [0001](ADR-0001-multi-board.md) | Multi-board architecture | Accepted |
| [0002](ADR-0002-rtos-independent.md) | RTOS-independent architecture | Accepted |
| [0003](ADR-0003-m33-first-boot.md) | CM33-first dual-core boot | Accepted; validation pending |
| [0004](ADR-0004-qspi-memory.md) | QSPI NOR memory architecture | Accepted; map is initial |
| [0005](ADR-0005-cm7-tcm-first.md) | CM7 TCM-first execution | Proposed |
| [0006](ADR-0006-atomic-dual-core-update.md) | Atomic dual-core update | Accepted requirement |
| [0007](ADR-0007-recovery-and-redundant-mcuboot.md) | Immutable recovery + redundant MCUboot | Accepted target; ROM support assumed |
| [0008](ADR-0008-version-policy.md) | Upgrade/downgrade policy | Accepted |
| [0009](ADR-0009-factory-data.md) | Factory data protection | Proposed |
| [0010](ADR-0010-sdk-and-build.md) | SDK and build system | Accepted |
| [0011](ADR-0011-security-cra-sbom.md) | Security, CRA and SBOM | Accepted |
| [0012](ADR-0012-documentation-ai-workflow.md) | Documentation and AI workflow | Accepted |
| [0013](ADR-0013-slot-swap-and-sector-reserve.md) | Slot swap and 50% sector reserve | Accepted direction; implementation pending |
