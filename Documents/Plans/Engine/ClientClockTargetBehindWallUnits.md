<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-10T17:51:24.609Z","dependsOn":[]} -->
# Convert the client clock target-behind budget with the scaled wall tick period

## Context

The client clock servo converts a wall-clock buffering budget into a sim-tick
offset by dividing it by the *unscaled* sim tick period, so the conversion is
only correct while the time scale is 1:1.

Steady state, `ClientSessionRuntime::EvaluateClock`
(`Engine/Source/Network/Client/ClientSessionRuntime.cpp:643-645`):

```
int64_t iJitterMicroseconds = mpClient->mSmoothedJitterUs.Get();
int64_t iTickMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(game::NetworkSessionContract::kTickDuration).count();
int64_t iComputedTargetBehind = (3 * iJitterMicroseconds + kiJitterSafetyUs + iTickMicroseconds - 1) / iTickMicroseconds;
```

Initial offset, `ClientSessionRuntime::ApplyReceivedFullStates`
(`Engine/Source/Network/Client/ClientSessionRuntime.cpp:330-331`):

```
static constexpr int64_t kiTickTimeMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(engine::kTickNs).count();
static constexpr int64_t kiInitialTargetBehind = (engine::kiJitterSafetyUs + kiTickTimeMicroseconds - 1) / kiTickTimeMicroseconds;
```

Both numerators are wall-clock microseconds. `kiJitterSafetyUs`
(`Engine/Source/Network/NetworkProtocol.h:74`) is documented by its unit comment
(`Engine/Source/Network/NetworkProtocol.h:70-73`) as "specified in wall-clock
microseconds", and `mSmoothedJitterUs` is a wall-clock interarrival
deviation measured from `steady_clock` in
`Engine/Source/Network/Client/ClientReceive.cpp:387-401`. Both denominators are
the fixed sim tick period (`kTickNs` / `NetworkSessionContract::kTickDuration`,
`Engine/Source/Frame/TimeStep.h:6-10`). One sim tick actually occupies
`SimToWall(kTickNs)` = `kTickNs * miTimeDivide / miTimeMultiply` of wall time
(`Engine/Source/Frame/TimeStep.h:41-44`), so the tick count is wrong by exactly
the time-scale ratio.

The mix is reachable and not deliberate. The client time scale follows the
server's: the ungated agent `timescale` command drives a
`kServerTimespeedUpdate`, which `Client::ServerTimespeedUpdate` applies through
`TimeStep::SetTimeScale` at
`Engine/Source/Network/Client/ClientReceive.cpp:689`. The *producer* of the
jitter measurement was already made scale-aware — `ClientReceive.cpp:397`
comments "Server broadcast cadence is wall-scaled by the debug timescale; expect
the scaled wall interval, not the fixed sim tick period" and compares against
`NetworkTimeState::iExpectedUpdateIntervalMicroseconds`, which is
`SimToWall(kTickDuration)`. Its consumer in `EvaluateClock` was not.

Consequences at a non-1:1 scale, with the 3.5-tick zero-jitter budget the
protocol constant documents:

- Fast-forward (`miTimeMultiply` 4): a tick occupies ~7.8 ms of wall time, so
  the 109.375 ms safety budget is ~14 ticks, but the code still computes 4.
  `miCurrentTargetBehind` is ~4x too small, so the servo target and the derived
  sim ceiling in `GameBase::ClientUpdate` sit far too close to
  `miLatestServerTick`; ordinary arrival jitter then repeatedly starves the sim
  against the ceiling instead of being absorbed.
- Slow motion (`miTimeDivide` 4): the budget is ~1 tick of wall time but the
  code computes 4, buffering ~500 ms of wall latency that nothing needs.

The two sites share one root cause and must move together: the initial offset
exists specifically to start the sim at the steady-state target
(`ClientSessionRuntime.cpp:322-327`), so fixing only one reintroduces the
startup freeze that comment describes.

The error is not absorbed by clock correction. `EvaluateClock` steers
`miClockError` toward `miLatestServerTick - miCurrentTargetBehind`; a wrong
`miCurrentTargetBehind` moves the target itself, so the servo converges
faithfully onto the wrong offset.

Found during `/update-affected-code` propagation of two scale-awareness fixes
made in one session and landed together: `ServerSessionRuntime::WaitForTick` by
the server tick-wait remainder Plan, and `GameBase::HandleDeferredSwapchain` by
the client minimized-throttle Plan. These two clock sites are pre-existing and
were outside both of those boundaries.

## Design

Replace the fixed sim-tick divisor at both sites with the wall time one tick
actually occupies at the current scale: `game::gpGame->mTimeStep.SimToWall(engine::kTickNs)`
(`Engine/Source/Frame/TimeStep.h:41-44`), converted to microseconds so it
matches the wall-clock numerators. This is the same conversion the two
scale-awareness fixes in the preceding landed change used, so the runtime states
one tick's wall duration one way, and it depends on no published per-update
value.

Both `kiInitialTargetBehind` and `iComputedTargetBehind` therefore stop being
derived from a `constexpr` tick period and become runtime values from that
divisor, keeping the existing ceiling-division rounding so the documented
off-boundary 3.5-tick budget still rounds up. No not-positive guard: the client
scale mirrors the server's, whose `miTimeMultiply`/`miTimeDivide` start at 1 and
are only halved or doubled by `TimeStep::IncreaseTimeScale`/`DecreaseTimeScale`,
so the divisor stays positive; at the 1:1 default it equals the current fixed
period exactly, which is what keeps behavior unchanged there.

Exposure review: client-only code under `BT_CLIENT`, outside the PostRender CRC
and outside replay determinism — the clock servo steers `miTickCounter`
alignment against the server, not simulation math. No wire format, no
`kuiProtocolVersion` bump, no serialization or `.pack`/`kiVersion` change, no
new allocation, no threading or affinity change, no shader change.

Change Workflow tier: Tier 2 — scoped behavior in one subsystem (the client
network session's clock servo), with no determinism/CRC, wire, serialization,
save/replay, threading, or trust-boundary surface touched.

## Critical files

- `Engine/Source/Network/Client/ClientSessionRuntime.cpp` — both conversion
  sites (`:330-331`, `:643-645`).
- `Engine/Source/Network/NetworkProtocol.h` — `kiJitterSafetyUs` and its
  wall-clock unit comment, `NetworkTimeState::iExpectedUpdateIntervalMicroseconds`.
- `Engine/Source/Frame/TimeStep.h` — `kTickNs`, `WallToSim`/`SimToWall`.
- `Engine/Source/Network/Client/ClientReceive.cpp` — the scale-aware jitter
  producer and `ServerTimespeedUpdate`.

## In scope

- `ClientSessionRuntime::EvaluateClock`: the `iTickMicroseconds` divisor and the
  `iComputedTargetBehind` expression at
  `Engine/Source/Network/Client/ClientSessionRuntime.cpp:643-645`.
- `ClientSessionRuntime::ApplyReceivedFullStates`: the `kiTickTimeMicroseconds`
  and `kiInitialTargetBehind` constants and the `iAppliedBehind` computation at
  `Engine/Source/Network/Client/ClientSessionRuntime.cpp:328-336`, including the
  `SetCurrentTime` term derived from the same offset.
- Comment updates at those two regions and, if their wording becomes wrong, the
  unit comments on `kiJitterSafetyUs` in
  `Engine/Source/Network/NetworkProtocol.h:70-74`.

## Out of scope

- `Engine/Source/GameBase.cpp` — the `NetworkTimeState` producer is already
  scale-aware; do not change it.
- `Engine/Source/Network/Client/ClientSend.cpp` `SendAck`/`kAckInterval` — a
  deliberately scale-independent wall-cadence ack timer, verified separately as
  not a defect.
- `ServerSessionRuntime::WaitForTick` and `GameBase::HandleDeferredSwapchain` —
  both already scale-aware in the landed change described in `## Context`; do
  not revisit either function here.
- The jitter measurement itself, the `3 *` jitter factor, the value of
  `kiJitterSafetyUs`, `kiSimCeilingSlackTicks`, `kiClockSnapThreshold`, and the
  `kiLowerTargetBehindStreakTicks` hysteresis — retuning any of these is a
  separate change.
- Any wire, protocol version, serialization, or server-side change.
- The `timescale` agent command and its gating.

## Acceptance criteria

The compiled log floor is `keLogLevelDefault = kDebug`, so the `kVerbose`
`ClockSync` line is not usable as evidence. The clock target-behind, offset, and
error are published to the network profile overlay through
`ProfileManagerBase::SetClockCorrection`
(`Engine/Source/Network/Client/ClientSessionRuntime.cpp:704`,
`Engine/Source/Profile/ProfileNetworkScreen.cpp:289-297`), so verify from that
overlay with `/agent-harness`.

1. Client and server connected at the default 1:1 time scale: the network
   profile overlay's clock target value and clock error match the pre-change
   run (zero-jitter target of 4 ticks), i.e. no behavior change at 1:1.
2. After the agent `timescale` command raises the server scale to 4x and the
   client applies the propagated `kServerTimespeedUpdate`, the overlay's clock
   target value rises by roughly the scale factor over its 1:1 value, instead of
   staying at the 1:1 value as it does today.
3. In that 4x steady state the overlay's clock error settles near zero and does
   not persistently sit at or beyond the aggressive-correction magnitude of 4,
   and the client log shows no repeated clock snap: the `kWarning`
   `ClientSessionRuntime::ApplyClockCorrection Clock snap` line
   (`Engine/Source/Network/Client/ClientSessionRuntime.cpp:712`) is emitted
   under the `kDebug` compile floor
   (`Projects/BrokenEngineSandbox/Source/Pch.h:87`), so its absence across that
   steady state is the observable.
4. Client and server both build clean.

## Notes

- The startup path is guarded by `bInitialSetup` and clamped by
  `std::min(kiInitialTargetBehind, iTick)`; a runtime divisor must keep that
  clamp so a fresh post-load server with a small `iTick` still cannot produce a
  negative sim tick.
- `EvaluateClock` raises `miCurrentTargetBehind` by only one tick per call by
  design (lowering the ceiling faster can stall the sim). A large scale change
  therefore ramps the target over several ticks rather than snapping; that is
  the existing mechanism and needs no new code.
