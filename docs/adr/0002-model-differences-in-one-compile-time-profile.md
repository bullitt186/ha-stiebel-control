# ADR-0002: Model differences live in one compile-time profile

- **Status:** Accepted
- **Date:** 2026-11-24

## Context

The firmware supports several heat pump models with different managers (WPMW II, WPM3, WPL
family) and different bus members, routes and verified writes. Model checks spread through the
code as string comparisons of the model name became hard to review and to test (#94). This ADR
records the decision as it stands in `model_profile.h`; it was made before ADRs existed.

## Decision

The build flag `-DHA_DEVICE_MODEL=<model>` is evaluated only in `model_profile.h`. It selects a
`constexpr ModelProfile kModel` with capability flags and defines the few preprocessor switches
the signal table and language files need (`HA_CONTROLLER_*`, `HA_ELSTER_WPL_FAMILY`). All other
code reads `kModel.<flag>`; it never compares the model name and never adds its own model `#if`.
A new difference between models is a new flag, named after the capability, not after the model.
Unverified behaviour of a model stays behind a flag that is false until checked on a real unit.

## Consequences

Every model is built and unit-tested in CI with its real tables and profile. Adding a model is a
profile row, a request table and a smoke manifest. The profile table grows a column per
capability; merges of parallel model work conflict there and are resolved column by column.

## Alternatives considered

- Runtime model selection: larger firmware, and the signal table could not be trimmed per model.
- Model name comparisons where needed: what #94 removed.
