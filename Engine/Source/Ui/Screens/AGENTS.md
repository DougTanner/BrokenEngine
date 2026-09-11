# `/Engine/Source/Ui/Screens/` - Engine ImGui Screens

Engine-owned ImGui screens: the six standard player-facing menus — Main Menu, Pause, Graphics, Sound, Game Settings, and Modal — plus the `TweaksScreen/` debug family. `ImGuiManager` owns construction and per-frame invocation. The HUD and the game extension of TweaksScreen stay in the game project. This document owns the runtime contracts that code changes must preserve.

## Standard Menu Contract

- A standard screen takes `Render(GameBase&)` and never names `game::Game`, `game::gpGame`, or a game session type. It uses the engine-owned `meUiState`, `mModalMessage`, `mGameFlags`, and `InMainMenu()` directly off `GameBase`, including the quit flag; calls `GetStandardMenuModel()` for the title, offered features, and live client/discovery state; and routes game-owned side effects through `ApplyStandardMenuAction()`. Engine-owned game, sound, and graphics settings save/reset are called directly, never as actions.
- `StandardMenuModel` is a snapshot taken at entry, so a screen rereads the model after any action a later decision in the same render pass depends on; Main Menu does that after starting discovery and after connecting.
- `StandardMenuModel::pcTitle` must point to storage that outlives the render pass; a workbuffer allocation would dangle.
- Missing `StandardMenuFeature::kLocalServer` suppresses discovery, auto-connect, the scanning placeholder, and the Local Server entry; missing `kRemoteServer` hides its disabled placeholder. Button width is measured over the full label set either way, so a disabled feature never reflows the column.
- Screen constructors stay implicit and inert: no body, game dereference, filesystem work, or allocation. `ImGuiManager` is built before `game::Game`, so only `Render` may assume a game exists.
- Screens gate themselves from authoritative game/UI state; `ImGuiManager` invokes main and modal surfaces independently of in-game screen gating.
- Auto-connect keeps render-local state so a succeeded connection is not retried and a dropped client rearms it; the model reports only what the session can observe.

## Screen-Specific Contracts

- Players see the audio menu labeled AUDIO, while its screen, UI state, wrappers, and settings file keep the internal Sound name; renaming the internals would rewrite a persisted filename.
- Time of Day stays visible and enabled while the Graphics screen is open, and the camera keeps returning that override. The main-menu entry point seeds it from the live camera; the pause-menu entry point does not, so opening Graphics in game reuses the retained override and may snap the sun. A new entry point must pick one of the two behaviors.
- Disabling a Graphics parent control never resets its child values.
- Water quality keeps the mesh-detail scale fixed at `0.25`: Low disables vertex displacement, Medium uses the fixed low-wave band, High the low- and medium-wave bands.
- Anisotropy applies only to visible image passes such as models, terrain, and water; offscreen lighting, shadow, smoke, and wind passes use non-anisotropic samplers.
- Compare GPU timers in Immediate mode or with fixed GPU clocks; FIFO down-clocking and boosting around the refresh cap makes per-pass times workload-dependent.
- The Graphics header's Defaults action calls `ResetGraphicsSettings()` immediately and stays on the screen.
- The Game Settings language buttons stay in their own language and are never routed through the translation table.

## Shared Authoring Rules

These apply to every ImGui screen, engine or game.

- General ImGui layout mechanics - scale, sizing, placement, rhythm, and primitives - live in the layout contract (`../../../../Documents/UserInterfaceDesign.txt`).
- A screen registers its rendered rectangle for world-render occlusion only while its background is fully opaque; a screen that picks its own background alpha independently of the opaque-UI setting stays unregistered even at full alpha.
- Control and header label text is the harness automation API and does not get renamed. Several Graphics rows repeat the same option labels, so a label-based harness lookup is ambiguous there and has to target those rows by coordinates; they stay distinguishable to ImGui by scoping each row to its wrapper, not by unique labels.
- Menu font pushes must be balanced within the owning ImGui window. Construct scoped font helpers before `Begin` or entirely inside the window so `End` sees the expected stack.
- Hover animation is caller-owned state and composes ImGui alpha, including disabled controls, without heap allocation.
- Settings controls bind through the shared menu helpers. Tweaks sliders instead follow the engine TweaksScreen contract (`TweaksScreen/AGENTS.md`).

## See Also

- `TweaksScreen/AGENTS.md` - Tweaks runtime parameter UI
- `../../../../Projects/BrokenEngineSandbox/Source/Ui/Screens/AGENTS.md` - Game HUD and `game::TweaksScreen`
- `../../Graphics/Managers/AGENTS.md#imguimanager` - Screen invocation and opaque regions
