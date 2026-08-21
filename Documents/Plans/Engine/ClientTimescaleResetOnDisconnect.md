<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-20T19:20:48.276Z","dependsOn":[]} -->
# Reset Client Timescale On Disconnect

## Context

The server drives every client's simulation speed ratio with a timescale
update packet. A client applies whatever ratio it last received
(`Engine/Source/Network/Client/ClientReceive.cpp:643`,
`game::gpGame->mTimeStep.SetTimeScale(...)`) and nothing ever puts that ratio
back to 1/1 locally: `ClientSessionRuntime::Disconnect`
(`Engine/Source/Network/Client/ClientSessionRuntime.cpp:136-148`) resets the
clock, subscription state, and discovery flags, but leaves `mTimeStep`
untouched. `SetTimeScale` is called from exactly two places in the tree — that
client receive path and the server's last-client-leave reset — so no other code
restores it.

The two server-side behaviours make the leftover ratio observable. The server
returns itself to 1/1 when its last client leaves
(`Engine/Source/Network/Server/ServerSessionRuntime.cpp:42-57`,
`ResetOnLastClientLeave`), and it deliberately sends nothing to a newly joining
client when the current ratio is already 1/1
(`Engine/Source/Network/Server/ServerSend.cpp:316-324`,
`Server::SendTimespeedToNewClient` returns early on multiply == 1 and
divide == 1).

Net effect: a client that disconnects while running a non-1/1 ratio keeps that
ratio. On reconnect — to a restarted server, or to a server that has since
returned to 1/1 — the client receives no timescale packet at all and keeps
simulating at the stale speed until some later timescale change is broadcast.

Pre-existing defect. Both the missing reset and the 1/1 send suppression exist
unchanged at commit f14b1c9c53a1bc8fbc609ad6383d7126c435adf7 (at that baseline
the suppression lived in the game at
`Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:381`
before being moved into the engine); the session that recorded this Plan
explicitly preserved those semantics and did not introduce the defect.

## Design

Reset the client's own timescale to 1/1 when the client session runtime
disconnects, in `ClientSessionRuntime::Disconnect`, alongside the existing
`ResetClock()` and `ClearSubscriptionState()` calls: when
`game::gpGame->mTimeStep.miTimeMultiply` or `miTimeDivide` is not 1, call
`game::gpGame->mTimeStep.SetTimeScale(1, 1)`. This mirrors the guarded shape
the server already uses in `ResetOnLastClientLeave`, and `Disconnect` already
reaches `game::gpGame` elsewhere in the same file, so no new ownership boundary
is crossed.

This choice — rather than removing the server's 1/1 send suppression — is
decided, not an option: the client-side reset also covers the case where the
client reconnects to a freshly restarted server process, and it keeps the
server's per-join packet count unchanged. Do not change
`Server::SendTimespeedToNewClient`; its suppression stays exactly as it is.

## Critical files

- `Engine/Source/Network/Client/ClientSessionRuntime.cpp` —
  `ClientSessionRuntime::Disconnect`
- `Engine/Source/Frame/TimeStep.h` / `.cpp` — `SetTimeScale`, `miTimeMultiply`,
  `miTimeDivide` (read and call only)

## In scope

- Adding the guarded 1/1 timescale reset inside
  `ClientSessionRuntime::Disconnect`.
- Any AGENTS.md sentence that must change because the disconnect reset set
  changed (`Engine/Source/Network/Client/AGENTS.md` reset description).

## Out of scope

- `Server::SendTimespeedToNewClient` and its 1/1 suppression.
- `ServerSessionRuntime::ResetOnLastClientLeave` and server-side pause or
  timescale policy.
- The timescale wire format, packet identifier, protocol version, and any
  ownership move between game and engine (`ServerTimescalePacketToEngine.md`
  owns that question).
- `ResetClock`, subscription state, discovery flags, and every other existing
  disconnect step.
- Adding a timescale reset to any other path (connect, server load, game
  shutdown).

## Risk tier and invariants

Expected Change Workflow Tier 2: one subsystem's runtime behavior, in the
client's own disconnect path. It touches no wire layout, no serialization, no
save/replay compatibility, and no threading. It does change simulation pacing,
so the reset must stay strictly on the disconnect path — the client must never
override a ratio the server has commanded while connected, and the server's
authority over timescale while connected is unchanged. Escalate to Tier 3 if
implementation finds the reset must move into shared or deterministic
per-tick state.

## Acceptance criteria

- After a client sets a non-1/1 speed and then disconnects, the client's
  `mTimeStep` ratio reads 1/1 (multiply 1, divide 1).
- A client that reconnects to a server running 1/1 simulates at 1/1 without
  waiting for any timescale broadcast.
- While connected, a server-commanded non-1/1 ratio still applies on the
  client and is not reset by any other path.
- `Server::SendTimespeedToNewClient` is unchanged in the final diff.
- Client and server both compile.
