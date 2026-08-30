# Replay desync frame capture and disabled diagnostic boundary

Status: Resolved decision; retained audit evidence.

Resolution: Conditional producer-side capture is chosen. Executable
implementation is owned by
[`Documents/Plans/Engine/ConditionalReplayDesyncFrameCapture.md`](../../Plans/Engine/ConditionalReplayDesyncFrameCapture.md).

Area: Engine

Record type: Non-executable reference material. This document intentionally has
no scheduler metadata and is not a Plan input.

Audit source: `CPS/shard-0009/003` from the frozen C++ plan-simplicity audit.

Frozen audit disposition: severity `MEDIUM`, confidence `HIGH`, occurrence
`hypothetical`; preliminary `promote-serious`, final `open-serious`.

Frozen audit commit: `80896f33661aaab99cf180a96db54600099be652`

## Finding recorded at audit freeze

On a replay CRC mismatch, `ReconcileValidateCrcCoord` records the tick and both
CRCs and unconditionally calls `CloneFrameViaSerialization` at
`Engine/Source/Network/Client/ReconcileReplayTick.cpp:193-216`. The helper at
`:16-24` serializes the complete `game::Frame` into a string stream, allocates a
second frame, and deserializes it. The replay path can perform this on a
provisional mismatch: `ReconcileReplay.cpp:195-218` clears the captured frame
when the shrunk-rollback attempt is retried from the full confirmed base.

The pointer crosses the engine/game reconciliation boundary through
`Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.cpp:62-77`.
`Engine/Source/Network/Client/ClientDesyncCore.cpp:15-39` always reports the
CRCs, but reads/moves the captured frame only inside
`if constexpr (kbDesyncDebugFrames)`. The frozen game PCH sets that switch to
false at `Projects/BrokenEngineSandbox/Source/Pch.h:6`; the active branch then
recovers or disconnects without reading the clone. The synthetic agent fixture
at `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsClient.cpp:453-470`
constructs its own snapshot and does not consume this replay clone.

At the frozen audit point, the source and leaf authorities therefore supported
an unused-work premise in the frozen build: `Engine/Source/Network/Client/AGENTS.md:45`
and `Projects/BrokenEngineSandbox/Source/Network/Client/AGENTS.md:28` say that
disabled builds do not request or stall for a real debug frame. The candidate
was nevertheless unresolved at that time because the governing architecture explicitly puts
“Deep-copy client Frame” before the enabled/disabled split in
`Documents/Architecture/GameReconciliation.md:129-157`. Its prose says that a
disabled build immediately follows recovery/disconnect, while its sequence
diagram documents unconditional deep-copy timing. The network architecture
also says the switch removes full-frame buffering when disabled at
`Documents/Architecture/Network.md:80`, without explaining this producer-side
clone.

## Controlling boundary and audit-time unknowns

At audit time, the unresolved decision was whether the desync client-frame snapshot is an always-on
reconciliation diagnostic artifact or belongs exclusively to the optional
`kbDesyncDebugFrames` diagnostic mode. Engine reconciliation owns production
and the `CoordScratch` lifetime; the game reconciler forwards the record; the
engine desync core owns the enabled consumer and the recovery/disconnect
boundary. No other first-party read of `pDesyncClientFrame` was found, but the
architecture's ordering is an explicit contrary authority rather than proof of
stale documentation.

The following behavior was unknown at the audit freeze:

- Whether a non-null `ReconcileDesyncInfo::pDesyncClientFrame` is an implicit
  contract for any disabled-build diagnostic, logging, recovery, or future
  consumer not represented by the current source.
- Whether provisional shrunk-rollback mismatches should capture a frame at all,
  given that fallback may discard it before the final desync decision.
- Whether the architecture diagram intentionally specifies unconditional
  capture, or merely describes the enabled diagnostic sequence and needs an
  authority correction before source behavior can change.
- Whether stream construction, frame allocation, serialization, or
  deserialization can fail on this mismatch path and, if so, whether the
  existing recovery/disconnect semantics must handle that failure or may let it
  propagate.

## Options retained from the audit

These options were recorded before the decision was resolved, without selecting an implementation or authorizing
a source, capability, wire, save, replay-format, or CRC change.

1. **Keep unconditional capture and clarify the authorities.** Document why
   disabled builds retain the private clone, including whether it supports an
   existing recovery or diagnostic obligation, and keep the current producer
   and consumer boundary.
2. **Align capture with the optional diagnostic consumer.** Establish that the
   disabled path leaves the snapshot empty while CRC reporting and recovery
   remain unchanged, and define whether the same rule applies to provisional
   mismatches and fallback retries.
3. **Defer capture until a confirmed diagnostic need.** Define how the exact
   replayed frame remains available across worker completion, fallback reset,
   and the enabled debug-frame wait before choosing any lazy capture point.

## Decisive questions and evidence recorded by the audit

The audit recorded the following questions and evidence needs before the
decision was resolved:

- Which authority owns the intended ordering: the architecture sequence,
  `Engine/Source/Network/Client/AGENTS.md:45`, the game client leaf at
  `Projects/BrokenEngineSandbox/Source/Network/Client/AGENTS.md:28`, or a
  clarified combination of them?
- Is `pDesyncClientFrame` required to be populated for disabled builds? Trace
  every first-party producer and consumer again, including the synthetic agent
  fixture, and establish an explicit nullability contract for
  `ReconcileDesyncInfo` if the answer is no.
- For a provisional mismatch followed by fallback, what exact frame and tick
  must an enabled diagnostic retain, and can a future capture boundary preserve
  that identity without changing replay or CRC semantics?
- Does the clone occur on every reachable replay mismatch in the current
  client, including the provisional path that fallback discards? A focused
  harness scenario should force both a provisional retry and an unresolved
  mismatch and report capture count, recovery outcome, and debug-frame behavior
  with the switch both disabled and enabled.
- With the chosen contract, do client and server `Debug|x64` builds preserve
  the existing `ReconcileDesyncInfo` layout, CRC values, wire messages,
  recovery/disconnect policy, and enabled frame-difference logging? The check
  must include allocation/serialization cost on the mismatch path and prove
  that any retained snapshot remains valid for its consumer lifetime, preserves
  the exact selected frame, and neither dangles nor aliases mutable or reused
  frame storage.
- What is the current and required behavior if stream construction, frame
  allocation, serialization, or deserialization fails during mismatch
  handling? A focused existing exception-boundary trace or failure-injection
  scenario should identify the error result, whether CRC reporting and
  recovery/disconnect complete, and the owning authority for that behavior;
  this record selects no new failure handling.

## Boundaries and invariants

- CRC mismatch reporting remains available in every build; differing expected
  and actual CRCs are not removed or changed.
- The enabled debug-frame path retains the exact client-frame identity needed
  by `LogDesyncFrameDifferences`, and its request, stall, timeout, recovery,
  and disconnect policy remains coherent with the server switch.
- The disabled path does not request or stall for a real debug frame and keeps
  its current recovery/disconnect behavior unless an explicit authority change
  says otherwise.
- Replay fallback may clear provisional mismatch state; any retained snapshot
  must remain valid for its consumer lifetime, preserve the exact selected
  frame, and neither dangle nor alias mutable or reused frame storage.
- At audit time, no option in this record authorized a new runtime mode, wire/save/replay
  format, CRC field, source fix, instrumentation capability, or unit test.

## Provenance

- Consolidated audit entry: `Temp/CppPlanSimplicityAudit/80896f33661aaab99cf180a96db54600099be652/consolidated-index.md:3062-3142`.
- Structured disproof: `Temp/CppPlanSimplicityAudit/80896f33661aaab99cf180a96db54600099be652/sol-triage-b.md:45-55`.
- Frozen evidence selectors: `Engine/Source/Network/Client/ReconcileReplayTick.cpp:16-24,193-216`; `Engine/Source/Network/Client/ReconcileReplay.cpp:195-218`; `Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.cpp:62-77`; `Engine/Source/Network/Client/ClientDesyncCore.cpp:15-39`; `Projects/BrokenEngineSandbox/Source/Pch.h:6`; `Engine/Source/Network/Client/AGENTS.md:45`; `Documents/Architecture/GameReconciliation.md:129-157`.
- Duplicate sweep: exact symbol/path/root-cause-and-boundary search across all
  current `Documents/Investigations/**/*.md` and `Documents/Plans/**/*.md`
  found no matching durable record. Existing replay records concern particle
  marker ordering, replay owner overlap, reset cancellation, or reserved
  status values and are not duplicates.
- No source, shader, build, script, scheduler, or capability change is part of
  this investigation.
