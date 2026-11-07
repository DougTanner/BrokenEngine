<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-07T01:19:52.097Z","dependsOn":[]} -->
# Bake static glTF normals with the inverse transpose

## Context

A static glTF export must keep each normal perpendicular to its surface. Under
non-uniform node scale or shear it does not. `BuildVertices` bakes the normal with
`XMVector3TransformNormal(vecNormal, rMatMeshWorld)`
(`DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp:162`), and that call
applies the matrix itself, as the comment at `:175` says of the tangent, for which
it is the right transform (`:176`). For a normal the right matrix is the inverse
transpose, so today a static primitive under such a node packs a skewed normal
that also stops being perpendicular to its packed tangent.

The Assimp path already does this: `AssimpSceneDecoder.cpp:525` builds `matNormal`
as `XMMatrixTranspose(XMMatrixInverse(nullptr, matWorld))` and applies it at
`:563`, after requiring a finite nonzero determinant (`:524`). The defect predates
the tangent bake beside it and is outside the boundary of that change.

No tracked model shows it. Across the default scenes of all 13 tracked glTF and
GLB files, the only mesh node with a non-conformal world matrix is
`AO_shadow_catcher` in `SpaceBattleshipAquamarine/scene.gltf`, under a parent with
scale 0.01, 0.01, 0.00808779. Its `lambert1` material is `BLEND` with base color
alpha 0, so `LoadVertices` skips it. A skeleton export keeps vertices mesh-local
and is not reached. The defect appears only for a future static model with
non-uniform node scale.

## Design

The author's recommendation is to compute the normal matrix once before the vertex
loop in `BuildVertices`, mirroring the Assimp form, and use it at `:162` in place
of `rMatMeshWorld`. The tangent keeps the forward transform. `:167` still
normalizes, so a uniform scale packs the same direction.

The inverse does not exist for a singular matrix, which the forward transform
tolerated, so a static primitive under a singular node matrix would pack
non-finite normals. The author's recommendation is to reject it with a
`std::runtime_error` naming the primitive, the failure path of the neighboring
checks and the case the Assimp path already refuses. Rationale: such a node
flattens the primitive, and a normal for a flattened surface has no defined value.

Bump the raw value of `ExportScene::kiVersion` for the semantic change, as
`DataPacker/Source/ExportJobs/AGENTS.md` `## Shared Pipeline` requires. That
re-runs pre-export for every scene once.

## Critical files

- `DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp` — `BuildVertices`.
- `DataPacker/Source/ExportJobs/ExportScene.h` — `kiVersion`.
- `DataPacker/Source/ExportJobs/Scene/AssimpSceneDecoder.cpp` — read for the form.

## In scope

- The static normal bake in `BuildVertices` and the singular-matrix rejection.
- The raw `ExportScene::kiVersion` bump and the tracked `.PreExport` and `.MODEL`
  outputs it regenerates.
- The owner-document sentence in `DataPacker/Source/ExportJobs/AGENTS.md`
  `## Scene and Model`, if the change makes it wrong.

## Out of scope

No change to the tangent transform, `FindAttribute`, attribute validation, winding
under a reflecting node, skeleton export, the Assimp path, `NodeLocalMatrix`,
`ModelVertex` or any shader. No new tracked fixture model, harness command or unit
test.

## Acceptance criteria

- After `/compile` of DataPacker Release|x64 and the ordinary export, every tracked
  `.MODEL` keeps each material's index count. Its vertex count, vertex order and
  triangle order may change, because a uniform-scale normal can change in its last
  bits and `meshopt_generateVertexRemap` dedups by exact bytes. So expand each
  material's indices into triangle corners and pair them one-to-one with the
  pre-change corners: every `ModelVertex` field except `f3Normal` matches exactly,
  and `f3Normal` matches within the tolerance the next criterion uses.
- A scratch, uncommitted copy of `aim-9_missile` with a node scale of 1, 1, 0.5
  packs each normal equal to the normalized inverse-transpose transform of its
  authored normal to within 0.001, and perpendicular to its packed tangent to
  within 0.001.
- A scratch copy with a zero scale component fails that scene's export through the
  aggregate with the new message, and other scenes still export.

This needs no live harness run, so no setup applies.

## Notes

Future implementation is Tier 2: trigger is one subsystem's tool behavior, static
glTF normal values in DataPacker, with no format or layout change.

The version bump changes no packed format. `ModelVertex` and `.pack` layout stay
as they are. Vertex data stays outside PostRender state, because packed normals
reach only client shading, so determinism/CRC, replay, wire, threading and
allocation are unaffected.

No prerequisite Plan or mandatory coordination constraint was identified. Other
live Plans edit other regions of this file, so refresh line numbers before use.
