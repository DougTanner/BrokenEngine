# Frame - Game Simulation

## Overview

Game frame state extends the engine base with space-combat collections and deterministic phase hooks.

Update `../../../../Documents/Architecture/FrameUpdatePipeline.md` when phase order or participation changes.

## Invariants

- The tick phases implemented here are called by engine-owned `RunFrameTick` (`../../../../Engine/Source/Frame/AGENTS.md`). CRC-affecting fixed-tick decisions use `Frame`, `FrameInput`, `CellStaticData`, or explicit phase parameters, not `game::gpGame`, wall-clock time, or client render state; if required input is unavailable, stop and resolve ownership.
- Every game collection pair registers in the tuples in `FrameCollections.h`;
  no collection has explicit per-collection dispatch. Register new collections
  in the correct tuple and preserve deterministic ordering: the three ship types
  follow in `ShipType` order, which `ForEachShipType`, the registry source
  layers, and `Frame::OwnershipLayers` share. `FrameInterpolate::Values()` and
  `FramePostRender::Values()` list the game frame-wide values under the engine
  `Values()` rule (`../../../../Engine/Source/Frame/AGENTS.md`
  `## Collection and Serialization Invariants`).
- `SharedCrcMembers()` is a subset of `SharedMembers()`. Full `Write()`/`Read()` is build-local; `ServerRead()` consumes the shared cross-build format. Both `Read()` and `ServerRead()` require exact Interpolate/PostRender count and capacity parity across every registered collection pair and rebuild ID maps (`../../../../Engine/Source/Frame/Collections/AGENTS.md`); build-local reads replace the destination only after the full frame passes that check.
- `Frame::kiVersion` composes navigation and collection versions. Bump its base when CRC semantics change without a contributing version bump.
- At save/replay read boundaries, reject a non-finite ship-spawn timer or one at or above the shared 0.5-second spawn interval as a corrupt stream, and leave smaller finite values unchanged before the loop that repeatedly subtracts the interval.
- Random-spawn waves cap enemy population per cell and per ship type: a wave adds at most the cell's non-dying fleet ships of its type minus its non-dying random-spawn ships of that type. The cap only limits new spawns; random-spawn ships are never removed when the fleet shrinks or leaves the cell.
- At the same read boundaries, reject as a corrupt stream a frame whose Fighters, Omnis, and Battleships `pPushers` handles do not pair one-to-one with its Pushers rows, or any of whose Pushers rows carries a nonzero `pLifecycleFlags`, or that holds a flying missile — neither falling nor dying — whose height is no flight plane's or whose velocity Z is nonzero.
- Navigation and elevation are deterministic derived data excluded from both the CRC and persisted frame payload. The game derives the clearance distance and, from each flight plane's height, that plane's elevation threshold for the engine's contour bake (`TerrainUtils.h`, server-only) and hands them to engine startup, the only way engine navigation gets them. The engine reads the game's immutable flight-plane table (`FlightPlaneTable.h`, both builds) directly at boot.
- Status changes are consumed here: `kUpdateShip`, `kUpdateFleet`, and `kDestroyShip` in Update, after every ship Update; spawn and transfer status changes in the post-tick batch, outside the tick phases, through `ApplySpawnStatusChange` (`StatusChange.h`). Their serialized tags and wire rule are owned by game Network (`../Network/AGENTS.md`).
- `FrameInput` — the per-tick deterministic unit, a versioned serializable list
  of `StatusChange`s — is declared here (`FrameInput.h/.cpp`) and serialized by
  the engine stream functions in `FrameBase.h`, compiles in both builds, and
  feeds the tick pipeline, server broadcast, and replay
  (`../../../../Engine/Source/File/AGENTS.md`). Instances are never built from
  hardware input; the server, client reconciler, and replay reader generate
  them. Bump `FrameInput::kiVersion` on any change to the bytes a server build
  writes — the stream format, the `StatusChange` payload, or the replay
  difference stream's post-dispatch channel layout, whose records carry
  `FrameInput` payloads. The version is the only guard: replays validate it, and
  the difference stream checks no payload size. A `BT_CLIENT`-only member
  persists no byte and takes no bump; a wire-visible change also takes the
  separate `engine::kiProtocolVersion` bump owned by `../Network/AGENTS.md`.
  `Crc()` serves the replay `DifferenceStream` (skipping identical consecutive
  differences) and diagnostics; reconciliation desync detection compares `Frame`
  CRCs, not `FrameInput::Crc()`.
- Variant read: reading a `StatusChange` must seat the correct `std::variant`
  alternative (via `DefaultDataForType`) before `common::Read` runs —
  `std::visit` assumes the active alternative already matches the type tag.
  After each payload read the engine replay reader (`engine::ReadFrameInput`,
  `../../../../Engine/Source/Frame/AGENTS.md`) calls
  `AdmitFrameInputStatusChange`, which rejects a payload the game cannot adopt:
  an unregistered Blaster or Explosion type index, an out-of-range ship
  controller or ship type, or a target or stored flight plane the ship type
  disallows (`IsAdoptableStatusChange`), or a fleet-controlled ship spawn
  payload with global ID 0.
- Delayed behavior updates use countdowns that transfer with the entity; transfer, spawn, and destroy status changes apply immediately.
- Missile and omni homing acquire through the engine frame registry (`../../../../Engine/Source/Frame/AGENTS.md` `## Frame Registry`). This scope builds its only two query windows — the Missile-Update window in Missiles Update and the Omni-Spawn window at omni spawn — as specified in Frame Registry Query Windows (`../../../../Documents/Architecture/FrameUpdatePipeline.md`).
- Acquiring a target writes the frame — it takes a subscription — so the order acquisitions happen in within a tick is part of the deterministic stream. Do not hoist one out of a loop, cache its result, or run it in parallel across entities.
- `Frame::OwnershipLayers` maps the engine registry's ownership layer onto each ship type's PostRender columns, one layer per type; `Frame::OwnedEntityCount` counts a frame's fleet-controlled rows.
- Cell containment uses `engine::IsInsideCell`; a transfer producer calls `PrepareTransferRequest` only for a position that fails it, because `CellExitDirection` names no direction for an inside position.
- `GetServerCellStatistics` and its row labels `kServerEntityCountNames`
  (`ServerCellStats.h`, server-only) are the cell-statistics names the
  engine-owned server monitoring window consumes
  (`../../../../Engine/Source/Server/AGENTS.md`): it reads one cell's frame and
  returns its tick, time, and collection populations by value, the populations
  as `entityCounts` rows indexed by `ServerEntityCount`. Keep it observational —
  no stored state, no display knowledge — and add fields here rather than
  opening a second display path into game collections.
- `PublishServerEntityCounts` (same header) is the server's only writer of the Fighters, Omnis, Battleships, Blasters, and Missiles profile counters, which the client publishes from the render path instead. It runs once per advancing tick from the post-tick client-finalize hook (`../Network/Server/AGENTS.md`), so totals describe the tick that just finished; the monitoring window must not become a second writer.

## See Also

- `../../../../Documents/Architecture/GameReconciliation.md`
