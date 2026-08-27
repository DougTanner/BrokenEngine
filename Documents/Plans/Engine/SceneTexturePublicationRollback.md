<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:31:05.786Z","dependsOn":[]} -->
# Make scene texture publication recoverable after a partial move

## Context

The frozen audit retained `CAI/shard-0006/008`. `ProcessTextures` moves each
stage to its final path at `DataPacker/Source/ExportJobs/ExportScene.cpp:337-351`,
but only swaps the completed list into `mPublishedTextureFiles` at `:358`.
On a later move failure, the catch calls `CleanupTextureAttemptFiles` at
`:353-356`, which knows only the remaining stages. A successfully replaced
prefix is therefore untracked and can be paired with old scene/model outputs.
Source bytes match baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`.

## Design

Treat the final-path replacement as an attempt transaction: move each prior
final to an attempt-owned destination-local backup before replacement, record
every successful replacement, and on any later failure restore the complete
prior set from those backups before clearing attempt state. Publish the new
final list to `mPublishedTextureFiles` only after all replacements succeed;
retain existing worker/stage cleanup and all-type failure propagation.

## Critical files

- `DataPacker/Source/ExportJobs/ExportScene.cpp` — texture attempt and rollback.
- `DataPacker/Source/ExportJobs/ExportScene.h` — attempt-owned state if needed.

## In scope

- `ExportScene::ProcessTextures` final-move transaction, rollback bookkeeping, and failure cleanup.
- Preservation of the previously complete texture set across mid-publication failure.

## Out of scope

- Texture encoding, external image dirtying, format inference, or scene/model serialization.
- General DataPacker cache/output cross-volume publication.

## Risk tier and invariants

Tier 3. Trigger: multi-file derived-asset publication and recovery span a
DataPacker trust boundary. A failed attempt must not leave a mixed generation or
destroy the prior recoverable set; a successful attempt publishes all finals.

## Acceptance criteria

- If any final move fails after a successful prefix, the prior complete set is restored from its destination-local backups before failure returns.
- A successful attempt records every final and clears temporary state exactly once.
- Scene/model output never observes a partial texture generation.

## Notes

Origin: `CAI/shard-0006/008`, source selector
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0006.md:178`.
No source fix, build, or harness was performed here.

## Coordination

`Documents/Plans/Engine/SceneExternalImageDirtying.md` and
`Documents/Plans/Engine/SharedTextureFormatInference.md` also touch
`ExportScene.cpp`; keep publication recovery separate from dependency and
format decisions.
