<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T01:58:07.713Z","dependsOn":[]} -->
# Validate animation references and spans before export

## Context

Final survivor `S002-C010` is a retained HIGH glTF-reader finding. `LoadAnimations` indexes samplers, accessors, buffer views, and buffers without validating their references, while `AccessorFloats` forms raw pointers from unchecked offsets before reading values (`DataPacker/Source/ExportJobs/Scene/SceneAnimationLoader.cpp:7-12,172-211`). TinyGLTF parse success and the sparse-accessor guard do not prove final buffer spans. A parser-accepted malformed scene can therefore trigger an out-of-range read before the export aggregate reports failure.

The final disposition is in `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-002.md` under `S002-C010 — FINAL: RETAIN_HIGH`; the source report is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-002.md:173` and the consolidated selector is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:97`. The target was unchanged from `a20acf1e31a24a0f61ae638e8976602b49655788`.

## Design

The author's recommendation is to validate every sampler, input/output accessor, buffer-view, and buffer reference before indexing, then validate accessor offsets, element counts, stride, and last-byte spans against the referenced buffer before `AccessorFloats` or any animation loop runs. Keep the existing sparse rejection and route malformed scenes through the existing export aggregate before serialized animation output is allocated or written. Preserve valid animation output and interpolation semantics.

## Critical files

- `DataPacker/Source/ExportJobs/Scene/SceneAnimationLoader.cpp:7-12,172-211` — reference and pointer reads.
- `DataPacker/Source/ExportJobs/Scene/SceneAnimationLoader.h` — animation loader contract.
- `DataPacker/Source/ExportJobs/ExportScene.cpp` — scene export caller and aggregate failure path.
- `DataPacker/Source/ExportJobs/AGENTS.md` — opaque glTF reader contract.

## In scope

- Reference, offset, stride, count, and final-span checks for animation sampler/accessor reads.
- Failure propagation before unsafe reads or serialized animation publication.
- Existing valid animation interpolation and output layout.

## Out of scope

- Animation keyframe cardinality/time policy, inverse-bind accessors, skeleton identity, or runtime animation validation.
- TinyGLTF changes, animation format/version changes, and unsupported-feature expansion.
- New compatibility readers or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (scoped DataPacker scene-reader behavior). Trigger: opaque glTF references cross a serialized animation output boundary, but the correction is confined to one existing reader and leaves valid output, format, and threading unchanged.

Preserve these invariants:

- Every accepted animation read references existing objects and an in-bounds accessor span before pointer formation.
- Invalid scene input fails through the export aggregate without partial animation publication.
- Valid sampler/accessor data retains its current serialized values and interpolation behavior.

## Acceptance criteria

- A parser-accepted scene with an invalid sampler/accessor/buffer reference, offset, stride, or final span fails before the first unsafe read.
- A valid scene with ordinary and strided accessors exports byte-identical animation data.
- DataPacker `Debug|x64` builds pass through `/compile`.

## Coordination

`Documents/Plans/Engine/AnimationKeyframeCardinalityValidation.md` owns input/output element and time relationships, while `Documents/Plans/Engine/InverseBindAccessorValidation.md` owns skeleton inverse-bind shape. Keep all three predicates at their owning reads and preserve the existing export aggregate.

## Notes

Origin: `S002-C010`; disposition selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-002.md` (`S002-C010 — FINAL: RETAIN_HIGH`), source selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-002.md:173`, and consolidated selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:97`. No exact existing Plan was found. No source fix or build was performed during routing.
