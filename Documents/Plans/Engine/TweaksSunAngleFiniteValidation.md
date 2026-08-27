<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:40:55.463Z","dependsOn":[]} -->
# Fix: Reject non-finite persisted Tweaks sun angles

## Context

The accepted survivor `CAI/shard-0045/003` shows that
`LoadTweaksSettings` passes a raw persisted `fSunAngle` directly to
`gSunAngleOverride.Set` (`Projects/BrokenEngineSandbox/Source/ClientSettings.cpp:41-54`).
`Wrapper::Set(float)` clamps finite values but does not repair NaN
(`Engine/Source/Ui/WrapperBase.h:132-135`), and the debug ImGui camera path
then emits the override into global lighting, shadow, and water uniforms
(`Projects/BrokenEngineSandbox/Source/Graphics/Camera.cpp:83-95`;
`Engine/Source/Graphics/Render/GlobalUniforms.cpp:56-72,486-510`).

The source report is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0045.md:74`;
the consolidated selector is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:1099`.
All 11 frozen target rows matched baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; no source was edited in this
routing session.

Impact: a version/size-valid local debug settings file can publish non-finite
sun direction and lighting parameters until the setting is manually reset.

## Design

Author's recommendation: finite-check `settings.fSunAngle` at the load
boundary.  Apply it through the existing wrapper only when finite; otherwise
call `gSunAngleOverride.ResetToDefault()` so the live override is a valid
finite default and is not exposed to downstream render math.  Keep the
wrapper's existing finite-range clamping for finite out-of-range values and do
not change the persisted field/version.

## Critical files

- `Projects/BrokenEngineSandbox/Source/ClientSettings.cpp:41-59` — persisted
  Tweaks load and override assignment.
- `Engine/Source/Ui/WrapperBase.h:132-149` — wrapper float-set behavior.
- `Projects/BrokenEngineSandbox/Source/Graphics/Camera.cpp:82-96` — debug
  override consumer.
- `Engine/Source/Graphics/Render/GlobalUniforms.cpp:56-72,480-510` — lighting,
  shadow, and water uniform publication.
- `Engine/Source/Ui/GameSettings.cpp:60-75` — repository finite-load precedent.

## In scope

- Rejecting non-finite `fSunAngle` values before they reach
  `gSunAngleOverride.Set` and resetting the override to its default.
- Preserving finite values, wrapper range clamping, debug ImGui activation, and
  the existing raw settings layout/version.
- Updating only local warning/comment text needed to describe the fallback.

## Out of scope

- Sun-angle range/tuning policy for finite values, lighting shader math, camera
  selection, or the separate persisted bool-representation finding.
- Version migration, a new settings schema, server code, simulation CRC,
  wire/save/replay formats, and unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3). Trigger: opaque
persisted float input crosses game/UI wrapper and Graphics uniform publication
boundaries.

Tier rationale: the fix is one finite check at one named load boundary that
falls back to the existing `ResetToDefault()` path. No persisted field, version,
wrapper behavior, or uniform layout changes, and finite values keep exactly
their current handling.

Preserve these invariants:

- `Camera::SunAngle()` returns a finite value whenever debug ImGui uses the
  persisted override.
- Finite in-range and finite out-of-range values retain current wrapper behavior;
  only non-finite input takes the default path.
- Global sun direction, lighting, water, and shadow uniforms never receive NaN
  or infinity from this settings load.
- No simulation CRC, wire, save, replay, or `.pack` representation changes.

## Acceptance criteria

- A version/size-valid Tweaks file with NaN or infinity resets the sun override
  to its finite default before the first debug render, with no non-finite global
  uniforms.
- Finite out-of-range angles still clamp through `Wrapper::Set`, and finite
  in-range values restore exactly.
- Client Debug and Release builds pass `/compile`; a debug ImGui harness load
  and render scenario has no NaN/inf shader inputs or validation error.
- A scoped search confirms the raw settings float is finite-checked before its
  wrapper assignment.

## Notes

The report mapped this to repository-local finite-load behavior, so no external
API verification or compatibility version bump is required.
