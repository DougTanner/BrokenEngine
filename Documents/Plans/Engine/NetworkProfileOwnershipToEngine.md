<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T18:52:47.288Z","dependsOn":["Documents/Plans/Engine/ClientDesyncPolicyToEngine.md","Documents/Plans/Engine/ClientPollTimingRecovery.md"]} -->
# Move the Network profile and NetworkGraphs surface to the engine

## Context

After world-grid state and the Frames profile are settled, the Network profile
surface remains game-coupled. `Projects/BrokenEngineSandbox/Source/Profile/ProfileManager.cpp:162-205,208-360`
and `NetworkGraphs.cpp:34-59` read client transport, subscriptions,
prediction, clock, desync, and reconciliation state. The investigation requires
this work to be sequenced with the client-runtime, desync, and reconciliation
ownership work rather than bundled into the Frames extraction.

## Pre-implementation decisions and options

The packet permits the next agent to resolve the remaining boundary. Record
the choice and the current prerequisite paths before implementation:

- **Option A (recommended):** move the generic Network profile host and graph
  formatting to the engine after the client-runtime, desync, and reconciliation
  seams are ready; keep game policy behind the existing direct data contract.
- **Option B:** keep `NetworkGraphs` game-owned and move only the generic profile
  host now, deferring graph ownership until those seams are complete.

In either option, preserve the existing profile accessors and graph sequencing;
do not invent a callback registry, retained writer, or speculative common
network abstraction.

## Design

Depend first on `WorldGridStateOwnershipToEngine`, then inspect the landed
client-runtime, prediction, desync, clock, and reconciliation boundaries before
choosing the NetworkGraphs placement. Adapt the profile rows and graph inputs
without changing their ordering, labels, cadence, or source-state semantics.
NetworkGraphs must consume the settled client-runtime/desync/reconciliation
state in its existing sequence; it must not reach back into the Frames/state
extraction or run as an unsequenced side effect.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Profile/ProfileManager.cpp:162-205,208-360`
  — Network profile rows and state reads.
- `Projects/BrokenEngineSandbox/Source/Profile/ProfileManager.h` — profile
  declarations and accessors used by the engine-facing surface.
- `Projects/BrokenEngineSandbox/Source/Profile/NetworkGraphs.cpp:34-59` — graph
  construction and sequencing.
- Client runtime, prediction, desync, clock, and reconciliation owners found
  and recorded during prerequisite preparation — required consumers/producers
  of the moved profile data.

## In scope

- Select and record the Network profile/NetworkGraphs ownership option after
  the required client-runtime, desync, and reconciliation seams are verified.
- Move or adapt the Network profile rows and, if selected, `NetworkGraphs`.
- Preserve existing data sources, row/graph order, labels, cadence, and
  allocation discipline.
- Update every direct producer/consumer required by the selected boundary.

## Out of scope

- World-grid storage, active-coordinate ordering, or the Frames profile surface
  owned by `WorldGridStateOwnershipToEngine`.
- ServerDisplay, `ServerCellStats`, or window lifetime.
- New network protocol, packet, CRC, replay, simulation, or save-state behavior.
- Reconciliation/client-runtime redesign, callback or hook registries, backward
  compatibility aliases, speculative abstractions, or unit tests.

## Risk tier and invariants

Tier 3 — cross-subsystem client-runtime/profile integration touching transport,
prediction, timing, desync, and reconciliation observations.

- Network profile and NetworkGraphs read the same settled state and sequence as
  today; no stale pre-reconciliation values are displayed.
- Frames/world-grid ownership remains a completed prerequisite, not duplicated.
- No profile-only read changes simulation, CRC, replay, wire, or save state.
- The selected boundary has no retained game callback or engine-owned policy
  registry.

## Coordination

The scheduler prerequisites are
`Documents/Plans/Engine/ClientDesyncPolicyToEngine.md`,
`Documents/Plans/Engine/ClientPollTimingRecovery.md`, and
`Documents/Plans/Engine/WorldGridStateOwnershipToEngine.md`; the metadata edge
above enforces their completion before this Plan is selected. Network profile
and NetworkGraphs work must consume the settled generic desync, poll-timing,
and world-grid/profile seams from those Plans without duplicating their state,
sequencing, or policy. Those prerequisite Plans do not depend directionally
on this profile extraction.

## Acceptance criteria

1. The selected option and the verified client-runtime/desync/reconciliation
   sequencing decisions are recorded before editing.
2. Static traces identify every Network profile and NetworkGraphs producer and
   consumer, with no stale game-only bypass across the chosen boundary.
3. A client scenario or deterministic source trace proves unchanged profile row
   order, graph order, labels, cadence, and post-reconciliation values.
4. NetworkGraphs is either moved with its verified seams or explicitly remains
   game-owned under Option B; it is not partially duplicated.
5. Client Debug/Release builds and the smallest network-profile observations
   pass; no unit tests are added.

## Execution card

- **Tier/triggers:** Tier 3; profile ownership crosses client runtime,
  prediction, timing, desync, reconciliation, and graph consumers.
- **Roles:** implementer; fresh plan/correctness/scope reviewers; builder and
  harness; affected-code, cleanup, and landing verification roles.
- **Dependency:** `Documents/Plans/Engine/ClientDesyncPolicyToEngine.md`,
  `Documents/Plans/Engine/ClientPollTimingRecovery.md`, and
  `Documents/Plans/Engine/WorldGridStateOwnershipToEngine.md` must land first;
  these three metadata edges are the required scheduler edges.
