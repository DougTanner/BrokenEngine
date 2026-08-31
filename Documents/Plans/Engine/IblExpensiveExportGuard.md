<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T01:58:31.350Z","dependsOn":[]} -->
# Honor the expensive-export guard for IBL convolution

## Context

Final survivor `S002-C018` is a promoted HIGH DataPacker guard finding. `MainThread` calls both IBL convolution passes unconditionally before ordinary exports, while the `BT_DATAPACKER_FORBID_EXPENSIVE_EXPORT=1` guard is consulted by ordinary texture/RDO encoding only (`DataPacker/Source/Main.cpp:707-715`; `DataPacker/Source/ExportJobs/ExportCubemapIbl.cpp:239-323,530-563`). A guarded verification run with dirty cube inputs still performs expensive CMFT convolution and writes outputs.

The final disposition is in `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-002.md` under `S002-C018 — FINAL: PROMOTE_HIGH`; the source report is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-002.md:269` and the consolidated selector is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:105`. The target remained at baseline `a20acf1e31a24a0f61ae638e8976602b49655788`.

## Design

The author's recommendation is to include dirty irradiance and prefiltered IBL convolution in the existing expensive-export boundary. Before each dirty convolution pass begins, check `mbForbidExpensiveExport` and fail through the existing guarded-run path; clean cached outputs remain readable and ordinary unguarded runs retain current generation and cleanup behavior. Do not add a second environment variable or change the IBL output format.

## Critical files

- `DataPacker/Source/Main.cpp:701-715` — manager construction and IBL invocation.
- `DataPacker/Source/ExportJobs/ExportCubemapIbl.cpp:239-323,530-563` — dirty convolution passes.
- `DataPacker/Source/ExportJobs/Texture/Texture.cpp:387-392` — existing expensive-export guard pattern.
- `DataPacker/Source/AGENTS.md` and `DataPacker/Source/ExportJobs/AGENTS.md` — guarded export contract.

## In scope

- Applying the existing expensive-export guard before dirty irradiance and prefiltered IBL computation.
- Preserving clean-cache reads, dirty markers, failure cleanup, and normal unguarded IBL output.

## Out of scope

- CMFT algorithms, IBL dimensions, cache fingerprints, output layout, ordinary texture/RDO guard behavior, or Gaea export.
- New guard variables, compatibility modes, or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (scoped DataPacker tool behavior). Trigger: an existing verification guard currently misses one offline expensive-export stage; the correction is a local pre-computation check with unchanged output format and valid unguarded behavior.

Preserve these invariants:

- A forbid-expensive run never begins dirty IBL convolution or writes its outputs.
- Clean IBL cache state remains readable under the guard.
- Unguarded valid IBL generation, dirty markers, and output publication remain unchanged.

## Acceptance criteria

- With `BT_DATAPACKER_FORBID_EXPENSIVE_EXPORT=1` and dirty IBL inputs, both convolution passes stop before CMFT work or output mutation.
- With clean IBL outputs, the guarded run completes without recomputation; without the guard, valid dirty inputs still generate the same outputs.
- DataPacker `Debug|x64` builds pass through `/compile`.

## Notes

Origin: `S002-C018`; disposition selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-002.md` (`S002-C018 — FINAL: PROMOTE_HIGH`), source selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-002.md:269`, and consolidated selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:105`. No exact existing Plan was found. No source fix or build was performed during routing.
