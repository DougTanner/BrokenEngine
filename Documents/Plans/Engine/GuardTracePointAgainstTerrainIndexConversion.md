<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-26T20:46:37.521Z","dependsOn":[]} -->
# Guard TracePointAgainstTerrain Against Float-to-int64 Overflow

## Context

`TracePointAgainstTerrain` (`Engine/Source/Frame/TerrainCollisionUtils.cpp:10-146`)
traces a point segment against the cell's elevation grid using a 2D DDA over
`int64_t` grid indices. At lines 21-22 it computes:

```cpp
int64_t iGridX = static_cast<int64_t>(std::floor((f4Start.x - kfCellOriginX) / kfGridPitchX));
int64_t iGridY = static_cast<int64_t>(std::floor((f4Start.y - kfCellOriginY) / kfGridPitchY));
```

Converting a floating-point value outside the representable `int64_t` range
(`|f4Start.x|` at or above roughly `9.5e18`, `|f4Start.y|` at or above roughly
`8.3e18`, `+/-inf`, or `NaN`) to `int64_t` is C++23 `[conv.fpint]/1` undefined
behavior. Even for large finite off-grid coordinates within `int64_t` range,
the DDA loop (`Engine/Source/Frame/TerrainCollisionUtils.cpp:31-145`) steps
cell-by-cell across off-grid space where elevation is constant
`sampler.fSeaFloor`.

This path is reachable from saved simulation state: `ValidateFlyingMissiles`
(`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:127-149`) validates only
Z finiteness and zero vertical velocity for flying missiles without bounding
saved XY coordinates (and saved blaster XY is likewise unbounded). On the next
tick, `MissilesPostRender::PreCollision`
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesUpdate.cpp:216-228`)
and `BlastersPostRender::PreCollision`
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/BlastersUpdate.cpp:173-185`)
submit `.pTerrainStaticData = &rStaticData` to `engine::Collision::AddLayer`,
which invokes `TracePointAgainstTerrain` at
`Engine/Source/Frame/Collision.cpp:150`.

## Design

Author's recommendation: guard `TracePointAgainstTerrain`
(`Engine/Source/Frame/TerrainCollisionUtils.cpp:10-146`) before any
`static_cast<int64_t>` runs so non-finite, out-of-range, or off-grid segments
never trigger float-to-`int64_t` overflow or unbounded off-grid DDA stepping,
while preserving bit-identical `/fp:strict` results for every finite segment
inside the elevation grid (whether `sampler.pGrid` is null or non-null). Apply
these three cases in strict precedence order:

1. **Non-finite XY guard (checked first):** Test `std::isfinite` on `f4Start.x`,
   `f4Start.y`, `f4End.x`, and `f4End.y` before any grid-index conversion,
   in-grid test, or interpolation. If any of the four XY endpoint coordinates is
   non-finite (`NaN` or `+/-inf`), return `{}` (no terrain hit) immediately.
   This takes precedence even when `f4Start` lies inside the elevation grid and
   `f4End` has non-finite XY, avoiding both undefined casts / non-finite DDA
   steps and synthesizing a `NaN` or `+/-inf` hit position via `XMVectorLerp`
   that `Collision::GetOutcome` (`Engine/Source/Frame/Collision.cpp:825`) would
   forward into simulation state (`MissilesUpdate.cpp:269`).
2. **Finite in-grid start (unchanged DDA with O(1) grid-exit bound):** Compute
   `fStartGridX = (f4Start.x - kfCellOriginX) / kfGridPitchX` and
   `fStartGridY = (f4Start.y - kfCellOriginY) / kfGridPitchY` (matching
   `CellElevationSampler::Sample` at `Engine/Source/Frame/IslandTerrain.cpp:451-453`).
   Whenever both lie in the half-open grid domain
   `[0.0f, static_cast<float>(kiElevationGridDimension))` — for both
   `sampler.pGrid != nullptr` and `sampler.pGrid == nullptr` — `iGridX` and
   `iGridY` are proven finite integers in `[0, kiElevationGridDimension - 1]`.
   Cast only `fStartGridX` and `fStartGridY` (never `f4End`) and run the
   existing `/fp:strict` DDA unchanged:
   - When `f4End` also lies inside
     `[0.0f, static_cast<float>(kiElevationGridDimension))` on both axes, the
     convex grid contains the entire segment and the existing DDA runs to
     `fNextPercent >= 1.0f` with bit-identical `/fp:strict` results.
   - When `f4End` lies outside
     `[0.0f, static_cast<float>(kiElevationGridDimension))`, preserve the
     existing DDA's float32 `fNextPercent` stepping
     (`TerrainCollisionUtils.cpp:79, 104`), terminating when either
     `fNextPercent >= 1.0f` (including when float32 boundary rounding reaches
     `1.0f` on the last in-grid cell, in which case the existing endpoint check
     completes the trace without an off-grid tail) or advancing
     `(iGridX, iGridY)` exits
     `[0, kiElevationGridDimension) x [0, kiElevationGridDimension)` with
     `fCurrentPercent < 1.0f`. Only when the loop exits the grid with
     `fCurrentPercent < 1.0f` — a line segment exiting the convex elevation grid
     never re-enters — stop stepping, check
     `fCurrentZ <= sampler.Sample(vecCurrentPosition)` at the exit boundary
     `fCurrentPercent`, and resolve the remaining off-grid tail
     `[fCurrentPercent, 1.0f]` directly against `sampler.fSeaFloor` in O(1).
     Because the cell hexagon is strictly enclosed inside the elevation grid by
     `kfCellLandMarginMeters`, normal in-cell gameplay exits the cell
     (`Collision.cpp:152, 823`) before any off-grid tail is reached.
3. **Finite off-grid start (O(1) direct sea-floor resolution):** When all four XY
   coordinates are finite and `fStartGridX` or `fStartGridY` lies outside
   `[0.0f, static_cast<float>(kiElevationGridDimension))` (unreachable for valid
   in-cell entities; reachable only from corrupt saved positions admitted by
   `ValidateFlyingMissiles` or blaster read validation), do not slab-clip or
   enter the elevation grid DDA, and perform zero `int64_t` casts. Resolve
   `[0.0f, 1.0f]` directly against `sampler.fSeaFloor` in O(1).

**Direct sea-floor resolution semantics:** On an off-grid interval
`[tStart, 1.0f]` (`tStart = 0.0f` for an off-grid start, or
`tStart = fCurrentPercent` for an off-grid tail), test start contact
`z(tStart) <= sampler.fSeaFloor` (using `f4Start.z` when `tStart == 0.0f`, or
`f4Start.z + fDeltaZ * tStart`), then interior crossing
`fHitPercent = std::clamp((sampler.fSeaFloor - f4Start.z) / fDeltaZ, tStart, 1.0f)`
when `f4End.z < sampler.fSeaFloor`, and finally end contact
`f4End.z <= sampler.fSeaFloor` at `1.0f`. Because it replaces off-grid DDA
stepping, off-grid segments and off-grid tails intentionally no longer sample
elevation grid cells or inherit float32 boundary-rounded off-grid DDA hit times.
Every finite segment that stays inside the elevation grid — in both
island-bearing and island-free cells — remains on the exact unchanged
`/fp:strict` DDA.

Rationale: checking non-finite XY first, keeping finite in-grid starts on the
unchanged `/fp:strict` DDA with an O(1) grid-exit bound, and resolving finite
off-grid starts directly against `sampler.fSeaFloor` in O(1) eliminates
`[conv.fpint]/1` UB for all inputs, bounds DDA work to the elevation grid
dimensions without slab-clipping or rounded-time Z-interpolation complexity,
keeps grid admission aligned with `CellElevationSampler::Sample`, and preserves
bit-identical `/fp:strict` simulation and CRC results for all normal in-cell
gameplay.

## Critical files

- `Engine/Source/Frame/TerrainCollisionUtils.cpp` — `TracePointAgainstTerrain`
  (`:10-146`)
- `Engine/Source/Frame/TerrainCollisionUtils.h` — public declaration (`:11`,
  read-only; signature unchanged)
- `Engine/Source/Frame/GridCoord.h` — `kiElevationGridDimension`,
  `kfCellOriginX`, `kfCellOriginY`, `kfGridPitchX`, `kfGridPitchY` (`:63-68`,
  read-only)
- `Engine/Source/Frame/IslandTerrain.cpp` — `CellElevationSampler::Sample`
  half-open grid ownership (`:441-461`, read-only)
- `Engine/Source/Frame/Collision.cpp` — sole caller (`:150, 820-831`, read-only)

## In scope

- `TracePointAgainstTerrain` in
  `Engine/Source/Frame/TerrainCollisionUtils.cpp` (`:10-146`): return `{}` on
  non-finite XY before any index conversion, keep finite in-grid DDA traversal
  unchanged while `(iGridX, iGridY)` remains inside
  `[0, kiElevationGridDimension)` with O(1) sea-floor termination upon exiting
  the elevation grid, and resolve finite off-grid starts directly against
  `sampler.fSeaFloor` in O(1) without converting XY to `int64_t` or entering the
  DDA.

## Out of scope

- Changing `OverlappedGridCells`, `ResolveDiscAgainstTerrain`, or any other
  function in `Engine/Source/Frame/TerrainCollisionUtils.cpp`.
- Changing `TracePointAgainstTerrain`'s public signature
  (`Engine/Source/Frame/TerrainCollisionUtils.h:11`) or `GridCoord.h`
  constants.
- Changing saved-entity horizontal range admission in
  `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`
  (`ValidateFlyingMissiles` or blaster read validation).
- Changing `Frame::kiVersion`, save/replay formats, wire protocol, or
  `/fp:strict` simulation/CRC output for normal in-cell gameplay queries (finite
  segments inside the elevation grid, or the in-grid prefix before cell exit,
  for both island-bearing and island-free cells).

## Acceptance criteria

1. Code inspection of `TracePointAgainstTerrain`
   (`Engine/Source/Frame/TerrainCollisionUtils.cpp`) confirms that non-finite XY
   endpoints (`!std::isfinite` on start or end X/Y) return `{}` before any
   `static_cast<int64_t>`, DDA step, or `XMVectorLerp`, and every remaining
   `static_cast<int64_t>` float operand is proven finite and within
   `[0.0f, static_cast<float>(kiElevationGridDimension))` before conversion,
   producing an `int64_t` index in `[0, kiElevationGridDimension - 1]` and
   eliminating C++23 `[conv.fpint]/1` UB for all inputs.
2. Code inspection confirms that DDA iterations are strictly bounded to the
   elevation grid dimensions for all finite segments: segments starting inside
   `[0.0f, static_cast<float>(kiElevationGridDimension))` and ending outside
   preserve the existing float32 `fNextPercent` DDA stepping without casting
   `f4End` to `int64_t`, terminating when `fNextPercent >= 1.0f` or immediately
   upon `(iGridX, iGridY)` exiting `[0, kiElevationGridDimension)` with
   `fCurrentPercent < 1.0f`; only in that grid-exit case do they check
   `sampler.Sample` at the exit boundary `fCurrentPercent` and resolve the
   remaining off-grid tail `[fCurrentPercent, 1.0f]` against `sampler.fSeaFloor`
   in O(1). Segments starting outside
   `[0.0f, static_cast<float>(kiElevationGridDimension))` on either axis
   (matching `CellElevationSampler::Sample` float grid-index ownership) bypass
   the DDA and resolve `[0.0f, 1.0f]` directly against `sampler.fSeaFloor` in
   O(1) with zero `int64_t` casts.
3. Code inspection confirms that finite segments starting and ending inside the
   half-open elevation grid — whether `sampler.pGrid != nullptr` or
   `sampler.pGrid == nullptr` — execute the exact unchanged `/fp:strict` DDA
   traversal and return bit-identical results, preserving simulation/CRC
   behavior for normal in-cell gameplay.
4. Code inspection confirms that O(1) direct sea-floor resolution on off-grid
   starts `[0.0f, 1.0f]` and off-grid tails `[fCurrentPercent, 1.0f]` detects
   contact with `sampler.fSeaFloor` at interval start, interior crossing, and
   interval end with no change to `Frame::kiVersion`, save/replay formats, or
   wire protocol.
5. `/compile` builds `BrokenEngineSandbox` (`BT_CLIENT`) and
   `BrokenEngineSandboxServer` (`BT_SERVER`) cleanly in both `Debug|x64` and
   `Release|x64`, and builds `ModelViewer` (`BT_CLIENT`) cleanly in
   `Debug|x64`.

## Notes

- Change Workflow tier: Tier 2 — scoped runtime behavior change in one engine
  subsystem (`Engine/Source/Frame/TerrainCollisionUtils.cpp`). Normal in-cell
  gameplay queries (finite segments inside the elevation grid, in both
  island-bearing and island-free cells) stay on the unchanged `/fp:strict` DDA
  with bit-identical CRC output; direct sea-floor resolution and the non-finite
  XY early return change behavior only for off-grid segments/tails and
  non-finite XY inputs, with no change to public signatures, `Frame::kiVersion`,
  serialization, save/replay formats, wire format, threading, allocation,
  shaders, or trust boundaries.
- Build affinity: `TerrainCollisionUtils.cpp` compiles into
  `BrokenEngineSandbox` (`BT_CLIENT`), `BrokenEngineSandboxServer`
  (`BT_SERVER`), and `ModelViewer` (`BT_CLIENT`) with no project-membership
  change.
- Verification: extreme out-of-range or non-finite coordinates are not directly
  reachable through existing `/agent-harness` commands, so verification is by
  code inspection plus the clean multi-target builds above.
