<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T18:45:38.995Z","dependsOn":["Documents/Plans/Ui/EngineGameLocalizationSplit.md"]} -->
# D6: Establish the GameSettings ownership boundary after localization

## Context

This is the localization-dependent D6 handoff extracted from the removed
settings/build investigation. The repository user explicitly permits this Plan
to carry unresolved choices and multiple options; they are recorded honestly
below for the implementing session rather than being presented as approved
architecture.

`Projects/BrokenEngineSandbox/Source/ClientSettings.cpp:18-90` currently owns
`GameSettings`: `kiVersion = 2`, `GameSettings.bin`, a fixed-width `int32_t`
language value, UI font scale, UI opacity, an opaque-UI byte, `engine::UiTheme`,
and padding. `SaveGameSettings()`, `LoadGameSettings()`, and
`ResetGameSettings()` use the existing versioned-file machinery. The file's
opaque-input guard in `LoadGameSettings()` accepts only values in the current
language range and otherwise selects English.

`Engine/Source/Main.cpp:245,465` drives the load/save entry points. The storage
root is created from the explicit launch AppData directory or the OS AppData
directory, then `game::kGameName` is appended at
`Engine/Source/File/FileManager.cpp:112-143`; `GetFilePath()` selects that root
for settings files at `:181-199`. `Projects/BrokenEngineSandbox/Source/Game.h:22-24`
supplies distinct client/server names. Shared default roots therefore require
stable distinct names, while distinct explicit roots isolate equal names.

The prerequisite `Documents/Plans/Ui/EngineGameLocalizationSplit.md` owns the
engine language state and count needed to validate the persisted language value.
Until that Plan lands, the settings language guard must remain local to the
file trust boundary and `GameSettings` must not be moved on the strength of an
assumption about language ownership.

## Design

### Direction and unresolved choices

There is no user-approved GameSettings owner yet. After the localization
dependency lands, the implementation session must resolve and record the
schema-owner, language-representation, and layout-assertion choices while
preserving the current per-game identity invariant:

1. **Schema owner.** Choose either (A) an engine-owned `GameSettings` schema
   and file, or (B) a game-owned schema with an engine helper for versioned
   read/write. The generic sound/graphics Plan records the analogous preferred
   direction but does not decide this GameSettings boundary.
2. **Language representation.** Preserve the persisted `int32_t` field and
   decide how the engine-owned language type/count participates in validation;
   do not expose a public conversion helper merely to move the guard.
3. **Current per-game identity invariant.** Preserve the current
   `FileManager`/`kGameName` path-identity contract described above, including
   stable distinct names for shared default roots or distinct explicit roots.
   This contract is fixed for this Plan: do not resolve or modify
   `FileManager`/path identity, change the `kGameName` requirement, or add a
   migration, compatibility lookup, or alternate path. Any future
   identity-policy change belongs exclusively to
   `EngineClientSettingsOwnership.md`.
4. **Layout assertion inventory.** Identify important field offsets as well as
   `sizeof` before moving the schema. The exact assertion set is intentionally
   not invented here.

### Landable stage

1. Wait for the localization Plan dependency and inspect its landed language
   type, count, persisted-value contract, and callers.
2. Resolve and record the schema-owner, language-representation, and
   layout-assertion choices before changing `GameSettings`; consume the current
   AppData identity contract without changing it.
3. Move or invert the selected schema while preserving `kiVersion = 2`,
   `GameSettings.bin`, every field's exact type/order/padding, the fixed-width
   serialized language integer, default semantics, and save/load/reset callers.
4. Keep invalid persisted language values at the opaque file boundary: only
   values in the valid range may select a language; all other values select
   English. Do not add a public `LanguageFromPersisted` helper.

For this persisted schema, add both `sizeof` and important `offsetof`
assertions. `sizeof` alone cannot detect same-size reordering, offset changes,
or an enum underlying-type change. A version or layout change intentionally
resets an existing settings file; no backward-compatibility code is authorized.

`GameSettings` is outside deterministic Frame/PostRender CRC and network
packets, but it is persistent state and opaque file input. Preserve the current
trust-boundary checks and menu/startup behavior.

## Critical files

- `Projects/BrokenEngineSandbox/Source/ClientSettings.cpp` — `GameSettings`,
  version/filename, save/load/reset functions, language guard, and layout.
- `Projects/BrokenEngineSandbox/Source/ClientSettings.h` — entry-point
  declarations and any ownership-facing interface.
- `Projects/BrokenEngineSandbox/Source/Ui/Localization.h` — current game
  language facade and the consumer boundary after the localization Plan.
- `Documents/Plans/Ui/EngineGameLocalizationSplit.md` — prerequisite language
  state, count, and persisted-value contract.
- `Engine/Source/File/FileManager.cpp` — existing AppData root and per-game-name
  file identity (`FileManager::FileManager`, `FileManager::GetFilePath`),
  consumed read-only; future path-identity changes belong exclusively to
  `EngineClientSettingsOwnership.md`.
- `Projects/BrokenEngineSandbox/Source/Game.h` — current client/server
  `kGameName` values, consumed read-only; no identity change is in scope here.
- `Engine/Source/Main.cpp` — GameSettings startup load and shutdown/save calls.
- The engine settings owner/helper header and implementation selected by the
  resolved schema-owner option, if new files are needed.
- The affected client/server Visual Studio project and filter files if the
  selected owner adds or removes a source/header from a target.

## In scope

- Consume the landed localization dependency, resolve/record the three
  GameSettings choices above, and preserve the current per-game identity
  invariant.
- Implement the selected engine/game ownership boundary for `GameSettings`.
- Preserve the versioned file identity, exact field layout, fixed-width
  language representation, defaults, invalid-value fallback, and all callers.
- Add `sizeof` and important `offsetof` assertions for the persisted schema.
- Preserve the current stable per-game AppData identity contract through the
  existing `FileManager` root/name behavior. This is a fixed invariant for this
  Plan: do not resolve or modify `FileManager`/path identity, change the
  `kGameName` requirement, or add an alternate path. Any future identity-policy
  change belongs exclusively to `EngineClientSettingsOwnership.md`.
- Verify affected engine/game callers, both target affinities, and project/filter
  membership for any new or removed files.

## Out of scope

- `SoundSettings` or `GraphicsSettings`; those belong to
  `EngineClientSettingsOwnership.md`.
- `TweaksSettings` or `ClientStateSettings`, including fleet GUID, focused ship,
  camera-height, developer-only tweak, or screen-specific policy.
- Changing language vocabulary, labels, translations, or the localization
  Plan's selected strong-type design.
- Changing any `GameSettings` field type/order/padding, version, filename,
  serialized width, default semantics, or file flags.
- A public persisted-language conversion helper, compatibility migration,
  alternate file format, or alternate file search path.
- Changes to Frame/PostRender state, CRC, replay, network packets, `.pack` data,
  generic UI layout, or unrelated settings/build-switch work.
- Unit tests, a new test framework, and unrelated cleanup.

## Risk tier and invariants

Tier 3 applies: this crosses the engine/game ownership boundary and changes a
persisted serialization/file-layout contract at an opaque file trust boundary.
It may also change client/server project membership. The settings file is
outside Frame CRC and wire protocol, but its version, bytes, and trust-boundary
behavior are load-bearing.

Invariants:

1. `GameSettings` remains version 2 and uses `GameSettings.bin`.
2. Every field retains its current type, order, padding, default, and serialized
   width; the language field remains a fixed-width `int32_t` independent of an
   enum underlying type.
3. Valid persisted language integers select the corresponding language; invalid
   or out-of-range values select English without indexing outside the language
   table.
4. Moved schemas have both size and offset assertions.
5. The current shared default AppData root isolation contract remains in force:
   stable distinct `kGameName` values or distinct explicit roots are required.
   This Plan treats the contract as fixed and does not modify `FileManager`,
   path identity, or that requirement. Any future identity-policy change
   belongs exclusively to `EngineClientSettingsOwnership.md`.
6. Existing startup, save, load, reset, and UI behavior remains unchanged, and
   no new Engine dependency on fleet/player/camera concepts is introduced.

## Coordination

This Plan depends on `Documents/Plans/Ui/EngineGameLocalizationSplit.md`; the
dependency is directional and no reciprocal Plan edit is required. The current
per-game AppData identity contract is a fixed invariant here and must not be
modified. Any future identity-policy change belongs exclusively to
`EngineClientSettingsOwnership.md`; this Plan has no dependency on that future
decision and must not be pulled into it. The language-state and
layout-assertion decisions are mandatory handoff points for implementation.

## Acceptance criteria

1. **Dependency and ownership are explicit.** The localization Plan is landed;
   the implementation record names the selected schema owner, language
   validation boundary, and exact size/offset assertion inventory. It preserves
   the current path-identity contract without redefining it. No option is
   presented as settled before that record exists.
2. **File identity and bytes remain compatible.** Inspect the owner and compile
   affected client/server targets. Expected: version 2, `GameSettings.bin`, all
   field order/types/padding, fixed-width language bytes, and defaults are
   unchanged.
3. **Language trust boundary remains safe.** Exercise load with valid,
   negative, and out-of-range persisted integers. Expected: valid values select
   a language and invalid values select English without an out-of-range table
   access; no public conversion helper is added.
4. **Normal behavior remains intact.** Exercise GameSettings startup load,
   reset, save, and menu use. Expected: current UI settings round-trip and the
   existing callers retain their behavior.
5. **Path identity remains unchanged.** Source review confirms this stage uses
   the existing `FileManager` root/name behavior and does not change path
    identity, `kGameName` requirements, or alternate paths. The current
    `FileManager` root/name contract and distinct-name/explicit-root invariant
    provide the isolation evidence for this stage.
6. **Non-goals stay non-goals.** Source review confirms sound/graphics,
   Tweaks/ClientState, localization vocabulary, Frame CRC, replay, wire data,
   `.pack` data, and build-switch work remain outside this stage.

## Notes

The preferred researched direction for D6 covers `SoundSettings` and
`GraphicsSettings`, not this deferred GameSettings owner. This Plan therefore
preserves the open owner choice rather than inventing an engine file shape. The
current per-game AppData identity is a fixed invariant consumed here without a
FileManager/path-identity change. Any future identity-policy change belongs
exclusively to `EngineClientSettingsOwnership.md`. The localization dependency
is a scheduler edge so this stage cannot be claimed before its language-state
prerequisite is available.
