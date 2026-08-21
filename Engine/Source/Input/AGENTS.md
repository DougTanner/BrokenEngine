# Input - Hardware Snapshot and Display Input

`RawInputManager` publishes one coherent `RawInput` snapshot per display frame; `Input` turns that snapshot into menu commands and camera input. Both are client-only in practice: every consumer compiles only in the client build, and `Input.h`/`Input.cpp` carry whole-file `BT_CLIENT` guards and appear only in the client vcxproj. The deterministic per-tick `FrameInput` type is game-owned and lives in game `Source/Frame`, unrelated to this directory.

Global: `gpInput` — constructed in `Main.cpp` under `BT_CLIENT`, polled only from `engine::GameBase::ProcessInput`. `BeginPoll` fills `engine::MenuInput` and writes straight into `engine::gpCamera->mCameraInput`, which the engine camera update reads. Consumers that need the complete types include `Input/Input.h` directly; the include is inert in a server-build translation unit because of the header's whole-file guard.

## Poll Lifetime

- One `ProcessInput` call is one poll: `BeginPoll` publishes the snapshot and produces this frame's values, and `CompletePoll` copies the current snapshot into the previous one. `CompletePoll` runs after every edge consumer, including the game callback, and also on the modal path that skips the callback — moving it earlier silently breaks edge detection for whatever still has to run.
- `InputPoll` is a borrowed view of the current and previous snapshots, so engine policy and the game callback detect edges against the same pair. It owns no state, allocates nothing, and must never outlive the `ProcessInput` call that handed it out.
- `MenuInputFlags` is the generic menu vocabulary. Game-specific actions are not flags: the game callback polls its own keys through `InputPoll`. `kSingleStep` is produced from Tab and deliberately has no consumer; `kMouseIsDown`, `kMouseClick`, `kGamepadButton`, `MenuInput::f2Mouse`, and `MenuInput::f2Gamepad` are likewise produced and intentionally unconsumed.

## Device and Frame Boundaries

- Win32 Raw Input owns keyboard state with `RIDEV_NOLEGACY`; window events write scratch state and `Update()` publishes one coherent snapshot per display frame. Mouse state comes from DirectXTK's Win32 message path, and gamepad state is polled from XInput.
- Because legacy keyboard messages are suppressed, hardware keyboard input does not feed ImGui. Gamepad navigation and agent-injected ImGui events use separate explicit paths.
- Mouse wheel state is a lifetime accumulator that consumers diff. The previous snapshot carries that baseline, seeded on the first poll so the first frame reports no delta. Mouse normalization uses the Vulkan framebuffer extent and may briefly leave the 0..1 range during resize.
- The DirectXTK mouse object must exist before window creation because synchronous window messages call its static processing path.
- Keyboard handling is hand-rolled on purpose rather than routed through DirectXTK's `Keyboard`. Adopting it would give up `RIDEV_NOLEGACY`, so Alt+F4 would arrive as an ordinary window-close message and bypass the game quit binding; it would let ImGui swallow keyboard messages; and it would break bindings that test the generic `VK_MENU`/`VK_SHIFT`/`VK_CONTROL` keys, because DirectXTK reports only the left/right-specific ones. Do not unify the keyboard onto DirectXTK to match the mouse and gamepad paths, and do not add `Keyboard.cpp` to the compiled DirectXTK units.

## Bindings and Mode

- Debug-gated bindings: debug/profile/screenshot/debug-render keys compile in via `if constexpr` on `kbDebugInput` / `kbProfiling` / `kbScreenshots` / `kbDebugRender`; free-camera WASD gates on `kbFreeCamera`. New debug-only keys belong inside those blocks.
- Mode auto-switch: gamepad engages when either thumbstick's `|x| + |y|` exceeds the threshold; mouse movement or a key in the KBM whitelist (WASD, arrows, numpad 1/2/3/5, LMB/RMB) flips back. New movement keys must extend the whitelist or mode detection misses them.
- ImGui gamepad feed: while a menu is up, gamepad buttons / d-pad / left stick are pushed into ImGui's IO so menus are pad-navigable. `GameBase` supplies menu visibility as a `BeginPoll` argument, so the engine never reads a game global to decide it.

## Scroll Ownership

The wheel belongs to the UI when an ImGui key owner claims `ImGuiKey_MouseWheelY`, while the cursor hovers an ImGui window that can actually consume it — nonzero `ScrollMax.y` and wheel input not disabled by window flags — or while ImGui's wheeling lock (`ImGuiContext::WheelingWindow`) still holds an earlier target. Everything else, including an open but non-scrolling menu or modal, gives the delta to the camera, so zoom keeps working over any menu that does not scroll. The test mirrors ImGui's own `UpdateMouseWheel` routing and reads `ImGuiContext` from the client-only `imgui_internal.h` view, so a notch is never both scrolled and zoomed outside the one-frame boundary tolerance below. Two timing constraints come with it: hover is one frame stale (ImGui resolves it in `NewFrame`, after input polling), an accepted tolerance meaning a notch on the frame the cursor crosses a panel edge may be double-handled or lost; and ImPlot's `MouseWheelY` ownership remains visible for the first two input polls after leaving a plot, with the following poll first seeing no owner. This conservative departure behavior may suppress an immediate background notch. A coordinate mouse move runs two active frames and, once complete, establishes the new target; a direct coordinate-bearing background wheel immediately after plot hover may remain UI-owned, while a subsequent notch after that command completes reaches the camera. The hovered-window test assumes no child windows, which holds today — if a `BeginChild`, popup, combo, list box, or scrolling table is ever added, the gate must grow ImGui's `ParentWindow` bubble logic from `FindBestWheelingWindow`. `CompletePoll` always advances the baseline for the running lifetime total, including when input is swallowed, so it cannot surface later as zoom applied after the fact, and ImGui state is inspected only after confirming a live context.

## Focus and Agent Input

- Focus gain registers the keyboard and clears scratch state; focus loss unregisters it and freezes the published snapshot. Gamepad suspend/resume follows focus.
- An active agent script may publish synthetic input while unfocused. Agent-port clients suppress physical keyboard, mouse, gamepad, and cursor trapping for the process lifetime while preserving window close handling.
- Synthetic key and mouse input overlays the published snapshot; synthetic ImGui mouse position is re-applied after the Win32 backend. Keep these sinks separate because their lifetimes differ.
- Cursor trapping uses Win32 `ClipCursor`, follows game policy while focused, and is forced off on focus loss or physical-input suppression.
- Gamepad construction may fail; all polling and vibration paths support a null gamepad object and clear the published pad state on disconnect.

## See Also

- Engine Agent (`../Agent/AGENTS.md`) - Synthetic input ownership
- Game source (`../../../Projects/BrokenEngineSandbox/Source/AGENTS.md`) - Game-specific menu callback
- Game frame (`../../../Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`) - `FrameInput` deterministic serialization
- `../../../Documents/Architecture/FrameUpdatePipeline.md` - Client main-loop order
