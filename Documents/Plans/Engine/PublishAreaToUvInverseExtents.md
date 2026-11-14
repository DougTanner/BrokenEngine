<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-13T23:18:25.128Z","dependsOn":[]} -->
# Publish area-to-UV inverse extents for WorldToVisibleArea

## Context

`WorldToVisibleArea` (`Engine/Data/Shaders/ShaderFunctions.h:21-26`) maps a
world position into the [0,1] UV of a world rectangle. On every call it
computes `1.0f / (f4VisibleArea.z - f4VisibleArea.x)` and
`1.0f / (f4VisibleArea.y - f4VisibleArea.w)`. Every area any caller passes is a
`GlobalLayout` uniform, so both reciprocals are uniform-only terms recomputed
per vertex, per fragment, or per compute invocation.

`Engine/Data/Shaders/AGENTS.md` `## Shared Contract` requires such terms to be
merged CPU-side into layout fields that shaders read finished. The user ruled
this session that the rule applies to existing code too and that the existing
code's performance should be improved.

Originating gap: the refactor-observation row dated 2026-11-12 in
`Documents/Investigations/Engine/RefactorCandidates.md`.

Verified at baseline `213d2577`:

- 33 calls in 14 files under `Engine/Data/Shaders/` (13 shader files plus one
  call inside `ShaderFunctions.h` `ShadowStretchProjection`, line 47). No shader
  under `Projects/` calls the helper or reads the fields this Plan removes.
- Calls by area argument: `f4VisibleArea` 9, `f4ObjectShadowsArea` 6,
  `f4ShadowArea` 3, `f4ShadowAreaPrevious` 1, `f4LightingArea` 2,
  `f4LightingAreaPrevious` 1, `pf4LightingAreas[...]` 11.
- Four of those fields have no shader reader except `WorldToVisibleArea` calls:
  `f4ObjectShadowsArea`, `f4ShadowAreaPrevious`, `f4LightingAreaPrevious`, and
  `pf4LightingAreas`. The other three are also read raw: `f4VisibleArea` and
  `f4LightingArea` by `VisibleAreaToWorld` callers, `DebugTexture.frag`, and the
  two `Quads/Quads*VisibleArea.vert` shaders (`f4VisibleArea` also by
  `Terrain/Terrain.vert`); `f4ShadowArea` by `Shadow/ShadowTemporal.comp:35`.
- The areas are written by `PopulateShadowArea` (`GlobalUniforms.cpp:316-326`),
  `PopulateObjectShadowsArea` (`GlobalUniforms.cpp:371`), `RenderFrameGlobal`
  (`GlobalUniforms.cpp:633`), and `PopulateLightingParameters`
  (`LightingUniforms.cpp:77-98`), all under
  `Engine/Source/Graphics/Render/`. `rGlobalLayout` is write-combined mapped
  memory and is write-only (`Engine/Source/Graphics/Render/AGENTS.md`
  `## Ordering and Publication`), so a reciprocal is computed from the CPU value
  the area was written from, never read back from the layout.
- Existing inverse fields: `f2LightingAreaExtentInverse`
  (`LightingUniforms.cpp:100-105`) is `(1/(z-x), 1/(y-w))` of
  `spTemporalAreaLatches[iChainLayer].f4CurrentArea`, the same rectangle
  published as `f4LightingArea` in both branches, so it already holds exactly
  the pair the helper needs for that one area; its only reader is
  `Lighting/LightingSpread.frag:89-90`. `f2WaterFoamAreaInverseSize`,
  `f2WaterFoamPreviousAreaInverseSize`, and `pf2PreviousSmokeAreaSizeInverses`
  belong to the foam and smoke areas, which no `WorldToVisibleArea` call
  passes (the smoke one also has the opposite Y sign), so none can be reused.

## Design

Recommended shape: one new layout struct that carries everything the helper
reads, so the helper takes a single argument and there is no second value to
pair. The helper's old `vec4` overload is deleted, so a call still passing a
raw area fails to compile.

### Layout (`Engine/Data/Shaders/ShaderGlobalLayout.h`)

Add, between `PatchLatticeLayout` and `GlobalLayout`:

```glsl
// A world rectangle's world-to-UV mapping (WorldToVisibleArea), from engine::PopulateAreaToUv. The reciprocals are
// uniform-only, so they are merged CPU-side; the subtraction stays in the shader because its operand is the
// per-invocation world position.
struct AreaToUvLayout
{
	vec2 f2Origin INIT;        // (left, bottom) = (area.x, area.w)
	vec2 f2ExtentInverse INIT; // (1 / (area.z - area.x), 1 / (area.y - area.w))
};
```

`GlobalLayout` field changes, each new field at the position of the field it
follows or replaces:

| Area | Field after this Plan | Raw area field |
|---|---|---|
| visible | `AreaToUvLayout visibleAreaToUv` after `f4VisibleArea` | kept |
| object shadows | `AreaToUvLayout objectShadowsAreaToUv` | `f4ObjectShadowsArea` removed |
| shadow | `AreaToUvLayout shadowAreaToUv` after `f4ShadowArea` | kept |
| shadow previous | `AreaToUvLayout shadowAreaPreviousToUv` | `f4ShadowAreaPrevious` removed |
| lighting chain | `AreaToUvLayout lightingAreaToUv` after `f4LightingArea` | kept |
| lighting chain previous | `AreaToUvLayout lightingAreaPreviousToUv` | `f4LightingAreaPrevious` removed |
| lighting layers | `AreaToUvLayout pLightingAreaToUvs[kiLightingLayers]` | `pf4LightingAreas` removed |

`f2LightingAreaExtentInverse` is removed: `lightingAreaToUv.f2ExtentInverse`
holds the same value. Each replacing field keeps the removed field's comment
meaning (which rectangle, which refresh). The four raw fields are removed
because this change takes away their last reader; a kept raw field still has
the readers listed in `## Context`.

The struct is two `vec2`, 16 bytes with 4-byte alignment on both sides of the
scalar-layout block, the same nesting `PatchLatticeLayout` already uses. The
header is single-sourced for C++ and GLSL, so the C++ type
`shaders::AreaToUvLayout` and the field order change with the same edit.

### Helper (`Engine/Data/Shaders/ShaderFunctions.h`)

Replace the body and signature of `WorldToVisibleArea`:

```glsl
vec2 WorldToVisibleArea(vec3 f3WorldPosition, AreaToUvLayout areaToUv)
{
	return vec2(areaToUv.f2ExtentInverse.x * (f3WorldPosition.x - areaToUv.f2Origin.x), 1.0f - areaToUv.f2ExtentInverse.y * (f3WorldPosition.y - areaToUv.f2Origin.y));
}
```

The arithmetic form is the current one with the two reciprocals read instead
of computed. The name stays. `VisibleAreaToWorld` is unchanged and keeps taking
the raw `vec4`.

### Call-site edit rule

Only the second argument of each of the 33 calls changes:

- `globalLayout.f4<Name>` becomes `globalLayout.<name>ToUv` — drop `f4`,
  lower-case the first letter, append `ToUv` (for example
  `globalLayout.f4ShadowAreaPrevious` becomes
  `globalLayout.shadowAreaPreviousToUv`).
- `globalLayout.pf4LightingAreas[<index>]` becomes
  `globalLayout.pLightingAreaToUvs[<index>]`, index expression unchanged.

Sites: `Objects/HexShield.vert:96`; `Model/ModelCommon.h:116,124`;
`Model/Model.frag:260,261,262,312,314,334,340`;
`Water/WaterDisplacement.comp:41`; `Water/Water.vert:45`;
`Water/Water.frag:49,181,182,202,208,255,265`;
`Lighting/VisibleLight.frag:34`; `Lighting/LightingTemporal.comp:44`;
`Lighting/LightingSpread.frag:79`; `ShaderFunctions.h:47`;
`Terrain/Terrain.frag:131,132,136,158,159,172,183`;
`Shadow/ShadowTemporal.comp:36`; `Smoke/SmokeSpreadTwo.comp:73`;
`Particles/ParticlesUpdate.comp:63`.

One more shader edit: `Lighting/LightingSpread.frag:89-90` reads
`globalLayout.lightingAreaToUv.f2ExtentInverse.x` and `.y` in place of the
removed field.

### CPU (`Engine/Source/Graphics/Render/`)

Add one writer to `Render.h`, beside `ScaleAreaAboutCenter`, and use it for
every `AreaToUvLayout` field so the origin and the reciprocals always come
from one rectangle:

```cpp
inline void PopulateAreaToUv(shaders::AreaToUvLayout& rAreaToUv, const XMFLOAT4& rf4Area)
{
	rAreaToUv.f2Origin.x = rf4Area.x;
	rAreaToUv.f2Origin.y = rf4Area.w;
	rAreaToUv.f2ExtentInverse.x = 1.0f / (rf4Area.z - rf4Area.x);
	rAreaToUv.f2ExtentInverse.y = 1.0f / (rf4Area.y - rf4Area.w);
}
```

It only writes its first argument. Write sites, each passing the CPU value
named, never a layout field:

- `RenderFrameGlobal`, after `GlobalUniforms.cpp:633`: `visibleAreaToUv` from
  `engine::gpCamera->mf4RenderVisibleArea`.
- `PopulateObjectShadowsArea`, `GlobalUniforms.cpp:371`: the
  `f4ObjectShadowsArea` write becomes `objectShadowsAreaToUv` from
  `sf4HeldArea`.
- `PopulateShadowArea`, after the if/else at `GlobalUniforms.cpp:316-326`:
  `shadowAreaToUv` from `sTemporalAreaLatch.f4CurrentArea` and
  `shadowAreaPreviousToUv` from `sTemporalAreaLatch.f4PreviousArea`, once for
  both branches. The `f4ShadowArea` writes (319, 323) stay; the
  `f4ShadowAreaPrevious` write (324) goes.
- `PopulateLightingParameters`: in the layer loop, `LightingUniforms.cpp:95`
  becomes `pLightingAreaToUvs[i]` from `spTemporalAreaLatches[i].f4CurrentArea`;
  lines 100-105 become `lightingAreaToUv` from
  `spTemporalAreaLatches[iChainLayer].f4CurrentArea` and
  `lightingAreaPreviousToUv` from
  `spTemporalAreaLatches[iChainLayer].f4PreviousArea`. The `f4LightingArea`
  writes (81, 87) stay; the `f4LightingAreaPrevious` write (88) goes.
- `TemporalAreaLatch::Update` (`Render.h:137-162`) loses its
  `XMFLOAT4& rf4PreviousArea` out-parameter and the assignment to it; its two
  callers (`GlobalUniforms.cpp:318`, `LightingUniforms.cpp:80`) drop the
  argument. The latch's own `f4PreviousArea` holds the same value after
  `Update`, and both callers passed a layout field that no longer exists.

No clamp or guard is added. A zero-extent rectangle (a lighting layer's latch
before its first refresh) gives the same infinite reciprocal the shader
computes today, and floating-point exceptions are masked.

### Alternatives considered

- A separate `vec2` inverse field per area and a three-argument helper: the
  smallest diff, but 33 calls would each name an area and an inverse that
  nothing ties together.
- A struct holding the raw `vec4` area plus the inverse, replacing every area
  field: no duplicated origin, but every other reader of `f4VisibleArea`,
  `f4LightingArea`, and `f4ShadowArea` would be edited for no gain.
- Folding the origin into a bias (`uv = position * scale + bias`): one multiply
  cheaper, but it changes the arithmetic form and its rounding where the
  temporal passes reproject.

### Size and documentation

`ShaderGlobalLayout.h` measures 5244 `bt-token-v1` tokens (343 lines) with
`.agents/scripts/Measure-Tokens.ps1`, already over the `.h` reduction threshold
that `.agents/skills/reduce-file/references/worker.md` `## Steps` step 2 owns,
and the review that logged it found no qualifying reduction. This Plan adds a
few lines net and does not plan a split.

Documentation through `/update-claude-docs`: field names that change in
`Engine/Data/Shaders/Lighting/AGENTS.md` `## Pipeline Contracts`
(`pf4LightingAreas[k]`), `Engine/Data/Shaders/Shadow/AGENTS.md`
(`f4ObjectShadowsArea`), and `Engine/Source/Graphics/Render/AGENTS.md`
`## Ordering and Publication` (`f4ObjectShadowsArea`).

## Critical files

- `Engine/Data/Shaders/ShaderGlobalLayout.h`
- `Engine/Data/Shaders/ShaderFunctions.h`
- The 13 shader files listed under `### Call-site edit rule`
- `Engine/Source/Graphics/Render/Render.h`
- `Engine/Source/Graphics/Render/GlobalUniforms.cpp`
- `Engine/Source/Graphics/Render/LightingUniforms.cpp`
- `Engine/Data/Shaders/Lighting/AGENTS.md`, `Engine/Data/Shaders/Shadow/AGENTS.md`,
  `Engine/Source/Graphics/Render/AGENTS.md`

## In scope

- `ShaderGlobalLayout.h`: new `AreaToUvLayout`; the seven new `GlobalLayout`
  fields; removal of `f4ObjectShadowsArea`, `f4ShadowAreaPrevious`,
  `f4LightingAreaPrevious`, `pf4LightingAreas`, and
  `f2LightingAreaExtentInverse`.
- `ShaderFunctions.h`: `WorldToVisibleArea` signature and body, and the call in
  `ShadowStretchProjection`.
- The second argument of the 32 other `WorldToVisibleArea` calls, and
  `Lighting/LightingSpread.frag:89-90`.
- `Render.h`: new `PopulateAreaToUv`; `TemporalAreaLatch::Update` out-parameter
  removal.
- `GlobalUniforms.cpp`: `PopulateShadowArea`, `PopulateObjectShadowsArea`, and
  the `f4VisibleArea` write in `RenderFrameGlobal`.
- `LightingUniforms.cpp`: `PopulateLightingParameters`, lines 77-105.
- The three `AGENTS.md` field-name mentions.

## Out of scope

- `VisibleAreaToWorld` and every raw read of `f4VisibleArea`, `f4LightingArea`,
  and `f4ShadowArea`.
- Other per-invocation divides by a uniform area that do not go through the
  helper: `DebugTexture.frag:73-80,94-101`, `Terrain/Terrain.vert:100-101`,
  `Quads/QuadsVisibleArea.vert:66-67`, and
  `Quads/QuadsAxisAlignedVisibleArea.vert:72-73`.
- The water foam and smoke area fields and their inverse fields.
- Refresh cadence, latch behavior, area values, `gPresentationContinuity`, and
  any change to which rectangle a shader samples through.
- Splitting or reducing `ShaderGlobalLayout.h` or `ShaderFunctions.h`.
- A clamp or guard on the reciprocals.
- Renaming `WorldToVisibleArea`.

## Risk tier

Tier 3, per `.agents/references/risk-tiers.md`. Trigger: data layout — the
change alters the shared CPU/GLSL scalar layout of `GlobalLayout` (fields
added, removed, and reordered), which every Global-set shader and the Render
populators must agree on.

Exposure:

- Shader and build: `Engine/Data/**` shader edits need a Local generation
  build through `/compile` so every shader is re-exported against the new
  layout; a stale shader would read shifted fields. Both projects that compile
  the populators, BrokenEngineSandbox and ModelViewer, are rebuilt; the server
  includes the header and is rebuilt too.
- Client-only visual state: nothing here is simulated, CRC-checked,
  serialized, sent on the wire, saved, or replayed. No `.pack` format or
  `kiVersion` changes; the uniform buffer is sized by
  `sizeof(shaders::GlobalLayout)` (`BufferManager.cpp:316`).
- Invariants: CPU and GLSL field types, alignment, and order stay identical;
  the layout is never read back; each `AreaToUvLayout` is written from the same
  CPU rectangle as the area it stands for, on refresh and skip frames alike;
  every shader samples through the same rectangle as before.
- Results are not bit-identical to before: a CPU IEEE reciprocal replaces a GPU
  one, which may differ in the last bits. This is render-only.
- Threading, allocation, affinity: unchanged.

## Acceptance criteria

- No `1.0f /` remains in `WorldToVisibleArea`, and the helper has exactly one
  overload, taking `AreaToUvLayout`.
- A search of `Engine/` and `Projects/` finds no `f4ObjectShadowsArea`,
  `f4ShadowAreaPrevious`, `f4LightingAreaPrevious`, `pf4LightingAreas`, or
  `f2LightingAreaExtentInverse`.
- Every `AreaToUvLayout` field of `GlobalLayout` is written only through
  `PopulateAreaToUv`, and no argument to it reads `rGlobalLayout`.
- The Local generation build through `/compile` exports every shader with no
  error and builds the client, server, and ModelViewer; a missed call site
  fails here because the `vec4` overload is gone.
- `/glsl-review` reports no CPU/GLSL layout finding for the change.
- Live check through `/agent-harness`: screenshots of one scene taken before
  and after the change, at the same camera and time of day, show no visible
  difference in terrain shadow, object shadows, lighting on terrain, water, and
  models, smoke shadow, and the shoreline elevation effects on water. The
  evidence behind this Plan records no harness setup; the executing session
  picks a scene with an island, ships, and active lights, and waits for every
  chain to refresh after launch.

## Notes

- The sibling Plan `Documents/Plans/Engine/SplitShaderPatchBlendingHeader.md`
  moves `ShaderFunctions.h` baseline lines 87-234 and edits the comment at
  `ShaderGlobalLayout.h` line 6. This Plan edits `ShaderFunctions.h` lines
  21-26 and 47 and adds its struct after `ShaderGlobalLayout.h` line 14, so the
  two do not touch the same lines and need no ordering.
- The choices in `## Design` are the author's recommendation, not a user
  decision; the user's decision is that the reciprocals move to the CPU for
  every area.
