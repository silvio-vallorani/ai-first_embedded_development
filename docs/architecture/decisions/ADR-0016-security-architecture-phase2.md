# ADR-0016: Security architecture, phase 2 (layout, TrustZone, encryption, keys, debug)

- Status: **Proposed — draft**. Decisions below are proposals; each depends on an AM254-5 spike (S1–S7) before acceptance.
- Date: 2026-11-09
- Plan: phase B of `docs/compliance/CRA_GAP_ANALYSIS.md`; feeds phases C (TRDC), D (TrustZone), E (keys, lifecycle, debug).

## Requirements

Confirmed by the product owner on 2026-11-09:

| # | Requirement |
|---|---|
| R1 | Firmware confidentiality: no plaintext image in flash or in transit |
| R2 | Rollback: an update that is not confirmed returns to the previous release |
| R3 | Images may be larger than the on-chip RAM: the design executes in place (XIP) from external flash |
| R4 | Debug access stays possible on production devices, but only after authentication |

## Context

Phase 1 (ADR-0014) runs NXP's MCUboot reference layout: one MCUboot copy, swap-move, plaintext images signed with NXP's public development key, a single CM33 security world, no TRDC policy and an open debug port. It meets R2 only. The risk assessment leaves T1, T3, T4, T5, T6 and T7 open.

Facts established from the pinned SDK (26.09.00) and MCUboot 2.4.0, not yet on hardware:

- **Encryption engines.** The RT1189 has **OTFAD1/OTFAD2 integrated in FlexSPI1/FlexSPI2**: on-the-fly AES-CTR decryption of XIP reads, with several contexts (key + counter + address range each). The CTR block includes the system address, so a ciphertext is valid only at the address it was produced for. The RT1189 also has an **IEE** with **IEE_APC** regions; the SDK `demo_apps/iee_apc` uses it on internal RAM, not on FlexSPI XIP.
- **SDK encrypted XIP for MCUboot.** The `encrypted_xip` extension has an OTFAD backend that wraps the OTFAD key with **CAAM** blobs (RT1160/RT1170). The RT1189 has no CAAM; its key services are in the **EdgeLock Enclave (ELE)**. The backend offers only `ENCRYPTED_XIP_OVERWRITE` (no rollback); `ENCRYPTED_XIP_REMAP` is limited to the RW612.
- **MCUboot modes.** MCUboot supports `DIRECT_XIP` with `DIRECT_XIP_REVERT` (boot the newest valid slot in place, fall back to the other one if the new image is not confirmed). The NXP port enables direct XIP only together with FlexSPI remap, and remap is offered for other devices, not the RT1189.
- **TrustZone.** FlexSPI1 is aliased at 0x28000000 (non-secure) and 0x38000000 (secure). The SDK `trustzone_examples/hello_world` places non-secure code at 0x28400000–0x287FFFFF and the NSC veneers at 0x383FFC00; its `tzm_config.c` (MCUXpresso Config Tools, TEE) configures SAU **and** the TRDC memory/block checkers. `secure_faults_trdc` demonstrates TRDC faults.
- **CM7** has no TrustZone; its isolation comes only from TRDC. Its image is copied into TCM and runs there in plaintext.
- `ele_crypto` examples exist for the RT1180 (RNG, key blobs, fuses through the ELE).

## Decision (proposed)

### 1. Update strategy and encryption

| Option | Confidentiality (R1) | Rollback (R2) | Large images (R3) | Verdict |
|---|---|---|---|---|
| Swap-move, plaintext (phase 1) | No | Yes | Yes | Excluded |
| Overwrite + OTFAD (SDK `ENCRYPTED_XIP_OVERWRITE`) | Yes | **No** | Yes | Excluded |
| RAM load of encrypted images | Yes | Yes | **No** | Excluded |
| FlexSPI remap + OTFAD | Yes | Yes | Yes | Not available for the RT1189 in the SDK port; kept only if S2 finds hardware remap |
| **Direct XIP + OTFAD, per-slot builds (proposed)** | Yes | Yes | Yes | Proposed |

**Proposal: direct XIP from two OTFAD-encrypted slots with MCUboot `DIRECT_XIP_REVERT`.**

- **Two executable slots.** Slot A and slot B are both execution locations. Each release is linked twice, once per slot address, and both variants are signed with the same version. The device receives the variant for its inactive slot. Because the OTFAD counter is bound to the address, a slot's ciphertext is meaningful only in that slot, which matches the per-slot build.
- **Transport.** The candidate travels as an MCUboot encrypted image (ECIES-P256 + AES-128-CTR), so it is ciphertext on the wire and while it is written into the inactive slot.
- **Install (re-encryption) in MCUboot.** At the next reset MCUboot verifies the candidate, generates a fresh OTFAD key with the ELE RNG, and transforms the slot in place from MCUboot encryption to OTFAD encryption, sector by sector. Progress is recorded in a journal in the metadata region, so a power loss resumes the transform. The journal reuses the host-tested state-machine design of `aied_slot_swap` (direction-idempotent per sector, ADR-0006/ADR-0013). The active slot is never touched, so an interrupted or failed install leaves the previous release bootable.
- **Keys.** Each slot has its own OTFAD key and counter, stored only as an **ELE key blob** in the metadata region. At every boot MCUboot unwraps the blob of the selected slot and loads it into an OTFAD context. The SDK backend's CAAM calls are replaced by ELE calls (S1, S3).
- **Boot and rollback.** MCUboot boots the newest valid slot in place as a test image. The application confirms it only after the CM7 pair check (as today). If it is not confirmed, the next reset reverts to the other slot, which is still encrypted with its own key (R2).
- **What is decrypted, and where.** The only plaintext is what the CPU reads through OTFAD and the CM7 code in TCM. Both are protected by TRDC, TrustZone and authenticated debug (sections 3–5).
- **Root of confidentiality.** The MCUboot ECIES private key must never be readable from flash. Either MCUboot is stored as an **ELE-encrypted boot container**, or the key is held by the ELE and used through it (S1).
- **Costs.** There are two builds and two signatures per release (automated in CI), plus a port of the NXP MCUboot glue: direct XIP without remap, the OTFAD backend on the ELE, and the in-place transform with its journal. OTFAD adds XIP read latency, measured in S1.

### 2. Flash layout

Use the target layout of `MEMORY_MAP.md` with these changes:

- **Slots A and B are both executable.** They replace the primary/secondary distinction. Each slot holds one MCUboot image: CM33 Secure + CM33 Non-Secure + the embedded CM7 image. There are no separate CM33 and CM7 areas, and pair atomicity (ADR-0006) stays a property of the single image. The 3-MiB slot size is the starting budget; images larger than RAM are expected (R3).
- **The metadata region holds:**
  - the OTFAD key blob and counter of each slot;
  - the install journal;
  - the MCUboot image trailers if they are not kept in the slots.

  There is no swap scratch area, because nothing is swapped.
- **Logs, application NVM and factory data stay plaintext.** They are not executable images. Their confidentiality, if required, is a separate decision; IEE or an ELE-wrapped key are the candidates.
- **The recovery stage and MCUboot A/B** stay as planned (ADR-0007). MCUboot is the component that holds the decryption capability, so its own redundancy matters more than before.

To confirm: final image sizes and XIP performance (S6) and the ROM fallback behaviour behind ADR-0007.

### 3. TrustZone partition (CM33)

| Secure world | Non-secure world |
|---|---|
| Start-up, clocks, SAU + TRDC configuration and lock, CM7 start | Application logic, communication, CM7 events |
| ELE messaging unit, key blobs, RNG | — |
| Flash driver for the inactive slot, metadata and logs; image confirmation | Download of the update through an NSC call |
| Persistent log writer (`aied_log_file`) | `LOG_*` calls reach the log core through an NSC call |

- **Initial NSC entry points:** `sec_log_write`, `sec_image_confirm`, `sec_image_state`, `sec_update_write`, `sec_update_mark_pending`, `sec_version`. Each one validates its arguments, checking non-secure pointers with `cmse_check_address_range`.
- **Writes only to the inactive slot.** `sec_update_write` accepts writes only to the inactive slot, so the non-secure world can never alter the running image.
- **Configuration generated per slot.** The SAU/TRDC configuration is generated with MCUXpresso Config Tools (TEE) and kept as generated source. There is one variant per slot address, because the code regions differ.

### 4. Per-core permissions (input to phase C)

| Master | Domain | Flash | CM33 secure RAM | Shared IPC RAM | Peripherals |
|---|---|---|---|---|---|
| CM33 Secure | 0 | All, read/write | Read/write | Read/write | All |
| CM33 Non-Secure | 1 | Its own NS code, read/execute only | None | Read/write | Application peripherals |
| CM7 | 2 | **None** (runs from TCM) | None | Read/write | LED GPIO, its own peripherals |
| DMA, NETC, USB | 3 | None unless a use case needs it | None | Per use case | Per use case |
| DAP (debug) | — | Per the authenticated debug scope (section 5) | Per scope | Per scope | Per scope |

TRDC is configured and **locked before CM7 starts**. The table is kept in a host-testable form and compared with the generated configuration in a unit test.

### 5. Authenticated debug (R4)

- **Lifecycle.** Production devices are moved to the OEM Closed lifecycle, in which the debug port is locked by the ELE.
- **Access with a credential.** Access is granted only through **ELE debug authentication**, a challenge–response flow. The debugging host proves possession of a **debug credential key (DCK)**. Its certificate is signed by the OEM root of trust, whose hash is in fuses. Tooling is SPSDK `nxpdebugmbox`, to be confirmed in S7.
- **What a credential carries:**
  - a **scope**: non-secure world only, or full access, the latter for failure analysis;
  - optionally the **device UUID**, so a leaked credential works on one unit only.
- **Credential lifetime.** Credentials are issued per engineer or service centre and revoked by rotating the DCK.
- **Logging and residual risk.** Every authenticated debug session is a security event (`LOG_SEC_WRN`, if the firmware can observe it). Authenticated full-scope debug can read decrypted code and RAM: this is by design and is the reason for scoped, per-device credentials.

### 6. Key inventory

| Key | Algorithm | On device | Off device | Rotation / revocation |
|---|---|---|---|---|
| SRK set (ELE boot container) | ECDSA P-256/P-384 | Hash in fuses | Offline HSM | SRK revocation fuses |
| MCUboot image signing | ECDSA P-256 | Public key in signed MCUboot | HSM, release step only | New key by MCUboot update (needs MCUboot A/B) |
| Image transport encryption (ECIES) | P-256 | ELE-protected (S1) | HSM | With MCUboot update |
| OTFAD key, per slot | AES-128 | ELE blob in metadata | Never | New key at every install |
| Debug credential key (DCK) | ECDSA | Certificate chained to the OEM root of trust | HSM / service tooling | Rotate DCK; per-device scope optional |
| Development | NXP public keys | EVKs only | SDK | Never in release (`check-release`) |

## Spikes on the AM254-5

| Spike | Question | Done when |
|---|---|---|
| S1 OTFAD XIP with ELE keys | Can CM33 execute in place from an OTFAD-encrypted FlexSPI1 region whose key is unwrapped from an ELE blob at boot? OTFAD read latency? Can the ELE hold or protect the ECIES key, or boot MCUboot from an encrypted container? | Encrypted test image runs; the region reads as ciphertext with OTFAD off; latency measured; no key visible in a flash dump |
| S2 Direct XIP without remap | MCUboot `DIRECT_XIP` + `DIRECT_XIP_REVERT` on the RT1189 with two per-slot builds; confirm the RT1189 has no FlexSPI remap | v1 in A → v2 in B confirmed; v3 in A not confirmed is reverted to B |
| S3 ELE key services | RNG, key blob wrap/unwrap across reset, monotonic counter or fuse for anti-rollback | Blob unwrapped after reset; counter read/advance on the AM254-5 |
| S4 In-place transform | MCUboot encryption → OTFAD encryption of the inactive slot with a power-cut-safe journal | Power cut at random points during install; previous release always boots; install resumes |
| S5 TrustZone + MCUboot | MCUboot boots a CM33 S + NS image in place; NSC calls work; CM7 still starts | Log and confirm through NSC on hardware |
| S6 TRDC and sizes | CM7, DMA and DAP forbidden accesses fault after lock; signed image sizes and XIP performance | Bus faults observed and logged; numbers recorded in `MEMORY_MAP.md` |
| S7 Debug authentication | OEM Closed with ELE debug authentication, scoped and device-bound credentials | Locked port refuses; valid credential opens the allowed scope only; wrong device refused |

All spikes run on the **AM254-5 custom board** (`docs/boards/AM254-5.md`), the designated sacrificial board. S3 and S7 write irreversible fuses or change the lifecycle: they run only with explicit authorization for every fuse or lifecycle operation.

## Consequences (if accepted)

- Supersedes the layout and update-mode parts of ADR-0014; revises ADR-0006 (pair packaging, install journal) and ADR-0013 (no swap, no separate CM33/CM7 areas, both slots executable).
- The open question of ADR-0014 on `aied_slot_swap` is answered: its journal design is reused for the in-place install transform.
- CI builds, signs and attests two variants per release; the SBOM covers both.
- The update test (`MCUBOOT_UPDATE_TEST.md`) moves from swap-move to direct XIP with encrypted slots; T1–T4 keep their meaning (update, confirm, revert), plus power cuts during install.
- The log commands become NSC services; `AIED_LOG_PORT_COMMANDS` stays 0 in release builds.

## Validation

Spikes S1–S7, then the update test with encrypted slots and power-cut injection during install, TRDC fault tests, and the authenticated-debug test in OEM Closed.
