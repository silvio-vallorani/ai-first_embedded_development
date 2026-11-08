# ADR-0012: Documentation and AI workflow

- Status: Accepted
- Date: 2026-11-08

## Context
Architecture, implementation and AI-assisted contributions must remain aligned.

## Decision
Use MkDocs for canonical documentation, Doxygen for API documentation, GitHub Pages for publication, and repository-maintained ADRs. Provide `AGENTS.md`, `CLAUDE.md` and structured AI guidance.

## Consequences
The GitHub Wiki is a generated, read-only publication of the repository Markdown documentation (see `WIKI_PUBLISHING.md`); it is never edited directly. AI agents must update ADRs when architecture changes.

## Validation
CI builds documentation and checks links; reviews check ADR/documentation drift.
