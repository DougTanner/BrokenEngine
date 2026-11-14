<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:41:52.999Z","dependsOn":[]} -->
# Share one checked integer reader between edit_frame and inject_payload

## Context

`ParseStatusChangeField` in
`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.cpp`
(`Rigs.cpp` below) hand-writes four integer reads that the engine helper
`engine::IntegerFromValue`
(`Engine/Source/Agent/Commands/FrameEdit.h:88-108`) already performs. Verified
against commit `035db8bf`:

- The four branches: `int64_t` (`Rigs.cpp:91-98`), `uint8_t` (`:99-111`),
  flags (`:154-168`), and `engine::GlobalId` (`:173-188`). Each tests
  `is_number_integer()`, reads `get<int64_t>()`, and, for the two narrow
  kinds, range-checks with `std::in_range`.
- `IntegerFromValue<INTEGER>` is the same read with the range check built in,
  and `ReadElement` in `FrameEdit.h` uses it for the same member kinds
  (`:130-133`, `:142-146`, `:156-159`). It has no caller outside `FrameEdit.h`.
- The two parse ladders already share `engine::AgentFloatValue` and
  `engine::CoordinateFromParameter`
  (`Engine/Source/Agent/AgentCommandsShared.h:25,28`) and `EnumFromToken`.
- `Rigs.cpp` cannot include `FrameEdit.h` as it is:
  `Engine/Source/Agent/AGENTS.md` `## Game contract` limits that header to the
  game frame-edit commands. `AgentCommandsShared.h` is where
  `Engine/Source/Agent/AGENTS.md` `## Architecture` puts "the checked
  grid-coordinate and float readers that engine and game commands share".

Originating gap: the architecture review (Phase 1) of the
`/external-deep-analysis` run over
`Projects/BrokenEngineSandbox/Source/Agent`; independent verification (Phase
3) accepted it with the caveats `## Design` lists. Unmet criterion: one
integer reader for agent command JSON. The duplication predates that run,
which changed no file.

## Design

Recommended:

1. Move the `IntegerFromValue` template, unchanged and under the same name,
   from `FrameEdit.h` to `AgentCommandsShared.h`, with a one-line comment in
   the style of its neighbours. `FrameEdit.h` callers need no edit: the header
   reaches them through `Engine.h` in the PCH.
2. `Rigs.cpp` `ParseStatusChangeField`: replace the bodies of the four
   branches with one call each:
   `engine::IntegerFromValue<int64_t>(rValue, kName)` for `int64_t` and for
   `GlobalId::iValue`, `<uint8_t>` for `uint8_t`, and `<Underlying>` for the
   flags branch. The minted-`globalId` refusal (`Rigs.cpp:176-179`) and every
   non-integer branch stay.
3. `Engine/Source/Agent/AGENTS.md` `## Architecture`: add "integer" to the
   reader list in the `AgentCommandsShared.h` sentence.
4. `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`
   `inject_payload` value list: at line 218 ("`int64_t` a JSON integer") and
   at line 230 ("`flagshipGlobalId` an integer"), say the integer must be
   within `int64_t`, as the `edit_frame` list already does.

Behavior changes the Plan owns, all in `inject_payload` error handling:

- The `int64_t` and `GlobalId` messages gain a range suffix: the text
  `must be an integer` becomes `must be an integer in [<min>,<max>]`. No
  document or script quotes the old text (a search of `*.md`, `*.ps1`, and
  `*.json` outside `Temp/` and `ThirdParty/` finds none).
- An unsigned JSON value above `INT64_MAX` is refused for those two kinds;
  today `get<int64_t>()` wraps it, because `is_number_integer()` is true for
  unsigned values (`ThirdParty/json/include/nlohmann/json.hpp:2853-2856`).
- The `uint8_t` and flags messages keep their text.

## Critical files

- `Engine/Source/Agent/AgentCommandsShared.h`
- `Engine/Source/Agent/Commands/FrameEdit.h`
- `Engine/Source/Agent/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.cpp`
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`

## In scope

- `engine::IntegerFromValue`: its definition moves between the two engine
  headers.
- `ParseStatusChangeField`: the `int64_t`, `uint8_t`, flags, and `GlobalId`
  branches.
- `Engine/Source/Agent/AGENTS.md` `## Architecture`: the reader list of the
  `AgentCommandsShared.h` sentence.
- `commands-server.md` `inject_payload`: the `int64_t` and `flagshipGlobalId`
  value wording.

## Out of scope

- Renaming `IntegerFromValue` or changing its body or message.
- The `tick` read in `BuildInjectedEntry` (`Rigs.cpp:321-332`) and
  `engine::AgentGridCoordinateValue`: other integer reads the finding did not
  cover.
- The bool, float, duration, `GridCoord`, `XMVECTOR`, alignment, and enum
  branches of `ParseStatusChangeField`.
- Merging the two parse ladders, or including `FrameEdit.h` from `Rigs.cpp`.
- `ReadElement` and every other part of `FrameEdit.h`.

## Acceptance criteria

1. `IntegerFromValue` is defined once, in `AgentCommandsShared.h`, and
   `ParseStatusChangeField` calls it in four branches and holds no
   `is_number_integer()` call.
2. `/compile` builds the BrokenEngineSandbox client and server and
   ModelViewer (all three include `Engine.h` in their PCH).
3. Through `/agent-harness`: an `inject_payload` entry with a non-integer
   `iUuid` fails with the ranged message, one whose `iUuid` is an integer
   above `INT64_MAX` (for example `9223372036854775808`) fails with the same
   message, one with an integer `iUuid` within `int64_t` is accepted as
   before, and `edit_frame` integer writes behave as before. The originating
   analysis ran nothing, so it records no harness setup.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

- Change Workflow tier: Tier 2 (the author's classification). Trigger: one
  tool's scoped behavior changes (two error texts and one refused value in
  `inject_payload`). The helper itself only moves between two engine headers,
  to the one that already holds the readers engine and game commands share
  (`AgentCommandsShared.h`). The agent command channel is an exempt developer
  tool, so no trust boundary changes.
- Exposure: build (an `Engine.h`-aggregated header changes, so every PCH
  rebuilds) and the live check above. No determinism/CRC, serialization,
  `.pack`, `kiVersion`, wire, replay, affinity, threading, allocation, or
  shader exposure.
- `Documents/Plans/Game/FileLocalQueueAgentStatusChange.md`,
  `Documents/Plans/Game/RemoveShipNavigationDelayColumn.md`, and
  `Documents/Plans/Game/RemoveWriteOnlyMissileTargetFlags.md` edit other
  functions of `Rigs.cpp`. The regions do not overlap and no order is
  required; line numbers here shift once one of them lands.
