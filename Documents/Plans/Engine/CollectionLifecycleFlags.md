<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-18T19:24:54.414Z","dependsOn":[]} -->
# Carry row lifecycle states in one shared engine flag column

Tier 3 (`.agents/references/risk-tiers.md`): the change alters CRC'd collection
layout, the grid save, replay, and snapshot formats, the `StatusChange` wire
payloads, and when owner-released rows leave the frame, including the CRC'd
Pushers, and spans all fifteen engine and game collections. Line numbers cite
baseline `15df91fc96670b336ba4ba1dc007023587da0d28`.

## Context

Five collections remove their own rows through per-collection flag bits whose
names and values differ for the same concept:

| Concept | Today |
|---|---|
| Leave the cell: Transfer removes the row and re-spawns it in the neighbour | `kTransfer`: `MissileFlags` 0x01 (`Missiles.h:39`), `BlasterFlags` 0x02 (`Blasters.h:79`), `SpaceshipFlags` 0x08 (`Spaceships.h:119`), `OmniFlags` 0x0020 (`Omnis.h:160`) |
| Remove at this tick's Destroy with no death sequence | `BlasterFlags::kDestroy` 0x01 (`Blasters.h:78`), `MissileFlags::kSilentDespawn` 0x40 (`Missiles.h:45`) |
| Staged death: removed by Destroy once the `pDestroyedTimes` countdown ends | `kExploding`: `OmniFlags` 0x0001 (`Omnis.h:155`), `SpaceshipFlags` 0x02 (`Spaceships.h:117`), `MissileFlags` 0x02 (`Missiles.h:40`) |
| Removed by Destroy once the row's own lifetime ends | `ExplosionFlags::kDestroysSelf` 0x01 (`Explosions.h:81`), on `ExplosionsInterpolate::pFlags` |

Game paths are relative to `Projects/BrokenEngineSandbox/Source/`; collection
files are under `Frame/Collections/`.

- Both concept-two releases are identical: `RemoveOwnedObjects` then
  `DestroyElement` (`Blasters/Blasters.cpp:194-202`,
  `Missiles/Missiles.cpp:352-363`).
- The four Transfer hooks repeat one predicate, `pFlags[i] & kTransfer`
  through `engine::DestroySweep` (`Engine/Source/Frame/FrameUtils.h:380`):
  `Blasters/Blasters.cpp:154-156`, `Missiles/Missiles.cpp:304-306`,
  `Omnis/OmnisNavigation.cpp:211-213`, `Spaceships/Spaceships.cpp:361-363`.
- The staged-death Destroy predicates differ only in the countdown test:
  `Omnis/Omnis.cpp:203-205` and `Missiles/Missiles.cpp:355` use `<= 0`,
  `Spaceships/Spaceships.cpp:400-402` uses `!(> 0)`.
- `kTransfer` and the concept-two bits are set in PostCollision and consumed
  the same tick: a marked row is removed before the CRC is stamped
  (`Engine/Source/Frame/FrameBase.cpp` `RunFrameTick`, Transfer then Destroy). Only
  `kExploding` and `kDestroysSelf` stay set at rest.
- No lifecycle bit crosses a cell. A row is marked for Transfer only when it
  is not dying (`Omnis/OmnisCombat.cpp:248-258` else-branch,
  `Spaceships/SpaceshipsCombat.cpp:140`, the `kExploding`/`kSilentDespawn`
  skips at `Missiles/MissilesUpdate.cpp:327-335`, the exclusive branches at
  `Blasters/BlastersUpdate.cpp:221-248`). Omnis clear `kTransfer` before
  sending (`Omnis/OmnisNavigation.cpp:218-219`), Missiles rebuild the sent mask
  from alignment alone (`Missiles/Missiles.cpp:312-313`), and the Blasters and
  Spaceships Transfer literals send no flags.
- `ExplosionsInterpolate::pFlags` has one reader after Spawn: the
  `kDestroysSelf` test in `ExplosionsPostRender::Destroy`
  (`Explosions.cpp:219-221`); `kYellow` and `kRed` are read only from the
  `SpawnInfo` at spawn (`ExplosionsSpawn.cpp:206-210`), and `ExplosionsUpdate.cpp:25,34`
  only carries the column forward.
- The harness `edit_frame` kill writes therefore differ per collection
  (`pFlags:1`, `pFlags:2`, `pFlags:64`, `pFlags:1`, `pFlags:1`) in the kill-state
  table of `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`.

The other ten collections carry no removal flag. Engine paths below are
relative to `Engine/Source/Frame/Collections/`.

- AreaLights, Billboards, HexShields, PointLights, Pushers, Sounds,
  SmokeTrails, and WindTrails rows are owned through a handle: the owned
  collection's `Add` writes the new id into one owner column, and the owner
  removes the row at its own call with `RemoveIndexableElementAndClearHandle`
  (`CollectionLifecycle.h:102-108` pair form, `:124-136` frame form), which
  removes the row by id and clears the handle. `PushersPostRender::Remove`
  (`Pushers/PushersUpdate.cpp:190-196`) and `HexShieldsPostRender::Remove`
  (`HexShields/HexShieldsUpdate.cpp:74-80`) only wrap it. Billboards has an
  `Add` (`Billboards/BillboardsUpdate.cpp:30`) and no caller.
- The owner removal sites: Blasters `RemoveOwnedObjects`
  (`Blasters/Blasters.cpp:129-145`: PointLights, AreaLights, WindTrails),
  called from `Transfer` and `Destroy`; Missiles `RemoveOwnedObjects`
  (`Missiles/Missiles.cpp:274-293`: AreaLights, SmokeTrails, Sounds), called
  from `Transfer` and `Destroy`, plus the same releases in `Fall`
  (`:440-452`, from Update at `MissilesUpdate.cpp:219`) and `Explode`
  (`:478-489`, from PostCollision at `MissilesUpdate.cpp:346,362`); Omnis
  `OmnisInterpolate::RemoveOwnedVisuals` (`Omnis/Omnis.cpp:174-185`:
  WindTrails, HexShields) and `PushersPostRender::Remove`, from the `Destroy`
  pre-loop and sweep (`Omnis.cpp:192-210`), `ProcessDestroyStatusChanges`
  (`:214-235`, run from `OmnisPostRender::Spawn`), and `Transfer`
  (`Omnis/OmnisNavigation.cpp:254-256`); Spaceships `RemoveOwnedObjects`
  (`Spaceships/Spaceships.cpp:339-351`: Pushers, WindTrails), from `Transfer`
  and `Destroy`; and `ExplosionsPostRender::Destroy` removing each expired
  smoke trail (`Explosions/Explosions.cpp:214`).
- Puffs, WindRadials, and PointLights rows added through `AddControlled`
  (Explosions, Blasters, Omnis, and Spaceships call it) expire by controller type in their
  own `Destroy`
  (`DestroyExpiredControlled`, `CollectionController.h:134-155`); Puffs and
  WindRadials have no id or owner.
- Phase order (`Engine/Source/Frame/FrameBase.cpp` `RunFrameTick`): Update
  (`:295`), PostCollision (`:302-303`), Transfer (`:309-310`), Destroy
  (`:312-313`), Spawn (`:314-315`), then the CRC (`:318`); each phase runs the
  engine collections, in `Collections()` order (`FrameBase.h:253`, Explosions
  before SmokeTrails), before the game's.
- Only Explosions and Pushers are server collections (`ServerCollections()`,
  `FrameBase.h:265-268`): the other nine engine collections are client-only,
  stay out of the CRC and every grid save, replay, and snapshot (replays and
  grid saves are server-only, `Engine/Source/File/AGENTS.md`), and own no
  `kiVersion`.
- The owned rows have no death behaviour of their own. A Sounds row carries
  only parent-synced voice data; when it disappears, `StaticVoices` fades its
  voice over the `pFadeOutTimes` value captured at voice creation
  (`Engine/Source/Audio/StaticVoices.cpp:456`, `DeactivationPass` at `:473`).
  SmokeTrails and WindTrails rows only deposit into the screen-space smoke
  simulation and the ping-pong wind texture (`SmokeTrails/AGENTS.md`,
  `WindTrails/AGENTS.md`), which keep what was deposited. No lights,
  hex shields, billboards, or pushers column holds a fade.

The user chose this direction in their own words: "I think 'Option 3: One
shared engine-level flag set' that every Collection implements in a standard
way (uint8_t?), Collections would need their own seperate field for their
unique-collection flags", after asking that `kExploding` and the destroy names
be standardized. The option it picked was one engine flag set with engine
Destroy and Transfer predicates testing it generically. Asked whether the
column goes on the five flag-removed collections or on all fifteen, the user
answered with a question: "I feel like all Collections could benefit from it,
owners could then mark them for 'start dying' and remove their references to
it, letting it clean itself up (if it is the type that wants to)?" The user
also wrote: "Then we could also have more shared code around destruction or
defered cleanup (DRY)". Recorded as the user's decision: every collection
carries the shared column. Recorded as the user's proposal, phrased as a
question, that the claim's Tier-3 grill must confirm: an owner releases a row
by setting the shared start-dying bit and dropping its handle. Recorded as the
user's direction: shared destruction and deferred-cleanup code, which
`### Shared sweeps` follows.

The author's interpretation, not a user decision: every owned type is released
through `kDying`, including types with no death behaviour, which their own
collection removes at the next Destroy, so release removes an owned row at the
next Destroy phase rather than at the owner's call. The alternative reading
of "(if it is the type that wants to)": owners mark start-dying only on a type
with death behaviour, and a type with none keeps being removed at the owner's
call, avoiding the one-tick delay, for example for the CRC'd pusher
(`### Release timing`). No owned type has death behaviour today (above), so
under that reading every current owner removal stays at the owner's call. The
claim's Tier-3 grill (`/external-grill-plan`) must confirm the reading with the
user. Apart from the recorded decision, proposal, and direction, everything
below is the author's recommendation, the `### Shared sweeps` helper design
included.

## Design

### Shared flag set

In `Engine/Source/Frame/Collections/CollectionLifecycle.h` (the existing row
lifecycle header, reached through `Collection.h`):

```cpp
enum class LifecycleFlags : uint8_t
{
	kTransfer = 0x01,
	kDestroy  = 0x02,
	kDying    = 0x04,
};
using LifecycleFlags_t = common::Flags<LifecycleFlags>;
```

- `kTransfer`: Transfer removes the row and re-spawns it in the neighbour.
- `kDestroy`: the next Destroy removes the row with no death behaviour.
- `kDying`, the user's "start dying": the row's own collection plays out its
  death behaviour and its Destroy removes the row when that ends, or at the
  next Destroy for a collection with none. Owners set it to release a row;
  setting it on types with no death behaviour is the author's reading,
  pending the grill (`## Context`).

`uint8_t`, the user's leaning, fits with five spare bits. `kDestroy` keeps the
Blasters name because it matches the Destroy phase, `DestroySweep`, and
`DestroyElement`; "silent" in `kSilentDespawn` only meant no explosion, which
every `kDestroy` removal already is. `kDying` takes over today's `kExploding`
(the staged death) and `kDestroysSelf` (removal once the explosion's trails
end): both already mean "removed by Destroy once this row's own death behaviour
ends", and the bit now also marks lights, sounds, and pushers, none of which
explodes. An explosion spawned with `kDying` is one nothing holds, finishing on
its own, which is what `kDestroysSelf` marked.

### The standard column

Every collection, all fifteen, declares
`engine::LifecycleFlags_t* __restrict pLifecycleFlags = nullptr;` on its
PostRender struct, which owns committed deterministic state
(`Engine/Source/Frame/AGENTS.md` `## Architecture`), and lists it in the
accessor that struct already uses: immediately before `pFlags` in the four
game collections (in place of `pFlags` for Blasters), after `pIds` in the
eight handle-owned engine collections, and as the only member of
`ExplosionsPostRender`, `PuffsPostRender`, and `WindRadialsPostRender`. The
author recommends against a `Collection` base member: the derived struct's
member tuple is what allocates, copies, serializes, and CRCs a column
(`Engine/Source/Frame/Collections/AGENTS.md` `## Core Contract`), so a
base-class pointer would sit outside every tuple. Per the
`/add-collection-member` checklist:

- CRC: inside `SharedMembers()` for the game collections, so
  `SharedCollectionCrc` covers it; Omnis also add it to `SharedCrcMembers()`.
  `PushersPostRender` and `ExplosionsPostRender` declare only `Members()`, so
  the column is CRC'd and on the wire there. The nine client-only collections
  are outside the CRC. Difference logging: beside `pFlags` in each game
  PostRender `LogDifferences`, and in `PushersPostRender::LogDifferences`
  (`Pushers/Pushers.cpp:36-48`) and Explosions'; the client-only collections
  use the base `Collection::LogDifferences`, which logs no column.
- Carry-forward: lifecycle bits are written only at transitions (PostCollision,
  AreaDamage, `Explode`, `BeginExplosion`, an owner release), so the column
  joins `PersistentMembers()` where declared (Omnis, Spaceships, Missiles) and
  rides the full `Members()` copy everywhere else, every engine PostRender
  included. A release set in Destroy or Spawn must survive into the next tick,
  and this copy carries it.
- Creation: every `Spawn`, `Add`, and `AddControlled` sets the new row's
  column. The game collections and every engine `Add` and `AddControlled` zero
  it (`ZeroMemberRow` covers only the Interpolate tuple there, so the write is
  explicit); `ExplosionsPostRender::Spawn` copies `SpawnInfo::lifecycleFlags`.
- Transfer: not carried and no game `SpawnInfo` gains a lifecycle member,
  because no lifecycle bit can be set on a transferring row (`## Context`).
  Owned rows never transfer: their owner releases them in its own `Transfer`.
- Hydration and identity: no client resource and no ID-map change. A released
  row keeps its id in `idToIndexMap` until its Destroy removes it through
  `RemoveIndexableElement`, so the map and rows stay paired and
  `ValidateCollectionPair` accepts a save taken in between. Full-state
  hydration rebuilds client-owned visuals from the owners
  (`Network/Client/ClientSession.cpp:249-266`), so a client-only row awaiting
  its Destroy is simply absent from a hydrated frame.
- Agent: the six `edit_frame` column lists, Pushers' included, add `pLifecycleFlags`, as the list's `static_assert` requires: `OmnisFrameColumns`, `SpaceshipsFrameColumns`, `MissilesFrameColumns`, and `BlastersFrameColumns` in `Agent/Commands/ServerFrameEdit.cpp`, and `ExplosionsFrameColumns` and `PushersFrameColumns` in `Engine/Source/Agent/Commands/FrameEdit.h`.

### Unique flags

Each collection's own enum keeps its name, its `pFlags` column, and every
remaining bit at its current value, so the packed Omnis navigation helpers
(`Omnis.h:178-200`) and recorded spawn values such as `flags:132` keep working:

| Collection | Moves to `pLifecycleFlags` | Stays in `pFlags` |
|---|---|---|
| Omnis (`OmniFlags`, `uint16_t`) | `kExploding` as `kDying`, `kTransfer` | every other bit |
| Spaceships (`SpaceshipFlags`) | `kExploding` as `kDying`, `kTransfer` | `kFleeOmni`, `kReturnToIslandCenter` |
| Missiles (`MissileFlags`) | `kExploding` as `kDying`, `kTransfer`, `kSilentDespawn` as `kDestroy` | `kDirectional`, `kTargetOmni`, `kTargetEnemy`, `kFalling` |
| Blasters (`BlasterFlags`) | `kDestroy`, `kTransfer` | none: the enum, `pFlags`, and `SpawnInfo::flags` are removed |
| Explosions (`ExplosionFlags`) | `kDestroysSelf` as `kDying` | `kYellow`, `kRed`, still on `SpawnInfo::flags` only; `ExplosionsInterpolate::pFlags` loses its last reader and is removed |

The other ten engine collections have no flag bit to move; they gain the
column alone. `BillboardFlags` and `PusherFlags` stay as they are on their
Interpolate structs.

### Owner release

This section and `### Release timing` follow the author's release-all-types
reading, which the grill must confirm (`## Context`). One helper in `CollectionLifecycle.h` replaces both
`RemoveIndexableElementAndClearHandle` forms:
`ReleaseIndexableElementAndClearHandle(rFrame, rId)`, the frame-collection
form, asserts the handle is set as the current helper does, sets `kDying` on
the row `idToIndexMap.at(rId)` names in the paired PostRender's
`pLifecycleFlags`, and clears the handle. `PushersPostRender::Remove` and
`HexShieldsPostRender::Remove` are removed and their callers call the helper,
so every owner releases one way. The owner helpers that now release are renamed
to say so: Blasters, Missiles, and Spaceships `ReleaseOwnedObjects`,
`OmnisInterpolate::ReleaseOwnedVisuals`.

No handle dangles: each handle has one owner (`## Context`), the release clears
it in the same call, and every owner already skips a cleared handle before a
`Sync` (for example `Omnis.cpp:176,180,470`, `Spaceships.cpp:319,347`,
`Blasters.cpp:132,140`, `Missiles.cpp:113,121`), as it must today after
`RemoveIndexableElementAndClearHandle`. The released row stays until its own
Destroy with its last synced values; nothing syncs it again.

What `kDying` does per collection:

| Collection | `kDying` | Reuses |
|---|---|---|
| Omnis, Spaceships, Missiles | Staged: removed once the death countdown ends | `pDestroyedTimes` and today's countdown tests |
| Explosions | Staged: removed once its last trail ends | `pStartTimes`, `pTrailTimes`, today's `Destroy` loop |
| Blasters | Removed at the next Destroy | nothing |
| AreaLights, Billboards, HexShields, PointLights, Pushers | Removed at the next Destroy | nothing: none has a fade (`## Context`) |
| Sounds | Removed at the next Destroy | the `StaticVoices` fade-out that already follows a removed row |
| SmokeTrails, WindTrails | Removed at the next Destroy | deposited smoke and wind stay in their simulations |
| Puffs, WindRadials | Removed at the next Destroy; nothing sets it, as they have no handle | nothing; controller expiry is unchanged |

The author adds no new fade or tail: each owned type's removal already looks
the same as today, and a staged owned death would be a new feature.

### Shared sweeps

The user's direction for shared destruction and deferred-cleanup code
(`## Context`); the helpers are the author's. Two helpers beside `DestroySweep` in `Engine/Source/Frame/FrameUtils.h`, each
a fixed-predicate `DestroySweep` over `rPostRender.pLifecycleFlags`:

- `TransferSweep(rInterpolate, rPostRender, Release)` releases rows with
  `kTransfer`; it replaces the four Transfer predicates.
- `LifecycleDestroySweep(rInterpolate, rPostRender, DeathEnded, Release)`
  releases rows with `kDestroy`, or with `kDying` and `DeathEnded(i)`.
  Each staged collection passes its current countdown test unchanged
  (`<= 0` for Omnis and Missiles, `!(> 0)` for Spaceships), so no behavior
  changes; every other caller passes a lambda returning `true`.

Every collection except Explosions honors `kDestroy` and `kDying` through
`LifecycleDestroySweep`. Omnis and Spaceships gain `kDestroy`, reached only by
an `edit_frame` write since no simulation path sets it there. The Omnis release
also calls `OmnisInterpolate::ReleaseOwnedVisuals` under `BT_CLIENT`, as
`ProcessDestroyStatusChanges` does (`Omnis.cpp:214-235`), because the
`kDying` pre-loop at `Omnis.cpp:192-200` does not see a `kDestroy` row.

AreaLights, Billboards, HexShields, Pushers, Sounds, SmokeTrails, and
WindTrails gain a PostRender `Destroy` hook, in the `ForEach*` signature
`PointLightsPostRender::Destroy` uses, that runs the sweep with
`RemoveIndexableElement` by `pIds[i]` as the release. PointLights, Puffs, and
WindRadials run the sweep in their existing `Destroy` before
`DestroyExpiredControlled`, releasing with `RemoveIndexableElement` (PointLights)
or `DestroyElement` (Puffs, WindRadials, which have no id map).

`ExplosionsPostRender::Destroy` keeps its forward loop, whose removal order is
CRC'd, tests `rPostRender.pLifecycleFlags[i] & kDying` in place of
`pFlags & kDestroysSelf`, and releases each expired trail with
`ReleaseIndexableElementAndClearHandle` in place of the removal at
`Explosions.cpp:214`. Explosions does not honor `kDestroy`, because removing a
row with live trails would orphan its smoke trails (`Explosions.cpp:199-215`).

### Release timing

A released row now leaves at the first engine Destroy after the release, not
at the owner's call. Per owner call site:

| Owner call | Phase | Owned row removed |
|---|---|---|
| Each game `Transfer` release (Blasters, Missiles, Omnis, Spaceships) | game Transfer | same tick, by the engine Destroy that follows |
| Missiles `Fall` | Update | same tick |
| Missiles `Explode` | PostCollision | same tick |
| `ExplosionsPostRender::Destroy` trail release | engine Destroy | same tick: SmokeTrails follows Explosions in `Collections()` |
| Each game `Destroy` release, including the Omnis `kDying` pre-loop and the Omnis and Spaceships pushers | game Destroy | next tick's Destroy, one tick later than today |
| Omnis `ProcessDestroyStatusChanges` (`kDestroyOmni`) | game Spawn | next tick's Destroy, one tick later than today |

The one-tick delay keeps a released light, hex shield, wind trail, smoke trail,
or sound row, client-only and outside the CRC, rendered or heard for one more
tick at its last synced values. A released pusher is CRC'd and stays in the
zones `SetupZones` builds at the next tick's Update, so it pushes for that one
tick before the Destroy removes it; both builds do the same, so CRCs and
replays stay deterministic, and the version bumps below reject frames from
before. The author recommends accepting that tick rather than having
`SetupZones` skip released rows: the owner it served is already gone or has
spent 0.25 to 0.7 s exploding with that pusher in place. No owned row needs a
synchronous removal: no phase reads a client-only owned row's presence, and no
owner re-adds a row through the handle it released (`OmnisPostRender::Spawn`
re-adds only for a live, non-dying omni, `Omnis.cpp:315,326`).

### Versions

No older file is accepted (root `AGENTS.md`):

- `OmnisPostRender` 19 to 20, `SpaceshipsPostRender` 6 to 7,
  `MissilesPostRender` 11 to 12, `BlastersPostRender` 2 to 3,
  `ExplosionsPostRender` 1 to 2, `ExplosionsInterpolate` 2 to 3,
  `PushersPostRender` 1 to 2 (`Pushers.h:85`). They move
  `game::Frame::kiVersion` (`Frame/Frame.cpp:27`) and
  `engine::kiFrameBaseVersion` (`Engine/Source/Frame/FrameBase.h:312`), so
  grid saves, replays, and mismatched clients are rejected; the `Frame.cpp`
  base literal does not change, because every collection whose CRCs or
  removal timing change also bumps its own `kiVersion`.
- The nine client-only collections own no `kiVersion` and gain none: their
  layout reaches no grid save, replay, or wire payload (`## Context`).
- `FrameInput::kiVersion` 21 to 22 (`Frame/FrameInput.h:10`): the Blasters and
  Explosions `SpawnInfo` layouts change and the Omnis and Missiles `flags`
  values lose bits.
- `engine::kiProtocolVersion` 28 to 29
  (`Engine/Source/Network/NetworkProtocol.h:83`): `Fields()` membership
  changes (game `Network/AGENTS.md` `## Status-Change Wire Format`), and the
  Pushers and game snapshot layouts change with it.

## Critical files

- `Engine/Source/Frame/Collections/CollectionLifecycle.h`,
  `Engine/Source/Frame/FrameUtils.h`
- `Engine/Source/Frame/Collections/Explosions/`: `Explosions.h`,
  `Explosions.cpp`, `ExplosionsSpawn.cpp`, `ExplosionsUpdate.cpp`
- `Engine/Source/Frame/Collections/`: `AreaLights/`, `Billboards/`,
  `HexShields/`, `PointLights/`, `Puffs/`, `Pushers/`, `SmokeTrails/`,
  `Sounds/`, `WindRadials/`, `WindTrails/`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/`: `Omnis/`,
  `Spaceships/`, `Missiles/`, `Blasters/`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp`,
  `Agent/AgentScene.cpp`, `Agent/AgentCommandsServerQueries.cpp`
- `Engine/Source/Agent/Commands/FrameEdit.h`
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`

## In scope

- `CollectionLifecycle.h`: `LifecycleFlags`, `LifecycleFlags_t`, and
  `ReleaseIndexableElementAndClearHandle`; both
  `RemoveIndexableElementAndClearHandle` forms removed.
- `FrameUtils.h`: `TransferSweep` and `LifecycleDestroySweep`.
- Explosions: `ExplosionFlags::kDestroysSelf` removed;
  `ExplosionsInterpolate::pFlags` removed with its `SharedMembers()` entry, the
  `ExplosionsUpdate.cpp:25,34` carry, the `ExplosionsSpawn.cpp:45` store, and
  its `LogDifferences` row; `ExplosionsPostRender` gains `pLifecycleFlags` in
  `Members()` and `LogDifferences`; `SpawnInfo` gains
  `LifecycleFlags_t lifecycleFlags` after `flags`, in `Fields()`;
  `ExplosionsPostRender::Spawn` stores it; `Destroy`'s test at
  `Explosions.cpp:219-221` and its trail release at `:214`; both `kiVersion`
  bumps.
- AreaLights, Billboards, HexShields, Pushers, Sounds, SmokeTrails, and
  WindTrails: the PostRender column in `Members()`, the zero in `Add`, and a
  new PostRender `Destroy`; `HexShieldsPostRender::Remove` and
  `PushersPostRender::Remove` removed; Pushers also
  `PushersPostRender::LogDifferences` and `kiVersion`.
- PointLights, Puffs, and WindRadials: the PostRender column, the zero in
  `Add` (PointLights) and `AddControlled`, and the sweep in each existing
  `Destroy`.
- Omnis: `Omnis.h` enum, column, `SharedMembers()`, `SharedCrcMembers()`,
  `PersistentMembers()`, `kiVersion`, and the `ReleaseOwnedVisuals`
  declaration (`:147`); `Omnis.cpp` `ReleaseOwnedVisuals` (`:174-185`),
  `Destroy`, `ProcessDestroyStatusChanges`, `Spawn`, `LogDifferences`, and the
  reads at `:195,315,326,422,432,493,569`;
  `OmnisCombat.cpp:226,250,257,283,370,446,468`; `OmnisNavigation.cpp`
  `Transfer`, dropping `flags.Set(kTransfer, false)` and releasing at
  `:254-256`.
- Spaceships: `Spaceships.h` enum, column, accessors, `kiVersion`;
  `Spaceships.cpp` `ReleaseOwnedObjects` (`:339-351`), `Transfer`, `Destroy`,
  `Spawn` (`:551`), `LogDifferences`, the explosion spawn at `:220`, the reads
  at `:85` (now `OmnisPostRender::pLifecycleFlags`), `:421,431,601`, and
  `.flags = {}` at `:471`; `SpaceshipsCombat.cpp:40,60,92,110,140-143,155,160`;
  `SpaceshipsNavigation.cpp:166`.
- Missiles: `Missiles.h` enum, column, accessors, `kiVersion`; `Missiles.cpp`
  `SynchronizeMissile` (a lifecycle parameter beside `flags`, both
  declarations and both call sites), `ClientInitialize`, the explosion spawn at
  `:261`, `ReleaseOwnedObjects` (`:274-293`), `Transfer`, `Destroy`, `Spawn`,
  `Fall`, `Explode`, `LogDifferences`;
  `MissilesUpdate.cpp:50-52,105,215,247,311,327-335,358,367`.
- Blasters: `BlasterFlags`, `pFlags`, and `SpawnInfo::flags` removed;
  `pLifecycleFlags` added; `Blasters.cpp` `ReleaseOwnedObjects`
  (`:129-145`), `Spawn`, `Transfer`, `Destroy`, `LogDifferences`;
  `BlastersUpdate.cpp:221-248`; `kiVersion`.
- `Frame/Frame.cpp:211,240`: the Omnis `kExploding` reads become `kDying`.
- `Frame/Frame.cpp` `ValidatePusherOwners`: the re-derived check in
  `## Coordination`; `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`
  `## Invariants`: the pusher pairing bullet is rewritten to state it.
- `Frame/FrameInput.h:10` and `NetworkProtocol.h:83`: the two bumps. If the
  status-change per-item maximum no longer covers the Explosions payload, it
  is raised.
- `Agent/Commands/ServerFrameEdit.cpp` (`OmnisFrameColumns`, `SpaceshipsFrameColumns`, `MissilesFrameColumns`, `BlastersFrameColumns`) and `Engine/Source/Agent/Commands/FrameEdit.h` (`ExplosionsFrameColumns`, `PushersFrameColumns`): the six column lists.
- `Agent/AgentScene.cpp:21-85`: `OmniFlagNames` and `SpaceshipFlagNames` take
  the lifecycle mask too and emit its names (`kTransfer`, `kDestroy`,
  `kDying`) through one shared file-static helper into the same `flags` array,
  so the `describe_scene` shape holds.
- `Agent/AgentCommandsServerQueries.cpp` `ExtractOmnis`: a `lifecycleFlags`
  integer beside `flags`.
- `commands-server.md`: the `query_omnis` row shape; the `inject_payload` key
  table (`SpawnBlaster` loses `flags`, `SpawnExplosion` gains
  `lifecycleFlags`); the `edit_frame` kill-state table: `pLifecycleFlags:2`
  (`kDestroy`) for omnis, spaceships, missiles, and blasters, staged deaths
  written with `pLifecycleFlags:4`, explosions with `pLifecycleFlags:4`, a
  `SpawnExplosion` without `lifecycleFlags` never removed, and the whole-mask
  note naming `pLifecycleFlags`; the removal paragraph's pusher clause (an
  owner's pusher is released with it and removed by the following tick's
  Destroy); and the `pushers` row: removed only through its owner, and a
  `pLifecycleFlags` write on a pusher removes it under an owner that still
  holds its handle, whose next `PushersInterpolate::Sync` throws at
  `idToIndexMap.at` (`Pushers/PushersUpdate.cpp:34`).
- `Engine/Source/Frame/Collections/AGENTS.md`: a `## Core Contract` bullet
  stating the standard column on every collection, its bits, the sweeps, and
  owner release; the `## Identity, Serialization, and CRC` bullet naming
  `RemoveIndexableElementAndClearHandle()` names the release helper instead.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/AGENTS.md`
  `## Game-Specific Rules`: the `Flags` packing bullet points lifecycle states
  at that contract.
- `Documents/Features/Graphics/OceanImpactSplashes.md:9`: `kSilentDespawn`
  becomes `kDestroy`; `Documents/Features/Audio/AdaptiveMusic.md:54,81` and
  `Documents/Features/Graphics/ShipWreckage.md:9`: `kExploding` becomes
  `kDying`.

## Out of scope

- New fades, tails, or other death behaviour for any owned collection; a
  `SetupZones` skip of released pushers.
- `StatusChangeType::kDestroyOmni` keeps removing the omni row directly in the
  Spawn phase; moving it to `kDestroy` would move that removal to the next
  tick. Its owned rows are released like every other owner's.
- `RemoveIndexableElement` removals of a collection's own rows (Omnis'
  sweeps, `DestroyExpiredControlled` callers), which consume no owner handle.
- Checking owner handle columns other than the pusher handles against the
  owned collection's id map when a grid save is read: no such check exists
  today (`Omnis.h:74,226` and `Spaceships.h:47,128` `PostRead` read no handle),
  so a save can already name a missing owned row; a saved `kDestroy` or
  `kDying` on a row a live owner still holds is one more value of that
  unchecked class.
- Controller-type `bDestroysSelf` (`CollectionController.h:42`) stays a type
  field.
- `kDestroy` handling in Explosions; any change to the staged countdown tests,
  `pDestroyedTimes` sentinels, or `PostRead` checks.
- Renumbering any unique flag bit, or renaming the per-collection enums or
  their `pFlags` columns.
- The Blasters, Missiles, and Spaceships query rows, which report no flags
  today.
- `/add-collection` and other skill files; the `Frame.cpp` version base
  literal.

## Acceptance criteria

| Criterion | Evidence |
|---|---|
| Client and server build | `/compile` BrokenEngineSandbox client and server |
| Layout invariants hold | `pwsh -NoProfile -File .agents/scripts/Test-CollectionLayout.ps1` clean |
| No old name remains | Search under `Engine/`, `Projects/`, `Documents/` (other Plans excepted) finds no `kSilentDespawn`, `BlasterFlags`, `kDestroysSelf`, `kExploding`, `RemoveIndexableElementAndClearHandle`, `PushersPostRender::Remove`, `HexShieldsPostRender::Remove`, `RemoveOwnedObjects`, or `RemoveOwnedVisuals`, and no `kTransfer` in a per-collection enum |
| Every collection carries the column | Each of the fifteen PostRender structs declares `pLifecycleFlags` in its member accessor (code read) |
| Determinism holds | `/agent-harness` replay record over live combat with deaths, owner releases, and cross-cell transfers, then playback with no CRC mismatch |
| Uniform kill write | `/agent-harness` `edit_frame` `pLifecycleFlags:2` on one omni, spaceship, missile, and blaster row: after one unpaused tick each collection's count drops by one; `pLifecycleFlags:4` on a trail-less explosion removes it |
| Staged death unchanged | `edit_frame` `pfArmors:0` on an omni: it explodes and is removed after `kDestroyTime` |
| A killed omni's owned rows leave through release | With a client connected, `edit_frame` `pfArmors:0` on an omni, then step one tick at a time with the `pause` one-tick recipe in `.agents/skills/agent-harness/references/command-reference.md` `## Server commands`: the client `query_profile` `HexShields` counter drops by one one tick after the death starts; on the tick the omni row is gone, `read_frame` `pushers` shows its pusher with `pLifecycleFlags` 4, and one tick later the `pushers` count is one lower |
| No pusher leak | Over a 2000-tick combat run with deaths and transfers, sampled every 100 ticks with the server paused: every `pushers` row with `pLifecycleFlags` 0 is the `pPushers` or `puiPushers` value of exactly one omni or spaceship, and every other `pushers` row is gone one tick later |
| No client-only owned-row leak | In a cell whose rows all came from `inject_payload`, with no other subscribed cell holding rows (the counters sum over active cells), after combat, writing every row's kill state and waiting for explosions and controller lights to expire leaves the `pushers` count and the client `HexShields`, `AreaLights`, `PointLights`, and `SmokeTrails` counters at 0 |
| Transfer unchanged | The `cross-cell.md` flagship recipe: the omni arrives in the neighbour with `kIsFlagship` set and lifecycle `0` |
| Old formats rejected | `Frame::kiVersion`, `FrameInput::kiVersion`, and `kiProtocolVersion` differ from baseline (code read) |

## Coordination

- `Documents/Plans/Game/ShipTypeSplit.md` decision 4 puts `kExploding 0x0001`
  and `kTransfer 0x0002` in its `ShipFlags` and rewrites the Omnis and
  Spaceships kill-state rows and their removal paths. Whichever lands second
  re-derives: if this Plan lands first, `ShipFlags` drops those two bits, its
  three ship collections each carry `pLifecycleFlags` with `kDying` for the
  staged death, and their pusher and wind-trail removals and the Omnis
  hex-shield removals are releases through
  `ReleaseIndexableElementAndClearHandle`; if that Plan lands first, this
  Plan's Omnis and Spaceships rows, including the owner releases, apply to its
  Fighters, Omnis, and Battleships.
- The game `Frame` read rejects a frame whose Omnis and Spaceships pusher
  handles do not pair one-to-one with its Pushers rows
  (`Frame/Frame.cpp` `ValidatePusherOwners`). A pusher this Plan releases stays
  ownerless until the next tick's Destroy, so it can be in a grid save. This
  Plan re-derives that check: an unowned Pushers row is valid only with
  `kDying` set, the count compares only rows without it, and an owned row with
  a nonzero `pLifecycleFlags` is rejected.

## Notes

- Every collection, all fifteen, carries the column, as the user decided
  (`## Context`). Owners releasing by setting `kDying` and dropping the handle
  is the user's proposal, pending the grill. Releasing types with no death
  behaviour through `kDying` is the author's reading, pending the grill; the
  per-collection `kDying` behaviour, release helper, timing, and sweep helpers
  in `## Design` are the author's.
- Memory: every collection's rows grow one byte, except Blasters, whose
  `pFlags` byte the column replaces.
