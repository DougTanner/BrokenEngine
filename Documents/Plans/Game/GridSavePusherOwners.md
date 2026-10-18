<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-18T19:53:04.852Z","dependsOn":[]} -->
# Reject a frame read whose pusher handles do not pair one-to-one with Pushers rows

Tier 2 (`.agents/references/risk-tiers.md`): the change adds a check inside one
unit, the game `Frame` read, at the existing grid-save and replay read
boundary, with the format and the trust unchanged; no version bump. Line
numbers cite baseline `15df91fc96670b336ba4ba1dc007023587da0d28`; game paths
are relative to `Projects/BrokenEngineSandbox/Source/`.

## Context

The root `AGENTS.md` trust policy has the server fully validate every grid
save, answering a bad value by failing the load
(`.agents/references/cpp-conventions.md`, "Never fix a bad value
automatically"; `Engine/Source/File/AGENTS.md` `## Grid Saves`). The grid
save does not check the owner pusher handles:

- Owners: every Omnis row holds one pusher in `OmnisInterpolate::pPushers`
  (`Frame/Collections/Omnis/Omnis.h:92`) and every Spaceships row one in
  `SpaceshipsInterpolate::puiPushers`
  (`Frame/Collections/Spaceships/Spaceships.h:69`). Both are persisted
  `Members()` columns (`Omnis.h:107`, `Spaceships.h:80`). They are the only
  `PushersPostRender::Add` callers (`Omnis/Omnis.cpp:357-358`,
  `Spaceships/Spaceships.cpp:539-540`), each run in the owner's Spawn, and every
  owner removal (`Omnis.cpp:207-210`, `:229`,
  `Omnis/OmnisNavigation.cpp:254`, `Spaceships.cpp:345` from `Transfer` and
  `Destroy`) removes the pusher and the owner row together. So between ticks
  every owner handle is nonzero, names a Pushers row, no two owners share one,
  and the Pushers count equals the Omnis plus Spaceships counts.
- The read checks none of that. `ReadGridSave` reads each frame through
  `operator>>` (`Engine/Source/File/GridSave.cpp:138`;
  `Frame/Frame.cpp:734-745`), which runs `ValidateCollectionPair` per pair:
  count and capacity parity, and `RebuildFromIds` rebuilding each collection's
  own id map from its own `pIds`, rejecting a zero or repeated id
  (`Engine/Source/Frame/FrameUtils.h:394-405`,
  `Engine/Source/Frame/Collections/Collection.h:177-195`). The `PostRead` hooks
  see one struct's columns (`Collection.h:73-95`); the Omnis and Spaceships ones
  (`Omnis.h:74,226`, `Spaceships.h:47,128`) read no handle.
- What a bad handle does after the save is adopted, outside the
  `ReadGridSave` catch (`GridSave.cpp:167-171`) that turns a read failure into a
  failed load:
  - A nonzero handle naming no Pushers row: the owner's Update sync
    (`Omnis.cpp:442`, `Spaceships.cpp:316` through `SyncSpaceship` at `:168-178`)
    calls `PushersInterpolate::Sync`, whose `idToIndexMap.at(id)` throws
    `std::out_of_range` (`Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp:34`)
    in the first tick; `Multithreading::Dispatch` rethrows it
    (`Common/Threading/Multithreading.h:116-119`).
  - A zero handle: `Sync` skips it (`PushersUpdate.cpp:28-31`), and the owner's
    removal reaches `RemoveIndexableElementAndClearHandle`, whose `ASSERT`
    (`Engine/Source/Frame/Collections/CollectionLifecycle.h:105`) fires, or in a
    build without asserts `RemoveIndexableElement`'s `idToIndexMap.at` throws
    (`:83`).
  - Two owners naming one pusher: the second owner's removal throws at the same
    `idToIndexMap.at`.
  - A Pushers row no owner names: never synced or removed, it keeps pushing
    from its saved position for the rest of the game.
- The same `operator>>` also reads replay full frames
  (`Engine/Source/File/DifferenceStream.h:557`, failure caught at `:560`) and
  commits every `edit_frame` (`Agent/Commands/ServerFrameEdit.cpp:473`).
  `Frame::ServerRead` (`Frame.cpp:717-725`) reads server-to-client snapshots,
  which the client trusts.

## Design

The author recommends one file-static check in `Frame/Frame.cpp`, beside
`AdmitSpawnTimer` (`:15-22`), called by `operator>>` on `loadedFrame` after the
`ValidateCollectionPairs` call at `:742`, once every id map is rebuilt, and
before the move into `rCurrent`. It throws
`std::ios_base::failure("Frame pusher owners")` when:

1. an Omnis `pPushers` or Spaceships `puiPushers` value is zero;
2. a value is absent from `interpolate.pushers.idToIndexMap`;
3. two owner rows name the same pusher; or
4. `pushers.iCount` differs from the Omnis count plus the Spaceships count.

Given 1 to 3, check 4 is what rejects an unowned pusher. A per-index seen
vector the size of the Pushers count detects 3; build it under
`ScopedSuppressAllocationTracking`, as `RebuildFromIds` does on the same read
path. `std::ios_base::failure` is the exception both read-boundary catches
take (`GridSave.cpp:167`, `DifferenceStream.h:560`), as the `PostRead` hooks
use. `Frame::ServerRead` gets no check, under the trust policy.

The check lives game-side because the owners are game collections; the engine
Pushers collection knows no owner. No format, CRC, or version changes: a save
this build writes passes the check, and nothing it rejects was ever valid.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`

## In scope

- `Frame/Frame.cpp`: the new file-static check and its call in
  `operator>>(std::istream&, Frame&)` (`:734-745`).
- `Frame/AGENTS.md` `## Invariants`: one bullet beside the spawn-timer read
  bullet stating that save and replay reads reject a frame whose Omnis and
  Spaceships pusher handles do not pair one-to-one with its Pushers rows.
- `commands-server.md`: the `edit_frame` sentence listing "the save reader's
  checks" adds the pusher-owner check.

## Out of scope

- `Frame::ServerRead` and every client-side read.
- Other owner handle columns: the client-only `pWindTrails`, `pHexShields`,
  `puiWindTrails`, and the Missiles and Blasters light, trail, and sound
  handles, which no server read carries; and the registry ids
  (`puiRegistryIds`, `puiRegistryTargets`).
- Making the tick-time `idToIndexMap.at` lookups tolerate a missing id, and
  any change to `ReadGridSave`, `PostRead`, `RebuildFromIds`, or a format or
  `kiVersion`.

## Acceptance criteria

| Criterion | Evidence |
|---|---|
| Client and server build | `/compile` BrokenEngineSandbox client and server |
| Each bad case fails the read | Code read: the check rejects each of the four conditions in `## Design` by throwing `std::ios_base::failure`, and `operator>>` calls it before `rCurrent` is assigned |
| No valid frame is rejected | `/agent-harness`: after combat with omni and spaceship deaths and a cross-cell transfer, `save` then `load` succeed; a `replay_record` over the same activity plays back without a full-frame read failure; an `edit_frame` value write on an omni commits |

## Coordination

- `Documents/Plans/Engine/CollectionLifecycleFlags.md` makes an owner release
  its pusher by setting `kDying` in a new `pLifecycleFlags` column, leaving the
  ownerless row until the next tick's Destroy, so such a row can be in a grid
  save. Whichever lands second re-derives this check: an unowned Pushers row is
  then valid only with `kDying` set, check 4 counts only the rows without it,
  and an owned row with a nonzero `pLifecycleFlags` is rejected, which closes
  the unchecked value that Plan's `## Out of scope` names.
- `Documents/Plans/Game/ShipTypeSplit.md` replaces the Omnis and Spaceships
  collections with Fighters, Omnis, and Battleships, each holding a pusher
  handle. Whichever lands second makes the check cover every ship collection's
  pusher column and count.

## Notes

- Found while planning `CollectionLifecycleFlags.md`, which lists this gap in
  its `## Out of scope`.
- `edit_frame` refuses writes to handle columns
  (`commands-server.md`, the `edit_frame` identity-column refusal), so no live
  recipe produces a bad handle; the rejection evidence is the code read.
