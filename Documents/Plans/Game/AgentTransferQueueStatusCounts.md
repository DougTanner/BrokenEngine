<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-13T00:20:48.501Z","dependsOn":[]} -->
# Report the server transfer and agent fixture queue depths in the status command

## Context

Harness acceptance rows that must prove "the transfer queue is unchanged" have
no server agent query that reads the transfer queues, so they settle the claim
only indirectly: the absence of a destination frame, the absence of a new
`status.activeCoords` entry, or the absence of a transfer log line. Each of
those is a downstream consequence, not the queue itself, and each stays silent
when a transfer was queued but discarded, or queued into an already-active
coordinate.

The three queues the rows care about are live server-side state with no reader:

- `engine::ServerTransferManager::mTransfers`
  (`Engine/Source/Network/Server/ServerTransferManager.h:33`) is public, but
  nothing outside the manager and `DrainReplayTransferFixtures` reads it.
  `HarvestTransfers` (`Engine/Source/Network/Server/ServerTransferManager.cpp:300-326`)
  clears it at entry, fills it from `CollectTransfers` plus the game fixture
  drain, captures it for replay, and applies it before returning, so between
  ticks it holds exactly the batch the most recent harvest applied — an empty
  map means that harvest transferred nothing.
- `sFixture.pendingAgentStatusChanges`
  (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:27`)
  holds injected status changes until `DrainPendingAgentStatusChanges` moves
  them into `mFrameInputs` (`:933-960`).
- `sFixture.replayTransferFixtures` (`:28`) holds queued transfer fixtures until
  `DrainReplayTransferFixtures` (`:963-984`) merges them into `mTransfers`.

Both fixture maps live in the file-static `ServerSimulationFixtureState`
(`:24-31`) with no accessor; `ServerSimulationFixtures.h:31-34` exposes only the
drain and reset entry points.

The server `status` command already publishes a queue depth by exactly this
means: `CommandStatus`
(`Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp:84-109`)
ends with
`rResult["pendingFlagshipUpdateCount"] = std::ssize(gpServerSession->mpFleetManager->mNavigation.mPendingFlagshipUpdates);`
and `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:7`
documents it. The gap is that no equivalent exists for the transfer path.

This is pre-existing observability debt: no change introduced it, and it is
outside the boundary of the agent fixture work that surfaced it.

## Design

Author's recommendation: extend the existing `status` command rather than add a
new command, because `status` is the command every harness scenario already
polls and the values are three integers the server already holds.

Add three scalar fields to the `CommandStatus` result, each a total across the
map's vectors computed with the values already in memory:

- `lastHarvestedTransferCount` — total `game::StatusChange` entries across
  `gpServerSession->mpTransferManager->mTransfers`. Named for what it is: the
  batch the most recent `HarvestTransfers` applied, not a still-pending queue.
  A finalized tick that transferred nothing leaves it `0`, which is the direct
  form of the "transfer queue unchanged" assertion. While paused, no harvest
  runs, so the value stays at the last harvest's batch.
- `pendingTransferFixtureCount` — total entries across
  `sFixture.replayTransferFixtures`, still waiting for the next harvest.
- `pendingAgentStatusChangeCount` — total entries across
  `sFixture.pendingAgentStatusChanges`, still waiting for the next drain.

Because the two fixture maps are file-static, add two accessors to
`ServerSimulationFixtures.h` mirroring the existing per-queue
`ResetPendingAgentStatusChanges` / `ResetReplayTransferFixtures` pair, each
taking the `ServerSession&` and returning `0` when
`sFixture.pSession != &rSession`, so the session-attachment guard every other
fixture entry point applies is not duplicated at the call site.

`CommandStatus` reads `mTransfers` through the existing
`gpServerSession->mpTransferManager` handle, the same way it reads
`mpFleetManager` one line earlier; `ServerTransferManager.h:9` states that its
consumers include it directly, so add that include to `AgentCommandsServer.cpp`
if the complete type is not already visible there.

Recommended scope discipline: scalar totals only. A per-coordinate breakdown
would enlarge every `status` response for a need no current acceptance row has;
the totals settle the "unchanged" assertions, and a breakdown can be added later
if a row needs one.

Document the three fields in the `status` bullet of
`commands-server.md`, including the "most recent harvest, stale while paused"
meaning of `lastHarvestedTransferCount`, so a harness author cannot read it as a
pending count.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp:84-109` — `CommandStatus` result publication and the `pendingFlagshipUpdateCount` precedent.
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:24-31,933-984` — file-static fixture queues and their drains.
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.h:31-34` — fixture entry-point declarations the accessors join.
- `Engine/Source/Network/Server/ServerTransferManager.h:9,33` — direct-include note and the `mTransfers` map.
- `Engine/Source/Network/Server/ServerTransferManager.cpp:300-326` — `HarvestTransfers` clear/fill/apply order that fixes the field's meaning.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:7` — the `status` response contract.
- `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md` — server agent command surface documentation.

## In scope

- Three new integer fields in the `CommandStatus` result in
  `AgentCommandsServer.cpp`, computed from
  `gpServerSession->mpTransferManager->mTransfers` and the two new fixture
  accessors.
- Two read-only count accessors for `sFixture.replayTransferFixtures` and
  `sFixture.pendingAgentStatusChanges`, declared in
  `ServerSimulationFixtures.h` and defined in `ServerSimulationFixtures.cpp`
  with the existing session-attachment guard.
- Any include `AgentCommandsServer.cpp` needs for the complete
  `engine::ServerTransferManager` type.
- The `status` bullet in `commands-server.md`, and the
  `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md` line if it enumerates
  the status fields.

## Out of scope

- Any change to transfer behavior: harvest timing, the clear/fill/apply order in
  `HarvestTransfers`, sorting, destination materialization, replay capture, or
  either fixture drain.
- A new agent command, a new query family, per-coordinate or per-type
  breakdowns, and any change to the existing `query_frame`, `query_players`,
  `query_collection`, or transfer-fixture responses.
- Client-side agent commands, the wire protocol, `StatusChange` layout,
  determinism, CRC, replay, and save formats.
- Making `mTransfers` private, adding a `ServerTransferManager` accessor, or any
  other engine-side restructuring.
- Rewriting existing harness scenarios or acceptance rows to consume the new
  fields.

## Risk tier and invariants

Expected Change Workflow Tier 2. Trigger: a new public signature on the game's
server agent fixture surface plus new fields in the `status` command response
that harness scenarios parse — scoped behavior of one subsystem, with no
determinism, CRC, wire, serialization, threading, or trust-boundary exposure.

Preserve these invariants:

- The additions are read-only: no queue is cleared, reordered, or drained by a
  `status` call, and no simulation state changes.
- `CommandStatus` continues to run on the server main thread between ticks, the
  same context in which it already reads `mPendingFlagshipUpdates`; no new
  synchronization is introduced and no queue is read from another thread.
- Existing `status` fields keep their names, types, and meanings.
- No CRC, replay, save, or wire layout changes.

## Acceptance criteria

- `status` returns `lastHarvestedTransferCount`, `pendingTransferFixtureCount`,
  and `pendingAgentStatusChangeCount` as integers on a fresh paused server, all
  `0`.
- After `inject_outward_transfer` while paused,
  `pendingTransferFixtureCount` is `1` and `lastHarvestedTransferCount` is
  unchanged; after unpausing for one finalized tick, the fixture count returns
  to `0` and `lastHarvestedTransferCount` reports the applied batch.
- A tick with no transfer leaves `lastHarvestedTransferCount` at `0`, so a
  harness row can assert "no transfer occurred" from the value alone.
- `inject_status_changes` while paused raises
  `pendingAgentStatusChangeCount`, which returns to `0` after the drain.
- Server `Debug|x64` builds clean through `/compile`, and
  `commands-server.md` documents all three fields including the harvest-batch
  meaning.

## Notes

The residual was raised because acceptance rows asserting "transfer queue
unchanged" could cite only indirect evidence. Naming the harvest field for the
batch it actually holds is deliberate: `mTransfers` is not a queue that
accumulates across ticks, and a field named `pendingTransferCount` would invite
exactly the misreading the indirect evidence already caused.
