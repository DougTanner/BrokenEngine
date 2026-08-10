<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-09T20:43:52.250Z","dependsOn":[]} -->
# Reset Client Clock Measurements On Server Load

## Context

The existing visual-reset fixture reaches the client reconciliation assertion and exits during a
server-load/replay loop. `Temp/client-agent.log:108-123` records
`inputs.iTargetTick >= 0` failing at `Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.cpp:47`,
with the call chain through `ClientSession::Reconcile` and `GameBase::ClientUpdate`. The server log shows the
same fixture loading and looping the replay at `Temp/server-agent.log:30-45`, so the client exit is the unmet
runtime acceptance signal rather than a missing server fixture.

The first faulty write is the signed clock correction added to the time-step remainder by
`ClientSession::Reconcile` (`Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp:223-240`).
`ClientSessionRuntime::EvaluateClock` derives `iComputedTargetBehind` from the client jitter smoother
(`Engine/Source/Network/Client/ClientSessionRuntime.cpp:417-423`) and returns a signed correction that may be
negative (`:470-472`). `TimeStep::TickRealtime` turns a negative remainder into a negative tick count
(`Engine/Source/Frame/TimeStep.cpp:54-58`), and `GameBase::ClientUpdate` adds that count to the tick counter
(`Engine/Source/GameBase.cpp:109-110`).

`ClientSessionRuntime::ResetForServerLoad` resets the clock targets through `ResetClock`
(`Engine/Source/Network/Client/ClientSessionRuntime.cpp:158-179`; `ResetClock` at `:75-87`), but it does not
reset `Client::mSmoothedJitterUs` (`Engine/Source/Network/Client/Client.h:116-120`) or the interarrival epoch
used to produce it (`Engine/Source/Network/Client/ClientReceive.cpp:350-361`). The post-load first-full-state
path deliberately seeds the client behind the received server tick (`Projects/BrokenEngineSandbox/Source/Network/Client/ClientSessionReceive.cpp:93-105`).
Retained pre-load timing state can therefore select a larger post-load target-behind value and emit a negative
correction before the new server epoch has supplied timing samples.

This is proven pre-existing and outside the approved intent: the active change resets only client visual Blaster
wind and Player shield state on transfer and does not modify network clock, time-step, or reconciliation logic.

## Design

Treat a server load as a new client timing-measurement epoch. Add a reset operation to
`common::Smoothed` that discards its rolling samples, cursor/count, and current smoothed value, then invoke it
for `mpClient->mSmoothedJitterUs` from `ClientSessionRuntime::ResetForServerLoad`. Clear the client's
`kHasLastUpdateArrival` flag at the same boundary so the first post-load coordinate update establishes a new
interarrival baseline instead of measuring across the load gap.

Keep `EvaluateClock`'s signed correction formula, the existing hard-snap path, the initial full-state tick seed,
`TimeStep`'s signed remainder behavior, and `GameBase` tick accounting unchanged. The first post-load clock
evaluation must use only post-load timing state; no packet, wire, Frame-version, status-change, CRC, or replay
ordering bytes change.

## Critical files

- `Common/Smoothed.h` — the reset operation for the rolling measurement; no other smoother behavior changes.
- `Engine/Source/Network/Client/ClientSessionRuntime.cpp` — `ResetForServerLoad`; reset the jitter measurement
  and arrival epoch alongside the existing clock reset.
- `Engine/Source/Network/Client/Client.h` — `mSmoothedJitterUs`, `kHasLastUpdateArrival`, and the arrival
  timestamp; read-only state ownership context.
- `Engine/Source/Network/Client/ClientReceive.cpp` — jitter/arrival producer; read-only post-reset behavior.
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSessionReceive.cpp` — post-load tick seeding;
  read-only contract.
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp` — signed correction consumer and
  assertion path; read-only contract.

## In scope

- The `common::Smoothed` reset semantics needed to discard a complete rolling measurement history.
- `ClientSessionRuntime::ResetForServerLoad`: reset `mpClient->mSmoothedJitterUs` and invalidate
  `ClientStateFlags::kHasLastUpdateArrival` before post-load clock evaluation.
- The `Transport Timing Inputs` section of `Engine/Source/Network/Client/AGENTS.md`, only if the reset
  contract needs to name the new timing epoch to remain accurate.

## Out of scope

- The active transfer visual-reset files and all Blaster wind or Player shield behavior.
- `ClientSessionRuntime::EvaluateClock`'s correction formula, `ClientSession::Reconcile`, the hard-snap branch,
  `TimeStep`, or `GameBase` tick arithmetic; do not hide the stale-state defect with a downstream clamp.
- Pipeline RTT smoothing, packet drain/order, slot/epoch/ACK state, subscription policy, and game hydration.
- Any server source, packet layout, protocol version, serialization, `Frame::kiVersion`, CRC field/order, or
  replay-range change.
- The separate game-packet buffer reset in
  `Documents/Plans/Network/ClientLoadResetGamePacketDrain.md`.

## Risk tier and invariants

Change Workflow Tier 3 — cross-frame client clock state controls `GameBase::ClientUpdate` tick progression and
game reconciliation/replay across a server-load boundary. The change has no wire or serialized-data exposure,
but the deterministic PostRender/CRC path and replay behavior are acceptance surfaces for the corrected pacing.

Invariants: after a server load reset, jitter and interarrival measurements belong only to the post-load epoch;
`ClientReconciler` never receives a negative target tick; the client remains governed by the existing
`latestServerTick`/`miCurrentTargetBehind` ceiling; signed correction, transfer ordering, CRC computation, and
replay determinism remain unchanged.

## Acceptance criteria

- With fresh client and server processes, the existing sequential Player-then-Blaster transfer fixture completes
  its replay/load loop without the `inputs.iTargetTick >= 0` assertion, client process exit, CRC/checksum error,
  or replay-reader error.
- Structural inspection shows `ResetForServerLoad` clears the complete jitter measurement epoch and that the
  first post-load update starts a new arrival baseline; `EvaluateClock`, `TimeStep`, and tick accounting retain
  their current signed behavior.
- Both Debug x64 client and server targets compile.
- No unit tests are added; `/agent-harness` remains the live verification path for the fixture.

## Coordination

`Documents/Plans/Network/ClientLoadResetGamePacketDrain.md` also edits
`ClientSessionRuntime::ResetForServerLoad`. The plans are order-independent: whichever lands second must
preserve both the `mReceivedGamePackets.clear()` operation and this plan's jitter/arrival-epoch reset, locating
each operation by member and symbol rather than line number. No directional dependency is required.

## Notes

Origin: accepted pre-existing/out-of-scope residual from the visual transfer-reset session. Session evidence is
`Temp/client-agent.log:108-123` and `Temp/server-agent.log:30-45`; those logs are machine-local and are not part
of the implementation boundary.
