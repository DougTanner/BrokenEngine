<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-07T01:19:53.515Z","dependsOn":[]} -->
# Validate glTF vertex attribute accessors

## Context

DataPacker treats source files as trust-boundary inputs
(`DataPacker/Source/AGENTS.md` `## Architecture`), but two kinds of glTF attribute
value reach `common::ModelVertex` unchecked, both in
`DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp`.

`FindAttribute` (`:10`, accessor fetched at `:19`) never checks `rAccessor.type` or
`rAccessor.count`. It bounds the read in bytes from the POSITION vertex count and
the caller's component count, which keeps it inside the buffer view but not inside
the accessor. A NORMAL accessor declared `VEC2`, or one holding fewer elements than
POSITION, inside a long enough view is therefore read as `VEC3` for every vertex,
packing neighboring bytes as vertex data with no error.

`TEXCOORD_0` to `TEXCOORD_4` (`:178` to `:182`) and `WEIGHTS_0` (`:206`, first read
at `:191`) are packed with no finite check. `RequireFiniteComponents` (`:112`)
already guards NORMAL (`:164`) and TANGENT (`:175`), and its comment gives the
reason: a non-finite component reaches the shaders as NaN. POSITION is covered
separately by the bounds checks.

These gaps predate the `TANGENT` packing beside them and are outside the boundary
of that change. No acceptance criterion failed.

No tracked model hits either. Across all 13 tracked glTF and GLB files, no
attribute has a wrong type or a count differing from POSITION, and none of the
1620080 `TEXCOORD` floats or 34360 `WEIGHTS_0` floats is non-finite.

One Assimp item belongs with these, and it is the only one in this Plan.
`LoadAssimpNode` in `DataPacker/Source/ExportJobs/Scene/AssimpSceneDecoder.cpp`
validates a normal before normalizing it: the `ASSERT` at `:565` reads `vecNormal`,
and `:567` packs `XMVector3Normalize(vecNormal)`. A finite normal whose squared
length overflows passes, because infinity is above zero, and under
`_XM_SSE4_INTRINSICS_`, which `Common/ExternalHeaders.h` defines,
`XMVector3Normalize` returns QNaN for an infinite squared length, so QNaN is packed.
The glTF path has no such gap, because `RequireFiniteComponents` checks the stored
value. This is inferred from reading and not reproduced. Like the gaps above, it is
outside the `TANGENT` change, and no tracked source reaches it: no file with one of
the `.dae`, `.ply`, `.stl` or `.3ds` extensions
`DataPacker/Source/ExportJobs/ExportScene.cpp` routes to Assimp is tracked.

## Design

The author's recommendation is two additions in `SceneVerticesLoader.cpp`, reusing
what that file already has, and one Assimp correction.

In `FindAttribute`, after the accessor is fetched, throw a `std::runtime_error`
naming `pcAttributeName` when
`tinygltf::GetNumComponentsInType(rAccessor.type)` differs from `iComponentCount`,
or `rAccessor.count` differs from `uiVertexCount`. That is the file's failure path,
which fails one scene through the export aggregate.

In `BuildVertices`, call `RequireFiniteComponents` on each present `TEXCOORD` set
before it is packed, and on `WEIGHTS_0` inside the `puiJoints` branch before the
zero-weight test at `:191`, because that test is the first read and weights are
packed only there.

In `LoadAssimpNode`, apply the `ASSERT`'s `IsFiniteVector` normal term to
`XMVector3Normalize(vecNormal)` instead of `vecNormal`, and keep its position term
and nonzero squared-length term. A NaN or infinite source component also normalizes
to NaN, so that one term covers every non-finite case. `ASSERT` throws in every
configuration, which fails one scene through the export aggregate.

Valid files export the same bytes, so no version bump is needed.

## Critical files

- `DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp` — `FindAttribute`,
  `BuildVertices`.
- `DataPacker/Source/ExportJobs/Scene/AssimpSceneDecoder.cpp` — `LoadAssimpNode`.

## In scope

- The accessor `type` and `count` checks in `FindAttribute`.
- `RequireFiniteComponents` calls for `TEXCOORD_0` to `TEXCOORD_4` and `WEIGHTS_0`
  in `BuildVertices`.
- The vertex normal `ASSERT` in `LoadAssimpNode`.

## Out of scope

No change to the `componentType` `ASSERT`s, normalized or integer `TEXCOORD` and
`JOINTS_0` support, sparse accessors, POSITION handling, `AppendIndices`, the
normal bake, any other part of the Assimp path, `ExportScene::kiVersion` or
`FindAttribute`'s signature. No new tracked fixture model, harness command or unit
test.

## Acceptance criteria

- After `/compile` of DataPacker Release|x64 and the ordinary export, no tracked
  `.MODEL`, `.PreExport` or texture intermediate changes.
- Scratch, uncommitted copies of tracked scenes fail that scene's export through
  the aggregate with a message naming the attribute, and other scenes still export,
  for each of: a NORMAL accessor with `type` `VEC2`; a TEXCOORD_0 accessor with
  `count` one below POSITION's; a NaN in a `TEXCOORD_0` value; and a NaN in a
  `WEIGHTS_0` value, in a copy of `SeaCreature2`, the one tracked scene with
  weights.
- A scratch, uncommitted ASCII `.ply` scene whose vertices carry unit normals
  exports. The same scene with a `1e30` normal component fails that scene's export
  through the aggregate on the `LoadAssimpNode` `ASSERT`, and other scenes still
  export.

This needs no live harness run, so no setup applies.

## Notes

Future implementation is Tier 2: trigger is tightened checks inside two DataPacker
scene decoding units at their existing boundaries, with the format and the trust
unchanged.

Only malformed sources behave differently, and they fail at export time. Packed
bytes, `.pack` layout, determinism/CRC, replay, wire, threading and allocation are
unaffected.

No prerequisite Plan or mandatory coordination constraint was identified. Other
live Plans edit other regions of both files, including `FindAttribute`'s signature
and the `LoadAssimpNode` mesh loop, so refresh line numbers before use.
