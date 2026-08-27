<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:39:58.077Z","dependsOn":[]} -->
# Fix: Guarantee mapped host-visible buffers stay mappable

## Context

The accepted survivor `CAI/shard-0031/001` shows that the non-indirect host
access branch in `Buffer::CreateBuffer` requests mapped sequential-write memory
but also allows VMA to transfer the allocation to non-host-visible device-local
memory (`Engine/Source/Graphics/Objects/Buffer.cpp:30-60`).  The wrapper stores
`pMappedData` without requiring it to be non-null, while staging and per-frame
callers write through that pointer (`Buffer.cpp:77-84,235-251`; `Engine/Source/Graphics/Render/GlobalUniforms.cpp:488-501`;
`MainUniforms.cpp:501-503,591-594`).

The source report is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0031.md:74`;
the consolidated selector is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:850`.
All 16 frozen target rows matched baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; no source changes were made in
this routing session.

Impact: a normal discrete GPU can return a null mapping for a staging or
per-frame buffer, causing boot/upload or first-frame failure on an unguarded
CPU write.

## Design

Author's recommendation: make the existing mapped-buffer contract explicit in
the non-indirect host-access branch.  Require
`VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT`
alongside the mapped sequential-write flags, and remove
`HOST_ACCESS_ALLOW_TRANSFER_INSTEAD` from that branch.  Keep the dedicated
indirect branch and device-local destination/staging copy paths unchanged; a
buffer exposing `mpMappedMemory` must always be directly mappable.

## Critical files

- `Engine/Source/Graphics/Objects/Buffer.cpp:30-60` — VMA allocation flags
  and required memory properties.
- `Engine/Source/Graphics/Objects/Buffer.cpp:64-90,235-258` — staging and
  mapped-buffer consumers.
- `Engine/Source/Graphics/Objects/Buffer.h` — mapped-pointer ownership contract
  if its comments require correction.
- `Engine/Source/Graphics/Managers/BufferManager.cpp:25-38,311-333` and
  `Engine/Source/Graphics/Render/GlobalUniforms.cpp:488-501` — representative
  callers.

## In scope

- Requiring host-visible/coherent memory for the upload/staging branch that
  returns a mapped pointer.
- Preserving the existing VMA mapped pointer publication and CPU-write callers
  once that precondition is guaranteed.
- Updating local contract comments or failure propagation needed to ensure a
  null mapping is never exposed as usable state.

## Out of scope

- Designing a new device-local staging fallback, changing transfer usage/barriers,
  indirect/readback allocation policy, or moving mapped storage to another
  manager.
- VMA version changes, shader/layout changes, and unrelated allocation failures.
- Server code, deterministic simulation, wire/save/replay formats, and unit
  tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3). Trigger: allocation
policy controls CPU/GPU buffer ownership and mapped writes across Buffer,
staging uploads, render uniforms, and VMA.

Tier rationale: the Design fully specifies a flag edit in one branch of one
function — require host-visible/coherent memory and drop
`HOST_ACCESS_ALLOW_TRANSFER_INSTEAD` — which makes the mapping contract callers
already assume explicit. No buffer usage, barrier, layout, or GPU-visible data
changes, and callers are untouched.

Preserve these invariants:

- Every staging, per-frame, or host-visible Buffer with a published mapping is
  host-visible and coherent before any CPU write.
- Device-local destination buffers continue to use explicit staging copies and
  existing barriers.
- Indirect and readback allocations retain their current required flags and
  mapping behavior.
- No simulation CRC, wire, save, replay, or `.pack` bytes change.

## Acceptance criteria

- On a discrete device without host-visible device-local/BAR memory, staging and
  per-frame allocations still return valid mapped pointers from host-visible
  memory and boot/first-frame writes complete.
- A VMA allocation cannot satisfy this wrapper branch with a non-host-visible
  memory type or a null mapped pointer.
- Client Debug and Release builds pass `/compile`; model/texture upload and
  Global/Main uniform scenarios have no access violation or VMA mapping error.
- A scoped search confirms `ALLOW_TRANSFER_INSTEAD` remains absent from the
  branch whose callers dereference `mpMappedMemory`.

## Notes

The audit catalog identifies this as `CAI-EXT-009`; the VMA flag and mapping
rules are an external allocator contract to preserve during implementation.
