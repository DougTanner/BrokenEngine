<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-19T01:46:49.924Z","dependsOn":["Documents/Plans/Agents/ClientAgentCommandsToEngine.md"]} -->
# Report the effective Tweaks UI state

## Context

This is an existing JSON-reporting defect in the post-move ownership from
`Documents/Plans/Agents/ClientAgentCommandsToEngine.md`. After that Plan
lands, `Engine/Source/Agent/AgentCommandsClientGeneric.cpp` owns
`describe_ui` and `UiStateName`, while
`Projects/BrokenEngineSandbox/Source/Agent/AgentScene.cpp` owns
`describe_scene`. Both current emitters serialize only `meUiState`.

F3 toggles the engine-owned `GameBase::mbShowImGui`
(`Projects/BrokenEngineSandbox/Source/Game.cpp:741-744`) to show the Tweaks
overlay, but does not assign `meUiState`. `UiState::kTweaks` already exists,
and the existing `AgentUiRegistry` pending-label/addressing behavior is
already implemented. The missing behavior is only the effective state in the
two existing JSON fields; tab addressing is not part of this Plan.

## Design

At both emitters, directly select `UiState::kTweaks` while
`mbShowImGui` is true, otherwise select the existing `meUiState`, and pass
that selected value to the existing naming helper. In the engine emitter use
the post-dependency `GameBase` state it already receives; in the game scene
emitter use the existing game state. Add no helper, public C++ API, enum,
command, response field, or production UI state transition.

Update only the project `AgentHarness` schema wording required to state this
effective-state rule for `describe_ui` and `describe_scene`.

## Critical files

- `Engine/Source/Agent/AgentCommandsClientGeneric.cpp` — post-dependency
  `BuildDescribeUi` effective-state selection and existing `UiStateName` call.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentScene.cpp` —
  `CommandDescribeScene` effective-state selection and existing naming call.
- `Engine/Source/GameBase.h:197-200` — `UiState`, `meUiState`, and
  `mbShowImGui` authority; read-only state contract.
- `Projects/BrokenEngineSandbox/Source/Game.cpp:741-744` — existing F3
  overlay toggle; read-only behavior authority.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness.md` — project command
  schema wording for the effective `uiState` value.
- `Documents/Plans/Agents/ClientAgentCommandsToEngine.md` — directional
  prerequisite and post-move ownership reference; do not edit it here.

## In scope

- Direct effective-state selection in the post-dependency `describe_ui`
  emitter and the existing `describe_scene` emitter.
- Passing that selected value to the existing `UiStateName` helper while
  preserving every other JSON field and response behavior.
- The minimum project AgentHarness schema wording needed to describe the
  effective `Tweaks` state.
- Debug client/server builds and the bounded live client acceptance below.

## Out of scope

- `AgentUiRegistry` hooks, tab labels, tab selection, pending-label work, or
  any other addressing/UI registry change.
- Tweaks implementation, settings, sliders, production UI transitions, or
  assigning `meUiState = UiState::kTweaks` for F3.
- New helpers, public C++ APIs, enums, commands, response fields, or state
  persistence.
- Simulation, CRC, replay, save, wire/protocol, and deterministic state.
- Changes to the prerequisite Plan or its ownership move.
- Screenshots or unit tests.

## Risk tier and invariants

Expected future Change Workflow Tier 3: the implementation coordinates
engine/game loopback JSON behavior at an external trust boundary.

Preserve these invariants:

- Only the reported effective value changes: `kTweaks` when `mbShowImGui` is
  true, otherwise the exact underlying `meUiState`.
- Both commands use the existing naming helper and retain all existing JSON
  fields, names, ordering, validation, and deferred behavior.
- F3 remains the existing overlay toggle; no production state machine or
  persisted state changes.
- No simulation, CRC, replay, save, wire, or deterministic state changes.

## Coordination

The metadata dependency on
`Documents/Plans/Agents/ClientAgentCommandsToEngine.md` is directional and
must land first. Implementation must consume its final
`AgentCommandsClientGeneric.cpp` ownership and `UiStateName` location, while
leaving the prerequisite Plan unchanged. No tab/addressing Plan or registry
change is a prerequisite for this effective-state reporting fix.

## Acceptance criteria

- Debug client and server builds pass through the repository build workflow
  after the prerequisite ownership move is present.
- A live client captures baseline `describe_ui {}` and
  `describe_scene {"includeUnits":false}` state. Sending
  `key {"key":"F3"}` produces the visible Tweaks window, and both commands
  report `uiState: "kTweaks"`.
- Sending the same F3 key a second time removes the Tweaks window and both
  commands restore the exact baseline underlying `meUiState` value; no other
  response fields regress.
- Bounded client logs remain clean for the scenario. No screenshot or unit
  test is added.

## Notes

The reporting rule reflects the already-visible overlay instead of changing
the production UI state machine. The dependency is required because the
engine generic emitter and `UiStateName` helper move as one ownership slice.

## Execution card

- Goal: make both existing UI reports identify the visible Tweaks overlay
  while preserving the underlying game UI state.
- Boundary: two post-dependency emitters and the minimum project schema
  wording; no UI registry or production-state change.
- Tier trigger: Tier 3 for coordinated engine/game loopback JSON behavior.
- Acceptance: Debug client/server builds; baseline/F3/second-F3 live reports;
  bounded clean logs; no screenshot or unit tests.
- Dependency: `Documents/Plans/Agents/ClientAgentCommandsToEngine.md`.
- No implementation is authorized by this conversion itself; this document
  is the scheduler action record.
