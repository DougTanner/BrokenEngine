<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:54:18.953Z","dependsOn":[]} -->
# Sort tick status changes by type before the server applies them

## Context

Verified against commit `4cf9d607`. `BES` is
`Projects/BrokenEngineSandbox/Source`, `SSB` is
`Engine/Source/Network/Server/ServerSessionBase.cpp`, and `REPLAY` is
`Engine/Source/File/Replay.cpp`.

Two documents say the server orders a tick's status changes by type:

- The header comment of `Engine/Source/Network/Server/ServerBroadcaster.h`
  (lines 5-6) says the class owns "the deterministic type grouping the wire
  format depends on".
- `Engine/Source/Network/AGENTS.md` `## Ownership`, the `NetworkSerialization`
  bullet, says "published server tick inputs use that sequence" — ascending
  `StatusChangeType` — "because clients apply the wire order".

The code does not do that for live tick input:

- `ServerBroadcaster::PrepareTickStatusChanges`
  (`Engine/Source/Network/Server/ServerBroadcaster.cpp:36-54`) copies each
  coordinate's `FrameInput::statusChanges` in push order. Nothing groups live
  tick input by type.
- The server applies and records that push order: the tick dispatch reads
  `mFrameInputs` (`Engine/Source/GameBase.cpp:550`), the game applies it in a
  range-for (`BES/Frame/Collections/Ships/ShipStatusChanges.cpp:37`), and the
  replay writer records the same list (`REPLAY:770`).
- Clients apply codec order. `SerializeStatusChangeBatch`
  (`Engine/Source/Network/NetworkSerialization.cpp`, `GroupIndicesByType`)
  emits type groups in ascending `StatusChangeType` order, keeping input order
  inside a type, and the client and its replay reconcile apply what they
  decode (`Engine/Source/Network/Client/Client.cpp:600`,
  `Engine/Source/Network/Client/ReconcileReplayTick.cpp:233`, `:278`).

A reorder is reachable today: `StatusChangeType::kUpdateSquadronOrders` is
pushed once per advancing update, after the network polls, when
`ServerBroadcaster::BuildFrameInputs` (`Engine/Source/GameBase.cpp:1160`,
`ServerBroadcaster.cpp:32`) calls the game
(`BES/Network/Server/ServerSession.cpp:178`,
`BES/Network/Server/ServerFleetManager.cpp:221`). That is before
`StatusChangeType::kUpdateFleet`, which `TickGameTimers` pushes later
(`SSB:49`, `BES/Network/Server/FleetNavigationController.cpp:202`) and which
sorts first on the wire.

There is no desync today: every current pair of tick-input handlers commutes
(`ShipStatusChanges.cpp`), `kDestroyShip` is first in both orders, and spawn
and transfer types never enter tick input (`REPLAY:863`, the
`StatusChangeType` comments in `BES/Frame/StatusChange.h`). The exposure is
latent, on the determinism/CRC surface: a future pair of handlers that do not
commute would desync server and clients, and no rule warns the author.

Only the agent harness rig sorts today:
`DrainPendingAgentStatusChanges`
(`BES/Agent/Commands/ServerSimulationHarnessRigs.cpp:643-650`) runs a stable
insertion sort by `eType` over a coordinate's whole list, and only on a tick
that releases an injected change for that coordinate.

Origin: an architecture review (simulation and cohesion lenses) of
`Engine/Source/Network/Server`, independently verified. Unmet criterion:
documents and code agree on what orders tick status changes, and the agreed
rule makes server apply order and client apply order equal or provably
equivalent.

## Design

The documents and the code contradict each other, and the repository
authority order ranks documents above current code behavior. The user was
shown two corrections — this server sort, or restating the two documents with
no code change — and chose the server sort. The rest of this section is the
author's recommendation for carrying it out.

In `ServerBroadcaster::PrepareTickStatusChanges`, after
`game::DrainPendingAgentStatusChanges` and before the snapshot loop, sort each
coordinate's `FrameInput::statusChanges` in place into ascending `eType`
order. The sort is stable: changes of one type keep their push order, the same
rule the codec applies inside each type group. Sorting the `FrameInput` list
itself, not the snapshot copy, is what makes every reader agree, because each
one reads that list after `ServerSessionBase::PrepareTick` returns:

- the broadcast snapshot, `mBroadcastStatusChanges`
  (`ServerBroadcaster.cpp:51`);
- replay capture, `DifferenceStreamWriter::Update` (`REPLAY:770`), called from
  `Replay::SyncReplayTick`, which `GameBase` runs after `PrepareTick`
  (`Engine/Source/GameBase.cpp:447-459`);
- the tick dispatch, through `ActiveFrameReference::pFrameInput`
  (`GameBase.cpp:550`).

The codec then finds its input already in group order, so the wire order and
the client apply order do not change; only the server's apply order and the
recorded order do. The executing session confirms no push into
`FrameInput::statusChanges` runs between the sort and the dispatch; none was
found at the verified commit. The sort runs every tick on the main loop, so it
follows the Memory allocation rules; the harness rig's in-place insertion sort
is a non-allocating form already in the tree.

The harness rig's own sort then duplicates the engine's and is removed: the
drain runs inside `PrepareTickStatusChanges` before the new sort, so injected
changes are still ordered.

The two documents quoted in `## Context` become true as written and are not
reworded.

Replay playback does not pass through `PrepareTickStatusChanges`:
`PrepareTick` returns early while `mbReplaying` (`SSB:31`), and playback
dispatches and broadcasts each recorded list as read (`REPLAY:855`, `:891`). A
recording made after this change holds sorted lists, so playback needs no sort
of its own. A recording made before it holds push order, which the server
would still apply on playback while clients apply type order, so the unsorted
server order would stay reachable and the first acceptance criterion would not
hold. This Plan therefore bumps the version that owns the recorded tick input,
so older recordings are rejected; the repository keeps no backward
compatibility, so no playback-side sort is added for them. That version is
`game::FrameInput::kiVersion`
(`Projects/BrokenEngineSandbox/Source/Frame/FrameInput.h:10`), which
`DifferenceStreamReader` checks for the recorded difference type
(`Engine/Source/File/DifferenceStream.h:239-248`). Before implementing, the
executing session confirms that this constant is the owning version of the
replay input stream and checks what else it gates, for example saves, and
states both in its hand-off.

## Critical files

- `Engine/Source/Network/Server/ServerBroadcaster.cpp`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/FrameInput.h`
- `Engine/Source/Network/Server/AGENTS.md`

## In scope

- `ServerBroadcaster::PrepareTickStatusChanges`: the stable sort by `eType`
  over each coordinate's `FrameInput::statusChanges`, between the agent drain
  and the snapshot loop.
- `DrainPendingAgentStatusChanges` in `ServerSimulationHarnessRigs.cpp`: the
  insertion sort over `rStatusChanges` (lines 643-650), removed.
- `game::FrameInput::kiVersion` in `FrameInput.h`, the version that owns the
  recorded tick input: bumped, so a recording made before the change is
  rejected.
- `Engine/Source/Network/Server/AGENTS.md` `## Transfers and Publication`: the
  sentence listing what `ServerBroadcaster` owns, which gains the type sort of
  the tick's status changes.

## Out of scope

- The codec's group order and `SerializeStatusChangeBatch`.
- `ServerTransferManager::SortTransfersByType` and the post-tick batch, which
  is already type-sorted.
- Any status-change handler, and the push sites named in `## Context`.
- Replay playback (`REPLAY:799-894`): no sort, check, or conversion is added
  there.
- The `ServerBroadcaster.h` header comment and the `NetworkSerialization`
  bullet of `Engine/Source/Network/AGENTS.md`.
- `Frame::kiVersion` and `engine::kiProtocolVersion`: the wire order and the
  payload layouts do not change. A finding that either must change is
  surfaced for re-planning, not made here.

## Risk tier

Tier 3 — invariant/integration (the author's classification). Trigger: it
changes the order in which the server applies CRC'd tick input and the order a
replay records — the determinism/CRC and replay surfaces — and bumps a replay
compatibility version.

## Acceptance criteria

- When the server tick, the replay writer, and the broadcast snapshot read a
  coordinate's `FrameInput::statusChanges`, it is in ascending `eType` order
  with push order kept inside each type.
- The harness rig sort is gone, and apart from the new sort no other server
  code reorders `FrameInput::statusChanges`.
- `game::FrameInput::kiVersion` is bumped, so a recording made before the
  change is rejected on playback, and the hand-off states that it owns the
  replay input stream and what else it gates.
- The client and the server build.
- Through `/agent-harness`, the replay determinism check passes on a recording
  made after the change: playback completes a loop with no checksum mismatch.
- Through `/agent-harness`, a live client/server run with fleets active — so
  ticks carry more than one status-change type for a coordinate — shows no CRC
  mismatch or desync line on either side.
- The supplied evidence records no harness setup for either run.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
