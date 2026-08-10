<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-09T13:58:25.870Z","dependsOn":[]} -->
# Clear Buffered Game Packets On Client Load Reset

## Context

`ClientSessionRuntime::ResetForServerLoad` (`Engine/Source/Network/Client/ClientSessionRuntime.cpp:158-179`)
discards every other applyable pre-load receive output on a server load reset — it calls `ResetClock`,
`ClearSubscriptionState`, `Client::ResetAllSlots`, clears `mpClient->mReceivedFullStates`,
`mpClient->mReceivedStaticData`, and every per-slot `mpClient->mReceivedCoordUpdates` entry, and purges
delayed coordinate-channel packets. It does not clear `mpClient->mReceivedGamePackets`
(`Engine/Source/Network/Client/Client.h:110`).

`Client::Poll` clears the transient receive buffers at entry (`Engine/Source/Network/Client/Client.cpp:162`)
and then services ENet, appending opaque game payloads at
`Engine/Source/Network/Client/Client.cpp:291`. `ClientSessionRuntime::PollAndDrain` consumes the load
notification and resets at `Engine/Source/Network/Client/ClientSessionRuntime.cpp:226-230`, and then drains
game packets first at `Engine/Source/Network/Client/ClientSessionRuntime.cpp:231`
(`mrSession.ProcessReceivedGamePackets()`), before static data, full states, and updates. A game packet that
arrived in the same `Poll` before the load notification is therefore still in `mReceivedGamePackets` when the
reset runs and is delivered to the game layer after `OnServerLoad`, applying pre-load game state on top of the
freshly reset session.

Found as an out-of-scope residual of the session that fixed the load/reset coordinate-channel race; that
change deliberately scoped itself to coordinate-channel state and did not touch the opaque game-packet
buffer, so this remains unfixed in the current tree.

## Design

Clear `mpClient->mReceivedGamePackets` inside `ClientSessionRuntime::ResetForServerLoad`, alongside the
existing `mReceivedFullStates` and `mReceivedStaticData` clears, so every applyable pre-load receive output is
dropped by the same reset. No new state, ordering change, or drain-order change: the existing
`ProcessReceivedGamePackets` call in `PollAndDrain` stays where it is and simply finds the buffer empty when a
load reset happened in the same poll.

Game packets are opaque engine-side payloads forwarded to the game session, so no wire layout, packet type,
protocol version, or server behavior changes.

## Critical files

- `Engine/Source/Network/Client/ClientSessionRuntime.cpp` — `ResetForServerLoad`; the only function changed.
- `Engine/Source/Network/Client/Client.h` — `mReceivedGamePackets` declaration; read-only context.
- `Engine/Source/Network/Client/ClientSessionRuntime.cpp` `PollAndDrain` — drain order evidence; read-only.
- `Engine/Source/Network/Client/AGENTS.md` — `Receive Paths` section, only if the reset contract stated there
  needs the game-packet buffer named.

## In scope

- The body of `ClientSessionRuntime::ResetForServerLoad`: clear `mpClient->mReceivedGamePackets`.
- The `Receive Paths` section of `Engine/Source/Network/Client/AGENTS.md`, only if it must name this buffer to
  stay accurate.

## Out of scope

- Any other `ClientSessionRuntime` function, including `PollAndDrain` drain order, `ResetForConnect`, and
  `Disconnect`.
- `Client::Poll`, `Client::ResetAllSlots`, slot/epoch/ACK state, and delayed-packet purge behavior.
- Any server source, packet layout, `PacketType`, protocol version, or `Frame::kiVersion` change.
- The game session's own reset or hydration policy in
  `Projects/BrokenEngineSandbox/Source/Network/Client/`.

## Risk tier and invariants

Change Workflow Tier 2 — scoped client transport behavior in one subsystem. It does not touch determinism/CRC
math, wire layout, serialization format, save/replay compatibility, threading, or trust boundaries; the
receive path is already main-thread-only.

Invariant to preserve: after a server load reset, no pre-load receive output — game packets, static data, full
states, or coordinate updates — reaches the game layer in the same poll or a later one.

## Acceptance criteria

- Structural inspection shows `ResetForServerLoad` clears `mReceivedGamePackets` and that no drain in
  `PollAndDrain` can observe a pre-load game packet after `OnServerLoad`.
- Both Debug x64 targets compile.

## Coordination

`Documents/Plans/Network/ClientLoadClockJitterReset.md` also edits
`ClientSessionRuntime::ResetForServerLoad`. The plans are order-independent: whichever lands second must
preserve both this plan's `mReceivedGamePackets.clear()` operation and the clock plan's jitter/arrival-epoch
reset, locating each operation by member and symbol rather than line number. No directional dependency is
required.
