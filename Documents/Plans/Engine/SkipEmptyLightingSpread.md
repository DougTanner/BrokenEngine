<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-21T21:04:07.582Z","dependsOn":["Documents/Plans/Engine/RemoveCompassHaloFromLighting.md"]} -->
# Skip spread draws when a lighting refresh has no deposits

## Context

`RenderLightingSpreadIndirect` in
`Engine/Source/Graphics/Render/LightingUniforms.cpp:328-354` schedules every
active spread draw on an enabled refresh even when the published point,
area and hex-shield deposit counts are all zero. `RecordLightingDeposit`
records those three pipeline maps. The spread render-pass load operations
already clear all six attachments when draws have zero instances, as on
disabled refreshes. Combine and temporal use the refreshing layer separately.

## Design

Collect an any-deposit predicate on the CPU in Main. Clear it before the
collection end-render phase and fold in the same effective instance counts
that point, area and hex-shield `EndRender` publish for the actual refreshing
layer. Use that frame's predicate in `RenderLightingSpreadIndirect` after all
three writers have run. Keep the existing enabled and refresh checks. Read
CPU counts at their publication site so mapped write-only indirect buffers
are never read back. Visible sprite and shield main draws remain independent.

Use zero-instance draws on empty refreshes so unconditional clears still
publish valid spread sources and debug output. Keep combine, temporal, and
history copy dispatched on every refresh, so history reprojects and fades
normally and reset still uses pure current data. On a skipped frame retain
the existing zero work publication and held result.

## Critical files

- `Engine/Source/Graphics/Render/MainUniforms.cpp:307-317`, `Render.h`, and
  `LightingUniforms.cpp:328-354`.
- `Engine/Source/Frame/Collections/PointLights/PointLightsRender.cpp`,
  `AreaLights/AreaLightsRender.cpp` and `HexShields/HexShieldsRender.cpp`,
  their `EndRender` deposit writes.
- `Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp:62-205`.

## In scope

- CPU reset and publication of the effective deposit predicate, and its
  use in spread indirect instance counts on Main.

## Out of scope

- Changing deposit culling, terrain fade, clears, command buffer topology,
  refresh/reset scheduling, combine/temporal math, and unit tests.

## Risk triggers and invariants

Tier 2 for scoped render scheduling. Main owns the predicate and writes only
the acquired frame's indirect slots. Empty lists and empty interpolates
must reset it. A skipped or nonrefreshing layer never satisfies the gate.
Maintain publication and debug behavior on every active lighting layer.

## Acceptance criteria

1. On populated-to-empty and empty-to-populated refresh sequences, the
   predicate uses current counts, all active spread draws have zero
   instances on the empty refresh, and each deposit kind by itself keeps
   the spread chain enabled. A visible sprite without a deposit does not.
2. With the predecessor and same scene and cadence, compare lighting dumps
   for fresh black/empty data, fade of previously lit history, enable and
   disable edges, re-enable, teleport/reset, skipped frames and no renderable
   coordinates. Keep resolved output, debug contents and visible sprites
   equivalent. Cover both refresh targets when two layers are present.
3. `/compile` builds the BrokenEngineSandbox client and ModelViewer. Use
   `/agent-harness` and the GPU Lighting Spread/Combine/Temporal profiles to
   verify the gate and observe measured costs for populated, empty and
   skipped refreshes. No live capture sequence was supplied. Use a frame
   capture for draw and dispatch counts if the profiles and texture dumps
   cannot distinguish the paths. The shared
   targets clear on skipped frames too, so use a known refresh to observe
   the empty branch and distinct retained output from previous lighting.

## Notes

Zero deposited instances is the trigger. Looking for black pixels after the
deposit draw would require extra GPU work and a different synchronization.
