# Cross-channel generation for server-load reset

Status: Open investigation; no implementation decision has been made.

Area: Engine

Record type: Non-executable reference material. This document intentionally has
no scheduler metadata and is not a Plan input.

Audit source: `CPT/shard-0036/004` in the frozen C++ Plan Trace Audit.

Frozen audit commit: `80896f33661aaab99cf180a96db54600099be652`

## Finding under investigation

`ClientSessionRuntime::PollAndDrain` processes transport events, drains the
control-channel load notification, calls `ResetForServerLoad`, and then
applies static data, full states, and updates
(`Engine/Source/Network/Client/ClientSessionRuntime.cpp:164-215`).
`ResetForServerLoad` clears received full/static/update buffers, resets all
slots, and purges delayed coordinate-channel packets while deliberately
leaving the control channel queued
(`ClientSessionRuntime.cpp:179-203`).

The server broadcasts `kServerLoadNotification` on control reliable while
subscription accept and static/full responses can use a slot reliable channel
(`Engine/Source/Network/Server/ServerSend.cpp:273-290`;
`Engine/Source/Network/Server/ServerReceive.cpp:343-410`;
`Engine/Source/Network/Server/ServerSessionRuntime.cpp:183-210`). Network
simulation tracks release timing per channel rather than globally
(`Engine/Source/Network/NetworkSimulation.h:82-90,180-187`). A valid slot
response can therefore arrive into receive buffers before the control load
notification is observed and then be erased by the reset, leaving a local slot
waiting for a response the server has already sent.

The durable source trace establishes the cross-channel ordering gap; the
ignored shard report is supplementary provenance.

## Controlling contract and invariant

`Projects/BrokenEngineSandbox/Source/Network/Client/AGENTS.md:24` requires load
reset to complete before post-load data is accepted. The engine client
authority at `Engine/Source/Network/Client/AGENTS.md:30,46` requires a fresh
timing epoch, coordinate-channel purge, and deliberately retained control
channel. `Documents/Architecture/Network.md:10-12,23-42` defines channel,
ACK/epoch, load, and replay sequencing. The unresolved invariant is that a
post-load static/full response is either associated with the committed reset
generation or retained until that generation is ready; valid responses must
not be discarded solely because control and slot channels released in a
different order.

## Boundary and impact

The open boundary is the handoff between the control-channel load notification,
slot-channel accept/static/full responses, receive buffers, and
`ResetForServerLoad`. It includes initial resubscription and slot epochs but
does not decide unrelated subscription cancellation or generic packet bounds.

If a valid post-load response is erased during reset, the client can remain in
`kWaitingFullState` while the server believes the slot is active. Recovery then
depends on timeout or slot churn instead of the documented load boundary.

## Open choices

These alternatives are recorded for a future decision; none is selected here.

1. **Cross-channel load generation.** Carry or associate a load generation
   with control and slot-channel responses and accept only responses belonging
   to the committed client reset generation. Define how the generation is
   published, whether a protocol or frame version changes, and how existing
   slot epochs interact with it.
2. **Retention barrier.** Keep post-load static/full responses and related
   slot data until `ResetForServerLoad` has committed, then apply or classify
   them under one ordered barrier. Define buffer ownership, memory bounds,
   duplicate handling, and the initial resubscription rule without allowing
   pre-load data through.

The decision must cover responses already delivered into `mReceived*` buffers,
responses still delayed in simulation, control-channel ordering, slot reuse,
and a valid subscribe crossing the server load event. No choice should rely
on same-channel ordering to solve a cross-channel race.

## Decisive questions and acceptance evidence

- Is a generation field permitted in the current protocol/stream, or must the
  boundary remain local to the client receive buffers and reset transaction?
- For a subscribe sent before the server load, what exact response is valid
  after the load, and when may the client commit its slot?
- Can a focused channel-delay scenario deliver static/full before control,
  run reset, and prove that the valid response is neither lost nor applied to
  pre-load state?
- Do ordinary same-channel reliable order, slot epochs, delayed-packet purge,
  and initial resubscription retain their current behavior?
- Are memory bounds and duplicate/late response rules explicit for both
  retained and generation-tagged alternatives?

The eventual executable Plan is expected Tier 3 because the decision crosses
client/server transport channels, protocol or receive-state identity, and
load/reconciliation ordering. Until the choice is made, no source or wire
change is authorized.

## Provenance

- Frozen source candidate: `CPT/shard-0036/004`.
- Frozen consolidated index: `Temp/CppPlanTraceAudit/80896f33661aaab99cf180a96db54600099be652/consolidated-index.md`.
- No exact existing Plan or Investigation owns this cross-channel load-reset
  barrier; subscription cancellation and normal network simulation records are
  separate boundaries.
- No source, protocol, or scheduler change is part of this investigation.
