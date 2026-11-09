# CRA gap analysis and compliance plan

- Date: 2026-11-08 (analysis), updated as phases complete
- Scope: this repository (firmware, build, CI, documentation) for the MIMXRT1189 dual-core product line
- Basis: Regulation (EU) 2024/2847 (Cyber Resilience Act), Annex I parts I and II, Annex VII

This is an engineering gap analysis, not a legal assessment. CRA obligations apply to the **final product** placed on the EU market; many of them are processes (risk assessment, technical documentation, EU declaration of conformity, support) that live partly outside the firmware.

Key dates: vulnerability and incident **reporting obligations (Art. 14) apply from 2026-09-11**; the remaining obligations apply from 2027-12-11. If a product running this firmware is already on the market, the reporting process is required now.

## Status at analysis date

| Area | Status | Present | Missing |
|---|---|---|---|
| SBOM | Not met | Intent in ADR-0011; NXP SDK ships `SBOM.spdx.json`; SDK, toolchain and pip versions pinned | No firmware SBOM, no vulnerability scanning, no build provenance; GitHub Actions pinned by tag, not SHA |
| Key management | Not met | Images signed with NXP's **public** ECDSA-P256 development key (`scripts/rt1180_mcuboot.py`); ADR-0011 forbids production keys on hosted runners | Unsigned MCUboot ROM container (OEM Open lifecycle), no SRK fuses, no production keys, no HSM/signing service, no rotation/revocation, open debug port without authentication, no guard against releasing dev-key images |
| Anti-rollback | Not met | `security_version` floor in host-tested code (not used on target) | MCUboot downgrade prevention and hardware counter not enabled (ADR-0008 not met) |
| TrustZone | Not met | Cautionary notes only | CM33 runs as a single world: no Secure/Non-Secure partition, no SAU configuration in the project |
| Encrypted image with runtime decryption | Not met | — | Images in plaintext in flash and in transit. The RT1189 has OTFAD in FlexSPI1/2 for XIP decryption (and IEE for other regions). The SDK MCUboot `encrypted_xip` OTFAD backend wraps keys with CAAM, which the RT1189 lacks (it has the EdgeLock Enclave), and works only in overwrite mode, not with the swap-move used today. *Corrected 2026-11-09: the first version named IEE as the only engine.* |
| Per-core permissions (TRDC) | Not met | Required by ADR-0003/ADR-0005; SDK `board.c` provides `BOARD_RequestTRDC` and `APP_CommonTrdcDACSetting` | No TRDC policy: CM7, DMA and DAP keep default permissions; CM7 can write flash, CM33 RAM and peripherals |
| Vulnerability handling (Annex I part II) | Not met | — | No `SECURITY.md` / coordinated vulnerability disclosure, no support period, no ENISA reporting procedure, no `LICENSE` |
| Attack surface / data | Partial | Persistent logs with boot events | Unauthenticated `logdump` UART command; security events (signature rejected, rollback, confirm) not logged; no secure data erasure |
| Testing and quality | Met for current scope | Unit tests with sanitizers and coverage, power-cut injection, formatting, reproducible CI build | Security HIL tests |

## Plan

Encryption, TrustZone and the flash layout all change the memory map and image format, so they are decided **together in one ADR** (phase B) before implementation, to avoid reworking the layout twice.

### Phase A — process and repository (no hardware)

1. SPDX 2.3 SBOM generated in CI: project sources, the SDK components actually linked (filtered from NXP's `SBOM.spdx.json`), toolchain and build tools; attached to the build artifacts.
2. Vulnerability scanning of the dependencies in CI; build provenance attestation for the firmware artifacts.
3. GitHub Actions pinned by commit SHA, Dependabot for actions and pip, secret scanning (gitleaks).
4. Release guard: an image that verifies with the public NXP development key is rejected as a release.
5. `SECURITY.md` (coordinated disclosure through GitHub private vulnerability reporting, 5-year support period, ENISA reporting timeline) and `LICENSE` (BSD-3-Clause).
6. CRA documentation skeleton in `docs/compliance/`: risk assessment, Annex VII technical documentation index, product classification.
7. Security event logging (MCUboot image state, confirmation), secure erasure of the log region, UART log commands removable from release builds.

### Phase B — ADR "Security architecture phase 2" (decision only)

Decides together: the new flash layout (including the planned recovery stage and redundant MCUboot), the TrustZone partition, the encryption strategy, the update strategy (encryption likely forces overwrite/remap instead of swap-move, which affects the ADR-0006 rollback), and the key inventory.

Draft: [ADR-0016](../architecture/decisions/ADR-0016-security-architecture-phase2.md). Preceded by EVK spikes: OTFAD XIP decryption with a run-time key, FlexSPI remap, ELE key blobs and counters, TrustZone with MCUboot, TRDC faults, image sizes. Requirements confirmed 2026-11-09: confidentiality, rollback, images larger than RAM, authenticated debug. Proposed strategy: direct XIP from two OTFAD-encrypted slots (per-slot builds) with MCUboot revert; candidates travel as MCUboot encrypted images and are re-encrypted for OTFAD in place by MCUboot with a power-cut-safe journal; per-slot OTFAD keys as ELE blobs. Highest-risk items: the OTFAD backend on the ELE (the SDK one uses CAAM) and protecting the MCUboot decryption key.

### Phase C — per-core permissions with TRDC (hardware)

Separate domains for CM33 Secure, CM33 Non-Secure, CM7, DMA and DAP. Memory and peripheral rules: CM7 without write access to flash, MCUboot, the log region and CM33 RAM; a single shared IPC region; LED/GPIO assigned to CM7; UART, FlexSPI and the EdgeLock Enclave MU reserved to CM33 Secure. Configuration **locked before CM7 starts**. The permission table lives in a host-testable module; the HIL test makes CM7 attempt a forbidden access and expects a bus fault (as in SDK `secure_faults_trdc`).

### Phase D — TrustZone on CM33

Secure world: boot, EdgeLock Enclave and crypto, image confirmation, keys, log writing. The application runs Non-Secure and reaches Secure services only through declared entry functions (NSC veneers). Starting point: SDK `trustzone_examples/hello_world`. The MCUboot image then contains CM33 Secure + CM33 Non-Secure + CM7.

### Phase E — keys and lifecycle (writes irreversible fuses)

Separate keys for the EdgeLock Enclave boot container SRKs (AHAB), MCUboot signing, image encryption and debug authentication. Keys in an HSM/KMS; signing in a manually approved release step, never on hosted runners. Signed MCUboot container and SRK hash in fuses, hardware monotonic counter for anti-rollback (completes ADR-0008), authenticated debug, OEM Closed lifecycle. Only on a dedicated, sacrificial EVK and with explicit authorization for every fuse operation.

### Phase F — field update

UART/xmodem transport, then network, with secure distribution of updates (Annex I).

## Not verified at analysis date

- Actual ROM/EdgeLock Enclave behavior on the EVK (TRDC defaults, secure/non-secure FlexSPI aliases, IEE write support): the board was not connected.
- The CRA classification of the final product (default, important or critical), which determines the conformity assessment procedure.

## Progress

| Phase | Status |
|---|---|
| A | Implemented 2026-11-09 (ADR-0011, ADR-0015). The first scan found fixable high vulnerabilities in the signing tools (cryptography 44.0.2, cbor2 5.6.5; build-time only, not in the firmware): updated to 50.0.2 and 5.9.0 |
| B | Draft ADR-0016 (proposed); EVK spikes S1–S6 pending |
| C–F | Not started |
