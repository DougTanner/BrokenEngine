<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-08T22:35:20.380Z","dependsOn":[]} -->
# Split terrain collision queries out of IslandTerrain.cpp

## Context

Adding the swept-disc terrain query (`engine::ResolveDiscAgainstTerrain`, landing commit "Sweep the Player and Spaceship body disc against terrain each tick") grew `Engine/Source/Frame/IslandTerrain.cpp` from 7,779 to 11,424 `bt-token-v1` (1,080 lines) and `IslandTerrain.h` from 2,123 to 2,445. Implementation files over 10,000 `bt-token-v1` route to `/reduce-file` (`.agents/skills/external-refactor-clean/references/worker.md`). The user chose to record the split as this follow-up instead of doing it in that landing, to fold in the grid-constant cleanup below, and not to adopt a 1 mm standoff in place of `RoundAlongNormal`.

Boundary evidence (line numbers at commit a98b47f6):
- `IslandTerrain.cpp:572-1078` hold the two terrain collision queries: `TracePointAgainstTerrain` (`:572-717`) and the disc query (`kfDiscOverlapTolerance` at `:722`, the file-local `Disc*` types and helpers, `ResolveDiscAgainstTerrain` ending at `:1078`).
- That range reaches `IslandTerrain` only through `gpIslandTerrain->MakeCellElevationSampler` (`:580`, `:915`). It uses no file-local symbol from `:1-571`, and `:1-571` uses none of its symbols.
- `Frame/NavQuery.h` and `Ui/WrapperBase.h` (`:6-7`) are used only inside that range (`NavQuerySnapToNavigable` and `gBaseHeight` at `:943`). `Game.h` stays, because `CellNormal` uses `game::gpGame` (`:560`).
- `IslandTerrain.h:179-203` hold the matching declarations: `TracePointAgainstTerrain`, `DiscTerrainFlags`, `DiscTerrainResult` with its comment, and `ResolveDiscAgainstTerrain` with its comment. They need only `SegmentHit` (`Frame/FrameUtils.h:62`), `common::Flags`, and a `CellStaticData` forward declaration.
- Callers: `TracePointAgainstTerrain` in `BlastersUpdate.cpp:219` and `MissilesUpdate.cpp:322`, and `ResolveDiscAgainstTerrain` in `PlayersNavigation.cpp:481` and `SpaceshipsNavigation.cpp:140`. All of them reach the declarations through `Engine.h` via `Pch.h`, not by including `IslandTerrain.h` for them.

Grid-constant duplication: `GridCellSquare` (`:758-762`) and `OverlappedGridCells` (`:779-783`) each declare the same five `static constexpr` grid constants: `kiGridDimension`, `kfGridPitchX`, `kfGridPitchY`, `kfCellOriginX`, and `kfCellOriginY`, the last four as `double`. `IsDiscClear` (`:815`) and the sweep in `ResolveDiscAgainstTerrain` (`:972`) already index the grid with `kiElevationGridDimension` directly.

## Design

Recommended design (from the `/reduce-file` standalone analysis):

1. Create `Engine/Source/Frame/TerrainCollisionUtils.h`: `#pragma once`, `#include "Frame/FrameUtils.h"`, then `namespace engine` with `struct CellStaticData;`. Move `IslandTerrain.h:179-203` there unchanged. The name `Frame/TerrainUtils.h` is already taken by the game's `Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.h` (included as `"Frame/TerrainUtils.h"` by the same callers), so the engine header needs a distinct name.
2. Create `Engine/Source/Frame/TerrainCollisionUtils.cpp`. It includes `TerrainCollisionUtils.h` first, then `Frame/CellStaticData.h`, `Frame/IslandTerrain.h`, `Frame/NavQuery.h`, and `Ui/WrapperBase.h`. Move `IslandTerrain.cpp:572-1078` byte for byte inside `namespace engine`.
3. Remove `#include "Frame/NavQuery.h"` and `#include "Ui/WrapperBase.h"` from `IslandTerrain.cpp`.
4. In `Engine/Source/Engine.h`, add `#include "Frame/TerrainCollisionUtils.h"` right after `#include "Frame/IslandTerrain.h"` (`:74`), outside any `BT_CLIENT`/`BT_SERVER` guard. This keeps every caller compiling unchanged.
5. Use `/update-vcxproj` to add both files to `BrokenEngineSandbox` and `BrokenEngineSandboxServer`, filter `Engine\Frame`. They are client+server files, like `IslandTerrain.{h,cpp}`.
6. Grid-constant cleanup, in the moved disc code:
   - Declare `kfGridPitchX`, `kfGridPitchY`, `kfCellOriginX`, and `kfCellOriginY` once as file-scope `constexpr double`, beside `kfDiscOverlapTolerance`, with the same initializers. Delete the five local declarations from `GridCellSquare` and `OverlappedGridCells`.
   - Replace `kiGridDimension - 1` in `OverlappedGridCells` with `kiElevationGridDimension - 1`, so no alias is needed.
   - This position comes after `TracePointAgainstTerrain`. That keeps its own `float` locals of the same names from hiding a file-scope declaration, which would raise warning C4459 under `/W4` with warnings as errors.
   - Every value is the same compile-time constant, so behavior does not change. `TracePointAgainstTerrain`'s `float` constants stay as they are.
7. Update the pointers to the moved declaration comments:
   - In `Engine/Source/Frame/AGENTS.md`, the `ResolveDiscAgainstTerrain` bullet should name `TerrainCollisionUtils.h` instead of `IslandTerrain.h`.
   - If `Documents/Plans/Game/SpaceshipZDrift.md` still exists, change its `DiscTerrainResult` citation to the new `TerrainCollisionUtils.h` line and its `IslandTerrain.cpp` mention to `TerrainCollisionUtils.cpp`.

Expected sizes: `IslandTerrain.cpp` ~6,510 `bt-token-v1`, `IslandTerrain.h` ~2,080, `TerrainCollisionUtils.cpp` ~5,000, `TerrainCollisionUtils.h` ~430.

Change Workflow tier: Tier 1. Trigger: a mechanical, behavior-preserving file split, plus project membership and a constant-scope cleanup. No public signature changes and no invariant is exposed. Both queries run on the deterministic sim path, but the moved code is byte-identical and the cleanup keeps every constant value, so codegen and CRC behavior should not change.

## Critical files

- `Engine/Source/Frame/IslandTerrain.cpp`, `Engine/Source/Frame/IslandTerrain.h`
- `Engine/Source/Frame/TerrainCollisionUtils.cpp`, `Engine/Source/Frame/TerrainCollisionUtils.h` (new)
- `Engine/Source/Engine.h`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj` and `.vcxproj.filters`, `BrokenEngineSandboxServer.vcxproj`
- `Engine/Source/Frame/AGENTS.md`
- `Documents/Plans/Game/SpaceshipZDrift.md` (citation only, if still present)

## In scope

- Moving `IslandTerrain.h:179-203` (`TracePointAgainstTerrain`, `DiscTerrainFlags`, `DiscTerrainResult`, `ResolveDiscAgainstTerrain` and their comments) to the new `TerrainCollisionUtils.h`.
- Moving `IslandTerrain.cpp:572-1078` (`TracePointAgainstTerrain` through the end of `ResolveDiscAgainstTerrain`) to the new `TerrainCollisionUtils.cpp`.
- The two include removals in `IslandTerrain.cpp`, the one include added to `Engine.h`, and project and filter membership for the two new files in both executables.
- The grid-constant cleanup in `GridCellSquare` and `OverlappedGridCells` (Design step 6).
- The `ResolveDiscAgainstTerrain` bullet in `Engine/Source/Frame/AGENTS.md`, and the `DiscTerrainResult`/`IslandTerrain.cpp` citation in `Documents/Plans/Game/SpaceshipZDrift.md`.

## Out of scope

- Any behavior change, symbol rename, or signature change.
- Moving the file-local `Disc*` types and helpers into an anonymous namespace.
- Deduplicating `TracePointAgainstTerrain`'s `float` grid constants, or the grid constants in `IslandTerrain.cpp:1-571`.
- Replacing `RoundAlongNormal` with a 1 mm standoff (user decision, see Context).
- Caller include changes in game files.

## Acceptance criteria

- `git diff --color-moved` shows only moved blocks, the include lines, the `Engine.h` line, the project and filter entries, the Design step 6 cleanup, and the two documentation pointer updates.
- `IslandTerrain.cpp` measures under 10,000 `bt-token-v1`.
- `/update-vcxproj` validation passes, and `/compile` passes for the client and server.

## Notes

- No determinism, serialization, `.pack`/`kiVersion`, replay, wire, threading, allocation, or shader exposure. Because the change preserves behavior, no live harness check is needed.
