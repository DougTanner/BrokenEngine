<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-12T18:50:20.823Z","dependsOn":[]} -->
# Stop DestroyElement adjusting the caller's loop index, so reverse walks stop skipping an element

## Context

`engine::DestroyElement` (`Engine/Source/Frame/Collections/CollectionLifecycle.h:139-150`) removes a row by
swapping the last row into index `i`, and then — when the removed row was not already the last row — also
decrements the caller's loop index:

```cpp
if (rInterpolate.iCount - 1 > i) [[likely]]
{
	SwapElement(rInterpolate, i, ...);
	SwapElement(rPostRender, i, ...);
	--i;
}
```

That `--i` is a *forward*-loop idiom: in `for (i = 0; i < iCount; ++i)` it cancels against `++i` so the
swapped-in row is visited next, which is required for correctness.

Six of the nine call sites are *reverse* walks, `for (int64_t i = rInterpolate.iCount - 1; i >= 0; --i)`,
where the swapped-in row came from the tail and has already been visited. There the helper's `--i` combines
with the loop's own `--i`, so index `i - 1` — which has *not* been visited — is skipped entirely whenever the
removed row was not the last row:

- Cross-cell transfer loops: `Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/Blasters.cpp:214`
  (loop head `:175`), `Missiles/Missiles.cpp:387` (head `:339`), `Spaceships/Spaceships.cpp:437` (head `:384`).
- Destroy sweeps, which now run through `engine::DestroySweep`
  (`Engine/Source/Frame/FrameUtils.h:276-291`, whose reverse walk owns the same `--i` contract in its comment):
  `Blasters.cpp:232`, `Missiles.cpp:408`, `Spaceships.cpp:455`.

The three remaining call sites are forward walks and are correct today: `Explosions.cpp:296` (head `:241`),
plus `Puffs/Puffs.cpp:26` and `WindRadials/WindRadials.cpp:26`, which run inside
`DestroyExpiredControlled`'s forward loop (`Engine/Source/Frame/Collections/CollectionController.h:153-155`).
`Players/Players.cpp:228` is also correct: its sweep removes through `engine::RemoveIndexableElement`, which
never touches the loop index.

Consequence: a flagged element that lands on a skipped index is not destroyed or transferred in that pass. It
is caught by a later pass, so the visible effect is a deferred destroy or a deferred cross-cell transfer, not
a leak. Client and server run identical code, so the two sides agree and this is not a desync source — which
is precisely why it has survived: nothing in the CRC comparison flags it.

Pre-existing and out of scope where it was found. The session that produced
`Documents/Plans/Frame/FrameUtilsSharedHelpers.md` moved these loops into `engine::DestroySweep` under an
explicit zero-behavior-change boundary (`## Out of scope`: "Any change to fold order, walk direction, byte
stream ..."), and both that session's preparation and its `/plan-audit` confirmed the defect while preserving
it deliberately. Fixing it changes the deterministic simulation stream, so it needs its own change.

## Design

Make `engine::DestroyElement` a pure removal that never touches the caller's iteration state, and move the
forward-walk adjustment to the three forward-walk callers that actually need it.

- `CollectionLifecycle.h:139-150`: change the parameter from `int64_t& i` to `int64_t i`, delete the `--i`,
  and rewrite the comment above it (`:137-138`) so it no longer promises loop-index adjustment. The swap
  condition, the swap order across the two collections, and both count decrements are unchanged.
- `Engine/Source/Frame/FrameUtils.h:276-291` (`DestroySweep`): its comment currently explains that the release
  callable takes the index by reference because `DestroyElement` decrements it. Rewrite that comment. The
  `TRelease` callable's parameter becomes `int64_t` by value, and the three sweep lambdas
  (`Blasters.cpp:227-233`, `Missiles.cpp:403-409`, `Spaceships.cpp:450-456`) drop the `&` from `int64_t& ri`.
  The loop body itself is unchanged.
- Forward-walk callers add the adjustment themselves, unconditionally: `Explosions.cpp:296` becomes the
  `DestroyElement(...)` call followed by `--i;` inside its own loop, and the two `DestroyExpiredControlled`
  removal lambdas (`Puffs.cpp:23-27`, `WindRadials.cpp:23-27`) do the same, keeping their `int64_t& i`
  parameter, which `DestroyExpiredControlled` already passes by reference. Unconditional `--i` is correct even
  when the removed row was the last one: `iCount` has already been decremented to the pre-decrement value of
  `i`, so the following `++i` restores `i` and the loop's `i < iCount` test ends the pass — the same place it
  would have ended before.
- The three reverse-walk transfer loops (`Blasters.cpp:214`, `Missiles.cpp:387`, `Spaceships.cpp:437`) and the
  three sweep lambdas need no body change beyond the by-value parameter; they simply stop losing an index.

Version handling decision: this changes the tick at which some elements are destroyed or transferred, so
post-change CRCs, saves, and replays do not match pre-change ones. Bump the base constant in
`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:14` from `123` to `124`, so `game::Frame::kiVersion`
changes and `GameSaveLoad.cpp:1183` rejects every pre-change grid save and replay generation with the existing
version-mismatch error instead of replaying them into a checksum mismatch. No compatibility path is added and
no collection `kiVersion` changes, because no serialized layout changes.

## Critical files

- `Engine/Source/Frame/Collections/CollectionLifecycle.h` — `DestroyElement` signature, body, comment
- `Engine/Source/Frame/FrameUtils.h` — `DestroySweep` comment and release-callable parameter contract
- `Engine/Source/Frame/Collections/Explosions/Explosions.cpp`,
  `Engine/Source/Frame/Collections/Puffs/Puffs.cpp`,
  `Engine/Source/Frame/Collections/WindRadials/WindRadials.cpp` — forward-walk callers gaining `--i`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/Blasters.cpp`,
  `Missiles/Missiles.cpp`, `Spaceships/Spaceships.cpp` — reverse-walk transfer loops and sweep lambdas
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp` — `kiVersion` base constant

## In scope

- `DestroyElement`'s parameter, the removal of its `--i`, and its comment
- `DestroySweep`'s release-callable parameter and comment, and the three sweep lambdas' parameter
- Adding `--i` to the three forward-walk call sites named in Design
- The `123` → `124` bump at `Frame.cpp:14`
- Any `Engine/Source/Frame/Collections/AGENTS.md` or `Engine/Source/Frame/AGENTS.md` sentence that states the
  loop-index-adjustment contract

## Out of scope

- `RemoveIndexableElement`, `RemoveIndexableElementAndClearHandle`, and the Players sweep, which never adjust
  a loop index
- Changing walk direction, predicate ordering, swap-and-pop strategy, or any serialized layout
- Any collection-level `kiVersion`, wire format, or save/replay backward-compatibility path
- The transfer-request payload types and the harvest loop shape themselves

## Risk tier and invariants

Tier 3 — determinism/CRC and save/replay compatibility are directly exposed: the change alters the tick on
which some elements are removed, so every per-tick CRC downstream of a skipped index changes, and pre-change
saves and replays become unreadable by design. Invariants to hold: client and server must remain
bit-identical, so no call site may be fixed on one build only; every reverse walk must still visit every
index exactly once; every forward walk must still revisit the swapped-in row exactly once; paired Interpolate
and PostRender counts stay equal after every removal.

## Acceptance criteria

- Client and server compile.
- A harness replay determinism run records and replays cleanly with matching per-tick CRCs on the new code.
- A save or replay written before the change is rejected with the existing version-mismatch error rather than
  producing a checksum mismatch.
- A runtime scenario that destroys or transfers more than one element in a single pass shows every flagged
  element handled in that same tick — for example, a multi-blaster or multi-missile cross-cell transfer whose
  source-cell count reaches zero in one pass instead of over successive ticks.
