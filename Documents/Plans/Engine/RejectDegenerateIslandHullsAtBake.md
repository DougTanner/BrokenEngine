<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-09T12:12:24.338Z","dependsOn":[]} -->
# Fail the island bake on a degenerate valid-area hull and delete the runtime fallbacks for missing island data

## Context

A `/plan-alternatives` step for an earlier engine runtime cleanup proposed "fix the island data where it is made". The user chose to keep that change to its verified candidates and record this alternative as a separate follow-up Plan.

Today the island pipeline tolerates two data states that the runtime then works around:

- **Valid-area hull with fewer than 3 vertices.** DataPacker `BuildValidAreaHull` (`DataPacker/Source/ExportJobs/ExportIsland.cpp`) returns silently, leaving `ExportedIsland::iValidAreaVertexCount` at 0, when fewer than 3 row-extreme candidates exist or when the monotone-chain hull ends up with fewer than 3 vertices. `VerifyHullCcwConvex` checks winding and convexity only inside an `iValidAreaVertexCount >= 3` guard, and `Export` serializes a count of 0. At runtime, `IslandTerrain::WaitForElevationMaps` (`Engine/Source/Frame/IslandTerrain.cpp`) accepts any `IslandHeader::iValidAreaVertexCount >= 0`. Three consumers then work around a missing hull: `LocalHull` (`Engine/Source/Frame/IslandChainPlacement.cpp`) substitutes a four-corner footprint rectangle held in caller-provided storage (`PlaceAnchor` and `TryTouchPlace` each declare a `rectangleHull[4]` and pass it in), and `DebugRenderIslandValidArea` (`Engine/Source/Graphics/Render/MainUniforms.cpp`) skips templates whose hull pointer is null or whose count is below 3.
- **Empty size bucket.** The `IslandTerrain` constructor sorts templates into `mHugeCrcs`, `mLargeCrcs`, `mMediumCrcs`, and `mSmallCrcs` by footprint area and asserts only that `mIslandCrcsSorted` is non-empty. `PickBucket` (`IslandChainPlacement.cpp`) falls back through related buckets and finally to `mIslandCrcsSorted` "so small asset sets still place something".

`OrientForTangent` has no short-side guard: its aspect test divides by `fShort` directly, because the `IslandTerrain` constructor asserts both footprint dimensions are positive.

## Design

### Order of work

1. **Make the data contract strict at the producer and the loader** (steps below under "Producer" and "Loader").
2. **Verify the precondition before deleting any fallback.** Run an authorized Local generation build through `/compile` (`.agents/skills/compile/references/runtime-data-mode.md` `## Local generation`), so DataPacker re-exports every island with the new check. Then boot a server and a client through `/agent-harness`. The precondition holds when the bake succeeds (no island fails the hull check) and both boots pass the four bucket `ASSERT`s.
   - **If the precondition fails** — an island fails the bake, or a bucket is empty — stop and return to the user. Fixing or removing island source assets is outside this Plan, and with it the fallbacks are live behavior: deleting them would change which islands are placed where, so chain placement output and the per-tick CRCs would change.
   - **If it holds**, every fallback below is unreachable with the shipped data, and deleting it leaves placement output bit-identical.
3. **Delete the fallbacks** (step below under "Fallback deletions").

### Producer

- In `ExportIslandData`, immediately after the `BuildValidAreaHull(rOut)` call, throw a `std::runtime_error` naming the island leaf (`rInputPath`) when `rOut.iValidAreaVertexCount < 3`, matching the other `Island leaf "{}" ...` bake failures there. Both early `return`s in `BuildValidAreaHull` (fewer than 3 candidates; fewer than 3 hull vertices) leave the count at 0, so this one check covers both, and `ExportIslandData` already holds the leaf path, so no function gains a parameter. `BuildValidAreaHull` and its header comment stay unchanged. The author recommends a throw over an `ASSERT` because a throw fails the owning export job through its existing `CleanupOnFailure` path and reports which island is degenerate.
- In `VerifyHullCcwConvex`, which runs after the new throw, delete the `>= 3` guard and run its checks unconditionally; update its comment, which limits verification to hulls with at least three vertices.
- Bump `ExportIsland::kiVersion` (`DataPacker/Source/ExportJobs/ExportIsland.h`) by one. The payload layout and `IslandHeader` layout do not change, so the author recommends leaving the shared data-format version alone (`DataPacker/Source/ExportJobs/Island/AGENTS.md` `## Versioning` bumps it only for layout changes). The `kiVersion` bump is what forces every cached island chunk through the new check, and the loader change below rejects any chunk an older packer wrote with fewer than 3 hull vertices.

### Loader

- In `IslandTerrain::WaitForElevationMaps`, the header-count validation that throws `std::ios_base::failure` rejects `iValidAreaVertexCount < 3` instead of `< 0`. Keeping it in the existing pack-count check keeps all `kIsland` payload validation in one place.
- In the `IslandTerrain` constructor, after the bucket loop, `ASSERT` each of `mHugeCrcs`, `mLargeCrcs`, `mMediumCrcs`, and `mSmallCrcs` is non-empty. The four asserts imply the existing `ASSERT(!mIslandCrcsSorted.empty())`; the author recommends replacing that assert with the four and carrying its comment (placement and the TextureManager slot-0 anchor need islands) onto them.
- Update the `IslandTemplate::pf2ValidAreaVertices` comment (`Engine/Source/Frame/IslandTerrain.h`), which says a count below 3 or a null pointer means no usable polygon.

### Fallback deletions

- `PickBucket`: each role returns its own bucket; delete the cross-bucket chains and the `mIslandCrcsSorted` fallback, and update the function comment.
- `LocalHull`: delete the footprint-rectangle substitute. The author recommends deleting `LocalHull` itself, so `PlaceAnchor` and `TryTouchPlace` read `rTemplate.pf2ValidAreaVertices` and `rTemplate.iValidAreaVertexCount` directly and drop their `rectangleHull` storage and `iLocalCount` locals; a helper reduced to returning two fields adds nothing.
- `DebugRenderIslandValidArea`: delete the null/count skip.

## Critical files

- `DataPacker/Source/ExportJobs/ExportIsland.cpp`, `DataPacker/Source/ExportJobs/ExportIsland.h`
- `Engine/Source/Frame/IslandTerrain.cpp`, `Engine/Source/Frame/IslandTerrain.h`
- `Engine/Source/Frame/IslandChainPlacement.cpp`
- `Engine/Source/Graphics/Render/MainUniforms.cpp`

## In scope

- `ExportIslandData`: the one new hull-count throw after the `BuildValidAreaHull` call.
- `VerifyHullCcwConvex`: its `>= 3` guard; its comment.
- `ExportIsland::kiVersion`: one increment.
- `IslandTerrain::WaitForElevationMaps`: the `iValidAreaVertexCount` bound in the header-count validation.
- `IslandTerrain::IslandTerrain`: the four bucket `ASSERT`s replacing the `mIslandCrcsSorted` non-empty assert.
- `IslandTemplate::pf2ValidAreaVertices` comment.
- `PickBucket` body and comment; `LocalHull` (deleted) and its two call sites in `PlaceAnchor` and `TryTouchPlace`.
- `DebugRenderIslandValidArea`: the null/count skip.

## Out of scope

- `OrientForTangent`: it has no short-side guard; its aspect test is real behavior and stays.
- `BuildValidAreaHull`, including its two early returns and its header comment.
- Island source assets under `Engine/Data/Islands` and the bake, split, and texture stages and their versions (`kiTextureVersion` included). If the precondition fails, this Plan stops instead of changing data.
- The shared data-format version, `IslandHeader`, and the `kIsland` payload layout.
- The bucket area thresholds and the chain roles in `GenerateIslandChain`.
- Every other region of `ExportIsland.cpp` and of `MainUniforms.cpp`.

## Acceptance criteria

- The Local generation build re-exports every island and succeeds; a server and a client boot through `/agent-harness` with no `ASSERT` and no `IslandTerrain::WaitForElevationMaps` failure. This is the precondition check of Design step 2, recorded before the fallback deletions.
- `cell_coordinate_probe` (`/agent-harness`) on a fixed set of at least eight coordinates, including `[0,0]` and coordinates with negative components, returns the same `placements.crc` and `elevation.crc` on the baseline build and on the changed build, and client and server agree for each coordinate.
- A client/server session through `/agent-harness` runs with no per-tick CRC mismatch.
- Client and server build clean in Debug and Release through `/compile`, and DataPacker builds clean in Release.

## Notes

- Change Workflow tier: **Tier 3** (`.agents/references/risk-tiers.md`). Highest trigger: a serialization/`.pack` change (`ExportIsland::kiVersion` bump and tightened pack-load validation) that spans independently owned subsystems (DataPacker and Engine Frame and Graphics). The deletions also sit on CRC-checked chain placement; they are behavior-preserving only when the precondition holds.
- Determinism/CRC: `GenerateIslandChain` output feeds per-tick CRC'd state on client and server. With all four buckets non-empty and every hull at least 3 vertices, `PickBucket` returns the same bucket and `LocalHull` returned the template hull, so placement is unchanged.
- Live verification needs an authorized Local generation build (`.agents/skills/compile/references/runtime-data-mode.md` `## Local generation`); request that authorization at plan approval.
- No ordering dependency: `FlightPlaneData.md` edits other `MainUniforms.cpp` functions. Locate each site by symbol; whichever lands first, the other rebases.
