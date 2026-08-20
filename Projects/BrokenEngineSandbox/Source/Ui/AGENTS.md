# Game UI - Localization and Settings Consumption

Game-owned localization and settings persistence; player-facing screens consume engine-owned menu helpers. Engine wrapper semantics are defined by the Engine UI hub (`../../../../Engine/Source/Ui/AGENTS.md`); ImGui screens live in Screens (`Screens/AGENTS.md`).

## Localization

- Localization is a shared, header-only UTF-32 table initialized in the `Game` constructor on both builds. Missing translations fall back to English, and a sentinel plus compile-time extent check prevents shifted rows.
- Initialization uppercases the table in place. Language selection is client runtime state, chosen on the Game Settings screen and persisted with the UI font scale, opaque-UI toggle, UI opacity, and theme in `GameSettings.bin`; a file whose stored index falls outside the Language enum falls back to English.
- Players see the audio menu labeled AUDIO, while its screen, UI state, wrappers, and settings file keep the internal Sound name. The split is deliberate: renaming the internals would rewrite a persisted filename for no player-visible gain.
- Engine UI owns UTF-32-to-UTF-8 conversion and its workbuffer-backed result handle. Screens perform conversion at the consuming expression and do not retain the result past its workbuffer lifetime.

## Sound Settings Persistence

- `SoundSettings.bin` is version 3 and persists the three volume controls plus the checked-by-default `Mute in background` setting. Version 2 is intentionally rejected and settings fall back to defaults without a compatibility reader; reset restores the checked default, and save/load preserve the setting.
- Background mute is client presentation state and remains outside Frame/PostRender CRC and network state.

## Wrapper Affinity

- Wrappers read by shared Frame code compile into both client and server projects even when only the client UI changes them.
- Rendering-only settings and their consumers remain whole-file `BT_CLIENT`-guarded and client-project-only.
- A new Frame dependency on a game wrapper must either stay on a client-only path or make that wrapper available to the server build.
- Game wrapper storage that mirrors engine Tweaks sliders keeps its declaration order matching the `../../../../Engine/Source/Ui/Screens/TweaksScreen/AGENTS.md` slider registration order. Wrapper storage with no Tweaks counterpart keeps a stable local order of its own.

## Graphics Quality Persistence

- `ClientSettings` persists the five engine-owned player-facing quality levels in `GraphicsSettings.bin`. A selected level is the persisted source of truth; derived renderer wrapper values are not serialized. Water mesh detail is fixed at `0.25` and is not persisted.
- `GraphicsSettings.bin` version 15 persists Water quality in place of the former detail float while retaining the payload size. Lighting defaults on; version 14 files intentionally fail the current-format gate and reset with the other graphics settings.
- Treat levels read from the settings file as opaque input: clamp each value before assigning it, then invoke the engine quality-level apply operation after both successful and failed graphics-settings loads so the defaults or loaded selections drive the renderer.
- `GraphicsMenuScreen` consumes the engine-owned level wrappers. Water is read directly by per-frame rendering; the other selections invoke their corresponding engine apply functions.
