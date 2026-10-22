<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-21T19:06:13.714Z","dependsOn":[]} -->
# Automatically export local Blender sources before scene discovery

## Context

Blender is the priority authoring tool. Local `.blend` files may coexist with
committed sibling `.glb` exports; only exports are stored in the repository.
Packing must convert changed local sources automatically, without requiring
Blender on an export-only checkout or launching it on an unchanged warm run.

## Design

The recommended design is a small DataPacker prepass before scene discovery and
a repository-owned Blender 4.5 LTS Python export script. `Chair.blend` owns and
replaces `Chair.glb`; only the GLB participates in scene identity and generated
CRC names. Use an installed Blender 4.5 LTS, located through normal Windows
installation/PATH discovery, and fail the affected asset when conversion needs
it but it is unavailable. Do not add an installer or another user configuration.

Record source and external dependency content hashes, converter identity,
conversion-policy/script version and output hash in the existing local cache.
The script reports the external files its export actually consumes, including
linked Blender data and unpacked images. Reuse that dependency list for warm
checks; a changed/missing dependency, source, converter, policy, or output dirties
conversion. Resolve the recorded converter identity without launching Blender on
a clean check. Export to a sibling staged GLB, validate it through the existing
tinygltf scene loader, then atomically publish and commit cache metadata only on
success. Never accept a retained old GLB as success after a dirty conversion fails.

## Critical files

`DataPacker/Source/Main.cpp`, new `DataPacker/Source/BlenderExport.{h,cpp}` and
`DataPacker/Scripts/ExportBlenderScene.py`, `ExportScene.{h,cpp}`, `.gitignore`,
DataPacker project/filter files, and pipeline documentation.

## In scope

- `Main.cpp`, `MainThread`: invoke conversion after FileManager initialization/
  migration and before `RunExportJobs<ExportScene>`; aggregate conversion failures
  and continue unrelated asset processing under the existing single-instance mutex.
- New `BlenderExport.{h,cpp}`: source enumeration, installed-tool resolution,
  local cache/dependency fingerprints, process launch and diagnostics, staging,
  validation/publication, and failed-source tracking for the current run.
- New `DataPacker/Scripts/ExportBlenderScene.py`: background GLB export using
  Blender 4.5 LTS, existing supported static/skeletal data, consumed-dependency
  reporting, and failure exit status. Keep source `.blend` files unmodified.
- `ExportScene.{h,cpp}`, `Handles`/`Export`: connect failed-source tracking so
  an owned old GLB cannot report success or publish a stale scene after failure;
  retain one GLB asset identity and ordinary export-only behavior.
- `.gitignore`: ignore `.blend` and Blender backup suffixes only in engine/game
  asset directories; retain sibling GLB exports and existing intermediates.
- DataPacker `.vcxproj`/`.filters`: new C++ and script membership only.
- `DataPacker/Source/AGENTS.md`, architecture/fingerprints; and
  `DataPacker/Source/ExportJobs/AGENTS.md`, Scene and Model: conversion ordering,
  optional tool, local sources, ownership, cache and failure rules.

## Out of scope

Assimp loading, runtime changes, tracked `.blend` files, Git operations during
packing, ownership receipt files, general converter plugins, Blender installation,
unsupported animation conversions, and unit tests.

## Risk and invariants

Tier 2: scoped offline asset conversion. Preserve scene-before-model ordering,
existing aggregation, output atomicity, material/skin limits and runtime formats.
Conversion failure preserves the prior GLB bytes but fails its affected asset and
overall run; export-only inputs never require a converter. User-authorized sibling
replacement needs no additional ownership mechanism.

## Verification

Before edits, record unchanged GLB/glTF output bytes and CRCs, existing local cache
behavior, and the installed Blender identity. Prepare local static, textured and
supported skinned `.blend` examples without committing them.

| Acceptance | Evidence |
|---|---|
| Changed source converts automatically | Packing alone creates/replaces valid sibling GLB, then normal scene/model/texture exports |
| No duplicate identities | Manifest/generated symbols contain one GLB identity and no `.blend` identity |
| Cache follows consumed inputs | Source, linked data/image, converter identity, policy and output changes each invalidate conversion |
| Warm run launches no Blender | Process observation plus unchanged output/cache hashes |
| Export-only checkout needs no Blender | Packing succeeds with no local `.blend` and no installed Blender available |
| Failures preserve files and surface | Missing tool and failed export keep old GLB byte-identical, return failed affected asset/nonzero run, and allow unrelated exports |
| Existing assets stay identical | Exact glTF/GLB byte and CRC baseline comparison |
| Build and visible poses work | `/compile` DataPacker Release x64 and required client/server builds; `/agent-harness` checks axes, size, bind pose and timed supported animation |

Use normal change workflow review and verification; do not manufacture unsupported
material or influence cases by editing source code for verification.
