# Architecture guardrails

- Do not merge MCU, board and target abstractions.
- CM33 is the first application core; CM7 release is controlled by CM33.
- A firmware release always contains a compatible CM33 + CM7 pair.
- Keep RTOS dependencies behind OSAL.
- Treat the 8-MiB spare region as reserved until a sector remapping design is approved.
- Never claim immutable NOR protection without hardware evidence.
- Never change boot flow, memory map, linker layout or security boundary without an ADR.
