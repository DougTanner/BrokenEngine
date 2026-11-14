<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:44:22.435Z","dependsOn":[]} -->
# Persist the client GUID from the runtime, without a transport callback

## Context

Found by the architecture review phase of a deep analysis of
`Engine/Source/Network/Client` at commit `d87ad840`. Line numbers are from
that commit; locate by symbol if they moved.

`Client` carries a function pointer for one caller:

- the typedef `Client::GuidAssignedCallback`
  (`Engine/Source/Network/Client/Client.h:68`);
- a constructor parameter (`Client.h:70`, `Client.cpp:16`);
- the member `mpGuidAssignedCallback` (`Client.h:183`), assigned at
  `Client.cpp:20`;
- a null test and the call, in the connection-response handler
  (`Client.cpp:672-676`).

`Client` is constructed at exactly one site in the repository,
`ClientSessionRuntime::Connect`
(`Engine/Source/Network/Client/ClientSessionRuntime.cpp:349`), which always
passes `&PersistClientGuidToDisk`. So the null test guards a state no caller
produces, and the pointer never varies.

`PersistClientGuidToDisk` is `static` in `ClientSessionRuntime.cpp` (line 43),
beside `ClientGuidFile` and `LoadClientGuidFromDisk`, so `Client.cpp` cannot
name it. `Engine/Source/Network/Client/AGENTS.md` `## Overview` says the
runtime owns GUID persistence, and the comment at `Client.cpp:672` gives the
callback's purpose as keeping GUID disk I/O out of the transport. A direct
call from `Client.cpp` would need external linkage or would move persistence
into the transport against that ownership.

The runtime already sees the accept. In `ClientSessionRuntime::PollAndDrain`,
after `mpClient->Poll`, the block that returns early handles the not-accepted
case; past it, line 467 sets `ClientSessionStateFlags::kConnectionWasAccepted`
on every poll of an accepted connection. The first such poll is the one where
that flag is still clear.

The server sets `bHasGuid` on every accept
(`Engine/Source/Network/Server/Server.cpp:668`, `703`, `895`), so "persist on
the first accepted poll" equals today's "persist when the accept carries a
GUID".

This is not a violation of `Engine/Source/AGENTS.md` `## Hub Conventions`: the
callback reaches engine code, not game code. The case for removing it is that
it is an extension point with one fixed use.

Unmet condition: the transport holds no single-use function pointer and no
null test for a value that is never null.

Originating gap: that review's AI-generation-residue lens. It is not an unmet
acceptance criterion of any change. It is pre-existing: the analysis session
changed no source.

## Design

Recommended (the author's recommendation):

- In `ClientSessionRuntime::PollAndDrain`, where
  `kConnectionWasAccepted` is set, test that the flag is still clear and, when
  it is, call `PersistClientGuidToDisk(mpClient->mClientGuid)` before the flag
  is set and before `game::gpClientSession->OnConnectionAccepted()`.
  Rationale: persistence stays in the runtime that owns it, in the same
  `PollAndDrain` call as today, on the main thread, and before any game
  callback.
- Remove the typedef, the constructor parameter, the member, the assignment,
  and the null test and call with their comment. The `mClientGuid` assignment
  and its log line in the handler stay.
- Update the game document's sentence that describes the callback
  (`## In scope`).

`Projects/Coretide` is a private game project that is absent from a session
worktree. This Plan changes the `Client` constructor signature, so the
executing session searches `Projects/Coretide` in the primary checkout for
`GuidAssignedCallback` and for constructions of `engine::Client`, and reports
what it finds before it changes the signature.

Risk tier: Tier 2 (the author's classification). Trigger: a public
constructor signature of an engine class changes, and a file write moves from
inside `Client::Poll` to just after it. `ClientGuid.bin` and
`ClientGuidFile::kiVersion` do not change. No packet, CRC, or replay change.

## Critical files

- `Engine/Source/Network/Client/Client.h` and `Client.cpp`
- `Engine/Source/Network/Client/ClientSessionRuntime.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Client/AGENTS.md`

## In scope

- `Client::GuidAssignedCallback`, the fourth `Client` constructor parameter,
  `Client::mpGuidAssignedCallback`, and its assignment.
- In the connection-response handler in `Client.cpp`: the comment, null test,
  and callback call inside the GUID-assigned block.
- In `ClientSessionRuntime::Connect`: the fourth constructor argument.
- In `ClientSessionRuntime::PollAndDrain`: the one persist call at the first
  accepted poll.
- `Projects/BrokenEngineSandbox/Source/Network/Client/AGENTS.md`
  `## Session Policy`: the words "let the transport accept callback write it
  atomically".

## Out of scope

- `PersistClientGuidToDisk`, `LoadClientGuidFromDisk`, and `ClientGuidFile`:
  bodies, linkage, and format.
- How the server assigns or sends a GUID.
- `OnConnectionAccepted` and the other connection handlers.
- `PollPendingCrashReport`.

## Invariants

- The GUID is written once per accepted connection, before the game sees
  `OnConnectionAccepted`.
- A rejected or failed connection writes nothing.

## Acceptance criteria

1. A search of `Engine` and `Projects/BrokenEngineSandbox` finds no
   `GuidAssignedCallback`.
2. `/compile` builds the BrokenEngineSandbox client and server.
3. Through `/agent-harness`, with a local server and a client app-data
   directory that holds no `ClientGuid.bin`: after the client connects, the
   file exists; after a second launch and connect with the same directory,
   the client log's "Client GUID assigned" line shows the same GUID. The
   supplied evidence records no harness setup; the executing session sets it
   up from the harness documentation.

## Coordination

No Plan below depends on this one, and it depends on none. Whichever lands
second re-derives its regions by symbol from the landed files.

- `Documents/Plans/Engine/ClientConnectFrameTransitionInEngine.md` replaces
  the `OnConnectionAccepted` call in `PollAndDrain`, next to where this Plan
  adds the persist call, and also edits the game client `AGENTS.md`. The
  persist call stays ahead of whatever the accepted branch calls first.
- `Documents/Plans/Engine/ReduceClientCpp.md` may move the `Client`
  constructor or the connection-response handler.
- `Documents/Plans/Engine/ReduceClientSessionRuntimeCpp.md` may move `Connect`
  or `PollAndDrain`. `PersistClientGuidToDisk` is `static`, so the persist
  call and that function must end up in one file, or the reduction gives the
  function a declaration.
