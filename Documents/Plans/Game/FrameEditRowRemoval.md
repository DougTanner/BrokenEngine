<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-17T15:06:11.923Z","dependsOn":["Documents/Plans/Game/FrameEditRows.md"]} -->
# Harness row removal through the regular Destroy phase: document the kill-state writes

Line numbers cite baseline `d8ce779b3b4fb984b572522896b6382d8dd19d19`. Where a statement here and the code disagree, the code wins; report the contradiction instead of matching one side to the other. Saved as `Documents/Plans/Game/FrameEditRowRemoval.md`; its `dependsOn` names `Documents/Plans/Game/FrameEditRows.md`, which holds the Stage 1 Plan (the unified spawn system): its `inject_payload` spawn entries are the setup of every check below.

## Context

User direction (binding, `.agents/references/authority-order.md`): the agent harness should be able to set a row's destroyed flag, and perhaps health to zero, so that rows clean up cleanly and normally, through the regular Destroy phase.

What the code already provides, by read:

- `edit_frame` writes any column of a collection's Interpolate and PostRender structs (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp:35-39` column lists; `commands-server.md:96-130`): every kill-state column is listed (`pFlags` for all five collections, `pDestroyedTimes` for omnis, spaceships, and missiles, `pDestroyedExplosionTimes`, `pfArmors`, `pfHealths`), a flags column takes an integer mask (`:119-120`), and only identity and handle columns are refused.
- `RunFrameTick` runs the Destroy phase every tick after Transfer and before
  Spawn (`Engine/Source/Frame/FrameBase.cpp:309-310`): the engine walk (Explosions,
  `Engine/Source/Frame/Collections/Explosions/Explosions.cpp:179-240`) then
  `FramePostRender::Destroy` (`Frame.cpp:184-191`: Omnis, then the game tuple).
  Each hook is a `DestroySweep` over its own predicate that releases the owned
  pusher and swap-and-pops the row (`Omnis.cpp:203-211`,
  `Spaceships.cpp:400-408`, `Missiles.cpp:347-351`, `Blasters.cpp:187-190`). A
  natural death ends in exactly this removal.
- So a kill state written by `edit_frame` is consumed by the next unpaused tick's Destroy phase on the server and, after the commit's client resynchronization (`ServerFrameEdit.cpp:472-475`, `commands-server.md:145-159`), by the same phase on every client, which receives the edited frame as full state and then the tick. Nothing in the engine or game is missing.

"Health to zero" settled by read:

- Omnis test `pfArmors <= 0` every tick in PostCollision for every non-exploding row (`OmnisCombat.cpp:248-253`) and then set `kExploding`, `pDestroyedTimes = kDestroyTime` (0.7 s, `Omnis.h:21`), and the death-explosion interval. Writing `pfArmors:0` therefore starts the full normal death sequence on the next tick and ends in the same removal about 23 ticks later.
- Spaceships test `pfHealths <= 0` only inside the two damage paths
  (`SpaceshipsCombat.cpp:131`, `:175`), after subtracting a hit. Writing
  `pfHealths:0` is latent until the next hit; the staged death is started by the
  three writes `BeginExplosion` makes (`SpaceshipsCombat.cpp:40-42`: `pFlags`
  `kExploding` (2), `pDestroyedTimes` 0.25 (`kSpaceshipDestroyTime`,
  `Spaceships.h:19`), `pDestroyedExplosionTimes` 0.024
  (`kSpaceshipDestroyExplosionInterval`, `Spaceships.h:20`)). `BeginExplosion`
  also clears `puiRegistryIds` (`:48`), an identity column `edit_frame` refuses,
  so missiles keep homing on the exploding ship until its removal 8 ticks later;
  documented, not worked around.
- Blasters, missiles, and explosions have no health: their kill state is a flag (`kDestroy` 1; `kSilentDespawn` 64, or `kExploding` 2 with `pDestroyedTimes` for the 0.35 s staged death, `Missiles.h:21`, `Missiles.cpp:464-470`; `kDestroysSelf` 1 once every trail has ended, `Explosions.cpp:221-236`).

Decision (author's): the smallest sufficient change is documentation plus a
harness proof. No code changes. Not chosen: an in-command `destroy:true` pass on
the staged frame (proposed by the earlier text of
`Documents/Plans/Game/FrameEditRows.md`, which Stage 1 replaced), because the
next tick's Destroy phase is the normal cleanup the user asked for and a second
invocation of the same hooks inside the command would duplicate it only to save
one tick; a destroy status change beside the spawn kinds, because `kDestroyOmni`
already exists for omnis and the other collections need no wire message to die
when a column write suffices.

## Design

### `commands-server.md` `edit_frame` entry

- Replace "Rows cannot be added or removed." (`:129`) with: rows are added only by `inject_payload` spawn entries and removed only by the collections' own Destroy hooks; `edit_frame` itself never changes a row count.
- Add a "Removing rows" paragraph after the staged-copy paragraph (`:134-143`): write a row's kill state and the next unpaused tick's Destroy phase removes it through the collection's own hook, exactly as a natural death ends; an omni's or spaceship's pusher goes with it; rows after the removed one may move into its slot (swap-and-pop), so read indices again after the tick. The kill-state table:

| Collection | Writes that remove the row at the next tick | Normal staged death instead |
|---|---|---|
| `omnis` | `pFlags:1` (`kExploding`; `pDestroyedTimes` is already 0 on a live omni) | `pfArmors:0`: PostCollision starts the 0.7 s death sequence with its explosions |
| `spaceships` | `pFlags:2` (`kExploding`; the live sentinel `pDestroyedTimes` -1 is not positive) | `pFlags:2`, `pDestroyedTimes:0.25`, `pDestroyedExplosionTimes:0.024`: the 0.25 s staged explosions; `pfHealths:0` alone changes nothing until the next hit; missiles keep homing until removal |
| `missiles` | `pFlags:64` (`kSilentDespawn`) | `pFlags:2`, `pDestroyedTimes:0.35` |
| `blasters` | `pFlags:1` (`kDestroy`) | none |
| `explosions` | `pFlags:1` (`kDestroysSelf`) on a row without trails (`piTrailCounts` 0, the `SpawnExplosion` default, `Explosions.h:282`) | `pFlags:1` on a row with trails: the row goes when its last trail ends; a row without `kDestroysSelf` is never removed |
| `pushers` | removed with their owning omni or spaceship only | |

- A `pFlags` write replaces the whole mask, so include any bit the scenario still needs (for omnis, the navigation bits are rebuilt by Update).
- With a handshaken client the commit resynchronizes clients before the tick, as every `edit_frame` does; a fleet-owned omni removed this way is detected as a death after that tick (`ServerClientManager::DetectOwnedEntityDeaths`, `ServerClientManager.cpp:172-239`) and its fleet respawns it under the same global id after the 1 s delay (`ServerFleetManager.cpp:17`, `:247-262`, `:185-211`), the ordinary death path.

### `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md` `## Contracts`

In the `edit_frame` bullet (`:46-47`), add one clause: the command adds and removes no row itself; removal is the next tick's Destroy phase consuming a written kill state, so links are never written and every collection's own hook stays the only remover.

## Critical files

- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md` (`edit_frame` entry).
- `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md` (`## Contracts`, the `edit_frame` bullet).

## In scope

- `commands-server.md` `edit_frame` entry: the `:129` sentence, the "Removing rows" paragraph, and the table above.
- `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md` `## Contracts` `edit_frame` bullet: the one clause above.

## Out of scope

- Any C++ change: no `destroy` key, no in-command Destroy pass, no destroy status change, no `edit_frame` response field, no change to any `Destroy` hook, `DestroySweep`, `FramePostRender::Destroy`, or `ServerFrameEdit.cpp`.
- Making `pfHealths:0` start a spaceship death, clearing `puiRegistryIds` through `edit_frame`, or any identity or handle write.
- `read_frame`, `query_*`, `status`, `inject_payload`, replay, and save behaviour; every other Plan under `Documents/Plans/`.

## Risk triggers and invariants

Tier 1 (`.agents/references/risk-tiers.md`): documentation only; no signature, format, trust, or behaviour change.

- Every statement in the table is true of the baseline code cited under `## Context`; the harness checks below prove each row.
- No sentence elsewhere in the `edit_frame` entry or the Agent contract still says rows cannot be removed.

## Acceptance criteria

Builds: none required by the change; the harness checks run on a server Debug and client Debug build of the tree with Stage 1 landed.

Pre-change runs: none. Checks D1-D4 prove pre-existing behaviour the documentation describes; they are not before/after comparisons. Check D5 is the only criterion the unedited tree fails.

Harness setup (`/agent-harness`, Debug server, `reset`, `pause {"paused":true}`,
`edit_frame {"coord":[0,0],"edits":[{"collection":"frame","writes":{"fSpawnTimer":0}}]}`
so the spaceship group spawner adds no rows during the setup and D1 ticks, one
`inject_payload` at `[0,0]` with `SpawnOmni` `vecPosition:[45,-12]`,
`SpawnSpaceship` `vecPosition:[10,20]` `alignment:"enemy"`, `SpawnBlaster`
`vecPosition:[5,5]`, `SpawnMissile` `vecPosition:[6,6]`, `SpawnExplosion`
`iTypeIndex:0` `vecPosition:[7,7]`, then one tick (`timescale {"faster":false}`
five times to 1/32, `pause false`, wait 1.2 s, `pause true`), then note every
count from `query_frame` and `read_frame`):

| # | Check | Expected observation |
|---|---|---|
| D1 | One `edit_frame` at `[0,0]`: blasters row 0 `pFlags:1`; the injected spaceship row `pFlags:2`; the injected omni row `pFlags:1`; the missile row `pFlags:64`; the explosion row `pFlags:1` | `edited:5`, `resynchronizedClients:0`; every count unchanged immediately after the command; after one tick: blasters -1, spaceships -1, missiles -1, explosions -1, omnis -1, pushers -2; `get_logs` has no new `Assert` line |
| D2 | Fresh setup; `edit_frame` the omni row `pfArmors:0`; one tick; then 24 more ticks at 1/32 | after the first tick the row has `pFlags` containing 1 and `pDestroyedTimes` exactly `kDestroyTime` 0.7f (set in PostCollision; Update decrements only rows already exploding in the previous frame, `Omnis.cpp:468-471`); explosions count rises during the 24 ticks; afterwards the omni row is gone and pushers -1 |
| D3 | Fresh `reset`, pause, inject only `SpawnSpaceship` `alignment:"enemy"` at `[10,20]`, one tick; `edit_frame` `pfHealths:0`; 32 ticks | the row is still present with `pfHealths` 0 and `pFlags` without bit 2; then `edit_frame` `pFlags:2`, `pDestroyedTimes:0.25`, `pDestroyedExplosionTimes:0.024` and 9 ticks: explosions count >= 1 and the spaceship row gone, pushers -1 |
| D4 | Fresh `reset`, unpaused, one connected client: `click {"label":"[+]##Fleet"}`, `click {"label":"[+]##Omni"}`, poll `set_client_grid_coord {"coord":[0,0]}` until `ok:true`; pause; find the row with nonzero `pClientGuids`, note its `globalId`; `edit_frame` `pFlags:1` on it; unpause 64 ticks | `resynchronizedClients:1`; server log gains `SendOwnedEntityState State: Died` and later `State: Spawned` for that `globalId`; `query_omnis` shows a row with that `globalId` again; no `CONFIRMED DESYNC` |
| D5 | Documentation diff read of `commands-server.md` `edit_frame` entry and game `Source/Agent/AGENTS.md` | the `:129` sentence is replaced, the table and the resynchronization and respawn notes are present, the Agent bullet carries the clause; no sentence says rows cannot be removed; `git diff --stat` lists only these two files |

## Execution card

Execution card:
### What does this plan do?
It documents, in the `edit_frame` command entry and the game Agent contract, how an agent removes a row: write the collection's kill state and let the next tick's Destroy phase remove it through the collection's own hook, exactly as a natural death ends. It records which writes remove each collection's row at once, which writes start the staged death instead, that a spaceship's health write alone is latent, and what happens to a fleet-owned omni. No code changes.
### Why this is good for the codebase
Agents need to take ships, shots, and effects out of a scene without bypassing the simulation's own cleanup; the engine already does this on the next tick, but no document says so and the command entry still claims rows cannot be removed. Writing it down, with a harness proof per collection, gives scenario recipes one supported removal path and keeps `edit_frame` free of a second copy of the Destroy phase.
- Goal: the two documentation edits under `## In scope`, proven by checks D1-D4.
- Out of scope: per `## Out of scope`; no C++ change.
- Tier trigger: Tier 1, documentation.
- Interfaces and invariants: `edit_frame` request and response unchanged; the kill-state table matches the Destroy predicates cited under `## Context`.
- Acceptance checks: the `## Acceptance criteria` table; pre-change runs none.
- Roles:
  - Implement: `implementer` `/implement-plan` in one slice (the two documents).
  - Review: fresh `reviewer` `/coherence-review` in the Tier-1 combined pass (coherence, triggered cleanup); the acceptance table is the Verify and land step's landing table, because this Plan lands in its own session.
  - Runtime: `implementer` drives `/agent-harness` for D1-D4 on a build of the tree with Stage 1 landed.
  - Cleanup: `implementer` `/update-claude-docs` not triggered (no C++ change); the Tier-1 combined pass carries `/progressive-disclosure-review` for the AGENTS.md clause.
  - Landing: `implementer` `/finalize-changes`.
- Data mode: Shared; no Local trigger.
- Leftovers: none.

## Unresolved decisions

None. The "health to zero" question is settled by code: it works for omnis (checked every tick) and is latent for spaceships (checked only on damage); the table documents both.
