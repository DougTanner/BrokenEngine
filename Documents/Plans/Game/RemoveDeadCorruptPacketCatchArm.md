<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:43:23.160Z","dependsOn":[]} -->
# Remove the dead corrupt-packet catch arm from ProcessReceivedGamePackets

## Context

Found by a deep analysis of `Projects/BrokenEngineSandbox/Source/Network`
(architecture phase), verified against commit `4cf9d607`.

`ClientSession::ProcessReceivedGamePackets`
(`Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp:99`)
wraps the fleet-sync and map fleet marker handlers in one `try` with two catch
arms. The first arm (`:118-126`) catches `std::ios_base::failure`, logs
"dropped corrupt packet" at `kError`, and asserts; its comment (`:120-123`)
says a shared reader throwing that type means server bytes the client cannot
decode. Nothing in the `try` throws that type:

- `MessageReader` (`Engine/Source/Network/NetworkMessages.h`) only clears its
  validity flag. The one throw is `ThrowCorruptStream`, reached from
  `NetworkMessages::Read`, which neither `FleetSyncMessage::ReadPayload` nor
  `MapFleetMarkersMessage::ReadPayload`
  (`Projects/BrokenEngineSandbox/Source/Network/GameMessages.h`) calls.
- `FleetSelection::SynchronizeFleets` has no `throw`.
- An out-of-range count reaches `std::vector::resize` and lands in the second
  arm (`std::exception`, log a warning and continue).

History shows the throw was removed on purpose. Commit `bca1752f` ("Remove
client-side checks on server-to-client data", completing a Plan) deleted the
validity throws from `FleetSyncMessage::ReadPayload` and wrote the comments
above both `ReadPayload` functions. The arm stayed live then because the same
`try` still decoded player events through a shared reader; commit `fe7ca062`
moved that decode into the engine `Client::Receive`, which is when the arm
lost its last thrower.

`Projects/BrokenEngineSandbox/Source/Network/Client/AGENTS.md`
`## Reconciliation Invariants` (third bullet) still says the fleet-sync and
map fleet marker handlers "share one boundary that splits corrupt data from an
ordinary local failure". The code cannot do that split.

Two authorities disagree here, and this Plan records which was trusted:
the leaf sentence above (restated on 2026-11-13 by commit `ae681f03`) against
`Engine/Source/Network/AGENTS.md` `## Corrupt Input Policy` ("client code adds
no new value or structural checks on server data") and the approved change
`bca1752f`. The policy and the approved change are trusted; the leaf sentence
is the side to correct.

## Design

Recommended by the authoring analysis, not chosen by the user: delete the arm
and correct the sentence. The rejected alternative is to make both
`ReadPayload` functions throw on an invalid reader so the arm becomes live; it
would re-add a check an approved change removed and that the corrupt-input
policy forbids. The one reason to keep the arm is as a parallel to the engine
`Client::Receive` catch, which stays live because engine decoders still go
through `NetworkMessages::Read`; keeping an arm for a thrower that does not
exist is speculative, so the recommendation is to delete it.

1. `ClientSession::ProcessReceivedGamePackets`: delete the
   `catch (const std::ios_base::failure&)` arm with its comment, log, and
   `ASSERT`. The `std::exception` arm and `eParsingType` stay.
2. `Client/AGENTS.md` `## Reconciliation Invariants`, third bullet: reword the
   clause so it says the two handlers share one boundary that logs an ordinary
   local failure as a warning, and that their decoders raise no corrupt-data
   signal, under the hub's corrupt-input policy; through `/update-claude-docs`.

No reachable behavior changes: the deleted arm cannot be entered.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Client/AGENTS.md`

## In scope

- The `std::ios_base::failure` catch arm of
  `ClientSession::ProcessReceivedGamePackets`.
- The fleet-sync and map fleet marker clause of the third bullet of
  `Client/AGENTS.md` `## Reconciliation Invariants`.

## Out of scope

- Adding any throw or validity check to `FleetSyncMessage::ReadPayload`,
  `MapFleetMarkersMessage::ReadPayload`, or `MessageReader`.
- The `std::exception` arm, and the engine `Client::Receive` catches.
- `Engine/Source/Network/AGENTS.md` and
  `Documents/Architecture/GameReconciliation.md`, which stay true.
- The `BoundedCount` arguments in `GameMessages.h` that feed the unread
  validity flag (noted by the analysis as an engine-mechanism question, not
  accepted as debt).

## Risk tier

Tier 2 — scoped behavior. Trigger: the change edits the exception boundary of
the client's handlers for server data, which Tier 1's "no invariant exposure"
excludes. It stays Tier 2 because it does not alter what the client trusts or
any reachable outcome.

## Acceptance criteria

- Code read: `ProcessReceivedGamePackets` has one catch arm, for
  `std::exception`, and no callee in its `try` throws
  `std::ios_base::failure`.
- Code read: `Client/AGENTS.md` `## Reconciliation Invariants` no longer says
  the game packet handlers split corrupt data from a local failure.
- `/compile` builds the BrokenEngineSandbox client.
- The repository static checks pass.

## Notes

The choice between deleting the arm and keeping it as a deliberate parallel
was made by the analysis from the evidence above. The user has not chosen it;
it binds only once the user approves it
(`.agents/references/authority-order.md`).
