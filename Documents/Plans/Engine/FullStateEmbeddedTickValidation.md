<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T01:59:39.226Z","dependsOn":[]} -->
# Reject full states whose embedded tick disagrees with the envelope

## Context

Final survivor `S011-C007` is a retained HIGH client-network finding. `ServerCoordFullState` decodes an embedded frame tick and independently receives the envelope `message.iTick`, then queues and activates the slot using the envelope tick without comparing them. `ApplyReceivedFullStates` stores confirmed/ACK state from the envelope while retaining the embedded tick; pending injection later asserts equality outside the packet catch (`Engine/Source/Network/Client/ClientReceive.cpp:248-267`; `ClientSessionRuntime.cpp:274-334`; `ReconcileReplay.cpp:21-23`).

The final disposition is in `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-014.md` under `S011-C007 — FINAL: RETAIN_HIGH`; the source report is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-011.md:173` and the consolidated selector is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:212`. The target remained at baseline `a20acf1e31a24a0f61ae638e8976602b49655788`.

## Design

The author's recommendation is to compare the envelope tick with the decoded `FrameInterpolate::iTick` immediately after decompression and before placeholder clearing, queueing, ACK-floor mutation, or slot activation. Reject the whole full state through the existing corrupt-packet/recovery channel; preserve matching-tick hydration, resync, ring, and ACK behavior.

## Critical files

- `Engine/Source/Network/Client/ClientReceive.cpp:248-274` — full-state decode and queue/slot mutation.
- `Engine/Source/Network/Client/ClientSessionRuntime.cpp:274-334` — generic full-state adoption.
- `Engine/Source/Network/Client/ReconcileReplay.cpp:21-23` — pending tick invariant.
- `Engine/Source/Network/Client/AGENTS.md` and `Engine/Source/Network/AGENTS.md` — full-state and hostile-input contracts.

## In scope

- Equality validation between full-state envelope and embedded frame ticks before any receive/adoption mutation.
- Existing malformed-packet recovery and matching full-state hydration/resync behavior.

## Out of scope

- Tick representation/version, packet layout, ACK algorithm, ring sizing, frame hydration, or replay interpolation.
- Timestamp/epoch policy unrelated to this cross-field identity, or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: opaque network full-state fields control ACK, reconciliation-ring, render, and CRC timelines across receive and session-runtime owners; deterministic/network integration is exposed.

Preserve these invariants:

- One accepted full state has one authoritative tick across envelope, embedded frame, ACK floor, ring, hydration, and reconciliation.
- A mismatched state is rejected before slot/frame publication and cannot throw later from reconciliation.
- Matching full states retain current wire compatibility, hydration order, resync behavior, and CRC checks.

## Acceptance criteria

- Initial and active-slot full-state packets with envelope tick `N` and embedded tick `M != N` are rejected before queue/slot/ACK mutation and follow the existing recovery path.
- Matching ticks still hydrate, activate/resync, and reconcile exactly as before.
- Client `Debug|x64` builds pass through `/compile`; a malformed full-state scenario produces no later tick assertion.

## Notes

Origin: `S011-C007`; disposition selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-014.md` (`S011-C007 — FINAL: RETAIN_HIGH`), source selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-011.md:173`, and consolidated selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:212`. No exact existing Plan was found. No source fix or build was performed during routing.
