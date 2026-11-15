<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:54:03.427Z","dependsOn":[]} -->
# AnimationData::SkinnedMaterialCount Reads Its Own Material Count

## Context

Found by a deep analysis of `Engine/Source/Graphics` (direct files only) and
verified independently. Pre-existing at commit `401e4b75`; line numbers are
from that commit, so locate by symbol if they moved. `G/` abbreviates
`Engine/Source/Graphics/`.

`AnimationData::SkinnedMaterialCount(int64_t iMaterialCount)`
(`G/AnimationData.h:27`, `G/AnimationData.cpp:192-203`) takes the material
count as a parameter although the object stores it (`miMaterialCount`, set at
`AnimationData.cpp:42`) and `Load` validates `mpMaterialInfos` for exactly
that count. The loop at `:195-201` indexes pack-aliased memory by the
caller's bound.

There are two callers. `AnimationData::ReserveSkinning` passes
`miMaterialCount` (`:212`).
`Projects/ModelViewer/Source/ModelViewerApplication.cpp:586` passes
`rSceneHeader.uiMaterialCount` of the chunk keyed by the same CRC, the value
`LoadAnimationDataFromEagerChunks` passed to `Load` (`AnimationData.cpp:488`).
Both pass the validated value, so no out-of-range index is reachable today;
the defect is structural: a caller must know which count is the validated
one.

Unmet criterion: the material count has one origin.

## Design

Recommended: drop the parameter, read `miMaterialCount` in the loop, and
update the two callers. No behavior changes.

## Critical files

- `Engine/Source/Graphics/AnimationData.h`
- `Engine/Source/Graphics/AnimationData.cpp`
- `Projects/ModelViewer/Source/ModelViewerApplication.cpp`

## In scope

- The declaration and definition of `AnimationData::SkinnedMaterialCount`.
- Its call in `AnimationData::ReserveSkinning` and its call at
  `ModelViewerApplication.cpp:586`.

## Out of scope

- `AnimationData::Load` and its validation.
- Any other use of `rSceneHeader.uiMaterialCount` in ModelViewer.

## Risk tier

Tier 2 — scoped behavior. Trigger: it changes a public member function
signature that an engine caller and ModelViewer both use, which Tier 1's "no
public signature exposure" excludes; behavior is unchanged. No
determinism/CRC, serialization, wire, replay, or threading exposure.

## Acceptance criteria

- `SkinnedMaterialCount` takes no parameter and no caller passes a count.
- The BrokenEngineSandbox client and ModelViewer build.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

- `Documents/Plans/Engine/ReuseComputedInverseAndEyeNormal.md` edits other
  functions of `AnimationData.cpp`.
- `Projects/Coretide` was not searched for a caller: the private game's
  source was not available to the authoring session. The executing session
  checks it where that source is available and reports it as unconfirmed
  otherwise.
