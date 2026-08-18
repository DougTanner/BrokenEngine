<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T17:52:11.437Z","dependsOn":[]} -->
# Move generic client agent commands into the engine

## Context

At baseline `12a682dad9ad483fe9b6098a0dd6576d62ba0af2`, the client-only
`Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsClient.cpp` mixes
engine-generic client automation with four game-owned commands. The generic
handlers reach through `gpGame` and `gpProfileManager`, while
`describe_scene` separately owns the game-specific scene view.

The current generic regions are identifiable by their functions rather than
their temporary line numbers:

- `PathFromParam`, `KeyHoldFramesParameter`, capture/render-doc state and
  `BeginCaptureAndDefer`;
- `CommandScreenshot`, `CommandRenderDocCapture`, `CommandResize`,
  `CommandFullscreen`, `CommandWindowState`, and `CommandDumpRenderTarget`;
- `BuildDescribeUi`, `CandidateLabels`, `ParseKeyVk`, `CopyStringParam`,
  `FillLabelTarget`, `BeginScriptAndDefer`, `CommandDescribeUi`,
  `CommandClick`, `CommandHover`, `CommandSetSlider`, `CommandKey`, and
  `CommandMouse`;
- the client GPU `CommandQueryProfile`;
- the private command-dispatch branches for those handlers.

The game-owned regions remain `CommandClientFullStateFixture` and its helper
functions, `DesyncProbeCountParameter`, `CommandDesyncProbe`,
`ClientGridCoordValue`, `CommandSetClientGridCoord`, the `describe_scene`
branch, and the game-side `ExecuteAgentCommandClient` dispatcher.

`engine::UiState` and `engine::GameFlags_t` are already owned by
`Engine/Source/GameBase.h:29-50`. `ProfileManagerBase` already exposes the
client GPU timer and shadow-sample state used by the existing query. No
prerequisite Plan is required.

## Design

Move the generic client handlers into uniquely named client-only engine files:

- `Engine/Source/Agent/AgentCommandsClientGeneric.h`
- `Engine/Source/Agent/AgentCommandsClientGeneric.cpp`

The unique basename is required because the game project already contains
`AgentCommandsClient.cpp`; the generic engine file must not create a second
same-basename object in Visual Studio/build bookkeeping.

The engine API is exactly:

```cpp
namespace engine
{

bool ExecuteClientAgentCommand(
	std::string_view cmd,
	const nlohmann::json& rParams,
	nlohmann::json& rResult,
	const GameBase& rGame,
	ProfileManagerBase& rProfileManager);

const char* UiStateName(UiState eState);
nlohmann::json GameFlagNames(GameFlags_t flags);

}
```

The header keeps `#pragma once` outside its client guard. The implementation
is wrapped as a whole in `#if defined(BT_CLIENT)` and remains in the `engine`
namespace. Include the header in `Engine.h` after `GameBase.h`, where the
public state types and base classes are defined.

### Command ownership and dispatch

`ExecuteClientAgentCommand` owns exactly these command names:

| Engine generic client command | Existing behavior retained |
| --- | --- |
| `screenshot` | Async image save; returns path and dimensions. |
| `renderdoc_capture` | Captures one to eight presented frames and returns absolute `.rdc` paths. |
| `resize` | Rounds requested client dimensions and waits for the applied extent. |
| `fullscreen` | Applies the live borderless/windowed style and waits for extent settlement. |
| `window_state` | Minimizes/restores without activation and waits for the settled state. |
| `dump_render_target` | Validates, reads, encodes, and returns the existing target result. |
| `describe_ui` | Reports the last completed UI registry frame plus live game UI state. |
| `click`, `hover`, `set_slider`, `key`, `mouse` | Runs the existing one-at-a-time synthetic input scripts. |
| `query_profile` | Returns the existing client GPU timer rows and shadow sample. |

The game dispatcher keeps exactly this outer order:

1. `engine::ExecuteSharedAgentCommand`;
2. the both-endpoint `collection_layout_capacity_fixture`;
3. `engine::ExecuteClientAgentCommand`, called with `*gpGame` and
   `*gpProfileManager`;
4. game `ExecuteAgentCommandClient`;
5. the existing `unknown command` failure.

The server branch remains shared-command → collection fixture → server game
handler → unknown. The new client translation unit and header are not added to
the server project.

### Explicit live state and deferred lifetime

Change `BuildDescribeUi` to take `const GameBase&` and read
`rGame.meUiState` and `rGame.mGameFlags` at execution time. Change
`CommandQueryProfile` to read the supplied `ProfileManagerBase&`; do not add a
derived-game profile dependency or a snapshot layer.

`BeginScriptAndDefer` receives a `const GameBase*` only when a script may append
UI output. Its deferred lambda captures that pointer by value and calls
`BuildDescribeUi(*pGame)` after the script completes. It must not capture the
reference parameter or a stack alias. The existing main-loop lifetime makes
the pointer valid: `Main.cpp` constructs the profile manager before the agent
server, constructs `Game` before the first `Drain()` at
`Engine/Source/Main.cpp:372-376`, and destroys the live loop objects only
after command processing has stopped.

Preserve all generic helper bodies mechanically. The only semantic wiring
changes are the namespace, the explicit game/profile references, the pointer
capture for deferred UI output, and the moved enum-name helper calls.

### Enum-name helpers

Move `UiStateName` and `GameFlagNames` from
`Projects/BrokenEngineSandbox/Source/Agent/AgentScene.cpp` into the new engine
source. Preserve every string and the existing flag-array order. Remove the
game declarations from `AgentScene.h`; `CommandDescribeScene` calls the engine
helpers with `engine::` qualification. `describe_ui` and `describe_scene`
therefore continue to expose the same names without an append hook. A game's
additional scene data remains owned by `describe_scene`.

### Behavior and contract preservation

Do not change command names, accepted parameters, validation order, error
messages, JSON key names or insertion order, deferred response timing,
`AgentCommandServer::kiDeferredTimeoutDrains`, capture phase transitions,
minimize/restore cleanup, synthetic-input status handling, or profile sample
reads. In particular:

- capture state destructors still restore a previously minimized window on
  timeout/disconnect;
- UI scripts still occupy the one in-flight command slot until completion;
- `describe_ui` still reads the completed `AgentUiRegistry` buffer while its
  `uiState` and `gameFlags` fields read live `GameBase` state;
- `query_profile` remains empty-object-only and returns all GPU rows plus the
  frame-coherent `shadowSample`;
- no simulation, CRC, replay, save, wire, or deterministic frame state is
  changed.

## Critical files

### New engine implementation

- `Engine/Source/Agent/AgentCommandsClientGeneric.h` — the three declarations
  above, guarded for client use.
- `Engine/Source/Agent/AgentCommandsClientGeneric.cpp` — the moved generic
  functions and private helpers, plus `UiStateName`, `GameFlagNames`, and the
  engine dispatcher.
- `Engine/Source/Engine.h` — client-only include after `GameBase.h`.

### Game dispatch and retained client commands

- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommands.cpp:218-245` —
  preserve shared and collection ordering, then invoke the engine handler with
  explicit game/profile references before the game client handler. Add the
  existing game `ProfileManager.h` include needed to name `gpProfileManager`.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsClient.cpp` — retain
  the fixture region (`BuildFullStateFixtureCoordState` through
  `CommandClientFullStateFixture`), `DesyncProbeCountParameter`,
  `CommandDesyncProbe`, `ClientGridCoordValue`, `CommandSetClientGridCoord`,
  the `describe_scene` branch, and the four-command game dispatcher. Remove
  only the generic handlers/helpers listed in the Design section.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommands.h` — update the
  client-dispatch comment so it describes only fixture, scene, desync, and
  grid commands; the declaration remains game-owned.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentScene.h` — remove the moved
  helper declarations and stale “shared by describe_ui” comment.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentScene.cpp` — remove the
  helper definitions and qualify the two `describe_scene` uses.

### Project membership

- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj`
  and `.filters` — add the new engine header/source under the client project’s
  Engine/Agent filter, alongside the existing agent entries.
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj`
  and `.filters` — explicitly remain unchanged; no new client-generic path may
  appear there.

### Current ownership documentation

- `Engine/Source/Agent/AGENTS.md` — state that build-agnostic commands remain
  in `AgentCommandsShared`, while client-generic capture/window/UI/input/GPU
  commands live in the client-only engine handler and receive explicit live
  state; retain the main-thread/deferred constraints.
- `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md` — state that the
  engine owns client capture/window/UI/input/GPU automation and the game owns
  scene, fixture, desync, and coordinate commands.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness.md:210` — update the
  current source-ownership sentence; command schemas and recipes remain
  unchanged.
- `Documents/UserInterfaceDesign.txt:266` — point the current
  `describe_ui`/`click`/`hover` source reference at
  `Engine/Source/Agent/AgentCommandsClientGeneric.cpp`.

Historical investigation and feature records are not revised merely because
they cite the baseline source layout.

## In scope

- The new client-only engine header/source and the exact interface above.
- Mechanical relocation of all generic capture, window, UI, synthetic-input,
  profile handlers, their private helpers, and their generic dispatch branches.
- Explicit `GameBase` and `ProfileManagerBase` context plumbing.
- Moving `UiStateName` and `GameFlagNames`; updating `describe_scene` calls.
- The game outer dispatcher change and its include/comment updates.
- Retaining only the four game client commands in
  `AgentCommandsClient.cpp`.
- Client project/filter additions and the server membership negative check.
- The exact current ownership documentation files listed above.
- Existing compile and Agent Harness verification; no new test files.

## Out of scope

- New commands, parameters, response fields, callbacks, append hooks, or
  compatibility wrappers.
- Any change to JSON schemas, JSON values/order, validation/error strings,
  deferred polling, capture cleanup, input timing, or profile sampling.
- `AgentCommandsShared.h/.cpp`, `AgentCommandServer`, server dispatch and
  handlers, network protocol, serialization, CRC, replay, save, simulation,
  `GameBase` layout, and `ProfileManagerBase` layout.
- Moving `client_full_state_fixture`, `describe_scene`, `desync_probe`, or
  `set_client_grid_coord` into the engine.
- Changes to `AgentInput`, `AgentUiRegistry`, Graphics, Screenshot, or Profile
  implementations.
- Any server project/filter addition.
- Historical investigation/feature-document rewrites.

## Risk tier and invariants

Tier 3: the change crosses the engine/game command boundary at a loopback JSON
trust boundary, introduces an engine API, changes deferred-reference lifetime
handling, and changes client-only Visual Studio project affinity.

The implementation must preserve:

- main-thread-only execution for all game, Vulkan, and UI work;
- valid `GameBase`/`ProfileManagerBase` lifetime for synchronous and deferred
  calls;
- no game global or derived `ProfileManager` reference in the new engine source;
- whole-file `BT_CLIENT` affinity for the new implementation;
- the exact shared → collection fixture → engine generic → game client →
  unknown dispatch order;
- exact `UiStateName` strings and ordered `GameFlagNames` output;
- capture RAII restoration and deferred timeout/disconnect cleanup;
- unchanged client/server determinism, CRC, replay, save, and wire behavior.

## Acceptance criteria

### Static and build evidence

- `rg` shows every generic handler/helper definition moved out of the game
  client file, while every retained game function and branch remains.
- `rg` shows no `gpGame`, `gpProfileManager`, or derived game type use in the
  new engine implementation.
- The client project contains both new files under Engine/Agent; the server
  project and filters contain neither path and have no unrelated diff.
- Client and server x64 Debug builds pass through `/compile`.
- The current ownership documentation names the new engine/game boundary and
  the sibling `CLAUDE.md` stubs remain valid imports.

### Dispatch and bounded runtime evidence

Use one existing Debug client/server Agent Harness run. Capture client and
server `get_logs` baselines immediately before the command cohort. The bounded
post window ends after the final deferred response and one successful `ping`;
compare only lines appended in that window. No new command failure, CRC,
confirmed-desync, checksum, or full-state read/decompression failure may appear.

The client dispatch smoke must prove, in order:

- `ping` succeeds through shared handling;
- `collection_layout_capacity_fixture {}` succeeds with its existing client
  result and `passed:true`;
- `describe_ui {}` succeeds through the engine handler;
- `client_full_state_fixture {"action":"clear"}` reaches the retained game
  handler and returns its existing top-level state shape;
- an unknown command returns the existing `ok:false` / `error:"unknown command"`
  envelope.

The server must still answer shared `ping` and the collection fixture and must
return `unknown command` for client-only commands. The server project/filter
negative membership check is an independent signal from the server build.

### Generic command families

Baseline/current `describe_ui` results are compared as complete schemas in the
same controlled UI state. The expected result shape is:

```json
{
  "uiState": "k...",
  "gameFlags": ["k..."],
  "framebuffer": [0, 0],
  "mouse": [0.0, 0.0],
  "windows": [{"name":"...","rect":[0.0,0.0,0.0,0.0],"focused":false}],
  "items": [{"label":"...","window":"...","rect":[0.0,0.0,0.0,0.0],"disabled":false,"checked":false,"inputable":false,"hovered":false,"visible":false}]
}
```

Run a state-changing existing UI script such as `click` with
`describeUiAfter:true`; require the existing
`{"found":true,"enabled":bool,"ui":{...}}` response, then immediately call
`describe_ui`. Compare only `uiState` and the ordered `gameFlags` array between
the nested and immediate results; this proves the deferred closure reads the
live `GameBase` without incorrectly claiming that the independently changing
mouse/registry fields must equal. `hover` retains the same found/enabled/UI
shape; `set_slider` retains found/enabled; `key` and `mouse` retain
`{"ok":true}`. Each completes through the existing single in-flight deferred
path.

Compare `describe_scene {"includeUnits":false}` independently against its
baseline. Its expected full shape is:

```json
{
  "camera":{"eye":[0.0,0.0,0.0],"visibleArea":[0.0,0.0,0.0,0.0],"lod":0},
  "uiState":"k...",
  "gameFlags":["k..."],
  "tick":0,
  "clientGridCoord":[0,0],
  "fleets":[{"index":0,"focused":false,"members":[]}],
  "subscribedCoords":[[0,0]],
  "units":[],
  "counts":{"players":0,"spaceships":0,"missiles":0,"blasters":0,"targets":0},
  "islands":[],
  "truncated":false
}
```

Only `uiState` and ordered `gameFlags` are compared across the two commands;
each full schema is separately baseline-equivalent.

Verify `query_profile {}` retains:

```json
{
  "gpuTimers":[{"index":0,"name":"...","currentUs":0,"averageUs":0,"maxUs":0}],
  "shadowSample":{"sequence":0,"currentUs":0}
}
```

Verify the window/capture family with the existing documented commands:

- `resize` returns applied `{"width":int,"height":int}` after extent
  settlement and retains its minimized/error behavior;
- `fullscreen` returns `{"fullscreen":bool,"width":int,"height":int}`;
- `window_state` returns `{"minimized":true}` when minimizing and
  `{"minimized":false,"width":int,"height":int}` after restore;
- `screenshot` returns `{"path":string,"width":int,"height":int}` and the
  file exists;
- `dump_render_target` retains its format/width/height/path result or exact
  existing validation error;
- with `--renderdoc`, `renderdoc_capture {"frames":1}` returns one existing
  absolute `.rdc` path; without the API it returns exactly
  `RenderDoc API not available (launch the client with --renderdoc)`.

### Retained game-command evidence

Use existing preconditions and do not add a new fixture:

- Arm the existing full-state fixture, then send
  `client_full_state_fixture {"action":"inspect"}`. The result retains
  `clientTick`, `stalled`, `desyncTick`, `syntheticStall`, `armedTick`,
  `timeMultiply`, `timeDivide`, and `coordState`; a present `coordState`
  retains all confirmed/high-water/full-state/ring/render/server-update fields,
  nullable pending/first/last update ticks, `ringValid`, and `tailTick`. Clear
  it afterward.
- Send `desync_probe {"desyncReports":0,"debugFrameRequests":0}` on a
  connected live frame. Require
  `{"tick":int,"coord":[int,int],"desyncDebugFrames":false,"stalled":bool,"desyncReports":0,"debugFrameRequests":0,"triggerRecovery":false}`.
  Also retain the validation probe `{"triggerRecovery":false}` with the exact
  existing error and no state mutation; do not trigger recovery.
- Send `set_client_grid_coord {"coord":[1000001,0]}` as the no-setup routing
  probe. Require the exact existing error
  `set_client_grid_coord 'coord' values must be within +/-1000000` and no
  coordinate/tick mutation. When the documented connected-player recipe is
  already active, the valid result remains exactly
  `{"clientGridCoord":[x,y]}`.

The retained command responses and their validation paths must remain
baseline-equivalent, and the bounded client/server logs must remain clean.

## Implementation sequence

1. Add the guarded engine header and aggregate it after `GameBase.h`.
2. Move the generic functions/helpers into the new engine source, changing only
   namespace and explicit context/pointer plumbing.
3. Trim the game client source to the retained handlers and update the game
   dispatcher to invoke the engine handler first.
4. Move the enum-name helpers and update `AgentScene` declarations/calls.
5. Add client project/filter membership and verify the server project/filter
   negative.
6. Update the exact current ownership documentation files.
7. Run targeted searches, `/compile`, the routed C++/scope/style/project/docs
   reviews, and the existing Agent Harness acceptance above.

No new command, test file, compatibility layer, or external dependency is
introduced.
