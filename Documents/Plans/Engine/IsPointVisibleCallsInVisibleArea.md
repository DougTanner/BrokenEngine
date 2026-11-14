<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:53:42.003Z","dependsOn":[]} -->
# IsPointVisible Calls CameraBase::InVisibleArea

## Context

Found by a deep analysis of `Engine/Source/Graphics` (direct files only) and
verified independently. Pre-existing at commit `401e4b75`; line numbers are
from that commit, so locate by symbol if they moved. `G/` abbreviates
`Engine/Source/Graphics/`.

`IsPointVisible` (`G/GraphicsUtils.cpp:98-102`) stores the position and
hand-writes the four comparisons against `gpCamera->mf4RenderVisibleArea`.
`CameraBase::InVisibleArea` (`G/CameraBase.h:216-219`) is the same rectangle
test, written as the negated disjunction of the opposite comparisons, with
margins that default to zero. Both are live: `IsPointVisible` has five callers
(`WindTrailsRender.cpp:97`, `WindRadialsRender.cpp:76`,
`SmokeTrailsRender.cpp:87`, `PuffsRender.cpp:84`,
`PointLightsRender.cpp:109`), and `InVisibleArea` is called directly by other
render files and `G/Islands.cpp:252`. The visible-area layout (minimum x,
maximum y, maximum x, minimum y) and the edge rule are therefore encoded
twice.

For ordered values the two tests are equal. They differ for a NaN coordinate:
every comparison is false, so `IsPointVisible` returns false (the point is
culled) and `InVisibleArea` returns true (the point is kept). Nothing in the
tree documents the reject as intended.

Unmet criterion: one owner of the visible-rectangle test. Authority: root
`AGENTS.md` `## Directives` (DRY).

## Design

Recommended: keep `IsPointVisible`'s signature and its store into
`rf4OutPosition`, and replace its comparison with a call to
`gpCamera->InVisibleArea(gpCamera->mf4RenderVisibleArea, rf4OutPosition)`.
Callers do not change.

The NaN decision, stated so the plan review at claim time can confirm or
overturn it: the author recommends accepting the flip, so a NaN position
passes this cull and is written to the GPU buffer where today it is dropped.
Rationale: a NaN position is already a defect upstream of the cull, the
reject is an accident of how the comparison was written and not a documented
contract, and the direct `InVisibleArea` callers already behave this way. If
the review wants the reject kept, this Plan is not worth doing: keeping it
means a second comparison body, which is the duplication the Plan removes.

## Critical files

- `Engine/Source/Graphics/GraphicsUtils.cpp`

## In scope

- The body of `IsPointVisible`.

## Out of scope

- `IsPointVisible`'s signature and its five callers.
- `CameraBase::InVisibleArea` and its direct callers.
- Any NaN check or sanitizing of positions.

## Risk tier

Tier 2 — scoped behavior. Trigger: one subsystem's runtime behavior changes
for one input class — a NaN position is no longer culled by these five
client render paths. Client render only: no determinism/CRC, serialization,
wire, replay, or threading exposure.

## Acceptance criteria

- `IsPointVisible` holds no comparison against `mf4RenderVisibleArea`
  members and calls `InVisibleArea`.
- The BrokenEngineSandbox client and ModelViewer build.
- Through `/agent-harness`, a screenshot of a scene with wind trails, smoke
  trails, puffs, and point lights at the screen edge matches one taken before
  the change. The supplied evidence records no harness setup.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

- `Documents/Plans/Engine/RemoveGraphicsDebugNamesSet.md` edits another
  function of `GraphicsUtils.cpp`.
