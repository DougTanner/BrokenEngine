# Resolve the Mode 6 and Mode 7 Random-Draw Rule Against the Code

This is an Investigation, not a Plan: it becomes a Plan under
`Documents/Plans/Game/` once the user chooses one of the two options under
`### Open choice for the user`.

## Context

`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/AGENTS.md`
`## Fleet Navigation Invariants` states a rule the code does not follow. Paths
below are relative to `Projects/BrokenEngineSandbox/Source/Frame/Collections`
unless they start at the repository root; line numbers are as of commit
`fe1fb69b`.

The document side:

- `Ships/AGENTS.md:304-306`: modes 6 and 7 "must consume the same number of
  random draws on any tick they can flip between", so mode 7 mirrors mode 6's
  three entry draws on every tick it runs.
- `Ships/AGENTS.md:312-315`: only the draw count matters, so mode 7's mirror
  draws are "load-bearing, not dead code".
- The comment at `Ships/ShipBehaviours.cpp:604-606` repeats the mirror claim.

The code side, all in `ComputeNavigation` in `Ships/ShipBehaviours.cpp`:

- The mode is settled before any draw: `UpdateFleetAndFlagshipNavigation`
  (`:595`) may change it, and the draws follow the mode it leaves (`:600`,
  `:622`).
- Mode 7 draws three values every tick in a cell with a centred island
  (`:607-615`) and throws them away.
- Mode 6 without a patrol flagship draws three values only when the stored
  destination's W is zero (`:647`, `:696-708`), and none otherwise.
- Roaming (mode -1) and the cross-cell modes draw none (`:757-786`).
- In a cell with no centred island neither mode draws (`:608`, `:689-693`).
- `FollowFlagship` sets mode 7 whenever the flagship is beyond
  `kfFlagshipFollowDistanceSquared`, without testing the entry mode
  (`:274-277`), and returns to mode 6 with a zeroed destination when it is
  close (`:279-286`).

Flip by flip, against the same row staying in its entry mode:

| Flip | Draws after the flip | Draws had it stayed | Equal |
|---|---|---|---|
| 7 to 6 (destination zeroed at `:284-285`) | 3 | 3 | yes |
| 6 with no stored destination to 7 | 3 | 3 | yes |
| 6 with a stored destination to 7 | 3 | 0 | no |
| Roaming or a cross-cell mode to 7 | 3 | 0 | no |

So the rule holds on two of the four flips. The analysis found nothing that
depends on equal counts:

- The flip inputs are previous-frame shared state (positions,
  `pFlagshipGlobalIds`, flags), so both builds take the same branch.
- The random stream is frame state: copied from the previous frame each tick
  (`Engine/Source/Frame/FrameBase.cpp:222`) and part of the frame CRC (`:79`
  there), so a re-simulated tick restarts from the same stream state.
- `Common/Math/AGENTS.md:41-43` requires draws to stay out of throttled or
  client-only branches. It does not require equal counts across modes.

On that evidence the code is deterministic as it stands, subject to the
untraced loop order recorded at the end of this section. The unsupported part
is the documented rule and the "load-bearing" claim.

Originating gap: an accepted architecture-review finding (simulation lens) from
a deep analysis of
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships`. It is
pre-existing and is not an unmet acceptance criterion of any change.

Not traced by the analysis: whether the ship Update loops that draw from the
cell's `rFrame.postRender.randomEngine` run one after another. The analysis
assumed it from the stream being CRC'd frame state.

## Design

### Open choice for the user

Two corrections remove the contradiction. The user was shown both and had not
chosen when this document was written. This investigation chooses neither. The
session that takes it up presents both options to the user, with the author's
recommendation below marked as a recommendation, and the Plan written from it
implements only the one the user picks.

Author's recommendation: option 2. Rationale: it removes the cause, three
discarded draws and the rule written to justify them, where option 1 keeps the
draws and only stops calling them required. The cost is that option 2 changes
simulation results and is Tier 3, where option 1 is documentation-only.

### Option 1 — correct the document and the comment, keep the draws

- Rewrite the `Ships/AGENTS.md` `## Fleet Navigation Invariants` bullet at
  `:303-315` to what the code does: mode 7 draws three values on every tick in
  a cell with a centred island and discards them, mode 6 draws three only on a
  tick it has no stored destination, and the counts are equal on the two flips
  the table above marks equal and unequal on the other two. Remove "must
  consume the same number" and "load-bearing, not dead code". Keep the bullet's
  other statements (the throttle sentence, the no-centred-island case, and the
  fleet Omni draw counts).
- Rewrite the comment at `Ships/ShipBehaviours.cpp:604-606` so it no longer
  presents the three draws as a required mirror.
- No code statement changes, so simulation results, CRC values, and
  `Frame::kiVersion` stay as they are.

### Option 2 — remove the mirror draws and the rule

- First confirm nothing depends on the draws: trace that the ship Update loops
  drawing from `rFrame.postRender.randomEngine` run one after another, and
  search for any reader that relies on mode 7 advancing the stream. If a
  dependency is found, stop and return to the user with that evidence; do not
  widen the change.
- Remove the mode 7 mirror block at `Ships/ShipBehaviours.cpp:607-615` (the
  `centredIslands` local and the three discarded `common::Random` calls) and
  the mirror wording in the comment at `:604-606`.
- In the `Ships/AGENTS.md` bullet at `:303-315`, remove the equal-count rule,
  the mirror sentence, and the "load-bearing" sentence, and reword what refers
  to them (the no-centred-island sentence and the fleet Omni clause about
  flipping between the modes). Keep the throttle sentence and the draw counts
  that remain true.
- Removing three draws shifts every later draw in the cell on a tick a row is
  in mode 7, so computed frame CRCs change without a collection version bump.
  Bump the `Frame::kiVersion` base at
  `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:114`, as
  `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` requires ("Bump its
  base when CRC semantics change without a contributing version bump"). Older
  saves and replays are then rejected.

### Risk tier

Tier 3 (the author's classification), the highest either option reaches.
Trigger: option 2 changes determinism and CRC results and save/replay
compatibility. Option 1 alone is Tier 1 (documentation and a comment). The
change is classified again once the user has picked.

Exposure for option 2: PostRender simulation code compiled by both
executables; random-stream consumption; frame CRC; save and replay gating
through `Frame::kiVersion`. No layout, wire, or serialization format change.

## What a Plan from this would touch

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp` (option 2 only)

## Proposed scope

- `Ships/AGENTS.md` `## Fleet Navigation Invariants`: the bullet that begins
  "Random draws, arrival checks, and mode transitions".
- `ComputeNavigation`, mode 7 branch: the comment above the mirror block, and
  under option 2 the mirror block itself.
- Under option 2 only: the base literal of `Frame::kiVersion`.

## Proposed out of scope

- Mode 6's own draws, the fleet Omni patrol picks, `FollowFlagship`'s flip
  conditions, and any other draw site.
- Making the counts equal by adding draws.
- Any other bullet of `## Fleet Navigation Invariants`, and
  `Common/Math/AGENTS.md`.
- Implementing either option before the user has picked one.

## Proposed acceptance

Under option 1:

1. The bullet and the comment state only what the table in `## Context` shows,
   with no "must", "mirror", or "load-bearing" claim about equal counts.
2. The diff changes no code statement.

Under option 2:

1. The trace in `### Option 2` is reported with its evidence before any edit.
2. No `common::Random` call remains in the mode 7 branch of
   `ComputeNavigation`, and neither the bullet nor the comment states the
   equal-count rule or the mirror.
3. The `Frame::kiVersion` base is higher than at the session baseline.
4. `/compile` builds the BrokenEngineSandbox client and server.
5. A live `/agent-harness` session with fleet ships following a flagship runs
   with no client/server CRC mismatch, and the replay determinism check passes.
   The originating evidence records no harness setup for this state.

## Related

`Documents/Plans/Game/SplitOmnisAndShipBehaviours.md` may move
`ComputeNavigation` out of `ShipBehaviours.cpp`. A Plan written from this
investigation after that lands re-derives its sites from the landed source.

A Plan written from
`Documents/Investigations/Engine/ResolveShipGlobalIdCrcExclusion.md` under its
own option 1 also edits `Ships/AGENTS.md`, and under its own option 2 bumps the
same `Frame::kiVersion` base. Whichever Plan lands second re-derives its base
value and its document edit from the landed source.

## Notes

- The equal-count sentence was checked against every statement of the rule in
  the tree at commit `fe1fb69b`: it appears only in `Ships/AGENTS.md:303-315`
  and the comment at `Ships/ShipBehaviours.cpp:604-606`.
- Do not add unit tests.
