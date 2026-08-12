<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-11T00:00:17.000Z","dependsOn":["Documents/Plans/Frame/EngineOwnedFrameConstants.md"]} -->
# Hoist RunFrameTick into engine FrameBase

## Context

`RunFrameTick` (`Projects/BrokenEngineSandbox/Source/Frame/FrameTick.cpp:12-90`, declared `FrameTick.h:20`) is the one function that advances a cell one deterministic tick, shared by server simulation and client replay. Every part of it is engine machinery:

- Preamble: `common::FrameTickScope` (arming the guard that makes a stray render-path `GlobalElevation`/`GlobalNormal` call fail fast), MXCSR verification of `_DN_FLUSH` and `_RC_NEAR`, server-only lazy `engine::BuildCellNavData`, lazy `engine::gpIslandTerrain->BuildElevationGrid`, and the client-only `BuildRenderPlacementCache` — all under `ScopedSuppressAllocationTracking`.
- Phase sequence (`:65-86`): Interpolate `AllocateAndCopy`/`Update` plus the `iTick`/`fCurrentTime` stamps, PostRender `AllocateAndCopy`/`Update`, `PreCollision`/`engine::Collision::Collide`/`PostCollision`/`AreaDamage`, `Transfer`, then `Destroy`/`Spawn`.
- The shared CRC stamp `rNext.postRender.sharedCrc = rNext.Crcs();` (`:89`).

The engine already declares every one of those phases: `FrameInterpolateBase::AllocateAndCopy`/`Update` at `Engine/Source/Frame/FrameBase.h:57-58` and the eight `FramePostRenderBase` phases at `:162-169`, all taking `game::` parameter types, and `Engine/Source/Frame/FrameBase.cpp` already includes the game `Frame.h` (`FrameBase.cpp:3`). `ActiveFrameRef` (`FrameTick.h:11-17`) is four pointers with no game content, and `FrameBase.h` and `FrameUtils.h` already forward-declare the `game::Frame`/`game::FrameInput` types its members name.

Exactly two call sites exist, verified by grep: `Engine/Source/GameBase.cpp:294-311` (builds `game::ActiveFrameRef` entries in the workbuffer and calls `game::RunFrameTick` from the per-coord dispatch) and `Projects/BrokenEngineSandbox/Source/Network/Client/ReconcileReplayTick.cpp:120-126` (client replay).

## Design

Move `ActiveFrameRef` and `RunFrameTick` into `Engine/Source/Frame/FrameBase.{h,cpp}` in namespace `engine`, verbatim: same statement order, same build guards, same allocation-suppression scopes, same comments. The declarations go in `FrameBase.h` (`ActiveFrameRef`'s member types requalify to `game::Frame*`/`game::FrameInput*`, matching the existing forward declarations); the definition goes in `FrameBase.cpp`, which carries over the includes `FrameTick.cpp` holds today (`Frame/FrameStaticData.h`, `Frame/IslandTerrain.h`, `Frame/NavBuild.h`, `Frame/FrameCollections.h` — `FrameTick.cpp:3-7`). The phase calls stay spelled against the game types (`game::FrameInterpolate::Update`, `game::FramePostRender::Spawn`, and so on); the game keeps supplying those definitions. Inside the body, `engine::` qualifiers drop and the game type names gain `game::`.

`kfDeltaTime` at `FrameTick.cpp:67` is engine-owned by the time this plan runs — the `dependsOn` edge on `Documents/Plans/Frame/EngineOwnedFrameConstants.md` exists precisely so the hoist lands with the constant already in `engine`.

Call-site edits: `GameBase.cpp` drops the `game::` qualifier at `:294`, `:301`, `:311` and replaces `#include "Frame/FrameTick.h"` (`:10`) with `#include "Frame/FrameBase.h"`; `ReconcileReplayTick.cpp` requalifies `ActiveFrameRef`/`RunFrameTick` to `engine::` at `:120-126` and replaces its `Frame/FrameTick.h` include (`:4`) the same way. The code comment at `Engine/Source/Frame/IslandTerrain.h:153` says "(see FrameTick.cpp)" and is updated to name `FrameBase.cpp`. Deleting `FrameTick.{h,cpp}` removes them from both game projects — `BrokenEngineSandbox.vcxproj` and `BrokenEngineSandboxServer.vcxproj` plus their `.filters` (there are no separate engine project files; `FrameBase.{h,cpp}` are already members) — so `/update-vcxproj` is triggered. The `BT_SERVER` nav-data block and `BT_CLIENT` render-cache block move with their guards; no other guard changes.

`Documents/Architecture/FrameUpdatePipeline.md` is updated in the same change, because it links `RunFrameTick` to the deleted `FrameTick.cpp` path (`FrameUpdatePipeline.md:7`).

## Critical files

- `Engine/Source/Frame/FrameBase.h`, `FrameBase.cpp` — new home
- `Projects/BrokenEngineSandbox/Source/Frame/FrameTick.h`, `FrameTick.cpp` — deleted
- `Engine/Source/GameBase.cpp` and `Projects/BrokenEngineSandbox/Source/Network/Client/ReconcileReplayTick.cpp` — the two call sites: requalification and include swap
- `Engine/Source/Frame/IslandTerrain.h:153` — stale file-name comment
- `Documents/Architecture/FrameUpdatePipeline.md`, `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`, `Engine/Source/Frame/AGENTS.md`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj`, `BrokenEngineSandboxServer.vcxproj`, and both `.filters` — file membership

## In scope

- Moving `ActiveFrameRef` and `RunFrameTick` verbatim into `engine`, with both `BT_SERVER` and `BT_CLIENT` guarded blocks intact, carrying `FrameTick.cpp`'s includes into `FrameBase.cpp`
- Requalifying the two call sites, swapping their `FrameTick.h` includes, and updating project/filter membership for the two deleted files
- Updating the `IslandTerrain.h:153` comment, `Documents/Architecture/FrameUpdatePipeline.md`, and the two Frame `AGENTS.md` files for the ownership move

## Out of scope

- Any reordering, addition, or removal of a phase, and any change to where the CRC is stamped
- Any change to the lazy-build conditions (`bNavDataBuilt`, `elevationGrid.empty()`, `islandRenderQueries.empty()`, and the `islands.empty()` guards) or to which build they run on
- The phase implementations themselves, which stay in the game
- Moving `Frame::Crcs`, the collection tuples, or `FrameCollections.h`
- Moving the frame constants — owned by `Documents/Plans/Frame/EngineOwnedFrameConstants.md`
- Moving the terrain traces — owned by `Documents/Plans/Frame/TerrainTraceToEngine.md`

## Risk tier and invariants

Tier 3 — this is the deterministic tick itself. Invariants, all documented in `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`: Interpolate precedes PostRender update, collision, transfer, destroy, and spawn; the shared CRC is stamped after those phases and before the server transfer harvest, which then recomputes destination CRCs; the elevation grid is built before any sim phase so every `FrameElevation`/`FrameNormal` caller this tick sees a populated grid; `FrameTickScope` is armed for the whole tick; the MXCSR assertions run before any deterministic state advances; nav data is built on the per-coord dispatch thread on the server only, and never on the client, which receives it prebuilt; the render placement cache is client-only and stays out of the CRC.

## Acceptance criteria

- Client and server compile; `RunFrameTick` exists once, in the engine, and `FrameTick.{h,cpp}` are gone.
- A replay determinism run reproduces bit-identical per-tick CRCs against a replay recorded before the change.
- A client/server harness session runs with no desync warnings, and a cross-cell transfer still produces matching CRCs on the receiving cell.
- Diff review confirms the moved function body is statement-identical — only namespace qualifiers changed — with the CRC stamp still the final statement and both build-guarded blocks intact.
- `Documents/Architecture/FrameUpdatePipeline.md` matches the code after the move.

## Scores

Effort 2 / Impact 3 / Risk 3
