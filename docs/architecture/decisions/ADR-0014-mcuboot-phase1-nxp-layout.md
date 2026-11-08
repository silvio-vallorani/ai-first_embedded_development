# ADR-0014: MCUboot integration, phase 1 (NXP reference layout)

- Status: Accepted for phase 1; hardware validation pending
- Date: 2026-11-08

## Context
The project needs a working MCUboot update path as soon as possible; the first hardware test after the LED example runs is a firmware update. The target architecture (ADR-0004, ADR-0007, ADR-0013, `MEMORY_MAP.md`) requires a recovery stage, redundant MCUboot copies and custom partitioning, none of which exists yet. MCUXpresso SDK 26.09.00 (ADR-0010) ships an MCUboot port for the MIMXRT1180-EVK (`ota_examples/mcuboot_opensource`) and an MCUboot application linker that supports an embedded CM7 image.

## Decision
- Use NXP's `mcuboot_opensource` port from the pinned SDK unmodified, on the CM33: ECDSA-P256 signatures, `SWAP_MOVE` upgrade strategy, 1-KiB image header, 4-byte write alignment, NXP defaults for bootstrap and serial recovery.
- Use the NXP reference flash layout for phase 1:

| Region | Start | Size | Content |
|---|---:|---:|---|
| MCUboot | 0x28000000 | 256 KiB | FCB at +0x400, boot container at +0x1000, bootloader code |
| Primary slot | 0x28040000 | 2 MiB | Active release image (header + CM33 code with embedded CM7 image + trailer) |
| Secondary slot | 0x28240000 | 2 MiB | Candidate release image |

- Package the release pair as **one MCUboot image**: the CM33 application embeds the CM7 image (`.core1_code`, at most 256 KiB, linker assert) and copies it to CM7 TCM before starting CM7. MCUboot therefore swaps, tests, confirms and reverts CM33 and CM7 as one unit.
- The CM33 application confirms a test image (`bl_update_image_state(kSwapType_Permanent)`) only after CM7 has started and reported ready. An unconfirmed test image is reverted by MCUboot at the next reset.
- Sign with the NXP development key shipped in the SDK (`sign-ecdsa-p256-priv.pem`). This key is public: it provides no authenticity and is for development only.
- First update transport: the debugger (LinkServer) writes a signed, padded test image into the secondary slot. UART reception in the application (xmodem, as in NXP `ota_mcuboot_basic`) is the next step.
- Build and flash automation: `scripts/rt1180_mcuboot.py`; test procedure: `docs/engineering/MCUBOOT_UPDATE_TEST.md`.

## Consequences
- The phase-1 layout differs from `MEMORY_MAP.md`, which remains the target layout: no recovery stage, a single MCUboot copy, no separate CM33/CM7 slot areas, no separate journal region (MCUboot keeps swap status in the slot trailer) and no use of the 8-MiB sector reserve.
- ADR-0006 (atomic pair) is met in phase 1 by single-image packaging plus MCUboot swap-move. The host-tested `aied_slot_swap` orchestration and `aied_manifest_validate` are not used on target in phase 1. Whether they are retired or become the policy layer for a later multi-image or separate-slot design must be decided before phase 2.
- ADR-0007 is not met: a corrupt MCUboot image means no boot until reprogrammed through the debugger or ROM serial download.
- ADR-0008 is not met: MCUboot downgrade prevention and a hardware security counter are not enabled; any correctly signed version is accepted.
- ADR-0011: the signing key is public; no production image may be signed with it.
- The bootloader is built in the `flexspi_nor_debug` configuration for bring-up. The ROM boots its container unsigned, which requires the OEM Open lifecycle.

## Validation
On the EVK, run `MCUBOOT_UPDATE_TEST.md`: boot of MCUboot and v1, update to v2 with confirmation, rollback of an unconfirmed v3. Then inject resets and power loss during the swap. Phase 2 (target layout, recovery, redundant MCUboot, anti-rollback, production keys) needs its own ADR.
