<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:52:31.368Z","dependsOn":[]} -->
# Invalidate the server display when memory peaks change

## Context

The retained survivor `CAI/shard-0039/002` identifies a repaint-hash omission.
`ServerUpdateDisplayStats` samples both memory peaks at
`Engine/Source/Server/ServerDisplay.cpp:114-124`, and the left/Profile panels
paint them at `:465-482,630-639`. `ServerDisplayContentChanged` mixes only
current committed and heap-used values at `:181-188`; `Main.cpp:451-462` then
uses that result to decide whether to invalidate the monitoring window. A
transient allocation that raises a peak but returns current usage to its prior
value therefore leaves the changed painted text outside the hash.

The direct evidence is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0039.md:62`
and consolidated selector `Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:1002`. Frozen/live source
identity matches baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`; this
routing session has not changed source. The existing heartbeat is for
free-running tick/time text, not ordinary statistic changes.

## Design

The author's recommendation is to mix both peak-memory fields into
`ServerDisplayContentChanged` under the existing
`!ENABLE_CRT_DEBUG_HEAP` condition. Keep the deliberate tick/time and CPU-timer
exclusions and the current heartbeat cadence. No display text or memory
sampling policy needs to change.

## Critical files

- `Engine/Source/Server/ServerDisplay.cpp:114-124,181-188,465-482,630-639` —
  sampled peaks, hash, and paint consumers.
- `Engine/Source/Main.cpp:451-462` — repaint decision and heartbeat.
- `Engine/Source/Server/AGENTS.md` — content-hash ownership and cadence.

## In scope

- Content-hash inputs for the two painted mimalloc peak values.
- Existing CRT-debug-heap guard and repaint decision.
- Peak-only, current-only, and heartbeat-driven display behavior.

## Out of scope

- Allocator sampling, display formatting, window invalidation mechanics, or
  CPU/tick/time hash policy.
- New memory metrics, profile layout, or client/server allocator changes.

## Risk tier and invariants

Expected Change Workflow Tier 2. Trigger: this is scoped server display
invalidation behavior and does not alter simulation, wire, serialization, or
CRC state.

Preserve these invariants:

- Every painted statistic whose ordinary value changes participates in the
  content hash.
- A peak-only change triggers the normal throttled repaint path.
- Free-running tick/time and CPU-timer exclusions remain heartbeat-driven, and
  CRT-debug-heap builds retain their existing field guard.

## Acceptance criteria

- A transient allocation that raises either painted peak while current values
  return to baseline causes `ServerDisplayContentChanged` to request repaint.
- Peak values remain painted accurately and current-only changes still trigger
  their existing path.
- Tick/time and CPU-timer behavior remains heartbeat-driven.
- Server `Debug|x64` builds pass through `/compile`.

## Notes

Origin: `CAI/shard-0039/002`; source selector is the shard line above and the
consolidated selector is `consolidated-index.md:1002`. No source fix or build
was performed during routing.
