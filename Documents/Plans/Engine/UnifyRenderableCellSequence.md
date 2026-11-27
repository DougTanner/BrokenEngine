<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-25T20:50:30.067Z","dependsOn":["Documents/Plans/Engine/SkipEmptyLightingSpread.md"]} -->
# Unify Renderable-Cell Sequence Across BeginRender and Draw

## Context

In `Engine/Source/Graphics/Render/MainUniforms.cpp`, `RenderFrameMain`
(`:268-309`) sizes collection storage buffers and fills them from two different
cell walks:

- `BeginFrameInterpolateRender` (`:255-260`, called at `:281`) passes
  `rActiveCoordinates` as-is to `PublishPopulationCounters` (`:214-252`),
  `ForEachBeginRender` (`Engine/Source/Frame/FrameUtils.h:187-196`), and
  `game::FrameInterpolate::BeginRender`
  (`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:626-659`). Each
  collection's `BeginRender` sums `iCapacity` across `rActiveCoordinates` via
  `AccumulateRenderCapacity`
  (`Engine/Source/Frame/Collections/Collection.h:384-396`), and
  `WindTrailsInterpolate::BeginRender` (`WindTrailsRender.cpp:28`) prunes stale
  render state with `EraseStaleRenderState` (`Collection.h:401-410`) over the
  same list.
- The draw loop (`MainUniforms.cpp:284-307`) visits `cameraCoordinate`
  explicitly first (`RenderFrame(cameraCoordinate)` at `:299`) and then walks
  `rActiveCoordinates` skipping `cameraCoordinate` (`:300-307`).

Because sizing and drawing do not iterate one shared sequence of renderable
cells, any frame where `cameraCoordinate` is present in `rRenderInterpolates`
but absent from `rActiveCoordinates` would draw a cell whose capacity was never
accumulated in `BeginRender` (and whose counts were omitted from population
counters and stale-state pruning). Even when `cameraCoordinate` is in
`rActiveCoordinates`, capacity safety relies on an unstated caller invariant
rather than construction, leaving defensive per-cell capacity `ASSERT`s in
`PuffsRender.cpp:55` and `SmokeTrailsRender.cpp:56`, as well as repeated
`rRenderInterpolates.find(...)` lookups across every `BeginRender` and the draw
loop.

## Design

Build one camera-first sequence of renderable cells once per frame in
`RenderFrameMain` (`Engine/Source/Graphics/Render/MainUniforms.cpp`) and feed
that exact sequence to both `BeginFrameInterpolateRender` and the per-cell draw
loop:

- Author's recommendation: build a `std::span<const game::FrameInterpolate*
  const>` in `RenderFrameMain` using `common::ScopedWorkbufferArena` (matching
  `activeIslandCells` in `Engine/Source/GameBase.cpp:783-792`). Look up
  `cameraCoordinate` in `rRenderInterpolates` first and append its pointer if
  found, then walk `rActiveCoordinates`, skipping `cameraCoordinate` and any
  entry absent from `rRenderInterpolates`. This resolves each hash-map lookup
  once per frame and guarantees by construction that `BeginRender`, population
  counters, stale-state pruning, and the draw loop visit the exact same cells
  in the exact same camera-first order.
- Update `BeginFrameInterpolateRender`, `PublishPopulationCounters`,
  `ForEachBeginRender` (`FrameUtils.h:187-196`), `AccumulateRenderCapacity` and
  `EraseStaleRenderState` (`Collection.h:384-410`), the eight engine collection
  `BeginRender` hooks, `game::FrameInterpolate::BeginRender`
  (`Projects/BrokenEngineSandbox/Source/Frame/Frame.h:48`, `Frame.cpp:626`),
  and the four game model collection `BeginRender` hooks (`Battleships`,
  `Fighters`, `Missiles`, `Omnis`) to accept the unified renderable-cell
  sequence instead of `(rRenderInterpolates, rActiveCoordinates)`.
- Keep the empty-sequence contract from
  `Engine/Source/Graphics/Render/AGENTS.md`: when the sequence is empty,
  `BeginFrameInterpolateRender` and `EndFrameInterpolateRender` still run so
  every indirect count and per-render profile counter zeroes cleanly.
- Author's recommendation: once every drawn cell is guaranteed by construction
  to have contributed its `iCapacity` to `AccumulateRenderCapacity`, remove the
  now-redundant defensive capacity `ASSERT(siRendered + rCurrent.iCount <=
  iBufferCapacity)` checks in `PuffsRender.cpp:55` and
  `SmokeTrailsRender.cpp:56`. Leave sibling engine collection renderer capacity
  `ASSERT`s and `DynamicPipelines.h:65` intact.

## Critical files

- `Engine/Source/Graphics/Render/MainUniforms.cpp`
- `Engine/Source/Frame/Collections/Collection.h`
- `Engine/Source/Frame/FrameUtils.h`
- `Engine/Source/Frame/Collections/AreaLights/AreaLights.h` and
  `AreaLightsRender.cpp`
- `Engine/Source/Frame/Collections/Billboards/Billboards.h` and
  `BillboardsRender.cpp`
- `Engine/Source/Frame/Collections/HexShields/HexShields.h` and
  `HexShieldsRender.cpp`
- `Engine/Source/Frame/Collections/PointLights/PointLights.h` and
  `PointLightsRender.cpp`
- `Engine/Source/Frame/Collections/Puffs/Puffs.h` and `PuffsRender.cpp`
- `Engine/Source/Frame/Collections/SmokeTrails/SmokeTrails.h` and
  `SmokeTrailsRender.cpp`
- `Engine/Source/Frame/Collections/WindRadials/WindRadials.h` and
  `WindRadialsRender.cpp`
- `Engine/Source/Frame/Collections/WindTrails/WindTrails.h` and
  `WindTrailsRender.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.h` and `Frame.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.h`
  and `BattleshipsRender.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.h`
  and `FightersRender.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.h`
  and `MissilesRender.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.h` and
  `OmnisRender.cpp`

## In scope

- Constructing one camera-first sequence of renderable cells in
  `RenderFrameMain` (`Engine/Source/Graphics/Render/MainUniforms.cpp:268-309`)
  and passing it to `BeginFrameInterpolateRender`, `PublishPopulationCounters`,
  and the per-cell draw loop.
- Updating `ForEachBeginRender`, `AccumulateRenderCapacity`,
  `EraseStaleRenderState`, engine collection `BeginRender` signatures and
  implementations, and `game::FrameInterpolate::BeginRender` plus game
  collection `BeginRender` signatures and implementations to consume that
  unified sequence.
- Removing the redundant defensive capacity `ASSERT`s in `PuffsRender.cpp:55`
  and `SmokeTrailsRender.cpp:56`.

## Out of scope

- Removing capacity `ASSERT`s outside `PuffsRender.cpp:55` and
  `SmokeTrailsRender.cpp:56` (including sibling engine collection renderers and
  `DynamicPipelines.h:65`).
- Changing simulation, interpolation, CRC, serialization, `Frame::kiVersion`,
  network, or `BT_SERVER` code paths.
- Changing how `GameBase::Render` or `Main.cpp` builds `rActiveCoordinates` or
  `activeIslandCells`.
- Changing `DebugRender` (`MainUniforms.cpp:314-320`), `RenderWaterMain`,
  `RenderLightingMain`, or `RenderLightingSpreadIndirect`.
- Changing `BufferManager::GetDynamicStorageBuffer` or
  `DynamicPipelines::RenderModelPipelines` buffer-growth policy.

## Risk triggers and invariants

- Change Workflow Tier 3: spans independently owned subsystems by changing the
  `BeginRender` contract across engine rendering (`MainUniforms.cpp:259`,
  `FrameUtils.h`, `Collection.h`) and game code
  (`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:626` and game
  collection `BeginRender` hooks). Client-only (`BT_CLIENT`); no
  determinism/CRC, serialization/`.pack`/`kiVersion`, replay, wire, threading,
  shader, or trust-boundary exposure.
- Camera cell must remain index 0 in the unified sequence when present in
  `rRenderInterpolates`.
- Every cell drawn in `RenderFrameMain` must appear in the exact same sequence
  passed to `BeginFrameInterpolateRender`, with no duplicate entries.
- Empty renderable-cell sequence must still run `BeginFrameInterpolateRender`
  and `EndFrameInterpolateRender` so indirect counts and per-render profile
  counters publish zero.
- No heap allocation in the render loop: build the per-frame sequence in
  `common::gpThreadLocal->mWorkbuffer` via `ScopedWorkbufferArena`.

## Acceptance criteria

1. `RenderFrameMain` builds one camera-first, deduplicated sequence of
   renderable cells and passes it to both `BeginFrameInterpolateRender` and the
   per-cell draw loop; no `BeginRender` helper or draw loop independently walks
   `rActiveCoordinates` or probes `rRenderInterpolates`.
2. Both `BrokenEngineSandbox` (client) and `BrokenEngineSandboxServer` build
   cleanly in Debug and Release (`x64`).
3. Live `/agent-harness` verification launches the client and server, renders
   populated gameplay cells (including smoke, lights, and ships), and confirms
   no asserts, Vulkan validation errors, or missing/ghosted collection renders.

## Notes

- Originated from an `/over-engineering-review` design suggestion noting that
  unifying the renderable-cell sequence between `BeginFrameInterpolateRender`
  and `RenderFrameMain`'s draw loop guarantees capacity coverage by
  construction and removes the need for defensive capacity guards in
  `PuffsRender.cpp:55` and `SmokeTrailsRender.cpp:56`.
- Any other game project implementing `game::FrameInterpolate::BeginRender`
  must update its signature to match the engine contract.
