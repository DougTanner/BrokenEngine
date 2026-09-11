# Input - Hardware Snapshot and Display Input

`RawInputManager` publishes one coherent `RawInput` snapshot per display frame; `Input` turns that snapshot into menu commands and camera input. `RawInputManager` and `Input` are client-only. The deterministic per-tick `FrameInput` type is game-owned and lives in game `Source/Frame`, unrelated to this directory.

Global: `gpInput` — constructed in `Main.cpp`, polled only from `engine::GameBase::ProcessInput`. `BeginPoll` fills `engine::MenuInput` and writes straight into `engine::gpCamera->mCameraInput`.

## Poll Lifetime

- One `ProcessInput` call is one poll: `BeginPoll` publishes the snapshot and produces this frame's values; `CompletePoll` copies the current snapshot into the previous one and runs last — after every edge consumer including the game callback, and also on the modal path that skips the callback. Moving it earlier silently breaks edge detection.
- `InputPoll` is a borrowed view of the current and previous snapshots, so engine policy and the game callback detect edges against the same pair. It owns no state, allocates nothing, and must never outlive its `ProcessInput` call.
- `MenuInputFlags` is the generic menu vocabulary; game-specific actions poll their own keys through `InputPoll` instead. `kSingleStep` (from Tab), `kMouseIsDown`, `kMouseClick`, `kGamepadButton`, `MenuInput::f2Mouse`, and `MenuInput::f2Gamepad` are produced and intentionally unconsumed.

## Device and Frame Boundaries

- Win32 Raw Input owns keyboard state with `RIDEV_NOLEGACY`; window events write scratch state and `Update()` publishes one coherent snapshot per display frame. Suppressed legacy messages keep hardware keyboard input out of ImGui, so gamepad navigation and agent-injected ImGui events use separate explicit paths.
- Mouse state comes from DirectXTK's Win32 message path and gamepad state from XInput. The DirectXTK mouse object must exist before window creation, because synchronous window messages call its static processing path.
- Wheel state is a lifetime accumulator consumers diff against the previous snapshot's baseline, seeded on the first poll so the first frame reports no delta. Mouse normalization uses the Vulkan framebuffer extent and may briefly leave the 0..1 range during resize.
- Keyboard handling stays hand-rolled on that Raw Input path: do not unify it onto DirectXTK's `Keyboard`. ThirdParty (`../../../ThirdParty/AGENTS.md`) owns the rationale.

## Bindings and Mode

- Debug/profile/screenshot/debug-render keys compile in via `if constexpr` on `kbDebugInput` / `kbProfiling` / `kbScreenshots` / `kbDebugRender`, free-camera WASD on `kbFreeCamera`. New debug-only keys belong inside those blocks.
- Gamepad mode engages when either thumbstick's `|x| + |y|` exceeds the threshold; mouse movement or a KBM whitelist key (WASD, arrows, numpad 1/2/3/5, LMB/RMB) flips back. New movement keys must extend the whitelist or mode detection misses them.
- While a menu is up, gamepad buttons / d-pad / left stick are pushed into ImGui's IO so menus are pad-navigable. `GameBase` supplies menu visibility as a `BeginPoll` argument, so the engine never reads a game global to decide it.

## Scroll Ownership

- The wheel belongs to the UI when an ImGui key owner claims `ImGuiKey_MouseWheelY`, while the cursor hovers an ImGui window that can actually consume it (nonzero `ScrollMax.y`, wheel not disabled by window flags), or while `ImGuiContext::WheelingWindow` still holds an earlier target. Everything else, including an open but non-scrolling menu or modal, gives the delta to the camera. The test mirrors ImGui's own `UpdateMouseWheel` routing, reads `ImGuiContext` from the client-only `imgui_internal.h` view, and inspects ImGui state only after confirming a live context.
- Two accepted timing tolerances: hover is one frame stale (ImGui resolves it in `NewFrame`, after input polling), so a notch on the frame the cursor crosses a panel edge may be double-handled or lost; and ImPlot's `MouseWheelY` ownership stays visible for two input polls after leaving a plot, which may suppress an immediate background notch.
- The hovered-window test assumes no child windows. If a `BeginChild`, popup, combo, list box, or scrolling table is ever added, the gate must grow ImGui's `ParentWindow` bubble logic from `FindBestWheelingWindow`.
- `CompletePoll` always advances the wheel baseline, including when input is swallowed, so a suppressed notch cannot surface later as zoom applied after the fact.

## Focus and Agent Input

- Focus gain registers the keyboard and clears scratch state; focus loss unregisters it and freezes the published snapshot. Gamepad suspend/resume follows focus.
- An active agent script may publish synthetic input while unfocused; agent-port clients suppress physical keyboard, mouse, gamepad, and cursor trapping for the process lifetime, preserving window close handling.
- Cursor trapping uses Win32 `ClipCursor`, follows game policy while focused, and is forced off on focus loss or physical-input suppression.
- Synthetic key and mouse input overlays the published snapshot; synthetic ImGui mouse position is re-applied after the Win32 backend. Keep these sinks separate because their lifetimes differ.
- Gamepad construction may fail; polling and vibration paths tolerate a null gamepad object and clear the published pad state on disconnect.

## See Also

- Engine Agent (`../Agent/AGENTS.md`) - Synthetic input
- Game source (`../../../Projects/BrokenEngineSandbox/Source/AGENTS.md`) - Menu callback
- Game frame (`../../../Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`) - `FrameInput`
- `../../../Documents/Architecture/FrameUpdatePipeline.md` - Main-loop order
