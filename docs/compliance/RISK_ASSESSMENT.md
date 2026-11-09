# Cybersecurity risk assessment (initial)

CRA Art. 13(2) and Annex VII point 3. Initial version for the MIMXRT1189 dual-core platform; it is revised with every phase of [the compliance plan](CRA_GAP_ANALYSIS.md) and before each production release. The intended purpose and operating environment of the final product are still to be defined; they can change likelihoods and impacts below.

## Assets

| Asset | Why it matters |
|---|---|
| Firmware images (MCUboot, CM33, CM7) | Integrity decides what code runs; confidentiality protects the IP |
| Signing, encryption and debug keys | Their compromise defeats every other control |
| Update path (secondary slot, transport) | Entry point for malicious or old firmware |
| Device state: lifecycle, fuses, security counter | Root of anti-rollback and debug protection |
| Persistent logs | Diagnostics and security evidence; may contain operational data |
| CM33/CM7 isolation | A fault or compromise in one core must not take over the other |

## Threats and treatment

| # | Threat | Today | Treatment (plan phase) |
|---|---|---|---|
| T1 | Malicious image installed (attacker-signed) | Signature checked by MCUboot, but the key is NXP's **public** dev key: anyone can sign | Production keys in HSM, release guard (A, done), SRK fuses and signed boot container (E) |
| T2 | Rollback to an old vulnerable version | Any signed version accepted | Hardware monotonic counter, MCUboot downgrade prevention (E) |
| T3 | Firmware read out and reverse engineered | Plaintext in flash and in transit | MCUboot-encrypted transport, OTFAD-encrypted executable slots with per-slot ELE-wrapped keys (B: ADR-0016) |
| T4 | Debug port used to read or modify the device | Debug open (OEM Open) | OEM Closed lifecycle with ELE debug authentication: scoped, optionally device-bound credentials (E, ADR-0016) |
| T5 | Compromised or faulty CM7 corrupts CM33, flash or peripherals | No TRDC policy | Per-core TRDC domains, locked before CM7 starts (C) |
| T6 | Application bug exposes boot, keys or update state | Single CM33 world | TrustZone Secure/Non-Secure split (D) |
| T7 | Bootloader corrupted (power loss, bad update) | Single MCUboot copy, no recovery | Recovery stage and redundant MCUboot (B layout, ADR-0007) |
| T8 | Logs read or wiped over the debug UART | `logdump` unauthenticated | Commands removable from release builds, secure erase (A); authenticated service access (E) |
| T9 | Vulnerable third-party component shipped | Not tracked | SBOM and vulnerability scanning (A) |
| T10 | Tampered build or CI supply chain | Pinned SDK/toolchain/pip, actions by tag | Actions pinned by SHA, Dependabot, build provenance, secret scanning (A) |
| T11 | Interrupted update bricks the device | Swap-move with test/confirm and revert | HIL power-cut tests (MCUBOOT_UPDATE_TEST), keep during the encryption redesign (B) |

## Residual risk

Until phases C–E are complete the platform is **not suitable for production deployment**: T1, T3, T4, T5 and T6 are open. This is stated in `SECURITY.md` (development builds are not supported releases).
