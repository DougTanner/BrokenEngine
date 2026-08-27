<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:52:34.695Z","dependsOn":[]} -->
# Rearm auto-connect after an accepted client drops

## Context

The retained survivor `CAI/shard-0041/001` identifies a screen-local state
machine that never returns from success. `MainMenuScreen::Render` sets the
function-static `seAutoConnectState` to `kSucceeded` on an accepted model at
`Engine/Source/Ui/Screens/MainMenuScreen.cpp:27-40`; its only absent-client
reset handles `kAttempted` at `:47-50`. The auto-connect predicate requires
`kReady` at `:95-99`. A real connection loss returns to the main menu after
`ClientSessionRuntime::Disconnect` destroys the client
(`Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp:199-207`;
`Engine/Source/Network/Client/ClientSessionRuntime.cpp:136-148`), but leaves
the static state at `kSucceeded`.

The direct evidence is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0041.md:65`
and consolidated selector `Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:1029`. Frozen/live source
identity matches baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`; this
routing session has not changed source. The accepted-then-dropped path is
ordinary client lifecycle behavior and is distinct from pre-accept failure,
which already rearms.

## Design

The author's recommendation is to reset the screen-local auto-connect state to
`kReady` when a previously succeeded client is absent on a main-menu pass (or
at the explicit connection-loss transition). Preserve `kSucceeded` as the
no-duplicate-attempt state while the accepted client remains present, and keep
the existing `kAttempted` failure rearm and model rereads.

## Critical files

- `Engine/Source/Ui/Screens/MainMenuScreen.cpp:27-50,88-102` — auto-connect
  state machine and predicate.
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp:199-207`
  and `Engine/Source/Network/Client/ClientSessionRuntime.cpp:136-148` —
  connection-loss transition.
- `Projects/BrokenEngineSandbox/Source/Game.cpp:490-500,527-554` — model
  presence and main-menu frame reset.
- `Engine/Source/Ui/Screens/AGENTS.md` — succeeded/no-retry and dropped-client
  rearm contract.

## In scope

- `AutoConnectState` reset after an accepted client disappears.
- The ordinary connection-loss/main-menu/discovery/auto-connect sequence.
- Existing pre-accept failure, successful connection, and manual Local Server
  actions.

## Out of scope

- Discovery transport, connect retry timing, connection-loss modal text, or
  server behavior.
- Main-menu layout, feature visibility, and unrelated UI state.
- A new persistent auto-connect setting or cross-screen state holder.

## Risk tier and invariants

Expected Change Workflow Tier 2. Trigger: this is scoped client screen state
and connection lifecycle behavior with no protocol, serialization,
deterministic simulation, or CRC impact.

Preserve these invariants:

- A live accepted client suppresses duplicate automatic attempts.
- After that client is dropped, a newly discovered server is attempted once.
- Pre-accept failures still rearm, and manual connection remains available.

## Acceptance criteria

- With auto-connect enabled, an accepted client that disconnects returns to the
  main menu and automatically reconnects after the next discovery.
- A healthy accepted connection is not repeatedly reconnected while it remains
  present.
- Pre-accept failure and manual Local Server paths retain their current
  behavior.
- Client `Debug|x64` builds pass through `/compile`.

## Notes

Origin: `CAI/shard-0041/001`; source selector is the shard line above and the
consolidated selector is `consolidated-index.md:1029`. No source fix or build
was performed during routing.
