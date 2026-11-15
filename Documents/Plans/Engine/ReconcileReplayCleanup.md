<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-15T13:27:28.000Z","dependsOn":[]} -->
# Clean up the client reconcile replay chain

## Context

Four clean-ups found by a deep analysis of `Engine/Source/Network/Client` at
commit `d87ad840` (in-function refactoring and architecture review phases),
merged into one Plan by user direction because they edit the same reconcile
files and each needs the same replay checks. Nothing was built or run. Line
numbers are from that commit; the sites spot-checked at `66452fe5` had not
moved. Locate by symbol if they have. Short names: `RR.cpp` is
`Engine/Source/Network/Client/ReconcileReplay.cpp`, `RR.h` is
`ReconcileReplay.h`, `Crc.cpp` is `ReconcileReplayCrc.cpp`, and `Tick.cpp` is
`ReconcileReplayTick.cpp`, all in that directory. `N` is
`engine::kiNetworkBufferSize`.

### Facts the items share

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
- F5, `CoordScratch::iReplayWriteCount` is set to 0 by `CoordScratch::Reset`
  and the replay-base resets, and incremented only by `ReconcileRunTickCoord`
  after its ring-full test passed (`Tick.cpp:94-98`, `172`). So it never
  exceeds `N - 1`.
- F6, the ring-full test at `Tick.cpp:94` stays reachable from
  `ReconcileReplayCoord`, because `kiMaximumBufferedFrames` is larger than
  `N - 1`: a consecutive range can be longer than the ring.
- F7, `ASSERT` is live in every build (`Common/ErrorUtils.h`).

### A. Redundant state and a repeated block in RR.cpp

Each item is a statement, parameter, or block that changes nothing today.

A1. `ComputeOutputLayout` ends with
`outputLayout.iCount = std::min(outputLayout.iCount, N)` (line 328) and then
`ASSERT(outputLayout.iCount >= 0 && outputLayout.iCount <= N)` (line 329).
`iCount <= N` already holds at line 328: the first branch (274-295) trims any
excess at lines 288-294; the other three (296-317) set
`iReplayWriteCount + 1`, at most `N` by F5; lines 322-326 do not touch
`iCount`. So the `min` never changes the value, and it makes the upper half
of the `ASSERT` impossible to fail: a silent clamp of a value another unit
sets, which `.agents/references/cpp-conventions.md` rules out.

A2. In `ComputeOutputLayout`, the `iLastValidatedIndex == 0` branch
(296-299) and the `iNewConfirmedTick >= 0` branch (300-305) hold the same
statement, `outputLayout.iCount = iReplayWriteCount + 1`. Index 0 implies
`iNewConfirmedTick >= 0`: the only writer of a non-negative
`iLastValidatedIndex` is `Tick.cpp:207`, and line 208 sets
`iNewConfirmedTick` to the tick with it; nothing lowers it afterwards. The
function comment (265-268) names four subcases and the comment at 302-303
describes only the walk case.

A3. `RunReplay` takes `bool& rbShrunkRollback` and never reads it. Its one
write (line 216) is in fallback mode, and nothing reads the caller's
`bShrunkRollback` after the fallback call: the variable (line 370) is written
by `DetermineRollbackBase` (160, 181) and read once, at line 376, before that
call. The log flag `ReconcileScratchFlags::kShrunkRollback` (217, read at 518
and 522) is separate and stays. After adoption,
`riReplayStart = iAdoptedTick + 1` (line 245) is a dead store: `iDesyncTick`
is still -1 (adoption returns before any validation), so the test at line
376 short-circuits and `iReplayStart` is not read again. `iAdoptedTick`
(line 243) then has no other use in `RunReplay`.

A4. `AdoptUnreachablePendingFullState` (58-93) repeats
`ReconcileInjectPendingFullState` (15-39): the tick `ASSERT`, the slot from
the write head and count, the CRC stamp, the move into the ring, the
replay-base reset statements, the high-water and last-full-state ticks, and
the pending reset (lines 64-75 and 85-87 against 25-38). Injection also sets
`iInjectedBaseSlot` (34). Adoption also asserts `pFrame != nullptr` (63),
writes `iLastValidatedIndex = -1` (76, already -1 from the rollback at line
220 with no validation between), sets `iNewConfirmedTick` (77) and
`outputLayout` (78-83), erases updates (88), clears the repeat-work guard
(89-90), and logs (92).

Calling injection from adoption is behavior-preserving. `iInjectedBaseSlot`
is read only in `ComputeOutputLayout`. After adoption `RunReplay` returns,
there is no desync, catch-up runs, and `ComputeOutputLayout` always runs with
`iLastValidatedIndex == -1` and `iNewConfirmedTick >= 0`: the
`iNewConfirmedTick >= 0` branch sets `iCount`, and lines 322-326 then set the
head to the adopted slot and the confirmed offset to 0, exactly what lines
78-83 wrote. The `iCount = 1` at line 81 is already overwritten today. The
comment on `iInjectedBaseSlot` in `RR.h` already describes this meaning.

`rPending` in adoption is `&*rCell.optionalPendingFullState`
(`FindDuePendingFullState`), and injection ends with
`rCell.optionalPendingFullState.reset()` (line 38), which destroys the object
`rPending` names. So adoption must read `rPending.iTick` into `iAdoptedTick`
before it calls injection; the erase, guard clears, and log then use only
`iAdoptedTick`.

### B. Unreachable branches in the replay chain

Four branches test for states no writer can produce. Each is a silent early
exit, which `.agents/references/cpp-conventions.md` rules out twice: no check
for an unproducible state, and no early return that hides an invalid value.

B1. The fallback empty-range branch in `RunReplay` (`RR.cpp:250-259`) needs
fallback mode, `iMaximumConsecutive < riReplayStart`, and no due pending full
state. It cannot be taken:

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
files. If a later edit broke it, control would reach `ReconcileReplayCoord`
with an empty range, then catch-up and `ComputeOutputLayout`, whose
`iNewConfirmedTick >= 0` branch would pair a walk-retained head with a new
count and commit a wrong ring without a crash. So an `ASSERT` of the
non-empty fallback range is not a useless one.

Contradiction, named as root `AGENTS.md` `### Diagnosis Discipline` asks: the
comment at `RR.cpp:188-190` says of the fallback "only its empty range
resolves the coord here", which documents the empty range as an intended
case. The code proves the case cannot occur. A comment ranks above code
behavior for intent; this Plan trusts the reachability proof and rewrites the
comment in the same change. The user sees this choice when the Plan is
presented for approval.

B2. `ReconcileForwardStepCoord` (`Tick.cpp:281-284`) returns false only when
`ReconcileRunTickCoord` does, which is only the ring-full test
`iReplayWriteCount >= N - 1`. Both callers stay under it:

- `ReconcileCatchUpCoord` (`Tick.cpp:290-315`): the budget is `N - 1 - W0`
  where `W0` is the write count on entry, at most `N - 1` by F5. The loop runs
  at most that many times, one tick each, so the write count at the test is
  at most `N - 2`.
- `ReconcileFastPathCatchUp` (`Tick.cpp:319-363`): the write count is reset
  to 0 (line 339) and the budget is `N - iStartCount` with
  `iStartCount >= 1` (the `ASSERT` at line 324), so the write count at the
  test is at most `N - 2`.

So the `if`/`break` pairs at `Tick.cpp:304-307` and `349-352` never fire.
They would hide a broken budget if one existed. Simply discarding the
callee's `bool` would keep hiding it and would also count a tick that did not
run in `iAssumedFrameTicks` (line 286).

B3. The `it == end` break in `ReconcileReplayCoord` (`Tick.cpp:222-225`). Its
only caller is `RR.cpp:261`, which passes `riReplayStart` and the smaller of
`iUncappedMaximumConsecutive` and `iTargetTick`, where
`iUncappedMaximumConsecutive` comes from `ReconcileFindReplayRangeCoord`
(`RR.cpp:239`), which proves every tick in the range is a key
(`Tick.cpp:36-45`). Between lines 239 and 261 the only `serverUpdates` writer
is adoption (244), which returns at 246. Inside the loop the only erase is of
the current tick after validation; `ReconcileRunTickCoord` works on a local
`frameInput` copy; injection does not touch the map; F1 covers other threads.
The proof is local: one caller, one preceding call.

B4. `CrcFastPathProcessCoord`: `Crc.cpp:187-190` returns when the map is
empty and no pending full state is due; lines 192-197 return when one is due.
Past line 197 none is due, so the map is not empty. Nothing between lines 187
and 248 changes the map: `CrcValidateLoop` only reads it, and the first erase
is at line 170 through the call at line 260. So `!rCell.serverUpdates.empty()`
at lines 202 and 248 is always true. At lines 253-256, with
`iHighestMatch == -1`, `bMatch`, and `iConfirmedTick >= iTargetTick`, every
later branch is false (line 258 needs a match, 279 needs `!bMatch`, 288 needs
`iConfirmedTick + 1 < iTargetTick`), so control reaches `return result` at
line 295 with the same untouched `result`. The early return equals falling
through.

### C. Redundant declarations in RR.h

C1. `ReconcileCoord` is declared at `RR.h:141`, defined at `RR.cpp:332`, and
called once, at `RR.cpp:472`, after its definition. The other seven free
functions declared beside it are each used from a second translation unit.
`Documents/C++StyleGuide.txt` rule 66 makes a file-local function `static`.
The name also appears as a diagram label in
`Documents/Architecture/GameReconciliation.md` (lines 25 and 89), which stays
true.

C2. `CoordScratch::iReplayStackCount` (`RR.h:72`) always equals
`replayStack.size()`. Every writer changes both together: `Reset`
(`RR.h:94-95`), the four replay-base resets (`RR.cpp:29-33`, `71-75`;
`Tick.cpp:24-28`, `335-339`), and `ReconcileRunTickCoord`
(`Tick.cpp:170-171`). No game code writes either. Readers: `Tick.cpp:106`,
`182`, `207`, `296`; `RR.cpp:393`, `518`, `522`, `534`; and in
`Projects/BrokenEngineSandbox/Source/Network/Client/`,
`ClientReconciler.cpp:54` and `ReconcileReplayClientState.cpp:29`.

C3. `ReconcileDispatcher::miActiveCount` (`RR.h:177`) is written at
`RR.cpp:434` and `571`, copied into `result.iActiveCount` at line 437, and
read outside the class only at
`Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.cpp:103`,
where the dispatch result with the same value is in scope (`dispatch`, from
the `Run` call at lines 89-91).

### D. The snapshot search and the replay-base reset are written more than once

D1. `DetermineRollbackBase` (`RR.cpp:167-176`) loops the cell's ring for a
non-null snapshot whose `interpolate.iTick` equals `iShrunkTick` and keeps
the first logical index, or -1. That is `FindSnapshotIndex`, the `static`
function at `Crc.cpp:13-24`, called with the cell's `ppSnapshots`,
`iSnapshotHead`, and `iSnapshotCount` and with `iShrunkTick`, written inline:
same bound, same `SnapshotIndex`, same null test, same tick compare. Its one
existing call (`Crc.cpp:65`) passes the same three cell fields.

D2. These statements, with only the base frame and base slot differing,

    replayStack.clear();
    replayStack.push_back(<base frame>);
    iReplayStackCount = 1;
    iReplayWriteHead = SnapshotIndex(<base slot>, 1);
    iReplayWriteCount = 0;

occur at `RR.cpp:29-33` (`ReconcileInjectPendingFullState`), `RR.cpp:71-75`
(`AdoptUnreachablePendingFullState`), `Tick.cpp:24-28`
(`ReconcileRollbackCoord`), and `Tick.cpp:335-339`
(`ReconcileFastPathCatchUp`). `Tick.cpp:27` writes
`SnapshotIndex(rCell.iSnapshotHead, iRollbackOffset + 1)`, which equals
`SnapshotIndex(iRollbackPhysical, 1)`: `SnapshotIndex` is
`(head + logical) % kiNetworkBufferSize` (`Engine/Source/GameBase.h`), and
the offset is asserted non-negative at line 22. After A4 the adoption copy is
gone and after C2 the `iReplayStackCount` line is gone, so four statements
remain at three sites.

Two writes sit beside the statements at some sites only and are not part of
what repeats: `iLastValidatedIndex = -1` follows at `Tick.cpp:29` (and at
`RR.cpp:76`, removed by A4) but not at `RR.cpp:29-33` or `Tick.cpp:335-339`.
Folding it into a shared helper would change the mid-replay injection path
(`Tick.cpp:241-245`) when the validation right after it mismatches, visible
in the logged `Validated:` value. `iInjectedBaseSlot = iSlot` is written only
at `RR.cpp:34`.

Unmet condition: the reconcile replay chain holds no clamp, parameter, store,
or block whose removal changes nothing, no branch for a state it cannot
reach, no file-local function declared in `RR.h` or member that repeats
another value, and one definition each of the snapshot search and the
replay-base reset.

Originating gap: that analysis's review of these functions. It is not an
unmet acceptance criterion of any change. It is pre-existing: the analysis
session changed no source.

## Design

Recommended (the author's recommendation). The steps are listed in the order
to apply them; no source required an order, and this one edits each
`RunReplay` and adoption region once before the shared helper is extracted.

1. `ComputeOutputLayout` (A1, A2): delete the `std::min` line (328) and keep
   the `ASSERT` at line 329. `ApplyCoordWriteback` repeats the same range
   `ASSERT` on the committed count (line 54), so removing line 329 too would
   be equally correct; the author recommends keeping it, as the smaller
   change. Delete the `iLastValidatedIndex == 0` branch (296-299), and correct
   the function comment (265-268) and the branch comment (302-303) so they
   describe what the remaining branch covers.
2. `AdoptUnreachablePendingFullState` (A4): capture
   `int64_t iAdoptedTick = rPending.iTick;` first, then call
   `ReconcileInjectPendingFullState(rWork)`; keep the `iNewConfirmedTick`
   assignment (77), the update erase, the repeat-work guard clears, and the
   log (88-92). Drop everything else in lines 63-87: the `pFrame` `ASSERT`,
   the lines that repeat injection, the `iLastValidatedIndex` write, and the
   `outputLayout` write.
3. `RunReplay` and its calls in `ReconcileCoord` (A3, B1): remove the
   `rbShrunkRollback` parameter and its store at line 216, and remove the
   `iAdoptedTick` local (243) and the `riReplayStart` store (245); the
   adoption block becomes the adoption call followed by `return;`. Make the
   function return `void`: delete the fallback empty-range branch (250-259)
   and the final `return false;`, and in fallback mode add
   `ASSERT(iMaximumConsecutive >= riReplayStart)` where the branch was. In
   `ReconcileCoord`, drop `bShrunkRollback` from both calls (374, 378) and
   replace `if (RunReplay(...))` and its `return` at 378-381 with the bare
   call. `riRollbackOffset` stays a reference (read at 391), as does
   `riReplayStart` (read at 376); `rPreWritebackLayout` stays (line 215
   reads it). Rewrite the comment at 188-190 so it no longer says an empty
   fallback range resolves the coordinate.
4. `ReconcileForwardStepCoord` (B2): return `void`, capture the
   `ReconcileRunTickCoord` result, and `ASSERT` it. Delete the two
   `if`/`break` pairs in `ReconcileCatchUpCoord` and
   `ReconcileFastPathCatchUp`. `ReconcileRunTickCoord` keeps its `bool` (F6),
   and the replay caller at `Tick.cpp:235-238` is untouched.
5. `ReconcileReplayCoord` (B3): delete `Tick.cpp:222-225`. No `ASSERT` is
   added, because the proof is local.
6. `CrcFastPathProcessCoord` (B4): drop `&& !rCell.serverUpdates.empty()` at
   `Crc.cpp:202` and `248`, and delete lines 253-256.
7. `RR.h` (C1, C2, C3): delete the `ReconcileCoord` declaration and add
   `static` to its definition. Remove `iReplayStackCount` and its `Reset`
   line; replace each read with `std::ssize(rScratch.replayStack)` and each
   `replayStack.at(iReplayStackCount - 1)` with `replayStack.back()`; remove
   the writes at the reset sites and in `ReconcileRunTickCoord`. Remove
   `miActiveCount` and its two writes; set `result.iActiveCount = iActiveCount`
   in `ReconcileDispatcher::Run`; make `ClientReconciler.cpp:103` read the
   dispatch result's `iActiveCount`. The LOG calls at `RR.cpp:518` and `522`
   keep their text byte-for-byte: the harness resimulation check matches
   that line.
8. Shared search and reset (D1, D2): declare `FindSnapshotIndex` once in
   `RR.h`, taking `(const engine::Cell& rCell, int64_t iTick)`, since both
   callers pass the same three cell fields; call it from
   `DetermineRollbackBase` in place of the inline loop, keeping the
   `iShrunkIndex >= 0` handling that follows. Add one `CoordScratch` member
   beside `Reset` that holds exactly the four remaining statements, taking
   the base frame pointer and the base slot; it needs no `Cell`, because
   `CoordScratch` owns all four fields. Call it from the three remaining
   sites. Leave every `iLastValidatedIndex` and `iInjectedBaseSlot` write at
   its current site, outside the helper.

The two `while` loops in `ReconcileCatchUpCoord` and
`ReconcileFastPathCatchUp` are textually identical, but their budgets, start
ticks, and commit differ, and the two functions are deliberately parallel
(root `AGENTS.md` Directives, "Mirrored patterns stay parallel"). The author
recommends not sharing them.

`Projects/Coretide` is a private game project that is absent from a session
worktree, and game reconcilers read `CoordScratch`. The executing session
searches `Projects/Coretide` in the primary checkout for `iReplayStackCount`,
`miActiveCount`, `ReconcileCoord`, `FindSnapshotIndex`, and the new member's
name, and reports what it finds before it removes or adds them. Every game
project's readers change in the same landing.

## Critical files

- `Engine/Source/Network/Client/ReconcileReplay.h`
- `Engine/Source/Network/Client/ReconcileReplay.cpp`
- `Engine/Source/Network/Client/ReconcileReplayTick.cpp`
- `Engine/Source/Network/Client/ReconcileReplayCrc.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.cpp`
- `ReconcileReplayClientState.cpp` in that same game directory

## In scope

In `## Design` order:

1. `ComputeOutputLayout`: the `std::min` line, the `iLastValidatedIndex == 0`
   branch, the function comment, and the comment in the
   `iNewConfirmedTick >= 0` branch.
2. `AdoptUnreachablePendingFullState`: the `iAdoptedTick` capture, the call
   to `ReconcileInjectPendingFullState`, the lines that repeat injection, the
   `pFrame` `ASSERT`, the `iLastValidatedIndex` write, and the `outputLayout`
   write.
3. `RunReplay`: the `rbShrunkRollback` parameter and its store, the
   `iAdoptedTick` local and `riReplayStart` store around the adoption call,
   the fallback empty-range branch, the return type and `return` statements,
   the new fallback `ASSERT`, and the comment above `enum class ReplayMode`.
   `ReconcileCoord`: the two `RunReplay` calls' argument lists and the
   handling of the fallback result.
4. `ReconcileForwardStepCoord`: return type, the result test, and the new
   `ASSERT`; in `ReconcileCatchUpCoord` and `ReconcileFastPathCatchUp`, the
   `if`/`break` around the forward step.
5. `ReconcileReplayCoord`: the `it == end` break.
6. `CrcFastPathProcessCoord`: the two `!rCell.serverUpdates.empty()` operands
   and the early return at lines 253-256.
7. The `ReconcileCoord` declaration and the linkage of its definition;
   `CoordScratch::iReplayStackCount`: the member, its `Reset` line, and every
   write and read listed in C2; `ReconcileDispatcher::miActiveCount`: the
   member, its two writes, the `result.iActiveCount` assignment, and the one
   game read.
8. `FindSnapshotIndex`: its signature, linkage, one declaration, and its call
   in `CrcValidateLoop`; the inline search loop in `DetermineRollbackBase`;
   one new `CoordScratch` member holding the four statements, and the
   statements at the three sites that remain.

## Out of scope

- `ReconcileInjectPendingFullState` beyond its replay-base reset statements,
  and `iInjectedBaseSlot`.
- Every `iLastValidatedIndex` write outside `AdoptUnreachablePendingFullState`.
- `DetermineRollbackBase` beyond its search loop, and its `rbShrunkRollback`
  output, which `ReconcileCoord` still reads.
- `ReconcileScratchFlags::kShrunkRollback`, the "Reconcile post-replay" log
  lines beyond their `iReplayStackCount` argument, and merging the two LOG
  calls at `RR.cpp:516-523`.
- `ApplyCoordWriteback` and its `ASSERT`.
- `ReconcileRunTickCoord`, its ring-full test, and the `break` at
  `Tick.cpp:235-238`.
- The gap early return at `Crc.cpp:248-251` apart from the one operand, and
  every `bHandled` outcome.
- The catch-up `while` loops, the flag-clear loops after them, and the
  budgets.
- `CoordScratch::Reset` beyond the one removed line, and every existing
  `CoordScratch` member other than `iReplayStackCount`.
- `ReconcileInputs` and the signature of `ReconcileDispatcher::Run`.
- The `DEBUG_BREAK` repeat-work guard in `ReconcileCoord`.
- The visibility of `ClientDesyncCore::RecoverFromDesync` and of
  `ClientSessionRuntime::PollDiscovery`, `SetDesiredCoordinates`, and
  `ResetClock`: no rule asks for a change, and both classes expose public
  data members.
- Gating the reconcile dispatch on `kbFrameDispatch`: nothing in the
  repository states what that switch is meant to cover.
- Whether a desync return without writeback leaves `iHighWaterValidatedTick`
  and erased updates consistent for the next reconcile. It was seen beside
  proof B1, was not traced, and does not affect the proof.
- `Documents/Architecture/GameReconciliation.md`.

## Risk triggers and invariants

Risk tier: Tier 3 (the author's classification, the highest among the merged
sources). Trigger: the edits are on the determinism and CRC surface. The
client reconcile chain decides which frames a cell commits and compares
against server CRCs; the branch removals are safe only because of
reachability proofs that span files and rest on the single-writer fact F1;
the adoption change is safe only by the reasoning in A4 about what
`ComputeOutputLayout` later writes; and public members of an engine header
that game projects read are removed or added. The change is meant to compute
exactly what the code computes today. Threading is unchanged: each reconcile
worker still writes only its own `CoordWork`. No packet, save, `.pack`, or
`kiVersion` change. The executing session derives the tier again and
re-proves each claim against the code it finds.

Invariants:

- Every path commits the ring head, count, confirmed offset, and confirmed
  tick it commits today; after adoption these are identical to today's.
- `iAssumedFrameTicks` and the committed `iSnapshotCount` are identical.
- A reconcile worker writes only its own coordinate's state.
- The adoption log line and the "Reconcile post-replay" log lines keep their
  text and argument values.

## Acceptance criteria

1. The steps of `## Design` are in the diff: the four unreachable branches are
   gone; `RunReplay` and `ReconcileForwardStepCoord` return `void`; the two
   new `ASSERT`s exist; adoption calls injection after capturing
   `iAdoptedTick`; a search of `Engine` and `Projects/BrokenEngineSandbox`
   finds no `iReplayStackCount` and no `miActiveCount`; `RR.h` does not
   declare `ReconcileCoord`; and the snapshot search loop and the four
   replay-base reset statements each appear once in the four `Engine` files.
2. `/compile` builds the BrokenEngineSandbox client and server.
3. The replay determinism recipe in
   `Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md` shows no
   CRC mismatch, the same result as before the change.
4. The resimulation check in
   `Projects/BrokenEngineSandbox/Documents/AgentHarness/verification.md`
   (steps 1 to 7) passes as that document states it, with no `ASSERT` in the
   client log.

The supplied evidence records no harness setup for criteria 3 and 4, and none
that makes a client adopt a pending full state past an update gap. The
executing session sets criteria 3 and 4 up from the harness documentation; if
it finds no adoption setup, step 2 of `## Design` is verified by code reading
plus criteria 2 to 4.

## Coordination

- `Documents/Plans/Engine/ClearWindTrailCacheAfterFullState.md` adds a signal
  to the injection and adoption functions. Once adoption calls injection, a
  signal set in injection covers both. Neither Plan depends on the other;
  whichever lands second re-derives its regions by symbol from the landed
  file.
