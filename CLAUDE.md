# Claude Code project instructions

At the start of every work session in this project, load the `ponytail:ponytail` skill (Skill tool) before doing any other work, and keep it active for the whole session.

Read `AGENTS.md` first. Follow all architecture constraints and ADRs. Use GitButler virtual branches for isolated work when available; do not push automatically. Never add Claude as author or co-author. Do not commit or push without explicit authorization. Use the Git author identity Silvio Vallorani <silvio.vallorani.dev@gmail.com> only when the user authorizes a commit.

Before modifying firmware, identify the target (board + core + configuration). Keep hardware-specific code in board/MCU/platform layers. Keep update metadata, image validation, and swap orchestration host-testable. Any change to memory layout, linker placement, boot sequence, or security boundaries requires an ADR update.
