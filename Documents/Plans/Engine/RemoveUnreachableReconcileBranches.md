<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:44:52.529Z","dependsOn":[]} -->
# Remove unreachable branches from the client reconcile replay chain

## Context

Found by the in-function refactoring phase of a deep analysis of
`Engine/Source/Network/Client` at commit `d87ad840`, and proven by an
independent read. Nothing was built or run. Line numbers are from that
commit; locate by symbol if they moved. Short names: `RR.cpp` is
`Engine/Source/Network/Client/ReconcileReplay.cpp`, `RR.h` is
`ReconcileReplay.h`, `Crc.cpp` is `ReconcileReplayCrc.cpp`, and `Tick.cpp` is
`ReconcileReplayTick.cpp`, all in that directory. `N` is
`engine::kiNetworkBufferSize`.

Four branches test for states no writer can produce. Each is a silent early
exit, which `.agents/references/cpp-conventions.md` rules out twice: no check
for an unproducible state, and no early return that hides an invalid value.

### Facts the proofs share

- F1, single writer. `ReconcileCoord` runs inside
  `common::gpMultithreading->Dispatch` (`RR.cpp:466-475`), which waits for
  every worker before it returns. The only other writers of
  `Cell::serverUpdates` and `Cell::optionalPendingFullState` are main-thread
  drain code in `ClientSessionRuntime.cpp` and `Cell::ResetClientState`, and
  none can run while main is blocked in `Dispatch`. Each worker touches only
  its own cell (`RR.cpp:463`). So within one `ReconcileCoord` call the cell
  changes only through the chain's own statements.
- F2, `serverUpdates` writers in the chain: `CrcApplyMatchResult` erases
  ticks `<= iHighestMatch` (`Crc.cpp:170`); adoption erases ticks
  `<= iAdoptedTick` (`RR.cpp:88`); `ReconcileReplayCoord` erases the tick it
  just validated (`Tick.cpp:254`). Nothing in the chain inserts.
- F3, `optionalPendingFullState` writers in the chain: three resets
  (`RR.cpp:38`, `87`, `235`). Nothing in the chain sets one.
- F4, `iTargetTick` is one value for the whole dispatch, so
  `FindDuePendingFullState` (`RR.h:130-137`) depends only on F3 state.
- F5, `iReplayWriteCount` is set to 0 by `CoordScratch::Reset` and the four
  replay-base resets, and incremented only by `ReconcileRunTickCoord` after
  its ring-full test passed (`Tick.cpp:94-98`, `172`). So it never exceeds
  `N - 1`.
- F6, the ring-full test at `Tick.cpp:94` stays reachable from
  `ReconcileReplayCoord`, because `kiMaximumBufferedFrames` is larger than
  `N - 1`: a consecutive range can be longer than the ring.
- F7, `ASSERT` is live in every build (`Common/ErrorUtils.h`).

### 1. The fallback empty-range branch in RunReplay (RR.cpp:250-259)

The branch needs fallback mode, `iMaximumConsecutive < riReplayStart`, and no
due pending full state. It cannot be taken:

1. The fallback call (`RR.cpp:378`) needs
   `iDesyncTick >= 0 && bShrunkRollback && iDesyncTick == iReplayStart`
   (line 376).
2. `bShrunkRollback` is true only through `RR.cpp:181`, which needs
   `iLowestUnresolvedMismatch` (call it `L`) `> iConfirmedTick + 1` and no
   due pending full state (line 162).
3. `L` is a key of `serverUpdates` with `L <= iTargetTick`: it comes from the
   tick list filtered at `Crc.cpp:53-59`. The only erase before the replay is
   of ticks `<= iHighestMatch` (`Crc.cpp:170`), and
   `L > iConfirmedTick + 1 > iHighestMatch`, because `iConfirmedTick` is the
   highest match when one exists (`Crc.cpp:266`). So
   `iTargetTick >= L >= iConfirmedTick + 2`.
4. `EarlyReturnIfNoServerData` returned false (`RR.cpp:350`), so at line 133
   either `serverUpdates.contains(iConfirmedTick + 1)` or a due pending full
   state existed. By step 2, F3, and F4, none is due at line 162 and nothing
   between changes that, so `contains(iConfirmedTick + 1)` held.
5. In the primary `RunReplay`, with no due pending full state, neither
   injection (line 224), discard (232), nor adoption (241) runs.
   `iReplayStart` is `L` (238). `ReconcileReplayCoord` at tick `L` finds the
   update (step 3), passes the ring-full test (write count 0 after rollback),
   and does not inject (a pending state that is not due has a tick above
   `iTargetTick`). The fallback condition needs the desync at this first
   tick, where `Tick.cpp:247-250` returns before the erase at line 254. So
   the primary pass erased nothing.
6. `rCell.iConfirmedTick` is not written between `RR.cpp:133` and `214`.
7. In the fallback, `riReplayStart` is `iConfirmedTick + 1` (lines 214, 238),
   which is still a key (steps 4 to 6, F1, F2), so
   `iUncappedMaximumConsecutive >= riReplayStart` (`Tick.cpp:36-45`). With
   `iTargetTick >= iConfirmedTick + 2` (step 3), `iMaximumConsecutive`, the
   smaller of the two, is `>= riReplayStart`.

The proof is not local to `RunReplay`: it rests on F1 and on facts in two
files.

Contradiction, named as root `AGENTS.md` `### Diagnosis Discipline` asks: the
comment at `RR.cpp:188-190` says of the fallback "only its empty range
resolves the coord here", which documents the empty range as an intended
case. The code proves the case cannot occur. A comment ranks above code
behavior for intent; this Plan trusts the reachability proof, and rewrites
the comment in the same change. The user sees this choice when the Plan is
presented for approval.

If a later edit broke the proof and the branch were gone, control would reach
`ReconcileReplayCoord` with an empty range, then catch-up and
`ComputeOutputLayout`, whose `iNewConfirmedTick >= 0` branch would pair a
walk-retained head with a new count and commit a wrong ring without a crash.
So an `ASSERT` of the non-empty fallback range is not a useless one.

### 2. ReconcileForwardStepCoord never returns false (Tick.cpp:281-284)

It returns false only when `ReconcileRunTickCoord` does, which is only the
ring-full test `iReplayWriteCount >= N - 1`. Both callers stay under it:

- `ReconcileCatchUpCoord` (`Tick.cpp:290-315`): the budget is
  `N - 1 - W0` where `W0` is the write count on entry, at most `N - 1` by F5.
  The loop runs at most that many times, one tick each, so the write count
  at the test is at most `N - 2`.
- `ReconcileFastPathCatchUp` (`Tick.cpp:319-363`): the write count is reset
  to 0 (line 339) and the budget is `N - iStartCount` with
  `iStartCount >= 1` (the `ASSERT` at line 324), so the write count at the
  test is at most `N - 2`.

So the `if`/`break` pairs at `Tick.cpp:304-307` and `349-352` never fire.
They would hide a broken budget if one existed. Simply discarding the
callee's `bool` would keep hiding it and would also count a tick that did not
run in `iAssumedFrameTicks` (line 286).

### 3. The `it == end` break in ReconcileReplayCoord (Tick.cpp:222-225)

Its only caller is `RR.cpp:261`, which passes `riReplayStart` and the smaller
of `iUncappedMaximumConsecutive` and `iTargetTick`, where
`iUncappedMaximumConsecutive` comes from `ReconcileFindReplayRangeCoord`
(`RR.cpp:239`), which proves every tick in the range is a key
(`Tick.cpp:36-45`). Between lines 239 and 261 the only `serverUpdates` writer
is adoption (244), which returns at 246. Inside the loop the only erase is of
the current tick after validation; `ReconcileRunTickCoord` works on a local
`frameInput` copy; injection does not touch the map; F1 covers other threads.
The proof is local: one caller, one preceding call.

### 4. Always-true tests and a redundant return in CrcFastPathProcessCoord

`Crc.cpp:187-190` returns when the map is empty and no pending full state is
due; lines 192-197 return when one is due. Past line 197 none is due, so the
map is not empty. Nothing between lines 187 and 248 changes the map:
`CrcValidateLoop` only reads it, and the first erase is at line 170 through
the call at line 260. So `!rCell.serverUpdates.empty()` at lines 202 and 248
is always true.

At lines 253-256, with `iHighestMatch == -1`, `bMatch`, and
`iConfirmedTick >= iTargetTick`, every later branch is false (line 258 needs
a match, 279 needs `!bMatch`, 288 needs `iConfirmedTick + 1 < iTargetTick`),
so control reaches `return result` at line 295 with the same untouched
`result`. The early return equals falling through.

Unmet condition: the reconcile replay chain holds no branch for a state it
cannot reach.

Originating gap: that analysis phase's review of these functions. It is not
an unmet acceptance criterion of any change. It is pre-existing: the analysis
session changed no source.

## Design

Recommended (the author's recommendation):

1. `RunReplay`: delete the branch at `RR.cpp:250-259`. Make the function
   return `void`: the `return false;` after adoption becomes `return;` and
   the final `return false;` is deleted. Replace the `if (RunReplay(...))`
   and its `return` at `RR.cpp:378-381` with the bare call. In fallback mode,
   add `ASSERT(iMaximumConsecutive >= riReplayStart)` where the branch was.
   Rewrite the comment at `RR.cpp:188-190` so it no longer says an empty
   fallback range resolves the coordinate. `rPreWritebackLayout` stays; line
   215 still reads it.
2. `ReconcileForwardStepCoord`: return `void`, capture the
   `ReconcileRunTickCoord` result, and `ASSERT` it. Delete the two
   `if`/`break` pairs in the callers. `ReconcileRunTickCoord` keeps its
   `bool` (F6), and the replay caller at `Tick.cpp:235-238` is untouched.
3. `ReconcileReplayCoord`: delete `Tick.cpp:222-225`. No `ASSERT` is added,
   because the proof is local.
4. `CrcFastPathProcessCoord`: drop `&& !rCell.serverUpdates.empty()` at
   `Crc.cpp:202` and `248`, and delete lines 253-256.

Risk tier: Tier 3 (the author's classification). Trigger: the edits are on
the determinism and CRC surface. The client reconcile chain decides which
frames a cell commits and compares against server CRCs, and the removals are
safe only because of reachability proofs that span files and rest on the
single-writer threading fact F1. The change is meant to compute exactly what
the code computes today. No packet, save, `.pack`, or `kiVersion` change.
The executing session derives the tier again and re-proves each branch
against the code it finds.

## Critical files

- `Engine/Source/Network/Client/ReconcileReplay.cpp`
- `Engine/Source/Network/Client/ReconcileReplayTick.cpp`
- `Engine/Source/Network/Client/ReconcileReplayCrc.cpp`

## In scope

- `RunReplay`: the fallback empty-range branch, the return type and `return`
  statements, the new fallback `ASSERT`, and the comment above
  `enum class ReplayMode`.
- `ReconcileCoord`: the handling of the fallback `RunReplay` result.
- `ReconcileForwardStepCoord`: return type, the result test, and the new
  `ASSERT`.
- `ReconcileCatchUpCoord` and `ReconcileFastPathCatchUp`: the `if`/`break`
  around the forward step.
- `ReconcileReplayCoord`: the `it == end` break.
- `CrcFastPathProcessCoord`: the two `!rCell.serverUpdates.empty()` operands
  and the early return at lines 253-256.

## Out of scope

- `ReconcileRunTickCoord`, its ring-full test, and the `break` at
  `Tick.cpp:235-238`.
- The gap early return at `Crc.cpp:248-251` apart from the one operand, and
  every `bHandled` outcome.
- `ComputeOutputLayout`, `AdoptUnreachablePendingFullState`, and
  `DetermineRollbackBase`.
- The `DEBUG_BREAK` repeat-work guard in `ReconcileCoord`.
- Gating the reconcile dispatch on `kbFrameDispatch`: nothing in the
  repository states what that switch is meant to cover.
- Whether a desync return without writeback leaves `iHighWaterValidatedTick`
  and erased updates consistent for the next reconcile. It was seen beside
  proof 1, was not traced, and does not affect the proof.

## Invariants

- Every path commits the ring head, count, confirmed offset, and confirmed
  tick it commits today.
- `iAssumedFrameTicks` and the committed `iSnapshotCount` are identical.
- A reconcile worker writes only its own coordinate's state.

## Acceptance criteria

1. The four branches are gone, `RunReplay` and `ReconcileForwardStepCoord`
   return `void`, and the two new `ASSERT`s exist.
2. `/compile` builds the BrokenEngineSandbox client and server.
3. The replay determinism recipe in
   `Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md` shows no
   CRC mismatch, the same result as before the change.
4. The resimulation check in
   `Projects/BrokenEngineSandbox/Documents/AgentHarness/verification.md`
   (steps 1 to 7) passes as that document states it, with no `ASSERT` in the
   client log.

The supplied evidence records no harness setup for criteria 3 and 4: nothing
was built or run. The executing session sets them up from the harness
documentation.

## Coordination

No Plan below depends on this one, and it depends on none. All edit the same
reconcile files; whichever lands second re-derives its regions by symbol from
the landed files and re-checks the proofs above against them.

- `Documents/Plans/Engine/RemoveRedundantReconcileReplayState.md` removes a
  `RunReplay` parameter and the two stores around the adoption return that
  this Plan turns into a bare `return;`. Both change the `RunReplay`
  signature and both of its calls.
- `Documents/Plans/Engine/ShareReconcileSnapshotSearchAndBaseReset.md` moves
  the replay-base resets that F5 counts into one helper.
- `Documents/Plans/Engine/TrimReconcileReplayHeaderSurface.md` makes
  `ReconcileCoord` `static` and replaces reads of `iReplayStackCount` in the
  tick functions.
