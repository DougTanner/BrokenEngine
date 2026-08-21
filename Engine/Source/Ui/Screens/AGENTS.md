# `/Engine/Source/Ui/Screens/` - Engine ImGui Screens

Engine-owned ImGui screens: the six standard player-facing menus — Main Menu, Pause, Graphics, Sound, Game Settings, and Modal — plus the `TweaksScreen/` debug family. `ImGuiManager` (in `Engine/Source/Graphics/Managers/`) owns construction and per-frame invocation. The HUD and the game extension of TweaksScreen stay in the game project's `Ui/Screens` (`../../../../Projects/BrokenEngineSandbox/Source/Ui/Screens/AGENTS.md`). Visual placement and hierarchy follow `../../../../Documents/UserInterfaceDesign.txt`; this document owns the runtime contracts that code changes must preserve.

## Standard Menu Contract

- A standard screen takes `Render(GameBase&)` and never names `game::Game`, `game::gpGame`, or a game session type. It uses the engine-owned `meUiState`, `mModalMessage`, `mGameFlags`, and `InMainMenu()` directly off `GameBase` — including setting the quit flag, calls `GetStandardMenuModel()` for the game's title, offered features, and live client/discovery state, and routes game-owned side effects through `ApplyStandardMenuAction()`. Engine-owned game, sound, and graphics settings save/reset are called directly and are deliberately not actions.
- `StandardMenuModel` is a snapshot taken at entry. An action can change the state behind it, so a screen rereads the model after any action a later decision in the same render pass depends on; Main Menu does that after starting discovery and after connecting.
- `StandardMenuModel::pcTitle` must point to storage that outlives the render pass. A workbuffer allocation would dangle.
- Missing `StandardMenuFeature::kLocalServer` suppresses discovery, auto-connect, the scanning placeholder, and the Local Server entry; missing `kRemoteServer` hides its disabled placeholder. Button width is measured over the full label set either way, so a disabled feature never reflows the column.
- Screen constructors stay implicit and inert: no constructor body, game dereference, filesystem work, or allocation. Graphics builds `ImGuiManager` before `Main.cpp` constructs `game::Game`, so only `Render` may assume a game exists.
- Screens gate themselves from authoritative game/UI state because `ImGuiManager` invokes main and modal surfaces independently of in-game screen gating.
- Auto-connect keeps its own render-local state machine so a succeeded connection is not retried and a dropped client rearms it. That state is screen-local by design; the model reports only what the session can observe.

## Screen-Specific Contracts

- Players see the audio menu labeled AUDIO, while its screen, UI state, wrappers, and settings file keep the internal Sound name. The split is deliberate: renaming the internals would rewrite a persisted filename for no player-visible gain.
- Time of Day remains visible and enabled whenever the Graphics screen is open, and the camera keeps returning that override while the screen is up. The main-menu entry point seeds it from the live camera; the pause-menu entry point does not, so opening Graphics in game uses the retained override and may snap the sun. Any new entry point has to choose one of the two seeding behaviors; nothing at compile time links them.
- Graphics controls with an unchecked parent remain in the layout inside an ImGui disabled scope: multisample count, minimum sample shading, maximum anisotropy, Lighting quality and cadence, and Smoke Detail and area. Disabling a parent never resets its child values.
- Graphics slider formatting reflects value granularity: Minimum Ambient shows four decimals for its unsnapped float, and Lighting Update Cadence shows whole numbers matching its 1.0 snap step.
- Water quality keeps the mesh-detail scale fixed at `0.25`: Low disables vertex displacement, Medium uses the fixed low-wave band, and High uses the fixed low- and medium-wave bands.
- Anisotropy applies only to visible image rendering such as models, terrain, and water; offscreen lighting, shadow, smoke, and wind passes use non-anisotropic samplers.
- Compare GPU timers in Immediate mode or with fixed GPU clocks. FIFO can down-clock the GPU while refresh-capped, then boost again once GPU-bound rendering misses the refresh rate, making per-pass times workload-dependent.
- Graphics keeps FPS, localized Defaults, and Back in its header. Defaults calls `ResetGraphicsSettings()` immediately and stays on the screen; both actions use the same text-measured fixed width.
- The Game Settings language buttons stay in their own language and are never routed through the translation table.

## Shared Authoring Rules

These apply to every ImGui screen in the repository, engine or game.

- Use the common 2160-height UI scale for fonts, style geometry, authored pixel dimensions, and anchors. Measure text under the active font and add style padding; do not rescale measured dimensions.
- Convert localized UTF-32 strings into workbuffer-backed UTF-8 at the consuming expression; do not retain the result beyond its workbuffer lifetime.
- A screen registers its rendered rectangle for world-render occlusion only while its background is fully opaque. A screen that selects its own background alpha independently of the opaque-UI setting stays unregistered, even when that alpha is fully opaque.
- Control and header label text is the harness automation API and does not get renamed. Several Graphics rows nonetheless repeat the same option labels, so harness automation that resolves a UI item by its label text gets an ambiguous result for those buttons and has to target them by coordinates. Rows that share labels stay distinguishable to ImGui by scoping each row to its wrapper, not by making the labels unique. Graphics slider rows draw their own label to the left of the bar, so the slider itself carries a hidden-label id ("##" plus the same name): the snapshot records that id, and a query for the visible name resolves through the registry's substring tier.
- Rendered target-resolution output is authoritative for judging whether spacing looks centered, not just whether it measures centered; window bounds and text metrics are diagnostics.
- Menu font pushes must be balanced within the owning ImGui window. Construct scoped font helpers before `Begin` or entirely inside the window so `End` sees the expected stack.
- Shared menu helpers in Engine UI (`../AGENTS.md`) own scaling, localization conversion, wrapper bindings, common button sizing, menu chrome, slide-panel behavior, and player-facing layout-contract defaults, including single-screen MainMenu, Graphics, and Modal anchors/extents.
- Panels take their fill from the window background; accent drawing adds border and strip geometry over it and never substitutes a fill.
- Hover animation is caller-owned state and composes ImGui alpha, including disabled controls, without heap allocation.
- Settings controls bind through the shared menu helpers. Tweaks sliders instead follow the engine TweaksScreen contract (`TweaksScreen/AGENTS.md`).

## See Also

- `TweaksScreen/AGENTS.md` - `TweaksScreenBase` multi-section runtime parameter UI bound to Wrapper globals
- `../../../../Projects/BrokenEngineSandbox/Source/Ui/Screens/AGENTS.md` - Game HUD and `game::TweaksScreen`
- `../../Graphics/Managers/ImGuiManager.AGENTS.md` - Screen invocation, submission, scaling, and opaque regions
