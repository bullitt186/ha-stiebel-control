# ADR-0003: Firmware modules in a strict include order

- **Status:** Accepted
- **Date:** 2026-11-24

## Context

`ha-stiebel-control.h` had grown into one header with all logic, which made changes collide and
reviews slow. ESPHome compiles the headers listed in `common.yaml` `includes:` as one unit, so a
module structure has to work without separate compilation (#109). Recorded after the fact; the
split was done in #118.

## Decision

One header per responsibility, each owning its global state. The module table in `AGENTS.md` and
`docs/ARCHITECTURE.md` is in include order: a module includes only modules above it, never one
that includes it. A lower module that needs to call up gets an injected hook (function pointer),
as `writeValueFilter`, `requestManagerExtraStep` and the CAN frame observers do.
`ha-stiebel-control.h` keeps only the CAN entry points and stays below 800 lines. Every new
header is listed in `common.yaml` `includes:` and in both module tables.
`tests/check_modules.py` enforces cycles, size and the list in CI.

## Consequences

New code has an obvious home and pure modules can be unit-tested on the host. Hooks are needed
where a lower module must reach a higher one.

## Alternatives considered

- Keep one header: merge conflicts and no testable seams.
- Separate `.cpp` compilation units: ESPHome's `includes:` model does not support it cleanly.
