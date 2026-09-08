<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-09T00:12:37.586Z","dependsOn":[]} -->
# Extract server and replay agent fixtures from production owners

## Context

The server and replay fixture extraction deferred from the approved agent-fixture work remains unresolved. `ServerSession` still owns the fixture-only `mReplayTransferFixtures` and `mPendingAgentStatusChanges` maps (`Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.h:58,66`). `ServerBroadcaster` drains and clears the status map and exposes `QueueAgentStatusChange` (`Engine/Source/Network/Server/ServerBroadcaster.cpp:91-108,331-349`), while `ServerTransferManager` drains and clears the replay-transfer map and exposes `QueueReplayTransferFixture` (`Engine/Source/Network/Server/ServerTransferManager.cpp:300-314,380-397`). The game command handler still prepares, swaps, clears, and queues those fixture values (`Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp:211,633,827-866,907`), and save/load clears the replay-transfer map directly (`Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp:40`).

`Replay` also still owns fixture-only capture, pause, retained-frame drop, and persistence-failure types, state, and methods (`Engine/Source/File/Replay.h:16-66,102-106`). Their behavior is embedded through replay reset, capture, adoption, persistence, writer-input, and playback paths (`Engine/Source/File/Replay.cpp:295-357,398-486,508-1121,1214`), with the fixture's one-tick cap read directly by `GameBase` (`Engine/Source/GameBase.cpp:339`). The game command handler reads and mutates that production fixture state (`Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp:212,247-353,531,639-640`).

This work was explicitly deferred after the client fixture extraction. It is a refactor of existing debug verification behavior: no new command or runtime capability is added.

## Design

Author's recommendation: create focused `ServerSimulationFixtures` modules under `Projects/BrokenEngineSandbox/Source/Agent/Commands/` and `ReplayFixtures` modules under `Engine/Source/Agent/Commands/`. Game server fixture modules own status/spawn and replay-transfer maps keyed by the current `ServerSession*`, initialized lazily by the first command for that exact session. The engine replay module owns capture, pause, retained-frame drop, and persistence-failure state keyed by the current `Replay*`.

Keep game-specific scenario constants, JSON assembly, and action sequencing in game command modules. Existing top-level dispatchers call narrow command-module declarations. Production objects expose only the ordinary members the modules currently need as direct public members under the existing client/server/debug guards; do not replace removed fixture forwarding APIs with getters or another wrapper layer.

Preserve status-change consumption at the existing broadcaster phase and replay-transfer consumption at the existing transfer phase. Call narrow module reset hooks from broadcaster load/reset, transfer reset, replay invalidation/start/stop, and command completion. Status changes retain their queue-until-valid behavior, replay transfers retain accepted-queue and capture ordering, and save/load uses the module reset rather than clearing production-owned fixture storage.

Attach `ReplayFixtures` after `gpReplay` is published. Invoke its reset from every current `ResetStreams`, recording invalidation or abort, pending-start cancellation, successful start/stop, and playback-adoption path. Preserve capture restoration during adoption. `Replay::~Replay` detaches before clearing `gpReplay`. `ServerSession::~ServerSession` detaches the game fixture bindings before `mpRuntime.reset()` and clears every session-keyed map. The existing Agent deferred slot must be cleared before either owning session is destroyed so captured fixture state completes RAII cleanup while its dependencies remain live.

Retain only narrow debug hook declarations and calls outside `Source/Agent/Commands/` where exact broadcaster, transfer, save/load, Replay, or `GameBase` timing and lifecycle boundaries require them. Inactive hooks preserve ordinary behavior and add no allocation to existing hot paths.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.{h,cpp}` — remove fixture maps and detach module state before runtime destruction.
- `Engine/Source/Network/Server/ServerBroadcaster.{h,cpp}` and `ServerTransferManager.{h,cpp}` — replace fixture forwarding/storage access with narrow timing and reset hooks at the existing drain phases.
- `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp` — clear server fixture state through its command-owned module at the existing load boundary.
- `Engine/Source/File/Replay.{h,cpp}` and `Engine/Source/GameBase.cpp` — remove fixture payload/state/method ownership while retaining exact replay lifecycle and one-tick timing hooks.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp` and focused modules under `Projects/BrokenEngineSandbox/Source/Agent/Commands/` — own server scenario state, handlers, JSON, and lifecycle entry points.
- Focused modules under `Engine/Source/Agent/Commands/` — own replay fixture types, state, controls, and behavior.
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox*.vcxproj*` plus affected Agent/network/File documentation — project membership and ownership text.

## In scope

- Move server status/spawn and replay-transfer fixture maps, snapshots, result wrappers, orchestration, and forwarding APIs from production classes into game Agent command modules bound to `ServerSession*`.
- Move Replay capture, pause, retained-frame drop, persistence-failure types/state/behavior, and result snapshots into engine Agent command modules bound to `Replay*`.
- Add only the narrow debug phase, reset, attachment, and detachment hooks required at current broadcaster, transfer, save/load, replay, `GameBase`, session-destruction, and deferred-command boundaries.
- Expose only directly used production members with ordinary production names and appropriate guards; update includes, project/filter membership, and affected ownership documentation.
- Preserve every existing server/replay fixture command name, schema, response, failure, and documented scenario sequence.

## Out of scope

- New fixture managers, registries, frameworks, general services, worker threads, configuration, aliases, compatibility paths, or unit tests.
- Production network, replay, save/load, simulation, command-transport, profile, query, or UI behavior beyond narrow visibility and essential debug timing/lifecycle hooks.
- Wire or packet bytes, save/replay formats, serialization, deterministic Frame/CRC state, replay generation rules, fixed-tick phase order, or fatal-process behavior.
- The client/audio fixtures and the already isolated collection, registry, packet, pre-handshake, and crash-report fixtures.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: the extraction crosses independently owned game Agent, server network, engine Replay, save/load, and fixed-tick subsystems and relocates state observed at exact replay and transfer phases.

- Fixture state remains outside deterministic Frame/CRC state and every persisted or wire layout.
- Status changes remain queued until a valid broadcaster phase; replay transfers retain accepted-queue, capture, application, and reset ordering.
- Replay fixture controls reset on the same invalidation, abort, pending-start cancellation, successful start/stop, and playback-adoption boundaries, including capture restoration during adoption.
- Session-keyed and Replay-keyed bindings detach before their owners die; deferred state releases while dependencies remain live.
- The one-tick pause cap at the writer-input boundary remains exact.
- Inactive hooks do not alter ordinary runtime decisions, ordering, or allocation behavior.

## Acceptance criteria

- Repository inventory finds no server/replay fixture payload type, DTO, snapshot, result wrapper, map, control state, or forwarding method in `ServerSession`, `ServerBroadcaster`, `ServerTransferManager`, `GameSaveLoad`, `Replay`, or `GameBase`; only necessary guarded hook declarations/calls remain outside Agent Commands.
- `status_change_fixture` and `inject_status_changes` preserve whole-batch validation, minted IDs, paused/deferred behavior, rejection, activation-arm behavior, next-valid-tick consumption, query-visible results, and no confirmed desync using `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:48-51` and `verification.md:9-15`.
- Replay transfer/capture/pause scenarios preserve event ticks, exact writer-input pause count, transfer counts/state, clean record/play loop, and pending-start cancel/failure behavior using `commands-server.md:38-43` and `replay.md:32-38`.
- Replay persistence/drop scenarios preserve optional full-frame failure, live-transfer preservation on capture failure, generation targeting, manifest-last publication, and playback abort/reset behavior using `replay.md:58,64-65,69,75,95`.
- With failure fixtures inactive, ordinary reset, spawn/injection, save/load, and record/play complete and logs contain no fixture injection, persistence-failure, CRC/read, or confirmed-desync signal.
- Source inspection proves every reset/start/stop/abort/adoption/detach hook and the one-tick cap retain their existing semantic point, with no wire/save/replay/CRC or production algorithm change.
- `/compile` passes `BrokenEngineSandbox` and `BrokenEngineSandboxServer` in `Debug|x64` and `Release|x64`; the full Tier 3 static, code, comment, coherence, style, project-membership, documentation, adversarial, and acceptance routes pass without unit tests.

## Coordination

`Documents/Plans/Engine/ReplayResetCancelsPendingRecord.md` and `Documents/Plans/Engine/ReplayRecordPlaybackExclusion.md` change replay lifecycle behavior in the same command, `GameBase`, and `Replay` regions. Neither is a prerequisite: whichever lands second must preserve the other's reset/admission behavior while adapting the final ownership boundary. `Documents/Plans/Game/AgentCoordinateIntegralValidation.md` changes the shared coordinate parser used by the replay-transfer and status-injection handlers; whichever lands second must keep one validated parser before any fixture lookup, ID allocation, or queue mutation.

## Notes

This Plan preserves the approved Stage 3 boundary and lifecycle decisions as a standalone deferred work unit. The later isolated-fixture normalization and final inventory depend on this extraction being complete.
