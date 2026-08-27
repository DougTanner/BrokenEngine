<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:52:20.029Z","dependsOn":[]} -->
# Bound delayed reliable-packet admission

## Context

The retained survivor `CAI/shard-0036/001` identifies an admission-order gap
when network simulation is enabled. `Server::DispatchIncoming` copies every
received event into `NetworkSimulation::mDelayedPackets` before
`Server::Receive` applies its per-client packet/byte contract
(`Engine/Source/Network/Server/Server.cpp:98-141,205-233`). The reliable path
in `Engine/Source/Network/NetworkSimulation.h:137-187` never drops and has no
entry or byte bound, so a sustained reliable flood can grow deferred storage
before the 256-packet/64 KiB budget is checked.

The direct evidence is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0036.md:64`
and consolidated selector `Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:939`. Frozen/live source
identity matches baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`; no source
was changed during routing. The default game switch is disabled, but the
non-disabled simulation profiles are a supported latency/loss test path and
the reliable-retention contract still applies there.

## Design

The author's recommendation is to apply the existing server packet/byte
admission policy before a deferred copy, with accounting that is bounded for
each peer and each server update. Route over-budget reliable input through the
central contract-violation/disconnect path rather than silently evicting it;
reliable packets that are admitted remain FIFO-retained. Release-time receive
processing must not double-charge a packet already admitted at enqueue.

## Critical files

- `Engine/Source/Network/NetworkSimulation.h:137-187` — deferred packet
  storage and reliable admission.
- `Engine/Source/Network/Server/Server.cpp:98-141,205-233` — event ordering
  and existing packet/byte contract gate.
- `Engine/Source/Network/NetworkProtocol.h:140-149` — packet/byte budget.
- `Engine/Source/Network/AGENTS.md` and
  `Engine/Source/Network/Server/AGENTS.md` — hostile-input and reliable
  retention contracts.
- `Projects/BrokenEngineSandbox/Source/Pch.h:104` — default simulation level
  (reference for verification only).

## In scope

- Reliable-packet admission and bounded deferred storage for non-disabled
  `NetworkSimulationLevel` profiles.
- Per-peer budget/violation accounting and release-time interaction.
- Existing disabled-mode receive behavior and FIFO delivery for admitted
  packets.

## Out of scope

- ENet socket buffer sizing, packet wire formats, or client/server protocol
  versions.
- Delayed ordering during fast-forward, which is a separate survivor.
- Simulation latency/loss profiles or a new reliable-packet drop policy.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: untrusted network bytes enter
main-thread deferred storage before admission, crossing resource, transport,
and violation-accounting boundaries.

Preserve these invariants:

- Deferred reliable storage is bounded by an explicit packet/byte admission
  budget before copying untrusted data.
- Admitted reliable packets are never silently discarded and retain per-channel
  FIFO release order.
- Contract violations use the existing accounting/disconnect path, and the
  disabled simulation path remains unchanged.

## Acceptance criteria

- Under a non-disabled simulation profile, a sustained reliable flood cannot
  grow deferred packet bytes or entries beyond the declared admission budget;
  over-budget input is accounted and disconnected through the existing path.
- Admitted reliable packets are released and processed exactly once in order.
- With simulation disabled, the existing immediate `Server::Receive` budget
  behavior remains unchanged.
- Client and server `Debug|x64` builds pass through `/compile`.

## Notes

Origin: `CAI/shard-0036/001`; source selector is the shard line above and the
consolidated selector is `consolidated-index.md:939`. No source fix or build
was performed during routing.
