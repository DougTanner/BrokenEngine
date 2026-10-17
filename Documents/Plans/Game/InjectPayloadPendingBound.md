<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-17T15:06:15.647Z","dependsOn":["Documents/Plans/Game/FrameEditRows.md"]} -->
# Bound `inject_payload` so agent-queued status changes cannot exceed the per-cell cap

Line numbers cite baseline `d8ce779b3b4fb984b572522896b6382d8dd19d19` plus the shape `Documents/Plans/Game/FrameEditRows.md` (the unified spawn system, this Plan's `dependsOn`) gives the harness and the engine `ServerSpawnQueue`. Where a statement here and the code disagree, the code wins; report the contradiction instead of matching one side to the other.

## Context

A cell's published batch for one tick is that tick's `FrameInput::statusChanges` snapshot plus the arrivals applied to it after the tick (`Engine/Source/Network/Server/ServerBroadcaster.cpp:36-54` snapshot, `:142-167` concatenation per coordinate). The protocol caps that batch at `engine::kiMaximumStatusChangesPerCell` 1,024 (`Engine/Source/Network/NetworkProtocol.h:94`), which is also the client decode scratch size and the compression-scratch sizing basis. `ServerBufferedFrames::BufferFrame` treats an over-cap batch as a simulation bug: `DEBUG_BREAK()`, an error log, and the payload dropped while the frame is still buffered with its CRC, so every subscribed client CRC-mismatches and resyncs (`Engine/Source/Network/Server/ServerBufferedFrames.cpp:48-59`).

`inject_payload` puts no bound on that count. It requires a non-empty `entries` array and validates each entry (`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.cpp:577-592`, `:615-692`), but neither the batch size nor what is already queued is limited, and every queued entry whose tick has come and whose coordinate is active drains into the same tick's input. One batch of more than 1,024 entries at one coordinate, or several smaller batches for the same tick, reaches the debug break. After `FrameEditRows.md` lands, every `inject_payload` entry queues through `engine::ServerSpawnQueue` and `ServerSpawnQueue::Drain` appends all due entries of a coordinate at once, so the hazard is unchanged. Recorded as pre-existing by `FrameEditRows.md` (`## Out of scope`, the per-cell cap bullet), which documents the hazard in the `inject_payload` entry and leaves the fix here.

Unmet condition: an agent command must not be able to drive the server into its "should never happen" over-cap path; the harness validates agent input before queuing it (`Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md` `## Contracts`).

## Design

Author's recommendation, with rationale: refuse the batch at `inject_payload` validation, before anything is queued, when it would leave more than `kiMaximumPendingAgentStatusChanges` entries pending in the server's `ServerSpawnQueue`:

```cpp
// ServerSimulationHarnessRigs.cpp, file scope. Half the per-cell cap: every pending entry may drain into one tick of
// one coordinate, and the other half stays for that tick's natural tick input and arrivals.
constexpr int64_t kiMaximumPendingAgentStatusChanges = engine::kiMaximumStatusChangesPerCell / 2;
```

In `inject_payload`, after the whole batch has been built and validated and before the commit loop that mints global ids and queues: if `std::ssize(built) + gpServerSession->mpSpawnQueue->PendingCount()` exceeds `kiMaximumPendingAgentStatusChanges`, throw `std::runtime_error` with a message naming the limit and the current pending count (`inject_payload would leave {} status changes pending; the limit is {}`), so the command returns `ok:false` with nothing queued and `status.nextGlobalId` unchanged.

Why this shape:

- It bounds the only unbounded producer. The other tick-input writers are natural and per-update (the fleet player spawns, `ProcessUpdatePlayerRequests`, `FleetNavigationController`), and they stay under the existing contract that the simulation never exceeds the cap.
- Counting all pending entries rather than per coordinate and per tick is conservative and needs only the existing `PendingCount()`: entries queued for different ticks or coordinates count against one limit, and 512 entries is far above any scenario in `Projects/BrokenEngineSandbox/Documents/AgentHarness/`.
- It changes no tick input, ordering, wire byte, replay record, or save content: a batch is either queued whole as today or refused whole.
- Not chosen: spreading over-budget entries across later ticks inside `ServerSpawnQueue::Drain`. That would change which tick a spawn enters, a determinism-visible change to server tick input, and would break the `inject_payload` contract that pause, inject, unpause applies every entry in the first unpaused tick.

Documentation: the `inject_payload` entry of `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md` replaces the per-cell cap hazard note `FrameEditRows.md` added with the limit: a batch that would leave more than 512 status changes pending is refused whole; split larger scenes across ticks with `tick` and unpaused steps.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.cpp`
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`

## In scope

- `ServerSimulationHarnessRigs.cpp`: the `kiMaximumPendingAgentStatusChanges` constant and the one pending-count check in the `inject_payload` handler between batch validation and commit.
- `commands-server.md` `inject_payload` entry: the cap hazard note becomes the limit sentence above, and the validation list gains it.

## Out of scope

- `ServerSpawnQueue` (no per-coordinate count, no deferral, no assert), `ServerBroadcaster`, `ServerTransferManager`, `ServerBufferedFrames.cpp` and its debug break, `kiMaximumStatusChangesPerCell`, the batch codec, and the client decode path.
- Natural producers and natural arrival counts; the fleet spawn path.
- Replay, save, and wire formats; `kiProtocolVersion`, `FrameInput::kiVersion`, `Frame::kiVersion`.
- `edit_frame`, `status` fields, and every other harness command.
- Every other Plan under `Documents/Plans/`.

## Risk triggers and invariants

Tier 2 (`.agents/references/risk-tiers.md`): one subsystem's tool behaviour (the game Agent's `inject_payload` handler) tightened by a check at its existing validation boundary; format, tick input, and trust unchanged.

- A refused batch queues nothing and mints no global id; an accepted batch behaves exactly as before.
- No tick input, ordering, CRC, wire, replay, or save change.

## Acceptance criteria

Builds: BrokenEngineSandbox server Debug; zero errors.

Harness (`/agent-harness`, Debug server, `reset`, `pause {"paused":true}`):

| # | Check | Expected observation |
|---|---|---|
| 1 | One `inject_payload` at `[0,0]` with 513 `SpawnBlaster` entries `vecPosition:[5,5]` | `ok:false` with the limit message; `status.pendingAgentStatusChangeCount` 0; `status.nextGlobalId` unchanged; no new `DEBUG_BREAK` or `Assert` line in the server log |
| 2 | One `inject_payload` at `[0,0]` with 512 `SpawnBlaster` entries `vecPosition:[5,5]`, then one tick (`timescale {"faster":false}` five times to 1/32, `pause false`, wait 1.2 s, `pause true`) | `ok:true`, `injected:512`; after the tick `pendingAgentStatusChangeCount` 0, `query_frame [0,0]` blasters +512, no `exceeds cap` line in the server log |
| 3 | While paused, `inject_payload` with 300 `SpawnBlaster` entries at `[0,0]`, then a second with 300 at `[0,0]` | first `ok:true`; second `ok:false` with the limit message; `pendingAgentStatusChangeCount` 300 |
| 4 | Documentation diff read | the `inject_payload` entry states the 512 limit in its validation list and no longer describes an unbounded batch |

## Notes

- Pre-change runs: none. The unedited tree's over-cap path is settled by code read (`ServerBufferedFrames.cpp:51-58`); driving it would hit `DEBUG_BREAK()`.
- Originating residual: `FrameEditRows.md` `## Out of scope` per-cell cap bullet (pre-existing; outside that Plan's boundary).
