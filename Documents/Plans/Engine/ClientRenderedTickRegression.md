<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-03T14:55:26.751Z","dependsOn":[]} -->
# Diagnose and fix client rendered-tick regression

## Context

A connected Debug client logged `Render regressed to older frame Coord: (0,0)
Tick: 1 LastTick: 14`, followed by the `GameBase.cpp:817` debug break, during
ship-layer baseline verification on gameplay baseline
`86b6b77005a610bb67fcb25567a12f95371ab3b4`. The session's query-only changes
did not touch GameBase or ClientSessionRuntime. This is a separate client
rendering residual; server ship-layer observations and paused query invariance
remain valid, but that run supplies no clean client-render verification.

Durable evidence is in
`ClientRenderedTickRegression/observations.txt` and
`ClientRenderedTickRegression/commands.jsonl` beside this Plan. The commands
retain the observed setup order: connected client, server reset, pause,
timescale changes, frame edits, and spawn injection. They include accidental
acceleration from 2x through 32x and later normalization to 1/1. This is a
confounded reproduction, not proof that reset alone causes the regression.

`GameBase::UpdateRenderInterpolation` compares the selected snapshot's tick
against `Cell::iLastRenderedTick` (`Engine/Source/GameBase.cpp:810-819`).
`RenderFrame` selects from the snapshot ring (`:598-612`). The observed values
prove that this guard failed; they do not identify which ring replacement or
writeback produced the older frame. `Cell::ResetClientState`
(`Engine/Source/GameBase.h:137`) already clears the remembered tick, and
`ClientSessionBase::ResetSessionForServerLoad`
(`Engine/Source/Network/Client/ClientSessionBase.cpp:98`) clears cells as well.
Root cause remains unconfirmed.

## Design

The author's recommendation is to use `/external-diagnose-bug` first, because
changing the guard or clearing a counter without proving the producer can hide
a ring lifecycle defect. Start a fresh connected client/server through
`/agent-harness`, verify timescale 1/1, let confirmed rendering advance, then
reset the server while the client stays connected. Pause/read/unpause only as
needed to retain before/after ticks; confirm 1/1 again after reset. Repeat the
reset with the same client. If reset alone is clean, isolate the retained
frame-edit resynchronization sequence and timescale changes separately rather
than attributing the recorded failure to reset.

Trace the failing selected slot, head/count, confirmed tick, remembered rendered
tick, and cell lifetime through full-state application, reconciliation writeback,
and clock correction. Establish the exact producer and event ordering before
proposing a fix. Once proved, make the smallest correction inside the client
snapshot/render lifecycle below. A cause requiring changes outside that region
returns for re-planning. Preserve the regression guard and its ability to detect
an older frame within one unchanged cell/session lifecycle.

## Critical files

- `Engine/Source/GameBase.cpp` — RenderFrame, UpdateRenderInterpolation,
  ResetSession, and ResetRenderClock.
- `Engine/Source/GameBase.h` — Cell snapshot ring and ResetClientState.
- `Engine/Source/Network/Client/ClientSessionBase.cpp` — ResetSessionForServerLoad.
- `Engine/Source/Network/Client/ClientSessionRuntime.cpp` — ApplyReceivedFullStates,
  full-state/update application, and ApplyClockCorrection.
- `Engine/Source/Network/Client/ReconcileReplay*.cpp` and corresponding headers —
  only the client snapshot-ring writeback found on the proven failing path.

## In scope

- Investigate the recorded rendered-tick regression and identify the exact ring
  replacement/writeback and lifecycle ordering responsible.
- Correct only client snapshot selection, publication/writeback, or client-only
  render-counter lifecycle in the critical regions, as established by diagnosis.
- Synchronize documentation made incorrect by that correction and verify it
  using existing harness commands and temporary diagnostic logging as needed.

## Out of scope

- Ship plane selection, navigation, avoidance, gameplay queries, and their
  approved changes or acceptance criteria.
- Server reset semantics, simulation math/state, CRC computation, deterministic
  reconciliation results, threading, wire formats, save/replay compatibility,
  data layout, and new harness commands.
- Removing the regression check or treating an unconditional counter clear as
  an adequate fix without a demonstrated lifecycle reason.

## Acceptance criteria

1. Diagnosis records a pre-fix reproduction with the event sequence and exact
   producer of the older selected snapshot; distinguish reset, frame-edit
   resynchronization, and accelerated-clock effects. If the recorded symptom
   cannot be reproduced, report that limit rather than implement a guessed fix.
2. Through `/agent-harness`, the same connected client renders before and after
   repeated server resets at confirmed 1/1 timescale without the regression log
   or debug break; capture both logs and client render progress.
3. The diagnosed failing sequence no longer selects an older frame against a
   retained newer tick; logs settle this separately from successful process exit.
4. Preserve committed-state-only rendering, the regression guard within a
   lifecycle, deterministic simulation/CRC, and the existing server/client
   reset and reconnect behavior. Verify relevant reset/reconnect paths with
   existing harness operations and inspect the final diff for excluded surfaces.
5. Build Debug client and server through `/compile`; complete applicable Change
   Workflow reviews and runtime verification before `/finalize-changes`.

## Coordination

No directional dependencies or mandatory nondirectional coordination with
existing Plans. The recorded symptom is distinct from their initialization,
logging, and cross-cell gameplay scopes.

## Notes

Future implementation is Tier 2: scoped client snapshot/render lifecycle
behavior. The scope excludes Tier 3 surfaces such as deterministic simulation,
wire/serialization, threading, and cross-subsystem behavior changes. Escalate
and re-plan if diagnosis requires any excluded surface. No unit tests.
