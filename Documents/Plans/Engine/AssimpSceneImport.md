<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-21T19:06:22.003Z","dependsOn":["Documents/Plans/Engine/SharedSceneExportData.md"]} -->
# Import exchange scenes directly into shared engine export data

## Context

The scene pipeline has decoded DataPacker scene export data and a shared payload
writer. Extend scene support to `.fbx`, `.3ds`, `.obj`, `.dae`, `.ply`, and `.stl`
with the pinned Assimp dependency. Existing `.gltf` and `.glb` remain on tinygltf.
Support static geometry and skeletal animation representable by existing engine
limits; native `.max` stays deferred.

## Design

The recommended design is one DataPacker-only Assimp decoder feeding the used
shared export data, without a glTF intermediate. Enable triangulation and missing
normal generation. Supply default material data and unused UV values for
untextured geometry. Map supported material factors/texture slots, embedded or
external images, node transforms, bones/inverse binds and supported animation
channels into existing engine semantics. Use existing image decoding/encoding.

Normalize import results to the established glTF model basis and meters. Apply
importer axis/unit corrections exactly once, accounting for corrections Assimp
already applied; metadata-free inputs use Y-up and one unit per meter. Preserve
node/mesh transforms and bind-space relationships rather than baking an extra
conversion into animation. Convert supported timing and interpolation into the
existing animation buffers; do not approximate unsupported animation forms.

The user's instruction is: “Add an ASSERT for the +4 influences and we will
handle if it ever happens. Same for the materials actually. YAGNI”. Assert on
more than four influences and unsupported material features; do not truncate,
reduce weights, approximate materials or add configurable fallback policies.

Record imported auxiliary inputs using the existing dependency-sidecar pattern:
the Assimp file-access path reports files actually read, including OBJ material
libraries and external textures. Store portable identities and content hashes
in local cache metadata so warm checks need not run Assimp just to discover them.
Missing/changed inputs dirty the affected asset and failures use normal aggregation.

## Critical files

New `DataPacker/Source/ExportJobs/Scene/AssimpSceneDecoder.{h,cpp}`;
`ExportScene.{h,cpp}`, `SceneExportData.h`, DataPacker project/filter files,
`Common/ExternalHeaders.h`, and scene documentation.

## In scope

- New `AssimpSceneDecoder.{h,cpp}`: supported import flags, input/dependency
  tracking, basis/units, geometry/defaults, image/material mapping, skeleton and
  supported animation decoding, and the requested ASSERT limits.
- `ExportScene.{h,cpp}`, `Handles`, `Export`/decoder dispatch,
  `GetInputFingerprint`, `UpdateCacheMetadata`: route the six exchange extensions
  to Assimp, preserve tinygltf routing, and persist/recheck imported dependencies
  using the established cache and job-failure lifecycle. Bump only the owning
  metadata version if its format changes; keep existing glTF fingerprints stable.
- `Scene/SceneExportData.h`: only concrete data adjustments required by these
  imported scenes and existing shared payload consumers; keep the interface small.
- `ExportScene.cpp`, shared texture/material/model/animation consumers: only
  adjustments required for actual decoded defaults and imported data, preserving
  existing glTF output, optimization, packed layouts and cleanup/publication.
- `Common/ExternalHeaders.h`, `BT_DATA_PACKER` Assimp includes: only additional
  consumption headers required by the decoder and file-access tracking.
- DataPacker `.vcxproj`/`.filters`: new source membership only.
- `DataPacker/Source/AGENTS.md` and `DataPacker/Source/ExportJobs/AGENTS.md`, Scene
  and Model: supported formats, decoder split, dependency and limit contracts.

## Out of scope

Assimp-to-glTF, plugin frameworks, runtime renderer/format changes, native `.max`,
new Assimp importers/exporters, dependency build changes, Blender behavior changes,
unsupported animation conversion, material approximation, influence reduction,
compatibility paths, production model replacement, and unit tests.

## Risk and invariants

Tier 3: imported mesh/material/skeleton/animation data crosses the shared payload
integration. Preserve current limits, one flattened supported skin, effective
material splits, model basis/meters, payload layouts, and old glTF bytes/CRCs.
Respect existing material/texture/node limits and validation; requested ASSERTs
must not be replaced by speculative rare-case machinery.

## Verification

Before editing, retain exact existing glTF/GLB scene/model/texture chunk bytes and
path/content CRCs. Prepare representative local static files for all six formats,
including untextured PLY/STL, textured OBJ plus MTL, and Blender-exported textured/
skinned FBX and DAE with known axes, dimensions, bind pose and timed poses.

| Acceptance | Evidence |
|---|---|
| Formats use correct decoders | Discovery/dispatch inspection and successful pack of each six-format static example; glTF/GLB stay tinygltf |
| Geometry/material defaults work | Rendered textured and untextured examples, triangle meshes and generated missing normals |
| Basis, size and animation match | `/agent-harness` known-axis/dimension, bind-pose and timed-pose comparison for FBX/DAE |
| Dependencies invalidate correctly | Change MTL/texture/auxiliary source, observe affected re-export; warm unchanged run clean; missing dependency fails its asset |
| Existing outputs are exact | Equal baseline glTF/GLB scene/model/texture bytes and path/content CRCs |
| Requested rare-case behavior exists | Read ASSERT sites and decoder flags; confirm no weight limiter/material approximation; build verification, without manufacturing corrupt inputs or temporary source edits |
| Integration builds | `/compile` DataPacker Release x64 and required client/server harness builds |

Use normal change workflow review and resolve every failed in-scope acceptance
criterion before completing this stage.
