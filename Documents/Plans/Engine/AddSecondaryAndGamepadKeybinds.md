<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-25T23:38:32.433Z","dependsOn":[]} -->
# Add Secondary Keyboard Slots and Gamepad Button Keybinds

## Context

During the Keybinds + re-mapping feature (originating session
`ab010ddd-8b2e-4b68-9bce-318949ec40b5`), single keyboard bindings per action
were implemented across the engine and game layers, and the user explicitly
deferred secondary slots and gamepad button remapping as a follow-up Plan
(`"secondary slots, and/or gamepad buttons" as follow-up plan`).

Current single-slot keyboard architecture:

- `Engine/Source/Input/Keybinds.h:11-37` defines `VirtualKey` (a 1-byte
  trivially copyable Win32 virtual-key wrapper where `0` means unbound).
- `Engine/Source/Input/Keybinds.h:42-84` defines `KeyAction`, `KeybindDomain`,
  `KeybindSlot`, and `KeybindRowInfo` (which holds a single `VirtualKey key`).
- `Engine/Source/Input/Keybinds.cpp:10-43`, `231-250`, and `292-347` store one
  `VirtualKey` per `KeyAction` in `spEngineKeybinds`, persist them in
  `EngineKeybinds.bin` via `EngineKeybindsFile` (`kiVersion = 1`), and validate
  uniqueness across `pKeys`.
- `Projects/BrokenEngineSandbox/Source/Input/GameKeybinds.h:10-30` and
  `GameKeybinds.cpp:8-50`, `72-132` mirror that single-slot table and
  `GameKeybinds.bin` (`GameKeybindsFile::kiVersion = 1`) for `GameKeyAction`.
- `Engine/Source/Input/RawInputManager.h:17-43` already polls an 8-bit
  `GamepadButtons` flag set (`kGamepadButtonA`, `kGamepadButtonB`,
  `kGamepadButtonX`, `kGamepadButtonY`, `kGamepadLeftShoulder`,
  `kGamepadRightShoulder`, `kGamepadStart`, `kGamepadMenu`) into
  `RawInput::gamepadButtons`.
- `Engine/Source/Input/Input.h:62-84` resolves `InputPoll::Pressed` and `Held`
  through ADL `KeybindFor(eAction)` into `PressedKey` / `HeldKey` and exposes
  `GamepadPressed`.
- `Engine/Source/Input/Input.cpp:99-187` resolves cross-domain startup
  conflicts, tracks one `KeybindSlot mCaptureSlot`, and steals conflicting keys
  in `RebindSlot`. `Input.cpp:215-255` scans only `pbKeyboardKeys[1..254]`
  during capture, and `Input.cpp:257-269` uses `IsActionRawDown` for KBM mode
  detection.
- `Engine/Source/GameBase.cpp:77-81` assigns
  `gpInput->mSuppressedChatOpenKey = KeybindFor(KeyAction::kOpenChat)`, and
  `Engine/Source/Main.cpp:644-661` matches `message.wParam` against
  `KeybindFor(KeyAction::kOpenChat)` before `TranslateMessage` to arm
  `mSuppressedChatOpenKey`, both assuming one keyboard key per action.
- `Engine/Source/Ui/Screens/KeybindsScreen.h:14-16` and
  `KeybindsScreen.cpp:21-110` render a 2-column `KeybindsTable` (action label
  and one bind button) with one hover-animation float per row.

## Design

### Binding slots, conflict resolution, and reserved gamepad buttons

- **Slots per action**: Every action holds three binding slots — primary
  keyboard, secondary keyboard, and one gamepad button — sharing one
  slot-indexed model, one UI layout pass, and a single `kiVersion` bump.
- **Conflict stealing**: Uniqueness is enforced per device type across both
  engine and game tables. Rebinding any keyboard slot steals from any primary or
  secondary keyboard slot in either domain; rebinding a gamepad slot steals from
  any gamepad slot in either domain, matching `Input::RebindSlot` behavior while
  letting keyboard and gamepad bindings coexist on the same action.
- **Reserved gamepad buttons**: `Input.cpp:319` uses `kGamepadMenu` and
  `kGamepadButtonB` for `kPauseMenu`, and `Input.cpp:353-355` forwards
  `kGamepadButtonA`, `kGamepadButtonB`, and `kGamepadMenu` to ImGui while a
  menu is visible. `IsBindableGamepadButton` disallows `kGamepadMenu` and
  `kGamepadButtonB` so pause and menu-back remain fixed like `VK_ESCAPE`, while
  allowing `A`, `X`, `Y`, shoulders, and `Start` (suppressed while menus are
  open or capture is active).

### Data model and persistence

- Add a 1-byte trivially copyable, standard-layout `GamepadButton` wrapper in
  `Engine/Source/Input/Keybinds.h` (`0` = unbound; nonzero = single bindable
  button code mapping to `GamepadButtons`), parallel to `VirtualKey`, along
  with `IsBindableGamepadButton` and zero-allocation
  `FormatGamepadButtonName(GamepadButton, std::span<char>)`.
- Define a fixed-size POD binding record per action
  (`VirtualKey pKeys[kiKeyboardSlotsPerAction]` with
  `kiKeyboardSlotsPerAction = 2`, plus `GamepadButton gamepadButton`) and
  extend `KeybindSlot` with a slot selector (primary keyboard, secondary
  keyboard, or gamepad) so capture and rebind APIs address any cell uniformly.
- Bump `EngineKeybindsFile::kiVersion` (`Engine/Source/Input/Keybinds.cpp:12`)
  and `GameKeybindsFile::kiVersion`
  (`Projects/BrokenEngineSandbox/Source/Input/GameKeybinds.cpp:10`) from `1` to
  `2`, and update their `static_assert` layout checks. Per repository
  no-backward-compatibility rules, do not migrate v1 files; v1 headers are
  rejected and fall back to defaults.
- Extend `LoadEngineKeybinds`, `LoadGameKeybinds`, and `Input::LoadAllKeybinds`
  to validate every keyboard and gamepad slot (valid bindable code, no
  duplicate keyboard keys across primary/secondary slots, no duplicate gamepad
  buttons) and resolve cross-domain startup conflicts per device type.

### Polling, capture, and chat-open suppression

- Update `KeybindFor` / `KeybindsFor` (ADL customization point) and
  `InputPoll::Pressed` / `Held` so an action fires when any of its bound
  keyboard slots or its bound gamepad button is pressed/held (respecting
  capture suppression and UI keyboard ownership for keyboard slots).
- Update `IsActionRawDown` (`Engine/Source/Input/Input.cpp:257-261`) to check
  all keyboard slots for camera movement without treating bound gamepad buttons
  as KBM input.
- Update `ProcessMessages` (`Engine/Source/Main.cpp:644-661`) to match
  `message.wParam` against every bound keyboard slot of `KeyAction::kOpenChat`
  before `TranslateMessage` and arm `gpInput->mSuppressedChatOpenKey` with that
  key, and update `GameBase::ProcessInput` (`Engine/Source/GameBase.cpp:77-81`)
  to set `gpInput->mSuppressedChatOpenKey` to whichever bound `VirtualKey`
  actually triggered `KeyAction::kOpenChat` (or `VirtualKey::Unbound()` if
  triggered by a gamepad button).
- Extend capture in `Input::BeginPoll` (`Engine/Source/Input/Input.cpp:215-255`)
  so keyboard slots capture bindable `VirtualKey` presses and gamepad slots
  capture bindable `GamepadButtons` rising edges; `VK_ESCAPE` (and
  `kGamepadButtonB` when capturing a gamepad slot) cancels, and
  `VK_BACK`/`VK_DELETE` unbinds the targeted slot.

### UI layout

- Expand `KeybindsTable` (`Engine/Source/Ui/Screens/KeybindsScreen.cpp:94-110`)
  to render one action label column plus three fixed-width button columns
  (primary keyboard, secondary keyboard, gamepad), with column headers and
  unique ImGui ID suffixes per cell (`##<id>_Primary`, `##<id>_Secondary`,
  `##<id>_Gamepad`).
- Size each bind-button column from fixed minimum widths and localized
  placeholder widths (`PRESS A KEY...` / `PRESS BUTTON` / `UNBOUND`) so
  clicking or rebinding never reflows columns
  (`Documents/UserInterfaceDesign.txt`).
- Expand `KeybindsScreen::mpfBindButtonHoverAnimations`
  (`Engine/Source/Ui/Screens/KeybindsScreen.h:16`) to hold one float per
  `(row, slot)` cell with no heap allocation, and keep the root-window height
  constraint without `BeginChild` so `Input`'s scroll gate stays intact.

## Critical files

- `Engine/Source/Input/Keybinds.h`
- `Engine/Source/Input/Keybinds.cpp`
- `Engine/Source/Input/Input.h`
- `Engine/Source/Input/Input.cpp`
- `Engine/Source/Input/RawInputManager.h`
- `Engine/Source/GameBase.cpp`
- `Engine/Source/Main.cpp`
- `Engine/Source/Ui/Screens/KeybindsScreen.h`
- `Engine/Source/Ui/Screens/KeybindsScreen.cpp`
- `Projects/BrokenEngineSandbox/Source/Input/GameKeybinds.h`
- `Projects/BrokenEngineSandbox/Source/Input/GameKeybinds.cpp`

## In scope

- Multi-slot action binding storage (`VirtualKey` primary/secondary and
  `GamepadButton`), slot-indexed `KeybindSlot`, `IsBindableGamepadButton`, and
  `FormatGamepadButtonName` in `Engine/Source/Input/Keybinds.h` and
  `Keybinds.cpp`.
- Matching game-side multi-slot storage, `DescribeKeybind`, and persistence in
  `Projects/BrokenEngineSandbox/Source/Input/GameKeybinds.h` and
  `GameKeybinds.cpp`.
- `EngineKeybindsFile::kiVersion` and `GameKeybindsFile::kiVersion` bump from
  `1` to `2` with full per-device duplicate validation and cross-domain
  resolution in `Input::LoadAllKeybinds`.
- Multi-slot `InputPoll::Pressed` / `Held`, `MoveAxis`, KBM-mode detection, and
  slot-typed capture/rebind in `Engine/Source/Input/Input.h` and `Input.cpp`.
- Trigger-key-accurate `mSuppressedChatOpenKey` matching and assignment in
  `Engine/Source/Main.cpp` (`ProcessMessages`, lines `644-661`) and
  `Engine/Source/GameBase.cpp`.
- Multi-column `KeybindsTable` and per-cell hover animation storage in
  `Engine/Source/Ui/Screens/KeybindsScreen.h` and `KeybindsScreen.cpp`.
- Synchronizing `Engine/Source/Input/AGENTS.md` and
  `Engine/Source/Ui/Screens/AGENTS.md` with the updated contracts.

## Out of scope

- Analog thumbstick, trigger, or D-pad axis remapping and deadzone tuning.
- Mouse button remapping.
- Modifier or chorded keybinds (such as Ctrl+key or Shift+key).
- Remapping fixed system inputs (`VK_ESCAPE`, `Alt+F4`) or compile-gated debug,
  profiling, and debug-render shortcuts.
- Backward-compatible loading or migration of v1 `EngineKeybinds.bin` or
  `GameKeybinds.bin` files.
- Adding new gameplay actions or unit tests.

## Risk tier and invariants

- **Change Workflow Tier 3** — triggers:
  - Serialization and data layout change: bumps `EngineKeybindsFile::kiVersion`
    and `GameKeybindsFile::kiVersion` from `1` to `2`.
  - Cross-subsystem integration: spans independently owned Engine Input, Engine
    UI Screens, Engine `GameBase`, and Game Input.
- **Determinism / CRC / wire / replay**: None. All keybind storage, capture,
  and UI are `BT_CLIENT`-only local input; `FrameInput` and `FramePostRender`
  CRC are unaffected.
- **Affinity**: Keep all keybind and screen code under `BT_CLIENT` guards so
  server targets remain unaffected.
- **Allocation**: No main-loop heap allocations; keep fixed-buffer formatting
  and fixed-array hover animations, and keep `ScopedSuppressAllocationTracking`
  around binary file I/O.

## Acceptance criteria

- `BrokenEngineSandbox` client and server build cleanly in Debug and Release
  (`x64`).
- Live `/agent-harness` verification (no prior multi-slot/gamepad harness run
  exists; design the harness check during implementation):
  - Opening `KEYBINDS` displays the primary keyboard, secondary keyboard, and
    gamepad slot columns with unique button IDs and fixed column widths that do
    not shift when entering capture or rebinding.
  - Rebinding a primary or secondary keyboard slot updates the label, steals
    conflicts across primary/secondary slots and engine/game domains, triggers
    the action from either slot, and persists across restart in v2 `.bin`
    files.
  - Triggering `kOpenChat` via a secondary keyboard binding opens chat without
    leaking that key's character into the input box.
  - Clicking `RESET TO DEFAULT` restores all slots across both domains and
    saves on `Back`.
- Rejection of v1 `EngineKeybinds.bin` / `GameKeybinds.bin` files and duplicate
  or unbindable codes is verified by code reading and layout `static_assert`s.
- Gamepad button capture, unbind, conflict stealing, and action triggering (if
  unexercised by existing synthetic harness commands) are verified by code
  reading and build.

## Notes

- Provenance: Deferred during Keybinds + re-mapping design in session
  `ab010ddd-8b2e-4b68-9bce-318949ec40b5`
  (`"secondary slots, and/or gamepad buttons" as follow-up plan`).
