<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:44:38.125Z","dependsOn":[]} -->
# Share the reconcile snapshot search and the replay-base reset

## Context

Found by the architecture review phase of a deep analysis of
`Engine/Source/Network/Client` at commit `d87ad840`. Line numbers are from
that commit; locate by symbol if they moved. Paths below are under
`Engine/Source/Network/Client/`.

### The snapshot search is written twice

`DetermineRollbackBase` (`ReconcileReplay.cpp:167-176`) loops the cell's ring
for a non-null snapshot whose `interpolate.iTick` equals `iShrunkTick` and
keeps the first logical index, or -1. That is `FindSnapshotIndex`, the
`static` function at `ReconcileReplayCrc.cpp:13-24`, called with the cell's
`ppSnapshots`, `iSnapshotHead`, and `iSnapshotCount` and with `iShrunkTick`,
written inline: same bound, same `SnapshotIndex`, same null test, same tick
compare. Its one existing call (`ReconcileReplayCrc.cpp:65`) passes the same
three cell fields.

### The replay-base reset is written four times

These five statements, with only the base frame and base slot differing,

    replayStack.clear();
    replayStack.push_back(<base frame>);
    iReplayStackCount = 1;
    iReplayWriteHead = SnapshotIndex(<base slot>, 1);
    iReplayWriteCount = 0;

occur at `ReconcileReplay.cpp:29-33` (`ReconcileInjectPendingFullState`),
`ReconcileReplay.cpp:71-75` (`AdoptUnreachablePendingFullState`),
`ReconcileReplayTick.cpp:24-28` (`ReconcileRollbackCoord`), and
`ReconcileReplayTick.cpp:335-339` (`ReconcileFastPathCatchUp`).
`ReconcileReplayTick.cpp:27` writes
`SnapshotIndex(rCell.iSnapshotHead, iRollbackOffset + 1)`, which equals
`SnapshotIndex(iRollbackPhysical, 1)`: `SnapshotIndex` is
`(head + logical) % kiNetworkBufferSize` (`Engine/Source/GameBase.h`), and the
offset is asserted non-negative at line 22.

Two writes sit beside the five at some sites only, and are not part of what
repeats:

- `iLastValidatedIndex = -1` follows at `ReconcileReplayTick.cpp:29` and
  `ReconcileReplay.cpp:76`, but not at `ReconcileReplay.cpp:29-33` or
  `ReconcileReplayTick.cpp:335-339`. Folding it into a shared helper would
  change the mid-replay injection path (`ReconcileReplayTick.cpp:241-245`)
  when the validation right after it mismatches, visible in the logged
  `Validated:` value.
- `iInjectedBaseSlot = iSlot` is written only at `ReconcileReplay.cpp:34`.

This state feeds the committed ring layout, so four hand-kept copies are four
places to get it wrong.

Unmet condition: the snapshot search and the replay-base reset each have one
definition.

Originating gap: that review's module-cohesion lens. It is not an unmet
acceptance criterion of any change. It is pre-existing: the analysis session
changed no source.

## Design

Recommended (the author's recommendation):

- Declare `FindSnapshotIndex` once in `ReconcileReplay.h`, taking
  `(const engine::Cell& rCell, int64_t iTick)`, since both callers pass the
  same three cell fields. Call it from `DetermineRollbackBase` in place of the
  inline loop, and keep the `iShrunkIndex >= 0` handling that follows.
- Add one `CoordScratch` member beside `Reset` that holds exactly the five
  statements, taking the base frame pointer and the base slot. It needs no
  `Cell`. Call it from the four sites. Rationale: `CoordScratch` owns all
  five fields.
- Leave every `iLastValidatedIndex` and `iInjectedBaseSlot` write at its
  current site, outside the helper.

The two `while` loops in `ReconcileCatchUpCoord` and
`ReconcileFastPathCatchUp` are textually identical, but their budgets, start
ticks, and commit differ, and the two functions are deliberately parallel
(root `AGENTS.md` Directives, "Mirrored patterns stay parallel"). The author
recommends not sharing them; they are out of scope.

`Projects/Coretide` is a private game project that is absent from a session
worktree, and game reconcilers read `CoordScratch`. This Plan only adds to
`ReconcileReplay.h`, but the executing session still searches
`Projects/Coretide` in the primary checkout for `FindSnapshotIndex` and the
new member's name, to rule out a name clash, and reports what it finds.

Risk tier: Tier 2 (the author's classification). Trigger: the edit is on the
client reconcile chain, whose committed ring is CRC-compared, and it adds a
declaration and a member to an engine header that game projects include. It
moves identical statements and computes nothing new. Threading is unchanged:
each reconcile worker still writes only its own `CoordWork`. No packet, save,
`.pack`, or `kiVersion` change.

## Critical files

- `Engine/Source/Network/Client/ReconcileReplay.h`
- `Engine/Source/Network/Client/ReconcileReplay.cpp`
- `Engine/Source/Network/Client/ReconcileReplayCrc.cpp`
- `Engine/Source/Network/Client/ReconcileReplayTick.cpp`

## In scope

- `FindSnapshotIndex`: its signature, linkage, one declaration, and its call
  in `CrcValidateLoop`.
- `DetermineRollbackBase`: the inline search loop.
- One new `CoordScratch` member holding the five statements.
- The five statements at the four sites named in `## Context`.

## Out of scope

- The `iLastValidatedIndex` and `iInjectedBaseSlot` writes.
- The catch-up `while` loops, the flag-clear loops after them, and the
  budgets.
- The rest of `ReconcileInjectPendingFullState` and
  `AdoptUnreachablePendingFullState`.
- `CoordScratch::Reset` and every existing `CoordScratch` member.

## Invariants

- The ring layout every path commits is byte-identical to today's.
- A reconcile worker writes only its own coordinate's state.

## Acceptance criteria

1. The search loop appears once and the five statements appear once in the
   four files.
2. `/compile` builds the BrokenEngineSandbox client and server.
3. The replay determinism recipe in
   `Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md` shows no
   CRC mismatch.
4. The resimulation check in
   `Projects/BrokenEngineSandbox/Documents/AgentHarness/verification.md`
   passes as that document states it.

The supplied evidence records no harness setup for criteria 3 and 4: nothing
was built or run. The executing session sets them up from the harness
documentation.

## Coordination

No Plan below depends on this one, and it depends on none. All edit the same
reconcile files; whichever lands second re-derives its regions by symbol from
the landed files.

- `Documents/Plans/Engine/TrimReconcileReplayHeaderSurface.md` removes
  `iReplayStackCount`. If it lands first, the shared helper holds four
  statements, not five; if this Plan lands first, that Plan removes the
  statement from the helper.
- `Documents/Plans/Engine/RemoveRedundantReconcileReplayState.md` makes
  `AdoptUnreachablePendingFullState` call
  `ReconcileInjectPendingFullState`, which removes the adoption copy of the
  five statements. If it lands first, this Plan has three sites.
- `Documents/Plans/Engine/RemoveUnreachableReconcileBranches.md` edits
  `RunReplay` and the catch-up loops in the same files.
- `Documents/Plans/Engine/ClearWindTrailCacheAfterFullState.md` adds a signal
  to the injection and adoption functions this Plan edits.
