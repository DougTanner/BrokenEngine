<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-12T00:11:16.917Z","dependsOn":[]} -->
# Add a server agent fixture that activates a frame at a supplied coordinate

## Context

The server refuses an outward transfer whose destination leaves the signed-int32
coordinate domain: `ServerTransferManager.cpp:72-81` calls
`engine::TryAddGridCoord` (`Engine/Source/Frame/FrameUtils.h:128-143`) and logs
"Transfer destination leaves the coordinate range" before any queue, publication,
or frame creation. That branch cannot be reached from the harness today. The
server dispatcher offers `status`, the packet/handshake fault fixtures, `pause`,
`timescale`, `save`, `load`, `reset`, and the `query_*` commands
(`Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp:296-365`),
and the simulation fixtures add `replay_record`, `replay_play`,
`replay_transfer_capture`, `replay_drop_retained_end_frame`,
`replay_inject_persistence_failure`, `replay_transfer_fixture`,
`inject_status_changes`, and `spawn_players`
(`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:734-772`).
None of them activates a frame at a caller-supplied coordinate:
`replay_transfer_fixture` requires an active recording, adjacent source and
destination, and an already-active, fully constructed source frame
(`ServerSimulationFixtures.cpp:255-332`), and it injects straight into the
destination transfer queue (`:858-878`) rather than producing a
`TransferRequest` from a source cell. `spawn_players` and
`inject_status_changes` likewise require an already-active cell.

Originating gap: `Documents/Plans/Engine/UnboundedRenderCoordinates.md`
acceptance criterion 4 ("unrepresentable outward transfers fail before partial
publication") was landed with its server half settled only by a code-review
trace of the call chain above, because no harness route can activate a cell at
`x = 2147483647` and drive a transfer out of it. This Plan supplies the missing
route; the checked arithmetic itself already exists and is not changed here.

Impact: the one guard that stops a coordinate wrap from silently teleporting an
entity to the far side of the grid has no live regression evidence.

## Design

Author's recommendation: add one `kbDebugInput`-gated server fixture command,
`activate_frame`, taking `{"coord":[x,y]}` and nothing else, alongside the
existing fixtures in
`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp`.
Parse the coordinate through the existing shared `CoordFromParam`
(`AgentCommandsServer.cpp:283-291`). Activate the cell exactly the way
`DrainReplayTransferFixtures` already does for a missing destination
(`ServerSimulationFixtures.cpp:866-874`): `gpGame->CreateFrameAtCoord(coord)`,
then prime `pNext` and swap so `pCurrent` is a constructed frame. Report the
resulting active state in the result so the caller can assert activation without
a second command; make an already-active coordinate a success reporting the same
state rather than an error. Reuse the existing per-session fixture attachment
and reset discipline (`ServerSimulationFixtures.cpp:882-906`) so an activated
cell does not outlive its session, and document the command in the harness
command reference.

Rationale for placing it with the simulation fixtures rather than in the shared
engine dispatcher: it mutates authoritative game cells, so it belongs with the
other game-side, Debug-only mutating fixtures instead of with the read-only
engine probes.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:725-772,858-906` — fixture dispatch, frame activation pattern, session attach/reset.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp:283-365` — shared `CoordFromParam` and the server dispatcher.
- `Engine/Source/GameBase.cpp:912` — `GameBase::CreateFrameAtCoord`, the activation entry point.
- `Engine/Source/Network/Server/ServerTransferManager.cpp:72-81` — the checked-destination branch the fixture must make reachable.
- `.agents/skills/agent-harness/references/command-reference.md` — the harness command contract this fixture must appear in.
- `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md` — server agent command and hostile-parameter documentation.

## In scope

- One `kbDebugInput`-gated `activate_frame` server fixture handler, its dispatch
  entry, its parameter validation, and its result shape.
- Frame activation through `GameBase::CreateFrameAtCoord` plus the existing
  `pCurrent`/`pNext` priming, and participation in the existing fixture session
  attach/detach/reset paths.
- Harness command-reference and server agent documentation for the new command.

## Out of scope

- Any change to the checked coordinate arithmetic, transfer semantics,
  publication order, or the error log at `ServerTransferManager.cpp:78`.
- Subscription, client grid-coord, or `SyncActiveFrames` policy; the fixture
  activates a cell directly and does not create a subscription.
- New or changed query schemas, replay fixtures, save/load behavior, wire
  format, CRC composition, or version gates.
- Coordinate type/range validation inside `CoordFromParam`, owned by
  `Documents/Plans/Game/AgentCoordinateIntegralValidation.md`.
- Retail-build exposure of the fixture, and new unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2. Trigger: agent JSON at the server trust
boundary creates authoritative simulation cells. The change adds one Debug-only
handler that calls an existing activation routine; no determinism, CRC, wire,
serialization, threading, or save/replay surface changes.

Preserve these invariants:

- The fixture exists only in `kbDebugInput` builds and fails with the existing
  build-gate error otherwise.
- Activation uses the existing `CreateFrameAtCoord` path; an activated cell is a
  normal active cell with no special-case state.
- A rejected coordinate activates nothing and leaves the frame map, transfer
  queues, and status queues unchanged.
- Simulation CRC, replay streams, save data, and wire layout are unchanged.

## Acceptance criteria

- With a Debug server, `activate_frame` at `[2147483647,0]` activates that cell,
  and driving an eastward transfer out of it produces exactly the
  "Transfer destination leaves the coordinate range" error with the source cell's
  transfer queue and published state unchanged and no destination frame created.
- A representable transfer out of an `activate_frame`-activated interior cell
  still lands, preserving destination ownership and destination-local position.
- `activate_frame` with a missing, non-array, or wrong-size `coord`, or with an
  extra parameter, fails without activating a cell; repeating it on an
  already-active coordinate succeeds and reports the same state.
- Server `Debug|x64` builds clean through `/compile`, and the new command's
  contract appears in the harness command reference.

## Notes

The fixture is verification observability for an existing guard, not a gameplay
capability: nothing in the shipping client or server can call it.
