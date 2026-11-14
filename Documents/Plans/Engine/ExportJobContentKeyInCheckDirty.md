<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-13T23:07:49.191Z","dependsOn":[]} -->
# Compute every export job's content key in ExportJob::CheckDirty

## Context

Every DataPacker export job names its shared-cache entry with a content key
set by `ExportJob::SetContentKey`
(`DataPacker/Source/ExportJobs/ExportJob.cpp`). Three sites compute that key
today:

- `ExportJob::CheckDirty` keys every ordinary job from `GetInputFingerprint`,
  inside a `try`/`catch` that turns a failed fingerprint into a warning and a
  dirty job, guarded by `if (mContentKey.empty())` so it does not overwrite a
  key a constructor set.
- The `ExportShader` constructor
  (`DataPacker/Source/ExportJobs/ExportShader.cpp`) calls
  `SetContentKey(ComputeShaderContentKey(...))`, which fingerprints the shader
  source and walks and fingerprints every `.h` under both input roots' `Shaders`
  directories, and then builds `mDependencyMetadataFile` from `mContentKey`.
- The `ExportScene` constructor
  (`DataPacker/Source/ExportJobs/ExportScene.cpp`) calls
  `SetContentKey(BlenderExportContentKey(mBlenderSourcePath))` for a Blender
  source. `BlenderExportContentKey` (`DataPacker/Source/BlenderExport.cpp`)
  fingerprints the source and the conversion script outside any catch, and
  wraps only its candidate image walk in its own `try`/`catch`.

Constructors run inside `DiscoverExportJobsAndAggregateDirty`
(`DataPacker/Source/Main.cpp`), which constructs a job and calls `CheckDirty`
on the next line, with no per-asset catch. `InputFingerprintCache::Get`
(`DataPacker/Source/InputFingerprint.cpp`) throws on a file it cannot open, and
`std::filesystem::recursive_directory_iterator` throws on a directory it cannot
walk. A throw from either constructor therefore leaves `RunExportJobs`,
reaches the top-level handler in `RunCommandWithExceptionHandling`, and ends
the project's run before the remaining asset types export. A failed read of a
shader source, a shader header, a Blender source, or the conversion script
does this today.

That contradicts the behavior the same code already states and half
implements: the comment above the `try` in `ExportJob::CheckDirty` says the
failure must stay per asset, `DataPacker/Source/AGENTS.md` `## Architecture`
says every asset type runs after an earlier failure, and
`BlenderExportContentKey` carries an image-walk catch only because it runs from
a constructor.

Origin: a refactor candidate logged in
`Documents/Investigations/Engine/RefactorCandidates.md`, validated against
commit `213d2577`.

## Design

Recommended: make `ExportJob::CheckDirty` the only caller of `SetContentKey`,
and let a job say what its key covers through one virtual function.

- `ExportJob` gains a protected virtual `const` function, recommended as
  `GetContentKeyInput`, taking `const std::string& rInputFingerprint` and
  returning `std::string`; the base version returns `rInputFingerprint`. It
  takes the fingerprint `CheckDirty` already computed so an ordinary job does
  not fingerprint twice.
- In `ExportJob::CheckDirty`, the existing `try` block computes the fingerprint
  and then passes `common::Crc` of `GetContentKeyInput(inputFingerprint)` to
  `SetContentKey`. The existing `catch` is unchanged: warning,
  `mbDirty = true`, return. The `if (mContentKey.empty())` guard goes away.
- `ExportShader` overrides the virtual to return the source fingerprint
  followed by the sorted header fingerprints, the same bytes
  `ComputeShaderContentKey` hashes today; that static function becomes the
  override body. `ExportShader` does not override `GetInputFingerprint`, so the
  fingerprint passed in is the same source fingerprint the constructor reads
  now. The constructor drops its `SetContentKey` call and its two
  `mDependencyMetadataFile` lines. `ExportShader::CheckDirty` builds
  `mDependencyMetadataFile` right after `ExportJob::CheckDirty` returns and
  before its early return on dirty, because a dirty export writes that path in
  `UpdateCacheMetadata`.
- `ExportScene` overrides the virtual: a Blender source returns the string
  `BlenderExportContentKey` hashes today, every other scene returns
  `rInputFingerprint`. `BlenderExportContentKey` returns that string instead of
  its CRC (rename it to match, for example `BlenderExportContentKeyInput`) and
  loses its image-walk `try`/`catch` and the comment that explains it; the
  rationale comment in the `ExportScene` constructor moves to the override. The
  constructor body becomes empty.
- `ExportScene::CheckDirty` reads the Blender marker through
  `GetBlenderExportMarkerPath`, which uses `mContentKey`. Recommended: call
  `ExportJob::CheckDirty` first and keep its result, then run the existing
  Blender block (`BlenderExportDirty` true sets `mbDirty` and
  `mbNeedsPreExport` and returns true), then continue with the Assimp
  dependency check and the pre-export branch as now. This is recommended over
  a separate key-deriving call because it keeps one place that derives the key.

Why this is safe:

- The key bytes do not change. Each override returns exactly the string the
  constructor hashed, and `common::Crc` of it is taken once, so every cache
  entry and marker keeps its current name and a warm cache stays warm.
- A job whose key derivation throws is dirty and reads no cache entry, exactly
  as an ordinary job with a failed fingerprint does today: `CheckDirty` returns
  before it opens the chunk or the `.meta`. Such a job fails in `RunExport`
  when the read fails again, through the job's future and the type's aggregate
  diagnostic.
- `CheckDirty` still runs on the main thread during discovery, directly after
  construction and before any `std::async` launch, so `mContentKey`,
  `mChunkFile`, `mCacheMetadataFile`, and `mDependencyMetadataFile` are still
  final before a worker thread reads them.
- Calling `ExportJob::CheckDirty` before the Blender marker check only adds
  reads of the chunk header and `.meta` for a scene whose conversion is dirty;
  the returned value and `mbNeedsPreExport` are the same.

## Critical files

- `DataPacker/Source/ExportJobs/ExportJob.h`
- `DataPacker/Source/ExportJobs/ExportJob.cpp`
- `DataPacker/Source/ExportJobs/ExportShader.h`
- `DataPacker/Source/ExportJobs/ExportShader.cpp`
- `DataPacker/Source/ExportJobs/ExportScene.h`
- `DataPacker/Source/ExportJobs/ExportScene.cpp`
- `DataPacker/Source/BlenderExport.h`
- `DataPacker/Source/BlenderExport.cpp`
- `DataPacker/Source/ExportJobs/AGENTS.md`

## In scope

- `ExportJob`: the new virtual's declaration and base definition; the `try`
  block and the `mContentKey.empty()` guard in `ExportJob::CheckDirty` and the
  comment above that `try`; the comments on `mContentKey` and on
  `SetContentKey` in `ExportJob.h` that say a constructor may set the key.
- `ExportShader`: the constructor's `SetContentKey` call and
  `mDependencyMetadataFile` lines; `ComputeShaderContentKey`; the new override;
  the top of `ExportShader::CheckDirty`.
- `ExportScene`: the constructor body and its comment; the new override; the
  order of the base call and the Blender block in `ExportScene::CheckDirty`.
- `BlenderExportContentKey` in `BlenderExport.h` and `BlenderExport.cpp`: its
  name, return type, image-walk `try`/`catch`, and leading comment.
- `DataPacker/Source/ExportJobs/AGENTS.md` `## Shared Pipeline`: the sentence
  that says the Shader and Blender-source Scene constructors set their own
  keys, and the `BlenderExportContentKey` name in `## Scene and Model` if the
  function is renamed.

## Out of scope

- What any key covers and the bytes it hashes; `SetContentKey`'s body and the
  entry-name format. No cache entry, marker, or sidecar is renamed and no
  export version is bumped.
- Folding the path CRC `mCrc` into the key, and the chunk-header CRC check in
  `ExportJob::RunExport`: a separate candidate (see `## Coordination`).
- `DiscoverExportJobsAndAggregateDirty` and the rest of `Main.cpp`; no
  per-asset catch is added to discovery.
- `BlenderExportDirty`, `ExportBlenderScene`, `FindBlenderCandidateImages`,
  and the conversion marker's content.
- Other throws reachable from `CheckDirty` overrides outside the base `try`
  (for example the shader dependency comparison in `ExportShader::CheckDirty`).
- `ExportIsland`, `ExportModel`, and every job that sets no key of its own.

## Risk tier and invariants

Tier 2 under `.agents/references/risk-tiers.md`: scoped tool behavior inside
DataPacker's export jobs — one failure path changes from ending the run to
failing one asset. It changes no serialized format, cache layout, entry name,
or export version, adds no thread, and does not touch runtime code,
determinism/CRC, or a trust boundary. Invariants to keep:

- Cache correctness: a job with no key reads no cache entry; a job with a key
  gets the same key value as before.
- Threading: every path derived from the key is final before the job's
  `std::async` launch.
- Per-asset failure: an export failure still reaches the type's aggregate
  diagnostic through the job's future.

## Acceptance criteria

- `SetContentKey` has exactly one caller, `ExportJob::CheckDirty`, inside its
  `try` block; neither the `ExportScene` nor the `ExportShader` constructor
  calls `InputFingerprintCache::Get` or walks a directory.
- `BlenderExport.cpp`'s key function has no `try`/`catch`, and
  `ExportJob::CheckDirty` has no `mContentKey.empty()` test.
- DataPacker builds through `/compile`.
- Entry names are unchanged: with the cache and output warm from a run of the
  pre-change DataPacker on the same tree, a run of the changed DataPacker's
  `kInfo` run summary reports zero exported and zero failed jobs.

## Notes

- A read failure cannot be staged without editing tracked assets or holding a
  file lock from another process, so the per-asset behavior is settled by the
  first two criteria and the unchanged `catch`, not by a live run.
- A persistent key failure (for example a `Shaders` directory missing under an
  input root) changes from ending the run to a warning per shader plus a
  re-export every run. Both input roots have that directory today.
- Do not add unit tests.
