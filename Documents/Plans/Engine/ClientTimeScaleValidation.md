<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:31:14.077Z","dependsOn":[]} -->
# Reject non-positive server time-scale updates before clock mutation

## Context

The accepted finding `CAI/shard-0014/002` identifies missing semantic
validation on a deterministic client clock input. `Client::ServerTimespeedUpdate`
checks packet size and decoding only, then passes signed `iMultiply` and
`iDivide` directly to `TimeStep::SetTimeScale`
(`Engine/Source/Network/Client/ClientReceive.cpp:627-649`; `TimeStep.cpp:70-75`).
`WallToSim`/`SimToWall` divide by those fields and `TickRealtime` uses the
result to choose the simulation tick count (`TimeStep.h:31-44`,
`TimeStep.cpp:8-57`). A correctly encoded `-1/1` update makes the accumulator
run backward and return no positive ticks; zero or overflowing values make the
conversion unusable. This violates the hostile-input and fixed-tick contracts.

The source shard's frozen/live target hashes match audit baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; current status contains only the
six pre-existing `Documents/Plans/Agents/Cpp*Audit.md` edits. The unchecked
ratio remains unresolved, pre-existing, and outside the approved audit work.

## Design

The author's recommendation is to validate both decoded fields in
`ServerTimespeedUpdate` before logging or calling `SetTimeScale`: require a
strictly positive, supported, overflow-safe numerator and denominator, using
the same ratio domain emitted by the server's internal increase/decrease paths.
Reject the message (or disconnect through the existing invalid-message policy)
without changing the accumulator when validation fails. Keep internal
`TimeStep` transitions and valid packet wire shape unchanged.

## Critical files

- `Engine/Source/Network/Client/ClientReceive.cpp:627-649` — received ratio trust boundary.
- `Engine/Source/Frame/TimeStep.cpp:8-108` and `Engine/Source/Frame/TimeStep.h:31-65` — accumulator and ratio consumers.
- `Engine/Source/Network/AGENTS.md` — hostile-input and timing rules.

## In scope

- Semantic positivity, range, and overflow validation of server-provided
  `iMultiply`/`iDivide` before `SetTimeScale`.
- The invalid-message handling and diagnostic at `ServerTimespeedUpdate`.
- Preserving valid server-generated ratios and the current fixed packet layout.

## Out of scope

- Changing the `TimeStep` arithmetic, accumulator clamp, server ratio policy,
  protocol message fields/version, or clock-correction algorithm.
- Other malformed network messages, local UI time controls, or replay timing
  compatibility.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3). The change guards
hostile wire input that directly controls deterministic fixed-tick progression.

Preserve these invariants:

- A stored time scale always has positive, supported, non-overflowing factors.
- Invalid packets cannot make `WallToSim`, `SimToWall`, or `TickRealtime` return
  negative/undefined timing results or mutate the accumulator.
- Valid server ratios, packet size/version, client/server timing order, and
  reconciliation behavior remain unchanged.

Tier rationale: the fix is a pre-specified positivity/range test on two already
decoded fields in one receive handler, rejecting the message through the
existing invalid-message policy. No packet layout, `TimeStep` arithmetic, or
tick structure changes, and valid server ratios behave identically.

## Acceptance criteria

- A correctly encoded `-1/1`, `0/1`, `1/0`, and out-of-range ratio is rejected
  before `SetTimeScale`; the prior valid ratio and accumulator remain usable.
- A valid server-generated ratio still updates the client and advances fixed
  ticks normally.
- Client and server `Debug|x64` builds clean through `/compile`.

## Notes

The consolidated index records no duplicate-family hint or external claim for
this candidate.
