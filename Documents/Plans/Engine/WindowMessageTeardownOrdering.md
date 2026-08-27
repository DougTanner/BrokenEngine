<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:52:02.841Z","dependsOn":[]} -->
# Keep client window messages inside manager lifetime

## Context

The retained survivor `CAI/shard-0012/002` identifies a shutdown message
dispatch gap at `Engine/Source/Main.cpp:235-251`. The `destroyWindow` scope
callback currently performs a final `ProcessMessages()` while the HWND still
exists. The survivor's premise that this callback runs after the raw-input and
audio managers is false: `pRawInputManager` and `pAudioManager` are declared at
`Engine/Source/Main.cpp:145-148`, before `destroyWindow` at
`Engine/Source/Main.cpp:235-259`, so reverse local destruction leaves both
managers alive until that callback has finished.

The actual late-dispatch hazard is the later-declared client owners at
`Engine/Source/Main.cpp:285-293`: `pGraphics` (including its ImGui context),
`pGame`, and `pInput` are destroyed before `destroyWindow`. `Game::~Game()` clears
`game::gpGame` at `Projects/BrokenEngineSandbox/Source/Game.cpp:361-380`,
while client `WndProc` still dereferences `game::gpGame` for `WM_SETFOCUS` at
`Engine/Source/Main.cpp:720-734` (and its client message paths remain live
through `Engine/Source/Main.cpp:603-799`). A queued focus message dispatched by
the callback's late pump can therefore reach a dead `Game`; the raw-input and
audio dereferences at `Engine/Source/Main.cpp:720-767` are not the lifetime
failure because those two managers are still alive at that point.

The direct evidence is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0012.md:53`
and consolidated selector
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:585`.
The local lifetime trace is corroborated by `Common/ScopedLambda.h:15-27`,
which runs the callback during reverse-scope destruction and swallows callback
exceptions. The target remains baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; no source change is part of this
Plan-preparation stage.

## Design

Use the smallest root-cause mechanism: keep the existing normal final client
pump at `Engine/Source/Main.cpp:486-487`, where `pGame`, `pGraphics`/ImGui,
`pInput`, `pRawInputManager`, and `pAudioManager` are all still alive, then make
the client side of the `destroyWindow` callback destroy-only. It must not call
a second client `ProcessMessages()` after the later-declared owners have been
released. `DestroyWindow` remains in the callback and still runs before the
raw-input and audio managers are released, because those managers were
declared earlier. In `Engine/Source/Main.cpp`, place only the callback's existing
`ProcessMessages()` under the `BT_SERVER` branch; leave the normal client
`PostQuitMessage(0); ProcessMessages();` pair at
`Engine/Source/Main.cpp:486-487` intact.

Keep the server branch of the callback unchanged in behavior: its `sbQuit`
assignment, `ValidateRect`, final `ProcessMessages()` drain, and existing
`WM_PAINT`/click protection remain in place. Do not add a shutdown gate,
manager-pointer null check, or message filtering to client `WndProc`; the
ordering change removes the late client dispatch that exposed the dead
`game::gpGame`.

On exception unwinding, `ScopedLambda` invokes the destroy-only callback after
`pGame`, `pGraphics`, and `pInput` have been destroyed but before
`pRawInputManager` and `pAudioManager` are destroyed. The callback therefore
destroys the HWND without pumping the thread queue. `wWinMain` then follows
the existing catch/`HandleException` path at
`Engine/Source/Main.cpp:853-875` and returns;
process exit is the cleanup boundary for any undispatched queued `WM_INPUT`.
Do not add speculative exception machinery around `DestroyWindow` or a new
process-wide queue drain.

## Critical files

- `Engine/Source/Main.cpp:143-166,225-259,285-293,365-488,603-799,853-875` —
  local declaration/destruction order, the normal final client pump, the
  destroy-only client callback, client/server message handling, and exception
  unwinding.
- `Projects/BrokenEngineSandbox/Source/Game.cpp:361-380` — `Game` teardown
  clears `game::gpGame` before the callback's current late pump.
- `Engine/Source/Graphics/Graphics.cpp:142-150` and
  `Engine/Source/Graphics/Managers/ImGuiManager.cpp:298-305` — later-owner
  and ImGui-context teardown that precede the callback.
- `Engine/Source/Input/RawInputManager.cpp:30-69` and
  `Engine/Source/Audio/AudioManager.cpp:293-310` — manager globals remain
  live through `destroyWindow` and are released afterward.
- `Common/ScopedLambda.h:15-27` — callback execution during scope teardown
  and exception swallowing.
- `Engine/Source/AGENTS.md` and `Engine/Source/Input/AGENTS.md` — startup,
  focus, and manager-lifetime contracts.

## In scope

- `Engine/Source/Main.cpp:235-259`: make the client `destroyWindow` callback
  destroy-only while preserving the server shutdown drain and paint guard.
- `Engine/Source/Main.cpp:486-487`: retain the normal final client
  `ProcessMessages()` while all client owners are alive, with no second client
  pump during scope teardown.
- The ordering proof for normal return and exception unwinding, including
  `DestroyWindow` before raw-input/audio manager release and the absence of
  queued dispatch on the exception path.

## Out of scope

- Any client `WndProc` shutdown gate, manager-pointer null check, message
  filtering, or raw-input/message-protocol change.
- Changes to normal per-frame message pumping, raw-input semantics, audio focus
  policy, manager construction, or manager declaration order.
- Any server shutdown or `WM_PAINT` behavior change; the existing server drain
  and paint protection remain authoritative.
- New exception handling around `DestroyWindow`, a process-wide queue drain,
  crash-report stream/logging changes, or unrelated teardown work.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3). Trigger: this crosses
Win32 message delivery, RAII destruction order, global manager lifetime, and
client shutdown thread-affinity boundaries.

Tier rationale: the Design resolves the ordering question in full and reduces
the edit to moving one existing `ProcessMessages()` call under the `BT_SERVER`
branch in a single file. It is a local teardown-ordering adjustment with no new
mechanism, no server behavior change, and no format, wire, or threading
structure change.

Preserve these invariants:

- The normal final client pump dispatches queued focus/raw-input messages only
  while `Game`, Graphics/ImGui, Input, RawInputManager, and AudioManager are
  alive; teardown performs no second client pump.
- Exception unwinding dispatches no queued client message after the later
  owners die; it destroys the HWND before the raw-input and audio managers are
  released, and process exit owns any undispatched queued `WM_INPUT` cleanup.
- A final close still reaches `DestroyWindow` exactly once and clears the
  client HWND; process exit remains the cleanup boundary for the exception
  path's undispatched queue state.
- The server callback's final drain and `WM_PAINT` protection are unchanged.
- Ordinary focus/raw-input handling before shutdown remains unchanged, and no
  client WndProc shutdown gate or null check is introduced.

## Acceptance criteria

- In the normal client close path, messages already queued before the final
  pump (`WM_INPUT`, `WM_SETFOCUS`, and `WM_KILLFOCUS`) are handled by the existing
  `Engine/Source/Main.cpp:486-487` `ProcessMessages()` while all client owners
  are alive; the subsequent
  `destroyWindow` callback destroys the HWND without a second client pump.
- In an exception unwind after window/owner construction, source inspection or
  instrumentation proves that `destroyWindow` performs no queued-message pump,
  calls `DestroyWindow` before raw-input/audio manager destruction, and leaves
  undispatched queued `WM_INPUT` to the documented process-exit cleanup
  boundary.
- Source inspection proves the client `WndProc` has no new shutdown gate or
  manager-pointer null check, and the server `sbQuit`/`ValidateRect`/paint
  protection plus server final drain remain behaviorally unchanged.
- Normal focused and unfocused client input behavior remains unchanged before
  teardown, and `DestroyWindow` still runs exactly once with `sHwnd` cleared.
- The future source implementation passes the client `Debug|x64` build through
  `/compile`.

## Notes

Origin: `CAI/shard-0012/002`; source selector is the shard line above and the
consolidated selector is `Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:585`. The two-researcher
comparison selected the smallest mechanism: preserve the normal final client
pump, remove only the late client pump from `destroyWindow`, and leave the
server drain/paint path unchanged. The survivor's raw/audio destruction claim
was corrected from the source declaration order above. No source fix or build
was performed during this Plan-only preparation; baseline remains
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`.
