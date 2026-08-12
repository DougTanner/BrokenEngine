<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-11T00:00:08.000Z","dependsOn":[]} -->
# Extract the game-packet admission gate into an engine helper

## Context

`ServerSession::ParseReceivedGamePackets` (`Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:73-282`) is the server's trust boundary for client-sent game packets. Its structure is two engine-owned halves wrapped around one game-owned `switch`:

- The admission gate at `:79-113`: skip silently when `FindClient` returns null (the client was purged mid-drain by an earlier violation disconnect); reject on the `iMaxSize == 0` sentinel meaning "not client-sendable"; reject when `payload.size() + 1` falls outside `[iMinSize, iMaxSize]`; increment and test the per-type per-tick counter `tickTypeCounts[uiPacketType]` against `iMaxPerTick`, recording a violation only when `bOverCapCountsViolation` is set and otherwise dropping silently.
- The throw boundary at `:115` (`try`) and `:270-279` (`catch`): drop the single packet, log at `kDebug`, and record a `"game packet handler threw"` contract violation.

Both halves are pure engine mechanics over `engine::ClientPacketContract`, `engine::ClientConnection`, and `engine::Server::RecordContractViolation` — the drop-then-count-then-escalate policy the engine already owns. Only the `switch` between them is game-specific. A second game would have to re-derive this gate exactly, and any divergence is a security hole rather than a cosmetic difference.

## Design

Add two `engine::Server` member functions beside `RecordContractViolation` (declared at `Engine/Source/Network/Server/Server.h:191`, defined in `Server.cpp`):

- `bool AdmitGamePacket(const ReceivedGamePacket& rPacket, const ClientPacketContract& rContract)` — returns `true` when the packet should be dispatched, `false` when it was skipped or rejected. It performs the `FindClient` null-skip, the three rejection checks, and the per-tick cap increment in exactly the current order, with the current violation reason strings and the current `iFullSize` argument (`static_cast<int64_t>(rPacket.payload.size()) + 1`). It looks up the client itself, so the game loop no longer holds a `ClientConnection*` at all.
- `void RecordGamePacketHandlerThrow(const ReceivedGamePacket& rPacket)` — the catch's violation accounting: `RecordContractViolation(rPacket.iClientId, "game packet handler threw", rPacket.uiPacketType, payload.size() + 1)`, so the reason string and the size argument stay in one place.

The game keeps ownership of the contract lookup (`NetworkSessionContract::GetClientPacketContract`, `ServerSession.cpp:88`), because the contract table is game-defined, and keeps the `switch` and the `try`/`catch` body. The loop in `ParseReceivedGamePackets` becomes: look up the contract, `if (!engine::gpServer->AdmitGamePacket(rPacket, contract)) continue;`, then `try`/`switch`/`catch`, with the catch keeping its `kDebug` log line and calling `RecordGamePacketHandlerThrow`.

Comment placement: the null-client, sentinel, and per-tick-cap-window comments (`:84`, `:93-94`, `:103-105`) move into `AdmitGamePacket` with the code they explain; the two-line loop-structure comment at `:79-80` ("Contract gate ... backstops remain") stays at the game call site; the throw-boundary rationale comment (`:272-275`) stays with the game `catch`, and the drop-then-count line (`:277`) moves into `RecordGamePacketHandlerThrow`.

No file is added or deleted, so `/update-vcxproj` is not triggered. `Documents/Architecture/Network.md:46` and `:51` describe this gate ("game types at `ServerSession::ParseReceivedGamePackets` via `GetGamePacketContract`"; the `"game packet handler threw"` accounting): the choke point stays in `ParseReceivedGamePackets`, so the flow those lines describe is unchanged, but they must be inspected and reworded only if naming the engine helper is needed for accuracy.

## Critical files

- `Engine/Source/Network/Server/Server.h`, `Server.cpp` — the two helpers, beside `RecordContractViolation`
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp` — the gate and catch accounting replaced by calls
- `Documents/Architecture/Network.md` — inspect `:46`/`:51`; edit only if the helper move makes the wording inaccurate

## In scope

- The admission gate at `ServerSession.cpp:79-113` and the violation accounting inside the catch at `:270-279`, moved into the two engine helpers named in Design
- The comments listed in Design, moved with or kept beside the code they explain
- Any `Engine/Source/Network/Server/AGENTS.md` or game `Network/AGENTS.md` sentence that names the old owner, and the `Documents/Architecture/Network.md` inspection above

## Out of scope

- `NetworkSessionContract::GetClientPacketContract` and the game contract table — the table stays game-owned
- The `switch (eType)` dispatch and every per-case handler, including their backstop size checks and `ValidateNavigationDelay` clamps, which stay exactly as they are
- The `try`/`catch` structure itself and its `kDebug` log line, which stay in the game
- `RecordContractViolation`'s own escalation and disconnect policy
- The engine-type contract gate in `Server::Receive` (`Server.cpp:241`), which already lives in the engine
- Subscription servicing in the same file — owned by `Documents/Plans/Network/ServerSubscriptionServicingToEngine.md`
- The active-set functions in the same file — owned by `Documents/Plans/Network/ActiveSetSkeletonToEngine.md`

## Risk tier and invariants

Tier 3 — this is a trust boundary against hostile input. Invariants that must hold byte-identically: check order is null-client, then sentinel, then size range, then per-tick cap; the size compared is `payload.size() + 1` (the stripped type byte counted back in); `RecordContractViolation` may remove the client, so no `ClientConnection*` is dereferenced after any call to it — the cap counter increment happens through the pointer before the only violation call that can follow it; the per-tick counter is incremented before the comparison (`++counter > iMaxPerTick`) so the cap counts this packet; an over-cap packet records a violation only when `bOverCapCountsViolation`; the violation reason strings (`"game type not client-sendable"`, `"game packet size out of range"`, `"game packet per-tick cap exceeded"`, `"game packet handler threw"`) are unchanged, since they are the operator-visible signal; the counter array is shared across engine and game dispatch points within one update window, which is coherent only because the two type-byte ranges are disjoint.

## Acceptance criteria

- Client and server compile; the gate appears once, in the engine.
- A harness run with normal play shows no new violations recorded and no dropped packets.
- Fault injection for each rejection path — an unknown/server-only type byte, an undersized payload, an oversized payload, and a burst exceeding the per-tick cap — produces the same violation reason, the same drop-versus-count outcome, and the same escalation as before the change.

## Scores

Effort 1 / Impact 3 / Risk 2
