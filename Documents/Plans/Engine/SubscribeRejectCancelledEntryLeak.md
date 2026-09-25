<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-25T21:35:19.650Z","dependsOn":[]} -->
# Consume the cancelled-subscription entry when the server rejects a subscribe

## Context

`Client::CancelSubscription` (`Engine/Source/Network/Client/Client.cpp`) drops
a pending subscribe locally and appends its coord to `mCancelledSubscriptions`;
no message goes to the server, so the server still answers that subscribe. The
list is a multiset: each cancelled generation adds one entry, and each server
response for that coord is expected to consume one through
`Client::RemoveCancelledSubscription` (`ClientReceive.cpp`). Every accept path
in `Client::ServerSubscribeAccept` that acts on the coord consumes an entry
(ghost reject, epoch heal, commit init), and so do the `ServerCoordFullState`
ghost and late-cancellation paths.

The reject-sentinel branch at the top of `Client::ServerSubscribeAccept`
(`uiSlotIndex == kuiSubscribeRejectSlot`, sent by `Server::ClientSubscribe` in
`Engine/Source/Network/Server/ServerReceive.cpp` for "not adjacent" and "no free
slot") calls only `RemovePendingSubscription(coord)` and returns. When the
rejected subscribe had been cancelled, its entry is never consumed. The stale
entry stays until `Client::ResetAllSlots`; the next live accept for that coord
takes the commit-init path, finds the entry, sends `SendUnsubscribe` as if the
new subscribe were cancelled, and `ClientSessionRuntime::BuildSubscriptionQueue`
re-requests the coord once. Reachable when the desired set drops a pending
non-adjacent cell before its reject arrives, for example leaving a rejected
cell in the `cross-cell.md` recipe.

Originating residual: plan-audit finding PA-F-005 raised while preparing and
executing `Documents/Plans/Engine/SubscriptionPlaceholderIndexMismatch.md`,
confirmed by an alternatives researcher and plan audit, and judged outside
that Plan's scope. The branch is unchanged from baseline
`3f98e324deb449e8318adbf4cc4c504a29612b67`, so the leak is pre-existing.

## Design

The author recommends calling `RemoveCancelledSubscription(coord)` in the
reject-sentinel branch alongside `RemovePendingSubscription(coord)`, with a
`kVerbose` log when an entry was consumed. Rationale: a server reject is the
server's single answer to one subscribe generation, exactly like an accept, so
it consumes one entry just as the accept paths do; the list stays one entry per
unanswered cancelled generation.

Before editing, the implementer verifies by trace that removing one entry on a
reject cannot consume an entry that belongs to a different generation:

- Two cancelled generations of the same coord hold two entries; a reject for
  the older one consumes one and leaves one for the newer response.
- `SendSubscribeAccept` sends every accept and reject on
  `NetworkManager::kuiChannelReliable`, so responses for a coord arrive in
  subscribe order; an older cancelled generation's response is processed before
  a newer generation's reject.
- A reject for a generation that was never cancelled finds no entry unless an
  older cancelled generation's response left its entry unconsumed. Check the
  paths that return without consuming — the empty `ClassifySubscribeAccept`
  result, the load-generation mismatch drop, and a full state that commits
  before its accept — and state in the change whether any of them can leave an
  entry that this reject would then wrongly consume. If one can, return that as
  a finding rather than widening this change.

## Critical files

- `Engine/Source/Network/Client/ClientReceive.cpp` — `Client::ServerSubscribeAccept` reject-sentinel branch.

## In scope

- The `uiSlotIndex == kuiSubscribeRejectSlot` branch of
  `Client::ServerSubscribeAccept`: add the `RemoveCancelledSubscription(coord)`
  call and its log.
- `Engine/Source/Network/Client/AGENTS.md`, only if it describes the reject
  path's cleanup.

## Out of scope

- The reject branch's `RemovePendingSubscription(coord)`, which can also remove
  a newer pending generation's placeholder for the same coord.
- `Client::CancelSubscription`, the other `ServerSubscribeAccept` and
  `ServerCoordFullState` paths, and the `mCancelledSubscriptions` container
  type.
- Server code and the wire format: no message, field, or channel changes.

## Risk

Change Workflow Tier 2, trigger: scoped client runtime behavior in one
subsystem (network client subscription bookkeeping). Not a determinism/CRC,
wire, serialization, replay, or threading change: the fix alters only
client-local bookkeeping on the network thread that already owns it. Bounded.

## Acceptance criteria

1. Reviewer trace: a reject for a coord with one `mCancelledSubscriptions`
   entry leaves no entry, and the next live accept for that coord commits
   without sending an unsubscribe.
2. Reviewer trace: the generation cases listed in `## Design` hold.
3. Live regression: the
   `Projects/BrokenEngineSandbox/Documents/AgentHarness/cross-cell.md` recipe
   passes through its restore step, with the rejected-cell probe logging
   `Client::ServerSubscribeAccept Rejected` and no subscription failure after
   the restore.
4. Client and server `Debug|x64` builds pass through `/compile`.

## Notes

Observed code at the working tree of the session that executed
`SubscriptionPlaceholderIndexMismatch.md` (client subscriptions keyed by coord
through `mPendingSubscriptions`); cite symbols, since line numbers move.
