<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-01T20:24:10.457Z","dependsOn":[]} -->
# DataPacker skips fully transparent meshes on the Assimp scene path

## Context

DataPacker's glTF scene path skips every primitive whose material is fully
transparent (`alphaMode` `BLEND` with base color factor alpha 0) in
`DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp` `LoadVertices`,
before `ResolveEffectiveMaterial`, so such a primitive sizes neither the
normalization bounds, the `.MODEL` vertices, nor the baked model outline
(hitbox). That change, which also retuned the Aquamarine Battleship alignment,
was limited to glTF by user decision, and the Assimp path was deferred to this
Plan.

Gap: the direct Assimp decode (`.dae`, `.ply`, `.stl`, `.3ds`;
`DataPacker/Source/ExportJobs/ExportScene.cpp` `IsAssimpScene`) has no such
skip, so a fully transparent Assimp mesh still sizes the model and its hitbox.

- `DataPacker/Source/ExportJobs/Scene/AssimpSceneDecoder.cpp`
  `LoadAssimpMaterial` (:196-202) already computes the fully transparent case:
  base color alpha is multiplied by `AI_MATKEY_OPACITY`, and any result below 1
  sets `fAlphaMask = 2` (blend). An effective alpha of exactly 0 is therefore
  always a blend material with nothing drawn; no `BLEND` test is needed
  separately.
- `LoadAssimpNode` (:526-643) appends every mesh of every node: per mesh it
  validates (:528-546), resolves the effective material (:547-548), appends
  vertices (:551-581), registers bone joints (:582-622), and appends indices
  (:632-642). An Assimp mesh has exactly one material (`mMaterialIndex`), so a
  per-mesh skip is the per-primitive skip of the glTF path.
- Bounds come from `AccumulateDrawnBounds` over every material's indices
  (`AssimpSceneDecoder.cpp:739`); the outline is baked from every `.MODEL`
  vertex by `DataPacker/Source/ExportJobs/ExportModel.cpp`.
- A material left without indices is already a supported state: materials are
  sized to the source material count up front (`AssimpSceneDecoder.cpp:699-700`),
  every material slot carries its source `iOriginalMaterialIndex`, its own
  index when not split (:792), and the glTF skip relies on the same
  zero-index material slot.
- No tracked scene uses the Assimp path today (`git ls-files` has no `.dae`,
  `.ply`, `.stl`, or `.3ds` file), so no tracked `.MODEL` or texture
  intermediate changes.

Precondition: the glTF skip described above is present in `LoadVertices`. If it
is absent, stop and report rather than implementing the Assimp half alone.

## Design

In `LoadAssimpNode`'s mesh loop, after the existing per-mesh validation
(the texture-set `ASSERT` loop ending at :546) and before `bSkinned` and
`ResolveEffectiveMaterial` (:547-548), `continue` past a mesh whose source
material is fully transparent:
`rShaderData.f4BaseColorFactor.w == 0.0f` (`rShaderData` is already bound at
:542). Add a one-line comment matching the glTF skip's rationale (fully
transparent: draws nothing, so it must not size the model or its hitbox). No
helper, log line, or option, mirroring the glTF skip.

Skinned meshes (the author's recommendation, with rationale): skip a fully
transparent mesh whether or not it is skinned, as the glTF skip does.

- Assimp registers skin joints per mesh from that mesh's bones
  (`rNodeJoints`, :589-597). A skipped mesh's bones are never registered, so
  joint indices stay contiguous over the drawn meshes and no exported vertex
  references a missing joint. The per-mesh weight checks (:610-628) and the
  shared-bone inverse-bind check (:600-605) then cover drawn meshes only,
  which is all the packed skin needs.
- `bHasBones` (:674) still counts every mesh, so a scene whose only skinned
  meshes are skipped still exports with a skeleton, now with zero skin joints.
  That is the state an animated scene without bones already produces
  (`bHasSkeleton = bHasBones || !animatedNames.empty()`, :690). The
  recommendation is to leave `bHasBones` as is.
- Skipping only rigid meshes would leave a fully transparent skinned mesh
  sizing the bounds, and the two scene paths would then disagree.

A scene in which every mesh is skipped fails at the existing
`ASSERT(std::ranges::any_of(...))` (:735), matching the glTF rule that an
export with no drawn vertex fails.

Version: bump the raw scene version in
`DataPacker/Source/ExportJobs/ExportScene.h` `kiVersion` by one, as
`DataPacker/Source/ExportJobs/AGENTS.md` `## Shared Pipeline` requires for a
semantic change. The Assimp path keys its `.PreExport` marker and cached chunks
on the same version (`ExportScene.cpp:371`, :465-478). The bump re-runs
pre-export for every scene and rewrites every tracked `.PreExport`
marker. No other output may change.

Risk tier: Tier 2. Trigger: one subsystem's tool behavior (DataPacker scene
export). The version bump changes cache and `.PreExport` identity only, and no
serialized layout, reader, wire, CRC, or runtime code changes. Live
verification exposure: none at runtime, because no tracked asset uses the path.

## Critical files

- `DataPacker/Source/ExportJobs/Scene/AssimpSceneDecoder.cpp`
- `DataPacker/Source/ExportJobs/ExportScene.h`
- `DataPacker/Source/ExportJobs/AGENTS.md`

## In scope

- `AssimpSceneDecoder.cpp` `LoadAssimpNode` mesh loop: the one `continue`
  described in `## Design`, with its comment.
- `ExportScene.h` `kiVersion`: raw scene version +1.
- `DataPacker/Source/ExportJobs/AGENTS.md` `## Scene and Model`: extend the
  sentence that records the glTF fully transparent primitive skip so it also
  covers an Assimp mesh whose base color alpha times opacity is 0 (wording via
  `/update-claude-docs`).
- The regenerated tracked `.PreExport` markers the version bump rewrites, from
  a Local generation build.

## Out of scope

- The glTF path (`SceneVerticesLoader.cpp`, `GltfSceneDecoder.cpp`).
- `LoadAssimpMaterial`, `bHasBones`/`bHasSkeleton` computation, joint
  registration, and every other part of `AssimpSceneDecoder.cpp`.
- Treating a texture-alpha-only, partially transparent, or `MASK` material as
  invisible.
- Removing or renumbering an emptied material, its shader data, or its
  textures; texture export.
- Blender-converted sources (`.blend`, `.fbx`, `.obj`), which export through
  the glTF path.
- Adding a test asset; `ExportModel`, runtime, and hitbox code.
- Any byte change to a tracked `.MODEL` or texture intermediate: that is a
  defect to investigate, not to commit.

## Acceptance criteria

1. Code read: the skip sits in the mesh loop after validation and before
   `ResolveEffectiveMaterial`, so a skipped mesh adds no vertices, indices,
   bone joints, or material split, whatever node it sits under and whether or
   not it is skinned; a mesh whose effective alpha is above 0 still exports.
2. `/compile` DataPacker Release|x64 builds clean, and the Local generation
   DataPacker run reports no failed scene or model job.
3. After that run, `git status` against the session baseline shows only the
   source, `AGENTS.md`, and `.PreExport` marker changes; no `.MODEL` or texture
   intermediate changes.

## Notes

- No live `/agent-harness` setup applies: no tracked asset reaches the Assimp
  path, so criterion 1 is settled by code read.
