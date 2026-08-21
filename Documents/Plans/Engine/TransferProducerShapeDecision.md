<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-20T18:28:50.047Z","dependsOn":[]} -->
# Cross-Cell Transfer Producer Shape Decision

## Context

The four game-side cross-cell transfer producers repeat the same six-step
shape: test the `kTransfer` flag, build a `TransferRequest`, call
`engine::ComputeTransferDelta`, `DEBUG_BREAK()` on a capacity hit, run
`common::ValidateVector` on the request vectors, `push_back` under
`ScopedSuppressAllocationTracking`, then remove/destroy the element. The
sites are:

- Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/Blasters.cpp:168-221
- Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.cpp:332-394
- Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.cpp:377-444
- Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersNavigation.cpp:233-305

The repetition is shape-level, not byte-level, and the divergence is
verified in the current tree:

- Each producer fills a different `TransferData` field subset.
- Each capacity-hit LOG carries different fields: Blasters.cpp:204 logs
  `TypeIndex`, PlayersNavigation.cpp:285 logs `GlobalPlayerId`,
  Missiles.cpp:377 logs neither, and Spaceships.cpp:413-427 is the only
  multi-line LOG call.
- Missiles alone carries the `BT_CLIENT`-only `smokeTrailId` asymmetry
  declared in Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:109-117
  and 178.
- The genuinely shared geometry work is already factored out into
  `engine::ComputeTransferDelta` and `engine::ComputeFrameBounds`.

Origin: this is the Q3 producer-ownership question of
Documents/Plans/Engine/ChangeListTransportContract.md, deferred out of that
Plan's implementation by an explicit user decision at its Tier-3 grill. It is
recorded here so the deferral is tracked, not so it is implemented by default.

## Design

The decision itself is deliberately pre-staged, not made here, because it is a
game-side ownership choice the user deferred. The implementing session must
resolve it with the user at Change Workflow Step 1 before touching source, and
must choose exactly one of:

1. No change. The divergence above (per-collection field sets, per-collection
   log fields, the client-only field) is real, so under the repository's
   minimum-sufficient-change and "extract helpers only for current
   duplication" directives, leaving four explicit producers can be the correct
   answer. Selecting this option completes the Plan with no code edit.
2. A game-side helper that owns only the provably identical steps (delta
   computation, vector validation, capacity check, allocation-suppressed
   push), with each producer still constructing its own `TransferData` and
   emitting its own LOG text.
3. A per-collection field-list table that drives construction and logging.

Whichever option is selected, every producer keeps its exact field set, its
exact log content, its allocation-suppression comment, and the client-only
`smokeTrailId` asymmetry. If the selected option would require changing any of
those, return the work for re-planning instead of expanding scope.

## Critical files

- Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/Blasters.cpp
- Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.cpp
- Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.cpp
- Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersNavigation.cpp
- Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h

## In scope

- Selecting one of the three options above with the user, and recording it.
- The four `Transfer` producer loops named in `## Critical files`, only if the
  selected option changes code.
- A game-side helper or field-list table introduced solely for those loops.

## Out of scope

- `TransferRequest` layout, the `transferRequests` buffer location, the wire
  payload, and cross-cell transfer ordering, all owned by
  Documents/Plans/Engine/ChangeListTransportContract.md.
- Any change to a producer's field set, log content, allocation-suppression
  comment, or the `BT_CLIENT`-only `smokeTrailId` asymmetry.
- Engine-side transfer harvest, publication, replay, and CRC paths.
- Unrelated collection extraction or new producers.

## Risk tier and invariants

Expected Change Workflow Tier 3: the producers feed the wire payload and the
deterministic cross-cell transfer ordering that the CRC covers, which the
root AGENTS.md Tier-2 exclusion list names. Preserve PostRender bit
determinism, transfer ordering, allocation-tracking rules, and client/server
build symmetry apart from the declared `smokeTrailId` asymmetry.

## Acceptance criteria

- The selected option is recorded before any source edit.
- If option 1 is selected, no tracked source file changes.
- If option 2 or 3 is selected: each producer's emitted `TransferData` field
  values and capacity-hit LOG text are unchanged, client and server both
  build, and a harness run with cross-cell transfers shows no new transfer,
  CRC, or desync errors.

## Notes

The parent Plan's `## In scope` line "the bounded transfer-producer ownership
choice and the four current producer loops" nominally covered this work; the
user's grill decision Q3 deferred it, and user direction outranks the plan
text.
