<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:44:57.171Z","dependsOn":[]} -->
# Remove redundant state and a repeated block from ReconcileReplay.cpp

## Context

Found by the in-function refactoring phase of a deep analysis of
`Engine/Source/Network/Client` at commit `d87ad840`, and confirmed by an
independent read. Nothing was built or run. Line numbers are from that commit
and are in `Engine/Source/Network/Client/ReconcileReplay.cpp` unless a file is
named; locate by symbol if they moved. `N` is `engine::kiNetworkBufferSize`.

Each item is a statement, parameter, or block that changes nothing today.

Shared fact: `CoordScratch::iReplayWriteCount` is set to 0 by
`CoordScratch::Reset` and the replay-base resets, and incremented only by
`ReconcileRunTickCoord` after its ring-full test passed
(`ReconcileReplayTick.cpp:94-98`, `172`), so it never exceeds `N - 1`.

### 1. A `std::min` that clamps nothing and blinds the ASSERT after it

`ComputeOutputLayout` ends with
`outputLayout.iCount = std::min(outputLayout.iCount, N)` (line 328) and then
`ASSERT(outputLayout.iCount >= 0 && outputLayout.iCount <= N)` (line 329).
`iCount <= N` already holds at line 328: the first branch (274-295) trims any
excess at lines 288-294; the other three (296-317) set
`iReplayWriteCount + 1`, at most `N` by the shared fact; lines 322-326 do not
touch `iCount`. So the `min` never changes the value, and it makes the upper
half of the `ASSERT` impossible to fail. It is a silent clamp of a value
another unit sets, which `.agents/references/cpp-conventions.md` rules out.

### 2. Two branches with one body

In `ComputeOutputLayout`, the `iLastValidatedIndex == 0` branch (296-299) and
the `iNewConfirmedTick >= 0` branch (300-305) hold the same statement,
`outputLayout.iCount = iReplayWriteCount + 1`. Index 0 implies
`iNewConfirmedTick >= 0`: the only writer of a non-negative
`iLastValidatedIndex` is `ReconcileReplayTick.cpp:207`, and line 208 sets
`iNewConfirmedTick` to the tick with it; nothing lowers it afterwards. The
function comment (265-268) names four subcases and the comment at 302-303
describes only the walk case.

### 3. A write-only reference parameter and two dead stores

`RunReplay` takes `bool& rbShrunkRollback` and never reads it. Its one write
(line 216) is in fallback mode, and nothing reads the caller's
`bShrunkRollback` after the fallback call: the variable (line 370) is written
by `DetermineRollbackBase` (160, 181) and read once, at line 376, before that
call. The log flag `ReconcileScratchFlags::kShrunkRollback` (217, read at 518
and 522) is separate and stays.

After adoption, `riReplayStart = iAdoptedTick + 1` (line 245) is a dead
store: `iDesyncTick` is still -1 (adoption returns before any validation), so
the test at line 376 short-circuits and `iReplayStart` is not read again.
`iAdoptedTick` (line 243) then has no other use in `RunReplay`.

### 4. Adoption repeats injection

`AdoptUnreachablePendingFullState` (58-93) repeats
`ReconcileInjectPendingFullState` (15-39): the tick `ASSERT`, the slot from
the write head and count, the CRC stamp, the move into the ring, the five
replay-base reset statements, the high-water and last-full-state ticks, and
the pending reset (lines 64-75 and 85-87 against 25-38).

Differences: injection also sets `iInjectedBaseSlot` (34). Adoption also
asserts `pFrame != nullptr` (63), writes `iLastValidatedIndex = -1` (76,
already -1 from the rollback at line 220 with no validation between), sets
`iNewConfirmedTick` (77) and `outputLayout` (78-83), erases updates (88),
clears the repeat-work guard (89-90), and logs (92).

Calling the injection function from adoption is behavior-preserving.
`iInjectedBaseSlot` is read only in `ComputeOutputLayout`. After adoption
`RunReplay` returns, there is no desync, catch-up runs, and
`ComputeOutputLayout` always runs with `iLastValidatedIndex == -1` and
`iNewConfirmedTick >= 0`: the `iNewConfirmedTick >= 0` branch sets `iCount`,
and lines 322-326 then set the head to the adopted slot and the confirmed
offset to 0, exactly what lines 78-83 wrote. The `iCount = 1` at line 81 is
already overwritten today. The comment on `iInjectedBaseSlot` in
`ReconcileReplay.h` already describes this meaning.

Two hand-kept copies of the CRC stamp and high-water update, on the path that
installs an authoritative base, is duplication that desyncs when one copy is
edited.

Unmet condition: `ReconcileReplay.cpp` holds no clamp, parameter, store, or
block whose removal changes nothing.

Originating gap: that analysis phase's review of these functions. It is not
an unmet acceptance criterion of any change. It is pre-existing: the analysis
session changed no source.

## Design

Recommended (the author's recommendation):

1. Delete the `std::min` line (328). Keep the `ASSERT` at line 329.
   `ApplyCoordWriteback` repeats the same range `ASSERT` on the committed
   count (line 54), so removing line 329 too would be equally correct; the
   author recommends keeping it, as the smaller change.
2. Merge the two same-body branches by deleting the
   `iLastValidatedIndex == 0` branch (296-299), and correct the function
   comment (265-268) and the branch comment (302-303) so they describe what
   the remaining branch covers.
3. Remove the `rbShrunkRollback` parameter from `RunReplay` and the store at
   line 216, and update both calls (374, 378). Remove lines 243 and 245.
   `riRollbackOffset` stays a reference (read at 391), as does
   `riReplayStart` (read at 376).
4. In `AdoptUnreachablePendingFullState`: capture `iAdoptedTick` first,
   because injection resets the optional that `rPending` refers to; call
   `ReconcileInjectPendingFullState(rWork)`; keep the `iNewConfirmedTick`
   assignment (77), the update erase, the repeat-work guard clears, and the
   log (88-92). Drop everything else in lines 63-87: the `pFrame` `ASSERT`,
   the lines that repeat injection, the `iLastValidatedIndex` write, and the
   `outputLayout` write.

Risk tier: Tier 3 (the author's classification). Trigger: the edits are on
the determinism and CRC surface. `ComputeOutputLayout` and the adoption path
decide the ring a cell commits, and item 4 is safe only by the reasoning
above about what `ComputeOutputLayout` later writes. The change is meant to
compute exactly what the code computes today. Threading is unchanged: these
functions run in a reconcile worker that writes only its own coordinate. No
packet, save, `.pack`, or `kiVersion` change. The executing session derives
the tier again and re-checks each claim against the code it finds.

## Critical files

- `Engine/Source/Network/Client/ReconcileReplay.cpp`

## In scope

- `ComputeOutputLayout`: the `std::min` line, the `iLastValidatedIndex == 0`
  branch, the function comment, and the comment in the
  `iNewConfirmedTick >= 0` branch.
- `RunReplay`: the `rbShrunkRollback` parameter and its store, and the
  `iAdoptedTick` local and `riReplayStart` store around the adoption call.
- `ReconcileCoord`: the two `RunReplay` calls' argument lists.
- `AdoptUnreachablePendingFullState`: the lines that repeat injection, the
  `pFrame` `ASSERT`, the `iLastValidatedIndex` write, and the `outputLayout`
  write.

## Out of scope

- `ReconcileInjectPendingFullState` itself, and `iInjectedBaseSlot`.
- `DetermineRollbackBase` and its `rbShrunkRollback` output, which
  `ReconcileCoord` still reads.
- `ReconcileScratchFlags::kShrunkRollback` and the "Reconcile post-replay"
  log lines.
- `ApplyCoordWriteback` and its `ASSERT`.
- A shared helper for the five replay-base reset statements, and
  `CoordScratch::Reset`.
- The one-field `ReconcileInputs` struct.

## Invariants

- After adoption, the committed head, count, and confirmed offset are
  identical to today's.
- Every other path commits the ring it commits today.
- The adoption log line keeps its text.

## Acceptance criteria

1. The four items of `## Design` are in the diff and nothing else in the file
   changes.
2. `/compile` builds the BrokenEngineSandbox client and server.
3. The replay determinism recipe in
   `Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md` shows no
   CRC mismatch, the same result as before the change.
4. The resimulation check in
   `Projects/BrokenEngineSandbox/Documents/AgentHarness/verification.md`
   passes as that document states it, with no `ASSERT` in the client log.

The supplied evidence records no harness setup for criteria 3 and 4, and none
that makes a client adopt a pending full state past an update gap. If the
executing session finds no such setup, item 4 of `## Design` is verified by
code reading plus criteria 2 to 4.

## Coordination

No Plan below depends on this one, and it depends on none. All edit the same
reconcile files; whichever lands second re-derives its regions by symbol from
the landed files.

- `Documents/Plans/Engine/RemoveUnreachableReconcileBranches.md` makes
  `RunReplay` return `void` and turns the adoption `return false;` into
  `return;`. Both change the `RunReplay` signature and both of its calls.
- `Documents/Plans/Engine/ShareReconcileSnapshotSearchAndBaseReset.md` moves
  the five replay-base reset statements into one helper. Item 4 here removes
  the adoption copy of them; if that Plan lands first, adoption's call to the
  helper goes with the rest of the repeated lines.
- `Documents/Plans/Engine/TrimReconcileReplayHeaderSurface.md` removes
  `iReplayStackCount`, one of those five statements, and makes
  `ReconcileCoord` `static`.
- `Documents/Plans/Engine/ClearWindTrailCacheAfterFullState.md` adds a signal
  to the injection and adoption functions. Once adoption calls injection, a
  signal set in injection covers both.
