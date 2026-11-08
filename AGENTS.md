# Repository guidance for AI agents

- Preserve the MCU / board / target separation.
- Keep application services independent from RTOS APIs; use OSAL interfaces.
- Treat CM33 and CM7 images as one release pair. Never activate a mixed pair.
- Do not change linker scripts, memory maps, boot chain, flash layout, or security policy without updating the relevant ADR and architecture documents.
- Keep hardware access behind interfaces so host tests can use mocks.
- Add or update unit tests for every behavior change.
- Code and identifiers must be English. C/C++ comments must use English Doxygen format.
- Do not add Claude as a Git author or co-author. Git author identity, when a human-authorized commit is eventually made, must be Silvio Vallorani <silvio.vallorani.dev@gmail.com>.
- Never commit, push, create a release, or publish artifacts without explicit user authorization.
