<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:41:27.960Z","dependsOn":[]} -->
# Make QueueAgentStatusChange file-local and test each rig command name once

## Context

Two findings in the game server simulation-harness-rig module share one
constraint, so they are one change. Verified against commit `035db8bf`; `Rigs`
below is
`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs`.

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

The shared constraint is `Bind` (`Rigs.cpp:41-48`), which binds the module's
one state slot to the live `ServerSession`. It is called twice, redundantly:
at `Rigs.cpp:588`, which the leading guard exists only to place, and at
`Rigs.cpp:605` inside `QueueAgentStatusChange`. `CommandReplayTransferCapture`
(`Rigs.cpp:55-69`) reads no slot state, and `inject_payload` reaches the slot
only through `QueueAgentStatusChange`. Exactly one `Bind` call must remain:
with none, `DrainPendingAgentStatusChanges`, `ResetPendingAgentStatusChanges`,
and `CountPendingAgentStatusChanges` (`Rigs.cpp:609-677`) see an unbound slot
and ignore a filled queue.

Originating gap: the `/external-deep-analysis` run over
`Projects/BrokenEngineSandbox/Source/Agent`. The export was found by its
architecture review (Phase 1), the dispatcher by its refactor-clean pass
(Phase 2); independent verification (Phase 3) accepted both on condition that
they are decided together, because each finding as first written removed a
different one of the two `Bind` calls. Unmet criteria: no exported name
without an outside caller; each command name tested once and no unreachable
statement. Both predate that run, which changed no file.

## Design

Recommended:

1. `Rigs.cpp`: delete lines 584-588 of
   `ExecuteServerSimulationHarnessRigCommand` (the leading guard and its
   `Bind`), leaving two plain branches and a reachable `return false`: the
   sibling shape. Calling `CommandInjectPayload` unconditionally after the
   first branch, as the refactor-clean report first proposed, is not
   recommended, because no sibling dispatcher has that shape.
2. `Rigs.cpp`: make `QueueAgentStatusChange` `static` and move its definition,
   with its comment and its `Bind(rSession)` call (line 605), above
   `CommandInjectPayload`. This is the one remaining `Bind` site.
3. `Rigs.h`: delete the `QueueAgentStatusChange` declaration (line 28) and the
   `engine::GridCoord` forward declaration block (lines 5-10), after
   confirming nothing else in the header names `engine::GridCoord`.

The comment at `Rigs.cpp:29-32` lists `QueueAgentStatusChange` among the
functions that would move together into engine code; it stays accurate and is
not edited.

Behavior is unchanged under the session invariant: `ServerSession` is a
singleton whose destructor calls `DetachServerSimulationHarnessRigs`
(`Source/Network/Server/ServerSession.cpp:19-30`), so the slot is either
unbound with an empty queue or bound to the live session. The removed `Bind`
at line 588 therefore only ever bound an empty slot early; with it gone the
slot binds when the first destroy or update entry is queued, and until then
Drain, Reset, and Count have nothing to act on.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.cpp`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.h`

## In scope

- `ExecuteServerSimulationHarnessRigCommand`: its leading guard and `Bind`
  call (lines 584-588).
- `QueueAgentStatusChange`: linkage and position of its definition in
  `Rigs.cpp`; its declaration in `Rigs.h`.
- The `engine::GridCoord` forward declaration in `Rigs.h`.

## Out of scope

- `Bind`, the `Bind` call inside `QueueAgentStatusChange`, and the body of
  `QueueAgentStatusChange`.
- `DrainPendingAgentStatusChanges`, `ResetPendingAgentStatusChanges`,
  `CountPendingAgentStatusChanges`, `DetachServerSimulationHarnessRigs`, and
  their session tests.
- Moving the queue or its functions into engine code (the deferral the comment
  at `Rigs.cpp:29-32` records).
- `CommandInjectPayload` other than where its callee is defined, and
  `ParseStatusChangeField`.
- `AGENTS.md` files: none names `QueueAgentStatusChange`.

## Acceptance criteria

1. `QueueAgentStatusChange` appears only in `Rigs.cpp`, as a `static`
   function defined before its one call and in the comment at lines 29-32.
2. `Bind(` is called exactly once in `Rigs.cpp`, inside
   `QueueAgentStatusChange`.
3. `ExecuteServerSimulationHarnessRigCommand` compares `command` against each
   of its two names once and its final `return false` is reachable.
4. `/compile` builds the BrokenEngineSandbox server and client.
5. Through `/agent-harness`: after an `inject_payload` batch holding a
   `DestroyShip` or `UpdateShip` entry with a later `tick`, `status` reports a
   nonzero `pendingAgentStatusChangeCount`, and the entry applies at its tick;
   `replay_transfer_capture` still answers. The originating analysis ran
   nothing, so it records no harness setup.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

- Change Workflow tier: Tier 2 (the author's classification). Trigger: a
  declaration leaves a game header that engine translation units include
  (`Engine/Source/Agent/Commands/ReplayHarnessRigs.h:5`), and the change
  leaves one session-slot `Bind` site that Drain, Reset, and Count depend on,
  more than Tier 1's "no public signature or invariant exposure" allows.
- Exposure: the drain writes `FrameInput`, which replay records
  (`Rigs.cpp:32`); nothing about what is queued or its order changes. No
  serialization, `.pack`, `kiVersion`, wire, affinity, threading, allocation,
  or shader exposure.
- `Documents/Plans/Engine/ShareAgentIntegerReader.md`,
  `Documents/Plans/Game/RemoveShipNavigationDelayColumn.md`, and
  `Documents/Plans/Game/RemoveWriteOnlyMissileTargetFlags.md` edit other
  functions of `Rigs.cpp`. The regions do not overlap and no order is
  required; line numbers here shift once one of them lands.
