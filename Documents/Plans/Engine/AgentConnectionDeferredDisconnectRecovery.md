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

The current generation-mismatch check is intentionally retained: it is
unreachable before recovery because `ServeConnection` does not observe peer
closure while deferred waiting. The recovery objective makes that existing
invalidation path reachable by adding peer-close detection; deleting the
generation identity or guard would remove the stale-response protection this
Plan is meant to establish. `AgentHarness` uses a normal 15,000 ms receive
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
thread. Keep `muiConnectionGeneration`, `muiDeferredGeneration`, the deferral
snapshot, and the existing `Drain()` mismatch branch: peer-close detection is
what makes that guard reachable during a deferred wait. Preserve the shutdown
response-flush deadline and the one-request, one-response-ID contract.

## Critical files

- `Engine/Source/Agent/AgentCommandServer.cpp:160-317` — active socket,
  deferred wait, generation, and response publication.
- `Engine/Source/Agent/AgentCommandServer.h:71-87` — generation identity and
  main-thread deferred state that must remain paired.
- `Engine/Source/Agent/AGENTS.md` — connection-generation, one-flight, and
  bounded-deferred-liveness contracts.
- `Tools/AgentHarness/AgentHarness.cpp:26-31,284-318,592-597` — ordinary
  client timeout and close behavior used for verification.

## In scope

- Releasing an active deferred `ServeConnection` when its peer closes.
- Generation invalidation and stale deferred-response handling needed so a
  later connection cannot receive the abandoned response.
- Retaining the existing generation fields, deferral snapshot, and
  `Drain()` mismatch guard, and making that guard reachable from peer-close
  detection during the deferred wait.
- The existing shutdown flush and socket ownership transitions.

## Out of scope

- Agent JSON framing, command payload validation, or deferred command behavior.
- Changes to the harness timeout, wire format, or command protocol.
- Deleting or bypassing the generation identity/invalidation guard; this Plan
  is the sole owner of that recovery concern.
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
- The listener may invalidate the generation and wake the main-thread path,
  but it never directly writes or destroys the main-thread-only
  `mDeferredPoll`.
- Socket close and response publication remain serialized with shutdown, and
  the existing bounded shutdown flush remains intact.

## Acceptance criteria

- A source/lifecycle trace shows why the existing generation-mismatch branch
  is unreachable before recovery today, and the implemented peer-close path
  bumps the generation so `Drain()` reaches that retained guard during a
  deferred wait.
- A deferred command whose client closes before completion releases the active
  listener and allows a second client to connect and complete a command within
  the normal transport timeout.
- The abandoned deferred result is discarded and cannot satisfy the second
  request's response ID.
- Normal deferred completion and process shutdown still preserve the existing
  response and close behavior.
- Client and server `Debug|x64` builds pass through `/compile`.

## Notes

Origin: `CAI/shard-0010/001`. The durable source and harness citations above
record the deferred-wait, peer-close, and generation-invalidation evidence. No
source fix or build was performed during routing.
