<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-23T20:51:07.451Z","dependsOn":[]} -->
# Prototype A — instantaneous compression control

Executable scheduler Plan; runtime execution approval pending. Research baseline `5f205af6a24a17ebf732f7005f03a48e0e6f91e0`. Execute in a dedicated preserved prototype worktree and branch, separate from B. Saving this Plan authorizes no runtime edits, builds, captures, or worktree setup now.

## Context

Build the cheap control that exposes current Jacobian compression with no persistence. It deliberately does not satisfy the user's lingering-foam requirement and cannot become the production winner without new approval. The [shared overview](../../Investigations/Graphics/WaterFoamPrototypes.md) owns shading, source thresholds, altitude preset, research, and comparison conditions; include those contracts in execution review.

## Design

Fetch normal RGBA once in Water.vert where `.xyz` is already fetched (`Water.vert:60-68`), pass J in a scalar varying;
over-land and Low quality publish J=1. In Water.frag apply the compression source defined in the shared overview, then
multiply by an emission enable/strength control. Gate land through existing fragment terrain rejection. Apply shared
noise/albedo/diffuse/specular composition from comparison `## Shared source and shading rules`. New parameters are
Global uniform values populated once by WaterUniforms; thresholds' reciprocal range is CPU-computed. No elapsed-time
variable or history resource is added. Thresholding interpolated J loses unresolved native-grid events; retain this as
the control's documented limitation, with no anti-aliasing correctness claim.

Use the [altitude comparison preset](../../Investigations/Graphics/WaterFoamPrototypes.md#altitude-and-fair-setup), with the same geometry settings as B. Keep default settings unchanged. The extended-band preset and measured native-grid limitations are Plan-authored design choices, not choices explicitly made by the user. No temporal or distance fade is added to A.

## Critical files

- `Engine/Data/Shaders/Water/Water.vert:60` — existing normal fetch/varyings.
- `Engine/Data/Shaders/Water/Water.frag:106` — prelighting color and specular composition.
- `Engine/Data/Shaders/ShaderGlobalLayout.h:6` — owning shared uniform struct (not ShaderLayoutsBase.h alone).
- `Engine/Source/Graphics/Render/WaterUniforms.cpp:153` — Global water parameter population.
- `Engine/Source/Ui/WaterWrappersBase.cpp:79` — bounded fade controls.

## In scope

- Engine/Data/Shaders/Water/Water.vert normal fetch and one J varying; matching Engine/Data/Shaders/Water/Water.frag input/source/composition regions.
- Engine/Data/Shaders/ShaderGlobalLayout.h water-only foam parameters; Engine/Source/Graphics/Render/WaterUniforms.cpp corresponding publication.
- Engine/Source/Ui/WaterWrappersBase.h and Engine/Source/Ui/WaterWrappersBase.cpp existing water declarations plus a minimal foam control group; Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenWater.cpp matching controls; four existing fade wrappers' maxima only.
- Required shader/render AGENTS contract updates only where the approved experiment changes an owning contract.
- Shared comparison captures and recording this control's limitations, following the fixed comparison artifact.
- `Documents/Investigations/Graphics/WaterFoamPrototypeAResults.md` result report and normal Plan-completion bookkeeping; experimental source stays on its preserved prototype branch/worktree.

## Out of scope

History, decay, new render targets, new compute shaders, changing J meaning or its normal W storage, CPU wave arrays, transport/advection, foam normals, distance fade, independent source resolution, FFT replacement, fluid solver, shoreline emitter, wakes, simulation/CRC/network/save/replay changes, production defaults, new unit tests, and landing as the production water implementation.

## Invariants and risk

Tier3 runtime: new fields alter shared CPU/GLSL uniform data layout, consumed by both targets (`Engine/Data/Shaders/AGENTS.md:7-13`). Client visual only; render remains outside CRC (`Engine/Source/Graphics/Render/AGENTS.md:3-5`). Preserve quality bypass and active-grid contracts (`Engine/Data/Shaders/Water/AGENTS.md:3-8`), existing noise precision, and matching shader varyings. Authoring this document is Tier1.

## Acceptance and verification

| Criterion | Evidence required |
|---|---|
| New source visible and controllable | /agent-harness screenshots with S=0 and nonzero S under same sea state; unedited shader has no foam composition at Water.frag:106-137 |
| Baseline remains explicit | At fixed position coverage follows source immediately and has no lingering trail; label A control in saved evidence |
| No false land/Low source | Low quality and over-land captures, plus code inspection of bypass values; no stale texture fetch |
| Shared shading and comparison | Shared matrix, identical preset manifests and no-foam captures before the first edit; no distance fade introduced |
| Safe layout and build | /compile BrokenEngineSandbox client Debug and Release x64, server Release x64, Local shader pack; shader reflection/interface validation and code review |
| Preserved runnable experiment | Dedicated A branch/commit/worktree identity, executable build and captures recorded in the result report; no automatic production replacement or experimental source landing on primary |

## Result and completion

Create `Documents/Investigations/Graphics/WaterFoamPrototypeAResults.md` recording the actual common comparison baseline, prototype branch/commit/worktree identity, exact build commands/results, executable and asset locations, preset manifests, captures, repeated GPU measurements, native-grid/filter limitations, and unresolved outcomes. Preserve the runnable prototype branch and worktree. B is independently executable; compare against its report when available, without requiring B to complete A.

Plan-completion landing includes only the result report and Plan/dependency bookkeeping. Keep experimental runtime source and its contract documentation on the preserved prototype branch/worktree; neither candidate automatically lands on primary as a winner. Future production selection requires separate approval.

Do not claim GPU/visual results before executing. Record source-grid aliasing and baseline cost rather than claiming the control solves them. Through the normal workflow run plan alternatives/simplicity/audit, Tier3 grill, implementation plus affected-code/docs, shader/code/comment/style/coherence reviews as triggered, adversarial review, and independent verification. All build/harness operations use their owning skills.
