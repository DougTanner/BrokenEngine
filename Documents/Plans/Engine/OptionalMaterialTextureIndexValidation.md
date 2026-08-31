<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T01:59:29.109Z","dependsOn":[]} -->
# Allow absent material textures while validating present indices

## Context

Final survivor `S010-C009` is a promoted HIGH Graphics/data compatibility finding. `MaterialShaderData` initializes texture indices to zero while matching texture-set selectors are `-1`; the exporter permits a scene with zero textures. `WriteModelDescriptor` nevertheless checks all five indices unconditionally against `uiTextureCount`, so a valid untextured material fails when `0 >= 0` before shader optional-set guards can ignore the fields (`Engine/Source/Graphics/Objects/PipelineDescriptorWriter.cpp:143-156`; `Common/DataFile.h:248-270`).

The final disposition is in `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-013.md` under `S010-C009 — FINAL: PROMOTE_HIGH`; the source report is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-010.md:192` and the consolidated selector is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:205`. The target remained at baseline `a20acf1e31a24a0f61ae638e8976602b49655788`.

## Design

The author's recommendation is to validate and resolve each texture index only when its matching material texture-set selector is present (`>= 0`); leave absent selectors at their ignored/safe default and rely on the existing shader guards. Preserve current rejection for an out-of-range index on a present texture, zero-texture material support, descriptor layout, and valid textured output.

## Critical files

- `Engine/Source/Graphics/Objects/PipelineDescriptorWriter.cpp:143-156` — material index validation/publication.
- `Common/DataFile.h:248-270` — optional texture-set/index representation.
- `DataPacker/Source/ExportJobs/ExportScene.cpp:774-815,866-923` — zero-texture exporter behavior (read-only producer evidence).
- `Engine/Source/Graphics/Objects/AGENTS.md` — pack-derived material index contract.

## In scope

- Conditional validation/resolution of the five material texture indices based on their matching texture-set selectors.
- Valid zero-texture material pipeline creation and present-index rejection.
- Existing descriptor and shader optional-texture behavior.

## Out of scope

- Material layout/version, texture-set encoding, shader changes, texture loading, or adding a default texture asset.
- Changing textured-material index semantics, descriptor binding order, or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (scoped Graphics descriptor validation). Trigger: one runtime descriptor writer rejects a valid optional-texture representation at an existing pack boundary; the correction preserves layout, shader guards, and valid textured data.

Preserve these invariants:

- A missing texture set never indexes the texture array or rejects an otherwise valid zero-texture material.
- Every present texture index remains in range before descriptor publication.
- Existing material ordering, descriptors, shader guards, and textured scene rendering remain unchanged.

## Acceptance criteria

- A one-material scene with zero glTF textures creates its model pipeline and renders without an out-of-range lookup.
- A material with a present texture and an out-of-range index is still rejected before descriptor publication.
- Client Debug and Release builds pass through `/compile`; a zero-texture scene scenario completes without a corrupt-pack error.

## Notes

Origin: `S010-C009`; disposition selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-013.md` (`S010-C009 — FINAL: PROMOTE_HIGH`), source selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-010.md:192`, and consolidated selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:205`. No exact existing Plan was found. No source fix or build was performed during routing.
