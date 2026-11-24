# Contributing

Thanks for helping. Coding agents and people follow the same rules: read [AGENTS.md](AGENTS.md) before you start.

Step-by-step guides (new heat pump model, new signal, new display language) are in
[docs/CONTRIBUTING.md](docs/CONTRIBUTING.md). Bugs and ideas go into an issue first; the
issue forms ask for what is needed.

<!-- repo-kit:begin v1 (managed, do not edit; https://github.com/bullitt186/repo-kit) -->
## Definition of Done

This section is shared by all repositories of this maintainer
([repo-kit](https://github.com/bullitt186/repo-kit)). The maintainer reviews
every pull request, from people and from coding agents, against exactly this
list. `AGENTS.md` explains how to get there.

A pull request is done when:

1. **Scope** – it solves one issue or topic; unrelated changes are left out.
2. **Design** – the design is written down (issue, pull request or commit
   message); an ADR in `docs/adr/` is added or updated where `AGENTS.md` asks for
   one, and no *Accepted* ADR is contradicted.
3. **Tests** – behaviour changes and bug fixes have tests that fail without the
   change; expected values come from a specification or real data.
4. **Docs** – README, `docs/`, examples and `CHANGELOG.md` (`[Unreleased]`)
   describe the new state; upgrade notes exist for breaking changes.
5. **Checks** – the repository's full local check and all CI checks, including
   `PR hygiene`, are green on the current default branch.
6. **Hygiene** – no secrets, personal data or generated artefacts; commit
   messages follow Conventional Commits (`feat:`, `fix:`, `docs:`, …).
7. **Template** – the pull request template is filled in truthfully.

### What needs the maintainer

These are decided by the maintainer and may take longer: breaking changes,
claims about hardware or external systems that need evidence from a real device,
new runtime dependencies, and anything that changes releases or repository
settings. Say so in the pull request; it saves a review round.

### Labels

| Label | Meaning |
|---|---|
| `needs-info` | waiting for information from the reporter |
| `needs-hardware` | needs a test on a device or system the maintainer does not have |
| `blocked` | waits for another issue or pull request (linked) |
| `decision` | needs a maintainer decision |
| `breaking` | changes behaviour users rely on |
| `adr` | adds or changes an architecture decision record |
| `no-test-change`, `skip-changelog` | maintainer waiver for a PR hygiene rule |
<!-- repo-kit:end -->
