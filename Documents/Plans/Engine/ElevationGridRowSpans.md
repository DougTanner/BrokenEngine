<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-10T23:38:09.813Z","dependsOn":[]} -->
# Reuse exact elevation grid row spans

## Context

The user asked to optimize the shared elevation-grid boundary check. Both builds construct their elevation grids
before their first simulation tick for a populated cell. Each placement currently samples each point in its
clamped footprint, then checks the same margin-grown hexagon. The grid dimensions and texel-center expressions
are coordinate-independent, and the margin-grown check has asymmetric edge ties. See
`Engine/Source/Frame/FrameBase.cpp:299`, `Engine/Source/Frame/IslandTerrain.cpp:404`,
`Engine/Source/Frame/GridCoord.h:63` and `Common/Math/MathUtils.cpp:188`.

## Design

The design below is the plan author's recommendation for Tier-3 review and user approval after claim.

The proposed implementation builds an immutable fixed-size table of column spans when IslandTerrain starts,
before either build can dispatch a terrain consumer. For each row, evaluate the current common::IsInsideHex at
the exact existing float texel centers and negative land margin to record the first and one-past-last accepted
column, or an empty span. A row's intersection with the six current half-planes is an interval. Verify the table
against every grid center under the original predicate on both builds, including empty rows and edge ties.
Preserve the existing threshold computation and edge ordering by calling that predicate during construction.
Keep the initialization on a thread with deterministic FP state (`Common/Threading/AGENTS.md:21`).

Intersect the placement's existing clamped footprint range with the row span before sampling. Leave footprint
construction, texel-center expressions, row/placement order, deterministic rotation, interpolation, and max
blend unchanged. The original guard rejected out-of-hex texels before writing, so clipped columns retain the
sea-floor initialization bit for bit. Preserve the existing RunFrameTick gate that skips an island-free live
cell. Keep the table derived and local to the IslandTerrain lifetime.

## Critical files

- Engine/Source/Frame/IslandTerrain.cpp and IslandTerrain.h own startup and the elevation builder.
- Engine/Source/Frame/GridCoord.h and Common/Math/MathUtils.cpp own the grid geometry and reference predicate.
- Engine/Source/Frame/FrameBase.cpp owns the existing build point.

## In scope

- The shared IslandTerrain constructor and storage for row spans, plus BlendPlacementIntoGrid and IslandTerrain::BuildElevationGrid.
- The relevant terrain documentation and profiling evidence for grid build and one-time span initialization.

## Out of scope

- The common hex predicate, geometry, heightmap sampling arithmetic, placement and blend order, and elevation and CRC formats.
- Moving builds off-thread, changing network data, and adding unit tests.

## Risk and invariants

Tier 3. The change substitutes a boundary computation in the shared terrain path that feeds CRC-affecting simulation positions. The output must stay bit-identical. Preserve the grid's lack of wire, save, and replay representation. `Engine/Source/Frame/FrameBase.cpp:299` and `Engine/Source/Frame/CellStaticData.cpp:8` describe and implement those contracts.

## Verification

- Before editing, run the existing `cell_coordinate_probe` on BES Debug client and server with the same packed
  data and record `placements.crc` and `elevation.crc`. Cover centered rotated terrain, multi-island overlap,
  edge and margin placements, a neighbour-only placement list and an island-free coord. Require the same CRCs
  before and after and between endpoints. Use temporary exact comparisons for synthetic boundary cases that the
  sampled coords cannot exercise. Exhaustively verify span membership against the original predicate at every
  grid center. Verify the live island-free path still leaves the grid empty and a Debug replay completes
  playback with per-tick checks and no desync across a cell transition.
- Before and after, profile the same representative island-rich grid-build scenarios on both executables with matched warmed repetitions. Record the distribution of grid build times and measure one-time table construction separately. Use existing profiling where it exposes the phase, with temporary timing only for missing observations. Require a representative gain outside measurement variability and remove temporary diagnostics before landing.
- Use the compile and agent-harness skills for BES client and server Profile x64 and Debug x64 evidence.

## Execution card

### What does this plan do?

It precomputes the hexagon's accepted columns for each elevation-grid row. Both builds clip each placement's row to that span before sampling.

### Why this is good for the codebase

The same margin-grown boundary is evaluated again for every placement and cell. Sharing one immutable table removes that repeated work from grid construction while keeping identical grid values for simulation.

- Goal: Remove repeated per-placement boundary work on both builds.
- Out of scope: Wire and save layout, shape and sampling math, and client scheduling.
- Tier trigger: Tier 3, a changed calculation feeding deterministic simulation (`Engine/Source/Frame/IslandTerrain.cpp:381`).
- Interfaces and invariants: Same predicate and FP path for span generation, identical grids and CRCs, empty grid for an island-free cell.
- Acceptance checks: Debug coordinate-probe and Profile timing baselines before the first edit. Exhaustive span membership, matching elevation CRCs on both builds, exact synthetic checks where needed, a clean Debug replay loop and a measured Profile gain.
- Roles: Plan alternatives, simplicity review, plan audit, grill, implementation and propagation, C++ correctness/style/comment/over-engineering reviews, adversarial review, compile and agent harness, and acceptance verification.

