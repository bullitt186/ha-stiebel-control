# ADR-0004: Bus analysis with raw read/write behind build flags

- **Status:** Accepted
- **Date:** 2026-11-24

## Context

Analysing the Elster bus needs access to arbitrary CAN ids and indices, not only to the signals in
`ElsterTable.h` and the member table. Bus scan (`bus_scan.h`, #109) covered table members and hex
ids, but no single read of one index at one id and no write. Issue #138 added the raw CAN frame
stream (`can_debug.h`, observers in `can_io.h`). Until now the debug functions lived in a separate
debug firmware, so each analysis step needed a second build and flash.

The forces: analysis builds must reach any id and index, while production firmware must not gain a
way to write arbitrary values to the bus. Writes to a heat pump are not reversible by the firmware.

## Decision

We will expose the bus analysis tools in the regular firmware, each one behind a build flag, and
keep the production build free of them by default:

- `can_debug: "true"` (`-DHA_CAN_DEBUG_true`) builds in the members scan and raw read. The
  `can_debug` substitution also enables the frame stream.
- `can_raw_write: "true"` (`-DHA_CAN_RAW_WRITE_true`) additionally builds in raw write. Without
  `can_debug` it is not built in, and `<root>/raw/write` only logs that it is not built in.

The MQTT topics and payloads, with `<root>` the MQTT root:

- `<root>/scan/start`: `MEMBER|0xID [0xLO 0xHI] [fa|f8|f9]`. `MEMBER` is a table member name or a
  hex CAN id. The switch byte is optional and at most one.
- `<root>/scan/status`, `<root>/scan/<name>/0x<idx>` (and `/f8`, `/f9`): scan state and results.
- `<root>/scan/members` (`can_debug` only): `[0xLO 0xHI] [0xIDX]`, default `0x000 0x7ff 0x000b`.
  Results go to `<root>/scan/members/0x<id>` as `{"index":…,"raw":<decimal>}`.
- `<root>/raw/read` (`can_debug` only): `0xID 0xIDX [fa|f8|f9]`.
- `<root>/raw/write` (`can_raw_write`): `0xID 0xIDX 0xVALUE`.
- `<root>/raw/result`: one JSON object per operation, not retained, for read and write.

Raw write is refused with status `rejected` and nothing is sent when any of these holds, checked in
this order:

1. `can_raw_write` is not `"true"` in the build.
2. `read_only` is `"true"` in the build.
3. The target is the PC id.
4. The index low byte is 0xF7 to 0xFA and the value is 0x8000.
5. A bus scan, another raw operation or passive listen mode is running.

There is no whitelist of allowed ids or indices.

Replies are accepted only when addressed to the PC id (`scanAnswerIsForPc()`). Raw read and raw
write run one at a time: a raw request is refused while a scan or another raw operation runs
(`busy` for read, `rejected` for write).

## Consequences

- With `can_raw_write: "true"`, any MQTT client with access to the broker can write any value to
  any CAN id. The option is therefore off by default and documented as a security note in
  `docs/CONFIGURATION.md`. Enabling or disabling it needs a reflash.
- The rules in this ADR are the only protection against unintended writes. Any change to them
  needs a new ADR.
- A members scan asks 2048 ids at 150 ms each (`SCAN_GAP_MS`), about 5 minutes plus the 1 s tail.
  Normal polling pauses for that time. The PC id is skipped.
- A `scan/start` is not refused while a raw operation runs (`startScan()` has no such check). The
  raw answer is still handled first in `handleBusAnalysisFrame()`, but the two can overlap. This is
  a known gap against the one-operation rule.
- Default and production builds are unchanged: with `can_debug` off, the functions are stubs that
  only log.
- The operating description lives in `docs/CONFIGURATION.md` under "Bus analysis".

## Alternatives considered

- Functions only in a separate debug firmware: rejected, because each analysis step then needs a
  second build and flash. This is the state this ADR replaces.
- A package extra for the analysis tools: rejected, for the same reason as a separate firmware.
- Arming raw write with a time limit or a whitelist of ids and indices: rejected by the author. A
  build flag plus the rules above is enough for analysis builds, and a whitelist would limit the
  analysis of unknown indices, which is the purpose.
- A foreign sender id for raw requests: not implemented. All requests use the PC id.
