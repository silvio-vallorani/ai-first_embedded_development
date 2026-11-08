# ADR-0012: Documentation and AI workflow

- Status: Accepted
- Date: 2026-11-08

## Context
Architecture, implementation and AI-assisted contributions must remain aligned.

## Decision
Use MkDocs for canonical documentation, Doxygen for API documentation, GitHub Pages for publication, and repository-maintained ADRs. Provide `AGENTS.md`, `CLAUDE.md` and structured AI guidance.

## Consequences
Wiki content is onboarding/troubleshooting material and links back to canonical docs. AI agents must update ADRs when architecture changes.

## Validation
CI builds documentation and checks links; reviews check ADR/documentation drift.
