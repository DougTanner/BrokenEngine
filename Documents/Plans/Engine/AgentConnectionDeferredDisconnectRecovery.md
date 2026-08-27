<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:51:58.088Z","dependsOn":[]} -->
# Fix deferred agent connections after peer disconnect

## Context

The retained survivor `CAI/shard-0010/001` identifies a deferred agent-channel
liveness gap. `AgentCommandServer::ServeConnection` waits at
`Engine/Source/Agent/AgentCommandServer.cpp:235-249` only for
`mPendingResponse` or process stop. `Drain()` can leave a deferred poll active
at `:289-317`, and the listener increments `muiConnectionGeneration`, clears
the response, and closes the active socket only after `ServeConnection` returns
at `:171-187`. A client that times out and closes during a deferred command
therefore cannot release the single active connection until the deferred
result or the 1,800-drain timeout.

The direct evidence is the source report
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0010.md:35`
and consolidated selector
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:516`. The report's frozen and live target hashes
match baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`; this routing session
has not changed the source. `AgentHarness` uses a normal 15,000 ms receive
deadline (`Tools/AgentHarness/AgentHarness.cpp:26-31`), shorter than the
deferred drain bound, so the stale active connection is an ordinary supported
path.

## Design

The author's recommendation is to observe peer closure while the listener is
waiting for a deferred response, using the existing nonblocking socket and
transport lock boundaries. Route closure through the same generation bump,
pending-response clear, and active-socket close used after
`ServeConnection` returns. The main-thread-only `mDeferredPoll` must be
discarded or explicitly cancelled through a narrow main-thread handoff when
its generation becomes stale; it must never be bare-written by the listener
thread. Preserve the shutdown response-flush deadline and the one-request,
one-response-ID contract.

## Critical files

- `Engine/Source/Agent/AgentCommandServer.cpp:160-317` — active socket,
  deferred wait, generation, and response publication.
- `Engine/Source/Agent/AGENTS.md` — connection-generation, one-flight, and
  bounded-deferred-liveness contracts.
- `Tools/AgentHarness/AgentHarness.cpp:26-31,284-318,592-597` — ordinary
  client timeout and close behavior used for verification.

## In scope

- Releasing an active deferred `ServeConnection` when its peer closes.
- Generation invalidation and stale deferred-response handling needed so a
  later connection cannot receive the abandoned response.
- The existing shutdown flush and socket ownership transitions.

## Out of scope

- Agent JSON framing, command payload validation, or deferred command behavior.
- Changes to the harness timeout, wire format, or command protocol.
- Graphics capture, input scripts, and unrelated listener performance work.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: this crosses a background socket
thread, main-thread deferred work, connection-generation identity, and the
agent trust boundary.

Preserve these invariants:

- A disconnected peer releases the single in-flight channel within a bounded
  wait and cannot block acceptance of the next peer.
- A response produced for an abandoned generation is never sent to a later
  connection.
- Socket close and response publication remain serialized with shutdown, and
  the existing bounded shutdown flush remains intact.

## Acceptance criteria

- A deferred command whose client closes before completion releases the active
  listener and allows a second client to connect and complete a command within
  the normal transport timeout.
- The abandoned deferred result is discarded and cannot satisfy the second
  request's response ID.
- Normal deferred completion and process shutdown still preserve the existing
  response and close behavior.
- Client and server `Debug|x64` builds pass through `/compile`.

## Notes

Origin: `CAI/shard-0010/001`; source selector is the shard line above and the
consolidated selector is `consolidated-index.md:516`. No source fix or build
was performed during routing.
