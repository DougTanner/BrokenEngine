<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-26T13:43:24.142Z","dependsOn":[]} -->
# Separate Fixed Keys From Bindable Keys

## Context

A post-landing quality audit of commit `81c17f92c` (which squashed the
Observer-camera commit `a74043df5` and the keybind-remapping commit
`d420d2071`) found that two hard-coded keyboard reads use keys the keybind
allowlist also accepts, so a player can bind an action to a key that already
fires a fixed action.

Evidence at baseline `804f4cf2d`:

- `Projects/BrokenEngineSandbox/Source/Game.cpp:238`
  (`Game::ProcessGameMenuInput`) toggles gameplay Observer mode on
  `rInputPoll.KeyboardPressed(VK_OEM_3)` (backtick), gated on gameplay,
  `UiState::kNone`, and `!MapOpen()`. It runs right after the
  `GameKeyAction::kToggleMap` branch at lines 225-236.
- `Engine/Source/Input/Input.cpp:339-354` reads `VK_UP`, `VK_DOWN`, `VK_LEFT`,
  and `VK_RIGHT` from `rRawInput.pbKeyboardKeys` into Observer pan `f2Move`.
- `Engine/Source/Input/Keybinds.cpp:90-133` (`IsBindableKey`) accepts the range
  `VK_OEM_1..VK_OEM_3` (line 99, so backtick) and the `VK_LEFT`, `VK_UP`,
  `VK_RIGHT`, and `VK_DOWN` cases (lines 117-120). `IsBindableKey` is the
  single allowlist for live capture (`Input.cpp:248`) and for file validation
  (`Keybinds.cpp:327`, `Projects/BrokenEngineSandbox/Source/Input/GameKeybinds.cpp:107`).

Observed consequence, from the code order: with Toggle Map bound to backtick, a
press that closes the map also toggles Observer mode, because `ToggleMap`
releases the framing override synchronously and the Observer check then sees
`!MapOpen()`. With any action bound to an arrow key, the same key also pans the
camera in Observer mode. The fixed-input rule in `Engine/Source/Input/AGENTS.md`
`## Bindings and Mode` lists only Esc and Alt+F4 as fixed inputs outside the
binding tables.

## Design

The author recommends the audit's smallest correction:

1. Make the Observer toggle a remappable game action. Add
   `GameKeyAction::kToggleObserver` with default `VK_OEM_3`, so the default
   behavior is unchanged and the key takes part in duplicate rejection and
   `Input::RebindSlot` unbinding. `Game::ProcessGameMenuInput` polls it through
   `rInputPoll.Pressed(GameKeyAction::kToggleObserver)` with the existing
   gameplay, `UiState::kNone`, and `!MapOpen()` gates. `InputPoll::Pressed`
   applies the same UI-keyboard-ownership suppression `KeyboardPressed` applies
   to a non-Tab key, so no gate changes.
2. Make arrow keys fixed: remove the `VK_LEFT`, `VK_UP`, `VK_RIGHT`, and
   `VK_DOWN` cases from `IsBindableKey`. No default binding uses an arrow key
   (`kpDefaultEngineKeybinds`, `kpDefaultGameKeybinds`). A saved file that
   binds an arrow is then rejected to defaults by the existing loaders; the
   engine file layout is unchanged, so `EngineKeybindsFile::kiVersion` stays.
3. Adding an action grows `GameKeybindsFile`, so bump
   `GameKeybindsFile::kiVersion`, as the root `AGENTS.md`
   no-backward-compatibility directive requires; older files are rejected to
   defaults. The size `static_assert` tracks `GameKeyAction::kCount` and does
   not enforce the bump.
4. `DescribeKeybind(GameKeyAction)` currently returns the `KeybindToggleMap`
   id and `TOGGLE MAP` label for every action; give each visible action its own
   id and label (the author suggests `KeybindToggleObserver` and
   `TOGGLE OBSERVER`), and list the new action in `VisibleGameKeyActions`.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Game.cpp`
- `Projects/BrokenEngineSandbox/Source/Input/GameKeybinds.h`
- `Projects/BrokenEngineSandbox/Source/Input/GameKeybinds.cpp`
- `Projects/BrokenEngineSandbox/Source/AGENTS.md`
- `Engine/Source/Input/Keybinds.cpp`
- `Engine/Source/Input/AGENTS.md`

## In scope

- `GameKeyAction` (new `kToggleObserver`) in `GameKeybinds.h`, and in
  `GameKeybinds.cpp`: `GameKeybindsFile::kiVersion`, `kpDefaultGameKeybinds`,
  `spGameKeybinds`, `VisibleGameKeyActions`, and `DescribeKeybind`.
- The Observer toggle branch of `Game::ProcessGameMenuInput` in `Game.cpp`
  (line 238).
- The arrow-key cases of `IsBindableKey` in `Keybinds.cpp`.
- `Engine/Source/Input/AGENTS.md` `## Bindings and Mode` (arrows join the fixed
  inputs) and the `Game::ProcessGameMenuInput` bullet of
  `Projects/BrokenEngineSandbox/Source/AGENTS.md` `## Ownership` (Observer
  toggle becomes a remappable `GameKeyAction`).

## Out of scope

- The Observer arrow-pan reads in `Input.cpp`, the KBM-mode whitelist, and
  edge-scroll.
- Collisions between bindable keys and compile-gated debug, profiling,
  screenshot, debug-render, or free-camera shortcuts (`kbDebugInput` and
  siblings), which `Engine/Source/Input/AGENTS.md` keeps outside the tables.
- Engine `KeyAction`, `EngineKeybindsFile` layout, and its version.
- Agent harness key-name mapping (`AgentCommandsClientGeneric.cpp`).
- Secondary or gamepad slots (owned by
  `Documents/Plans/Engine/AddSecondaryAndGamepadKeybinds.md`).

## Acceptance criteria

- Keybind capture refuses the four arrow keys, and a keybinds file holding an
  arrow key is rejected to defaults.
- The Keybinds screen lists a separate Observer toggle row bound to backtick by
  default; rebinding Toggle Map to backtick unbinds the Observer toggle, and a
  single backtick press then only toggles the map.
- With default bindings, backtick still toggles Observer mode in gameplay with
  the map closed and no UI open.
- Client and server builds succeed. The supplied audit evidence records no
  harness command sequence; live verification uses `/agent-harness` UI input on
  the Keybinds screen.

## Notes

- Change Workflow risk tier: Tier 3. Trigger: serialization or data layout
  change (`GameKeybindsFile` grows and `GameKeybindsFile::kiVersion` bumps), and
  the change spans independently owned Engine Input and Game Input.
- Determinism, CRC, wire, replay, `.pack`: none; all keybind code is
  `BT_CLIENT`-only and `FrameInput` is untouched.
- Affinity: keep every change under the existing `BT_CLIENT` guards.
- `Documents/Plans/Engine/AddSecondaryAndGamepadKeybinds.md` also bumps
  `GameKeybindsFile::kiVersion` and rewrites `GameKeybinds.*`; whichever lands
  second takes the next version and carries the other's action list. This is
  minor file overlap between independent Plans, not a dependency.
