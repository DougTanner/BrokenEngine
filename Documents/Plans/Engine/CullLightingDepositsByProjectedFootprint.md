<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-21T21:04:02.887Z","dependsOn":[]} -->
# Cull point and area light deposits by projected footprint

## Context

`PointLightsInterpolate::Render` skips the light at
`Engine/Source/Frame/Collections/PointLights/PointLightsRender.cpp:102-106`
when its raw center is outside `mf4RenderVisibleArea`. That happens before
projection and construction of the deposit quad. `AreaLightsInterpolate`
likewise rejects an unprojected AABB against the live rectangle at
`AreaLightsRender.cpp:144-148`, before projecting the four deposit corners.
An offscreen light can land in the padded lighting target and contribute to
the visible scene. Lighting deposit shaders already fade at that target's
boundary. The visible sprites have a separate live-view purpose.

## Design

Use independent sprite and deposit predicates. Keep the point sprite's
current center test and the area sprite's current unprojected union AABB
test against the live view, along with their texture, geometry and rotation.
For a deposit on the refreshing layer, apply the established plane projection
and camera-cell rebasing, then test the actual submitted footprint against
the current latched, padded, texel-snapped deposit rectangle. Use the point
quad's projected center plus/minus its actual half extent, and the projected
four-corner AABB for area lights after the current sizing/shape adjustments.
Re-use the projected coordinates for both test and vertex publication.

Publish the latched area to Main through the existing CPU presentation state
if suitable, keeping one source of truth for the deposit rectangle. The
Global path sets the refresh and area before Main builds deposit lists. Use
the render order at execution time and preserve the GPU upload direction.
On skipped frames and nonrefreshing layers, emit no deposit. Keep camera-cell
rebasing single and use the same coordinate frame for comparisons. Apply the
equivalent projection and latch when executing against one layer.

## Critical files

- `Engine/Source/Frame/Collections/PointLights/PointLightsRender.cpp:84-179`
  and `Engine/Source/Frame/Collections/AreaLights/AreaLightsRender.cpp:83-213`.
- `Engine/Source/Graphics/Render/LightingUniforms.cpp:21-103`, `Render.h`,
  `GlobalUniforms.cpp:598-603`, `MainUniforms.cpp:272-317`, and
  `Engine/Source/Graphics/Graphics.cpp:220-243` for Global/Main order.
- `Engine/Source/Graphics/GraphicsUtils.cpp:73-77,98-106`, plus the
  lighting deposit shaders for the existing edge fade.

## In scope

- The `Render` deposit visibility decisions for point and area lights and
  the needed CPU refresh-area access from `PopulateLightingParameters`.

## Out of scope

- Hex-shield deposit culling, sprite culling changes, projection formulas,
  lighting layer count, cadence, edge-fade tuning, and unit tests.

## Risk triggers and invariants

Tier 2 for scoped lighting visibility. Test overlap after projection and
rebase, using the snapped area for the same refresh. Keep per-collection
capacity, append and indirect instance counts paired. Preserve submission of
sprite-only and deposit-only cases independently.

## Acceptance criteria

1. On a fixed-view camera sweep, a point with its center outside the view
   and an area light with unprojected bounds outside the view still deposit
   when the projected quad intersects the padded target. When that quad
   leaves the padded target, its deposit stops. A sprite-only case keeps its
   visible quad. Capture `Lighting` and the lit scene before and after.
2. Repeat at different light heights and camera tilt, near a camera-cell
   crossing and a snapped texel boundary, and with a skipped refresh. The
   deposit follows the refreshing area and existing edge fade without a
   jump at the live edge. Visible sprites and held outputs keep their
   existing behavior. Cover both layers if still present at execution.
3. `/compile` builds the BrokenEngineSandbox client and ModelViewer. Use
   `/agent-harness` for the sweep, the
   continuity probe and target dumps. The supplied review recorded code
   inspection and no live capture sequence.

## Coordination

- With `SingleLayerWorldLighting.md`, preserve independent sprite and
  projected deposit tests when changing plane selection. Use that Plan's
  active area and base-height projection if it lands first.

## Notes

Keep the light just outside the live view but inside active render-cell
coverage so upstream cell selection is controlled during verification.
