<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T15:48:10.861Z","dependsOn":[]} -->
# Reject bad values in the engine settings files instead of fixing them

## Context

The `.agents/references/cpp-conventions.md` bullet "Never fix a bad value
automatically" says a value read from a network packet or a file that a check
finds non-finite, out of range, or otherwise invalid rejects the input and is
never clamped, substituted, or normalized. The user decided that bad values from
any external or trust-boundary input are rejected, never fixed. The client
settings files were left out of the change that added that bullet; they still
fix bad values on load (line numbers at `46241779`, re-derive at claim):

- `Engine/Source/Ui/GameSettings.cpp:59` — a stored language index outside
  `[0, kLanguageCount)` becomes `Language::kEnglish`.
- `Engine/Source/Ui/GameSettings.cpp:60-76` — a non-finite UI font scale or
  opacity is replaced by the wrapper default; a finite out-of-range value is
  clamped by `Wrapper::Set` (`Engine/Source/Ui/WrapperBase.h:152`,
  `std::clamp(Snap(...), mfMin, mfMax)`).
- `Engine/Source/Ui/GraphicsSettings.cpp:104-114` (`LoadFiniteGraphicsSetting`)
  and `:145-152` (lighting update cadence) — the same default substitution and
  `Wrapper::Set` clamp for the six persisted graphics floats.
- `Engine/Source/Ui/GraphicsSettings.cpp:116-121` (`LoadGraphicsQualityLevel`)
  — a quality-level byte past the last level is clamped to the last level.
- `Engine/Source/Ui/SoundSettings.cpp:52-54` — the three persisted volumes go
  through the same clamping `Wrapper::Set` with no check.

Documentation describing those fixes contradicts the new rule until this lands:
`Engine/Source/Ui/AGENTS.md` `## Graphics Quality Levels` ("clamp each level
before indexing its table") and `## Settings Persistence` (the non-finite reset
bullet and the English fallback in the `GameSettings.cpp` bullet), and
`Projects/BrokenEngineSandbox/Source/Ui/AGENTS.md` `## Localization` (a stored
index outside the enum "falls back to English").

What rejection means for a local settings file is set by the existing reject
path of these same loaders: when `ReadVersionedFile` fails (bad header, wrong
version, short read) the loader applies no field and every setting keeps its
default, which `Engine/Source/Ui/AGENTS.md` `## Settings Persistence` already
documents for an old `SoundSettings.bin` ("intentionally rejected and settings
fall back to defaults") and an old `GraphicsSettings.bin` ("reset with the
other graphics settings").

## Design

Author's recommendation, following the loaders' existing reject path:

1. Each of `LoadGameSettings`, `LoadGraphicsSettings`, and `LoadSoundSettings`
   validates every covered field of the read struct before applying any field.
   One invalid field rejects the whole file: the loader logs one `kWarning`
   naming the file and the first invalid field, applies nothing, and continues
   exactly as its failed-read branch does today (`LoadGraphicsSettings` still
   calls `ApplyAllGraphicsQualityLevels()` and returns `false`). No throw: these
   loaders are not inside a catch chain and reject through the same `bool`
   flow `ReadVersionedFile` uses, the owning boundary's failure path the
   `.agents/references/cpp-conventions.md` "Never fix a bad value
   automatically" bullet requires.
2. Covered checks: language index in `[0, kLanguageCount)`; each persisted
   float finite and within its wrapper's `GetMin()`..`GetMax()`; each quality
   level byte below `GraphicsQualityLevel::kCount`. Accepted floats keep going
   through `Wrapper::Set`; its step snap is left as is because the game writes
   only values `Set` already snapped, and an exact on-step float comparison
   could reject files the game itself wrote.
3. Put the float check in one place used by all three loaders — a `Wrapper`
   query such as `bool IsInRange(float fValue) const` returning
   `std::isfinite(fValue) && fValue >= mfMin && fValue <= mfMax` — and delete
   `LoadFiniteGraphicsSetting` and the clamp in `LoadGraphicsQualityLevel`
   (quality levels still load with `Reset<int64_t>`, now unclamped after the
   check).
4. Update the documentation and code comments that describe the removed fixes
   (see `## In scope`).

No file layout changes, so no `kiVersion` bump.

## Critical files

- `Engine/Source/Ui/GameSettings.cpp` — `LoadGameSettings`.
- `Engine/Source/Ui/GraphicsSettings.cpp` — `LoadGraphicsSettings`,
  `LoadFiniteGraphicsSetting`, `LoadGraphicsQualityLevel`.
- `Engine/Source/Ui/SoundSettings.cpp` — `LoadSoundSettings`.
- `Engine/Source/Ui/WrapperBase.h` — the range query.
- `Engine/Source/Ui/AGENTS.md`, `Projects/BrokenEngineSandbox/Source/Ui/AGENTS.md`.

## In scope

- `LoadGameSettings`: validate `iLanguage`, `fUiFontScale`, `fUiOpacity`
  before applying; remove the English fallback and the non-finite default
  substitution.
- `LoadGraphicsSettings`: validate `fMaxAnisotropy`, `fMinSampleShading`,
  `fMipLodBias`, `fSmokeSimulationArea`, `fMinimumAmbient`,
  `fLightingUpdateCadence`, and the five quality-level bytes before applying;
  remove `LoadFiniteGraphicsSetting` and the `std::min` clamp in
  `LoadGraphicsQualityLevel`.
- `LoadSoundSettings`: validate the three volumes before applying.
- One `Wrapper` range query in `Engine/Source/Ui/WrapperBase.h`.
- The comments at `GameSettings.cpp:58` and `:60` and `GraphicsSettings.cpp:116`
  that describe the removed fixes.
- `Engine/Source/Ui/AGENTS.md` `## Graphics Quality Levels` (the clamp bullet)
  and `## Settings Persistence` (the non-finite reset bullet and the English
  fallback clause), and `Projects/BrokenEngineSandbox/Source/Ui/AGENTS.md`
  `## Localization` (the English fallback clause): each states that a file with
  an invalid value is rejected whole and every setting it holds stays at its
  default.

## Out of scope

- `eSampleCount` admission: `Documents/Plans/Engine/MultisampleCountCapabilityValidation.md`
  owns it (see `## Notes`).
- `ePresentMode`, `eUiTheme`, the bool bytes (`uiOpaqueUi`,
  `uiMuteInBackground`), and `GraphicsSettingsFlags` bits: no check exists for
  them today and none is added here.
- The device-capability clamps applied after load (`InstanceManager`,
  `Graphics::Refresh`) — not file reads.
- Game-owned settings files in `Projects/BrokenEngineSandbox/Source/ClientSettings.cpp`
  (Tweaks, client state).
- `Wrapper::Set` and `Wrapper::GetIndex` behavior for UI and code callers.
- Rewriting or deleting the rejected file on disk; it is replaced on the next
  save.
- Any settings layout, version, or new settings format.

## Risk tier and invariants

Expected Change Workflow Tier 2. Trigger: tightening checks inside one unit
(engine UI settings persistence) at an existing file-read boundary with the
format and trust unchanged (`.agents/references/risk-tiers.md`). Client-only
(`BT_CLIENT`); no simulation CRC, wire, save, replay, or `.pack` exposure.

Invariants:

- A settings file the game itself wrote loads exactly as before.
- A rejected file leaves every setting of that file at its default, the same
  state as a missing file; `LoadGraphicsSettings` still drives the derived
  renderer wrappers from the default levels.

## Acceptance criteria

- A version- and size-valid `GameSettings.bin`, `GraphicsSettings.bin`, or
  `SoundSettings.bin` carrying one invalid covered value (out-of-range language,
  NaN or out-of-range float, quality byte past the last level) loads with every
  setting of that file at its default and logs one warning naming the file.
- A file saved by the game restores the same values as before the change.
- No settings loader contains a clamp, `std::min`, or default substitution on a
  read value; the listed docs describe rejection, not fallback.
- Client Debug builds through `/compile`.

## Notes

- `Documents/Plans/Engine/MultisampleCountCapabilityValidation.md` recommends
  normalizing the persisted sample count by membership at load, and
  `Documents/Plans/Engine/TweaksSunAngleFiniteValidation.md` recommends
  resetting a non-finite Tweaks sun angle to its default and keeping finite
  clamping. Both predate the new convention and both substitute bad file
  values; each reconciles with the convention at its own claim. The
  multisample Plan also edits `LoadGraphicsSettings`, so whichever lands second
  rebases onto the other's load path.
