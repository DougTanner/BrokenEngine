<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T01:58:14.410Z","dependsOn":[]} -->
# Validate inverse-bind accessors before skeleton export

## Context

Final survivor `S002-C012` is a retained HIGH skeleton-reader finding. `LoadSkeletonData` indexes the inverse-bind accessor, buffer view, and buffer without range checks, asserts only float component type, forms one tightly packed pointer, and reads sixteen floats per joint (`DataPacker/Source/ExportJobs/Scene/SceneSkeletonLoader.cpp:26-63`). The final locator refuted only the proposed valid-stride clause: glTF inverse-bind accessors are tightly packed and `byteStride` is not a valid requirement here. It confirmed the remaining FLOAT/MAT4/count/reference/offset/span contract.

The final disposition is in `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-002.md` under `S002-C012 — FINAL: RETAIN_HIGH`; the source report is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-002.md:197` and the consolidated selector is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:99`. The target remained at baseline `a20acf1e31a24a0f61ae638e8976602b49655788`.

## Design

The author's recommendation is to validate the accessor, buffer-view, and buffer references; require FLOAT MAT4 elements and at least one matrix per joint; check offsets and the complete tightly packed matrix span before forming the pointer; and reject unsupported non-tightly-packed shapes rather than honoring an invalid stride. Keep the missing-inverse-bind identity path, valid skeleton output, and existing export aggregate.

## Critical files

- `DataPacker/Source/ExportJobs/Scene/SceneSkeletonLoader.cpp:26-63` — inverse-bind reads.
- `DataPacker/Source/ExportJobs/Scene/SceneSkeletonLoader.h` — skeleton loader contract.
- `DataPacker/Source/ExportJobs/ExportScene.cpp` — skeleton export caller and failure aggregation.
- `DataPacker/Source/ExportJobs/AGENTS.md` — opaque glTF reader contract.

## In scope

- Inverse-bind reference, type, count, offset, tightly packed span, and per-joint coverage validation.
- Failure propagation before inverse-bind pointer formation or serialized skeleton publication.
- Valid inverse-bind and absent-inverse-bind behavior.

## Out of scope

- Animation sampler/cardinality validation, selected-skin identity, runtime skeleton checks, or skin remapping.
- Adding support for nonconforming inverse-bind strides, changing skeleton layout/version, or compatibility readers.
- New unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (scoped DataPacker scene-reader behavior). Trigger: opaque glTF skeleton bytes cross a serialized runtime-skinning boundary, while the correction is a local pre-read validation with unchanged valid layout and no threading/protocol change.

Preserve these invariants:

- Every accepted inverse-bind accessor is FLOAT MAT4, covers the joint count, and is fully in bounds before reading.
- Malformed skeleton input fails through the existing export aggregate without partial publication.
- Valid tightly packed inverse-bind data and the identity fallback remain unchanged.

## Acceptance criteria

- An invalid reference, non-FLOAT/MAT4 accessor, insufficient count, offset overflow, or truncated matrix span fails before any matrix read.
- A valid tightly packed inverse-bind accessor exports unchanged; absent inverse-bind data still uses the current identity path.
- DataPacker `Debug|x64` builds pass through `/compile`.

## Coordination

`Documents/Plans/Engine/AnimationAccessorSpanValidation.md` and `Documents/Plans/Engine/AnimationKeyframeCardinalityValidation.md` own animation-specific predicates. Keep inverse-bind validation at the skeleton loader and preserve the common scene export failure channel.

## Notes

Origin: `S002-C012`; disposition selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-002.md` (`S002-C012 — FINAL: RETAIN_HIGH`), source selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-002.md:197`, and consolidated selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:99`. External claim `EXT-009` was narrowed by a VERIFIED/REFUTED final verdict; no source fix or build was performed during routing.
