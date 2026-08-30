<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-30T12:54:59.840Z","dependsOn":[]} -->
# Remove unused island peak-height metadata

## Context

The false required condition is that every island must compute, serialize, and
retain its shipped peak height. `ExportIslandData` scans the downsampled
heightmap and stores `ExportedIsland::fMaxHeightMeters`
(`DataPacker/Source/ExportJobs/ExportIsland.cpp:419-423`), `Export` writes it
into `common::IslandHeader` (`ExportIsland.cpp:694-700`), and
`IslandTerrain` copies it into `IslandTemplate::mfMaxHeightMeters`
(`Engine/Source/Frame/IslandTerrain.cpp:36-45`). No current runtime, server,
renderer, shader, save, or tool reads the copied value. The independent low-
island rejection still computes its own local peak before the taper
(`DataPacker/Source/ExportJobs/Island/ProcessBakedRegion.cpp:336-350`).

This Plan combines the exact duplicate candidates `CPS/shard-0002/001` and
`CPS/shard-0005/002`. The user has explicitly directed removal at the
next/current intentional pack-format change, overriding the earlier
advisory route outcome. The concern is pre-existing at session baseline
`80896f33661aaab99cf180a96db54600099be652`; this Plan records debt and does
not change the active Investigation files.

## Design

The author's recommendation is to remove the complete unconsumed peak-height
chain in one pack-format change: delete `ExportedIsland::fMaxHeightMeters`
and its producer assignment, delete `IslandHeader::fMaxHeightMeters`, and
delete `IslandTemplate::mfMaxHeightMeters` plus its constructor copy. Keep the
`ProcessBakedRegion` local peak and its rejection threshold unchanged; it is
the independent live consumer.

Advance `ExportIsland::GetVersion` from `Version(29)` to `Version(30)` and
advance the manual component of `common::DataHeader::kiVersion` from `51` to
`52` while retaining the `sizeof(ChunkHeader)` term. Update the nearby
`IslandHeader` size assertion to the measured post-removal layout (64 bytes),
then regenerate island `.pack` chunks, manifests, and generated data through
the normal Local DataPacker route. Do not add an old-layout reader, dual-format
fallback, replacement metadata field, or a new assertion for an unconsumed
value.

## Critical files

- `DataPacker/Source/ExportJobs/ExportIsland.cpp:22-37,419-423,644-700` — peak producer and chunk writer.
- `DataPacker/Source/ExportJobs/ExportIsland.h:30-39` — owning island payload version.
- `Common/DataFile.h:279-307,423-442` — serialized header layout, assertion, and shared version.
- `Engine/Source/Frame/IslandTerrain.h:53-64` and `IslandTerrain.cpp:36-45` — runtime mirror and copy.
- `DataPacker/Source/ExportJobs/Island/ProcessBakedRegion.cpp:336-350` — independent low-island rejection that must remain.
- `Engine/Source/File/PackChunks.cpp:263-274` — current pack-header version admission.

## In scope

- Removing the producer, serialized field, runtime member, and assignment for
  the unused peak-height metadata.
- Advancing the two owning format/version values and regenerating current
  island packed data under the repository's Local DataPacker workflow.
- Updating the layout assertion and comments that describe the removed field,
  then checking every current island writer and reader for the new layout.
- Preserving the existing local rejection maximum, heightmap payload, hull,
  texture CRCs, and configured `fWorldElevationMeters` behavior.

## Out of scope

- Changing the low-island rejection threshold or its pre-taper ordering.
- Adding a peak-height consumer, alternate metadata representation, fallback
  reader, compatibility layout, wire/save/replay field, or CRC behavior.
- Changing island elevation sampling, placement, navigation, textures, Gaea
  geometry, or any unrelated `IslandHeader` field.
- The separate Flow-mask cleanup, except for the reciprocal shared-file
  coordination below.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: `IslandHeader` is shared serialized
`.pack` data consumed by DataPacker and both runtime builds; its size and
version gates must change together and current packed data must be regenerated.

Preserve these invariants:

- `IslandHeader` writer and runtime reader agree on field order, offsets, and
  the post-removal size.
- Old packed data is rejected by the current version gate; no speculative
  compatibility path is introduced.
- The independent downsampled peak still rejects low/underwater leaves before
  tapering, and accepted heightmap, hull, and texture outputs keep their
  existing meaning.
- Deterministic frame CRC, wire messages, saves, replays, and simulation
  semantics do not gain a peak-height dependency.

## Acceptance criteria

- A repository-wide exact-symbol search finds no `fMaxHeightMeters` or
  `mfMaxHeightMeters` producer/header/runtime assignment or read; the only
  remaining `fMaxHeightMeters` occurrence is the local rejection value in
  `ProcessBakedRegion`.
- `Common/DataFile.h` compiles with the measured 64-byte `IslandHeader`, and
  `ExportIsland` and `DataHeader` use the explicitly advanced versions.
- A Local DataPacker regeneration produces valid island `.pack`/manifest output,
  and client and server pack loading accepts the regenerated layout while a
  stale old-version pack follows the existing rejection path.
- The low-island rejection decision and its pre-taper ordering remain present,
  and no current texture, hull, placement, navigation, CRC, wire, save, or
  replay behavior changes because of the metadata removal.
- Client/server Debug builds pass `/compile`; no unit tests are added.

## Coordination

`Documents/Plans/Engine/IslandFlowMaskCleanup.md` also changes
`DataPacker/Source/ExportJobs/ExportIsland.cpp`. Neither Plan depends on the
other. Implement and review the producer/header edits as one coordinated
ExportIsland pass (or rebase before editing), preserving this Plan's payload
version bump alongside the Flow Plan's texture/fingerprint changes.

## Notes

The durable source and authority citations above remain sufficient after the
ignored triage files are cleaned up. The manual low-island scan is not the
duplicate metadata consumer; it must remain local to `ProcessBakedRegion`.
