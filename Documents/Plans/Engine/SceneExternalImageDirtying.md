<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:30:52.843Z","dependsOn":[]} -->
# Include external scene images in dirty fingerprints

## Context

The frozen audit retained `CAI/shard-0006/001`. `ExportScene::CheckDirty`
delegates to the base fingerprint and only checks the `.gltf` plus a version
marker (`DataPacker/Source/ExportJobs/ExportScene.cpp:26-48`); the base returns
`GetFingerprint(mInputPath)` at `ExportJob.cpp:232-234`. External URI image
bytes are consumed during `PreExport` (`ExportScene.cpp:249-307`) but are not
part of that fingerprint, so editing an image leaves generated scene textures
clean. No source diff from baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`
exists, proving this is pre-existing.

## Design

Resolve every external URI dependency used by the scene and combine its
content fingerprint with the glTF fingerprint in deterministic URI order.
Keep the pre-export version marker and generated intermediate ownership, and
use the established FileManager fingerprint/error path for missing or changed
dependencies so a changed image forces the existing pre-export phase.

## Critical files

- `DataPacker/Source/ExportJobs/ExportScene.cpp` — dependency enumeration and dirty state.
- `DataPacker/Source/ExportJobs/ExportJob.cpp` — existing fingerprint mechanism.

## In scope

- External URI image dependency discovery and inclusion in `ExportScene::CheckDirty`.
- Fingerprint serialization/order and dirty routing into `PreExport`.

## Out of scope

- glTF parsing semantics, embedded image handling, texture format selection, or cache publication transactions.
- New asset dependency database or broad source-tree watcher behavior.

## Risk tier and invariants

Tier 2 (downgraded from Tier 3). Trigger: a DataPacker fingerprint controls
serialized scene/texture publication across a cache/runtime boundary. Any input
that affects scene output must dirty it; unchanged scenes remain clean and
deterministic.

Tier rationale: the change stays inside one exporter's dirty check, combining
existing FileManager fingerprints for the images the scene already reads, in the
deterministic URI order the Design fixes. No packed output layout or runtime
consumer changes; a stale cache simply rebuilds through the normal pre-export
path.

## Acceptance criteria

- Editing only a referenced external image causes the scene's generated texture/model outputs to rebuild on the next run.
- Unchanged external images preserve the clean path.
- Multiple URI dependencies produce the same fingerprint independent of discovery order.

## Notes

Origin: `CAI/shard-0006/001`, source selector
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0006.md:52`.
No source fix, build, or DataPacker run was performed in this routing stage.

## Coordination

`Documents/Plans/Engine/SharedTextureFormatInference.md` and
`Documents/Plans/Engine/SceneTexturePublicationRollback.md` also touch
`ExportScene.cpp`; preserve their separate dirty, format, and publication
boundaries and re-read the current file before implementation.
