<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T23:17:37.745Z","dependsOn":[]} -->
# Remove the creation-time identity fill of the skinning buffers

## Context

`M/` is `Engine/Source/Graphics/Managers/` and `BES/` is
`Projects/BrokenEngineSandbox/Source/Frame/Collections/`. Lines are those at
commit `401e4b75`.

`M/BufferManager.cpp:359-375` and `:389-407` fill the mesh-data and
joint-matrix skinning buffers with identity values when they are created.
The grow paths do not: they copy only the valid prefix, and their comments
(`:539`, `:566`) say slots past it need no value. The two paths contradict
each other, and the grow side is the right one. No path reads a slot that
was not written in the same frame:

- The only shader that reads either buffer is
  `Engine/Data/Shaders/Model/ModelSkinned.vert` (`:12-13`, `:24-27`): it
  reads `meshData[uiMeshDataBase + materialIndex]`, and
  `jointMatrices[offset + joint]` only when that entry's `jointCount > 0`.
  `ModelStatic.vert` reads neither.
- `M/DynamicPipelines.cpp:41` selects the skinned vertex shader exactly when
  `sceneHeader.bHasAnimation` is set, and
  `LoadAnimationDataFromEagerChunks`
  (`Engine/Source/Graphics/AnimationData.cpp:470-477`) puts exactly those
  scenes in `gAnimationDataMap`.
- Every renderer of a scene in that map reserves and writes its slots the
  same frame: `BES/Battleships/BattleshipsRender.cpp:154-156,206-209`,
  `BES/Fighters/FightersRender.cpp:120-122,153-156,201-205`,
  `BES/Omnis/OmnisRender.cpp:170-173,222-226`, and
  `Projects/ModelViewer/Source/ModelViewerApplication.cpp:834-853`.
  `EvaluateMaterial` (`AnimationData.cpp:416-458`) writes the whole
  `MeshData` entry and, for a skinned material, every joint.
- The CPU only writes these buffers; the grow `memcpy` is the only CPU read
  and covers the valid prefix.

One reference is unreserved: `BES/Missiles/MissilesRender.cpp:136` writes
`uiMeshDataBase = 0` with no reservation. It is safe only because the missile
scene is static. That fact rests on the source asset, not on the built pack:
`Projects/BrokenEngineSandbox/Data/Models/aim-9_missile/scene.gltf` has no
`"animations"` block, and the packer sets `bHasAnimation` only when the clip
list is non-empty
(`DataPacker/Source/ExportJobs/ExportScene.cpp:928-934`), so the missile
draws with `ModelStatic.vert`.

Originating gap: the in-function phase of an analysis of
`Engine/Source/Graphics/Managers` at that commit. Unmet criterion: both
skinning buffers are created without a data function, so creation and grow
give the same guarantee.

## Design

Recommended: delete the two creation-time fills, about 40 boot-only lines.
The grow paths are right and stay as they are.

Consequence: slots outside the range written in a frame hold indeterminate
mapped memory instead of identity. Nothing reads them today. A collection
that draws an animated scene must reserve through `ReserveSkinning`; Missiles
does not, and is static today.

## Critical files

- `Engine/Source/Graphics/Managers/BufferManager.cpp`

## In scope

- The two creation-time identity fills in the skinning buffer creation
  (`:359-375`, `:389-407`).

## Out of scope

- The two bump allocators and both grow paths (`:487-571`).
- `MissilesRender.cpp` and every other renderer.
- `ModelSkinned.vert`.

## Risk tier

Tier 2 — scoped behavior. Trigger: the initial contents of two GPU buffers
change, which is runtime behavior of one subsystem. The buffers are client
render state outside the simulation CRC; no determinism, layout, wire, or
threading exposure.

## Acceptance criteria

1. Neither skinning buffer is created with a data function.
2. The BrokenEngineSandbox client and ModelViewer build through `/compile`.
3. Through `/agent-harness`, with validation layers on, BrokenEngineSandbox
   and ModelViewer each render skinned and static models correctly,
   including a frame that forces a skinning buffer grow. Neither grow
   function logs, so the executing session confirms the grow with a
   temporary `kTemp` log in `GrowMeshDataBuffer` and `GrowJointMatrixBuffer`,
   each inside a `// TEMP START` / `// TEMP END` block removed once that run
   is recorded; the dynamic-buffer grow warning in
   `ResizeDynamicBufferIfNeeded` is a different buffer and does not count.
   This run is the decisive check: the evidence above is from source
   reading, and no runtime check was run. The supplied evidence records no
   harness setup.
4. The missile, drawn in the same run, renders as before.

## Coordination

`Documents/Plans/Engine/BufferManagerDeadStateAndDuplicates.md` merges the
two callers of `InitializePerCommandBufferBuffers`, the function this Plan
edits. Neither Plan depends on the other; whichever lands later re-derives
its lines by symbol.
