# Input - Hardware Snapshot and Display Input

`RawInputManager` publishes one coherent `RawInput` snapshot per display frame;
`Input` turns that snapshot into menu commands, remappable key actions, and
camera input. All files here are client-only. The deterministic per-tick
`FrameInput` type is game-owned and lives in game `Source/Frame`, unrelated to
this directory.

Global: `gpInput` — constructed in `Main.cpp`, polled only from
`engine::GameBase::ProcessInput` through `engine::InputPoll`.

## Poll Lifetime

- One `ProcessInput` call is one poll: `InputPoll` is a non-copyable RAII guard
  that allocates nothing and must not outlive `ProcessInput`. Its constructor
  publishes `RawInput`, resolves active keybind capture, and fills
  `engine::MenuInput` and `engine::gpCamera->mCameraInput`; its destructor
  advances private `Input::mPreviousRawInput` last — after every edge consumer
  including `game::gpGame->ProcessGameMenuInput` and on the modal and open-chat
  early-return paths.
- `InputPoll::Pressed` and `InputPoll::Held` resolve `KeybindFor` through ADL so
  engine `KeyAction` and game action enums share one poll API. Both return false
  when unbound, during capture, or while the UI owns the keyboard (except
  `VK_TAB` in `Pressed`).
- `MenuInputFlags` is the generic menu vocabulary; game-specific actions poll
  their own bindings through `InputPoll`. `kSingleStep` (from Tab),
  `kMouseIsDown`, `kMouseClick`, `kGamepadButton`, `MenuInput::f2Mouse`, and
  `MenuInput::f2Gamepad` are produced and intentionally unconsumed.

## Device and Frame Boundaries

- Win32 Raw Input owns keyboard state for every binding; window events write
  scratch state and `Update()` publishes one coherent snapshot per display
  frame. The keyboard registers without `RIDEV_NOLEGACY`, so legacy keyboard and
  IME messages also reach ImGui for text entry; no binding reads them. Tab key
  and character messages are withheld from ImGui (reserving Tab for the map),
  and `mSuppressedChatOpenKey` withholds the chat-open key's key-down messages
  and `TranslateMessage` calls until release or focus loss so opening chat does
  not type into it. ImGui keyboard navigation stays off, so arrows, Space,
  Enter, and Esc never move, activate, or cancel menu focus. Gamepad navigation
  and agent-injected ImGui events use separate explicit paths.
- Mouse state comes from DirectXTK's Win32 message path and gamepad state from
  XInput. The DirectXTK mouse object must exist before window creation, because
  synchronous window messages call its static processing path.
- Wheel state is a lifetime accumulator consumers diff against the previous
  snapshot's baseline, seeded on the first poll so the first frame reports no
  delta. Mouse normalization uses the Vulkan framebuffer extent and may briefly
  leave the 0..1 range during resize.
- Keyboard handling stays hand-rolled on that Raw Input path: do not unify it
  onto DirectXTK's `Keyboard`. ThirdParty (`../../../ThirdParty/AGENTS.md`) owns
  the rationale.

## Bindings and Mode

- Remappable player-facing actions split across engine (`KeyAction` in
  `Keybinds.h`, persisted in `EngineKeybinds.bin`) and game (`GameKeyAction` in
  game `Input/GameKeybinds.h`, persisted in `GameKeybinds.bin`) versioned binary
  files; fixed inputs (Esc, Alt+F4) and debug/profile/debug-render shortcuts
  stay outside those tables. `IsBindableKey` is the single allowlist shared by
  live capture and file validation; loaders reject invalid or duplicate keys to
  defaults, and `Input::LoadAllKeybinds` resolves cross-domain startup conflicts
  (loaded overrides default; engine wins when both load). `Input::RebindSlot`
  unbinds any prior owner across both domains before assigning. During keybind
  capture, normal keyboard bindings and menu-back are suppressed for the entire
  capture frame (including completion/cancel) while raw Alt+F4 quit stays live;
  Esc cancels and Backspace/Delete unbinds.
- Debug/profile/screenshot/debug-render keys compile in via `if constexpr` on
  `kbDebugInput` / `kbProfiling` / `kbScreenshots` / `kbDebugRender`, and
  free-camera movement on `kbFreeCamera`. New debug-only keys belong inside
  those blocks.
- Gameplay Observer mode (`CameraBase::ObserverState::kObserving`) reads arrow
  keys and mouse edge-scroll into `mCameraInput.f2Move` and suppresses
  `kDebugTextureNext`/`kDebugTexturePrevious`. Both require no open menu or
  map; arrow keys also require UI-free keyboard ownership and window focus
  (unless an agent script is active), while edge-scroll requires a positive
  `gObserverEdgeScrollMargin` (its disable setting), retains proportional
  strength, and is separately blocked by focus loss, physical-input suppression,
  gamepad mode, or a hovered ImGui window. The combined vector scales down only
  when its length exceeds 1.
- Gamepad mode engages when either thumbstick's `|x| + |y|` exceeds the
  threshold; mouse movement or a KBM whitelist key (configured camera-move
  bindings, arrows, numpad 1/2/3/5, LMB/RMB) flips back. New movement keys must
  extend the whitelist or mode detection misses them.
- While a menu is up, gamepad buttons / d-pad / left stick are pushed into
  ImGui's IO so menus are pad-navigable. `GameBase` supplies menu visibility as
  an `InputPoll` constructor argument, so the engine never reads a game global
  to decide it.
- While an ImGui text field is being edited
  (`ImGuiContext::PlatformImeData.WantTextInput` from the last rendered frame),
  the keyboard belongs to the UI: `KeyboardPressed` reports no press except for
  Tab, and free-camera keys add no movement. The Tab exception retains raw
  rising-edge detection and existing game eligibility, modal, and chat gates.
  Mouse, gamepad, the KBM-mode whitelist, and scroll ownership are unaffected,
  and the previous snapshot still advances. A key pressed before the field's
  first rendered active frame can still reach a binding.

## Scroll Ownership

- The wheel belongs to the UI when an ImGui key owner claims
  `ImGuiKey_MouseWheelY`, while the cursor hovers an ImGui window that can
  actually consume it (not collapsed, nonzero `ScrollMax.y`, wheel not disabled
  by window flags), or while `ImGuiContext::WheelingWindow` still holds an
  earlier target. Everything else, including an open but non-scrolling menu or
  modal, gives the delta to the camera. The test mirrors ImGui's own
  `UpdateMouseWheel` routing and reads `ImGuiContext` from the client-only
  `imgui_internal.h` view.
- Two accepted timing tolerances: hover is one frame stale (ImGui resolves it in
  `NewFrame`, after input polling), so a notch on the frame the cursor crosses a
  panel edge may be double-handled or lost; and ImPlot's `MouseWheelY` ownership
  stays visible for two input polls after leaving a plot, which may suppress an
  immediate background notch.
- The hovered-window test assumes no child windows. If a `BeginChild`, popup,
  combo, list box, or scrolling table is ever added, the gate must grow ImGui's
  `ParentWindow` bubble logic from `FindBestWheelingWindow`.
- The previous-snapshot assignment always advances the wheel baseline, including
  when input is swallowed, so a suppressed notch cannot surface later as zoom
  applied after the fact.

## Focus and Agent Input

- Focus gain registers the keyboard and clears scratch state; focus loss
  unregisters it and freezes the published snapshot. Gamepad suspend/resume
  follows focus.
- An active agent script may publish synthetic input while unfocused; agent-port
  clients suppress physical keyboard, mouse, gamepad, and cursor trapping for
  the process lifetime, preserving window close handling. The one exception is
  IME: `WM_IME_*` messages are not stripped, so IME result characters a human
  composes on a focused agent window still reach an active ImGui text field.
- Cursor trapping uses Win32 `ClipCursor`, follows game policy while focused,
  and is forced off on focus loss or physical-input suppression.
- Synthetic keys overlay the published snapshot only while a script runs; the
  synthetic wheel offset is added to the lifetime wheel value on every publish,
  script or not. Keep these sinks separate because their lifetimes differ.
- Gamepad construction may fail; polling and vibration paths tolerate a null
  gamepad object and clear the published pad state on disconnect.

## See Also

- Engine Agent (`../Agent/AGENTS.md`) - Synthetic input
- Game source (the game project's own `Source/AGENTS.md`) - Game menu input
- Game frame (the game project's own `Source/Frame/AGENTS.md`) - `FrameInput`
- `../../../Documents/Architecture/FrameUpdatePipeline.md` - Main-loop order
