<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T18:48:49.885Z","dependsOn":["Documents/Plans/Engine/ClientReconcilerBaseToEngine.md","Documents/Plans/Engine/ReconcileReplayChainToEngine.md"]} -->
# Client Desync Policy Boundary

## Context

ClientDesyncManager is split between generic report/escalation work and game
policy. Projects/BrokenEngineSandbox/Source/Network/Client/ClientDesyncManager.h
and .cpp still
perform game reset sequencing, frame-difference logging, and agent full-state
fixture/debug policy at the regions identified by the investigation (:44 and
:128-166). Modal text now lives on engine::GameBase, but this does not make
the whole manager engine-owned.

The unresolved D2 gap is a boundary that lets the engine report and escalate
desyncs while leaving game reset sequencing and game/agent diagnostics in the
game. The user-approved conversion allows the alternatives to remain explicit.

## Design

Split the manager into an engine-owned report/escalation core and a
game-owned policy surface. The implementation must choose either required
direct game functions for reset/frame-difference/fixture operations or a
single game-owned state record passed to the engine. Do not add a new player
message hook, virtual registration, or callback registry. Keep
Desynced from server as engine-authored English text until the existing
localization Plan establishes the language contract.

## Critical files

- Projects/BrokenEngineSandbox/Source/Network/Client/ClientDesyncManager.h
- Projects/BrokenEngineSandbox/Source/Network/Client/ClientDesyncManager.cpp
- Engine/Source/GameBase.h
- Projects/BrokenEngineSandbox/Source/Network/Client/ReconcileReplay.cpp
- Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsClient.cpp
- Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.h

## In scope

- Generic desync detection, reporting, escalation, and engine modal-state
  writes.
- The required game seam for reset sequencing, frame-difference logging, and
  agent full-state fixture/debug behavior.
- Existing correlation, recovery, and log ordering.
- Any direct references needed to retain the completed historical prerequisite
  behavior without moving game policy.

## Out of scope

- CRC/ring/replay tick ownership and transfer ordering.
- Per-cell reconciler dispatch and focus/visual-error policy.
- Client poll timing and clock-snap recovery.
- Moving the game fixture, scene, desync, or coordinate agent commands into
  the engine; the approved ClientAgentCommandsToEngine Plan owns its boundary.
- Localization table extraction beyond the existing EngineGameLocalizationSplit
  Plan.

## Risk tier and invariants

Expected Change Workflow Tier 3: desync recovery, replay/CRC escalation,
agent diagnostics, and client/server state replacement interact. Preserve
recovery thresholds, modal text timing, frame-difference and debug request
correlation, agent fixture behavior, log levels, and no-new-allocation rules.
No engine source may name or require Player semantics.

## Coordination

Metadata depends on Documents/Plans/Engine/ReconcileReplayChainToEngine.md and
Documents/Plans/Engine/ClientReconcilerBaseToEngine.md. Those Plans provide
the replay and dispatch seams consumed here.
`Documents/Plans/Engine/NetworkProfileOwnershipToEngine.md` depends on this Plan,
`Documents/Plans/Engine/ClientPollTimingRecovery.md`, and
`Documents/Plans/Engine/WorldGridStateOwnershipToEngine.md` and must read
state through the resulting generic boundary rather than duplicating manager
internals. This Plan remains independent of Network profile ownership; its
implementation must not consume or preempt that downstream extraction.

## Acceptance criteria

- The current desync probe and recovery scenario reports the same tick,
  coordinate, modal state, escalation, and debug-frame behavior.
- Game reset and full-state fixture policy remain callable for a game that has
  no Player collection.
- No new player-message hook, registration storage, virtual dispatch, or
  localization dependency is introduced.
- Client and server compile; existing desync/replay logs contain no new errors
  or duplicated recovery.

## Notes

This Plan preserves the investigation's explicit rejection of a whole-class
desync move. The implementation record must identify which of the two bounded
policy shapes was selected.
