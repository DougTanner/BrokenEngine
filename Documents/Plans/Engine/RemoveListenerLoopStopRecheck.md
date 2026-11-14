<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:42:48.850Z","dependsOn":[]} -->
# Remove the locked stop recheck from AgentCommandServer::ListenerLoop

## Context

Found by a deep analysis of `Engine/Source/Agent` (in-function phase),
verified by code reading against commit `4fa6d2d7`; nothing was built or run.
Line numbers are from that commit and are in
`Engine/Source/Agent/AgentCommandServer.cpp`.

After accepting a connection, `ListenerLoop` takes `mMutex`, tests
`stopToken.stop_requested()`, and on stop closes the socket and breaks
(`:163-170`). The recheck excludes nothing: stop can be requested the instant
after `:170` releases the lock, so every later point already handles it —
the `ServeConnection` loop condition (`:191`), the `ReadExact` checks (`:452`,
`:472`), the locked check at `:226`, and the wait predicate at `:242`.

With the block removed and stop already requested, `ServeConnection` fails
its loop condition and returns without touching the socket or a shared slot;
the teardown block (`:178-183`) bumps `miConnectionGeneration`, resets
`mOptionalPendingResponse`, and closes the accepted socket; the loop top
(`:132`) breaks. The socket is closed exactly once either way. The only
state difference is that bump and reset, and neither is observed: the
previous connection's teardown already reset the slot, `PublishResponse`
(`:438`) refuses a stale generation, and `request_stop` has one caller, the
destructor (`:108`), which runs on the main thread that calls `Drain` and has
already called `ClearDeferredResponse` (`:98`).

Unmet criterion: `ListenerLoop` checks stop once per iteration before handing
the socket to `ServeConnection`.

Low priority: the gain is one lock acquisition per connection and eight
lines.

## Design

Delete `:163-170` only. The stop-under-lock rule in
`Engine/Source/Agent/AGENTS.md` `## Architecture` exists for the
condition-variable waits and is not touched.

Risk tier: Tier 3 (the author's classification). Trigger: threading — the
change removes a locked stop check on the listener thread's shutdown path,
and its safety rests on the interleaving argument above, which was made by
code reading and not run. No determinism, wire, or serialization exposure;
the command channel is a developer tool.

## Critical files

- `Engine/Source/Agent/AgentCommandServer.cpp`

## In scope

- The locked `stop_requested` block between the non-blocking socket
  configuration and the `ServeConnection` call in
  `AgentCommandServer::ListenerLoop`.

## Out of scope

- `ServeConnection`, `ReadExact`, `Drain`, `PublishResponse`, and the
  destructor.
- The connection teardown block and the generation counter.
- Whether a request from a closed connection still runs.

## Acceptance criteria

1. Code read: `ListenerLoop` holds one stop check per iteration, at the loop
   top, and passes the accepted socket straight to `ServeConnection`.
2. `/compile` builds the BrokenEngineSandbox client and server.
3. A `/agent-harness` run that launches the client and the server, sends
   commands over several connections, and sends `quit` to each sees both
   processes exit without a hang. The supplied evidence records no
   harness setup, because the finding was proven by code reading only.
