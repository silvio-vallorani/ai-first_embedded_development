# Firmware update and paired slot swap

## Release model

A release is one inseparable CM33 + CM7 pair. A signed manifest must include release ID, version, board/MCU compatibility, minimum bootloader version, image lengths, hashes, signatures, dependencies and pair compatibility. The boot manager rejects a mixed pair.

## Update transaction

1. Receive a signed release into the inactive slot.
2. Validate manifest structure, compatibility, image lengths, hashes, anti-rollback floor and signature (`aied_manifest_validate`).
3. Persist a pair-level `PREPARED` journal record (`aied_slot_swap_prepare`).
4. At the next boot, before any core starts, swap CM33 and CM7 slot contents (`SWAPPING`) using a scratch sector and a durable per-sector journal. The swap backend must be restartable after power loss.
5. Verify both images at their fixed active addresses. On success persist `TRIAL`; on failure revert the pair (step 8).
6. Boot CM33, then CM7, in probationary state. Each unconfirmed probationary boot consumes one attempt of the trial budget.
7. Commit the pair only after both cores report successful health checks (`aied_slot_swap_confirm`): persist `COMMITTING`, make the confirmation permanent (for example, advance the anti-rollback counter), then persist `COMMITTED`.
8. If verification fails or the trial budget is exhausted without confirmation, swap the previous pair back (`REVERTING`), verify it and persist `REVERTED`. The pair is always reverted as one unit.

## Journal state machine

```text
IDLE / COMMITTED / REVERTED --prepare--> PREPARED --boot--> SWAPPING --verify ok--> TRIAL
SWAPPING --verify failed--> REVERTING
TRIAL --confirm--> COMMITTING --commit_pair--> COMMITTED
TRIAL --trial budget exhausted--> REVERTING --verify ok--> REVERTED
```

`aied_slot_swap_boot` returns the boot action: `NORMAL` (start the confirmed pair), `TRIAL` (start on probation) or `RECOVERY` (neither core may start). Rules:

- `IDLE`, `COMMITTED` and `REVERTED` are terminal; a new update may be prepared only from them.
- An interrupted swap, revert or commit is resumed at the next boot. A reset after `COMMITTING` is persisted always completes the commit; it never reverts a pair whose commit may already have advanced the anti-rollback counter.
- A reset during `TRIAL` before `COMMITTING` is persisted counts as an unconfirmed boot.
- A sector I/O failure during the forward swap returns `RECOVERY` and leaves `SWAPPING` persisted, so the next boot retries. A previous pair that fails verification after revert also returns `RECOVERY`.
- An unreadable or inconsistent journal returns `RECOVERY`: the slot arrangement is unknown.

Backend contract (`aied_swap_backend_t`): journal records are durable, atomic and integrity-checked by the backend; `swap_sector` is restartable and idempotent per direction, core and sector; `commit_pair` is idempotent.

The code implements this orchestration and validation as host-testable logic. Host tests inject a power cut at every journal write, sector swap and commit. It does not yet implement the flash-sector swap algorithm, journal persistence/CRC, cryptography, anti-rollback counter access, MCUboot port or UART transport.

## Bootloader update

Bootloader update is disabled by default and must require an explicit build-time feature. It uses a separate image type and journal from application updates. The immutable recovery stage is not field-updatable. A bootloader update must be staged and verified before changing the active bootloader. The actual power-fail-safe A/B selection depends on Boot ROM support, which is currently assumed but unverified.

## Version policy

Production release policy: upgrade-only, enforced by a hardware-backed monotonic counter or equivalent anti-rollback primitive. Debug policy may permit downgrade using separate authorization. Manifest version comparison alone is not sufficient anti-rollback protection.
