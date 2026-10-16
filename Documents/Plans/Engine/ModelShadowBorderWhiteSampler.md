<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-15T20:46:52.517Z","dependsOn":[]} -->
# Sample the terrain shadow in Model through the border-white sampler

## Context

`Engine/Data/Shaders/Shadow/AGENTS.md` `## Terrain Shadow Contract` requires every terrain-shadow consumer to read the final texture through the border-white sampler. The terrain shadow stores `1 - shadow`, so clamp-to-border-white sampling outside texture coverage degrades to no shadow instead of an artifact (`:9`, `:12`).

Terrain and Water follow the rule: both bind `mShadowBlurTexture` with `kSamplerBorderWhite` (`Engine/Source/Graphics/Managers/WorldLightingShadowPipelines.cpp:292`, `:359`). The Model pipeline does not. `ModelPipeline::Create` binds `mShadowBlurTexture` with plain `kCombinedSamplers` and no sampler flag (`Engine/Source/Graphics/Objects/ModelPipeline.cpp:36-37`), so `TextureManager::GetSampler` returns its unflagged default, `kSamplerSlotLinearClamp` (`Engine/Source/Graphics/Managers/TextureManager.cpp:496-498`).

The two samplers differ (`TextureManager.cpp:368-415`):

- LinearClamp uses `CLAMP_TO_EDGE` with anisotropy off, so a sample outside coverage repeats the edge texel's shadow.
- BorderWhite is built from the Clamp sampler state with `CLAMP_TO_BORDER` and an opaque white border. It also carries the player anisotropy and mip LOD bias settings, which Terrain and Water already receive on the same texture.

`Model.frag` samples through the same `SampleTerrainShadow` helper, which calls `textureLod` at LOD 0 (`Engine/Data/Shaders/ShaderFunctions.h:219-222`), at `WorldToVisibleArea(f3InWorldPosition, f4ShadowArea)` (`Engine/Data/Shaders/Model/Model.frag:261-262`).

The terrain shadow update cadence Plan placed this deviation out of its scope. It predates that change.

## Design

The author recommends adding `DescriptorFlags::kSamplerBorderWhite` to the Model shadow descriptor's flags at `ModelPipeline.cpp:36`, matching Terrain and Water. Rationale: the subsystem contract outranks the current code, and Terrain and Water already use the identical binding. Model shadows then fall to "lit" outside coverage, the same as terrain and water beneath them. Inside coverage the result is unchanged except for any anisotropy effect, which the other two consumers already receive.

Risk tier: Tier 2 (`.agents/references/risk-tiers.md`) — trigger: scoped client render behavior in one subsystem (sampler choice for one descriptor). No determinism/CRC, serialization, `.pack`, wire, threading, or affinity exposure, and no shader source change. Render output is outside the CRC.

## Critical files

- `Engine/Source/Graphics/Objects/ModelPipeline.cpp`

## In scope

- The `flags` of the shadow descriptor (`descriptorInfos.at(i + 2)`) in `ModelPipeline::Create` (`ModelPipeline.cpp:36`).

## Out of scope

- The lighting (`i + 1`) and smoke (`i + 3`) descriptors in the same block, and every other sampler choice.
- `TextureManager` sampler creation and `GetSampler`.
- `Model.frag`, `SampleTerrainShadow`, and the Shadow `AGENTS.md` contract text.
- Object shadows (`ModelPipelineShadow`, the object-shadow textures).

## Acceptance criteria

1. The Model shadow descriptor carries `kSamplerBorderWhite`, like the Terrain and Water shadow descriptors.
2. The client builds.
3. An `/agent-harness` screenshot of a fixed scene with lit models shows no visible change in model shading inside the shadow area, compared with a pre-change screenshot of the same scene.

## Notes

- `Documents/Plans/Engine/SmokePerFlightPlane.md` edits the adjacent smoke descriptor (`ModelPipeline.cpp:40-41`). The overlap is textual only.
