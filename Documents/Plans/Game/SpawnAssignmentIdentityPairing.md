<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-17T17:13:16.671Z","dependsOn":[]} -->
# Pair a connecting client with its own spawned player

## Context

`ServerClientManager::FinalizeNewClients`
(`ServerClientManager.cpp:111-171`)
pairs each connecting client with a player it has no identity link to. It builds
`newPlayerIds` from every `kOriginCoord` player id absent from
`mPreSpawnPlayerIds` (`:121-131`), zips that list positionally against
`mClientsWaitingForSpawn` (`:136-167`), and assigns whatever it lands on: the
global id sent by `SendAssignPlayer`/`SendPlayerState`, the client GUID written
back into Frame state (`:157`), the `mClientPlayers` ownership record, and the
fleet association through `ServerFleetManager::OnPlayerSpawned`
(`Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetManager.cpp:288`).

Because the pairing is a snapshot diff, correctness is a property of every
producer of an origin-cell player rather than of `FinalizeNewClients` itself:
any player appearing between the snapshot capture
(`ServerBroadcaster.cpp:100-109`) and the pairing is
indistinguishable from a waiting client's own spawn. The cost is a guard per
producer, and every future spawner would owe one. The agent command surface
carries nine such sites in
`ServerSimulationFixtures.cpp`:
a forward declaration (`:54`), four command refusals in
`CommandReplayTransferFixture` (`:261`), `CommandInjectStatusChanges` (`:578`),
`CommandSpawnPlayers` (`:689`), and `CommandInjectOutwardTransfer` (`:744`), the
`ClientsWaitingForSpawn` helper with the comment stating the invariant
(`:513-518`), and two drain deferrals in `DrainPendingAgentStatusChanges`
(`:938`) and `DrainReplayTransferFixtures` (`:980`) that test
`mClientsWaitingForSpawn` directly rather than through the helper. The contract
is documented in the server `AGENTS.md` listed under `## Critical files`, line
25.

Two identities the pairing needs already exist and are both discarded.
`ServerBroadcaster::BuildFrameInputs` mints exactly one global id per waiting
entry with `gpGame->GenerateGlobalId()`
(`ServerBroadcaster.cpp:52-70`), emits it as that
entry's `kSpawnPlayer` change, and keeps it only in a local and a log line.
`Players` writes that id into the spawned player's `pGlobalPlayerIds` slot
(`Players.cpp:280`
extracts it, `:481` stores it), so after the tick the origin cell already holds
a unique per-request key. `GameBase::GenerateGlobalId`
(`Engine/Source/GameBase.h:233`) post-increments `miNextGlobalId` (`:325`), so
those ids are unique and monotonic and an agent-minted id can never collide with
a client's. Separately, each waiting entry already carries the connecting
client's `ClientGuid`
(`ServerClientManager.h:10-17`),
which the spawn request never carries and which the pairing therefore has to
patch into the row after the tick.

This is pre-existing design debt, not a defect of any one change.

## Design

Give the row its owner at birth, and key the pairing on the minted id. These two
halves are independent and both are required: the GUID makes the row owned, the
id makes the pairing unambiguous.

Four decisions are fixed and not open to the implementer: the player row is born
owned with the client GUID carried in the spawn request; the pairing key is the
per-entry minted global id; that GUID is not serialized over the network; and
all nine guard sites are removed. There is no backward compatibility — existing
replay recordings stop loading, and that is accepted.

**1. `ClientSpawnInfo` gains one `engine::global_id_t` member.**
Declare it last, after `iMemberIndex`
(`ServerClientManager.h:10-17`):
`QueueSpawnForClient` brace-initializes the struct positionally with exactly
four values
(`ServerClientManager.cpp:31`),
so a member inserted anywhere else silently rebinds that initializer. There is
no named unassigned-id constant in the tree and none should be added:
`engine::global_id_t` already defaults to a zero `iValue`, offers `IsValid()`,
and renders that value as `(none)` in logs
(`Engine/Source/Frame/Collections/CollectionId.h:10-15`,
`Engine/Source/Frame/Collections/CollectionId.h:111-127`).

**2. `SpawnPlayerData` gains one `engine::ClientGuid` member, declared but not
serialized.**
Declare it last in the struct
(`StatusChange.h:57-68`) so every
existing designated initializer stays valid. Leave it off the wire exactly as
`TransferData`'s client GUID already is — same header, same reason, and its
comment states the exclusion
(`StatusChange.h:161-163`). That keeps
the four coupled `kSpawnPlayer` codec sites in
`Engine/Source/Network/NetworkSerialization.cpp` untouched — the wire-size case
(`:142`), the write case (`:175`), the read case (`:337`), and the per-item
maximum — and keeps `engine::kuiProtocolVersion`
(`Engine/Source/Network/NetworkProtocol.h:63`) at its current value, so no
connected or older client is affected. The client therefore keeps building its
spawned row with an empty GUID until a coordinate full state ships the column,
exactly as it does today; that column is already in `SharedMembers()`
(`Players.h:266-283`),
so every full state carries it and nothing the client sees changes.

Two nearby sites need verification rather than edits, and the implementer should
record both. The receive side seats the default payload through
`DefaultDataForType`
(`StatusChange.h:174-184`) before
reading fields, which is why the unserialized member arrives zeroed rather than
uninitialized. And no byte cap needs raising: the wire per-item maximum of 120
bytes (`Engine/Source/Network/NetworkSerialization.h:18`) is unaffected because
the serialized spawn item does not grow, while the path that does grow — the raw
replay record write — has no per-item byte cap at all, only a count bound
against the stream
(`FrameInput.cpp:37-39`).

**3. Bump `FrameInput::kiVersion`, and nothing else.**
The replay difference stream writes each status-change payload as a raw
trivially-copyable struct
(`FrameInput.cpp:20-31`, through
`Common/Serialization.h:88-92`), so the added member silently grows the recorded
layout. Increment `FrameInput::kiVersion`
(`Projects/BrokenEngineSandbox/Source/Frame/FrameInput.h:11`). The gate that then
refuses an older recording is `DIFFERENCE_TYPE::kiVersion` inside the reader
(`Engine/Source/File/DifferenceStream.h:259-277`), reached through a template
parameter — a plain text search for the qualified constant finds no consumer and
must not be read as evidence that the bump is inert. Its companion
`sizeof(DIFFERENCE_TYPE)` check does not catch this, because `FrameInput` holds
a vector and its own size does not change.

`Frame::kiVersion` does not bump. It is a base term plus the navigation-data
version plus each collection's own version
(`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:30-33`), scoped by its own
comment to changes that shift computed frame CRCs. The spawn payload enters no
format it covers: the grid save serializes collections rather than frame inputs
(`Engine/Source/File/GridSave.cpp:15`), the replay's saved type is the Frame
whose layout is untouched, and no collection member is added, removed, or
reordered. Existing saves keep loading, which is the intent.

**4. `ServerBroadcaster::BuildFrameInputs` stores the id and stamps the GUID.**
In the waiting-entry loop
(`ServerBroadcaster.cpp:52-70`), store the id from
`GenerateGlobalId()` on the entry and set the payload's GUID member from the
entry's own `clientGuid`, in the same pass. The loop iterates by `const`
reference today and must iterate by mutable reference. Nothing else moves: not
the fleet lookup, not the stale-fleet drop above it, not the ordering.

The loop re-emits a spawn change for every still-waiting entry on every advancing
update, so a still-waiting entry's stored id is overwritten each advancing
update and the last one written is the one that update's spawn used. That is the
non-obvious part and deserves a comment. It is safe because `BuildFrameInputs`
runs once per update (`Engine/Source/GameBase.cpp:940-955`) and
`FinalizeFrameTick` clears each frame input's status changes after every tick
(`Engine/Source/GameBase.cpp:507-530`), so the change is consumed by the first
tick of the update and later ticks of a multi-tick update see none.

**5. `Players` stores the payload GUID at creation.**
Extract the GUID in the `kSpawnPlayer` branch alongside the existing fields
(`Players.cpp:279-290`),
leave it empty for the reserved `kRespawnPlayer` branch that carries no payload,
carry it through `PlayersPostRender::SpawnInfo`
(`Players.h:327-350`),
and store it where the row currently clears that column
(`Players.cpp:481`).
`SpawnInfo` is a transient spawn descriptor, absent from `Members()`,
`PersistentMembers()`, and every serialized tuple, so this is not a collection
layout change and no collection version moves.

Every other producer is already correct without an edit. Exactly three sites
construct the payload, plus one default read: this broadcaster loop;
`inject_status_changes`
(`ServerSimulationFixtures.cpp:455-462`);
`spawn_players` (`:715-720`); and the reserved-branch default read at
`Players.cpp:273`. Both agent sites use designated initializers that omit the new
member, so their players are born unowned, which is what the harness
documentation already promises. `inject_outward_transfer` is not a producer — it
builds a `TransferData` (`:808-820`) and keeps its existing empty-GUID behavior.
Transfer arrivals likewise call the same `Spawn(SpawnInfo)` overload without the
new field (`Projects/BrokenEngineSandbox/Source/SpawnTransfer.cpp:76`), so the
row is born unowned and `ServerTransferManager` still assigns ownership
immediately afterwards through `AssignRegistryClientGuid`
(`Engine/Source/Network/Server/ServerTransferManager.cpp:277`); transfer
behavior must come out bit-identical.

**6. `FinalizeNewClients` resolves each waiting entry by its stored id.**
For each entry, scan the origin cell's `pGlobalPlayerIds` for a row equal to the
stored id and use that row index for `SendAssignPlayer`, `SendPlayerState`,
`mClientPlayers.Add`, and `OnPlayerSpawned`. A linear scan is recommended over a
new index map: it walks the array once per waiting client, origin player counts
are small, and a map is speculative. The scan index is itself the player row
index, so the `PlayersInterpolate::idToIndexMap` lookup
(`ServerClientManager.cpp:146`)
and the `puiIds` reads both drop out, and the post-tick `pClientGuids`
write-back at `:157` is deleted because the row is already owned.

Details that matter:

- Skip an entry whose stored id is not `IsValid()`, in one line, for one reason:
  it was not minted yet this update, so there is nothing to look for. Say that
  in the comment and claim nothing more.
- Remove only the entries whose id was found, in place, instead of erasing a
  leading run. An entry whose spawn the tick refused — `Players` silently
  refuses an out-of-cell spawn offset
  (`Players.cpp:293-300`)
  — must keep waiting exactly as it does today, and with unique keys a later
  entry resolving before an earlier one is no longer a reason to stop.
- Because minted ids are unique, each entry resolves to exactly one row. No
  tie-breaking, no consultation of the ownership record during the scan, and no
  empty-value matching is needed anywhere.
- Keep today's behavior when the id is found but `FindClient` returns null
  (`:143`): the entry resolves and is erased, the assign and player-state sends
  no-op for the vanished client, and the ownership record and fleet notification
  are skipped. Under unique ids this is harmless, since the released id belongs
  to no other entry.
- Recommended shape: do the assignment work inside a `std::erase_if` predicate
  that returns whether the entry resolved, mirroring the existing
  side-effecting `erase_if` at
  `ServerBroadcaster.cpp:34-49`. No new helper.
- Move the pairing log line (`:133`) from `kVerbose` to `kInfo`, matching the
  connect diagnostic in the same file (`:83-109`), and have it name each client
  id with the global id it resolved to and note an entry that stayed queued.
  Recommended because it is the acceptance evidence surface and must be compiled
  in; see `## Notes`. Reword the allocation-suppression comment above it
  (`:118-119`) to describe what still allocates once the two id vectors are gone.

**7. Delete the snapshot mechanism and update the prose it justified.**
Delete `mPreSpawnPlayerIds` and `RefreshPreSpawnSnapshot`
(`ServerClientManager.h:28`
and `:34`,
`ServerClientManager.cpp:264-273`,
the `ResetState` clear at `:280`, and the `PreSpawn` log field at `:133`) and the
broadcaster's capture block
(`ServerBroadcaster.cpp:100-109`).

Proposed replacement for the spawn-assignment sentence at line 25 of the server
`AGENTS.md`:

> Spawn assignment carries the requesting client's GUID in the spawn status
> change, so the server's player row is born owned, and carries the global ID
> minted for that request on the waiting entry; the post-tick pass pairs each
> waiting client with the origin row holding its own minted ID, and an
> unresolved client stays queued for a later tick. That GUID, written into Frame
> state at spawn and excluded from the wire, is the persistent relink key.

Proposed replacement for the clause in the following sentence (`:26`): drop
`plus broadcast and pre-spawn snapshot capture` down to `plus broadcast`.

Two documented contracts elsewhere were checked and need no edit. Deleting the
post-tick write-back does not remove the only main-thread write into
`pClientGuids` — `ServerTransferManager` still writes it through
`AssignRegistryClientGuid`
(`Engine/Source/Network/Server/ServerTransferManager.cpp:277-294`) — so
`Engine/Source/Frame/AGENTS.md` `## Frame Registry` stays true, and the new
spawn-time write is a collection writing its own column inside its own phase,
not a registry write. And `Documents/Architecture/Network.md:87` stays true
sentence by sentence: it states the wire-protocol version, which does not move;
it scopes the joint bump of the protocol, navigation, Frame, and frame-input
gates to one past generation break rather than a standing coupling; and it
already describes the frame-input gate separately as the replay-input gate,
which is exactly the case this change exercises.

**8. Delete all nine guard sites and the prose documenting them.**
With each waiting client resolved by its own minted id and each row owned at
birth, an agent-minted or transfer-arrived player can no longer be handed to a
connecting client, so the guards protect nothing. Both drain deferrals are
pairing-only: their stated reason is the snapshot-diff mis-assignment and
nothing else, and the rest of `DrainReplayTransferFixtures` — the
destination-frame creation and swap and the merge into the transfer manager
(`ServerSimulationFixtures.cpp:985-999`)
— reads no client or waiting state. Delete the sites listed in `## Context`
together with the comment at
`ServerBroadcaster.cpp:82-90` that names the
snapshot-diff zip, and the documentation sentences enumerated in `## In scope`.

## Critical files

Every file is given here with its full repository-relative path; elsewhere in
this Plan the same files are cited by bare filename.

- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp:111-171` — `FinalizeNewClients`: the snapshot diff at `:121-131`, the positional zip at `:136-167`, the log line at `:133`, the allocation comment at `:118-119`, the `FindClient` branch at `:143`, the index-map lookup at `:146`, and the GUID write-back at `:157`.
- `ServerClientManager.cpp:264-273` — `RefreshPreSpawnSnapshot`, deleted; the `ResetState` clear at `:280`; the dedup comment at `:23-24`; the positional brace-init at `:31`; the `kInfo` connect diagnostic to match at `:83-109`.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.h:10-34` — `ClientSpawnInfo` and its new id member, `RefreshPreSpawnSnapshot`, `mClientsWaitingForSpawn`, `mPreSpawnPlayerIds`.
- `Engine/Source/Network/Server/ServerBroadcaster.cpp:52-70` — the mint-and-emit loop that records the id and stamps the GUID; the stale-fleet drop above it at `:34-49`; the drain comment at `:82-90`; the snapshot capture at `:100-109`, deleted.
- `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:57-68` — `SpawnPlayerData`, which gains the GUID member; the transfer payload's unserialized GUID precedent at `:161-163`; `DefaultDataForType` at `:174-184`.
- `Engine/Source/Network/NetworkSerialization.cpp:129-152` — `StatusChangeItemWireSize`, whose `kSpawnPlayer` case stays unchanged along with the write case at `:175` and the read case at `:337`.
- `Projects/BrokenEngineSandbox/Source/Frame/FrameInput.h:11` — `kiVersion`, bumped; the raw payload record write at `Projects/BrokenEngineSandbox/Source/Frame/FrameInput.cpp:20-39`.
- `Engine/Source/File/DifferenceStream.h:259-277` — the reader gate that compares that version.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.cpp:266-314` — the spawn status-change loop, including the out-of-cell refusal at `:293-300`; `:481` stores the GUID where the row now clears it.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.h:266-350` — `SharedMembers`, the CRC-only subset comment at `:305` excluding `pClientGuids` and `pGlobalPlayerIds`, and `SpawnInfo`.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetManager.cpp:288` — `OnPlayerSpawned`, the consumer that must keep receiving the matching `ClientSpawnInfo` and global id.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:243-268` — `AfterNetworkPoll` request ordering with the comment at `:248-250`, and `FinalizeTickClients`, the sole caller, whose replay guard at `:259` must stay.
- `Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md:25-26` — the documented spawn-assignment-by-diff contract and the pre-spawn-snapshot clause.
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:54-999` — the nine guard sites, the two agent spawn producers at `:455-462` and `:715-720`, and the fixture drain body at `:985-999`.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:7-52` — the command rows and closing paragraph stating the waiting-for-spawn preconditions.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerQueries.cpp:53` — `query_players`, which reports a global id and no owner.

## In scope

- `ClientSpawnInfo` gains one `engine::global_id_t` member, declared last, with
  the type's own zero default.
- `SpawnPlayerData` gains one `engine::ClientGuid` member, declared last, not
  serialized over the network.
- The `BuildFrameInputs` waiting-entry loop stores each minted id on its entry
  and stamps that entry's GUID into the payload, iterating by mutable reference;
  no change to the fleet lookup, the stale-fleet drop, or the ordering around it.
- The `Players` spawn status-change loop extracts the payload GUID, carries it
  through `PlayersPostRender::SpawnInfo`, and stores it into `pClientGuids` where
  the row currently clears that column.
- `FinalizeNewClients` resolves each waiting entry by its stored id against the
  origin cell's `pGlobalPlayerIds`, keeps the existing assign, player-state,
  ownership-record, and fleet-notification work, drops the post-tick GUID
  write-back and the index-map lookup, removes only resolved entries, and skips
  an entry whose id is still unassigned.
- The `FinalizeNewClients` log line: moved to `kInfo`, reporting each client id
  with the global id it resolved to, with the `PreSpawn` field removed; and the
  allocation-suppression comment above it reworded.
- Incrementing `FrameInput::kiVersion`.
- Deleting `mPreSpawnPlayerIds`, `RefreshPreSpawnSnapshot`, their `ResetState`
  and log-line uses, and the broadcaster snapshot capture block.
- The server `AGENTS.md`
  `## Deterministic Tick Contracts` spawn-assignment and pre-spawn-snapshot
  sentences.
- All nine `ClientsWaitingForSpawn` sites in `ServerSimulationFixtures.cpp`: the
  forward declaration, the four command refusals, the helper and its comment,
  and the two drain deferrals with the "Held, not dropped" comment above the
  second.
- The comment sentences that assert the deleted mechanism: the drain rationale
  in `ServerBroadcaster.cpp:82-90`, the `AfterNetworkPoll` ordering rationale in
  `ServerSession.cpp:248-250` (its wording only — request order still fixes the
  order spawn changes enter the frame input, and therefore the simulation and the
  CRC, not which client gets which player), and the `QueueSpawnForClient` dedup
  comment in `ServerClientManager.cpp:23-24`.
- The harness documentation sentences stating the waiting-for-spawn
  preconditions: `commands-server.md` rows `:7`, `:40`, `:48`, `:50` and its
  closing injection paragraph `:52`;
  `Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md:24` and `:100`;
  `Projects/BrokenEngineSandbox/Documents/AgentHarness/cross-cell.md:9`; and the
  drain sentence in `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md:14`.

## Out of scope

- Which clients enter `mClientsWaitingForSpawn`, in what order, or when:
  `QueueSpawnForClient`, `NewClients`, `Disconnects`, the fleet spawn/respawn
  request processing, and the `AfterNetworkPoll` ordering behavior all stay as
  they are; only that ordering comment's wording changes.
- The `kSpawnPlayer` wire layout and the status-change codec: the wire-size,
  write, and read cases and the per-item maximum are verified unchanged, not
  edited. `engine::kuiProtocolVersion` does not move, so no client is refused.
- `Frame::kiVersion`, the collection versions, the CRC subset, and the grid save
  format; existing saves must keep loading.
- `Players` spawn behavior apart from the GUID store: the out-of-cell spawn
  refusal, flagship flags, and fleet wanted-coord carry are untouched.
- Transfer arrival ownership, which must stay bit-identical, and
  `AssignRegistryClientGuid` and the Frame Registry contract around it.
- `ServerFleetManager::OnPlayerSpawned` internals and every other fleet path.
- The stale-fleet drop `std::erase_if` above the mint loop
  (`ServerBroadcaster.cpp:34-49`) and everything
  else in `BuildFrameInputs`.
- Transfer validation, publication assembly, replay capture, save/load, and the
  engine-owned status-change ordering.
- Adding a global-id-to-index map, an id lookup helper, any new Collection
  member, or any new harness query.
- `Documents/Architecture/Network.md`, which stays true as written, and
  `Documents/Investigations/AgentFrameInjection.md`, a dated findings record
  that is deliberately left unchanged.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: the change spans independently owned
subsystems — the engine broadcaster mints the id and stamps the payload, the
game client manager consumes both — it rewrites a contract documented in the
server `AGENTS.md`, and it changes a
serialization surface: the recorded frame-input payload layout and its version
gate. Tier 3 under `.agents/references/risk-tiers.md` on each count
independently.

Surfaces, stated so the tier reviewer confirms rather than assumes:

- Network wire and protocol: unchanged. The new payload member is unserialized,
  the codec is untouched, and `engine::kuiProtocolVersion` does not move.
- Replay: changes. The recorded payload grows, `FrameInput::kiVersion` bumps,
  and every existing recording is refused. Accepted; no compatibility path.
- Save: unchanged. No collection member or collection version moves and
  `Frame::kiVersion` does not bump.
- CRC: unchanged. `pClientGuids` stays outside `SharedCrcMembers()`, so writing
  it earlier in the tick changes no checksummed byte. The same-session
  frame-input checksum does hash the new member, but it is writer-side replay
  dedup and a verbose reader log, never persisted or compared across builds.

Preserve these invariants, each with the check that settles it:

- A connecting client is assigned the player spawned for its own request and no
  other, for every producer of an origin-cell player, present and future,
  without that producer carrying a guard. Check: the same-tick injection run
  below.
- A row born from a producer that supplies no GUID is unowned, and each waiting
  entry resolves to exactly one distinct row because minted ids are unique.
  Check: a code read of the mint site and the scan, plus that same run.
- A waiting entry whose spawn did not materialize this tick stays queued and is
  assigned on a later tick; none is dropped, double-assigned, or reordered.
  Check: a code read of the erase predicate showing only found ids are erased.
  No harness path can force a refusal — the broadcaster's own spawns use the
  in-cell default offset — so the code read is the evidence, not a scenario.
- `SendAssignPlayer`, `SendPlayerState`, the `mClientPlayers` ownership record,
  and `OnPlayerSpawned` still all run together for a still-connected assigned
  client, with the same global id and the same `ClientSpawnInfo`; the
  vanished-client partial branch is unchanged. Check: the connect run below.
- Transfer arrival ownership assignment is bit-identical. Check: the transfer
  fixture CRC run below.
- Replay playback still skips `FinalizeNewClients` entirely, and the simulated
  Frame and its CRC are unchanged by the rewrite. Check: a code read of the
  unchanged guard at
  `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:259`,
  plus the determinism run below.
- Disconnect still removes a waiting entry, and `ResetState` still leaves no
  waiting or pairing state behind. Check: a code read showing the diff leaves
  both paths unchanged apart from the deleted snapshot clear.

## Acceptance criteria

- A word-boundary search for `\bClientsWaitingForSpawn\b` returns zero hits —
  the retained member `mClientsWaitingForSpawn` must not match, which is the
  point of the boundary — and searches for `mPreSpawnPlayerIds` and
  `RefreshPreSpawnSnapshot` each return zero hits.
- With a client waiting for spawn, an agent `spawn_players` injection into the
  origin cell on the same tick leaves the client assigned its own minted global
  id and the injected player unowned. Verified through `/agent-harness` against
  the `kInfo` `FinalizeNewClients` pairing line and `query_players`, with the
  injected id appearing in no pairing or assign line.
- A normal client connect still receives `AssignPlayer` and a `kSpawned`
  `PlayerState`, and a reconnect's connect diagnostic reports the GUID match for
  that player, proving the GUID reached the row at spawn. A fleet-triggered
  spawn still lands in its fleet's member list.
- `FrameInput::kiVersion` is incremented in the diff and the difference-stream
  reader compares that constant, so pre-change recordings are refused rather
  than misread. Settled from the diff and that code read; capture no baseline
  artifact.
- A replay recorded after the change plays back with no CRC mismatch.
- A transfer fixture run reports the same CRCs as before the change.
- Server and client both build.

## Notes

- No harness query reports a player's owner: `query_players` returns a global id
  and no client GUID, and the client scene query likewise. The `kInfo`
  `FinalizeNewClients` pairing line is therefore the evidence surface for every
  ownership claim in the acceptance criteria, which is why its level and its
  fields are in scope rather than incidental.
- Both halves of the design are needed and neither substitutes for the other. The
  GUID alone would leave the pairing scanning for a key a client can hold on
  several rows at once; the id alone would leave the row unowned until after the
  tick, keeping the post-tick write-back this change removes.
