<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:09:01.881Z","dependsOn":[]} -->
# Put the ship global ID column in the shared CRC

## Context

Paths are under `Projects/BrokenEngineSandbox/Source/` unless they start at
the repository root; `FC/` stands for `Frame/Collections/`. Line numbers are
from the commit that added this Plan; locate each site by symbol.

Originating gap: the architecture phase of a deep analysis of
`FC/Fighters` (its directory only), verified by a second reader. It is not
an unmet acceptance criterion of any change. The false condition: every column
a simulation decision reads is in the CRC. When the analysis ran, the
exclusion also had no documented reason; it has one now, described below.

Each ship type's PostRender `SharedCrcMembers()` leaves out `pGlobalIds` and
`pClientGuids` under one comment (`FC/Fighters/Fighters.h:298-301`,
`FC/Battleships/Battleships.h:286-289`, `FC/Omnis/Omnis.h:349-352`): "CRC-only
subset: omits pClientGuids, which is server-local, and pGlobalIds, which the
tick reads but Spawn sets identically on client and server". Commit `7dae9a27`
wrote that wording; before it the comment called both columns "server-side
bookkeeping". The current comment states the reason for the `pGlobalIds`
exclusion this Plan removes. Its two facts about `pGlobalIds` are true, and
they are the case for the CRC, not against it: a column the tick reads, and
that is meant to be equal on both sides, is what the CRC exists to verify.

- It is a simulation input. Readers: `FC/Fighters/Fighters.cpp:628`
  (`ChooseFighterFleetTactic`); `FC/Ships/FleetCoordination.cpp:145` (escort
  slot), `:184`, `:188` (shared-target leader and kept-target holder), `:225`
  (`FleetBoard` takes the flagship ID from Battleships `pGlobalIds`);
  `FC/Ships/ShipBehaviours.cpp:344` (path-recompute cadence
  `(iTick + globalId) % kiNavigationRecomputeInterval`, all three types),
  `:243`, `:252` (Omni Patrol Together leader); `FC/Ships/ShipQueries.cpp:20`
  (`FindShipByGlobalId`).
- The client reads it too (`Ui/Screens/HudScreen.cpp:71`).

Client values equal the server's by construction, so nothing legitimate makes
the two sides differ: the column is in `SharedMembers()` (`Fighters.h:277`,
read by `ServerRead`) and `PersistentMembers()` (`:295`); `globalId` is in
`SpawnInfo::Fields()` (`:330`), the list
`Engine/Source/Network/StatusChangeFields.h` serializes; the only simulation
write is `Spawn` (`Fighters.cpp:528`); and `RegistryOwnershipLayer` holds the
column `const` (`Engine/Source/Frame/FrameRegistry.h:52`).

Authority: `Engine/Source/Frame/AGENTS.md` `## Simulation and Visual State`
says simulation state is "CRC'd through `SharedCrcMembers()` where a
collection declares one", and its `## Frame Registry` names only the
client-GUID column as CRC-excluded. `FC/Omnis/AGENTS.md` `## State Boundaries`
states that the shared CRC excludes "server-assigned global identity, which
simulation reads" (commit `d87ad840` added the last three words) but gives no
reason; no other `AGENTS.md` mentions the exclusion.
`git log -S"server-side bookkeeping"` reaches the initial commit `e571f6f1`,
so the exclusion itself is inherited, not a recorded decision; only the
comment's reason is new.

Effect today: a wrong global ID (a wire, spawn-payload, or agent `edit_frame`
bug) is not caught by the CRC and shows up later as a position or target
mismatch. No such bug was observed.

`pClientGuids` is different and correctly excluded: the server rewrites it
after the tick through `AssignRegistryClientGuid`, and client rows created
from a spawn or transfer status change hold zero.

## Design

The authoring agent's record of what the user chose in the session that
authored this Plan: `pGlobalIds` goes into the shared CRC of all three ship
types, with the version bump. Keeping the column out and correcting only the
comments and documents was the alternative, and it was not chosen.

Contradiction, named so it is not resolved silently: commit `7dae9a27`, a
parallel change, carried out the comment half of that alternative, so the
current comment records the opposite position (the column stays out because
both sides set it identically). Under the authority order in root `AGENTS.md`
`### Diagnosis Discipline` the recorded user choice outranks a code comment, so
this Plan trusts the choice and rewrites the comment.

Why the column belongs there: it is a simulation input with no legitimate
client/server difference, which is what the engine rule above puts in the CRC.
"Equal by construction" describes every CRC'd column and so is no reason to
leave one out. Cost: a CRC-semantics change, so saves and replays written
before it are rejected.

The steps are the author's recommended way to carry that choice out:

1. Add `rSelf.pGlobalIds` to the PostRender `SharedCrcMembers()` tuple of
   `FightersPostRender`, `BattleshipsPostRender`, and `OmnisPostRender`, at its
   `SharedMembers()` position (before `pFlagshipGlobalIds`).
2. In the comment above each of the three tuples, delete the `pGlobalIds`
   clause ("and pGlobalIds, which the tick reads but Spawn sets identically on
   client and server"), which the column's inclusion makes false. The
   `pClientGuids` clause and its reason ("server-local") stay, so the comment
   names `pClientGuids` as the one omitted column.
3. Bump the base term of `Frame::kiVersion` (`Frame/Frame.cpp`), as
   `Frame/AGENTS.md` `## Invariants` requires when CRC semantics change without
   a contributing version bump. No layout, wire, or save-layout byte changes,
   so `FrameInput::kiVersion` and `engine::kiProtocolVersion` stay.
4. Documentation through `/update-claude-docs`: `FC/Omnis/AGENTS.md`
   `## State Boundaries` stops listing global identity as CRC-excluded, its
   "which simulation reads" clause included, and names the client GUID
   instead.

Invariants: the three ship types change together; `SharedCrcMembers()` stays a
subset of `SharedMembers()`; `pClientGuids` stays out of the CRC;
`LogDifferences` already compares `pGlobalIds` and is unchanged.

Risk tier: Tier 3 — invariant/integration. Trigger: determinism/CRC (the set
of CRC'd columns changes) and save/replay compatibility (the version bump).

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/AGENTS.md`

## In scope

- The PostRender `SharedCrcMembers()` tuple and the comment above it in
  `FightersPostRender`, `BattleshipsPostRender`, and `OmnisPostRender`.
- The base term of `Frame::kiVersion` in `Frame/Frame.cpp`.
- The CRC-exclusion bullet of `FC/Omnis/AGENTS.md` `## State Boundaries`, and
  any other `AGENTS.md` sentence `/update-claude-docs` finds that the change
  makes false.

## Out of scope

- `pClientGuids`: its CRC exclusion, its `SharedMembers()` membership, and
  whether sending every owner's client GUID to subscribed clients is wanted.
- The Interpolate `SharedCrcMembers()` of every type.
- `LogDifferences` in every type.
- `SharedMembers()`, `PersistentMembers()`, `SpawnInfo`, and every reader of
  `pGlobalIds`: no layout or behavior change.
- The three collection `kiVersion` values, `FrameInput::kiVersion`, and
  `engine::kiProtocolVersion`.

## Acceptance criteria

1. Code read: no ship `SharedCrcMembers()` comment names `pGlobalIds` as
   omitted, and each still names `pClientGuids` as the omitted column with its
   reason.
2. Code read: each of the three PostRender `SharedCrcMembers()` tuples holds
   `pGlobalIds`, and the `Frame::kiVersion` base is above 188, its value in
   the commit that added this Plan.
3. `/compile` builds the BrokenEngineSandbox client and server.
4. `FC/Omnis/AGENTS.md` `## State Boundaries` no longer lists global identity
   as CRC-excluded.
5. Harness (`/agent-harness`): a connected client and server with a fleet
   holding all three ship types, run long enough for Fighters to launch and a
   ship to transfer between cells, report no frame CRC mismatch; and the
   replay determinism acceptance in
   `.agents/skills/agent-harness/references/replay.md`
   `### Replay determinism acceptance` over such a recording reports none. The
   originating analysis was a code read and records no harness setup, so the
   executing session builds the scene.

## Notes

`Documents/Investigations/Engine/ResolveShipGlobalIdCrcExclusion.md` is the
options record that the decision in `## Design` resolves; this Plan is the
work of its option 2.

`Documents/Plans/Game/RemoveShipNavigationDelayColumn.md` edits the same three
tuples; neither Plan needs the other, and whichever lands second merges one
tuple entry.

Other Plans from the same analysis edit the same headers elsewhere. No
ordering is required; whichever lands second merges. Each name is a Plan under
`Documents/Plans/Game/`:

- `Fighters.h` and `Battleships.h`: `ShareShipAvoidTerrain.md`.
- `Fighters.h`: `ReduceFightersCpp.md`, as far as its reduction adds a
  declaration.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
