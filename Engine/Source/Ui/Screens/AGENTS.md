# `/Engine/Source/Ui/Screens/` - Engine ImGui Screens

Engine-owned ImGui screens: the six standard player-facing menus — Main Menu, Pause, Graphics, Sound, Game Settings, and Modal — plus the in-game `ChatScreen` and the `TweaksScreen/` debug family. `ImGuiManager` owns construction and per-frame invocation. The HUD and the game's Tweaks content stay in the game project. This document owns the runtime contracts that code changes must preserve.

## Standard Menu Contract

- A standard screen takes `Render()`, reaches the game through `game::gpGame`,
  and never names a game session type. It uses the engine-owned `meUiState`,
  `mModalMessage`, `mGameFlags`, and the `GameFlags::kMainMenu` bit through
  `game::gpGame`, including the quit flag; calls `GetStandardMenuModel()` for
  the title, offered features, and live client/discovery state; and routes
  game-owned side effects through `ApplyStandardMenuAction()`. Engine-owned
  game, sound, and graphics settings save/reset are called directly, never as
  actions.
- `StandardMenuModel` is a snapshot taken at entry, so a screen rereads the model after any action a later decision in the same render pass depends on; Main Menu does that after starting discovery and after connecting.
- `StandardMenuModel::pcTitle` must point to storage that outlives the render pass; a workbuffer allocation would dangle.
- Missing `StandardMenuFeature::kLocalServer` suppresses discovery, auto-connect, the scanning placeholder, and the Local Server entry; missing `kRemoteServer` hides its disabled placeholder. Button width is measured over the full label set either way, so a disabled feature never reflows the column.
- Screen constructors stay implicit and inert: no body, game dereference, filesystem work, or allocation. `ImGuiManager` is built before `game::Game`, so only `Render` may assume a game exists.
- Screens gate themselves from authoritative game/UI state; `ImGuiManager` invokes main and modal surfaces independently of in-game screen gating.
- Auto-connect keeps render-local state so a succeeded connection is not retried and a lost connection rearms it, while a deliberate return to the main menu does not; the model reports only what the session can observe.

## Screen-Specific Contracts

- Players see the audio menu labeled AUDIO, while its screen, UI state, wrappers, and settings file keep the internal Sound name; renaming the internals would rewrite a persisted filename.
- Time of Day stays visible and enabled while the Graphics screen is open, and the camera keeps returning that override. The main-menu entry point seeds it from the live camera; the pause-menu entry point does not, so opening Graphics in game reuses the retained override and may snap the sun. A new entry point must pick one of the two behaviors.
- Disabling a Graphics parent control never resets its child values.
- Water quality keeps the mesh-detail scale fixed at `0.25`: Low disables vertex displacement, Medium uses the fixed low-wave band, High the low- and medium-wave bands.
- Anisotropy applies to visible image passes such as models, terrain, and water; TextureManager (`../../Graphics/Managers/AGENTS.md` `### TextureManager`) lists which samplers omit anisotropy.
- Compare GPU timers in Immediate mode or with fixed GPU clocks; FIFO down-clocking and boosting around the refresh cap makes per-pass times workload-dependent.
- The Graphics header's Defaults action calls `ResetGraphicsSettings()` immediately and stays on the screen.
- The Game Settings language buttons stay in their own language and are never routed through the translation table.
- `ChatScreen` draws the chat state in `GameBase::mChatLog` only in game with
  `UiState::kNone`; anywhere else it closes the input box and clears its text.
  `ImGuiManager` calls it only while Tweaks is hidden, and
  `GameBase::ProcessInput` opens the box only then, so an open box is always
  drawn. Every Enter in the box passes the text to `Client::SendChat`, which
  owns every send check, then clears and closes the box; deactivating the field
  without Enter (Esc or a click elsewhere) also clears and closes it. History
  lines sit in the lower-left corner and fade out over `kChatFadeDuration` from
  arrival, and are drawn unfaded while the box is open. The `Player N` label is
  fixed mixed-case text, never routed through the translation table.

## Shared Authoring Rules

These apply to every ImGui screen, engine or game.

- General ImGui layout mechanics - scale, sizing, placement, rhythm, and primitives - live in the layout contract (`../../../../Documents/UserInterfaceDesign.txt`).
- A screen registers its rendered rectangle for world-render occlusion only while its background is fully opaque; a screen that picks its own background alpha independently of the opaque-UI setting stays unregistered even at full alpha.
- Control and header label text is the harness automation API and does not get renamed. A label whose visible text repeats within one window carries a hidden `##<section>` suffix, as the Graphics quality rows' `RadioRow` options carry their row header, so the harness names each item by its window and full label.
- Menu font pushes must be balanced within the owning ImGui window. Construct scoped font helpers before `Begin` or entirely inside the window so `End` sees the expected stack.
- Hover animation is caller-owned state and composes ImGui alpha, including disabled controls, without heap allocation.
- Settings controls bind through the shared menu helpers. Tweaks sliders instead follow the engine TweaksScreen contract (`TweaksScreen/AGENTS.md`).

## See Also

- `TweaksScreen/AGENTS.md` - Tweaks runtime parameter UI
- The game project's own `Source/Ui/Screens/AGENTS.md` - Game HUD and game Tweaks content
- `../../Graphics/Managers/AGENTS.md#imguimanager` - Screen invocation and opaque regions
