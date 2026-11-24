# Architecture decision records

Decisions that shape this repository, one file each: `NNNN-kebab-case-title.md`,
numbered in order, written from `template.md`. When to write one: `AGENTS.md`.

- An *Accepted* ADR is binding. To change it, add a new ADR and set the old
  one's status to `Superseded by ADR-NNNN`; never rewrite an accepted decision.
- Agents and people write ADRs the same way, in the pull request that implements
  the decision. Merging the pull request accepts the ADR.
- Every ADR is listed in the table below (PR hygiene checks this).

| ADR | Title | Status |
|---|---|---|
| [0001](0001-record-architecture-decisions.md) | Record architecture decisions | Accepted |
| [0002](0002-model-differences-in-one-compile-time-profile.md) | Model differences live in one compile-time profile | Accepted |
| [0003](0003-firmware-modules-in-a-strict-include-order.md) | Firmware modules in a strict include order | Accepted |
| [0004](0004-project-scope-in-docs-scope.md) | Project scope in docs/SCOPE.md | Accepted |
| [0005](0005-bus-analysis-raw-access.md) | Bus analysis with raw read/write behind build flags | Accepted |
