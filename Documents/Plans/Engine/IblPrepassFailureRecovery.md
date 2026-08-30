<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-30T21:49:04.780Z","dependsOn":[]} -->
# Aggregate IBL prepass failures and recover published outputs

## Context

The current DataPacker source is byte-identical to frozen audit commit
`80896f3`. The public IBL passes `GenerateIrradianceCubemaps` and
`GeneratePreFilteredCubemaps` return `void` and have no local failure
aggregation. `ProcessKtxCubemaps` and `ProcessFaceImageCubemaps` can throw
while loading an input, running CMFT, or writing an intermediate, while the
irradiance loop has the same direct-failure shape
(`DataPacker/Source/ExportJobs/ExportCubemapIbl.cpp:239-322,378-562`).
`MainThread` calls both passes outside the `bSuccess` fold, before later IBL
and ordinary exports (`DataPacker/Source/Main.cpp:690-723,776-811`), so one
failure can skip independent work and its aggregate diagnostics.

The IBL publication path also writes directly to final output paths. Each
dirty item calls `BeginOutputUpdate`, removing completed metadata after making
the dirty marker; the irradiance writer and `WriteFilteredCubemap` open the
final path with `std::ios::out | std::ios::binary`. A write failure can
therefore leave partial final bytes, a dirty marker, and no completed metadata
(`ExportCubemapIbl.cpp:105-116,270,310-318,366-373,407-429,497-525`).
`ReconcileIblOutputs` removes orphan paths only when a mirrored sidecar proves
producer ownership, so reconciliation alone cannot restore the expected
previous complete output.

## Design

The author's recommendation is one combined Tier-2 scoped DataPacker fix,
because failure aggregation and output recovery are two halves of the same
IBL prepass boundary. Each public pass should return `bool`, accumulate
per-input `std::exception` failures with asset paths, and report exactly one
structured diagnostic for that pass. The prefiltered pass should aggregate
KTX and face-image failures together. Pass-level discovery and reconciliation
failures should also be caught. `MainThread` should fold both results with
non-short-circuit `bSuccess &=` so the other IBL pass and ordinary exports
still run.

The recommended publication transaction is a deterministic, producer-owned
sibling stage in the same directory. Create the dirty marker first while the
prior final remains intact; fully write and check the stage; replace the final
with `MoveFileExW(MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`; atomically write the
completed metadata; and remove the dirty marker last. On failure, remove the
attempt stage where possible, preserve the prior final, and leave the dirty
marker with no completed metadata. A crash before replacement therefore keeps
the old complete final, while a crash after replacement leaves the new
complete final; either state is retried when dirty. Retry/reconciliation may
clean a stale stage only when its mirrored sidecar proves ownership.

Keep existing formats, output names, fingerprints, CMFT settings, raw-texture
routing, and ordinary export behavior unchanged. Do not add a production
failure-injection seam or unit tests.

## Critical files

- `DataPacker/Source/ExportJobs/ExportCubemapIbl.cpp` — `BeginOutputUpdate`, `CompleteOutputUpdate`, `ReconcileIblOutputs`, `GenerateIrradianceCubemaps`, `WriteFilteredCubemap`, `ProcessKtxCubemaps`, `ProcessFaceImageCubemaps`, and `GeneratePreFilteredCubemaps`.
- `DataPacker/Source/ExportJobs/ExportCubemapIbl.h` — public IBL pass result declarations.
- `DataPacker/Source/Main.cpp` — `MainThread` result folding and later-export order.

## In scope

- Failure capture and one structured aggregate diagnostic per `GenerateIrradianceCubemaps` and `GeneratePreFilteredCubemaps` pass, including asset paths and pass-level discovery/reconciliation failures.
- Non-short-circuit result folding in `MainThread` so both IBL passes, ordinary exports, generated headers, and attribution work continue after an earlier failure.
- Producer-owned same-directory stage creation, complete-write verification, final replacement, metadata publication, dirty-marker lifecycle, stale-stage cleanup, and retry recovery for the irradiance and prefiltered IBL intermediates.
- Preserving the last complete final output and truthful dirty/completed metadata state through write failure and process termination.

## Out of scope

- CMFT algorithms or settings, cubemap formats, output naming, cache fingerprint formulas, raw-texture routing, or the ordinary `RunExportJobs` aggregate.
- Runtime consumers, wire/replay formats, `.pack` layout, generated data identities, and unrelated export-job publication paths.
- New compatibility formats, unit tests, production failure-injection hooks, implementation, build, or commit in this planning stage.

## Risk tier and invariants

Expected future Change Workflow Tier 2 (scoped DataPacker behavior). Re-
evaluate if implementation crosses a separate output-owner or runtime
contract. The transaction must keep final bytes, the dirty marker, and the
completed fingerprint mutually truthful; failed attempts must not destroy the
last complete output or remove another producer's file. Failure aggregation
must not skip independent asset types, and existing deterministic output
identity and CMFT/raw routing must remain unchanged.

## Acceptance criteria

- A disposable noninteractive DataPacker fixture first establishes a valid IBL output and metadata, then dirties the source, blocks the deterministic stage path by creating a directory there, and supplies a malformed IBL input followed by a valid later IBL input in that same pass.
- The blocked run leaves the old final hash unchanged, leaves the dirty marker present with completed metadata absent, emits one aggregate per failed pass with identifiable input entries, proves the valid later same-pass output, the other IBL pass, ordinary exports, generated headers, and attribution complete, and exits failure.
- After the obstruction is removed, retry succeeds, removes the owned stage and dirty marker, and publishes complete output and metadata with unchanged naming and fingerprint rules.
- A static transaction-order check covers the before-replacement and after-replacement termination states when deterministic process termination is impractical; no production failure-injection seam is added.
- The DataPacker target compiles through the repository build workflow; no unit tests are added.

## Notes

Source evidence is the frozen audit candidate `CPT/shard-0006/004` and
`80896f33661aaab99cf180a96db54600099be652`; the current tree is byte-identical
to that source. No source implementation, build, or DataPacker run is part of
this Plan creation.

## Coordination

No directional prerequisite is required. Keep this IBL-specific transaction
separate from `Documents/Plans/Engine/SceneTexturePublicationRollback.md`,
which owns scene texture publication and has a different output boundary.
