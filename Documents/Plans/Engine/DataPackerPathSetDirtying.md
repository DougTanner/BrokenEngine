<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:30:49.121Z","dependsOn":[]} -->
# Dirty DataPacker exports when the path set changes

## Context

The frozen audit retained `CAI/shard-0005/001`. `DiscoverExportJobsAndAggregateDirty`
compares only the manifest chunk count with the current job count at
`DataPacker/Source/Main.cpp:333-338`; the structural validator at `:278-301`
checks the old manifest against the old pack, not current job path CRCs. A
count-preserving rename can therefore take the clean return at
`Main.cpp:567-570` with stale cache/pack/header identity. No DataPacker source
diff exists from baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`, so this is
pre-existing.

## Design

When discovering jobs, compare the sorted current `mCrc` path-identity set
with the manifest location CRC set and reject duplicate/mismatched embedded
chunk paths. Set the existing aggregate dirty flag on any mismatch so the
normal dirty export rebuilds the pack and generated header; keep fingerprint,
count, and timestamp checks as complementary guards.

## Critical files

- `DataPacker/Source/Main.cpp` — discovery, manifest comparison, and dirty routing.
- `DataPacker/Source/ExportJobs/ExportJob.cpp` — cache/path identity.
- `Common/DataFile.h` — manifest/chunk identity fields.

## In scope

- Current-job versus manifest/packed path-CRC set comparison before clean return.
- Dirty routing and diagnostics for a count-preserving rename or replacement.

## Out of scope

- Cache body validation, cross-volume publication, content CRC, or runtime lookup redesign.
- Asset naming policy and generated symbol encoding.

## Risk tier and invariants

Tier 2 (downgraded from Tier 3). Trigger: serialized `.pack`/manifest identity
and generated build outputs must agree with current assets. Equal counts must
never certify a different path set; valid clean exports remain clean.

Tier rationale: the fix is a pre-specified path-CRC set comparison in one
offline DataPacker discovery function that sets the existing aggregate dirty
flag and reuses the normal dirty export. No manifest or pack format changes,
and a genuinely unchanged path set still takes the clean path.

## Acceptance criteria

- A same-count rename or path replacement marks the type dirty and republishes current manifest, pack, and header identities.
- A truly unchanged path set still takes the clean path.
- No stale removed path CRC remains in a successful clean output.

## Notes

Origin: `CAI/shard-0005/001`, source selector
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0005.md:50`.
No source fix or build was performed while recording this residual.
