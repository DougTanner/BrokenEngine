<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:31:00.195Z","dependsOn":[]} -->
# Choose a shared texture format from all material uses

## Context

The frozen audit retained `CAI/shard-0006/005`. `ComputeTextureFormats` stops
at the first occlusion use and assigns BC4 before scanning later materials
(`DataPacker/Source/ExportJobs/ExportScene.cpp:98-118`). The same image can be
referenced as a later normal map (`:793-805`), while the runtime samples normal
`.rg` channels (`Engine/Data/Shaders/Model/Model.frag:168-171`). No source bytes
changed from baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`.

## Design

Scan every material use for each image before selecting one generated format:
choose BC5 if any normal use exists, otherwise BC4 for occlusion-only use, and
otherwise BC7. Keep the existing one-source/format intermediate, suffix/CRC
routing, and compatible single-use choices.

## Critical files

- `DataPacker/Source/ExportJobs/ExportScene.cpp` — format inference and texture references.
- `DataPacker/Source/ExportJobs/ExportTexture.cpp` — format encoding.
- `Engine/Data/Shaders/Model/Model.frag` — normal-channel consumer.

## In scope

- `ComputeTextureFormats`' all-use scan and strongest-compatible format choice.
- The existing generated intermediate and CRC references required by that choice.

## Out of scope

- New texture formats, shader reconstruction, duplicate source-image policy, or mip generation.
- External image dirtying and publication rollback.

## Risk tier and invariants

Tier 2 (downgraded from Tier 3). Trigger: texture representation is serialized
by DataPacker and consumed by a GPU shader. Any normal use must preserve its
required channels; occlusion-only and ordinary uses retain their current valid
encodings.

Tier rationale: the fix is a fully specified scan-then-choose change inside one
function of one offline tool subsystem, with no new format, shader, or
intermediate/CRC routing work. Single-use images keep exactly their current
encodings, so only the already-incorrect multi-use case changes.

## Acceptance criteria

- An image used first for occlusion and later for normal is emitted as BC5 and the normal material reads both channels correctly.
- Occlusion-only images remain BC4; ordinary images remain BC7.
- All generated texture CRC references still name the selected intermediate.

## Notes

Origin: `CAI/shard-0006/005`, source selector
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0006.md:124`.
No source fix, build, or GPU run was performed during routing.

## Coordination

`Documents/Plans/Engine/SceneExternalImageDirtying.md` and
`Documents/Plans/Engine/SceneTexturePublicationRollback.md` also touch
`ExportScene.cpp`; keep format inference, dependency dirtying, and publication
transaction boundaries separate.
