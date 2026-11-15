<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:42:27.264Z","dependsOn":[]} -->
# Clean up the edit_frame and inject_payload agent commands

## Context

Three findings from `/external-deep-analysis` runs over
`Engine/Source/Agent` and `Projects/BrokenEngineSandbox/Source/Agent` touch
the `edit_frame` and `inject_payload` agent commands. `Rigs` below is
`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs`
(`Rigs.cpp`, `Rigs.h`). Line numbers for the integer-reader and
`QueueAgentStatusChange` findings are from commit `035db8bf`; those for
`WriteRow` are from commit `4fa6d2d7`. None of the runs built or ran anything,
and every finding predates them.

### One integer reader for agent command JSON

`ParseStatusChangeField` in `Rigs.cpp` hand-writes four integer reads that the
engine helper `engine::IntegerFromValue`
(`Engine/Source/Agent/Commands/FrameEdit.h:88-108`) already performs:

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

Unmet criterion: one integer reader for agent command JSON.

### QueueAgentStatusChange export and the rig command dispatcher

- `QueueAgentStatusChange` is exported but has no outside caller. `Rigs.h:28`
  declares it; its only call is `Rigs.cpp:570`, inside `CommandInjectPayload`
  in the same file (a search of `Engine/Source` and
  `Projects/BrokenEngineSandbox/Source` finds the declaration, the definition
  at `Rigs.cpp:602`, that call, and a comment at `Rigs.cpp:29`). The
  `engine::GridCoord` forward declaration at `Rigs.h:5-10` serves that
  declaration alone. `Documents/C++StyleGuide.txt` rule 66 makes a file-local
  function `static`. The header's other functions do have outside callers
  (`Engine/Source/Network/Server/ServerBroadcaster.cpp`,
  `AgentCommandsServer.cpp`, `Source/Network/Server/ServerSession.cpp`).
- `ExecuteServerSimulationHarnessRigCommand` (`Rigs.cpp:582-600`) tests each
  command name twice and ends in a statement nothing reaches. Lines 584-587
  return unless the name is `replay_transfer_capture` or `inject_payload`;
  lines 589-593 handle the first; so the test at line 594 is always true and
  the `return false` at line 599 is unreachable.
  `.agents/references/cpp-conventions.md` (first bullet) excludes a fallback
  for a case nothing reaches. The sibling dispatchers are a plain chain of
  `if (command == ...) { ...; return true; }` ending in a reachable
  `return false` (`AgentCommandsServer.cpp:21-53`;
  `Engine/Source/Agent/AgentCommandsServerGeneric.cpp`
  `ExecuteServerAgentCommand`).

The two share one constraint, `Bind` (`Rigs.cpp:41-48`), which binds the
module's one state slot to the live `ServerSession`. It is called twice,
redundantly: at `Rigs.cpp:588`, which the leading guard exists only to place,
and at `Rigs.cpp:605` inside `QueueAgentStatusChange`.
`CommandReplayTransferCapture` (`Rigs.cpp:55-69`) reads no slot state, and
`inject_payload` reaches the slot only through `QueueAgentStatusChange`.
Exactly one `Bind` call must remain: with none,
`DrainPendingAgentStatusChanges`, `ResetPendingAgentStatusChanges`, and
`CountPendingAgentStatusChanges` (`Rigs.cpp:609-677`) see an unbound slot and
ignore a filled queue. Each finding as first written removed a different one
of the two `Bind` calls, so they are decided together.

Unmet criteria: no exported name without an outside caller; each command name
tested once and no unreachable statement.

### WriteRow's iCount parameter

In `FrameEdit.h`, `WriteRow` (`:278`) takes `int64_t iCount` beside
`POST_RENDER& rPostRender`, while its mirror `ReadRow` reads
`rPostRender.iCount` itself (`:420`). The one `WriteRow` caller,
`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp:109`,
passes `std::get<INDICES>(postRenders).iCount` and the same
`std::get<INDICES>(postRenders)` as `rPostRender`, so the parameter restates
an argument. The `ReadRow` caller (`:254`) passes elements of the same tuple,
so every `POST_RENDER` that reaches `WriteRow` has `iCount`.

Unmet criterion: `WriteRow` takes no count parameter that its `rPostRender`
argument already supplies.

## Design

1. Move the `IntegerFromValue` template, unchanged and under the same name,
   from `FrameEdit.h` to `AgentCommandsShared.h`, with a one-line comment in
   the style of its neighbours. `FrameEdit.h` callers need no edit: the header
   reaches them through `Engine.h` in the PCH.
2. `FrameEdit.h`: remove `int64_t iCount` from `WriteRow`'s parameter list
   (`:278`) and read the count from `rPostRender.iCount` as `ReadRow` does,
   for the uses at `:291`, `:294`, and `:296`. `ServerFrameEdit.cpp:109`:
   drop the `.iCount` argument. Step 1 removes lines above `WriteRow`, so
   these line numbers shift.
3. `Rigs.cpp` `ParseStatusChangeField`: replace the bodies of the four
   branches with one call each:
   `engine::IntegerFromValue<int64_t>(rValue, kName)` for `int64_t` and for
   `GlobalId::iValue`, `<uint8_t>` for `uint8_t`, and `<Underlying>` for the
   flags branch. The minted-`globalId` refusal (`Rigs.cpp:176-179`) and every
   non-integer branch stay.
4. `Rigs.cpp`: delete lines 584-588 of
   `ExecuteServerSimulationHarnessRigCommand` (the leading guard and its
   `Bind`), leaving two plain branches and a reachable `return false`: the
   sibling shape. Calling `CommandInjectPayload` unconditionally after the
   first branch, as the refactor-clean report first proposed, is not
   recommended, because no sibling dispatcher has that shape.
5. `Rigs.cpp`: make `QueueAgentStatusChange` `static` and move its definition,
   with its comment and its `Bind(rSession)` call (line 605), above
   `CommandInjectPayload`. This is the one remaining `Bind` site.
6. `Rigs.h`: delete the `QueueAgentStatusChange` declaration (line 28) and the
   `engine::GridCoord` forward declaration block (lines 5-10), after
   confirming nothing else in the header names `engine::GridCoord`.
7. `Engine/Source/Agent/AGENTS.md` `## Architecture`: add "integer" to the
   reader list in the `AgentCommandsShared.h` sentence.
8. `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`
   `inject_payload` value list: at line 218 ("`int64_t` a JSON integer") and
   at line 230 ("`flagshipGlobalId` an integer"), say the integer must be
   within `int64_t`, as the `edit_frame` list already does.

Steps 3 to 5 edit different functions of `Rigs.cpp`; the line numbers each
cites shift once an earlier step lands.

The comment at `Rigs.cpp:29-32` lists `QueueAgentStatusChange` among the
functions that would move together into engine code; it stays accurate and is
not edited.

Behavior changes this Plan owns, all in `inject_payload` error handling:

- The `int64_t` and `GlobalId` messages gain a range suffix: the text
  `must be an integer` becomes `must be an integer in [<min>,<max>]`. No
  document or script quotes the old text (a search of `*.md`, `*.ps1`, and
  `*.json` outside `Temp/` and `ThirdParty/` finds none).
- An unsigned JSON value above `INT64_MAX` is refused for those two kinds;
  today `get<int64_t>()` wraps it, because `is_number_integer()` is true for
  unsigned values (`ThirdParty/json/include/nlohmann/json.hpp:2853-2856`).
- The `uint8_t` and flags messages keep their text.

Everything else is behavior-preserving:

- `WriteRow` reads the same count from the same object.
- Rig dispatch is unchanged under the session invariant: `ServerSession` is a
  singleton whose destructor calls `DetachServerSimulationHarnessRigs`
  (`Source/Network/Server/ServerSession.cpp:19-30`), so the slot is either
  unbound with an empty queue or bound to the live session. The removed
  `Bind` at line 588 therefore only ever bound an empty slot early; with it
  gone the slot binds when the first destroy or update entry is queued, and
  until then Drain, Reset, and Count have nothing to act on.

## Critical files

- `Engine/Source/Agent/AgentCommandsShared.h`
- `Engine/Source/Agent/Commands/FrameEdit.h`
- `Engine/Source/Agent/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.cpp`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.h`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp`
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`

## In scope

- `engine::IntegerFromValue`: its definition moves from `FrameEdit.h` to
  `AgentCommandsShared.h`.
- `WriteRow` in `FrameEdit.h`: its parameter list and its three uses of
  `iCount`; the `engine::WriteRow` call in `ServerFrameEdit.cpp`.
- `ParseStatusChangeField`: the `int64_t`, `uint8_t`, flags, and `GlobalId`
  branches.
- `ExecuteServerSimulationHarnessRigCommand`: its leading guard and `Bind`
  call (lines 584-588).
- `QueueAgentStatusChange`: linkage and position of its definition in
  `Rigs.cpp`; its declaration in `Rigs.h`.
- The `engine::GridCoord` forward declaration in `Rigs.h`.
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
- Every part of `FrameEdit.h` other than the `IntegerFromValue` move and
  `WriteRow`'s count parameter: `ReadElement`, `ReadRow`, `WriteKeys`,
  `ReadKeys`, and the `ReadElement`/`WriteElement` names.
- A shared helper for the index read that `WriteRow` (`:289-297`) and
  `ReadRow` (`:421-429`) both hold: the pair is a deliberate mirror and the
  analysis did not accept the helper.
- `edit_frame` error text.
- `Bind`, the `Bind` call inside `QueueAgentStatusChange`, and the body of
  `QueueAgentStatusChange`.
- `DrainPendingAgentStatusChanges`, `ResetPendingAgentStatusChanges`,
  `CountPendingAgentStatusChanges`, `DetachServerSimulationHarnessRigs`, and
  their session tests.
- Moving the queue or its functions into engine code (the deferral the comment
  at `Rigs.cpp:29-32` records).
- `CommandInjectPayload` other than where its callee is defined.
- `AGENTS.md` files other than `Engine/Source/Agent/AGENTS.md`: none names
  `QueueAgentStatusChange`.

## Risk

- Change Workflow tier: Tier 2 (the authors' classification). Triggers: one
  tool's scoped behavior changes (two error texts and one refused value in
  `inject_payload`); the signature of an engine template that game code calls
  (`WriteRow`) changes; a declaration leaves a game header that engine
  translation units include
  (`Engine/Source/Agent/Commands/ReplayHarnessRigs.h:5`); and the change
  leaves one session-slot `Bind` site that Drain, Reset, and Count depend on.
  Each exceeds Tier 1's "no public signature or invariant exposure".
- Invariant: exactly one `Bind` call remains, inside `QueueAgentStatusChange`.
- The agent command channel is an exempt developer tool, so no trust boundary
  changes; `edit_frame` is a server-only developer command.
- Exposure: build (an `Engine.h`-aggregated header changes, so every PCH
  rebuilds) and the live checks below. The drain writes `FrameInput`, which
  replay records (`Rigs.cpp:32`); nothing about what is queued or its order
  changes. No determinism/CRC, serialization, `.pack`, `kiVersion`, wire,
  affinity, threading, allocation, or shader exposure.

## Acceptance criteria

1. `IntegerFromValue` is defined once, in `AgentCommandsShared.h`, and
   `ParseStatusChangeField` calls it in four branches and holds no
   `is_number_integer()` call.
2. Code read: `WriteRow` has no `iCount` parameter and its one caller passes
   none.
3. `QueueAgentStatusChange` appears only in `Rigs.cpp`, as a `static`
   function defined before its one call and in the comment at lines 29-32.
4. `Bind(` is called exactly once in `Rigs.cpp`, inside
   `QueueAgentStatusChange`.
5. `ExecuteServerSimulationHarnessRigCommand` compares `command` against each
   of its two names once and its final `return false` is reachable.
6. `/compile` builds the BrokenEngineSandbox client and server and
   ModelViewer (all three include `Engine.h` in their PCH).
7. One `/agent-harness` server run (the originating analyses ran nothing, so
   they record no harness setup) observes:
   - an `inject_payload` entry with a non-integer `iUuid` fails with the
     ranged message, one whose `iUuid` is an integer above `INT64_MAX` (for
     example `9223372036854775808`) fails with the same message, and one with
     an integer `iUuid` within `int64_t` is accepted as before;
   - after an `inject_payload` batch holding a `DestroyShip` or `UpdateShip`
     entry with a later `tick`, `status` reports a nonzero
     `pendingAgentStatusChangeCount`, and the entry applies at its tick;
   - `replay_transfer_capture` still answers;
   - `edit_frame` integer writes behave as before.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
`Documents/Plans/Game/ShipCrcTuplesAndOmniTransfer.md` and
`Documents/Plans/Game/MissileTransferCleanup.md` edit other functions of
`Rigs.cpp`. The regions do not overlap and no order is required; line numbers
here shift once one of them lands.
