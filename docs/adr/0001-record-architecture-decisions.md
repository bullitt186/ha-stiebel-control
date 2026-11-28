# ADR-0001: Record architecture decisions

- **Status:** Accepted
- **Date:** 2026-11-24

## Context

Contributors, human and coding agents, need to know why the code is the way it
is, without reading the whole history. Decisions hidden in issues or commit
messages get lost and are silently reversed.

## Decision

We record architecturally significant decisions as ADRs in `docs/adr/`, in the
format of `template.md`, following repo-kit. `AGENTS.md` defines when an ADR is
required. ADRs are written in the pull request that implements the decision.

## Consequences

Agents read the ADRs before designing a change and must not contradict an
accepted one without superseding it. PR hygiene checks format and index.

## Alternatives considered

Decisions in issues or the wiki only: not versioned with the code, not read by
agents working in the repository.
