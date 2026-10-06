<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-06T17:48:32.995Z","dependsOn":["Documents/Plans/Game/FrameEditRead.md"]} -->
# `edit_frame`: add rows through each collection's `Spawn` and remove rows through one `Destroy` pass

Line numbers cite baseline `2b4cb8d38aa57ca7ccd45983cc5121f64e85cf15`, plus the names `Documents/Plans/Game/FrameEditNaming.md` introduces (`FrameCollections`, `OwnerInFrame`) and the `read_frame` command of `Documents/Plans/Game/FrameEditRead.md`. Where a statement here and the code disagree, the code wins; report the contradiction instead of matching one side to the other.

## Context

User direction (binding): `edit_frame` adds a row in any collection by calling that collection's existing static `Spawn(Frame&, SpawnInfo)` on the staged frame and then applying the edit's writes to the new row; it removes rows immediately through the collections' kill-state columns plus one `FramePostRender::Destroy` pass on the staged frame. Links (uuids, pusher handles, registry ids) are set only by `Spawn` and `Destroy`; identity and handle columns stay refused.

Why these two mechanisms: every server collection's row append is its `Spawn(SpawnInfo)` overload, which mints the row's uuid and pusher (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.cpp:388-457`, `Spaceships/Spaceships.cpp:518-568`), its registry id (`Spaceships.cpp:552`), draws the random numbers the simulation expects (`Players.cpp:436`; `Missiles/Missiles.cpp:405-419`; `Engine/Source/Frame/Collections/Explosions/ExplosionsSpawn.cpp:67`), and tests the cell bounds (`Engine/Source/Frame/AGENTS.md`, the refuse-outside-the-cell rule). Every server collection's row removal is the predicate sweep in its `Destroy` phase hook, which releases owned pushers and clears handles (`Players.cpp:199-224`; `Spaceships.cpp:339-352`, `:397-411`; `Missiles.cpp:300-302`, `:352-369`; `Blasters/Blasters.cpp:203-217`; `Engine/Source/Frame/Collections/Explosions/Explosions.cpp:200-262`), and `FramePostRender::Destroy` (`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:212-221`) already runs every hook, engine, players, then game, so a new collection joins it automatically. Both are callable on the main thread against a staged `Frame`: none opens a registry query window or reads a collision queue, and the staged copy carries the frame's uuid counter and random engine (`Engine/Source/Frame/FrameBase.h:144-145`).

Decisions this Plan makes (author's, with rationale):

1. A spawn edit supplies only the position; every other `SpawnInfo` field keeps its default, and the edit's `writes` set whatever else the scenario needs. One uniform rule for every collection, and the per-collection defaults are already documented by each `SpawnInfo` struct. The one exception: a spawned player gets a global id minted from `gpGame->miNextGlobalId`, the way `inject_payload` `SpawnPlayer` does (`commands-server.md:69`), because that id must be unique process-wide and no write could guarantee that. The counter advances only when the batch commits.
2. The agent spawns players through this path as unowned synthetic AI, like `inject_payload` `SpawnPlayer`: no fleet or ownership bookkeeping runs. An owned player removed by the Destroy pass is detected as a death on the next tick and respawned by its fleet (`Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp:203-232`); documented, not prevented.
3. `pushers` cannot be spawned: a pusher row belongs to its player or spaceship and has no `SpawnInfo` overload (`Engine/Source/Frame/Collections/Pushers/Pushers.h:91-93`). Explosions use their four-argument overload with the cell's coordinate and the frame's `fCurrentTime` (`Explosions.h:270-271`); type index 0 is spawned and `puiTypeIndices` is a write.
4. A blaster spawned with the default zero velocity gets a zero direction, because `XMVector3Normalize` returns the zero vector for zero length (`Blasters.cpp:127`); the agent writes `pVecVelocities` and `pVecDirections` as it needs. That settles the open item from the research.
5. The Destroy pass is an opt-in top-level key, `destroy`, so an ordinary batch pays nothing and index-addressed edits never see rows move underneath them: the pass runs once after every edit of the batch, and swap-and-pop reorders rows only then.
6. The response reports `spawned` and `destroyed` row counts and the minted `globalIds`, so acceptance and recipes do not need a second query.

## Design

### Spawn edit

`{"collection":<name>,"spawn":[x,y,z],"writes"?:{...}}`, keys exactly these; `writes`, when present, is a non-empty object. `spawn` is the row's position in cell-local meters with w set to 1 (`common::ValidateVector<true>` requires it, `Common/AGENTS.md` `## Determinism and Math`). Before anything else, `common::InsideArea(vecPosition, engine::LocalFrameArea())` must hold, else `'spawn' position must lie inside the cell`; the test runs here because the player overload asserts instead of refusing (`Engine/Source/Frame/AGENTS.md`, the producer-must-test rule).

Dispatch inside the `FrameCollections` fold, per descriptor `C` with `P = typename C::PostRender`; `SpawnInfo` is named only inside the branch that requires it, because `engine::PushersPostRender` declares none (`Pushers.h:85-95`):

- `if constexpr (requires { typename P::SpawnInfo; })`: declare `typename P::SpawnInfo info {}; info.vecPosition = vecPosition;` then
  - `if constexpr (std::is_same_v<P, PlayersPostRender>)`: `info.globalPlayerId = engine::GlobalId {.iValue = riNextGlobalId++};` where `riNextGlobalId` is the command's local copy of `gpGame->miNextGlobalId`, and the minted value is appended to the response `globalIds`;
  - `if constexpr (requires { P::Spawn(rFrame, info); })`: call it;
  - `else`: call `P::Spawn(rFrame, coordinate, std::chrono::duration<float>(rFrame.interpolate.fCurrentTime), info)` with `coordinate` the request's `coord`;
- `else`: throw `'<collection>' rows are owned by another collection and cannot be spawned`.

The new row is `OwnerInFrame<P>(rFrame).iCount - 1`; when `writes` is present, `WriteKeys<true>(typename C::Columns {}, rFrame, iIndex, writes, collection)` applies them and their count joins `edited`. Each spawn edit adds one to `spawned`. The random draws and uuid mints `Spawn` performs land in the staged frame and commit with it; `uiNextUuid` advancing is within the existing guard (`ServerFrameEdit.cpp:412-417`).

A row edit may address a row spawned earlier in the same batch, because edits apply in request order.

### Destroy pass

Top-level `"destroy":bool`, optional; `edit_frame`'s top-level keys become exactly `coord`, `edits`, and `destroy`. When `true`, after the last edit and before the `uiNextUuid` guard, call `FramePostRender::Destroy(stagedFrame, gpGame->mCoordinateFrames.at(coordinate).staticData)` once. `destroyed` is the sum over `FrameCollections` of `OwnerInFrame<P>(stagedFrame).iCount` before the pass minus after it (zero when `destroy` is absent or `false`), so a destroyed player or spaceship counts 2: its own row and the pusher row its hook releases in the same pass (`Players.cpp:221`, `Spaceships.cpp:345`). The staged frame's id maps are valid because `TransferViaStream` rebuilt them (`Frame.cpp:821-834`), so pusher release through `idToIndexMap.at` succeeds, and the commit read rebuilds them again.

The kill states the pass consumes, as the hooks define them (the implementer copies the enumerator bit values from the flag enums into the documentation):

| Collection | Rows the pass removes |
|---|---|
| `players` | `pFlags` has `kExploding` and `pfDestroyedTimes <= 0` (`Players.cpp:215-218`) |
| `spaceships` | `pFlags` has `kExploding` and not `pfDestroyedTimes > 0` (`Spaceships.cpp:402-404`) |
| `missiles` | `pFlags` has `kSilentDespawn`, or has `kExploding` with `pfDestroyedTimes <= 0` (`Missiles.cpp:357-361`) |
| `blasters` | `pFlags` has `kDestroy` (`Blasters.cpp:203-217`) |
| `explosions` | `pFlags` has `kDestroysSelf` and `fCurrentTime - pfStartTimes` has passed every trail end time (`Explosions.cpp:239-256`) |
| `pushers` | removed with their owning player or spaceship only |

Without the pass, the same writes remove the rows at the next tick's Destroy phase, with that tick's earlier phases still seeing them.

### Refusals and resynchronization

Unchanged. The resynchronization refusal (`ServerFrameEdit.cpp:394-399`) reads the live frame's player count before any edit, so a spawn edit that would put the first player into a playerless non-origin cell is refused when a client is handshaken, as any edit to that cell is.

### Response

`{"edited":int,"spawned":int,"destroyed":int,"globalIds":[...],"resynchronizedClients":int}`; `globalIds` is always present, in spawn-edit order, holding only player ids. `gpGame->miNextGlobalId` is assigned the local counter after the commit succeeds, so a failed batch leaves `status.nextGlobalId` unchanged.

### Documentation

- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md` `edit_frame` entry: the spawn edit form and its position rule; that every other `SpawnInfo` field keeps its default (each collection's `SpawnInfo` struct lists them), that players get a minted global id, that explosions spawn type index 0, that pushers cannot be spawned, and that a default blaster has zero velocity and direction; the `destroy` key, the kill-state table with enumerator values, the swap-and-pop note (indices change only after the pass; address rows before it), the one-tick-later alternative, and the fleet-respawn note for owned players; the random-draw and uuid note; the response fields; delete "Rows cannot be added or removed".
- `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md` `## Contracts`, the `edit_frame` writes and refusal bullet (`:17`): rows are added only through each collection's `Spawn(SpawnInfo)` and removed only through the collections' own `Destroy` hooks run by one `FramePostRender::Destroy` pass, so links are never written directly; a collection with a `SpawnInfo` overload is spawnable with no further code.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp`: spawn edit, destroy pass, response.
- Documentation: `commands-server.md`, game `Agent/AGENTS.md`.

## In scope

- `ServerFrameEdit.cpp`: `ApplyEdit` (the spawn edit arm and its key check), a `SpawnRow` helper per the dispatch above, `WriteRow`'s key check (`:296-302`) so a `spawn` key routes to the spawn arm instead of failing, `CommandEditFrame` (the `destroy` key in the top-level key check at `:377-383`, the local global-id counter, the pass, the `destroyed` count, the response fields at `:431-432`), and the includes those need (`Frame/Collections/Pushers/Pushers.h` is reached through `FrameBase.h`; `Explosions.h` likewise).
- The documentation edits under `### Documentation`.

## Out of scope

- Any `Spawn` or `Destroy` hook, `SpawnInfo` struct, `DestroySweep`, `CollectionLifecycle.h`, and `FramePostRender::Destroy` itself.
- Spawn fields beyond the position and the player global id: no alignment, direction, velocity, health, flags, fleet, or type defaults set by the command; no fleet or ownership bookkeeping; no `inject_payload` change.
- A per-row remove form, a `remove`/`index` removal key, direct `DestroyElement` calls, capacity changes, and identity or handle writes.
- Running Update, Transfer, or any other phase on the staged frame; a `destroy` pass before or between edits.
- Validation beyond the position-in-cell test and what `Spawn`, the save reader, and the existing guards already do: no position-versus-terrain test, no check that a kill state is reachable, no duplicate-global-id scan.
- `read_frame`, `query_*`, `status`, cells and pins, the save format, and client code.

## Risk triggers and invariants

Tier 3 (`.agents/references/risk-tiers.md`): the command now changes row counts, uuid and global-id counters, pusher and registry links, and the random stream of CRC'd frame state, spanning game Agent, game Frame collections, engine Frame collections, and the server global-id counter.

- Every link is set by the collections' own code: no `engine::Id<T>` column is ever written by the command, a spawned player or spaceship owns exactly one new pusher row, a destroyed one releases it, and `ValidateCollectionPair` passes at commit (count parity, no zero or duplicate id).
- The committed frame is one a save can hold: the commit still runs the save reader's checks, and the Destroy pass leaves no row matching its own predicate.
- `status.nextGlobalId` advances by exactly the number of spawned players in a committed batch and not at all for a failed one; a minted id is never below the counter.
- Uuid and random-engine state advance exactly as the same spawns would advance them inside a tick; `uiSharedCrc == Crc()` after commit, as today.
- Rows the pass does not select keep their relative order, because `DestroySweep` walks backwards and swap-and-pop moves only the tail (`Engine/Source/Frame/FrameUtils.h:357-372`).
- Threading, trust, replay refusals, and resynchronization: unchanged.

## Acceptance criteria

Builds: server Debug and Profile compile (the `requires` dispatch instantiates for every descriptor).

Harness checks (`/agent-harness`, Debug server, after `reset`; the player is created with `inject_payload` `SpawnPlayer` and one unpaused tick, then `pause {"paused":true}`; `z` is the player's `local` z from `query_players`; counts come from `query_frame` and `read_frame` `count`):

| # | Check | Expected observation |
|---|---|---|
| 1 | `edit_frame` one spawn edit `spaceships` `spawn: [10,20,z]` with `writes: {"pAlignments": <enemyAlignment from read_frame frame>}` | `spawned:1`, `edited:1`, `destroyed:0`, `globalIds:[]`; spaceship count +1, pusher count +1; `read_frame` `spaceships` last index shows `pVecPositions [10,20,z]`, `pfHealths` equal to `kfSpaceshipHealth`, nonzero `puiPushers` and `puiRegistryIds`, `pfDestroyedTimes -1`, the written alignment |
| 2 | Spawn edit `players` `spawn: [-30,15,z]` | `spawned:1`, `globalIds` has one id equal to the pre-command `status.nextGlobalId`; `status.nextGlobalId` +1; player count +1, pusher count +1; `read_frame` shows that `pGlobalPlayerIds`, nonzero `pIds` and `pPushers`, `pClientGuids [0,0]`, `pfArmors` equal to `kfPlayerArmor` |
| 3 | Spawn edits `blasters` `spawn: [5,5,z]`, `missiles` `spawn: [6,6,z]`, `explosions` `spawn: [7,7,z]` in one batch | `spawned:3`; each count +1; the blaster's `pVecDirections` and `pVecVelocities` are `[0,0,0]`; the explosion's `puiTypeIndices` is 0 and `pfStartTimes` equals `frame.fCurrentTime`; the missile's `pfDeltaRotationDelays` is positive |
| 4 | Rejections, each its own command: `pushers` spawn; `spawn: [10000,0,z]`; `spawn: [1,2]`; a spawn edit with `index`; a spawn edit with `writes: {}`; a batch of a `players` spawn followed by `players` `index: 999` | `ok:false` each; counts and `status.nextGlobalId` unchanged after every one |
| 5 | Destroy: `blasters` row 0 `pFlags` set to `kDestroy` with `destroy:true`; then `spaceships` the row from check 1 `pFlags: kExploding`, `pfDestroyedTimes: 0`, `destroy:true`; then the player from check 2 likewise | `destroyed:1` for the blaster, `destroyed:2` for the spaceship and for the player; blaster count -1; spaceship count -1 and pusher count -1; player count -1 and pusher count -1; `read_frame` on the removed index either errors (count shrank) or shows a different `pIds` |
| 6 | `destroy:true` with one unrelated write and no matching row; `destroy:false` | `destroyed:0` both, `ok:true` |
| 7 | Unpause 64 ticks after checks 1-3 (fresh setup), then `save`, `load {"pauseAfterLoad":true}` | `get_logs` has no new `Assert` line; `resetToFresh:false`; counts after the load equal counts before the save |
| 8 | One connected client subscribed to `[0,0]`: spawn a spaceship and a player, then unpause 128 ticks; then destroy the fleet-owned player (the client's own, found by its nonzero `pClientGuids`) with `destroy:true` and unpause 64 ticks | `resynchronizedClients:1`; no `CONFIRMED DESYNC` line; after the destroy, `query_players` eventually shows a new row with the same `globalId` (fleet respawn) |
| 9 | Record about 64 ticks after a spawn batch, stop, and run the replay determinism check | CRC match, an `End replay ..., looping` marker, no `LogDifferences CRC` line |

By code reading: `SpawnRow` calls the player overload only after `InsideArea` passed, so its `ASSERT` is unreachable from the command; the `requires` chain selects the four-argument overload only for `engine::ExplosionsPostRender` and throws only for `engine::PushersPostRender`; the Destroy pass runs after the last edit and before the guard and commit.

## Execution card

- Goal: the spawn edit, the `destroy` pass, the response fields, and the documentation edits under `## In scope`.
- Tier 3; roles as the Change Workflow assigns: `implementer` runs `/implement-plan` in one slice, then `/update-affected-code`; `builder` runs `/compile` (server Debug and Profile); `reviewer` runs `/plan-audit` and `/plan-simplicity-review` before implementation, `/external-grill-plan` after `/plan-audit`, and `/repo-code-review`, `/comment-review`, `/coherence-review`, and `/adversarial-review` after; `/update-claude-docs` and `/progressive-disclosure-review` follow; `/finalize-changes` lands with user confirmation.
