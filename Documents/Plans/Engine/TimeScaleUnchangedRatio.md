<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-20T23:51:24.863Z","dependsOn":[]} -->
# TimeStep::SetTimeScale Ignores an Unchanged Ratio

## Context

`TimeStep::SetTimeScale` (`Engine/Source/Frame/TimeStep.cpp:82-87`) assigns
`miTimeMultiply` and `miTimeDivide` and sets `mbTimeScaleChanged = true` with no
comparison. Its only two callers each guard the call themselves, calling it
only when `iMultiply != miTimeMultiply || iDivide != miTimeDivide`:

- `Client::ServerTimespeedUpdate`
  (`Engine/Source/Network/Client/Client.cpp:829-835`), with a two-line comment
  warning that an unchanged ratio must not call it or the echo of a SPACE pause
  would erase the PAUSED debug note.
- `Engine/Source/Network/Server/ServerSessionRuntime.cpp:51-54`, which writes a
  1:1 ratio only when the ratio differs.

The only other code that writes `miTimeMultiply` or `miTimeDivide` is
`TimeStep::DecreaseTimeScale` and `IncreaseTimeScale`
(`Engine/Source/Frame/TimeStep.cpp:89-121`), which set the flag only when they
change the ratio. The flag's consumers are `GameBase.cpp:278-280` (client debug
note) and `Server.cpp:950-954` (`kServerTimespeedUpdate` broadcast). Through
these `TimeStep` writers they see the flag only on a real ratio change, both
today and after this change; the one other writer of the flag,
`Engine/Source/Network/Client/ClientSessionRuntime.cpp:478`, sets it on the
first connection accept unless the server is paused at 1:1, and this Plan
leaves it alone. The rule "flag only a real change" lives in each caller
instead of in the function that owns the flag, so a future caller can
reintroduce the PAUSED-note bug.

## Design

Move the comparison into `SetTimeScale`: return before any assignment when
`iMultiply == miTimeMultiply && iDivide == miTimeDivide`. Both callers then call
it unconditionally. One comparison replaces two, and the caller-side trap
comment goes away. The author recommends a one-line comment on the
`SetTimeScale` declaration (`Engine/Source/Frame/TimeStep.h:51`) stating that an
unchanged ratio leaves `mbTimeScaleChanged` alone, as local rationale.

The ratio scales wall time to simulation time only (`WallToSimulation`,
`TickRealtime`); it is not frame state, so no CRC, save, replay, or wire layout
changes, and `kServerTimespeedUpdate` traffic is unchanged because the server
caller already skipped the call when equal.

## Critical files

- `Engine/Source/Frame/TimeStep.cpp`
- `Engine/Source/Frame/TimeStep.h`
- `Engine/Source/Network/Client/Client.cpp`
- `Engine/Source/Network/Server/ServerSessionRuntime.cpp`

## In scope

- `TimeStep::SetTimeScale`: the early return on an unchanged ratio, and its
  declaration comment in `TimeStep.h`.
- `Client::ServerTimespeedUpdate`: drop the ratio `if` and its two-line
  rationale comment; call `SetTimeScale(message.iMultiply, message.iDivide)`
  directly (the local `rTimeStep` may be inlined).
- The `ServerSessionRuntime.cpp` 1:1 reset: drop the ratio `if` and call
  `SetTimeScale(1, 1)` directly.

## Out of scope

- `TimeStep::DecreaseTimeScale` and `IncreaseTimeScale`, which always change
  the ratio.
- The direct `mbTimeScaleChanged` write in `ClientSessionRuntime.cpp`.
- The flag consumers in `GameBase.cpp` and `Server.cpp`.
- Any AGENTS.md text: none names `SetTimeScale`, and
  `Engine/Source/Network/Server/AGENTS.md` `## Session Invariants` ("writes 1:1
  timescale only ... when the ratio differs") stays true.

## Acceptance criteria

- Clean `/compile` builds of the BrokenEngineSandbox client and server.
- No harness check: every caller already guarded the call, so no reachable
  path changes behavior.

## Notes

- Change Workflow tier: Tier 1. Trigger: local behavior-preserving change; no
  signature, wire, CRC, or caller-observable behavior changes.
