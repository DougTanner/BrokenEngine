<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:50:40.053Z","dependsOn":[]} -->
# Let the Westward Shadow March Reach Elevation Column 0

## Context

Found by the architecture review phase of a deep analysis of
`Engine/Source/Graphics/Render` (the directory, not its subdirectories). Line
numbers are from commit `c675b6b2`; locate by symbol if they moved.

`Engine/Data/Shaders/Shadow/Shadow.comp:38-40` marches `i` from
`x + iShadowStartOffset + iShadowIncrement` while `i != iEnd`, with `iEnd` =
`globalLayout.iShadowElevationSize`. `PopulateShadowSunExtension`
(`Engine/Source/Graphics/Render/GlobalUniforms.cpp:388`) publishes the loop
parameters:

- East (`:410-412`): end = elevation texture width, increment +1. The last
  column sampled is width - 1, the texture's last column.
- West (`:422-424`): end = 0, increment -1. The last column sampled is 1;
  column 0 is never an occluder.

So the two directions are not mirror images. Nothing in
`Engine/Data/Shaders/Shadow/AGENTS.md` `## Terrain Shadow Contract` says the
exclusion is intended.

Size of the effect: skipping column 0 equals an elevation extension one texel
narrower, weighted by a distance falloff that is near zero at that range.
`fShadowDistanceFalloff` ends the march earlier (`Shadow.comp:43`) unless half
the shadow-area width falls below `gShadowDistanceFalloff`, which can happen
near the minimum framing; that was not evaluated against the field of view.
No visible defect was shown. This is the lowest-priority item of the
analysis.

Unmet criterion: the westward march samples through elevation column 0,
matching the eastward march through the last column.

## Design

Recommended: change the literal at `GlobalUniforms.cpp:422` from `0` to `-1`.
With end = -1 and increment -1 the loop samples column 0 and terminates; the
start index is never below 0 because `iShadowStartOffset` is the elevation
extension half-width, at least 1.

## Critical files

- `Engine/Source/Graphics/Render/GlobalUniforms.cpp`

## In scope

- `PopulateShadowSunExtension`: the `iShadowElevationSize` assignment in the
  westward branch.

## Out of scope

- Renaming or commenting `iShadowElevationSize` in
  `Engine/Data/Shaders/ShaderGlobalLayout.h`; either touches the shared
  layout header for wording only, and the field carries no comment the new
  literal would make wrong.
- `Shadow.comp` and the eastward branch.
- The distance falloff and the shadow area extension.

## Risk tier

Tier 2 — scoped behavior. Trigger: one client-only render path computes a
different result (one more occluder column in the westward terrain shadow
march). No layout field, type, or order changes. Render is outside the CRC:
no determinism, serialization, wire, save, replay, or threading exposure.

## Acceptance criteria

- The westward branch publishes -1 and the eastward branch is unchanged.
- `/compile` builds the BrokenEngineSandbox client and ModelViewer.
- Through `/agent-harness`, a screenshot with the sun in the western half of
  the sky shows terrain shadows with no new artifact at the western edge of
  the view. The evidence behind this Plan records no harness setup.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

- The one-literal fix is the author's recommendation, not a user decision.
