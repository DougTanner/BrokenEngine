<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:54:18.953Z","dependsOn":[]} -->
# Sort tick status changes by type and simplify the tick publication walk

## Context

Verified against commit `4cf9d607`. `BES` is
`Projects/BrokenEngineSandbox/Source`, `SB` is
`Engine/Source/Network/Server/ServerBroadcaster.cpp`, `SSB` is
`Engine/Source/Network/Server/ServerSessionBase.cpp`, and `REPLAY` is
`Engine/Source/File/Replay.cpp`.

### Tick status change order

Two documents say the server orders a tick's status changes by type:

- The header comment of `Engine/Source/Network/Server/ServerBroadcaster.h`
  (lines 5-6) says the class owns "the deterministic type grouping the wire
  format depends on".
- `Engine/Source/Network/AGENTS.md` `## Ownership`, the `NetworkSerialization`
  bullet, says "published server tick inputs use that sequence" — ascending
  `StatusChangeType` — "because clients apply the wire order".

The code does not do that for live tick input:

- `ServerBroadcaster::PrepareTickStatusChanges` (`SB:36-54`) copies each
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
`SB:32`) calls the game (`BES/Network/Server/ServerSession.cpp:178`,
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

Only the agent harness rig sorts today: `DrainPendingAgentStatusChanges`
(`BES/Agent/Commands/ServerSimulationHarnessRigs.cpp:643-650`) runs a stable
insertion sort by `eType` over a coordinate's whole list, and only on a tick
that releases an injected change for that coordinate.

### Always-true publication filter

`ServerBroadcaster::BuildTickPublication` walks the publication coordinates
through the lambda `ForEachPublicationCoord` (`SB:63-95`). Its replay branch
first walks `rActiveCoordinates` and calls back only for an element not found
earlier in the same list (`SB:74-80`). `rActiveCoordinates` is
`game::gpGame->mActiveCoordinates` (`SB:59`), and every writer keeps that list
free of duplicates. Each push is guarded by a `contains` test:
`ServerSessionRuntime::AddSubscribedCoords` (`:338-340`) and
`ServerSessionRuntime::ComputeActiveSet` (`:376-382`) in
`Engine/Source/Network/Server/ServerSessionRuntime.cpp`, and
`ServerSessionBase::AddOwnedEntityCoordinates`. Or the list is rebuilt from
the unique keys of the replay reader map: `REPLAY:556-560` and
`GameBase::RefreshReplayActiveSet` (`Engine/Source/GameBase.cpp:1034-1038`).
So the `std::find` test at `SB:76` is always true, and the loop does what the
non-replay loop at `SB:67-70` does, with a quadratic scan.

Also, `ServerSessionRuntime::CompleteTick` clears
`ServerBroadcaster::mBroadcastStatusChanges`
(`ServerSessionRuntime.cpp:284`) right after `BuildTickPublication` returns,
while `BuildTickPublication` itself retires the transfer batch at its end
(`SB:187`). Nothing reads the map between the two points. The clear is
load-bearing in a multi-tick update and must remain.

Origin: an architecture review (simulation and cohesion lenses) and an
in-function refactor review of `Engine/Source/Network/Server`, each
independently verified. Unmet criteria: documents and code agree on what
orders tick status changes, and the agreed rule makes server apply order and
client apply order equal or provably equivalent; the publication walk holds no
branch whose test cannot fail.

## Design

### Sort tick status changes by type

The documents and the code contradict each other, and the repository
authority order ranks documents above current code behavior. The user was
shown two corrections — this server sort, or restating the two documents with
no code change — and chose the server sort. The rest of this subsection is the
author's recommendation for carrying it out.

In `ServerBroadcaster::PrepareTickStatusChanges`, after
`game::DrainPendingAgentStatusChanges` and before the snapshot loop, sort each
coordinate's `FrameInput::statusChanges` in place into ascending `eType`
order. The sort is stable: changes of one type keep their push order, the same
rule the codec applies inside each type group. Sorting the `FrameInput` list
itself, not the snapshot copy, is what makes every reader agree, because each
one reads that list after `ServerSessionBase::PrepareTick` returns:

- the broadcast snapshot, `mBroadcastStatusChanges` (`SB:51`);
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

The two documents quoted in `### Tick status change order` become true as
written and are not reworded.

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

### Remove the always-true publication filter

Recommended: in `ForEachPublicationCoord`, run the plain walk of
`mActiveCoordinates` always, return after it when not replaying, and keep the
two replay-only loops over `mBroadcastStatusChanges` and `rTransfers`
(`SB:81-94`) after it. Publication order is unchanged in both modes.

In the same change, move the `mBroadcastStatusChanges.clear()` from
`CompleteTick` to the end of `BuildTickPublication`, beside the transfer
batch clear, so the broadcaster retires both of its tick inputs in one place.

## Critical files

- `Engine/Source/Network/Server/ServerBroadcaster.cpp`
- `Engine/Source/Network/Server/ServerSessionRuntime.cpp`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/FrameInput.h`
- `Engine/Source/Network/Server/AGENTS.md`

## In scope

Tick status change sort:

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

Publication walk:

- The `ForEachPublicationCoord` lambda in
  `ServerBroadcaster::BuildTickPublication`, and the `rActiveCoordinates`
  alias if it is then unused.
- The end of `BuildTickPublication`: one added clear of
  `mBroadcastStatusChanges`.
- `ServerSessionRuntime::CompleteTick`: the removed clear.

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
- The two replay-only loops in `ForEachPublicationCoord` and their `contains`
  tests.
- The publication block layout, the workbuffer preflight, and `PublishTick`.
- Every other clear of `mBroadcastStatusChanges`: `BuildFrameInputs`,
  `ResetState`, and the replay owner's.
- The writers of `mActiveCoordinates`.

## Risk tier

Tier 3 — invariant/integration (the author's classification). Trigger: it
changes the order in which the server applies CRC'd tick input and the order a
replay records — the determinism/CRC and replay surfaces — and bumps a replay
compatibility version. The publication walk restructure also touches the code
that fixes the order of a tick's published coordinates, which the buffered
updates and the replay publication follow, with that order meant to be
unchanged.

## Acceptance criteria

- When the server tick, the replay writer, and the broadcast snapshot read a
  coordinate's `FrameInput::statusChanges`, it is in ascending `eType` order
  with push order kept inside each type.
- The harness rig sort is gone, and apart from the new sort no other server
  code reorders `FrameInput::statusChanges`.
- `game::FrameInput::kiVersion` is bumped, so a recording made before the
  change is rejected on playback, and the hand-off states that it owns the
  replay input stream and what else it gates.
- `ForEachPublicationCoord` has no duplicate test over the active list, and
  visits the same coordinates in the same order as before in both modes.
- `mBroadcastStatusChanges` is cleared once per completed tick, inside
  `BuildTickPublication`, after `PublishTick`.
- The client and the server build.
- Through `/agent-harness`, one combined scenario:
  - a live client/server run with fleets active — so ticks carry more than one
    status-change type for a coordinate — and a multi-tick update shows no CRC
    mismatch or desync line on either side;
  - the replay determinism check on a recording made after the change, played
    back with a subscribed client, completes a loop with no checksum mismatch.
- The supplied evidence records no harness setup for any run.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
