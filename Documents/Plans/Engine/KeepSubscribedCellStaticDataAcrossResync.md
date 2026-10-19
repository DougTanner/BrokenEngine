<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-19T18:55:28.871Z","dependsOn":[]} -->
# Keep a subscribed cell's static data across a client resynchronization

## Context

After a client resynchronization, a neighbor cell that leaves the visible set
before its resync full state arrives loses its received static data (islands
and per-plane navigation graphs) while its subscription stays active. The
server's resync answer carries a full state only, so the client then
simulates that cell from a default `CellStaticData`: no islands and no
navigation graphs. Its simulation diverges from the server (a desync) until
the subscription ends and a later subscription resends the static data.

Verified path, from code in the tree at `0f3fc10c3`:

- A resync starts from `ClientSessionRuntime::ApplyReceivedUpdates` on a full
  update buffer (`Engine/Source/Network/Client/ClientSessionRuntime.cpp:387`)
  or from `ClientDesyncCore::RecoverFromDesync`
  (`Engine/Source/Network/Client/ClientDesyncCore.cpp:131`). Both call
  `ClientSessionBase::ResetCoordinateStatesForResynchronization`
  (`Engine/Source/Network/Client/ClientSessionBase.cpp:71`). It runs
  `Cell::ResetClientState` on every cell, which sets `iConfirmedTick` to -1
  and `iSnapshotCount` to 0 but keeps `staticData`
  (`Engine/Source/GameBase.h:136-153`). It also clears `mUnwantedTimestamps`
  (`ClientSessionBase.cpp:79`), the sticky-subscription window.
- `ClientSessionRuntime::ComputeActiveSet` erases every cell outside
  `mActiveCoordinates` whose `iConfirmedTick` is negative
  (`ClientSessionRuntime.cpp:742-747`). Its comment states the intent: "Delete
  local-only cells outside the active set, preserve network-subscribed cells".
  The predicate uses `iConfirmedTick < 0` to mean "local-only", which a resync
  makes true for every subscribed cell until its full state arrives.
- After a resync the active set holds only the client cell and the currently
  visible neighbors (`ClientSessionRuntime.cpp:655-700`). A neighbor that stops
  being visible between the resync and its full state's arrival is therefore
  erased with its static data. On the next frame `SetDesiredCoordinates` moves
  that coordinate into `mUnwantedTimestamps` (`ClientSessionRuntime.cpp:489-494`),
  so `SynchronizeSubscriptions` keeps its slot subscribed for
  `kStickySubscriptionDuration` (2 s, `ClientSessionRuntime.h:130`).
- The server answers the resync with `SendCoordinateFullState` for each active
  slot (`Engine/Source/Network/Server/ServerSessionRuntime.cpp:190`). It sends
  static data only when serving a new subscription
  (`ServerSessionRuntime.cpp:229`).
- The slot is still `kActive` with a matching coordinate and epoch, so
  `ClientSubscriptions::ClassifyFullState` commits the full state
  (`Engine/Source/Network/Client/ClientSubscriptions.cpp:163-170`).
  `ClientSessionRuntime::ApplyReceivedFullStates` then creates a default `Cell`
  with `try_emplace` (`ClientSessionRuntime.cpp:281`) and confirms it, and the
  reconcile dispatcher selects every confirmed cell for simulation
  (`Engine/Source/Network/Client/ReconcileReplay.cpp:417-421`).
- If the coordinate becomes visible again inside the sticky window,
  `BuildSubscriptionQueue` skips it because its slot is already active
  (`ClientSessionRuntime.cpp:601-615`). The visible cell keeps its empty static
  data until it leaves view and its sticky window expires.

Two narrower variants are safe today and stay as they are. A cell that is
already in the sticky window when the resync starts drops out of the desired
set, because the reset clears `mUnwantedTimestamps`. `SynchronizeSubscriptions`
(`Engine/Source/GameBase.cpp:294`) then unsubscribes it and erases the cell
before any resync answer can arrive. Its late full state is rejected: it arrives
on a `kUnsubscribing` slot, and it carries the retained epoch
(`ClientSubscriptions.cpp:109-121`, `:158`).

The defect predates the session that found it. That session only made the
game-side `NavigationPlaneData` lookup
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp`)
return an empty graph instead of throwing when a cell lacks a plane's graph.
The empty static data itself is still reachable.

## Design

The author recommends that the client keep the static data. Change the
`ComputeActiveSet` erase predicate so that it never erases a cell whose
coordinate holds a client subscription slot that `IsSlotActive` accepts
(`ClientSessionRuntime.cpp:61`, every state except `kUnsubscribed` and
`kUnsubscribing`). Read the slots from `mpClient->mSubscriptions.mCoordinateSlots`.
When `mpClient` is null, no slot exists and the predicate behaves as it does
today. The resync full state then lands in the existing `Cell` through the
`iConfirmedTick < 0` branch of `ApplyReceivedFullStates`, with its
`staticData` intact.

Rationale:

- It makes the predicate match the intent its own comment states, which by the
  repository authority order outranks the current behavior.
- Static data is fixed for the life of a subscription, and a server load resets
  every slot and cell anyway, so the static data the client already holds is
  still correct after a resync. `Engine/Source/Network/Client/AGENTS.md`
  `## Subscription Receive Invariants` describes resync as server full-state
  replacement, so a full-state-only answer stays correct.
- Cell lifetime stays owned by the subscription. `UnsubscribeStaleCoordinates`
  still erases the cell when its slot unsubscribes
  (`ClientSessionRuntime.cpp:583-587`), so a preserved cell lives no longer
  than its slot.

Alternative the claiming session may weigh: the server could also send static
data with every resync full state. The author does not recommend it. It
changes what the resync answer carries on the wire, resends navigation data for
every slot on each resync, and has to reopen the static-data ordering guard
that `HandleResyncRequests` keeps for queued new subscriptions
(`ServerSessionRuntime.cpp:173-181`). All of that fixes a client-only erase.

The `ASSERT` after the erase (`ClientSessionRuntime.cpp:749-752`) bounds the
number of unconfirmed cells by 9. Preserved subscribed cells can exceed that,
up to `kiCoordinateSlots` (16, `Engine/Source/Network/NetworkProtocol.h:86`).
The author recommends counting only unconfirmed cells that hold no active slot.
Those are the local-only cells, which lie inside the active set, so the bound
of 9 still holds for them.

Exposure:

- Determinism/CRC: this changes the static data the client simulation reads
  for a confirmed cell after a resync, restoring agreement with the server. It
  changes no simulation math, CRC input, `Frame::kiVersion`, save, or replay
  format.
- No wire, protocol, `.pack`, server, threading, or shader change.
  `ComputeActiveSet` runs on the client main thread.
- Callers that iterate `mCells` already tolerate an unconfirmed cell with no
  snapshot (`ReconcileReplay.cpp:405-421`, `GetConfirmedTick`,
  `DescribeScene.cpp:81`, `HudScreen.cpp:54`, `AgentScene.cpp:205`).

Change Workflow tier: Tier 3. Trigger: a determinism/CRC change. It changes the
inputs of the client's CRC-checked simulation for a confirmed cell.

## Critical files

- `Engine/Source/Network/Client/ClientSessionRuntime.cpp` —
  `ClientSessionRuntime::ComputeActiveSet`

## In scope

- `ClientSessionRuntime::ComputeActiveSet`: the `std::erase_if` predicate over
  `game::gpGame->mCells` keeps a cell whose coordinate holds an
  `IsSlotActive` slot. Its comment states that rule.
- The `ASSERT` immediately after that erase: count only unconfirmed cells that
  hold no `IsSlotActive` slot, and keep the bound of 9.

## Out of scope

- `ClientSessionBase::ResetCoordinateStatesForResynchronization`,
  `Cell::ResetClientState`, and the `mUnwantedTimestamps` clear.
- `UnsubscribeStaleCoordinates`, `SetDesiredCoordinates`, `SynchronizeSubscriptions`,
  and the sticky-window duration.
- `ClientSubscriptions::ClassifyFullState` and every other receive
  classification.
- Every server file, including `ServerSessionRuntime::HandleResyncRequests` and
  `SendNewSubscriptionFullStates`. Nothing on the wire changes.
- The active-set membership rules and `CreateCellAtCoordinate`.
- The game-side `NavigationPlaneData` empty-graph guard in `ShipBehaviours.cpp`.

## Acceptance criteria

| Criterion | Verification |
|---|---|
| After a resync, a cell whose slot is still active is never erased by `ComputeActiveSet`, and a cell with no active slot outside the active set still is | The diff, read against the verified path in Context |
| A connected client panning so neighbors enter and leave view keeps islands for every subscribed coordinate. A neighbor that left view leaves `subscribedCoords` after its sticky window expires. No `LogDifferences CRC Client` or `CONFIRMED DESYNC` line appears | `/agent-harness` client and server launch, camera pan, `describe_scene`, `get_logs` |

## Notes

- No harness command forces `ResetCoordinateStatesForResynchronization` on its
  own. `edit_frame` resynchronizes through a load-generation reset, which
  frees every subscription and so does not reach this path. The diff therefore
  settles the fix, and the live run checks that it caused no regression.
