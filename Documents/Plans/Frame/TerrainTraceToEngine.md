<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-11T00:00:16.000Z","dependsOn":[]} -->
# Move the terrain and frame-exit traces into the engine

## Context

`Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.cpp` holds two geometric queries with no gameplay in them:

- `TracePointAgainstTerrain` (`:132-275`, declared `TerrainUtils.h:23`) — a DDA walk across the per-cell elevation grid, stepping cell boundaries in x and y and sampling through `engine::FrameElevationSampler` from `engine::gpIslandTerrain->MakeFrameElevationSampler(rStaticData)`, returning the first time and position at which a moving point meets the terrain.
- `TracePointToFrameExit` (`:277-329`, declared `TerrainUtils.h:24`) — an axis-aligned box exit-time solve against the cell's `vecArea`, including the boundary cases where the segment starts outside the box or on a boundary heading outward.

Both return `SegmentHit` (`TerrainUtils.h:13-18`), which is `{bool bHit; float fTime; XMVECTOR vecPosition;}`.

Their only ties to the game layer are `Frame::kiElevationGridDim`, `Frame::kfCellWidth`, `Frame::kfCellHeight` (`:134-136`) and `FrameBounds`/`ComputeFrameBounds` (`:279`). Both prerequisite plans move exactly those names into the engine, after which the coupling set is empty and this becomes a byte-preserving move.

## Design

- `SegmentHit` and the `TracePointAgainstTerrain` declaration move to `Engine/Source/Frame/IslandTerrain.h` as namespace-scope names in `engine` (free function, not an `IslandTerrain` member — it is one today); the definition moves to `IslandTerrain.cpp`, which already owns the elevation sampler and the grid it walks.
- `TracePointToFrameExit` moves to `Engine/Source/Frame/FrameUtils.h`, beside the `FrameBounds`/`ComputeFrameBounds` helpers it calls, which the prerequisite plan puts there. `FrameUtils.h` has no matching `.cpp`, so the function becomes an `inline` header definition — the added `inline` keyword and the header placement are the only spelling changes to it; under `/fp:strict` the identical expressions produce identical results. `FrameUtils.h` adds `#include "Frame/IslandTerrain.h"` for `SegmentHit` (no cycle: `IslandTerrain.h` includes only `GridCoord.h`).

Signatures, parameter order, `XM_CALLCONV`, DirectXMath function-form calls, and every comment move unchanged. Inside the moved bodies the only edits are qualifier spelling: `engine::` prefixes on `FrameElevationSampler`, `gpIslandTerrain`, and `FrameStaticData` drop, and the `Frame::kiElevationGridDim`/`Frame::kfCellWidth`/`Frame::kfCellHeight` and `FrameBounds`/`ComputeFrameBounds` names become the bare engine names the prerequisite plans created. Call sites requalify from unqualified-in-`game` to `engine::` — `SegmentHit`, both trace calls, and the two `std::vector<SegmentHit>` scratch members. `TerrainUtils.{h,cpp}` keep `AiSteeringResult`, `ComputeAiSteering`, and `ComputeTerrainAvoidance`, so both files survive and no project membership changes; `/update-vcxproj` is not triggered and no `BT_CLIENT`/`BT_SERVER` guard is involved (all touched code is shared).

## Critical files

- `Engine/Source/Frame/IslandTerrain.h`, `IslandTerrain.cpp` — `SegmentHit` and the terrain trace
- `Engine/Source/Frame/FrameUtils.h` — the frame-exit trace
- `Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.h`, `TerrainUtils.cpp` — removals
- The four callers, verified by grep: `Frame/Collections/Blasters/BlastersUpdate.cpp` (`:39-40`, `:262-263`, `:316-317`), `Missiles/MissilesUpdate.cpp` (`:27-28`, `:296-297`, `:361-362`), `Players/Players.cpp:692`, `Spaceships/SpaceshipsCombat.cpp:123` — requalification

## In scope

- Moving `SegmentHit`, `TracePointAgainstTerrain`, and `TracePointToFrameExit` with byte-identical bodies and comments, plus the qualifier-spelling and `inline` edits named in Design
- Requalifying every call site, including the `std::vector<SegmentHit>` scratch declarations
- Any `Engine/Source/Frame/AGENTS.md` or game `Frame/AGENTS.md` sentence that names the old owner

## Out of scope

- `ComputeAiSteering` and `ComputeTerrainAvoidance` (`TerrainUtils.cpp:8-130`, declared `TerrainUtils.h:20-22`) and the `AiSteeringResult` struct (`TerrainUtils.h:8-11`). They carry AI tuning parameters — avoidance angles, look-ahead distances, and the smoothing factor — whose ownership between engine and game is an open decision, so they are not a Plan yet. Leave them where they are.
- Any change to the DDA stepping, the grid pitch derivation, the sampler usage, the boundary-case ordering in the exit solve, or any floating-point expression
- Any change to `FrameElevationSampler`, `BuildElevationGrid`, or the `FrameElevation`/`GlobalElevation` split
- The constants and bounds helpers themselves, owned by the two prerequisite plans

## Risk tier and invariants

Tier 3 — both traces feed CRC-visible simulation results: collision times and impact positions determine where entities stop and what they hit. Invariants: every arithmetic expression, comparison, and DirectXMath call moves character-identical, because `/fp:strict` results must stay bit-identical across client and server; the exit solve keeps its exact boundary semantics (starting outside, or on a boundary with a non-inward delta, yields time zero) and its `std::numeric_limits<float>::max()` sentinel for "no exit"; the terrain trace keeps calling `FrameElevation`-family sampling that reads only the cell's own static data, never `mCoordFrames`, which is what makes parallel per-cell ticks safe (`Engine/Source/Frame/AGENTS.md`).

## Acceptance criteria

- Client and server compile; both traces exist once, in the engine.
- A replay determinism run reproduces bit-identical per-tick CRCs against a replay recorded before the change.
- A harness scenario with projectiles hitting an island and with entities crossing a cell boundary produces the same impact positions and transfer ticks as before.
- Diff review confirms nothing inside either moved function changed beyond the qualifier spellings and the one `inline` keyword named in Design — every arithmetic expression, comparison, and comment is character-identical.

## Scores

Effort 2 / Impact 4 / Risk 2
