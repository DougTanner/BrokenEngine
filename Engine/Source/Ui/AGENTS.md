# Engine UI - Shared Wrapper and Screen Infrastructure

Shared runtime settings, standard localization, player-facing menu helpers, the six standard menu screens, renderer quality levels, network-pending controls, and client-only curve/editor support. Screen composition and the standard-menu `GameBase` contract live in Screens (`Screens/AGENTS.md`); engine Tweaks registration contracts live in TweaksScreen (`Screens/TweaksScreen/AGENTS.md`); the game-side localization facade lives in game UI (`../../../Projects/BrokenEngineSandbox/Source/Ui/AGENTS.md`). Game, sound, and graphics settings persistence is engine-owned here.

## Wrapper Contracts

- `Wrapper` stores float, bool, or discrete values with one-consumer change tracking. `Changed<T>()` advances the previous-value state, so exactly one consumer may poll a wrapper each frame.
- Mutate through `Set` or `Reset`; assignment is disabled. `Set` leaves the prior value intact so the next poll observes a change. `Reset` updates both values and is for initialization or capability clamping that must not trigger rebuilds.
- Discrete wrappers fall back to their first allowed value when persisted or capability-derived input is invalid. Float wrappers may snap to a configured grid.
- One-argument construction is Boolean-only. Non-Boolean numeric and enum constructors are deleted so a scalar cannot silently convert to the Boolean overload; floats use the ranged constructor and discrete values supply an allowed-value set.
- Wrapper bounds annotated with shader invariants are correctness constraints, not UI tuning.
- Settings that alter baked render-target dimensions must participate in `Graphics::Refresh`; prefer per-frame uniforms when recreation is unnecessary.
- Wrapper headers are intentionally consumed directly rather than aggregated into `Engine.h`, limiting recompilation from tuning edits.

## Localization

- `LocalizationBase.h` owns the localization vocabulary: `Language` (a scoped `int32_t` enum whose values 0..5 are persisted in the game settings file), `kLanguageCount`, the `LanguageOption` labels in enum order, the selected-language state `geLanguage`, and `StandardString` with the standard menu strings. Game UI re-exports these through its facade (`../../../Projects/BrokenEngineSandbox/Source/Ui/AGENTS.md`).
- The UTF-32 standard table lives in `LocalizationBase.cpp` with internal linkage. Its outer extent stays deduced so the trailing all-empty sentinel row plus the `static_assert` catches a dropped or added row; an empty language cell falls back to English.
- `InitializeLocalization` sets the locale and uppercases the table in place. It is startup work, called from the `Game` constructor on both builds, and never runs in the main loop.
- The header compiles into both projects, carries no `BT_CLIENT`/`BT_SERVER` guard, and stays out of `Engine.h`: consumers include it directly or through the game facade.

## Menu and Panel Helpers

- Engine UI owns reusable player-facing menu layout and interaction helpers, including workbuffer-backed localized text conversion, wrapper controls, common sizing, menu chrome, and slide-panel behavior, plus the six standard menu screens that compose them. Game UI owns the HUD's composition, game-state gating, labels, and interaction flows.
- The shared menu-helper header remains parseable in both client and server projects because shared game UI code includes it on both sides. Its client-only panel state, font, and chrome declarations stay behind `BT_CLIENT`; game screen headers guard their client-only members without hiding shared declarations needed by server-side screens.

## Graphics Quality Levels

- The renderer quality levels are engine-owned client-only wrappers. Each discrete level is the persisted source of truth. Water is consumed directly during per-frame rendering; the other levels drive derived renderer wrappers that are never persisted.
- Apply functions write derived renderer wrappers through `Set` when a level changes and once after both successful and failed graphics-settings loads. `Graphics::Refresh` is the sole consumer polling those wrappers; apply functions never call `Changed<T>()`.
- Values read from a settings file are opaque input, so clamp each level before indexing its table.
- `GraphicsQualityWrappersBase.h` remains a `BT_CLIENT` direct include and stays out of `Engine.h`; engine UI owns persistence and game UI owns menu consumption.

## Settings Persistence

- `SoundSettings.bin` is version 3 and persists the three volume controls plus the checked-by-default `Mute in background` setting. Version 2 is intentionally rejected and settings fall back to defaults without a compatibility reader; reset restores the checked default, and save/load preserve the setting.
- Background mute is client presentation state and remains outside Frame/PostRender CRC and network state.
- `GraphicsSettings.cpp` persists the five engine-owned player-facing quality levels in `GraphicsSettings.bin`. A selected level is the persisted source of truth; derived renderer wrapper values are not serialized. Water mesh detail is fixed at `0.25` and is not persisted.
- `GraphicsSettings.bin` version 15 persists Water quality in place of the former detail float while retaining the payload size. Lighting defaults on; version 14 files intentionally fail the current-format gate and reset with the other graphics settings.
- `GameSettings.cpp` persists the selected language, UI font scale, opaque-UI toggle, UI opacity, and UI theme in `GameSettings.bin` version 2. The language is stored as a fixed-width `int32_t` so the `Language` enum's underlying type never decides the file layout, and an index outside the enum falls back to English rather than indexing the translation table. The engine-owned Game Settings screen calls save and reset directly (`Screens/AGENTS.md`).

## Shared Types

- `engine::UiState`, the unguarded `engine::GameBase` members `meUiState`, `mModalMessage`, and `mbShowImGui`, and the unguarded `GameBase::InMainMenu()` query form the shared engine/game UI-state contract. The engine owns their public vocabulary and storage for both client and server; the game HUD retains its own state transitions, gating, and rendering semantics.
- The client-only `StandardMenuFeature`, `StandardMenuState`, `StandardMenuModel`, and `StandardMenuAction` types plus the `GetStandardMenuModel` and `ApplyStandardMenuAction` virtuals extend that contract for the engine-owned menus. They are the only surface through which a standard screen reaches game-owned networking and frame transitions; their usage rules live in Screens (`Screens/AGENTS.md`). No server declaration depends on them.
- Height-dependent wrapper groups resolve camera-height-conditioned render values; Render (`../Graphics/Render/AGENTS.md`) owns how those resolved values reach the per-frame GPU buffers.
- Client-only curves use bounded monotone cubic interpolation and an ImPlot editor. Endpoints remain X-locked and interior control points ordered.
- The Lighting tab deliberately keeps two combine curves, `gCombineCurveOld` and `gCombineCurveNew`, behind the `gbUseCombineCurveNew` toggle so tuning can be compared live against the shipping baseline. They currently hold identical control points; that is the A/B setup, not dead duplication. Collapse to a single curve once tuning settles.
- `NetworkUiControl` disables a control while authoritative state has not resolved its request; call `Update` every frame with that authoritative state.

Most wrapper storage compiles into both builds so server-side simulation can read defaults. Client-only ImPlot types and menu panel, font, and chrome state remain `BT_CLIENT`-guarded; the shared menu helper's conversion and wrapper-control surface is available in both builds.

## See Also

- Screens (`Screens/AGENTS.md`) - Engine screen routing
- TweaksScreen (`Screens/TweaksScreen/AGENTS.md`) - Runtime parameter UI contracts
