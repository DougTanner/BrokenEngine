<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T18:40:19.836Z","dependsOn":[]} -->
# D6: Establish the engine/game client-settings ownership boundary

## Context

This is the executable handoff for D6 from the removed settings/build
investigation. The repository user explicitly permits the implementation Plan to
carry unresolved choices and multiple options; the options below are therefore
not silently decided. The implementing session must resolve them from the
evidence and record which direction it takes before changing the persisted
boundary.

`Projects/BrokenEngineSandbox/Source/ClientSettings.cpp` currently contains:

- `GameSettings` at lines 18-90 (`kiVersion = 2`, `GameSettings.bin`) is a
  separate, localization-dependent D6 stage recorded in
  `EngineGameSettingsOwnership.md`; it is listed here only to establish the
  complete current schema range.
- `SoundSettings` at lines 92-154 (`kiVersion = 3`, `SoundSettings.bin`), with
  three volume floats at offsets 0/4/8, a `uint8_t` `Mute in background`
  setting at offset 12, padding at offsets 13-15 (size 16), and the existing
  `kbRecording` music mute override in `LoadSoundSettings()`.
- `GraphicsSettings` at lines 156-312 (`kiVersion = 14`,
  `GraphicsSettings.bin`), including its flag bitset, Vulkan enum fields,
  floats, quality-level bytes, and padding.
- `TweaksSettings` at lines 314-361 (`kiVersion = 14`, `TweaksSettings.bin`),
  including the game UI visibility and the engine-owned tweak section state.
- `ClientStateSettings` at lines 363-406 (`kiVersion = 3`,
  `ClientState.bin`), including fleet GUID, focused ship, and camera-height
  state.

The engine already consumes these game entry points in
`Engine/Source/Main.cpp:213,245-246,462-465` and consumes the wrapper values
through the engine UI/graphics/audio code. The current storage root is created
from the explicit launch AppData directory or the OS AppData directory, then
`game::kGameName` is appended at `Engine/Source/File/FileManager.cpp:112-143`;
`GetFilePath()` selects that root for settings files at `:181-199`.
`Projects/BrokenEngineSandbox/Source/Game.h:22-24` supplies distinct client and
server names. This is the evidence that same-root games remain isolated only
when each game supplies a distinct stable name; distinct explicit roots also
isolate equal names.

The unresolved ownership question for this Plan is whether the engine owns one
shared schema and the versioned files, or whether each game owns its file
schemas while an engine helper supplies the read/write machinery. The
researched recommendation (2026-08-11, not user-approved) is that the engine
own `SoundSettings` and `GraphicsSettings` outright, while `TweaksSettings` and
`ClientStateSettings` remain game-owned. `GameSettings` ownership and its
localization dependency are recorded separately in
`EngineGameSettingsOwnership.md`.

## Design

### Direction and unresolved choices

Keep the following as explicit implementation decisions rather than assuming
that the recommendation is already approved:

1. **Schema owner.** Choose either (A) engine-owned schemas and files for the
   generic settings, the preferred direction, or (B) game-owned schemas with an
   engine read/write helper. Either choice must retain the exact existing file
   identity and layout. The helper-only option is not permission to move a
   game-specific field into Engine.
2. **Recording policy.** Before moving `SoundSettings`, decide whether the
   `kbRecording` music mute policy moves with the sound-settings contract or is
   split into a separate game policy. Do not leave a moved sound owner reading
   an undeclared game switch.
3. **Per-game identity (owned here).** Confirm the stable, distinct `kGameName`
   requirement for shared default AppData roots, or require distinct explicit
   roots. This Plan solely owns this file-identity decision and any resulting
   `FileManager` path-identity contract; it is not a new compatibility layer.
4. **Layout assertion inventory.** Identify the important offsets as well as
   `sizeof` for every moved schema before editing it. The exact assertion set is
   intentionally not invented here.

### Landable stages

1. Resolve the choices above, including the per-game identity and any resulting
   `FileManager` path-identity contract, and document the selected ownership
   shape in the implementation change. This Plan has no localization
   dependency; the separate `EngineGameSettingsOwnership.md` Plan carries that
   edge.
2. Move or invert `SoundSettings` after the recording-policy decision. Preserve
   `kiVersion = 3`, `SoundSettings.bin`, the field order and types (three volume
   floats at offsets 0/4/8, `uint8_t` `Mute in background` at offset 12,
   padding at offsets 13-15, size 16), the default-checked mute setting,
   v2 rejection/defaulting without a compatibility reader, load/reset behavior,
   `SaveSoundSettings()` main-loop allocation suppression, and the existing
   recording music override.
3. Move or invert `GraphicsSettings` after the completed
   `GraphicsQualityWrappersToEngine` prerequisite (landing evidence `cd07f0b`).
   Preserve `kiVersion = 14`, `GraphicsSettings.bin`, the flag bit positions,
   Vulkan enum fields, all persisted floats and four quality-level bytes, padding,
   wrapper application, and reset behavior.
4. Handle `GameSettings` only through the separate
   `EngineGameSettingsOwnership.md` Plan after its localization dependency and
   language-state decision; this Plan does not claim that stage.
5. Leave `TweaksSettings` and `ClientStateSettings` in the game for this Plan.
   They are developer-only or contain fleet/focus/camera concepts and remain
   entangled with future screen/game-state ownership decisions.

For every moved schema, add both `sizeof` and important `offsetof` assertions;
`sizeof` alone cannot detect a same-size reorder, offset change, or enum-layout
change. Existing version counters and `.bin` filenames are file-format
identity. The current `SoundSettings` v3 format rejects v2 and defaults it
without a compatibility reader; do not add backward-compatibility code or
migration without explicit user approval. A version bump or changed layout
intentionally resets an existing settings file.

The settings files are outside deterministic Frame/PostRender CRC and network
packets, but they are opaque file input and their layout is persistent state.
Keep validation at the file boundary, keep wrapper/default behavior unchanged,
and keep all settings reads/writes on the existing startup/menu paths.

## Critical files

- `Projects/BrokenEngineSandbox/Source/ClientSettings.cpp` — current schemas,
  versions, filenames, save/load/reset functions, `kbRecording` policy, and
  language validation.
- `Projects/BrokenEngineSandbox/Source/ClientSettings.h` — game-facing settings
  entry points and any ownership-facing declarations.
- `Engine/Source/File/FileManager.cpp` — AppData root and per-game-name path
  identity (`FileManager::FileManager`, `FileManager::GetFilePath`).
- `Projects/BrokenEngineSandbox/Source/Game.h` — client/server `kGameName` values
  consumed by file identity.
- `Engine/Source/Main.cpp` — settings startup load and shutdown/save callers.
- `Engine/Source/Ui/GraphicsSettingsWrappersBase.h/.cpp` and
  `Engine/Source/Ui/SoundSettingsWrappersBase.h/.cpp` — existing engine-owned
  wrapper state driven by the persisted schemas.
- `Documents/Plans/Engine/EngineGameSettingsOwnership.md` — separate
  localization-dependent D6 stage for `GameSettings`.
- The engine settings owner/helper header and implementation selected by the
  resolved schema-owner option, if new files are needed.
- The affected client/server Visual Studio project and filter files, if the
  selected owner adds or removes a source/header from a target.

## In scope

- Resolve and record the four D6 choices above before changing ownership.
- Implement the selected engine/game boundary for `SoundSettings` and
  `GraphicsSettings`, preserving their current versioned file identities,
  exact layout, trust-boundary behavior, wrappers, and menu/startup callers.
- Add layout assertions covering `sizeof` and important `offsetof` values for
  every moved persisted schema.
- Resolve and own the stable per-game AppData identity contract through the
  existing `FileManager` root/name behavior; this is the only settings Plan
  authorized to modify that path-identity contract.
- Verify all affected engine and game callers, both client/server target
  affinities, and project/filter membership for any new or removed files.

## Out of scope

- Moving `TweaksSettings` or `ClientStateSettings`.
- Moving or re-owning `GameSettings`; that stage belongs to
  `EngineGameSettingsOwnership.md` after the localization dependency.
- Changing any settings field type, order, padding, version counter, filename,
  serialized width, default semantics, or file flags.
- Moving fleet GUID, focused-ship, camera-height, developer-only tweak, or
  screen-specific policy into Engine.
- Changing localization vocabulary or adding a public language conversion
  helper.
- Changing `FileManager` behavior beyond the selected settings path-identity
  contract; no compatibility search, migration, or alternate file format.
- Changes to Frame/PostRender state, CRC, replay, network packets, `.pack` data,
  rendering/audio behavior outside settings persistence, or generic UI layout.
- Unit tests, a new test framework, unrelated settings cleanup, and build-switch
  extraction (tracked separately by `EngineBuildSwitchContract.md`).

## Risk tier and invariants

Tier 3 applies: this crosses the engine/game ownership boundary and changes
persisted serialization/file-layout contracts at an opaque file trust boundary.
It also may change client/server project membership. Settings are outside the
Frame CRC and wire protocol, but all existing file-identity and layout
invariants remain load-bearing.

Invariants:

1. The current `SoundSettings.bin` v3 format remains the owned format: three
   volume floats at offsets 0/4/8, `uint8_t` `Mute in background` at offset 12,
   padding at offsets 13-15, and size 16. Version 2 is rejected/defaulted
   without a compatibility reader. `GraphicsSettings.bin` bytes decode with
   the same version, field types, order, padding, and defaults after the move.
2. `SoundSettings` keeps version 3, the default-checked mute setting, and the
   existing recording music override; reset, save, and load preserve them. Its
   file write remains allocation-suppressed when invoked from the UI.
   `GraphicsSettings` keeps version 14, all flag bits, Vulkan fields, persisted
   floats, four quality-level bytes, and padding.
3. A shared default AppData root cannot make two games read each other's files:
   stable distinct `kGameName` values or distinct explicit roots are required.
4. Moved schemas have both size and offset assertions; an enum's underlying
   type is never allowed to silently define persisted width.
5. `TweaksSettings` and `ClientStateSettings` remain game-owned and no new
   Engine dependency on fleet/player/camera concepts is introduced.
6. Existing startup, reset, save, and load call ordering remains unchanged.

## Coordination

The separate `EngineGameSettingsOwnership.md` Plan depends on
`Documents/Plans/Ui/EngineGameLocalizationSplit.md`; this generic
sound/graphics Plan does not. The AppData identity decision and any
`FileManager` path-identity contract change belong solely to this Plan; the
GameSettings Plan consumes the established contract without modifying it. The
graphics stage requires the completed `GraphicsQualityWrappersToEngine`
prerequisite (landing evidence `cd07f0b`).
The recording-policy, stable-name, and layout-assertion decisions are mandatory
coordination constraints for any implementer taking this Plan. Build-switch
ownership is independent and belongs to `EngineBuildSwitchContract.md`.

## Acceptance criteria

1. **Ownership decision is explicit.** The implementation record names the
   selected schema-owner option, recording-policy resolution, per-game identity
   rule, and exact size/offset assertion inventory;
   no option is presented as settled before that record exists.
2. **Sound persistence remains compatible.** Inspect the owner and compile the
   affected client/server targets; exercise save, load, reset, and recording
   behavior. Expected: version 3 and the `SoundSettings.bin` v3 layout (three
   volume floats at offsets 0/4/8, `uint8_t` mute at offset 12, padding at
   13-15, size 16) remain unchanged; v2 is rejected/defaulted without a
   compatibility reader; all three volumes and the default-checked mute
   setting round-trip; UI-triggered saves remain allocation-suppressed; and
   the existing recording music override remains in effect.
3. **Graphics persistence remains compatible.** Inspect layout assertions,
   compile the affected targets, and exercise graphics reset/save/load. Expected:
   version 14 and `GraphicsSettings.bin` remain unchanged, flag/enum/float/
   quality-level fields round-trip, and quality wrappers are applied as before.
4. **Path identity is isolated.** With two game names under one default root,
   inspect or run the file-path scenario. Expected: each game resolves its own
   settings directory; explicit roots remain isolated as documented.
5. **Non-goals stay non-goals.** Source review confirms GameSettings is left to
   its separate Plan, Tweaks/ClientState remain game-owned, frame CRC, replay,
   wire data, `.pack` data, and unrelated UI/audio/graphics behavior are
   unchanged.

## Notes

The researched recommendation is a direction, not an approval: engine-owned
`SoundSettings` and `GraphicsSettings`, game-owned `TweaksSettings` and
`ClientStateSettings`, with `GameSettings` handled by the separate
localization-dependent Plan. The implementation session may choose the
alternate helper-only shape if its evidence supports it, but it must preserve
every invariant and surface that choice in review. The original investigation's
stronger objection was path identity, not the mechanics of
`ReadVersionedFile`/`WriteVersionedFile`.
