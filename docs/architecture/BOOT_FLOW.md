# Boot flow

## Selected conceptual sequence

1. NXP SoC Boot ROM executes and follows the configured boot source.
2. CM33-side boot management starts first.
3. The immutable recovery stage / redundant MCUboot selection concept is applied. The exact ROM-to-first-stage chain is pending hardware validation.
4. CM33 configures clocks, memory and resource access policy (including TRDC where required).
5. The boot manager validates the release manifest, both image hashes/signatures, hardware compatibility, bootloader compatibility and pair identity.
6. Slot swap is completed or resumed using the journal and scratch sector. The active pair is always mapped to fixed logical execution addresses.
7. CM33 performs its self-test and starts CM7 only after the pair-level activation state is valid.
8. Both cores run probationary health checks. A confirmed pair is committed; otherwise, when the trial budget of unconfirmed boots is exhausted, the pair is rolled back as one unit. See the journal state machine in `FIRMWARE_UPDATE.md`.

## Invariants

- CM33 is the first application core and owns CM7 release/startup sequencing.
- CM7 must not execute an image from a different release ID than the validated CM33 image.
- No core starts while a pair-level swap is incomplete or the journal is inconsistent.
- Reset/power loss during swap must lead to deterministic resume or rollback before application execution.
- Never assume that CM33 is automatically a secure enclave.

## Hardware validation backlog

- Confirm exact EVK revision, fitted flash devices, boot straps and FlexSPI configuration.
- Verify Boot ROM boot image format and whether redundant bootloader selection is possible.
- Verify recovery behavior when either MCUboot copy is corrupt.
- Verify TRDC policy, TCM ECC initialization, CM7 reset/start sequence and linker placement.
