# Resolve the Ship Global ID's Exclusion From the Shared CRC

This is an Investigation, not a Plan. It is the options record for a choice
the user has since made: `### Open choice for the user` records that choice,
and option 2's tuple and comment change has landed, so `pGlobalIds` is in
each ship PostRender `SharedCrcMembers()`. The three ship PostRender collection
version bumps took the place of that option's `Frame::kiVersion` base bump.

## Context

The three ship collections read `pGlobalIds` to make simulation decisions and
leave the column out of the per-tick CRC. Paths below are relative to
`Projects/BrokenEngineSandbox/Source/Frame/Collections` unless they start at
the repository root; line numbers are as of commit `fe1fb69b`, and the comment
text quoted below is as of commit `7dae9a27`.

Simulation reads of the column:

- Path cadence: `Ships/ShipBehaviours.cpp:343-344`, with the ID read at `:571`.
- Patrol Together leader: `Ships/ShipBehaviours.cpp:243` and `:252`.
- Escort slot order: `Ships/FleetCoordination.cpp:144`.
- Fighter squadron leader and kept-target holder:
  `Ships/FleetCoordination.cpp:183` and `:187`.
- Flagship lookup: `Ships/ShipQueries.cpp:18`.

The exclusion: `pGlobalIds` is in `SharedMembers()` and absent from
`SharedCrcMembers()` in all three types.

| Type | `SharedMembers()` | `SharedCrcMembers()` | Comment |
|---|---|---|---|
| Fighters | `Fighters/Fighters.h:275` | `:297-300` | `:296` |
| Battleships | `Battleships/Battleships.h:263` | `:285-288` | `:284` |
| Omnis | `Omnis/Omnis.h:323` | `:345-348` | `:344` |

At `fe1fb69b` each comment said the subset excludes `pGlobalIds` and
`pClientGuids`, "which are server-side bookkeeping", which was false for
`pGlobalIds`. Commit `7dae9a27` rewrote all three. Each now reads "CRC-only
subset: omits pClientGuids, which is server-local, and pGlobalIds, which the
tick reads but Spawn sets identically on client and server". So the code now
says the column is a simulation input and gives a reason for leaving it out:
both builds hold the same value by construction. That reason agrees with the
writer trace below, apart from the one path this section ends on as not traced.

What the analysis found about the reason:

- No `AGENTS.md` gives one; the comment quoted above is the only place a
  reason is written. `Omnis/AGENTS.md:89-91` (`## State Boundaries`, as of
  commit `d87ad840`) says the shared CRC excludes "server-assigned global
  identity, which simulation reads" and gives no reason for leaving it out.
  `Ships/AGENTS.md:278-279` and `:289-290` document the reads themselves.
- The client-GUID column has no post-tick writer: the server's `Spawn` writes
  it from the server-local spawn payload, and client rows created from a spawn
  or transfer status change hold zero. `pGlobalIds` has no post-tick writer
  either.
- `Engine/Source/Frame/AGENTS.md` `## Simulation and Visual State` lets a
  declared `SharedCrcMembers()` narrow the CRC and gives no criterion for what
  may be left out.

Whether the two builds can hold different values: no path was found.

- A client row gets the value from a full frame through `ServerRead`, which
  reads `SharedMembers()`, and from spawn and transfer status changes, whose
  `SpawnInfo` payload the three `Spawn` overloads store
  (`Fighters/Fighters.cpp:531`, `Omnis/Omnis.cpp:327`,
  `Battleships/Battleships.cpp:382`).
- The only writers are those three `Spawn` overloads and the server
  `edit_frame` agent command, whose column lists name `pGlobalIds`
  (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp:21-23`).

So no live desync was found. A mismatch, if a future change introduced one,
would not fail the CRC on its own tick; it would surface later as a position or
target mismatch away from its cause. `LogDifferences` already compares the
column (`Fighters/Fighters.cpp:787`, `Omnis/Omnis.cpp:843`,
`Battleships/Battleships.cpp:599`), so a later mismatch report already names it.

Originating gap: an accepted architecture-review finding (simulation and
client/server lenses, one root cause) from a deep analysis of
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships`. It is
pre-existing and is not an unmet acceptance criterion of any change.

Not traced by the analysis: whether a server `edit_frame` write of `pGlobalIds`
reaches clients.
`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:97`
documents that write as unchecked apart from the fleet-controlled ship rule.

## Design

### Open choice for the user

The comments no longer say anything false, so what was left to choose was
whether the exclusion stays or ends. The user was shown an earlier form of the
two options, in which option 1 also rewrote the three comments, before commit
`7dae9a27` made that rewrite separately, and chose nothing then. This
investigation chooses neither. The authoring agent's record of what the user
chose afterwards, on 2026-11-14, when the session that authored the global-ID
Plan asked: `pGlobalIds` goes into `SharedCrcMembers()` for Fighters,
Battleships, and Omnis, which is option 2 below. Its tuple and comment change
has landed, with the three ship PostRender collection version bumps in place
of its `Frame::kiVersion` base bump, so no further Plan is written from this
investigation, which stays as the record of the options.

Author's recommendation: option 2. Rationale: it would catch a client/server
`pGlobalIds` mismatch on its own tick instead of ticks later through downstream
state, where option 1 keeps the exclusion and only adds an `AGENTS.md` record
of it. The cost is that option 2 changes every tick's CRC value and is Tier 3,
where option 1 is documentation-only. This is the author's recommendation only,
and the rewritten comments make the case for option 2 weaker than when it was
first recommended.

The rewritten comments change how much each option has going for it, and the
user weighs that. The false statement that first called for a correction is
gone, and the code now gives a reason for the exclusion. Option 2 therefore
rests only on catching a future mismatch on its own tick, and option 1 only on
whether that reason should also be recorded in `Ships/AGENTS.md`.

### Option 1 — keep the exclusion and document it in `Ships/AGENTS.md`

- The three `SharedCrcMembers()` comments already name `pGlobalIds` as a
  simulation input and give the reason it is left out; they stay as they are.
- Record the deliberate exclusion once in `Ships/AGENTS.md`
  `## Ship Invariants`, at the bullet that begins "Stable global IDs provide
  lookup and transfer identity" (`:75`): the column decides the reads listed
  in `## Context`, is outside the shared CRC, both builds receive it through
  the shared format and the spawn payload, and `LogDifferences` compares it.
- No code statement changes, so CRC values and `Frame::kiVersion` stay as they
  are.

### Option 2 — add the column to the shared CRC

- First trace whether a server `edit_frame` write of `pGlobalIds` reaches
  clients. If it does not, including the column would turn that write into a
  CRC mismatch; stop and return to the user with that evidence, and do not
  widen the change.
- Add `rSelf.pGlobalIds` to the three `SharedCrcMembers()` tuples
  (`Fighters/Fighters.h:299`, `Battleships/Battleships.h:287`,
  `Omnis/Omnis.h:347`) and rewrite the three comments above them to name
  `pClientGuids` as the only exclusion.
- Reword `Omnis/AGENTS.md` `## State Boundaries`, first bullet, which lists
  "server-assigned global identity" among what the shared CRC excludes.
- Layout and wire bytes do not change; computed CRCs do, with no collection
  version bump. Bump the `Frame::kiVersion` base at
  `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:114`, as
  `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` requires ("Bump its
  base when CRC semantics change without a contributing version bump"). Older
  saves and replays are then rejected.

### Risk tier

Tier 3 (the author's classification), the highest either option reaches.
Trigger: option 2 changes what the determinism CRC covers and save/replay
compatibility. Option 1 alone is Tier 1 (documentation only). The change is
classified again once the user has picked.

Exposure for option 2: the per-tick frame CRC both executables compute; save
and replay gating through `Frame::kiVersion`; the server `edit_frame` agent
command. No layout, wire, or serialization format change.

## What a Plan from this would touch

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.h`
  (option 2 only)
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.h`
  (option 2 only)
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.h`
  (option 2 only)
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/AGENTS.md`
  (option 1 only)
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/AGENTS.md`
  (option 2 only)
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp` (option 2 only)

## Proposed scope

- Under option 1 only: the `Ships/AGENTS.md` `## Ship Invariants` bullet named
  above.
- Under option 2 only: `FightersPostRender::SharedCrcMembers`,
  `BattleshipsPostRender::SharedCrcMembers`, and
  `OmnisPostRender::SharedCrcMembers`, each tuple and the comment above it;
  the first bullet of `Omnis/AGENTS.md` `## State Boundaries`; and the base
  literal of `Frame::kiVersion`.

## Proposed out of scope

- `pClientGuids` and its exclusion.
- `SharedMembers()`, `PersistentMembers()`, `LogDifferences`, the `Spawn`
  overloads, and every read of `pGlobalIds`.
- The `edit_frame` command's behavior and its `pGlobalIds` admission rule.
- Implementing either option before the user has picked one.

## Proposed acceptance

Under option 1:

1. `Ships/AGENTS.md` states the exclusion once as deliberate, with the reason
   the three `SharedCrcMembers()` comments give.
2. The diff changes no source file.

Under option 2:

1. The `edit_frame` trace in `### Option 2` is reported with its evidence
   before any edit.
2. `pGlobalIds` is in all three `SharedCrcMembers()` tuples, and no comment or
   `AGENTS.md` sentence says the shared CRC excludes it.
3. The `Frame::kiVersion` base is higher than at the session baseline.
4. `/compile` builds the BrokenEngineSandbox client and server.
5. A live `/agent-harness` client/server session that spawns and transfers
   ships of all three types runs with no CRC mismatch, and the replay
   determinism check passes. The originating evidence records no harness setup
   for this state.

## Related

A Plan written from
`Documents/Investigations/Engine/ResolveModeSixSevenRandomDrawRule.md` also
edits `Ships/AGENTS.md`, which this investigation edits under its option 1
only, and under its own option 2 bumps the same `Frame::kiVersion` base.
Whichever Plan lands second re-derives its base value and its document edit
from the landed source.

## Notes

- Do not add unit tests.
