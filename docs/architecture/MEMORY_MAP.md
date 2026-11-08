# QSPI NOR memory map (initial proposal)

## Phase-1 active layout (ADR-0014)

The firmware currently uses NXP's MCUboot reference layout, not the target layout below:

| Region | Start (CPU address) | Size | Content |
|---|---:|---:|---|
| MCUboot | 0x28000000 | 256 KiB | FCB at +0x400, boot container at +0x1000, bootloader |
| Primary slot | 0x28040000 | 2 MiB | Active image: header, CM33 code with embedded CM7 image, trailer |
| Secondary slot | 0x28240000 | 2 MiB | Candidate image |

The rest of the device is unused in phase 1. The target layout below remains the design goal for phase 2.

## Target layout

## Reference device and geometry

The EVK board manual identifies two separate 128-Mbit (16-MiB each) Winbond W25Q128JWSIQ QSPI NOR devices (U10 and U12) connected to the FlexSPI1 A/B paths. This project memory map is defined for **one device / one 16-MiB address space**, not the sum of both devices. Confirm the exact fitted ordering code and board revision against the actual EVK BOM before programming. The family datasheet describes 256-byte page programming, 4-KiB sector erase, and 32-KiB / 64-KiB block erase. The array contains 4096 4-KiB sectors. This layout uses 4-KiB units throughout. Do not infer that every board revision has identical flash population.

Reference sources: [NXP MIMXRT1180-EVK Board User Manual UM12021](https://www.nxp.com/docs/en/user-guide/UM12021.pdf), [Winbond W25Q128JW datasheet](https://www.winbond-electronics.com/pdf-b1/w25q128jwpim-tr.pdf), [Zephyr board description](https://docs.zephyrproject.org/latest/boards/nxp/mimxrt1180_evk/doc/index.html).

## Capacity split

- Total: 16 MiB = 128 Mbit.
- Operational half: 8 MiB = 64 Mbit.
- Sector replacement reserve: 8 MiB = 64 Mbit.

The second half is reserved for a future logical-to-physical sector replacement layer. The reserve is not automatically managed by the NOR device. The boot ROM and direct-XIP consumers cannot use a firmware-defined remap unless the boot chain explicitly supports it. The remapping design is intentionally deferred.

## Operational half (offsets relative to QSPI base)

| Region | Start | Size | End (exclusive) | Purpose |
|---|---:|---:|---:|---|
| Immutable recovery first-stage reservation | 0x000000 | 256 KiB | 0x040000 | Minimal recovery / bootloader selection concept; physical immutability mechanism not yet proven |
| MCUboot A | 0x040000 | 512 KiB | 0x0C0000 | Primary MCUboot image |
| MCUboot B | 0x0C0000 | 512 KiB | 0x140000 | Redundant MCUboot image |
| Slot 1 / CM33 | 0x140000 | 1 MiB | 0x240000 | CM33 image + image metadata |
| Slot 1 / CM7 | 0x240000 | 2 MiB | 0x440000 | CM7 image + image metadata |
| Slot 2 / CM33 | 0x440000 | 1 MiB | 0x540000 | Candidate CM33 image |
| Slot 2 / CM7 | 0x540000 | 2 MiB | 0x740000 | Candidate CM7 image |
| Update metadata, journal and swap scratch | 0x740000 | 256 KiB | 0x780000 | Pair state, swap journal, 4-KiB scratch sector and redundant records |
| Application NVM | 0x780000 | 256 KiB | 0x7C0000 | Mutable configuration/data |
| Factory data | 0x7C0000 | 128 KiB | 0x7E0000 | Provisioned product data; protection mechanism TBD |
| Persistent logs | 0x7E0000 | 128 KiB | 0x800000 | Bounded diagnostic log |
| **Operational half total** | **0x000000** | **8 MiB** | **0x800000** | |

## Sector replacement reserve

| Region | Start | Size | End (exclusive) | Purpose |
|---|---:|---:|---:|---|
| Spare-sector pool | 0x800000 | 8 MiB | 0x1000000 | Reserved physical sectors for future replacement / redundancy mechanism |

All region boundaries are 4-KiB aligned. Slot 1 and Slot 2 are each 3 MiB and hold a complete CM33 + CM7 release pair. The CM7 allocation is twice the CM33 allocation as an initial planning budget, not a measured maximum.

## Slot swap semantics

The conceptual update model swaps slot contents so that the confirmed active images always occupy the same logical execution addresses. The swap scratch sector and journal are budgeted inside the 256-KiB metadata region. A pair-level transaction record prevents either core from starting until both images have been swapped, verified and committed. The exact MCUboot integration and power-fail guarantees remain to be implemented and tested on hardware.

## Explicit caveats

1. The first-stage recovery reservation in external NOR is not physically immutable merely because firmware treats it as read-only. A real immutable root must be anchored in ROM, OTP/eFuse, or a verified irreversible hardware protection mechanism.
2. The boot ROM's ability to select redundant MCUboot copies is an unverified assumption for this project.
3. A bad-sector remapping layer must not silently alter addresses consumed by Boot ROM, FlexSPI XIP, image headers, or linker scripts.
4. Factory data in NOR is not permanently immutable until a hardware-enforced protection/provisioning method is validated.
5. Final slot budgets require actual signed image sizes, headers, TLVs, alignment and bootloader metadata.
