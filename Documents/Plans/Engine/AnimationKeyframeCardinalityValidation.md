<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T01:58:11.075Z","dependsOn":[]} -->
# Validate animation keyframe cardinality and times

## Context

Final survivor `S002-C011` is a retained HIGH glTF animation-contract finding. `LoadAnimations` drives reads and serialized channel counts from the input accessor alone, ignores output accessor cardinality/element shape, and accepts duplicate, decreasing, or non-finite input times (`DataPacker/Source/ExportJobs/Scene/SceneAnimationLoader.cpp:187-211`). The final locator verified that glTF requires target-compatible output elements, strictly increasing input times, and three output elements per input key for `CUBICSPLINE`. Runtime zero-keyframe rejection does not protect the producer-side reads or time interpolation.

The final disposition is in `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-002.md` under `S002-C011 — FINAL: RETAIN_HIGH`; the source report is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-002.md:185` and the consolidated selector is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:98`. The target remained at baseline `a20acf1e31a24a0f61ae638e8976602b49655788`.

## Design

The author's recommendation is to validate finite strictly increasing input times and require output accessor element counts and target-path element shapes to match the input. Require one output element per key for step/linear interpolation and exactly three per key for `CUBICSPLINE`; reject the animation through the existing export aggregate before reading or serializing values. Preserve valid interpolation, key ordering, and current output layout.

## Critical files

- `DataPacker/Source/ExportJobs/Scene/SceneAnimationLoader.cpp:187-211` — sampler cardinality/time handling.
- `DataPacker/Source/ExportJobs/Scene/SceneAnimationLoader.h` — animation output contract.
- `DataPacker/Source/ExportJobs/ExportScene.cpp` — export aggregate caller.
- `DataPacker/Source/ExportJobs/AGENTS.md` — opaque glTF validation and serialization rules.

## In scope

- Input-time finiteness/order checks and sampler input/output element-cardinality checks.
- Target-path-compatible output shape and cubic triplet validation before reads/publication.
- Existing failure aggregation and valid animation output behavior.

## Out of scope

- Object-reference, buffer-span, stride, or inverse-bind validation owned by sibling Plans.
- Changing interpolation formulas, keyframe encoding, animation layout/version, or runtime evaluator policy.
- New compatibility modes or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (scoped DataPacker scene-reader behavior). Trigger: opaque glTF animation values cross an existing serialized output boundary; the correction is a local admission predicate with unchanged valid layout and no threading or protocol change.

Preserve these invariants:

- Accepted samplers have finite strictly increasing times and target-compatible output cardinality.
- Malformed keyframe data fails before producer-side reads or serialized output publication.
- Valid step, linear, and cubic animations retain current values, ordering, and format.

## Acceptance criteria

- A scene with duplicate/decreasing/non-finite times, wrong output element count, or an incomplete cubic triplet is rejected before animation reads/serialization.
- Valid step/linear/cubic channels export and evaluate with unchanged bytes and timing.
- DataPacker `Debug|x64` builds pass through `/compile`.

## Coordination

`Documents/Plans/Engine/AnimationAccessorSpanValidation.md` owns reference/span checks, and `Documents/Plans/Engine/InverseBindAccessorValidation.md` owns inverse-bind shape. Keep cardinality/time validation separate while sharing the existing animation export failure path.

## Notes

Origin: `S002-C011`; disposition selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-002.md` (`S002-C011 — FINAL: RETAIN_HIGH`), source selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-002.md:185`, and consolidated selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:98`. External claim `EXT-008` was VERIFIED in the final disposition. No source fix or build was performed during routing.
