<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-30T12:56:56.222Z","dependsOn":[]} -->
# Make replay desync frame capture conditional

## Context

The false required condition is that every replay CRC mismatch must serialize,
allocate, and deserialize a complete client `Frame`, even when the diagnostic
consumer is compiled out. `ReconcileValidateCrcCoord` unconditionally calls
`CloneFrameViaSerialization` on a mismatch
(`Engine/Source/Network/Client/ReconcileReplayTick.cpp:16-24,193-216`). A
provisional shrunk-rollback mismatch can then have the clone discarded by the
full-rollback retry (`Engine/Source/Network/Client/ReconcileReplay.cpp:201-217`).
`ClientDesyncCore` moves and reads the clone only inside
`if constexpr (kbDesyncDebugFrames)`; the current game PCH sets that switch to
false (`Engine/Source/Network/Client/ClientDesyncCore.cpp:15-39`;
`Projects/BrokenEngineSandbox/Source/Pch.h:5-7`).

The originating candidate is `CPS/shard-0009/003`. The retained session Investigation
`Documents/Investigations/Engine/ReplayDesyncFrameCapture.md` records the
authority conflict and lifecycle evidence. The user explicitly directs the
narrow implementation: gate clone production with `kbDesyncDebugFrames`, keep
enabled diagnostic capture and fallback/retry semantics, and update the
architecture to show conditional capture. This Plan resolves the former open
decision; it does not authorize a new mode or recovery policy.

## Design

The author's recommendation is to wrap only the assignment to
`rScratch.pDesyncClientFrame` in `ReconcileValidateCrcCoord` with the existing
`if constexpr (kbDesyncDebugFrames)` switch. Keep mismatch CRC recording,
logging, `false` return, replay stack selection, and the fallback reset of
`pDesyncClientFrame` unchanged. Keep `ClientReconciler` forwarding and the
enabled `ClientDesyncCore` move, request, stall, timeout, frame comparison, and
recovery/disconnect behavior unchanged. The disabled path must continue
straight to its current recovery/disconnect branch without requesting or
stalling for a debug frame.

Update `Documents/Architecture/GameReconciliation.md:129-160` so the deep-copy
step appears inside the enabled-debug branch after the mismatch report (or in
the equivalent conditional position), while the disabled branch explicitly
shows immediate recovery/disconnect. Keep the CRC and reconciliation diagrams'
other ordering and the separate Network architecture statement intact.

## Critical files

- `Engine/Source/Network/Client/ReconcileReplayTick.cpp:16-24,193-224` — clone producer, mismatch record, and validation outcome.
- `Engine/Source/Network/Client/ReconcileReplay.cpp:196-217` — provisional fallback reset/retry semantics.
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.cpp:62-77` — game-side forwarding seam.
- `Engine/Source/Network/Client/ClientDesyncCore.cpp:15-39,42-77` — enabled consumer and disabled recovery/disconnect.
- `Projects/BrokenEngineSandbox/Source/Pch.h:5-7` — existing diagnostic switch values.
- `Engine/Source/Network/Client/AGENTS.md:36-45` and `Projects/BrokenEngineSandbox/Source/Network/Client/AGENTS.md:19-30` — reconciliation/desync authorities.
- `Documents/Architecture/GameReconciliation.md:129-160` — conditional capture sequence authority.
- `Documents/Investigations/Engine/ReplayDesyncFrameCapture.md` — retained session evidence; do not delete or rewrite as part of this Plan.

## In scope

- Gating full-frame clone production at the existing `kbDesyncDebugFrames`
  branch in `ReconcileValidateCrcCoord`.
- Preserving the enabled clone's exact frame identity, fallback reset,
  forwarding, debug-frame request/wait/timeout, comparison, and recovery or
  disconnect behavior.
- Preserving mismatch CRC logs/values, replay and CRC outcomes, disabled
  immediate recovery, and no-request/no-stall behavior.
- Updating the GameReconciliation architecture sequence to depict the same
  conditional capture boundary.
- Statically tracing the producer guard, provisional fallback reset/retry, and
  final unresolved replay paths, without adding an instrumentation or harness
  seam.

## Out of scope

- Changing `kbDesyncDebugFrames` or `kbDesyncRecovery` values, adding a runtime
  mode, lazy/deferred capture, new instrumentation, or a replacement guard.
- Changing replay rollback bases, fallback selection, ring/storage lifetime,
  CRC calculation, frame serialization format, wire messages, or recovery/
  disconnect policy.
- Changing the retained Investigation's evidence record, synthetic agent
  fixture snapshot, server debug-frame handling, or adding an instrumentation
  seam, harness seam, or unit test.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: this changes the client replay
reconciliation producer at a deterministic CRC/desync boundary and must keep
the enabled client/server diagnostic contract and disabled recovery behavior
identical apart from the removed clone work.

Preserve these invariants:

- CRC mismatch reporting and expected/actual CRC values remain available in
  every build and do not change reconciliation outcomes.
- Enabled matching client/server builds retain the exact selected client frame
  until `LogDesyncFrameDifferences` consumes it; request, stall, timeout,
  retry, recovery, and disconnect semantics remain intact.
- Disabled builds never request or stall for a real debug frame and recover or
  disconnect immediately as today; no stale pointer is consumed.
- A provisional clone may still be reset on fallback, and no retained snapshot
  aliases mutable ring storage or outlives its consumer.
- No wire, save, replay-format, frame CRC, or deterministic simulation bytes
  change.

## Acceptance criteria

- The current `Projects/BrokenEngineSandbox/Source/Pch.h:6` configuration remains
  `kbDesyncDebugFrames=false`; client and server Debug builds compile this
  disabled configuration and the static trace shows the producer is compiled
  out while CRC reporting and immediate recovery/disconnect remain reachable.
- For verification only, record the exact bytes/hash of `Pch.h`, change only
  `kbDesyncDebugFrames` to `true`, compile matching client and server Debug
  configurations, then restore the original bytes and prove the hash and Git
  diff are identical. No configuration byte remains changed.
- Static path tracing covers the enabled producer/consumer, provisional
  fallback clearing, final unresolved mismatch, CRC values, replay outcomes,
  frame identity, and enabled difference logging; no runtime mismatch
  injection, instrumentation, or harness seam is required.
- `GameReconciliation.md`'s sequence diagram places deep-copy only in the
  enabled branch and agrees with both Network/Client authorities.
- Both compile configurations pass `/compile`; no unit tests are added.

## Notes

The retained Investigation is intentionally preserved as session evidence and
the durable source/authority citations above settle the implementation choice.
