<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:30:56.443Z","dependsOn":[]} -->
# Prevent skin-joint count wrap at the serialized boundary

## Context

The frozen audit retained `CAI/shard-0006/003`. `BuildMaterialInfos` logs when
the first skin exceeds the shader limit but casts its count directly to
`uint8_t` (`DataPacker/Source/ExportJobs/ExportScene.cpp:490-507`). The declared
skeleton limit is 256 (`Common/DataFile.h:185-192`), so exactly 256 wraps to
zero and disables runtime skinning, which only enters its joint path for a
nonzero count (`Engine/Source/Graphics/AnimationData.cpp:419-467`). Baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5` has no source changes.

## Design

Clamp the material joint count to `common::kiMaxJointsPerMesh` before narrowing
to the serialized byte, retaining the existing warning for an oversized skin.
Keep the full skeleton bound and runtime's own clamp; an accepted skinned
material must never become non-skinned solely because its source count is 256.

## Critical files

- `DataPacker/Source/ExportJobs/ExportScene.cpp` — material count narrowing.
- `Common/DataFile.h` — skeleton/material limits.
- `Engine/Source/Graphics/AnimationData.cpp` — consumer semantics.

## In scope

- `BuildMaterialInfos` count clamp and warning/serialization pairing.
- The runtime/shader-facing count contract needed to preserve skinning.

## Out of scope

- Increasing shader joint capacity, changing skeleton serialization, or multi-skin identity.
- General material splitting or animation evaluation.

## Risk tier and invariants

Tier 2 (downgraded from Tier 3). Trigger: serialized material data and CPU/GPU
skinning contracts are affected. Counts above the shader maximum clamp
predictably but remain nonzero; valid counts and bind-pose behavior remain
unchanged.

Tier rationale: the fix is a single pre-specified clamp before an existing
narrowing conversion in one function, with no change to the serialized field,
its width, or any value at or below the supported maximum.

## Acceptance criteria

- A 256-joint accepted scene serializes a nonzero material count capped at the shader maximum and renders through the skinned path.
- Counts at and below the supported maximum are unchanged.
- No narrowing conversion can wrap a positive accepted skin count to zero.

## Notes

Origin: `CAI/shard-0006/003`, source selector
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0006.md:88`.
No source fix, build, or harness was performed here.
