<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T02:00:05.521Z","dependsOn":[]} -->
# Reject empty island-placement lists at the shared reader

## Context

The island generator always places the dominant anchor at index zero
(`Engine/Source/Frame/IslandChainPlacement.cpp:287-322`), and Player navigation
modes 4 and 5 use the placement count as a random/modulo bound and index the list
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersNavigation.cpp:311-367`).
`FrameStaticData::Read` currently accepts a serialized count of zero because its
bounded-count helper permits any nonnegative count within capacity
(`Engine/Source/Frame/FrameStaticData.cpp:24-37`; `Common/Serialization.h:48-69`).
An empty list can therefore cross the shared save/network reader even though valid
generated cells are nonempty.

The earlier implementation attempt expanded this into per-slot admission identity,
reset/reuse/recovery state, initial-full-state gating, and a server harness fixture.
That design solved selective same-connection recovery, but selective recovery was not
part of the required invariant and conflicts with the existing policy that corrupt
server data is fatal to the client (`Engine/Source/Network/AGENTS.md:18-22`;
`Engine/Source/Network/Client/Client.cpp:246-313`). The user rejected that expansion
as over-engineered. Its evidence remains available in commit
`56f2906f3851e4a7e51a6c317b52d6ea8b069721` and
`Temp/StaticIslandPlacementGrillDecision.md`; its latch architecture is not a fixed
decision for this Plan.

## Design

Add a local zero-count rejection in `FrameStaticData::Read` after the existing bounded
count validation and before `islands.resize`. Throw the reader's existing
`common::CorruptStreamException` with `FrameStaticData::Read` as the reader name. Keep
the shared count helper unchanged because other collections may validly be empty.

This is sufficient for network input because `Client::Receive` already treats a
reader-reported corrupt server packet as fatal, so the client does not continue to the
separately sent full-state packet. It also rejects an empty saved placement list
through the same reader contract. Preserve positive counts, serialized layout,
placement ordering, generator output, and navigation behavior. Do not add selective
packet recovery, subscription state, or a purpose-built runtime fixture.

## Critical files

- `Engine/Source/Frame/FrameStaticData.cpp:24-37` — add the local nonempty-count
  predicate before placement-vector resize.
- `Common/Serialization.h:48-69` — existing generic bounded-count behavior; read-only.
- `Engine/Source/Network/Client/Client.cpp:246-313` — existing fatal corrupt-input
  policy; read-only.
- `Engine/Source/Frame/IslandChainPlacement.cpp:287-322` — generated nonempty anchor
  invariant; read-only.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersNavigation.cpp:311-367`
  — empty-list consumers; read-only.

## In scope

- Rejecting a zero island-placement count inside `FrameStaticData::Read` after the
  existing capacity/stream validation and before resizing or reading placements.
- Preserving the existing exception type and reader name used by shared save/network
  deserialization.
- Verifying that otherwise-valid positive counts retain their existing serialized
  layout, ordering, and reader behavior.

## Out of scope

- Changing the generic count helper, placement format/version, island membership,
  area geometry, navigation topology, generator algorithm, or Player navigation.
- Selective same-connection recovery, packet-local drop behavior, per-slot admission
  state, full-state gating, unsubscribe/retry policy, reconnect policy, or active
  resync behavior.
- Server/client fixtures, new Agent Harness commands or scenarios, fallback islands,
  compatibility code, collection changes, or unit tests.

## Risk tier and invariants

Tier 2 — this tightens one semantic predicate at an existing shared deserialization
boundary without changing the format, trust model, exception contract, or valid data.

- Every successfully read `FrameStaticData` placement list is nonempty.
- Zero is rejected before `islands.resize` or placement reads; the client network path
  reaches its existing fatal corrupt-input policy before any paired full state can be
  processed.
- Positive placement data, ordering, index-zero generator output, serialization bytes,
  and deterministic navigation behavior remain unchanged.

## Acceptance criteria

- Changed-byte inspection shows the zero-count predicate after bounded count
  validation and before `islands.resize`, throwing `CorruptStreamException` with the
  existing reader name.
- Call-site inspection confirms network reads still reach the client-fatal corruption
  catch and save/network readers share the tightened predicate; no subscription or
  recovery state is added.
- The diff changes no wire layout, generic count-helper behavior, generator code,
  navigation code, or valid positive-count path beyond the new zero branch.
- BrokenEngineSandbox client and server `Debug|x64` builds pass through `/compile`.

## Notes

The original immutable `createdUtc` and empty dependency list are preserved. A future
request for selective recovery from corrupt server packets would change the existing
client corruption policy and must be planned separately rather than inferred here.
