<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T18:52:53.186Z","dependsOn":["Documents/Plans/Engine/NetworkProfileOwnershipToEngine.md"]} -->
# Move ServerDisplay ownership to the engine

## Context

This is the final world-tools slice, after world-grid state and Network profile
ownership. `Projects/BrokenEngineSandbox/Source/Server/ServerDisplay.cpp:83-190,548-672`
currently mixes generic window/profile/map work with direct frame and collection
reads. The approved game seam is a raw required call returning a fixed cell-stat
record by value from a `const game::Frame&`; the example names below are
illustrative only:

```cpp
ServerCellStats GetServerCellStats(const game::Frame& rFrame);
```

The required semantic shape is a fixed cell-stat record returned by value from a
`const game::Frame&` through a direct, synchronous, observational game call; the
illustrative type and function names are not prescribed. The engine owns the
returned value only for the synchronous display operation. The game function
retains no pointer, reference, callback, writer, or other display data. Existing
required timer/counter names and `ProfileManagerBase` accessors remain in use:
`game::kCpuTimerFrameUpdate`,
`GetCpuTimer`, `GetCpuCounter`, `FormatCpuTimersText`,
`FormatCpuCountersText`, and `TickVisibilityCadence`.

The lifetime path is observable. `destroyWindow` is declared before `pGame` and
calls `ProcessMessages()` before `DestroyWindow` at `Engine/Source/Main.cpp:230-240`;
the server `WM_PAINT` handler calls `PaintServerDisplay` at `:648-653`, where
the current implementation reads `game::gpGame`.

## Pre-implementation decisions and options

The raw required call has the fixed semantic shape above; its example type and
function names are illustrative only. Record the display-lifetime choice before
implementation:

- **Option A (recommended):** while `gpGame` is alive, drain/disable the
  display's queued messages before teardown and then destroy the window.
- **Option B:** detach the display from game state before teardown and make the
  queued `WM_PAINT` path a safe no-op until the window is destroyed.

Both options must make a queued `WM_PAINT` safe after game teardown. If a
game-specific click/action policy remains necessary, use one direct required
game function; do not add registration, callbacks, retained function pointers,
writer objects, or a display base class.

## Design

The engine owns display state, cadence, invalidation decisions, window messages,
and generic profile/map rendering. It synchronously calls the required game
function to obtain a fixed cell-stat record by value from a `const game::Frame&`,
then formats and paints from the returned record. Preserve existing timer/counter
rows and map behavior. Implement the
selected teardown ordering/no-op strategy and prove that both normal painting
and queued `WM_PAINT` cannot dereference a destroyed `gpGame`.

`ServerDisplayContentChanged` currently hashes committed and heap-used memory,
but not the peak values shown by the profile panel. Keep this incomplete
peak-memory hash residual visible; correcting it is not silently folded into
this ownership move.

## Critical files

- `Engine/Source/Main.cpp:230-240,425-443,648-653` — window ownership,
  destruction, and `WM_PAINT` dispatch.
- `Projects/BrokenEngineSandbox/Source/Server/ServerDisplay.cpp:83-190,548-672` and its header —
  current display state, map/profile rendering, invalidation, and direct reads.
- `Projects/BrokenEngineSandbox/Source/Server/ServerDisplay.h:8-12` — current
  game-side declaration and required-call replacement.
- `Engine/Source/Profile/ProfileManagerBase.cpp` and
  `Projects/BrokenEngineSandbox/Source/Profile/ProfileManager.h` — required
  timer/counter accessors and formatting names.

## In scope

- Move/adapt generic ServerDisplay state, cadence, invalidation, window-message
  handling, and map/profile rendering into the engine.
- Add the direct required game call with a fixed cell-stat record returned by value
  from a `const game::Frame&`; example type and function names are illustrative
  only, and the call remains synchronous and observational.
- Preserve existing timer/counter names, profile rows, map output, and display
  cadence.
- Implement and verify the selected lifetime strategy for queued `WM_PAINT`.
- Update direct callers, declarations, and project membership required by the
  ownership move.

## Out of scope

- World-grid state or Network profile/NetworkGraphs ownership beyond the two
  metadata prerequisites.
- Changing the approved raw-call shape, adding hooks/registries/base classes,
  or retaining display data beyond the current operation.
- Correcting the incomplete peak-memory hash; it remains an explicit residual.
- Simulation, frame CRC, replay, wire/save formats, compatibility aliases,
  speculative abstractions, or unit tests.

## Risk tier and invariants

Tier 3 — engine/game display integration with a cross-boundary synchronous data
call and teardown-sensitive Win32 message lifetime.

- The required call returns a fixed cell-stat record by value from a
  `const game::Frame&`; its example type and function names are not prescribed,
  and no display callback, pointer, reference, writer, or other display data is
  retained.
- The engine owns display state and message/cadence decisions; game owns only
  the required observational data function and any direct action policy.
- Existing timer/counter names, map/profile output, and cadence remain stable.
- Queued `WM_PAINT` cannot read `gpGame` after teardown.
- The peak-memory hash omission remains documented and visible as residual.
- No simulation, CRC, replay, wire, or save-state layout changes occur.

## Acceptance criteria

1. The direct required game call has the approved semantic shape: a fixed
   cell-stat record returned by value from a `const game::Frame&`, used
   synchronously and observationally with no retained pointer, reference,
   callback, writer, or display data; any example names are illustrative only.
2. Static traces show engine ownership of display state, cadence, invalidation,
   window messages, and generic rendering, with every direct frame/collection
   read routed through the fixed cell-stat record returned by the direct required
   game call.
3. A server display scenario preserves current map/profile output, timer/counter
   rows, cadence, and invalidation behavior.
4. A teardown scenario queues `WM_PAINT`, destroys `gpGame`, and proves the
   queued message is drained, disabled, or safely ignored without a dereference.
5. Server Debug and the affected client target builds pass; no unit tests are
   added.
6. The incomplete peak-memory hash residual is reported unchanged, naming the
   displayed peak values that are not hashed.

## Execution card

- **Tier/triggers:** Tier 3; final cross-boundary display extraction and
  teardown-sensitive window-message lifetime.
- **Roles:** implementer; fresh plan/correctness/scope reviewers; builder and
  harness; cleanup and landing verification roles.
- **Dependencies:** state and Network profile Plans above must land first;
  execute this Plan last in the split.
