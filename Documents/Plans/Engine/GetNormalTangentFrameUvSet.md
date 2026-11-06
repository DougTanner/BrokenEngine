<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-05T13:56:43.279Z","dependsOn":[]} -->
# Build the GetNormal tangent frame from the sampled UV set

## Context

`GetNormal` must build its tangent frame from the UV set the normal texture is
sampled with. It does not: `tex_dx` and `tex_dy` always read `f2InUV`
(`Engine/Data/Shaders/Model/Model.frag:156-157`), while the sample at
`Engine/Data/Shaders/Model/Model.frag:177` uses
`getUV(material.iNormalTextureSet)`. `getUV` selects among five interpolated
sets (`Engine/Data/Shaders/Model/Model.frag:86-92`), and
`DataPacker/Source/ExportJobs/Scene/GltfSceneDecoder.cpp:516` carries the
authored `texCoord` into `iNormalTextureSet`. Every other material texture
already reads its own set. The mismatch is present at commit
`5c2e6a8d3c22cc8815256342cd63ec7fe8f4d8b3`.

An adversarial review of glTF normal strength work found this; it is outside
that change's boundary and failed none of its acceptance criteria.

No tracked model shows the defect today. The only tracked model with a nonzero
normal `texCoord` is Spaceship
(`Projects/BrokenEngineSandbox/Data/Models/Spaceship/scene.gltf:225-228`, set
3), and across its one normal-mapped primitive `TEXCOORD_3` (accessor 6) equals
`TEXCOORD_0` (accessor 3) to within 0.000001 per component. Every other tracked
normal-mapped glTF uses set 0, as does the Molnia GLB's material. The defect
appears only for a model whose normal set differs from set 0.

## Design

The author's recommendation is to compute the derivatives from
`getUV(material.iNormalTextureSet)`, reusing the sampled coordinate rather than
adding a second selection. `iNormalTextureSet` is already known non-negative at
that point. The `det` test and the frame math stay as they are.

## Critical files

- `Engine/Data/Shaders/Model/Model.frag` — `GetNormal`.

## In scope

- `tex_dx` and `tex_dy` in `GetNormal`, and the placement of the UV selection
  they share with the normal texture sample.

## Out of scope

No change to `getUV`, the tangent-frame math, the bitangent sign, the
degenerate-frame fallbacks, vertex inputs, packed material data, DataPacker,
authored models or textures, or other shaders. No new fixture model or harness
command, and no unit tests.

## Acceptance criteria

- The diff shows the derivatives and the normal texture sample reading the same
  UV set in `GetNormal`.
- `Model.frag` compiles through the ordinary DataPacker shader export via
  `/compile`.
- Spaceship in ModelViewer through `/agent-harness` is visually unchanged under
  unchanged framing and lighting, since its two sets match. The supplied
  evidence records no ordered harness setup commands; establish and record the
  ordinary setup during execution.

## Notes

Future implementation is Tier 2: trigger is one subsystem's runtime behavior,
confined to one client shader function, with no CPU/GPU layout change. Model
shading is outside PostRender state, so determinism/CRC, replay, wire, threading
and allocation are unaffected. No serialization change or `kiVersion` bump: the
shader recompiles through its dependency fingerprint. No prerequisite Plan or
mandatory coordination constraint was identified.
