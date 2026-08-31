<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T01:59:52.643Z","dependsOn":[]} -->
# Reject non-finite persisted graphics settings before use

## Context

Final survivor `S013-C008` is a retained HIGH settings trust-boundary finding. `LoadGraphicsSettings` sends `fMaxAnisotropy`, `fMinSampleShading`, `fMipLodBias`, `fSmokeSimulationArea`, and `fMinimumAmbient` directly to float wrappers; only lighting cadence has a finite branch. `Wrapper::Set` uses comparison-based `std::clamp`, so NaN survives into sampler/pipeline creation or render uniforms (`Engine/Source/Ui/GraphicsSettings.cpp:126-132`; `Engine/Source/Ui/WrapperBase.h:150-153`).

The final disposition is in `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-018.md` under `S013-C008 — FINAL: RETAIN_HIGH`; the source report is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-013.md:152` and the consolidated selector is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:236`. The target remained at baseline `a20acf1e31a24a0f61ae638e8976602b49655788`.

## Design

The author's recommendation is to validate all five persisted floats for finiteness at `LoadGraphicsSettings` and reset each invalid field to its existing wrapper default before Graphics construction or render publication. Preserve finite range snapping/clamping, settings versions, valid graphics quality values, and current downstream sampler/pipeline/render consumers.

## Critical files

- `Engine/Source/Ui/GraphicsSettings.cpp:113-140` — persisted graphics settings adoption.
- `Engine/Source/Ui/WrapperBase.h:142-153` — float wrapper behavior.
- `Engine/Source/Graphics/Managers/TextureManager.cpp:389-405` — sampler consumers.
- `Engine/Source/Graphics/Objects/PipelineCreator.cpp:450-452` — pipeline consumer.
- `Engine/Source/Graphics/Render/SmokeUniforms.cpp:49-56` — smoke render consumer.
- `Engine/Source/Graphics/Render/GlobalUniforms.cpp:80-92` — global render consumer.
- `Engine/Source/Ui/AGENTS.md` — opaque settings contract.

## In scope

- Finite-value admission/default reset for the five named persisted graphics floats.
- Existing settings-load failure/default behavior before resource creation and render use.
- Valid snapping, capability handling, sampler/pipeline creation, and uniform publication.

## Out of scope

- Sample-count enum validation, Sound/Tweaks settings, wrapper implementation redesign, Vulkan capability policy, or shader changes.
- Graphics resource recreation policy, settings format/version changes, or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (scoped Graphics settings behavior). Trigger: opaque persisted floats enter renderer resource creation and client-only render data, but the correction is a local load-boundary finite check with unchanged settings layout and valid behavior.

Preserve these invariants:

- Graphics resource and render consumers receive finite values for every persisted field.
- Invalid settings fall back before first resource creation; valid values retain current snap/range behavior.
- Settings file layout/version and client/server shared defaults remain unchanged.

## Acceptance criteria

- A current-format settings file containing NaN in each affected field resets that field to its finite default before `Graphics::Create` or render population.
- Finite in-range and out-of-range values retain current snapping/clamping and valid resource behavior.
- Client Debug and Release builds pass through `/compile`; a settings-load scenario shows no invalid Vulkan/render value.

## Notes

Origin: `S013-C008`; disposition selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-018.md` (`S013-C008 — FINAL: RETAIN_HIGH`), source selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-013.md:152`, and consolidated selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:236`. External claims `EXT-057` and `EXT-059` were VERIFIED. No exact existing Plan was found. No source fix or build was performed during routing.
