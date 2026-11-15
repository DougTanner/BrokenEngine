<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:44:13.434Z","dependsOn":[]} -->
# Remove the out-of-range slot branch in Client::ServerSubscribeAccept

## Context

Found by the architecture review phase of a deep analysis of
`Engine/Source/Network/Client` at commit `d87ad840`. Line numbers are from
that commit; locate by symbol if they moved.

`Client::ServerSubscribeAccept`
(`Engine/Source/Network/Client/Client.cpp:721-732`) tests
`iSlotIndex >= std::ssize(mSubscriptions.mCoordinateSlots)` and, when true,
logs a warning, sends an unsubscribe, and returns. Its comment calls it
"Defensive (trust boundary: network input)".

That is a structural check on server data. Root `AGENTS.md` `## Key Patterns`
says the client trusts server-to-client data, and
`Engine/Source/Network/AGENTS.md` `## Corrupt Input Policy` says "client code
adds no new value or structural checks on server data".

No writer can produce the state:

- The client pool is `mCoordinateSlots.resize(kiCoordinateSlots)`
  (`Client.cpp:25`). The server's `AllocateSlot` loops
  `i < kiCoordinateSlots` (`Engine/Source/Network/Server/Server.h:103-112`).
  Both read the one constant in `Engine/Source/Network/NetworkProtocol.h`.
- The only other value the server sends is `kiSubscribeRejectSlot`
  (`Server.cpp:749`, `757`), which the handler takes earlier, at
  `Client.cpp:713`.
- The pools can differ only between builds with different
  `kiCoordinateSlots`, a compatibility constant `NetworkProtocol` owns under
  the `kiProtocolVersion` Hello gate.
- The sibling handlers index the slot unchecked (`Client.cpp:431`, `493`,
  `591`, `604`, `791`).

Without the branch, an impossible out-of-range accept would throw at
`mCoordinateSlots.at()` (`Client.cpp:736`) and reach the dispatch catch, the
outcome every sibling handler has.

`Engine/Source/Network/Client/AGENTS.md` `## Subscription Receive Invariants`
records the branch twice: "The one exception is the out-of-range accept branch
in `Client::ServerSubscribeAccept`, which consumes nothing." and "A non-reject
subscribe-accept slot outside the client pool immediately unsubscribes so the
server slot cannot leak."

History: the Plan `ClientServerDataCheckRemoval` (landed as commit
`bca1752f`; read it with
`git show bca1752f^:Documents/Plans/Engine/ClientServerDataCheckRemoval.md`)
removed client checks on server data and kept this branch on purpose, as "a
recovery unsubscribe that only agent fixtures reach", which a fixture
observed. Commit `9c746fa0` renamed that fixture to a harness rig, and commit
`1e88528b` then deleted the rig: no file outside `Client.cpp`, `Server.cpp`,
`NetworkMessages.h`, and `NetworkProtocol.h` names
`ServerSubscribeAcceptMessage` or `kServerSubscribeAccept`, and no
`client_packet_fault` fixture remains. The reason for keeping the branch is
gone, but the leaf document still records it.

Contradiction, named as root `AGENTS.md` `### Diagnosis Discipline` asks: the
root and hub policy and
`.agents/references/cpp-conventions.md` (no check for a state no writer can
produce) favor removal; the last approved Plan and the leaf document record
the branch as kept. The hub policy wording forbids new checks and does not by
itself order removing a recorded one. This Plan trusts the policy side,
because the kept side rested on a fixture that no longer exists.

Unmet condition: the client holds no structural check on server data that no
writer can fail.

Originating gap: that review's client/server lens. It is not an unmet
acceptance criterion of any change. It is pre-existing: the analysis session
changed no source.

## Design

Remove the branch: delete the branch and its comment (`Client.cpp:721-732`),
and delete the two document sentences quoted in `## Context`. This is the
Plan author's choice, not a choice the user has made. Rationale: the state is
unreachable, the reason an earlier Plan kept the branch no longer exists, and
every sibling handler already relies on the same pool-size agreement.

Alternative considered and not recommended: keep the branch, reword its
comment so it no longer claims a trust boundary, and state once in the leaf
document that the branch is a recorded exception to the corrupt-input policy.
It leaves in place a check no writer can fail.

The reject-sentinel branch and the position of the `TakeAnswer` call stay
exactly as they are.

Risk tier: Tier 3 (the author's classification). Trigger: the change removes
a check at the client's trust boundary for server data. The wire layout and
`kiProtocolVersion` do not change, and no CRC, save, or replay state is
involved. The executing session derives the tier again.

## Critical files

- `Engine/Source/Network/Client/Client.cpp`
- `Engine/Source/Network/Client/AGENTS.md`

## In scope

- In `Client::ServerSubscribeAccept`: the
  `iSlotIndex >= std::ssize(mSubscriptions.mCoordinateSlots)` branch and the
  two comment lines above it.
- `Engine/Source/Network/Client/AGENTS.md`
  `## Subscription Receive Invariants`: the sentence "The one exception is the
  out-of-range accept branch in `Client::ServerSubscribeAccept`, which
  consumes nothing." and the sentence "A non-reject subscribe-accept slot
  outside the client pool immediately unsubscribes so the server slot cannot
  leak."

## Out of scope

- The `kiSubscribeRejectSlot` branch, the `TakeAnswer` order,
  `ClassifySubscribeAccept`, and the rest of the handler.
- The `kConnectionAccepted` tests in `Client::ServerTimespeedUpdate` and
  `Client::ServerChat`: they test protocol state, an approved Plan kept them,
  and a simulated-delay build reaches them.
- The four load-generation gates.
- The server's slot allocation and `kiCoordinateSlots`.

## Acceptance criteria

1. `Client::ServerSubscribeAccept` holds no out-of-range slot test, and the
   leaf document no longer describes one.
2. `/compile` builds the BrokenEngineSandbox client and server.
3. Through `/agent-harness`, with a local server: the client connects,
   subscribes, and publishes an authoritative cell, so the ordinary accept
   path still commits. The supplied evidence records no harness setup; the
   executing session sets it up from the harness documentation.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
