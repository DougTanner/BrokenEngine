<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-11T00:00:01.000Z","dependsOn":[]} -->
# Move world-cell geometry, tick constants, and the coord RNG seed into the engine

## Context

Engine translation units link against `game::` symbols for world geometry and tick timing, which violates the engine/game contract in `Engine/Source/AGENTS.md` ("Engine code may consume game types... an engine-owned shared/public member... must not name a game-only concept"). These are not game concepts at all — they are the engine's own cell size, elevation-grid resolution, tick period, and per-coord RNG seed mixer, which happen to be declared in game headers.

Verified engine consumers of game symbols:

- `Engine/Source/Frame/IslandChainPlacement.cpp:296-299` (`game::Frame::kfCellWidth/kfCellHeight/kfBaseAreaMinX/kfBaseAreaMaxY`) and `:306-310` (five `game::SeedFromGridCoord` RNG streams).
- `Engine/Source/Frame/IslandTerrain.cpp:252-255`, `:424-426`, `:490-494`, `:512-515`, `:545-547` (cell extents and `kiElevationGridDim`).
- `Engine/Source/Frame/TimeStep.cpp:38`, `:47`, `:55`, `:56`, `:68` (`game::kTickNs`).
- `Engine/Source/GameBase.cpp` — `game::kfDeltaTime` reads on sixteen lines (`:107`, `:110`, `:179`, `:193`, `:218`, `:240`, `:382`, `:401`, `:408`, `:411`, `:412`, `:417`, `:436`, `:438`, `:450`, `:452`) and `game::kTickNs` at `:656` (comment naming it at `:644`).
- `Engine/Source/Frame/FrameStaticData.h:53` — comment referring to `game::Frame::kiElevationGridDim`.

Consequence: a second game project cannot link the engine at all, because the engine's own object files carry unresolved `game::` externs that only this game defines.

## Design

Pure code motion, values byte-identical:

- `kTickNs` and `kfDeltaTime` (`Projects/BrokenEngineSandbox/Source/Frame/Frame.h:36-37`) move to `Engine/Source/Frame/TimeStep.h`, beside the `engine::kiTickRate` they are derived from, keeping the existing comment.
- `kfCellWidth`, `kfCellHeight`, `kiElevationGridDim`, `kfBaseAreaMinX`, `kfBaseAreaMaxY`, `kfBaseAreaMaxX`, `kfBaseAreaMinY` (`Frame.h:187-198`) move to `Engine/Source/Frame/GridCoord.h` as namespace-scope `inline constexpr` values beside `kOriginCoord`, keeping the elevation-grid comment.
- `SeedFromGridCoord` (`Frame.h:214-222`) moves to `Engine/Source/Frame/GridCoord.h` with its comment; it already takes an `engine::GridCoord` and uses only `ToKey()`.

Destination details:

- `TimeStep.h`: the two constants go directly below `kiTickRate` (`TimeStep.h:6`). They need only `std::chrono`, the `ns` literal, and `common::NanosecondsToFloatSeconds`, all PCH-backed exactly as this header's existing `common::Timer`/`0ns` usage is; no new `#include` in the header.
- `GridCoord.h`: the seven world constants and `SeedFromGridCoord` go below `kOriginCoord` (`GridCoord.h:42`). The constants change spelling from class-static `static constexpr` to namespace-scope `inline constexpr`; types, initializer expressions, and comments are byte-identical. `SeedFromGridCoord` already takes `engine::GridCoord` and calls only `ToKey()`, so its body needs no edit beyond the namespace.
- Neither destination header is build-guarded and no moved name is client- or server-only, so there are no `BT_CLIENT`/`BT_SERVER` guard changes. No file is created or deleted, so `/update-vcxproj` is not triggered. `Engine.h` already aggregates `TimeStep.h` (`Engine.h:9`); `GridCoord.h` reaches every consumer through existing includes (game `Frame.h:4`, `IslandTerrain.h:3`, `FrameStaticData.h:3`, `FrameUtils.h:3`).

They leave `game::Frame` as a class-scope name, so game call sites requalify from `Frame::kfCellWidth` to `engine::kfCellWidth` and from `SeedFromGridCoord(...)` to `engine::SeedFromGridCoord(...)`. Engine call sites drop the `game::` qualifier. Two engine files then include the game `Frame.h` for no remaining reason and drop that include: `IslandChainPlacement.cpp:4` and `TimeStep.cpp:3` (`IslandTerrain.cpp` keeps its `Game.h` include for its out-of-scope `game::gpGame->mCoordFrames` reads at `:406-407` and `:606-607`). Every reference is compile-checked, so a missed site is a build error rather than a silent behavior change.

Game requalification sites, verified by grep: `Game.cpp:234-235`, `:378`, `:506`; `Frame/TerrainUtils.cpp:134-136`; `Frame/FrameTick.cpp:67` (`kfDeltaTime`); `Frame/Collections/Players/PlayersNavigation.cpp:435-444`; `Agent/AgentCommandsServer.cpp:558`, `:587`; `Network/NetworkSessionContract.h:24` (`kTickNs`); `Network/Client/ClientSessionReceive.cpp:101`, `:105`; `Network/Client/ReconcileReplayTick.cpp:236`, `:305`. Comment-only mentions that keep the bare name (`PlayersCombat.cpp:337`, `:355`; `FleetNavigationController.cpp:57`; `GameBase.h:188-189`, `:229`) need no edit.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Frame.h` — removals only
- `Engine/Source/Frame/TimeStep.h`, `Engine/Source/Frame/GridCoord.h` — new homes
- `Engine/Source/Frame/IslandChainPlacement.cpp`, `Engine/Source/Frame/IslandTerrain.cpp`, `Engine/Source/Frame/TimeStep.cpp`, `Engine/Source/GameBase.cpp`, `Engine/Source/Frame/FrameStaticData.h` — requalification and the two include drops
- The twelve game files listed at the end of Design — requalification only

## In scope

- Moving the ten names listed in Design, with their existing comments and byte-identical initializers
- Requalifying every reference to them in engine and game code, including the enumerated `Engine/Source/GameBase.cpp` and game sites
- Dropping the now-unused game `Frame.h` include from `IslandChainPlacement.cpp` and `TimeStep.cpp`
- Updating the comment at `Engine/Source/Frame/FrameStaticData.h:53` to name the new symbol
- Any `Engine/Source/Frame/AGENTS.md` or game `Frame/AGENTS.md` sentence that becomes wrong by naming the old owner

## Out of scope

- Any change to a constant's value, type, or expression, including "tidying" the derived `kfBaseArea*` expressions
- Moving `TracePointAgainstTerrain` / `TracePointToFrameExit` or anything else in `Frame/TerrainUtils.{h,cpp}` — owned by `Documents/Plans/Frame/TerrainTraceToEngine.md`
- Moving `FrameBounds` / `ComputeFrameBounds` / `ComputeFrameArea` / `IsOutOfBounds` / `ComputeTransferDelta` / `kuiInitialTransferCapacity` — owned by `Documents/Plans/Frame/FrameUtilsSharedHelpers.md`
- Moving `RunFrameTick` — owned by `Documents/Plans/Frame/RunFrameTickHoistToEngine.md`
- Any other member of `game::Frame`, `game::GameFlags`, or the game frame collections

## Risk tier and invariants

Tier 3 — the moved values feed deterministic state: island placement and rotation, the per-cell elevation grid, and five CRC-affecting RNG streams (`IslandChainPlacement.cpp:306-310`), plus the fixed tick period. Invariants: every moved value stays bit-identical; `SeedFromGridCoord`'s multiply-and-shift expression is unchanged (its comment explains why a naive cast would drop `x`); the five distinct seed multipliers stay paired with the same streams. Value drift here is a client/server desync, not a compile error — which is exactly why the change must be a move and nothing else.

## Acceptance criteria

- Client and server both compile and link with no `game::` symbol referenced from any file under `Engine/Source/Frame/`.
- A repo-wide grep for the nine moved names finds each defined exactly once, in `TimeStep.h` or `GridCoord.h`, and finds zero remaining `game::`-qualified references to any of them (including `Engine/Source/GameBase.cpp`).
- A harness replay determinism check passes: same seed, same island placements, matching per-tick CRCs across client and server.
- Diff review confirms every moved initializer, type, and the `SeedFromGridCoord` body is character-identical to the original.

## Scores

Effort 2 / Impact 4 / Risk 3
