<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:31:09.598Z","dependsOn":[]} -->
# Validate cached chunk bodies before clean republishing

## Context

The frozen audit retained `CAI/shard-0006/010`. `ExportJob::CheckDirty` checks
only the outer cache marker/version and source fingerprint at
`DataPacker/Source/ExportJobs/ExportJob.cpp:106-159`; `RunExport` returns the
body at `:168-188` without validating its embedded `ChunkHeader`. A complete
same-size cache edit can therefore be copied into a new pack. The source tree
matches baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`.

## Design

Before treating a cache chunk as clean, validate its complete serialized body:
header magic/version, path CRC against the job's `mCrc`, flags, header/data
extents, compression sizes, and exact body bounds. On any mismatch mark the job
dirty so `Export()` regenerates it; preserve the existing outer marker and
fingerprint checks as interruption guards.

## Critical files

- `DataPacker/Source/ExportJobs/ExportJob.cpp` — clean predicate and cache reader.
- `Common/DataFile.h` — chunk header/body contract.
- `DataPacker/Source/Main.cpp` — repackaging path.

## In scope

- Inner cached chunk validation before `RunExport` returns clean bytes.
- Dirty fallback and diagnostics for a complete-size corrupted cache body.

## Out of scope

- Runtime pack validation, cache digest format redesign, path-set dirtying, or publication atomicity.
- Changes to freshly generated chunk writers except to share an existing validator.

## Risk tier and invariants

Tier 2 (downgraded from Tier 3). Trigger: persistent opaque cache bytes cross a
serialized `.pack` publication boundary. A clean cache body must satisfy the
same identity/extent contract as a fresh export; valid clean jobs remain clean.

Tier rationale: the fix is a pre-specified header/extent check inside one
offline DataPacker clean predicate that falls back to the existing dirty
regeneration path. No chunk format, manifest layout, or runtime behavior
changes, and a valid cache still produces byte-identical output.

## Acceptance criteria

- Editing an inner header or payload without changing outer marker/size causes a dirty regeneration, not republishing.
- Valid cache chunks are reused with unchanged output bytes.
- No clean cache path returns a body whose header/path identity disagrees with its job.

## Notes

Origin: `CAI/shard-0006/010`, source selector
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0006.md:214`.
No source fix, build, or cache experiment was performed here.
