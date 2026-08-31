<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T01:58:04.384Z","dependsOn":[]} -->
# Validate reused processed island meshes before serialization

## Context

Final survivor `S002-C009` is a retained HIGH cache trust-boundary finding. `ReadProcessedMesh` checks nonnegative counts, checked byte arithmetic, exact file size, and complete reads, then publishes positions and indices without requiring positive/triangle-complete geometry, finite coordinates, or `index < vertexCount` (`DataPacker/Source/ExportJobs/ExportIsland.cpp:289-350`). Runtime checks counts and byte extents but aliases/uploads the values without replacing the missing semantic scan. A modified or corrupted `MeshProcessed.bin` can therefore cross the island chunk publication boundary.

The final disposition is in `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-002.md` under `S002-C009 — FINAL: RETAIN_HIGH`; the source report is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-002.md:161` and the consolidated selector is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:96`. The target remained at baseline `a20acf1e31a24a0f61ae638e8976602b49655788`.

## Design

The author's recommendation is to retain the existing exact file/byte checks and, before moving the decoded arrays into island output, require a positive vertex/index shape, triangle-complete index count, finite position values, and every index below the decoded vertex count. Report a malformed reused cache through the existing export-job aggregate and do not publish an island chunk. Keep the upstream Gaea reader and runtime checks as separate defenses, and preserve valid cached output bytes.

## Critical files

- `DataPacker/Source/ExportJobs/ExportIsland.cpp:289-350` — `ReadProcessedMesh` cache reader.
- `DataPacker/Source/ExportJobs/ExportIsland.cpp` — island chunk serialization and existing export failure path.
- `Documents/Plans/Engine/GaeaMeshShapeValidation.md` — upstream Gaea validation owner (coordination reference only).
- `DataPacker/Source/ExportJobs/Island/AGENTS.md` — mesh and island-output contract.

## In scope

- Semantic validation of reused `MeshProcessed.bin` positions, counts, and indices before island serialization.
- Structured failure propagation for malformed processed-mesh cache data.
- Preservation of valid cache reuse and downstream runtime structural checks.

## Out of scope

- Upstream Gaea accessor validation, shoreline subdivision, runtime GPU value scans, or mesh remapping.
- Island payload layout/version changes, new cache formats, or compatibility readers.
- Unrelated island dimensions/elevation validation or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (scoped DataPacker cache-reader behavior). Trigger: opaque cached mesh values cross an existing serialized island/GPU boundary, while the fix is a pre-specified producer-side validation with unchanged valid output and no format or threading change.

Preserve these invariants:

- Every serialized processed mesh has finite positions, a positive triangle-complete shape, and in-range indices.
- Invalid cache data fails before island chunk allocation/publication through the export aggregate.
- Valid processed meshes remain byte-compatible and downstream runtime checks remain intact.

## Acceptance criteria

- A cache with a finite but out-of-range index, non-finite position, zero/incomplete mesh, or exact-size malformed array fails before island serialization.
- A valid cached mesh is serialized identically to the current output.
- DataPacker `Debug|x64` builds pass through `/compile`.

## Coordination

`Documents/Plans/Engine/GaeaMeshShapeValidation.md` owns the upstream Gaea reader. Keep upstream source validation and reused-cache validation as separate boundaries and preserve the common island export aggregate; neither Plan substitutes for the other.

## Notes

Origin: `S002-C009`; disposition selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-002.md` (`S002-C009 — FINAL: RETAIN_HIGH`), source selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-002.md:161`, and consolidated selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:96`. The audit explicitly found the Gaea plan non-duplicative because it does not validate later reused-cache bytes. No source fix or build was performed during routing.
