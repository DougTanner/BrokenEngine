<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:54:16.557Z","dependsOn":[]} -->
# Reuse the Mesh-World Inverse and the Stored Eye Normal

## Context

Found by a deep analysis of `Engine/Source/Graphics` (direct files only) and
verified independently. Pre-existing at commit `401e4b75`; line numbers are
from that commit, so locate by symbol if they moved. `G/` abbreviates
`Engine/Source/Graphics/`.

Two render functions compute a value they already hold.

1. `AnimationData::EvaluateMaterial` inverts the same matrix twice.
   `G/AnimationData.cpp:434` computes
   `XMMatrixInverse(nullptr, matMeshWorld)` for the normal matrix, and `:441`
   computes the same inverse of the same `matMeshWorld`, not written between
   them (`:431-440`), into `matMeshWorldInverse` for every skinned material.
   It runs per material per instance per rendered frame; ModelViewer reaches
   it too (`Projects/ModelViewer/Source/ModelViewerApplication.cpp:590`).
2. `CameraBase::CalculateMatricesAndVisibleArea` re-derives a unit vector it
   just stored. `G/CameraBase.cpp:498` builds the unit vector
   `mVecToEyeNormal`, `:499` sets `mVecEyePosition` to the position plus that
   vector scaled by the eye distance, and `:502` recovers the direction by
   subtracting and normalizing into the local `vecToEyeNormal`, which only the
   cross product at `:503` uses. The eye distance is positive, so the local
   equals the member up to rounding.

Unmet criteria: the inverse is computed once per call; the stored unit vector
is used as stored. Authority: root `AGENTS.md` `## Directives` (DRY);
`.agents/references/cpp-conventions.md` "Unit vectors stay unit".

## Design

Recommended:

1. In `EvaluateMaterial`, compute the inverse once into `matMeshWorldInverse`
   before the normal matrix, use it for the normal matrix and in the skinned
   block, and delete the second call. The same function runs on the same
   input, so the outputs are bit-identical.
2. In `CalculateMatricesAndVisibleArea`, delete the local at `:502` and pass
   `mVecToEyeNormal` to the cross product at `:503`. This is not
   bit-identical: only `mMatView` can move, by rounding, because the up vector
   is derived from a slightly different input. The visible areas come from
   the tilted footprint (`:519-535`), not from the view matrix.
   `XMMatrixLookAtRH` normalizes the basis it derives and accepts a non-unit
   up vector (verified against DirectXMath), so the cross product needs no
   explicit normalize. The new input also avoids the cancellation in the
   subtraction and no longer depends on the eye distance being nonzero.

Both outputs are client render state outside the CRC (`MeshData` and
`JointMatrix` GPU records; camera matrices), so the rounding change in item 2
is permitted.

## Critical files

- `Engine/Source/Graphics/AnimationData.cpp`
- `Engine/Source/Graphics/CameraBase.cpp`

## In scope

- The two `XMMatrixInverse` calls in `AnimationData::EvaluateMaterial`
  (`AnimationData.cpp:434-441`).
- The local `vecToEyeNormal` and the cross product that reads it in
  `CameraBase::CalculateMatricesAndVisibleArea` (`CameraBase.cpp:502-503`).

## Out of scope

- `mVecToEyeNormal`, `mVecEyePosition`, and the tilted-footprint code.
- Every other statement of both functions.
- Any shared matrix or vector helper.

## Risk tier

Tier 2 — scoped behavior. Trigger: item 2 changes one subsystem's runtime
output (the view matrix, by rounding); item 1 alone is behavior-preserving.
Client render only, outside the CRC: no determinism, serialization, wire,
replay, or threading exposure.

## Acceptance criteria

- `EvaluateMaterial` calls `XMMatrixInverse` once.
- `CalculateMatricesAndVisibleArea` holds no normalize of
  `mVecEyePosition - mVecPosition`.
- The BrokenEngineSandbox client and ModelViewer build.
- Through `/agent-harness`, screenshots at camera tilt zero and at a nonzero
  tilt, in perspective and orthographic projection, show no visible change,
  and a scene with skinned models (or ModelViewer on a skinned model) is
  unchanged. The supplied evidence records no harness setup.

## Coordination

No Plan below depends on this one, and it depends on none. Whichever lands
second re-derives its regions by symbol from the landed files.

- `Documents/Plans/Engine/CameraAspectRatioFromFramebufferExtent.md` edits
  other lines of `CameraBase::CalculateMatricesAndVisibleArea`.

## Notes

- `Documents/Plans/Engine/SkinnedMaterialCountReadsMember.md` edits other
  functions of `AnimationData.cpp`.
