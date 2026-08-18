<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T17:51:59.745Z","dependsOn":["Documents/Plans/Frame/FrameInputSourceSplit.md","Documents/Plans/Graphics/CameraOwnershipToEngine.md"]} -->
# Move display input ownership to Engine

## Context

Baseline: `12a682dad9ad483fe9b6098a0dd6576d62ba0af2`.

This Plan executes the selected D9 design C after both prerequisites have landed:

- `Documents/Plans/Frame/FrameInputSourceSplit.md`, which moves shared deterministic `FrameInput` out of the old game input files and leaves those files client-only;
- `Documents/Plans/Graphics/CameraOwnershipToEngine.md`, which exposes the engine-owned client `engine::Camera::mCameraInput` display-input slot in `Engine/Source/Graphics/EngineCamera.h` and migrates camera readers away from `game::gpInput`.

At baseline, display input is game-owned:

- `Projects/BrokenEngineSandbox/Source/Input/Input.h` owns `MenuInputFlags`, `MenuInput`, `CameraInput`, the display `Input` class, and `game::gpInput`.
- `Projects/BrokenEngineSandbox/Source/Input/Input.cpp:12-153` updates raw input, detects edges, fills menu/camera input, feeds ImGui gamepad navigation, and arbitrates wheel ownership.
- `Engine/Source/GameBase.cpp:34-39` calls `game::gpInput` and delegates all menu policy to `Game::ProcessMenuInput`.
- `Projects/BrokenEngineSandbox/Source/Game.cpp:570-650` mixes generic engine UI behavior with game-specific behavior.
- `Projects/BrokenEngineSandbox/Source/Game.cpp:713-852` handles generic menu flags and game-specific reset/connect/island actions together.
- `Engine/Source/Main.cpp:269-270,389-390` constructs `game::Input` and `game::MenuInput`.

After D9A, the old game Input files remain in the client project only. D9B removes them from the client, adds the new engine display-input files to the client project only, and leaves the server without any display-input translation unit.

## Selected design C

### Engine display-input interface

Add whole-file client-only `Engine/Source/Input/Input.h` and `Input.cpp`.

`Input.h` owns the retained generic menu input types and a borrowed polling view:

```text
namespace engine
{
enum class MenuInputFlags : uint64_t
{
    // Keep every retained enumerator and its current numeric value.
    // Remove only kResetFrame, kConnectLocal, and kCycleMenuIsland.
    // kSingleStep remains engine-typed and bound to VK_TAB.
};
using MenuInputFlags_t = common::Flags<MenuInputFlags>;

struct MenuInput
{
    bool bGamepad = false;
    MenuInputFlags_t flags {};
    XMFLOAT2 f2Mouse {};
    XMFLOAT2 f2Gamepad {};
};

class InputPoll
{
public:
    bool KeyboardPressed(int64_t iKey) const;
    bool MousePressed(MouseButtons eButton) const;
    bool GamepadPressed(GamepadButtons eButton) const;

private:
    friend class Input;
    InputPoll(const RawInput& rCurrent, const RawInput& rPrevious);
    const RawInput& mrCurrent;
    const RawInput& mrPrevious;
};

class Input
{
public:
    InputPoll BeginPoll(bool bLostFocus, bool bMenuVisible, MenuInput& rMenuInput);
    void CompletePoll();

private:
    enum class InputStateFlags : uint8_t
    {
        kScrollWheelInitialized = 1 << 0,
        kGamepadMode            = 1 << 1,
    };

    RawInput mPreviousRawInput {};
    common::Flags<InputStateFlags> mStateFlags;
};

inline Input* gpInput = nullptr;
}
```

`InputPoll` is a borrowed view. It owns no input state, allocates nothing, and is valid only between `BeginPoll` and `CompletePoll` during the current `GameBase::ProcessInput` call. The game callback may use its edge helpers but must not store it.

`BeginPoll` will:

1. call `gpRawInputManager->Update(bLostFocus)` exactly once;
2. use the current raw snapshot and `mPreviousRawInput` for all retained edge checks;
3. fill every retained `engine::MenuInput` field and generic flag;
4. write free-camera movement and wheel delta to D8's `gpCamera->mCameraInput`;
5. preserve the existing gamepad-mode threshold and keyboard/mouse whitelist;
6. preserve scroll baseline initialization and always advance the wheel baseline, including when ImGui owns the wheel;
7. preserve the existing ImGui wheel-owner, hovered-window, wheeling-lock, and live-context checks;
8. feed ImGui gamepad navigation only when the `bMenuVisible` value supplied by `GameBase` is true, preserving pre-policy UI-state timing without requiring a game global.

The retained generic `MenuInputFlags` keep their current values. Only `kResetFrame`, `kConnectLocal`, and `kCycleMenuIsland` leave the shared enum. `kSingleStep` remains engine-typed, is still produced from `VK_TAB` under `kbDebugInput`, and remains intentionally unconsumed exactly as at baseline. No packet, game flag, or new behavior is added for it.

`CompletePoll` unconditionally copies the current `gpRawInputManager->mRawInput` into `mPreviousRawInput`. It is called after the game callback and is also called when modal gating skips the callback. First-poll wheel seeding remains equivalent to the existing behavior: initialize only the previous raw snapshot's wheel field before edge helpers run, so a held first-frame key retains the existing edge behavior.

### GameBase policy and ordering

Change `Engine/Source/GameBase.h` to own the engine-typed menu interface. Its client-facing declaration area forward-declares `engine::MenuInput` and `engine::InputPoll` (and removes the old `game::MenuInput` forward declaration); the complete definitions remain in the direct `GameBase.cpp` include and the client aggregation. This keeps the public header independent of PCH include order while the implementation can call the polling methods.

```text
virtual void ProcessGameMenuInput(
    const engine::MenuInput& rMenuInput,
    const engine::InputPoll& rInputPoll) = 0;

void ProcessInput(bool bLostFocus, engine::MenuInput& rMenuInput);
```

`GameBase::ProcessInput` has this fixed order:

1. `gpInput->BeginPoll(bLostFocus, meUiState != UiState::kNone, rMenuInput)`;
2. resolve quit, including the existing main-menu/pause Escape special case, using engine `mGameFlags`;
3. apply the modal gate;
4. when not modal, update cursor visibility;
5. handle pause/back-out transitions with the existing `UiState` cases;
6. handle engine-owned toggles;
7. call `ProcessGameMenuInput(rMenuInput, inputPoll)` last;
8. call `gpInput->CompletePoll()` on every path, including the modal path.

The modal gate skips cursor, pause/back-out, engine toggles, and the game callback exactly as the existing early return did, but it must not skip `CompletePoll`.

Move these engine-owned actions from `Game::ProcessMenuInput`/`ProcessDebugInput` into the engine portion of `GameBase::ProcessInput`, preserving all existing compile-time guards and behavior:

- fullscreen toggle;
- profile text toggle;
- debug-render toggle;
- ImGui/tweaks visibility toggle;
- debug-texture toggle and next/previous selection;
- screenshot continuous-capture toggle and request mailbox behavior;
- local pause-frame state toggle and debug text update;
- `mTimeStep.mbTimeScaleChanged` display-text update.

The generic save/replay/time flags remain members of `engine::MenuInputFlags`:

- `kQuicksave`;
- `kQuickload`;
- `kSaveReplay`;
- `kLoadReplay`;
- `kSlowTime`;
- `kSpeedUpTime`;
- `kTogglePauseFrame`.

Their game-specific network transport remains in the final game callback. The base changes local engine pause state before the callback, so the callback's pause packet observes the updated state. `kSingleStep` is not handled by either the base or the callback.

### Game-specific callback

Declare `GameMenuInputFlags` in the client portion of `Projects/BrokenEngineSandbox/Source/Game.h`:

```text
enum class GameMenuInputFlags : uint8_t
{
    kResetFrame      = 1 << 0,
    kConnectLocal    = 1 << 1,
    kCycleMenuIsland = 1 << 2,
};
using GameMenuInputFlags_t = common::Flags<GameMenuInputFlags>;
```

Replace `ProcessMenuInput` with `ProcessGameMenuInput`.

Inside the final callback:

- poll `VK_RETURN` once through `InputPoll::KeyboardPressed` and set both `kResetFrame` and `kConnectLocal`, preserving the existing dual Return action;
- poll `E` for `kCycleMenuIsland`;
- keep all three game flags inside the existing `if constexpr (kbDebugInput)` guard;
- send the existing quicksave, quickload, replay, slow-time, speed-up-time, pause, and reset packets through the existing game network objects;
- preserve the existing `InMainMenu()` gates for local connection and menu-island cycling;
- preserve island placement, elevation-cache reset, render-query reset, and texture-slot acquisition behavior;
- preserve all existing null checks, packet channels, reliability flags, and payload values;
- do not consume `kSingleStep` or create a packet for it.

Delete the old `ProcessDebugInput` helper after its generic engine-owned portions have moved to `GameBase`. Do not add packet types, wire fields, shared menu bits, or compatibility aliases.

### Build and documentation wiring

- Add `Engine/Source/Input/Input.h/.cpp` only to the client project and client filter.
- Remove `Projects/BrokenEngineSandbox/Source/Input/Input.h/.cpp` from the client project and client filter; D9A already removed them from the server project and server filter.
- Remove the now-empty game input filter entries.
- Add `Input/Input.h` to the client-only input span of `Engine/Source/Engine.h`.
- Update `Engine/Source/GameBase.h/.cpp` and `Engine/Source/Main.cpp` to use engine-qualified input types and `engine::gpInput`.
- Update `Projects/BrokenEngineSandbox/Source/Game.h/.cpp` to use `engine::MenuInput`, `engine::InputPoll`, and the independent game flags.
- Retain direct `#include "Input/Input.h"` in `Main.cpp` and `Game.cpp`, now resolving the engine header; retain the existing direct include in `GameBase.cpp`.
- Verify `Projects/BrokenEngineSandbox/Source/Graphics/Camera.cpp` has no old game-input include or `game::gpInput` reader; D8 owns that completed migration to `engine::gpCamera->mCameraInput`, so this Plan must not re-edit the camera implementation.
- Update the engine and game input ownership notes.
- Update `Documents/Architecture/FrameUpdatePipeline.md` to depict `BeginPoll → engine policy → game callback → CompletePoll → ClientUpdate`.
- Leave `Documents/Architecture/GameReconciliation.md` unchanged: deterministic `FrameInput` and reconciliation flow are not altered.

## Critical files

- `Engine/Source/Input/RawInputManager.h/.cpp` — read-only raw snapshot producer and shared `RawInput` definitions.
- `Engine/Source/Input/Input.h` — new client-only engine interface.
- `Engine/Source/Input/Input.cpp` — moved polling, edge, wheel, camera, and ImGui behavior.
- `Engine/Source/Engine.h:73-78` — client-only aggregation.
- `Engine/Source/GameBase.h:13-16,157-164,197-249` — input interface, callback, UI state, flags, and menu state.
- `Engine/Source/GameBase.cpp:1-13,34-39` — direct include and ordered polling/policy implementation.
- `Engine/Source/Main.cpp:1-10,265-270,389-390` — direct include, engine input construction, and menu-input type.
- `Projects/BrokenEngineSandbox/Source/Game.h:62-64,197-198` — callback and game-specific flags.
- `Projects/BrokenEngineSandbox/Source/Game.cpp:1-8,570-650` — old mixed menu policy to split.
- `Projects/BrokenEngineSandbox/Source/Game.cpp:713-852` — generic/game debug actions to split.
- `Projects/BrokenEngineSandbox/Source/Graphics/Camera.cpp` — read-only verification that D8 removed the old game-input include and reader.
- `Engine/Source/Graphics/EngineCamera.h` — D8 prerequisite contract, read-only here.
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj` — add engine Input, remove game Input.
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj` — verify no display-input source remains after D9A.
- Both `.vcxproj.filters` files — maintain client-only engine affinity and no game Input filter.
- `Engine/Source/Input/AGENTS.md` — engine snapshot/poll ownership.
- `Projects/BrokenEngineSandbox/Source/Input/AGENTS.md` — game callback and independent game flags.
- `Documents/Architecture/FrameUpdatePipeline.md` — client main-loop order.

## In scope

- Add the client-only engine `Input` and `InputPoll` interface and implementation.
- Move display polling, edge detection, camera-input filling, wheel arbitration, and ImGui gamepad feed into engine `Input`, preserving behavior.
- Replace the game-owned previous snapshot with one engine-owned `RawInput mPreviousRawInput`.
- Implement `BeginPoll` and `CompletePoll` with the exact lifetime and modal-capture contract above.
- Move generic menu policy and engine-owned toggles into `GameBase::ProcessInput` in the fixed order above.
- Change the game callback to `ProcessGameMenuInput(const engine::MenuInput&, const engine::InputPoll&)`.
- Add independent game flags for reset, local connect, and menu-island cycling.
- Preserve both Return actions, all bindings, all compile-time guards, wheel behavior, ImGui behavior, packet transport, and allocation behavior.
- Preserve `kSingleStep` as an engine-typed, Tab-bound, intentionally unconsumed baseline flag.
- Delete the old game `Input.h/.cpp` from the client and reconcile both project/filter files.
- Retain direct engine-input includes in Main/GameBase/Game and verify D8 already removed the obsolete Camera include; do not edit the camera implementation in this Plan.
- Update the named ownership and frame-pipeline documentation.

## Out of scope

- Any change to `RawInputManager` hardware acquisition, focus registration, synthetic-input overlay, or agent transport.
- Any change to D8 camera ownership or camera smoothing; D8's `gpCamera->mCameraInput` is an input sink consumed here.
- Any change to `FrameInput`, `StatusChange`, CRCs, stream bytes, replay compatibility, frame versions, or network protocol layouts.
- Any new shared high-bit game flag reservation, input serialization, callback registry, template, type erasure, or backward-compatibility alias.
- Any new behavior for `kSingleStep`; it remains produced by Tab and intentionally unconsumed.
- Any change to UI screen composition beyond the existing pause/modal/cursor policy order.
- Any server-side display input or raw polling.
- Any synthetic or manual gamepad verification requirement; gamepad preservation is source/static/client-build verified.
- Any new unit tests or test files.

## Risk tier and invariants

Tier 3: this change crosses engine/game ownership, changes client-only project affinity, changes a main-loop callback contract, and exposes ordering/lifetime requirements across UI, camera, raw input, and game transport.

Preserve these invariants:

- Only client builds compile engine `Input` and `Input.cpp`.
- The server project contains neither old game display-input sources nor engine display-input sources.
- `RawInputManager::Update` runs once per client display frame.
- Menu and camera values derive from the same published raw snapshot.
- `InputPoll` borrows current and previous snapshots and cannot outlive `ProcessInput`.
- Previous-snapshot capture occurs after every edge consumer, including the game callback.
- Modal frames still complete snapshot capture.
- Quit resolution precedes modal gating.
- Settings Escape backs out, while main-menu Escape from the pause state quits.
- Cursor handling precedes pause/back-out, which precedes engine toggles, which precedes the game callback.
- Generic save/replay/time flags remain engine-typed, but their existing game packet transport runs in the final callback.
- `VK_RETURN` independently triggers both reset and local-connect game flags under `kbDebugInput`.
- `kSingleStep` remains engine-typed, bound to `VK_TAB`, and unconsumed.
- Existing key bindings, mode-switch whitelist, debug/profile/screenshot/free-camera guards, wheel ownership, and scroll-baseline advancement remain unchanged.
- Existing gamepad polling and ImGui mapping remain source-equivalent; no runtime hardware assumption is added.
- `gpCamera->mCameraInput` is filled before render and read by the camera on the same main thread.
- No new main-loop heap allocation is introduced.
- No deterministic simulation or CRC input path is changed.

## Coordination

This Plan is blocked until both metadata dependencies have landed:

- `Documents/Plans/Frame/FrameInputSourceSplit.md` must leave the old game Input files client-only and the shared FrameInput files in both projects.
- `Documents/Plans/Graphics/CameraOwnershipToEngine.md` must provide `engine::gpCamera->mCameraInput` and migrate camera readers away from `game::gpInput`.

D9B removes the client-retained game Input files only after D9A has landed. It consumes the D8 camera member and does not edit D8-owned camera implementation.

Do not implement this Plan concurrently with either prerequisite. After D9B lands, all display-input consumers must use engine `Input`/`MenuInput` or the game callback; no `game::Input`, `game::gpInput`, or old game-input compatibility path remains.

## Acceptance criteria

- Source inventory finds no compiled game `Input.h/.cpp`, no `game::Input`, no `game::gpInput`, and no stale game `MenuInputFlags` references. The only display-input manager is `engine::Input`.
- `InputPoll` exposes only borrowed current/previous edge helpers; `BeginPoll` fills engine `MenuInput` and D8 camera input; `CompletePoll` copies the snapshot after the callback on every path.
- Static ordering review of `GameBase::ProcessInput` finds exactly `BeginPoll → quit → modal gate → cursor → pause/backout → engine toggles → ProcessGameMenuInput → CompletePoll`.
- Both client and server standard `/compile` builds succeed. The server build proves no engine display-input translation unit is linked.
- A Debug client `/agent-harness` scenario covers keyboard, mouse, and wheel behavior only: Escape backs out of a settings screen and quits from the main-menu pause state; modal input does not run policy or game handling while the following poll detects a newly pressed key; one Return press reaches both independent reset and local-connect branches without repeat-on-hold behavior; and wheel input remains UI-owned over a scrolling ImGui target and camera-owned over a non-scrolling panel/background.
- No manual hardware or synthetic gamepad setup is required. Source comparison of the moved body proves preservation of gamepad polling and ImGui mapping for A/B/Menu, d-pad, and left-stick events; client compilation/static inspection provides the independent signal.
- Static inspection proves `kSingleStep` remains engine-typed, is bound to `VK_TAB` under `kbDebugInput`, has no consumer, and introduces no packet or behavior.
- The exercised keyboard/mouse/wheel scenario completes without allocation-tracker breaks.
- Existing generic packet channels, reliability flags, payloads, debug guards, and camera zoom/free-camera behavior remain source- and runtime-equivalent.
- Project/filter validation reports engine Input as client-only, no old game Input files in either project, and no obsolete Camera include.
- Updated ownership and pipeline documentation matches the implementation.
- `plan validate` reports valid metadata and both dependency edges when this body is wrapped with its plan metadata.
- No unit tests are added.

## Execution card

### What does this plan do?

It moves display-rate input ownership from the game layer to a client-only engine `Input`, with one borrowed `InputPoll` view spanning engine and game edge detection. `GameBase` gains the fixed menu-policy order and the game receives only a final callback for game-specific flags and transport, while raw-input, wheel, ImGui, camera, and binding behavior remain intact.

### Why this is good for the codebase

The engine no longer depends on a game-owned display-input singleton, and a second game can supply only its game-specific menu callback and flags. Snapshot lifetime and policy order become explicit, preventing duplicate previous-state tracking, modal edge bugs, and accidental game handling before engine Escape/cursor/pause policy.

- Goal: Make client display input engine-owned while preserving current observable input, camera, ImGui, transport, allocation, and `kSingleStep` behavior.
- Out of scope: RawInputManager, D8 camera implementation, deterministic FrameInput/replay/CRC, server display input, UI redesign, protocol changes, synthetic/manual gamepad verification, and tests.
- Tier trigger: Tier 3 because the change crosses engine/game ownership, client/server project affinity, main-loop ordering, and borrowed snapshot lifetime.
- Interfaces and invariants: `engine::Input::BeginPoll`, `engine::Input::CompletePoll`, borrowed `engine::InputPoll`, engine `MenuInput`, `GameBase::ProcessGameMenuInput`, independent game flags, exact policy order, one previous raw snapshot, no server Input, and intentionally unconsumed engine `kSingleStep`.
- Acceptance checks:
  - Source/project inventory → old game Input and `game::gpInput` absent; engine Input client-only.
  - Interface/order review → exact BeginPoll/callback/CompletePoll sequence and modal capture.
  - Client build → exit `0`.
  - Server build → exit `0` and no engine Input translation unit.
  - Debug harness → keyboard/mouse/wheel behavior observed; no gamepad hardware requirement.
  - Gamepad preservation → moved-body source comparison, static mapping inventory, and client build.
  - `kSingleStep` check → Tab producer remains and no consumer/packet appears.
  - Allocation-tracker run → no break on exercised paths.
  - Documentation/project validation → ownership and filters agree.
  - Plan validation → `status: valid`, `code: ok`.
- Roles:
  - Preparation/implementation: implementer.
  - Plan review: fresh `/plan-audit` and `/plan-simplicity-review` reviewers.
  - Tier-3 decision review: `/external-grill-plan` preparation and manager interview route.
  - Build: builder through `/compile`, client and server.
  - Runtime: `/agent-harness` operator for keyboard/mouse/wheel only.
  - C++ correctness: fresh `/repo-code-review` reviewer.
  - Scope: fresh `/scope-review` reviewer.
  - Tier-3 fresh-eyes: `/adversarial-review` reviewer.
  - Propagation/docs: implementer through `/update-affected-code` and `/update-claude-docs`.
  - Project/style: mechanic through `/update-vcxproj` and `/code-style-review`.
  - Landing gate: fresh `/verify-changes` reviewer.
  - No unit-test role.
