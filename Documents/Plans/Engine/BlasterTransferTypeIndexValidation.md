<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:49:31.926Z","dependsOn":[]} -->
# Reject invalid Blaster type indices before transfer hydration

## Context

The accepted finding `CAI/shard-0046/001` identifies a custom transfer payload
gap.  `DeserializeBlasterTransfer` reads any `uint8_t uiTypeIndex` after only
checking the structural item width (`Engine/Source/Network/NetworkSerialization.cpp:74-83,292-360`).
The client buffers the decoded status and later routes it through
`SpawnTransfer` to `BlastersPostRender::Spawn`; client hydration calls the
immutable registry's `.at()` lookup with the raw byte
(`Projects/BrokenEngineSandbox/Source/SpawnTransfer.cpp:30-40`;
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/Blasters.cpp:42-55`).
An out-of-range value such as `0xFF` therefore throws during reconciliation,
outside the receive recovery boundary.  Replay transfer records carry the same
custom arm.

The source finding is recorded at
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0046.md:50`
and the consolidated selector at
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:1107`.
All assigned source and authority hashes match frozen baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; the semantic gap is pre-existing,
unresolved, and outside the audit work.

Impact: one structurally valid but semantically invalid transfer from the
server or replay stream can terminate the client before CRC/resync recovery.

## Design

Author's recommendation: add one Blaster-transfer semantic validation at the
client materialization boundary immediately before a transfer row is appended,
using the immutable registered Blaster-type count and rejecting the invalid
status through the existing malformed-batch or replay-abort path.  Apply the
same check to network and replay-created transfer records, keep startup type
registration unchanged, and leave valid transfer hydration and client-owned
visual initialization unchanged.

## Critical files

- `Engine/Source/Network/NetworkSerialization.cpp:74-83,292-360` — structural Blaster transfer codec.
- `Projects/BrokenEngineSandbox/Source/SpawnTransfer.cpp:30-40` — transfer materialization dispatch.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/Blasters.cpp:42-55,119-147` — registry lookup and spawn boundary.
- `Engine/Source/Network/Client/ClientReceive.cpp:389-404` and `Engine/Source/Network/Client/ReconcileReplayTick.cpp:132-145` — network/replay consumers.
- `Engine/Source/Frame/Collections/Collection.h:124-167` — immutable registry contract.

## In scope

- Validation of every `kTransferBlaster` type index before client row creation
  for network and replay transfer paths.
- Whole-status-batch/replay-record rejection through the current recovery
  mechanism rather than an uncaught registry exception.
- Valid Blaster registry order, transfer fields, client visual ownership, and
  server-produced type indices.

## Out of scope

- Generic full-state collection type-index validation (owned by
  `Documents/Plans/Engine/CollectionTypeIndexValidation.md`).
- New Blaster types, registry registration order, transfer wire layout/version,
  unrelated collection payloads, or Graphics recovery.
- Substituting a default type for corrupt input or changing valid replay output.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3).  Trigger: opaque
network/replay transfer data selects a runtime registry object during client
reconciliation.

Preserve these invariants:

- Every accepted Blaster transfer names a registered, non-sentinel type before
  any collection row or owned visual is created.
- Invalid transfer data reaches the existing packet/replay recovery path and
  cannot throw from a later fixed-tick reconciliation phase.
- Valid transfer fields, client/server registration order, CRC, and wire layout
  remain unchanged.

Tier rationale: the change is one pre-specified range test against the
registered Blaster-type count at the client materialization boundary, routing
a corrupt index into the existing malformed-batch/replay-abort path.  Wire
layout, transfer fields, and valid hydration are untouched, so only rejection
of already-invalid input is new.

## Acceptance criteria

- A structurally valid `kTransferBlaster` carrying `0xFF` or any index at least
  the registered Blaster-type count is rejected before `ClientInit`/`.at()` and
  leaves no partial row or visual resource.
- A replay transfer with the same invalid index aborts or rejects through the
  established replay path before `RunFrameTick` publishes the row.
- Valid player and enemy Blaster transfers hydrate exactly as before.
- Client and server `Debug|x64` builds clean through `/compile`; a malformed
  transfer/replay scenario reaches recovery rather than client termination.

## Coordination

`Documents/Plans/Engine/CollectionTypeIndexValidation.md` validates generic
serialized collection indices.  Keep this custom `TransferData.uiTypeIndex`
check separate from that full-state explosion boundary, while reusing the same
immutable-registry contract and re-deriving line ranges before implementation.

## Notes

The consolidated index explicitly records this as distinct from the generic
collection type-index finding; no duplicate Plan exists for this transfer arm.
