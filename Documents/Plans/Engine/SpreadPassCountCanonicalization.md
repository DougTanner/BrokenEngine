<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:31:27.797Z","dependsOn":[]} -->
# Use one integral lighting spread-pass count

## Context

The frozen audit retained `CAI/shard-0009/001`. The UI exposes
`gSpreadPassCount` as a continuous float (`Engine/Source/Ui/LightingWrappersBase.cpp:17`;
`TweaksScreenBase.cpp:227-229`). CPU texture/command-buffer setup truncates it
at `Engine/Source/Graphics/Managers/RenderTargetTexturesLighting.cpp:181` and
`CommandBufferRecordMain.cpp:125-134`, while `LightingUniforms.cpp:170-202`
normalizes with the raw float and `LightCombine.comp:78-89` truncates again.
Source/shader bytes are unchanged from baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`.

## Design

Introduce one bounded integral accessor for the spread-pass setting, canonicalize
the control at its setter/UI boundary, and use that value for texture sizing,
record-once pass count, UBO publication, combine normalization, and spread
interpolation. Preserve the existing `[1, kiMaxSpreadPasses]` range and shader
array limits; a valid setting must describe the same number of passes in every
CPU/GPU phase.

## Critical files

- `Engine/Source/Ui/LightingWrappersBase.cpp` and tweak UI registration — control boundary.
- `Engine/Source/Graphics/Render/LightingUniforms.cpp` — UBO/normalization.
- `Engine/Source/Graphics/Managers/RenderTargetTexturesLighting.cpp` and `CommandBufferRecordMain.cpp` — pass creation/recording.
- `Engine/Data/Shaders/Lighting/LightCombine.comp` and `LightingSpread.frag` — GPU consumers.

## In scope

- Canonical integral spread-pass value and all listed CPU/GPU consumers.
- Rebuild/refresh behavior when the setting changes.

## Out of scope

- Lighting quality presets, other float controls, descriptor layout, or unrelated shader math.

## Risk tier and invariants

Tier 3. Trigger: independently owned UI, CPU render setup, UBO, command buffer,
and GLSL consumers must share one cross-phase value. The active pass count,
normalization, interpolation, and shader loop are identical and integral.

## Acceptance criteria

- A fractional UI/programmatic value is canonicalized before any consumer and cannot produce a floor-count/raw-float mismatch.
- Integer values from 1 through `kiMaxSpreadPasses` retain current lighting behavior.
- Texture creation, command recording, UBO values, and both shaders observe the same count after refresh.

## Notes

Origin: `CAI/shard-0009/001`, source selector
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0009.md:28`.
No source fix, shader build, or runtime capture was performed during routing.
