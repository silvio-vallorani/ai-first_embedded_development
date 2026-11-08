# Firmware update and paired slot swap

## Release model

A release is one inseparable CM33 + CM7 pair. A signed manifest must include release ID, version, board/MCU compatibility, minimum bootloader version, image lengths, hashes, signatures, dependencies and pair compatibility. The boot manager rejects a mixed pair.

## Update transaction

1. Receive a signed release into the inactive slot.
2. Validate manifest structure, compatibility, image lengths, hashes and signatures.
3. Persist a pair-level `PREPARED` journal record.
4. Swap CM33 and CM7 slot contents using a scratch sector and a durable per-sector journal. The swap backend must be restartable after power loss.
5. Verify both images at their fixed active addresses and persist `SWAPPED`.
6. Boot CM33, then CM7, in probationary state.
7. Commit the pair only after both cores report successful health checks.
8. If health checks fail or confirmation times out, roll back the pair as one transaction.

The initial code implements only a host-testable orchestration contract. It does not yet implement the flash-sector swap algorithm, cryptography, MCUboot port, UART transport or persistent journal driver.

## Bootloader update

Bootloader update is disabled by default and must require an explicit build-time feature. It uses a separate image type and journal from application updates. The immutable recovery stage is not field-updatable. A bootloader update must be staged and verified before changing the active bootloader. The actual power-fail-safe A/B selection depends on Boot ROM support, which is currently assumed but unverified.

## Version policy

Production release policy: upgrade-only, enforced by a hardware-backed monotonic counter or equivalent anti-rollback primitive. Debug policy may permit downgrade using separate authorization. Manifest version comparison alone is not sufficient anti-rollback protection.
