<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:54:16.557Z","dependsOn":[]} -->
# AnimationData and Camera Clean-ups: One Material Count, Reused Inverse and Eye Normal

## Context

Found by a deep analysis of `Engine/Source/Graphics` (direct files only) and
verified independently. Pre-existing at commit `401e4b75`; line numbers are
from that commit, so locate by symbol if they moved. `G/` abbreviates
`Engine/Source/Graphics/`.

1. `AnimationData::SkinnedMaterialCount(int64_t iMaterialCount)`
   (`G/AnimationData.h:27`, `G/AnimationData.cpp:192-203`) takes the material
   count as a parameter although the object stores it (`miMaterialCount`, set
   at `AnimationData.cpp:42`) and `Load` validates `mpMaterialInfos` for
   exactly that count. The loop at `:195-201` indexes pack-aliased memory by
   the caller's bound. There are two callers.
   `AnimationData::ReserveSkinning` passes `miMaterialCount` (`:212`).
   `Projects/ModelViewer/Source/ModelViewerApplication.cpp:586` passes
   `rSceneHeader.uiMaterialCount` of the chunk keyed by the same CRC, the
   value `LoadAnimationDataFromEagerChunks` passed to `Load`
   (`AnimationData.cpp:488`). Both pass the validated value, so no
   out-of-range index is reachable today; the defect is structural: a caller
   must know which count is the validated one.
2. `AnimationData::EvaluateMaterial` inverts the same matrix twice.
   `G/AnimationData.cpp:434` computes
   `XMMatrixInverse(nullptr, matMeshWorld)` for the normal matrix, and `:441`
   computes the same inverse of the same `matMeshWorld`, not written between
   them (`:431-440`), into `matMeshWorldInverse` for every skinned material.
   It runs per material per instance per rendered frame; ModelViewer reaches
   it too (`Projects/ModelViewer/Source/ModelViewerApplication.cpp:590`).
3. `CameraBase::CalculateMatricesAndVisibleArea` re-derives a unit vector it
   just stored. `G/CameraBase.cpp:498` builds the unit vector
   `mVecToEyeNormal`, `:499` sets `mVecEyePosition` to the position plus that
   vector scaled by the eye distance, and `:502` recovers the direction by
   subtracting and normalizing into the local `vecToEyeNormal`, which only the
   cross product at `:503` uses. The eye distance is positive, so the local
   equals the member up to rounding.

Unmet criteria: the material count has one origin; the inverse is computed
once per call; the stored unit vector is used as stored. Authority: root
`AGENTS.md` `## Directives` (DRY); `.agents/references/cpp-conventions.md`
"Unit vectors stay unit".

## Design

Recommended:

1. `SkinnedMaterialCount`: drop the parameter, read `miMaterialCount` in the
   loop, and update the two callers (`ReserveSkinning` and
   `ModelViewerApplication.cpp:586`). No behavior changes.
2. In `EvaluateMaterial`, compute the inverse once into `matMeshWorldInverse`
   before the normal matrix, use it for the normal matrix and in the skinned
   block, and delete the second call. The same function runs on the same
   input, so the outputs are bit-identical.
3. In `CalculateMatricesAndVisibleArea`, delete the local at `:502` and pass
   `mVecToEyeNormal` to the cross product at `:503`. This is not
   bit-identical: only `mMatView` can move, by rounding, because the up vector
   is derived from a slightly different input. The visible areas come from
   the tilted footprint (`:519-535`), not from the view matrix.
   `XMMatrixLookAtRH` normalizes the basis it derives and accepts a non-unit
   up vector (verified against DirectXMath), so the cross product needs no
   explicit normalize. The new input also avoids the cancellation in the
   subtraction and no longer depends on the eye distance being nonzero.

The outputs of items 2 and 3 are client render state outside the CRC
(`MeshData` and `JointMatrix` GPU records; camera matrices), so the rounding
change in item 3 is permitted.

## Critical files

- `Engine/Source/Graphics/AnimationData.h`
- `Engine/Source/Graphics/AnimationData.cpp`
- `Engine/Source/Graphics/CameraBase.cpp`
- `Projects/ModelViewer/Source/ModelViewerApplication.cpp`

## In scope

- The declaration and definition of `AnimationData::SkinnedMaterialCount`;
  its call in `AnimationData::ReserveSkinning` and its call at
  `ModelViewerApplication.cpp:586`.
- The two `XMMatrixInverse` calls in `AnimationData::EvaluateMaterial`
  (`AnimationData.cpp:434-441`).
- The local `vecToEyeNormal` and the cross product that reads it in
  `CameraBase::CalculateMatricesAndVisibleArea` (`CameraBase.cpp:502-503`).

## Out of scope

- `AnimationData::Load` and its validation.
- Any other use of `rSceneHeader.uiMaterialCount` in ModelViewer.
- `mVecToEyeNormal`, `mVecEyePosition`, and the tilted-footprint code.
- Every other statement of `EvaluateMaterial` and
  `CalculateMatricesAndVisibleArea`.
- Any shared matrix or vector helper.

## Risk tier

Tier 2 — scoped behavior. Triggers: item 1 changes a public member function
signature that an engine caller and ModelViewer both use, which Tier 1's "no
public signature exposure" excludes (behavior unchanged); item 3 changes one
subsystem's runtime output (the view matrix, by rounding); item 2 alone is
behavior-preserving. Client render only, outside the CRC: no determinism,
serialization, wire, replay, or threading exposure.

## Acceptance criteria

- `SkinnedMaterialCount` takes no parameter and no caller passes a count.
- `EvaluateMaterial` calls `XMMatrixInverse` once.
- `CalculateMatricesAndVisibleArea` holds no normalize of
  `mVecEyePosition - mVecPosition`.
- The BrokenEngineSandbox client and ModelViewer build.
- Through `/agent-harness`, screenshots at camera tilt zero and at a nonzero
  tilt, in perspective and orthographic projection, show no visible change,
  and a scene with skinned models (or ModelViewer on a skinned model) is
  unchanged. The supplied evidence records no harness setup.

## Coordination

No directional dependencies. Whichever of this Plan and the one below lands
second re-derives its regions by symbol from the landed files.

- `Documents/Plans/Engine/SwapchainTeardownAndCameraAspect.md` edits other
  lines of `CameraBase::CalculateMatricesAndVisibleArea`.

## Notes

- `Projects/Coretide` was not searched for a `SkinnedMaterialCount` caller:
  the private game's source was not available to the authoring session. The
  executing session checks it where that source is available and reports it
  as unconfirmed otherwise.
