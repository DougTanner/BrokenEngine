<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-29T18:17:51.926Z","dependsOn":[]} -->
# Bake scene vertices by whether the skinning path is actually taken

## Context

`BuildVertices` decides whether to bake a primitive's vertices into world space
from two flags:

```cpp
if (!bHasSkinning && !bHasSkeleton)
{
	// Only transform static models without skeleton data
	// Skinned vertices must remain in local space for runtime skinning pipeline
	vecPosition = XMVector4Transform(vecPosition, rMatMeshWorld);
	vecNormal = XMVector3TransformNormal(vecNormal, rMatMeshWorld);
}
```

(`DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp:139-145`)

`bHasSkinning` is per primitive and means only "this primitive declares a
`JOINTS_0` attribute" (`SceneVerticesLoader.cpp:330`), while `bHasSkeleton`
means "the model has a skin or a surviving animation clip"
(`DataPacker/Source/ExportJobs/ExportScene.cpp:547`). A model can declare
`JOINTS_0` on a primitive while having neither: glTF allows the attribute
without the node referencing a skin, and the export-time guard that rejects a
skin with no surviving clip (`ExportScene.cpp:335-338`) does not fire when
`rGltfModel.skins` is empty.

In that case the primitive is left in mesh-local space while its sibling
primitives under the same node, and every other static primitive in the model,
are baked to world space — so within one model two different space conventions
ship in one vertex buffer.

Nothing at runtime restores the missing node transform. The animation section
that carries node matrices and `MaterialInfo` is written only when the model has
animations (`ExportScene.cpp:817-821`), so no `AnimationData` exists for such a
model and each mesh keeps the initial identity fill of the `MeshData` storage
buffer — identity matrix, `uiJointCount = 0`
(`Engine/Source/Graphics/Managers/BufferManager.cpp:372-383`). The unbaked
primitive therefore renders at the model origin instead of at its node's world
position, silently and with no diagnostic.

Not reachable with the current repository assets: the four tracked `.gltf` files
(`Engine/Data/Models/DualGeodesicIcosahedron/DualGeodesicIcosahedron.gltf`,
`Projects/BrokenEngineSandbox/Data/Models/aim-9_missile/scene.gltf`,
`Projects/BrokenEngineSandbox/Data/Models/Spaceship/scene.gltf`,
`Projects/BrokenEngineSandbox/Data/Models/spaceship2/scene.gltf`) declare no
`JOINTS_0`, no `skins`, and no `animations`, and there are no `.glb` files. The
defect is pre-existing at session baseline
`a6fea9665d2d6f2b01ce4caaa4054cea5671ab3b`.

Correction to the routing note that produced this Plan: the effect is not
`iParentNodeIndex = -1`. `LoadVertices` derives `bHasSkinning` from the presence
of `JOINTS_0` (`SceneVerticesLoader.cpp:330`) and passes it to
`ResolveEffectiveMaterial` (`:332`), which stores it on the material's
`MaterialNodeInfo` (`:84` for the unclaimed original material, `:96` for a split
one), so `BuildMaterialInfos` takes its skinned branch and does set
`iParentNodeIndex = iNodeIndex` (`ExportScene.cpp:622-628`). The identity mesh
world comes from the animation section never being written at all, as cited
above; the observable symptom is the same.

## Design

Make the bake condition ask the question the runtime actually answers — whether
this primitive's vertices will be posed by the skinning or node-matrix path —
instead of asking whether a `JOINTS_0` attribute exists. When the model has no
skeleton (`bHasSkeleton == false`), no skinning or node matrices will ever be
applied, so every primitive in that model, `JOINTS_0` or not, is baked to world
space exactly like the static primitives beside it. That is a one-condition
change at `SceneVerticesLoader.cpp:139`, and it leaves every model that does
have a skeleton byte-identical.

The asset is exported, never rejected: the input is representable and the
correct output is well defined — an unused `JOINTS_0` attribute on an otherwise
static model is a common exporter artifact — so refusing to export it would cost
an asset the exporter can handle correctly. The existing skin-without-clip
rejection (`ExportScene.cpp:335-338`) stays untouched and keeps covering the
genuinely unsupported input it already names.

The dummy joint data written for non-`JOINTS_0` vertices
(`SceneVerticesLoader.cpp:174-181`) and the joint data copied for `JOINTS_0`
vertices (`:157-173`) stay as they are: with no skeleton the runtime sets
`uiJointCount = 0` and never reads them.

The change alters exported vertex bytes for any model that hits this case, so
bump the version that owns the scene payload
(`DataPacker/Source/ExportJobs/ExportScene.h:24-26`) so cached chunks re-export
instead of shipping stale positions.

## Critical files

- `DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp:108-145` —
  `BuildVertices` and its bake condition; `:330,336` — where `bHasSkinning` and
  `bHasSkeleton` are supplied.
- `DataPacker/Source/ExportJobs/ExportScene.cpp:540-560` — `bHasSkeleton`
  derivation; `:335-338` — the existing skin-without-clip rejection; `:817-821`
  — the condition under which the animation section is written at all.
- `DataPacker/Source/ExportJobs/ExportScene.h:24-26` — the scene payload version
  to bump.
- `Engine/Source/Graphics/Managers/BufferManager.cpp:372-383` — the identity
  `MeshData` fill a no-animation model renders with.

## In scope

- The bake decision in `BuildVertices` (`SceneVerticesLoader.cpp:139`) and the
  flags `LoadVertices` passes it (`:336`), so a primitive is left in mesh-local
  space only when the model's skinning or node-matrix path will actually pose
  it.
- The matching version bump in `ExportScene.h:24-26`.
- The comments at `SceneVerticesLoader.cpp:105-107,141-146`, which state the
  current rule and must state the corrected one.

## Out of scope

- Changing `MaterialInfo`, the scene/model chunk layout, or any serialized
  field's meaning.
- Effective-material identity and splitting by skinning state, owned by
  `Documents/Plans/Engine/MixedSkinningMaterialSplit.md`.
- Multi-skin identity selection
  (`Documents/Plans/Engine/SceneSkinIdentityValidation.md`) and the joint-count
  limit (`Documents/Plans/Engine/OversizedSkinJointIndexMismatch.md`).
- Runtime skinning, shader math, `AnimationData` evaluation, and the animation
  section's own contents.
- Rejecting or warning about a `JOINTS_0`-without-skeleton model at the export
  trust boundary, and any change to the existing skin-without-clip rejection.
- Adding a test asset to the tracked repository.

## Risk tier and invariants

Expected Change Workflow Tier 2. Trigger: scoped behavior of one DataPacker unit
at the existing opaque-glTF trust boundary, with no serialized format, chunk
layout, wire, replay, or runtime-consumer change. Escalate to Tier 3 if
implementation changes a serialized field's meaning or reaches the runtime
evaluator.

Preserve these invariants:

- Every vertex a model publishes is in the space the runtime will interpret it
  in: mesh-local only when node or joint matrices will be applied to it, world
  space otherwise.
- A model with a skeleton (skin or surviving animation clip) exports
  byte-identically to today, as do the four current repository assets.
- The two space conventions never coexist inside one model that has no skeleton.

## Acceptance criteria

- A glTF whose primitive declares `JOINTS_0` while the model has no skin and no
  surviving animation clip exports successfully, with that primitive's vertices
  baked to world space by its node's transform exactly like the static
  primitives beside it. Export does not fail and no primitive ships in
  mesh-local space for such a model.
- The four tracked `.gltf` assets export byte-identically to the current output
  (compare the published scene packs before and after).
- DataPacker builds through `/compile`, and a run over the current assets
  reports no new warning or failure.

## Notes

Origin: preparation residual from the session claiming
`Documents/Plans/Engine/MixedSkinningMaterialSplit.md`, session baseline
`a6fea9665d2d6f2b01ce4caaa4054cea5671ab3b`. Related but independent: that Plan
fixes material identity for a node mixing skinned and non-skinned primitives
while a skeleton exists; this Plan fixes the vertex space chosen when no
skeleton exists at all. Neither depends on the other, and no source fix, build,
or runtime scenario was performed while routing this item.
