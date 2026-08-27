<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:33:25.731Z","dependsOn":[]} -->
# Reject out-of-range serialized explosion type indices

## Context

The accepted finding `CAI/shard-0017/002` identifies raw registry-index bytes
being accepted at the collection trust boundary. `Collection::Read` and
`CollectionRead` hydrate member arrays without semantic validation
(`Engine/Source/Frame/Collections/Collection.h:338-353`), while
`TypeRegistry::GetType` throws only when a later phase calls `sTypes.at`
(`Collection.h:164-167`). `ExplosionsInterpolate::PostRead` currently clamps
trail counts but does not validate `puiTypeIndices` (`Engine/Source/Frame/Collections/Explosions/Explosions.h:171-184`).
An explosion row containing `0xFF` can therefore be adopted and throw from
`ExplosionsPostRender::Destroy` on the next server tick
(`Explosions.cpp:241-305`), outside the parser's recovery boundary.

The source shard's frozen/live target hashes match audit baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; current status contains only the
six pre-existing `Documents/Plans/Agents/Cpp*Audit.md` edits. The missing
explosion registry check is unresolved, pre-existing, and outside the approved
audit work.

## Design

The author's recommendation is to extend `ExplosionsInterpolate::PostRead` to
check every deserialized `puiTypeIndices` value against the immutable
`ExplosionsInterpolate::sTypes` registry before the loaded collection can be
adopted. Treat `kuiInvalidTypeIndex` and any value outside the registry as
corrupt input and throw the existing `CorruptStreamException`; retain trail
count normalization and the current startup-only registration order. Keep
other collection registries outside this focused candidate unless the same
read path proves they are part of this exact explosion record.

## Critical files

- `Engine/Source/Frame/Collections/Explosions/Explosions.h:171-184,209-237` — post-read hook and serialized type-index column.
- `Engine/Source/Frame/Collections/Collection.h:70-83,152-167,338-353` — post-read dispatch and registry access.
- `Engine/Source/Frame/Collections/Explosions/Explosions.cpp:241-305` and `ExplosionsUpdate.cpp:69-72` — later registry consumers (read-only evidence).
- `Engine/Source/Frame/AGENTS.md` and `Engine/Source/Frame/Collections/Explosions/AGENTS.md` — serialized registry and replay contracts.

## In scope

- Validation of every serialized explosion `puiTypeIndices` entry against the
  registered explosion-type count and invalid sentinel before adoption.
- Routing an invalid explosion type through the existing corrupt save/replay/
  network result rather than a later `.at()` exception.
- Preserving `PostRead` trail-count normalization and valid registry behavior.

## Out of scope

- ID-map/paired-row identity (owned by `CollectionIdMapIdentity.md`), controller
  registration redesign, new registry entries, collection layout, or protocol/
  frame version changes.
- Validation of unrelated collection type columns, unless the implementation
  proves they are the same serialized explosion registry boundary.
- Clamping or substituting a default explosion type for corrupt input.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3). Serialized save/
replay/network bytes select runtime registry objects and can reach
deterministic simulation phases.

Preserve these invariants:

- Every accepted explosion row names a registered, non-sentinel type.
- Invalid input fails before adoption and cannot throw from a later tick/destroy
  phase; valid rows use the same immutable registry object.
- Random streams, collection layout/CRC, trail-count normalization, and client/
  server registration order remain unchanged.

Tier rationale: the fix is one pre-specified range test over an existing column
inside the existing `ExplosionsInterpolate::PostRead` hook, throwing the
existing `CorruptStreamException`. Serialized layout, registry contents, and
valid explosion rows are untouched, so only corrupt input changes outcome.

## Acceptance criteria

- A structurally valid frame with one explosion `puiTypeIndices` byte set to
  `0xFF` or an out-of-range value is rejected during read/post-read, before
  `ExplosionsPostRender::Destroy` can call `.at()`.
- Valid explosion frames retain the same post-read normalization and phase
  behavior on both client and server.
- Client and server `Debug|x64` builds clean through `/compile`; malformed
  save/full-state input is dropped through the existing corrupt-data boundary.

## Coordination

`Documents/Plans/Engine/CollectionIdMapIdentity.md` shares the generic
collection read/adoption boundary. Keep registry-index checks separate from
ID/map identity checks, preserve the common corrupt-stream gate, and re-derive
line ranges before implementation. No dependency is required.

## Notes

The consolidated index notes shared mechanics with a distinct custom transfer
candidate in another shard; no duplicate Plan exists for this explosion path.
