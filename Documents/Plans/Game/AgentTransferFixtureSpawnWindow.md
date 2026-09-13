<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-13T00:23:10.923Z","dependsOn":[]} -->
# Keep agent transfer fixtures out of the client spawn-assignment window

## Context

`ServerClientManager::FinalizeNewClients`
(`Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp:111-167`)
assigns a connecting client its player by diffing the `kOriginCoord` player set
against `mPreSpawnPlayerIds` and handing the waiting clients, in order, whatever
player ids appeared since the snapshot. Any other producer of a new origin-cell
player during that window is therefore indistinguishable from the client's own
spawn, and the client is assigned the foreign player, including its global id,
client GUID write-back, and fleet association.

The agent command surface already recognizes this. `ClientsWaitingForSpawn()`
(`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:507-512`)
exists precisely for it, and its comment states the invariant.
`CommandSpawnPlayers` (`:677-686`), `CommandInjectStatusChanges` (`:572-575`),
and `CommandInjectOutwardTransfer` (`:738-741`) all refuse while it is true, and
`DrainPendingAgentStatusChanges` (`:918-939`) additionally holds its queue with
an early `return` while a client waits (`:932-935`).

`CommandReplayTransferFixture` (`:247-407`) does not refuse. It checks
`gpGame->mbReplaying`, the recording/pending-start state, the type, the
coordinates, and the pause arming, then mints a global player id for the
`player` case (`:390`) and queues the arrival through
`QueueReplayTransferFixture` (`:910`) into `sFixture.replayTransferFixtures`.

The command-time refusal alone is also incomplete, for both fixture commands.
Agent commands drain at `Engine/Source/GameBase.cpp:309`, between the first
`Poll` and `PollTickBoundary` (`:321`), while `ServerSession::AfterNetworkPoll`
runs `ServerClientManager::NewClients` and the fleet spawn requests at every
poll (`Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:243-255`).
A client whose spawn request arrives in the second poll therefore enters
`mClientsWaitingForSpawn` after the command already passed its check, and
`ServerBroadcaster::BuildFrameInputs` snapshots the pre-spawn ids for that same
tick (`Engine/Source/Network/Server/ServerBroadcaster.cpp:102-105`). Unlike
`DrainPendingAgentStatusChanges`, `DrainReplayTransferFixtures` (`:967-989`) has
no waiting-for-spawn deferral, so the queued arrival lands at the origin that
tick and `FinalizeNewClients` (`ServerClientManager.cpp:122-140`) can pair the
waiting client with the synthetic id. Both `replay_transfer_fixture` and the
already-guarded `inject_outward_transfer` reach the mis-assignment this way,
because both queue into `sFixture.replayTransferFixtures`.

This is the same defect class the session that surfaced it fixed in
`inject_outward_transfer`; that session's claimed Plan excluded changes to the
replay fixtures, so the remaining `replay_transfer_fixture` refusal and the
shared drain deferral are both out-of-scope, pre-existing debt. The drain gap is
accepted finding CXX002 of that session's focused C++ re-review.

## Design

Two changes, both mirroring code that already exists in the same file.

Author's recommendation, first: add the existing refusal to
`CommandReplayTransferFixture`, mirroring `CommandSpawnPlayers` and the
`inject_outward_transfer` fix — immediately after the `mbReplaying` check and
before the recording-state check, so the command fails before it validates
parameters, mints a global id, arms a writer pause, or touches the fixture
queue:

```
if (ClientsWaitingForSpawn())
{
	throw std::runtime_error("cannot inject while clients are waiting for spawn");
}
```

Recommended as an unconditional refusal rather than one narrowed to
`type:"player"` or to an origin destination: it keeps the four agent
injection commands textually parallel, it needs no reasoning about which
destination the origin diff can observe, and the fixture is a
`kbDebugInput` harness tool whose scenarios do not run during client connect.
The narrower player-only variant is available if a future scenario needs a
spaceship, blaster, or missile fixture mid-connect; it is not needed now.

Second, and this is the part that actually closes the window: add the same early
`return` deferral `DrainPendingAgentStatusChanges` uses to
`DrainReplayTransferFixtures`, after the existing `sFixture.pSession` check and
before the loop that merges the queue into `ServerTransferManager::mTransfers`:

```
if (!rSession.mpClientManager->mClientsWaitingForSpawn.empty())
{
	return;
}
```

The queue is not cleared on that path, so every queued fixture stays intact and
drains at the first harvest after the waiting list empties, exactly as pending
agent status changes already behave. This covers `inject_outward_transfer` as
well, since both commands feed the same queue, and it is what makes the fix
complete rather than a narrowing of the window.

Recommended sequencing note for the implementer: keep the command-time refusal
even though the drain deferral subsumes it. The refusal is the mirrored first
line the other three commands present, it fails the harness call loudly instead
of silently postponing it, and it prevents the command from minting a global id
and arming a writer pause for a transfer that will not run this tick.

Update the `replay_transfer_fixture` row in
`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:40` to
state the new precondition alongside the existing "requires no active replay, a
ready active source frame, and distinct Chebyshev-adjacent coordinates"
sentence, so a harness author sees the refusal before hitting it. The
`inject_outward_transfer` row (`commands-server.md:50`) states that the next
finalized tick spawns the arrival; extend it to say the drain also waits while a
client is waiting for spawn, because that row's timing claim is otherwise wrong
after the deferral lands.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:247-407` — `CommandReplayTransferFixture`, the command missing the refusal.
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:507-512` — `ClientsWaitingForSpawn` and the invariant comment.
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:677-686` — `CommandSpawnPlayers`, the refusal shape to mirror.
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:967-989` — `DrainReplayTransferFixtures`, the unguarded drain that gets the deferral.
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:918-939` — `DrainPendingAgentStatusChanges`, whose `:932-935` early `return` is the deferral shape to mirror.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp:111-167` — the snapshot-diff spawn assignment, with the pairing loop at `:122-140`.
- `Engine/Source/GameBase.cpp:309,321` — the agent command drain between the two polls, which is why a command-time check can go stale within one update.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:243-255` — `AfterNetworkPoll`, which enqueues waiting clients at every poll.
- `Engine/Source/Network/Server/ServerBroadcaster.cpp:102-105` — the pre-spawn snapshot refresh that fixes which ids count as new.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:40,50` — the `replay_transfer_fixture` and `inject_outward_transfer` contract rows.

## In scope

- One `ClientsWaitingForSpawn()` refusal in `CommandReplayTransferFixture`,
  placed before parameter validation, global id minting, pause arming, and
  queueing, using the existing failure message text.
- One early-`return` deferral in `DrainReplayTransferFixtures` while
  `mClientsWaitingForSpawn` is non-empty, placed after the `sFixture.pSession`
  check and before the merge loop, leaving the queue untouched.
- The `replay_transfer_fixture` and `inject_outward_transfer` rows in
  `commands-server.md`, extended with the precondition and the deferral.

## Out of scope

- `QueueReplayTransferFixture`, the fixture reset paths, and any change to what
  the drain merges once it does run.
- `DrainPendingAgentStatusChanges` and the other agent commands that already
  carry the refusal, including any change to their message text or placement.
- `ServerClientManager`, `AfterNetworkPoll` ordering, the pre-spawn snapshot,
  the agent-command drain position in `GameBase`, and the spawn-assignment
  algorithm itself.
- The `replay_transfer_fixture` request or response schema,
  `pauseAfterWriterInput` arming semantics, and the harvest, sort, capture, and
  replay logic itself — the deferral changes only which tick a fixture transfer
  reaches an unchanged harvest.
- Client-side agent commands, the wire protocol, determinism, CRC, save, and
  replay payload formats.

## Risk tier and invariants

Expected Change Workflow Tier 2. Trigger: a `kbDebugInput` agent fixture can
mint a player and hand it to a connecting client through the origin-cell spawn
diff — an ownership exposure on one server subsystem's tool surface. The fix
adds one precondition at an existing command boundary and one deferral that
mirrors the sibling drain, with no format, schema, or trust-model change, so it
stays scoped behavior. It shifts which tick a `kbDebugInput` fixture transfer
is applied on, and therefore which tick replay captures it, but only while a
client is waiting for spawn — a state no replay determinism scenario runs in.

Preserve these invariants:

- No producer of an origin-cell player on the agent fixture path can materialize
  one while `mClientsWaitingForSpawn` is non-empty, so a connecting client can
  only ever be assigned its own spawn — enforced at the drain, so a client that
  starts waiting after the command was accepted is covered too.
- The refusal happens before the command mints a global id, arms a writer pause,
  or enqueues anything, leaving no partial state behind on rejection.
- The deferral never drops or reorders a queued fixture: the queue is left
  untouched and drains whole at the first harvest after the waiting list empties.
- With no client waiting, `replay_transfer_fixture` and
  `inject_outward_transfer` behave exactly as they do today for every type,
  including pause arming and replay capture.
- No determinism, CRC, replay payload, save, or wire layout changes.

## Acceptance criteria

- The diff shows exactly one added refusal in `CommandReplayTransferFixture`,
  ahead of every state-mutating step, matching the `CommandSpawnPlayers` shape
  and message.
- The diff shows exactly one added early `return` in
  `DrainReplayTransferFixtures`, ahead of the merge loop, matching the
  `DrainPendingAgentStatusChanges` shape, with no clear of
  `sFixture.replayTransferFixtures` on that path.
- `commands-server.md` documents the precondition on the
  `replay_transfer_fixture` row and the waiting-for-spawn deferral on the
  `inject_outward_transfer` row.
- Live: with no client waiting, a `player` fixture and a `missile` fixture each
  still queue, transfer, and capture as documented, and the recorded run replays
  with no checksum or `CONFIRMED DESYNC` line.
- Live: an `inject_outward_transfer` arrival queued before a client connects
  still spawns and still reaches its edge crossing after that client's spawn is
  assigned, with the client holding its own player, not the synthetic one.
- Server `Debug|x64` builds clean through `/compile`.

## Notes

The waiting-for-spawn state is short and is not reachable on demand from the
harness, so the rejection branch and the deferral branch are proven by the diff
and by the identical, already-accepted branches in the sibling command and
sibling drain, rather than by a runtime scenario that forces the race.

The command-time refusal is kept even though the drain deferral subsumes it:
without the refusal a harness call would silently postpone instead of failing,
and the command would still mint a global id and arm a writer pause for a
transfer held back that tick.

`Documents/Plans/Engine/ReplayTransferCaptureFixtureEventPin.md` also edits the
`replay_transfer_fixture` row of `commands-server.md`, for unrelated
last-event wording. Neither Plan depends on the other; whichever lands second
keeps both the pinned-capture wording and this precondition in the row.
