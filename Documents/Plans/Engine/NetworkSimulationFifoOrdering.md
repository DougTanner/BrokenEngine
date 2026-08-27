<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:52:21.613Z","dependsOn":[]} -->
# Preserve reliable FIFO across fast-forward transitions

## Context

The retained survivor `CAI/shard-0036/002` identifies a transition-ordering
gap in network simulation. `NetworkSimulation::DispatchOrEnqueue` dispatches a
new event immediately in the fast-forward branch at
`Engine/Source/Network/NetworkSimulation.h:190-203`, while
`Server::Poll`/`Client::Poll` flushes the earlier delayed deque only after the
ENet event loop. A delayed reliable packet can therefore be processed after a
newer packet on the same channel even though the normal queue establishes FIFO
release order.

The direct evidence is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0036.md:82`
and consolidated selector `Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:948`. Frozen/live source
identity matches baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`; this
routing session has not changed source. The supported timescale/fast-forward
transition and reliable control channel make the inversion reachable without
malformed traffic.

## Design

The author's recommendation is to flush already delayed packets before
dispatching newly received events when fast-forward becomes active. The flush
must preserve the existing per-channel release order and deliver each packet
once; an empty queue retains the current immediate fast-forward path. Apply the
same transition rule to client and server poll wrappers so reliable control
traffic has one ordering contract at both ends.

## Critical files

- `Engine/Source/Network/NetworkSimulation.h:137-155,182-240` — delay queue,
  fast-forward dispatch, and flush.
- `Engine/Source/Network/Server/Server.cpp:98-124` and the client poll caller
  — event-loop/flush order.
- `Engine/Source/GameBase.cpp:298-321` — supported timescale transition.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:75-95,175-201`
  — ordered control consumers used for verification.
- `Engine/Source/Network/AGENTS.md` and
  `Engine/Source/Network/Client/AGENTS.md` — FIFO and fast-forward contracts.

## In scope

- The delayed-to-fast-forward transition and poll ordering for reliable packets.
- Server/client delivery order and pending-queue flushing.
- Existing empty-queue fast-forward and normal delayed behavior.

## Out of scope

- Deferred queue admission bounds, packet budgets, or ENet transport tuning.
- Reliable wire formats, game request semantics, and timescale policy itself.
- Unreliable packet loss behavior except where required to preserve its current
  simulation profile semantics.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: this changes transport ordering across
client/server poll phases and can reorder reliable save/reset/subscription work.

Preserve these invariants:

- A reliable channel always processes older admitted packets before newer ones,
  including at the fast-forward boundary.
- Each delayed event is delivered once and remains on its documented channel.
- Fast-forward still removes intentional wall-clock delay and normal mode keeps
  its current release timing.

## Acceptance criteria

- In a non-disabled simulation profile, a delayed reliable save followed by a
  reset during fast-forward is processed in send order.
- The same ordering holds for client/control subscribe and unsubscribe traffic
  on the transition boundary.
- Empty-queue fast-forward and steady-state delayed/fast-forward operation
  retain their current behavior.
- Client and server `Debug|x64` builds pass through `/compile`.

## Notes

Origin: `CAI/shard-0036/002`; source selector is the shard line above and the
consolidated selector is `consolidated-index.md:948`. No source fix or build
was performed during routing.
