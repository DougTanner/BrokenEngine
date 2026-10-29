# Frame - Base Simulation State

## Overview

Engine-owned frame state, fixed-tick phase orchestration, per-cell static data, collision, terrain sampling, navigation, and generic collections.

## Architecture

- `RunFrameTick` and `ActiveFrameReference` are engine-owned; the phase bodies are game-implemented (the game project's own `Source/Frame/AGENTS.md`). `RunFrameTick` verifies the required floating-point environment before advancing deterministic state, and the shared CRC is stamped after the tick phases and before server transfer harvest.
- `FrameInterpolateBase` owns continuous state used for fixed-tick interpolation; `FramePostRenderBase` owns committed deterministic state and per-tick queues. Fixed-tick Interpolate state may feed `FramePostRenderBase` and shared CRC state; shared PostRender/CRC state must not derive from a client render-interpolated frame or client-only collection member. Stop when a proposed source crosses or obscures this boundary.
- `FramePostRenderBase::transferRequests` is the base frame's only data member
  that names a game type, a deliberate exception to the engine/game contract:
  the cross-cell payload `game::TransferRequest` is game-defined, while the
  per-tick queue and the producer calls that fill it are engine-owned. The queue
  stays outside `Collections()`, CRC, serialization, and difference logging.
  `PrepareTransferRequest` rewrites a request's position into the destination
  cell's local frame once, and no consumer downstream converts it again.
- A simulation instant (`SimulationTime`, `TimeStep.h`) is integer
  nanoseconds, and no clock is stored: compute the current instant from the tick
  in hand through `TickToSimulationTime`. Convert a float length with
  `common::FloatSecondsToNanoseconds` before adding it to an instant or
  comparing it with an instant difference, and convert a difference once with
  `common::NanosecondsToFloatSeconds` before float math; chrono's implicit
  common type of `nanoseconds` and `duration<float>` compiles silently into
  float nanoseconds.
- Each client conversion point rebases a cell-local position into the camera
  cell's frame through a `RenderBasis` (`GridCoord.h`): collection render bodies
  read the per-cell one the render entry point stamps on the interpolate copy,
  and every other conversion point builds its own with `MakeRenderBasis`
  (`../Graphics/CameraBase.h`); none is published render-wide. Convert once,
  where the position reaches the GPU or a cross-cell query; never rewrite the
  position columns. The basis is client-only and stays out of CRC,
  serialization, and difference logging.
- Static cell data derives from the grid coordinate and packed island assets;
  elevation is shared, while navigation is server-built. Every cell is the same
  flat-top regular hexagon, 900 m tall (`kfCellApothem` 450 m), centred on its
  local origin, and `GridCoord` holds its axial coordinates (`iQ`, `iR`). The
  cell shape is one constant stored nowhere, the `kCellDirections` table
  (`GridCoord.h`): its six edge neighbours in the fixed order
  `common::HexDirections` (`../../../Common/Math/MathUtils.h`) defines, each
  with its axial delta and centre offset. Containment, exit, transfer direction, and
  centre offsets all evaluate that table through the `FrameUtils.h` wrappers
  (`IsInsideCell`, `TracePointToCellExit`, `CellExitDirection`,
  `CellCenterOffset`), and adjacency (`GridDistance`) is pure axial arithmetic,
  so no save file or network payload carries a cell shape to validate. Form a neighbour or
  transfer-destination coordinate through `TryAddGridCoordinate`
  (`FrameUtils.h`), which may report that none exists.
- Collision and area-damage queues are thread-local because coord ticks may run
  in parallel. The `thread_local` containers in `Collision.cpp` and
  `AreaDamage.cpp` are `common::StableVector`s (`../../../Common/AGENTS.md`)
  sized on first use, with no tracking suppression: their constructors run
  during mimalloc's `mi_process_init`, before the allocator is ready, so
  allocating there would crash at process start. An overflow site keeps its
  `DEBUG_BREAK` naming the pre-allocate constant to raise. The exception is the
  collision layers, whose count is fixed at compile time: a `std::vector` sized
  once under `ScopedSuppressAllocationTracking`, whose overflow logs `kError`
  and asserts rather than growing.
- An entity's position must lie inside its own cell's hexagon
  (`IsInsideCell`). A position on a shared edge or vertex belongs to the cell
  whose `IsInsideCell` accepts it, by the tie-break rule `IsInsideHex`
  (`../../../Common/Math/MathUtils.h`) owns; a position outside the hexagon
  belongs to the neighbour `CellExitDirection` names, so an
  out-of-cell entity transfers instead of being simulated locally. The
  elevation, collision, and pusher grids span the hexagon's bounding rectangle
  (`kfCellHalfWidth`, `kfCellHalfHeight`), whose corners outside the hexagon
  belong to the neighbours: they hold no island placement, no elevation sample
  but the sea floor, and no entity this cell spawns, and the zone grids there
  hold only this cell's own rows. Each collection's `Spawn(SpawnInfo)` tests the
  position before appending anything. An overload either refuses an outside
  position silently, never clamping, requeuing, or leaving the row to the
  transfer path, and returns whether it appended, or, where an outside position
  is a bug, asserts instead and always reports success. No caller is exempt. A
  producer must test `IsInsideCell` itself before calling `Spawn` when a side effect
  that cannot be undone precedes the append, or when the position comes from
  outside the simulation rather than another cell's transfer, because an
  asserting overload asserts rather than refusing.
- The produce and consume windows are fixed, not merely ordered: area damage is added during PostCollision and queried during AreaDamage, and `RunFrameTick` clears each queue right after the game's body for its owning phase returns, so every engine and game collection has run that phase first. Adding from AreaDamage, or querying from Update, silently drops the hit or delivers it a tick late.
- Collision gathers candidate events across every layer pair, sorts them
  globally by time of impact with layer and object indices as tiebreakers so the
  order is total and reproducible, and commits each accepted event to both
  sides. An object flagged destroy-on-collide accepts only its earliest event.
  Per-object maximum-time cutoffs are exclusive, reserving exact-time ties for
  terrain and cell-boundary outcomes. Collision owns those cutoffs and the
  outcome, so collections trace neither (`Collision::AddLayer`,
  `Collision::GetOutcome` in `Collision.h`). Per-pair collision masks must be
  bi-directional and same-layer collision is unsupported; both are asserted.
- Large terrain/nav allocations are lazy. Do not make frame constructors allocate proportional to world or mesh size.
- `FlightPlanes` reads the game's immutable flight-plane table directly at boot on both builds; its lowest plane is the base height every simulation and render consumer reads. Construction asserts that the table holds 1 to `kiMaxFlightPlanes` planes with positive, strictly ascending heights, then that every island template's header elevation ceiling stays below the highest plane, so it follows `IslandTerrain` construction.

## Frame Registry

- The frame registry (`FrameRegistry.h`) lets one collection find rows of another by binding columns the collections already own: it copies no records, allocates nothing, and keeps no state between query windows, so nothing it produces is serialized, CRC'd, or transferred.
- The game builds both query windows and owns their timing, bound columns, and batch storage (`../../../Documents/Architecture/FrameUpdatePipeline.md`); the engine never names a game collection.
- The caller owns one combined workbuffer reservation per query window, holding
  the source-row indices, registry scratch, and layer descriptors, and keeps it
  alive until the context is destroyed; form every pointer and span the context
  uses only after the reservation returns. A workbuffer grow while a context is
  live keeps the context's published pointers valid and only signals an
  under-sized reservation (`../../../Common/AGENTS.md`). `RegistryScratchBytes`
  sizes only the eligible-row and subscriber-count storage, whose `uint16_t`
  counts assert before the 65,536th subscription so ranking state cannot wrap.
  `RegistryBatch::rows` and `results` are caller-provided. A context never
  survives source-layer movement, reallocation, or cell transfer.
- Source ids are unique across every bound layer and eligible row spans are strictly ascending and in range; debug builds validate both when the context is built.
- Acquisition is a deterministic direct scan feeding CRC'd state: one fixed ranking — fewest subscribers, then smallest angle, exact ties by layer order then row order — always bounded by the caller's radius. Alignment filtering needs the source and consumer alignment columns both bound.
- `AssignRegistryClientGuid` is the registry's only write. It is legal only on the main thread after the tick, through an ownership layer whose client-GUID column is CRC-excluded. The caller holds that layer by value and never retains it across a tick, a transfer, or a reallocation.

## Collection and Serialization Invariants

- `Collections()` order is a dependency order, not just a type list. A producer must precede an owned consumer; Explosions before SmokeTrails is the live example. Tuple-size and client/server type checks do not verify this ordering.
- `Write()`/`Read()` walk full `Collections()` and are build-local. Cross-build server snapshots use `ServerRead()` with `ServerCollections()` and shared members. Preserve tuple order, type parity, and member wire order together.
- A frame-wide value missing from its frame struct's `Values()` is never persisted. `Crcs()` and `LogDifferences()` list the values other than the `kbClientOnlyValue` ones by hand and change together with the list.
- `ReadFrameInput` bounds the `statusChanges` count against the stream before resizing and rejects an unknown `StatusChange` type tag before seating the variant alternative, because a wrong alternative reads the wrong payload size and silently desyncs the rest of the stream; after each payload read it hands the change to the game's admission check (the game project's own `Source/Frame/AGENTS.md`).

## Terrain and Navigation

- An island-free cell is a supported state whose elevation samples all return the sea floor (no grid is built) and whose navigation data is built empty.
- World cells place and rotate islands deterministically from independent RNG
  streams seeded from the grid coordinate with distinct multipliers. Preserve
  each stream's purpose and load-bearing draw order: separate streams keep one
  category from directly consuming another's RNG state, but placement control
  flow can still change later draws. The generator caps each cell at 107
  placements; static-data deserialization rejects a larger count before
  resize/publication, and client rendering derives its aggregate placement arena
  from that cap and the active-cell subscription slots plus one local
  unconfirmed cell. Every placement's hull lies inside the cell's hexagon inset
  by the generator's edge margin. Keep world/local transforms, the placement
  bound, and the west/east/north/south convention consistent across CPU
  sampling and shaders.
- Two terrain-sampling paths exist and must not be mixed. Simulation code calls `CellElevationSampler::Sample`/`CellNormal`, which read only the cell's own `CellStaticData` elevation grid, never `mCells` or a neighbouring cell, so parallel per-cell ticks cannot race the tick-time grid build. Render code reaches the other path through the client-only `ProjectToFlightPlane`/`ProjectToBaseHeight`, whose elevation query walks `mCells` and must never run from frame-tick code; it asserts on it.
- `ResolveDiscAgainstTerrain`, the simulation's swept-disc terrain query,
  resolves an overlapped start with the navigation snap (its declaration comment
  in `TerrainCollisionUtils.h` owns the snap and cell-exclusion behavior), but
  navigation polygons do not match the elevation grid: they come from inflated,
  simplified template contours the server traces once at startup at a different
  height and builds into each cell's polygons on its first tick, and polygons of
  different placements may overlap. A snapped start can therefore still overlap,
  and a start outside every polygon is not moved; the query searches no further.
- CPU and GPU terrain elevation deliberately disagree below sea level:
  `TerrainElevation.frag` applies a depth-compression curve to undersea samples
  that no CPU path mirrors, while the CPU folds raw max-blended samples from
  packed height data. The texture feeds downstream shaders only and is never
  read back, so the disagreement never reaches the simulation or the CRC. Do not
  port the curve into `CellElevation`: it would change CRC'd sim positions for a
  purely visual effect. Do not assert stronger guarantees about the texture's
  sampling format here.
- Client terrain residency keeps the shared heightmap and hull resident but
  decommits each template's CPU mesh slice whenever it is not being restored.
  Coordinate texture and mesh restoration or eviction only in the renderer's
  bindless write epoch after all pending adds and removes are processed; this
  visual residency stays outside the CRC. A template's elevation image is a raw
  copy of the resident heightmap: on a distinct transfer queue the upload thread
  uploads it after first mint and restoration waits until it is adopted;
  otherwise restoration records it. Eviction retires images and mesh ranges, so
  arena room returns only when retirement frees them; do not evict another
  template for arena room while earlier evictions are still retiring. Renderer
  teardown invalidates its mesh allocations without cancelling File-owned
  range-reload state.
- Each flight plane has its own template contours and cell graph, index-parallel with the planes (`CellStaticData::navigationData`); a plane without `kTerrainCollision` gets an empty one. A caller picks the plane by passing that plane's graph, and queries take waypoint Z from the query position.
- The obstacle visibility graph is built once per cell, in that cell's local meters, over every obstacle polygon the cell contains; island templates contribute contour geometry only, because per-template building would yield disjoint per-island graphs with inter-island routes unreachable. Because the graph spans the cell, an A* miss is unexpected: disconnection is the usual cause, and a found path whose first waypoint coincides with the start also reports as a miss.
- The build path and the runtime query share one segment-blocked and one point-in-polygon predicate through `NavBuildInternal.h`. Keep them shared: a divergent epsilon or boundary rule lets the graph claim a segment the query rejects, threading paths through obstacles.
- Navigation data arrives only from the server, which built it, so its deserialization trusts it under the corrupt-input policy (`../Network/AGENTS.md`); the acceleration build, runtime queries, and debug nav rendering must not validate it either.
- The contour bake takes its clearance distance and per-plane elevation thresholds from the caller (the game derives both and the server passes them in at startup) and forwards them unchanged; engine navigation code must not reach back into game constants for them.
- Frame and gameplay geometry uses meters (one meter per engine unit); UV is only the normalized template representation. For navigation contours, union in UV, convert to centered local meters with the template's anisotropic footprint, apply clearance and simplification there, then map back to UV for storage; a scalar offset in UV makes the margin in meters depend on the footprint.
- Nav polygons in the cell frame wind clockwise, because placing a template mirrors Y; the template's own UV-space contour winds counter-clockwise. Derive orientation-dependent tests such as vertex convexity from measured winding rather than assuming the counter-clockwise convention the placement hulls carry.
- Navigation version changes invalidate derived data and contribute to the game frame version.
