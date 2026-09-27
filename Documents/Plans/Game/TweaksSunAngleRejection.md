<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T17:49:16.794Z","dependsOn":[]} -->
# Fix: Reject an invalid persisted Tweaks sun angle instead of fixing it

## Context

`LoadTweaksSettings` (`Projects/BrokenEngineSandbox/Source/ClientSettings.cpp:41-67`)
still fixes a bad `fSunAngle` read from `TweaksSettings.bin`. A non-finite
value is replaced with `engine::gSunAngleOverride.ResetToDefault()` (L54-61,
landed by commit `e38ff63d`), and a finite value outside the wrapper range
`[0, XM_2PI]` (`Engine/Source/Ui/GraphicsSettingsWrappersBase.cpp:25`) is
clamped by `Wrapper::Set` (`Engine/Source/Ui/WrapperBase.h`, `Set(float)` uses
`std::clamp`). Both paths also apply `bShowImGui` and the Tweaks section layout
before the sun angle is looked at, so a bad file is partly applied.

This violates `.agents/references/cpp-conventions.md` "Never fix a bad value
automatically": a non-finite or out-of-range value read from a file must fail
the load, never be clamped or substituted. The engine settings loaders
(`GameSettings.cpp`, `GraphicsSettings.cpp`, `SoundSettings.cpp`) already follow
it: they check every persisted value before applying any, and on the first
invalid one log one warning naming the file and field and apply nothing
(`Engine/Source/Ui/AGENTS.md` `## Settings Persistence`). That change excluded
the game-owned files in `ClientSettings.cpp`, which leaves this loader as the
remaining settings loader that fixes a bad value.

`LoadTweaksSettings` runs at client startup (`Engine/Source/Main.cpp:324`) and
again after device-loss recreation (`Engine/Source/Graphics/Graphics.cpp:464`);
it is a no-op unless `kbDebugInput`.

## Design

Author's recommendation: after `engine::ReadVersionedFile` succeeds, check
`engine::gSunAngleOverride.IsInRange(settings.fSunAngle)` before applying any
field. `Wrapper::IsInRange` (`Engine/Source/Ui/WrapperBase.h`) already rejects
NaN and infinities as well as finite out-of-range values, so one call replaces
the `std::isfinite` branch. When the check fails, log one `kWarning` naming
`TweaksSettings.bin` and `fSunAngle` and return without touching
`gpGame->mbShowImGui`, the Tweaks section state, or the sun angle, so every
Tweaks setting keeps its current value, as when the file is missing. When it
passes, apply the three fields as today, setting the angle through
`gSunAngleOverride.Set`, which keeps the wrapper's normal snapping. Remove the
`ResetToDefault` substitution and the comment explaining it. Keep the
existing read-failure warning, the `TweaksSettings` layout, and `kiVersion`
unchanged.

Rationale: reusing `IsInRange` matches the engine loaders exactly and needs no
new helper; checking first and applying second is what makes the rejection
whole-file.

## Critical files

- `Projects/BrokenEngineSandbox/Source/ClientSettings.cpp:41-67` —
  `LoadTweaksSettings`.
- `Engine/Source/Ui/WrapperBase.h` — `Wrapper::IsInRange`, `Wrapper::Set(float)`.
- `Engine/Source/Ui/GraphicsSettingsWrappersBase.cpp:25` — `gSunAngleOverride`
  range.
- `Engine/Source/Ui/GameSettings.cpp` — precedent for the check-then-apply
  shape and warning wording.
- `Projects/BrokenEngineSandbox/Source/AGENTS.md` `## Architecture` — the
  game-owned persisted settings bullet.

## In scope

- `game::LoadTweaksSettings`: the pre-apply `IsInRange` check on
  `fSunAngle`, the single rejection warning, the early return that applies
  nothing, and removal of the non-finite `ResetToDefault` branch and its
  comment.
- The `Projects/BrokenEngineSandbox/Source/AGENTS.md` bullet on
  `TweaksSettings.bin`, only if needed to state that an invalid sun angle
  rejects the whole file.

## Out of scope

- `TweaksSettings::bShowImGui` validation and representation, owned by
  `Documents/Plans/Engine/TweaksSettingsBoolRepresentation.md`.
- `ClientState.bin` / `LoadClientState` and `Camera::RestoreEyeHeight`, owned
  by `Documents/Plans/Engine/PersistedCameraEyeHeightNormalization.md`.
- `TweakSectionState` contents and `TweaksScreenBase::LoadState`.
- `SaveTweaksSettings`, the `TweaksSettings` layout, `kiVersion`, `Wrapper`
  itself, the engine settings loaders, server code, and unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2. Trigger: scoped behavior change in one
game-owned loader — a check tightened inside one unit at an existing file
boundary, with the file format and trust unchanged (`.agents/references/risk-tiers.md`).

Preserve these invariants:

- A `TweaksSettings.bin` whose `fSunAngle` is non-finite or outside
  `[0, XM_2PI]` applies no field; the live ImGui flag, section layout, and sun
  angle keep the values they had before the load.
- An in-range file applies all three fields exactly as today.
- No simulation CRC, wire, save, replay, or `.pack` change; the sun-angle
  override is client presentation state.

## Acceptance criteria

- A version/size-valid `TweaksSettings.bin` with `fSunAngle` of NaN, +inf,
  -inf, `-0.5`, or `7.0` logs one `kWarning` naming the file and `fSunAngle`,
  and startup shows the default sun angle, ImGui flag, and Tweaks layout.
- A file with an in-range `fSunAngle` loads it, the ImGui flag, and the layout
  as before.
- `LoadTweaksSettings` contains no `ResetToDefault`, `std::isfinite`, or
  clamp-reliant path for the sun angle.
- Client Debug builds pass `/compile`.

## Notes

`Documents/Plans/Engine/TweaksSettingsBoolRepresentation.md` edits the same
function; its rejection of an invalid flag byte also leaves live state
unchanged, so the two Plans compose in either order into one pre-apply check.
