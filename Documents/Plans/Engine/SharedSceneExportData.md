<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-21T19:06:17.761Z","dependsOn":["Documents/Plans/Engine/AutomaticBlenderExport.md"]} -->
# Share decoded scene export data while preserving glTF output

## Context

Scene export currently passes `tinygltf::Model` through texture processing,
mesh optimization, materials, skeletons and animation serialization. Direct
exchange-format import needs those existing engine payload producers without
building an Assimp-to-glTF intermediary.

## Design

The recommended design extracts a small DataPacker-only decoded scene containing
only data the existing exporter consumes: mesh vertices/indices and node/material
association, decoded images and texture roles, engine material values, flattened
skeleton/joint maps/inverse binds, and supported animation buffers. Reuse existing
engine structs and helper outputs where they already express those values.

Move tinygltf decoding into one adapter, then run the existing optimization,
texture encoding, intermediate publication and chunk writers on this decoded
data. Refactor the currently used glTF/GLB path completely in this stage. Preserve
source order, effective material splits, image/texture indices, skeleton and clip/
channel/keyframe ordering and every serialization detail. Do not construct a
general scene graph or interfaces for a hypothetical importer.

## Critical files

`ExportScene.{h,cpp}`, `SceneVerticesLoader`, `SceneSkeletonLoader`,
`SceneAnimationLoader`, new `Scene/SceneExportData.h` and
`Scene/GltfSceneDecoder.{h,cpp}`, DataPacker project files and scene documentation.

## In scope

- New `Scene/SceneExportData.h`: minimal owned decoded values passed to the shared
  pipeline; no runtime types, virtual importer API, or persistent new format.
- New `Scene/GltfSceneDecoder.{h,cpp}`: tinygltf loading and existing supported
  glTF material/texture/node/mesh/skeleton/animation decoding into those values.
- `SceneVerticesLoader.{h,cpp}`: `LoadVertices`, attribute/indices decoding,
  node transforms and effective-material mapping, texture-role helpers; adapt
  only to the decoding/shared-data boundary while retaining existing calculations.
- `SceneSkeletonLoader.{h,cpp}`: `BuildNodeParentMap`, `CanonicalizeSceneSkin`,
  `LoadSkeletonData`; preserve canonical skin/node ordering and rejection rules.
- `SceneAnimationLoader.{h,cpp}`: `DetermineAnimationPath`, `LoadAnimations` and
  local decoding/filter diagnostics; preserve supported interpolation and output.
- `ExportScene.{h,cpp}`: `Export`, loading, `PreExport`, `ProcessTextures`,
  `SetupSkeletonAndMaterials`, `LoadVerticesAndOptimizeMeshes`, `BuildMaterialInfos`,
  `MainExport`, `FillMaterialShaderDatas`, `WriteAnimationSection`, and necessary
  signatures/helpers: consume decoded values while retaining optimization and
  exact writes; keep dirty checks, failure cleanup and publication ownership.
- DataPacker `.vcxproj`/`.filters`: add new decoder/data files only.
- `DataPacker/Source/AGENTS.md` and `DataPacker/Source/ExportJobs/AGENTS.md`, Scene
  and Model: document only the new decoding/shared export boundary.

## Out of scope

New formats/importers, Blender behavior changes, speculative importer interfaces,
runtime renderer or serialization changes, material approximation, new animation
support, unrelated naming/style cleanup, and unit tests.

## Risk and invariants

Tier 3: touches payload-producing mesh, material and animation integration and
must prove exact bytes/CRCs unchanged. The change is a refactor, not permission
to normalize ordering, transform math, defaults, or rounding. Runtime formats and
versions remain unchanged; update only owning cache schema versions if actually
changed. Existing source/auxiliary dependency fingerprints remain valid.

## Verification

Before edits, export and retain exact scene/model/texture chunks and path/content
CRCs for the existing glTF corpus and representative binary GLB. Include static,
textured, shared-material mesh nodes, skinned and node-animation examples covering
currently supported interpolation. Retain input identities and tool configuration.

| Acceptance | Evidence |
|---|---|
| Real path uses shared data | Both tinygltf entry formats decode once into consumed scene export data; no unused second pipeline |
| Export is byte preserving | Exact before/after scene/model/texture chunk comparison and equal path/content CRCs for every baseline asset |
| Limits and mappings are unchanged | Read material splits, canonical skin, joint mapping and animation filter/interpolation paths against baseline |
| Caches remain correct | Warm run clean; referenced source/image/buffer change dirties affected scene; cleanup/publication still owned |
| Builds and rendered behavior hold | `/compile` DataPacker Release x64 and required client/server harness builds; `/agent-harness` axes, size, bind and timed-pose checks |

Follow normal change workflow review; any byte/CRC difference is an acceptance
failure to explain and resolve within this refactor, not an approved format change.
