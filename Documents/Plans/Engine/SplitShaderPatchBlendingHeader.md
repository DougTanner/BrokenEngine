<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-13T23:06:15.884Z","dependsOn":[]} -->
# Move randomized patch blending out of ShaderFunctions.h

## Context

`Engine/Data/Shaders/ShaderFunctions.h` measures 6223 `bt-token-v1` tokens
(489 lines) with `.agents/scripts/Measure-Tokens.ps1`, over the `.h` reduction
threshold that `.agents/skills/reduce-file/references/worker.md` `## Steps`
step 2 owns. Two review sessions logged it as a size observation
(`.agents/skills/glsl-review/references/worker.md` routes a shader header over
that threshold to follow-up work).

Forty-five shader entry points include `ShaderFunctions.h`. One self-contained
block in it, the randomized patch blending group at lines 87-234, measures
1661 tokens and is used by the Water family only:

- `struct PatchTriangle` (lines 87-96) — named by `Water/Water.frag:72`,
  `Water/WaterNormalSampling.h:8,27`, and `Water/WaterSpecular.h:9`.
- `PatchVertexOffset` (98-117) — called only by `MakePatchTriangle`.
- `MakePatchTriangle` (119-155) — called by `Water/WaterNormalSampling.h:14,
  18,22` and by the two functions below.
- `TextureGradPatched` (157-183) — called by `Water/Water.frag:96,104,122,128`.
- `WaterNormalGradPatched` (185-209) — called by
  `Water/WaterNormalSampling.h:66`.
- `TextureLodPatched` (211-234) — called by `Water/WaterFoam.comp:39`.

`Water/WaterNormalSampling.h` and `Water/WaterSpecular.h` are included only by
`Water/Water.frag` (lines 39-40). So exactly two shader entry points,
`Water/Water.frag` and `Water/WaterFoam.comp`, need the block; the other
entry points parse it for nothing, and every reader of the shared header pays
for it. No shader outside `Engine/Data/Shaders/` uses any of these symbols.

The block reads nothing else from `ShaderFunctions.h` except
`DecodeWaterNormal` (lines 80-85), which `WaterNormalGradPatched` calls. No
function left in `ShaderFunctions.h` reads anything in the block.

## Design

Recommended: move lines 87-234 of `ShaderFunctions.h` — the block comment that
opens with "Randomized patch blending" through the closing brace of
`TextureLodPatched` — unchanged into a new top-level header
`Engine/Data/Shaders/ShaderPatchBlending.h`, and include it from the two
entry points that use it.

1. Create `Engine/Data/Shaders/ShaderPatchBlending.h` holding the moved text
   byte for byte, in the same order. Like `ShaderFunctions.h` and
   `ShaderRandom.h` it has no include guard, no `#include`, no BOM, and LF line
   endings; entry points own include order.
2. Delete the block from `ShaderFunctions.h`, leaving one blank line between
   `DecodeWaterNormal` and the comment above `SunLighting`.
   `DecodeWaterNormal` stays: it is the "normal mapping" helper the shared
   header documents, and the new header is always included after
   `ShaderFunctions.h`.
3. Add `#include "ShaderPatchBlending.h"` directly after
   `#include "ShaderFunctions.h"` in `Water/Water.frag` (line 4) and
   `Water/WaterFoam.comp` (line 4). In `Water.frag` this precedes the
   `WaterNormalSampling.h` include at line 39, which needs `PatchTriangle`.
   The existing `-I <data root>/Shaders` search path in
   `DataPacker/Source/ExportJobs/ExportShader.cpp` resolves it the same way it
   resolves `ShaderFunctions.h`, and its `-MD` dependency capture picks the new
   header up with no DataPacker change.
4. Project membership through `/update-vcxproj`: add the new header to
   `BrokenEngineSandbox.vcxproj` and its `.filters` file the same way the
   sibling `ShaderRandom.h` is listed (filter `Engine\Data\Shaders`).
5. Documentation, through `/update-claude-docs`:
   `Engine/Data/Shaders/AGENTS.md` `## Shared Contract` — drop "randomized
   patch blending" from the `ShaderFunctions.h` ownership bullet, and name
   `ShaderPatchBlending.h` as its owner in the "Randomized patch blending"
   bullet. `Engine/Data/Shaders/ShaderGlobalLayout.h` line 6 — the comment
   "(ShaderFunctions.h MakePatchTriangle)" names the new header instead.

Why a top-level header and not a `Water/` one: the mechanism is not
water-specific. Its CPU side is the generic `PopulatePatchLattice`
(`Engine/Source/Graphics/Render/Render.h`), its layout is the generic
`PatchLatticeLayout`, and `Engine/Data/Shaders/AGENTS.md` documents it at the
shared layer. `ShaderRandom.h` is the precedent for a top-level helper header
that only some entry points include.

Why this block and not the lighting or smoke groups: those are used across the
Model, Terrain, Water, Lighting, Particles, Smoke, and Wind families, so moving
them would add includes to many entry points. The patch block is the only
group whose users are two entry points, and it alone brings the file under the
threshold.

Expected sizes: `ShaderFunctions.h` about 4560 tokens (from 6223);
`ShaderPatchBlending.h` about 1660.

## Critical files

- `Engine/Data/Shaders/ShaderFunctions.h`
- `Engine/Data/Shaders/ShaderPatchBlending.h` (new)
- `Engine/Data/Shaders/Water/Water.frag`
- `Engine/Data/Shaders/Water/WaterFoam.comp`
- `Engine/Data/Shaders/ShaderGlobalLayout.h` (one comment)
- `Engine/Data/Shaders/AGENTS.md`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj`
  and `BrokenEngineSandbox.vcxproj.filters`

## In scope

- `ShaderFunctions.h`: remove `PatchTriangle`, `PatchVertexOffset`,
  `MakePatchTriangle`, `TextureGradPatched`, `WaterNormalGradPatched`,
  `TextureLodPatched`, and their comments (baseline lines 87-234).
- `ShaderPatchBlending.h`: new file holding exactly that text.
- `Water/Water.frag` and `Water/WaterFoam.comp`: one added `#include` line
  each.
- `ShaderGlobalLayout.h` line 6: the header name inside the
  `PatchLatticeLayout` comment.
- `Engine/Data/Shaders/AGENTS.md` `## Shared Contract`: the two bullets that
  name `ShaderFunctions.h` as the owner of randomized patch blending.
- Project and filter entries for the new header.

## Out of scope

- Any change to the moved code: no rename, no reformatting, no signature or
  behavior change, no comment rewording.
- Moving `DecodeWaterNormal`, or any lighting, smoke, shadow, wind, or
  transform helper.
- The `WorldToVisibleArea` reciprocal change, the `ShaderGlobalLayout.h` size
  observation, and the `Model/Model.frag` observation; each is a separate
  candidate.
- The stale line reference in the comment at `Water/Water.frag:186`.
- DataPacker, C++ sources, layout fields, bindings, and `.pack` formats.

## Risk tier

Tier 1 — mechanical, per `.agents/references/risk-tiers.md`: a
behavior-preserving move of shader text plus project membership and
documentation. No layout, binding, CPU/GLSL contract, determinism, wire,
serialization, or threading surface changes; the two entry points see the same
declarations in the same order as before.

## Acceptance criteria

- `ShaderFunctions.h` measures at or below the `.h` threshold with
  `.agents/scripts/Measure-Tokens.ps1`.
- The diff shows the moved text as a pure move: the removed lines of
  `ShaderFunctions.h` equal the added lines of `ShaderPatchBlending.h`.
- None of the six moved symbols remains in `ShaderFunctions.h`, and only
  `Water/Water.frag` and `Water/WaterFoam.comp` include the new header.
- The shader export through `/compile` compiles every shader with no error;
  this is the decisive check that no other entry point depended on the block.
- Project membership validation through `/update-vcxproj` passes.

## Notes

- Logged in `Documents/Investigations/Engine/RefactorCandidates.md` as two
  size-observation rows for this file (6161 and 6223 tokens).
- The one-line comment edit in `ShaderGlobalLayout.h` touches a file that has
  its own separately logged size observation; if a Plan splitting that file
  lands first, apply the comment edit wherever `PatchLatticeLayout` then lives.
