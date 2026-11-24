# ADR-0004: Project scope in docs/SCOPE.md

- **Status:** Accepted
- **Date:** 2026-11-24

## Context

Contributions and agent work need a shared answer to "does this belong here?". Until now the
purpose and limits of the project were spread over the README, the model table, ADR-0002 and the
release job. repo-kit v2 asks every repository for a `docs/SCOPE.md` and checks that a change of
it comes with an ADR.

## Decision

We will keep the project's purpose, what is in and out of scope, the direction and the deciding
principles in `docs/SCOPE.md`. The first version records only what the repository already shows
(README, ADRs, CI, open issues); points the repository does not decide are marked "Open: to be
decided by the maintainer". A later change of the scope is a maintainer decision and comes with
its own ADR.

## Consequences

Feature requests are judged against one file, and the issue form asks how a request fits it.
Open points in the scope stay open until the maintainer decides them.

## Alternatives considered

- Keep the scope implicit in the README: contributors and agents could not check a request
  against it, and PR hygiene could not guard it.
