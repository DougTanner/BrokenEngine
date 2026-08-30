<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-30T21:38:21.092Z","dependsOn":[]} -->
# Make the scene pre-export marker own generated outputs

## Context

`ExportScene::CheckDirty` currently delegates to the outer scene cache check and
then accepts a matching `int64_t` version read from `.PreExport`
(`DataPacker/Source/ExportJobs/ExportScene.cpp:27-49,256-267`).  The marker
written by `WriteModelFile` contains only that version
(`ExportScene.cpp:663-748`).  A clean scene therefore does not prove that its
generated `.MODEL` and block-compressed texture intermediates still exist or
have the contents that were present when the scene chunk was published.

`MainExport` consumes the model and constructs the scene's model and texture
path CRCs from those intermediates (`ExportScene.cpp:750-815`).  The later
`DiscoverExportJobsAndAggregateDirty` pass in `DataPacker/Source/Main.cpp:337-365`
creates model/texture jobs only for files that are present, so deletion can
leave the scene clean while its published CRCs name no manifest entry.
`DynamicPipelines::CreateModelPipeline` and
`ModelPipeline::WriteIndirectBuffer` reject or assert on those missing CRCs
(`Engine/Source/Graphics/Managers/DynamicPipelines.cpp:25-43`;
`Engine/Source/Graphics/Objects/ModelPipeline.cpp:187-203`).  The source
evidence is present in the current tree; the originating frozen audit record
was `CPT/shard-0006/007`.

## Design

Keep `.PreExport` as the single authoritative, deterministic ownership record
for a scene's generated outputs.  Replace the version-only payload with a
versioned binary record containing the marker schema/version, the current
`ExportScene::GetVersion()`, and a sorted, duplicate-free list of entries.  An
entry stores the canonical root-relative output path and its raw SHA-256 content
fingerprint from the existing `gpFileManager->GetFingerprint` mechanism.  Use
the same relative-path spelling that `mRelativeFile`, `mRelativeDirectory`,
and `MainExport` use for the path CRCs.  Write the record to a temporary sibling
and atomically replace `.PreExport`; malformed, truncated, old, or otherwise
unreadable records are not accepted as clean.

Derive the expected set with the existing `LoadGltfModel` parsing and
`ComputeTextureFormats` inference.  Always include the scene's `.MODEL` path;
for textures, include one path per unique `(image source, inferred format)` pair
using the existing `GetTextureIntermediatePath` naming.  Reuse one small
enumeration/helper path in `ExportScene.cpp` (and only the corresponding
declaration or record type in `ExportScene.h` if needed) so the CheckDirty
comparison, texture publication, marker entries, and MainExport CRC path
construction cannot disagree.

After the base cache check is clean, `CheckDirty` reads the complete marker and
derives that expected set.  It marks the scene dirty and sets the existing
pre-export flag when the marker version is wrong, the marker membership differs,
an expected output is missing or not a regular file, or an output fingerprint
differs from the recorded identity.  A scene parse needed for this comparison
must retain the existing per-export failure path when it cannot be read; it
must not turn an invalid source into a falsely clean scene.

`PreExport` continues to run the existing deduplicated texture workers,
publication, orphan sweep, skeleton/material work, and model writer.  Remove
the early version-only marker write from `WriteModelFile`.  Once every required
texture final and the `.MODEL` file has succeeded, enumerate the complete final
set, fingerprint it, and commit the marker as the final PreExport publication
step.  Keep the marker and generated paths connected to the existing
`mIntermediateFiles`, `mTextureAttemptFiles`, `mPublishedTextureFiles`, and
`CleanupOnFailure` ownership so a failed attempt follows the current cleanup
and aggregate-failure behavior.  This Plan does not add rollback for a
successfully replaced texture prefix; that is the separate publication-recovery
boundary.

The separate-manifest alternative was considered and rejected.  It would add a
second ownership artifact and require broader coordination with independent
model/texture discovery, while the existing sidecar already sits at the scene
producer boundary and can carry the required membership and content identities
without changing pack or manifest formats.

## Critical files

- `DataPacker/Source/ExportJobs/ExportScene.cpp:27-49` — complete clean predicate and pre-export dirty flag.
- `DataPacker/Source/ExportJobs/ExportScene.cpp:249-284` — marker parsing, deterministic output enumeration, and path helpers.
- `DataPacker/Source/ExportJobs/ExportScene.cpp:306-354,663-748` — PreExport ordering, model output, and marker commit.
- `DataPacker/Source/ExportJobs/ExportScene.cpp:356-538,1059-1083` — existing texture publication and failure ownership to preserve.
- `DataPacker/Source/ExportJobs/ExportScene.cpp:750-815` — scene CRC construction, kept aligned with the recorded relative paths.
- `DataPacker/Source/ExportJobs/ExportScene.h:48-73` — only minimal record/enumeration declarations or state required by the implementation.

## In scope

- Extending `.PreExport` from a version-only sidecar to the versioned deterministic path/fingerprint record, including bounded complete reads, atomic write/replace, and invalid-record dirtying.
- Deriving and comparing the `.MODEL` plus deduplicated scene texture output set in `ExportScene::CheckDirty` with existing scene parsing, format inference, and relative CRC path rules.
- Committing the marker only after the complete generated set succeeds, while preserving existing texture staging/publication, model-output registration, cleanup, and independent later export discovery.
- Keeping `MainExport`'s existing scene payload and path-CRC format, with only the minimal shared path construction needed to prove every recorded output matches an emitted CRC.

## Out of scope

- Runtime fallback or changes to `DynamicPipelines`, `ModelPipeline`, or other runtime consumers.
- Pack/manifest formats, generated CRC header formats, or redesigning `Main.cpp`'s independent model/texture discovery.
- Rollback or restoration of a successfully replaced texture prefix; `SceneTexturePublicationRollback` owns that separate publication-recovery work and is not a dependency of this Plan.
- `CacheChunkBodyValidation`, unrelated cache validation, texture encoding/format policy, scene serialization layout, and any new backward-compatibility path for the old version-only marker.
- Deterministic simulation, wire/save/replay formats, and unit tests.

## Risk tier and invariants

Expected future implementation tier: Tier 3.  Trigger: the marker and
generated producer outputs cross the DataPacker publication boundary while the
scene chunk carries path CRCs consumed by runtime model/texture lookup.  The
change therefore exposes serialized sidecar identity, publication ordering,
and an offline-to-runtime completeness invariant even though it does not edit
runtime code or the pack/manifest schema.

Preserve these invariants:

- The marker's canonical entries are deterministic, sorted, duplicate-free,
  and contain exactly one `.MODEL` output plus one entry per unique source/
  format texture intermediate required by the current scene.
- A clean scene requires a valid marker, exact expected membership, existing
  regular files, and matching raw content fingerprints; any mismatch forces
  PreExport before scene publication.
- A successful marker commit follows successful completion of every generated
  output.  A failed regeneration keeps the existing cleanup and per-type
  aggregate behavior; this Plan makes no new prior-output rollback claim.
- Existing complete outputs remain clean and byte-identical, and MainExport's
  relative path CRCs continue to resolve through the later independent model
  and texture manifests after a successful run.
- No simulation CRC, wire/save/replay data, runtime fallback, or pack/manifest
  format changes are introduced.

## Acceptance criteria

- With unchanged source and a complete matching output set, a warm DataPacker
  run accepts the marker without PreExport and leaves the scene and generated
  outputs byte-identical.
- Deleting any required `.MODEL` or deduplicated texture intermediate, or
  replacing one with different contents, makes `CheckDirty` force PreExport;
  the regenerated set is complete before the scene chunk is published.
- Adding/removing or changing a scene texture's inferred source/format pair
  changes marker membership, forces PreExport, and leaves no stale scene
  intermediate in the existing orphan sweep.
- A missing, malformed, truncated, or old version-only marker never selects a
  clean scene path and is replaced only after the complete generated set has
  succeeded.
- If regeneration or marker publication fails, existing stage/final/model
  cleanup and structured failure reporting remain in force, no rollback beyond
  that existing contract is claimed, and later asset types still run.
- For a successful export, every model and texture path CRC emitted by
  `MainExport` has a corresponding entry in the resulting model/texture
  manifests; no runtime fallback is needed.

## Notes

This is the decision-complete follow-up for the frozen `CPT/shard-0006/007`
finding.  The current Plan-document move/deletion is Tier 1 documentation
work; the implementation is Tier 3 under the trigger above.  No source fix,
build, or live runtime scenario is part of Plan creation.

## Coordination

`Documents/Plans/Engine/SceneTexturePublicationRollback.md` owns recovery when
texture final-path publication fails after a successful prefix.  Keep that
transaction boundary separate, do not add a dependency edge, and reconcile
overlapping `ExportScene.cpp` regions when implementing both Plans.  This Plan
only consumes the existing publication and cleanup behavior.
