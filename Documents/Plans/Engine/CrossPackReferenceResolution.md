<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:29:36.419Z","dependsOn":[]} -->
# Classify missing cross-pack asset references before map access

## Context

The accepted finding `CAI/shard-0013/004` identifies a mixed-generation pack
window in which opaque CRC references are assumed to exist. DataPacker publishes
Scene/Island and Model/Texture types independently
(`DataPacker/Source/Main.cpp:657-665,140-168`). Runtime scene texture and island
channel references reach `.at` lookups through
`PackChunks::RequestChunkLoad`/`GetLazyChunk`
(`Engine/Source/File/PackChunks.cpp:399-421,749-751`),
`ModelPipeline.cpp:192-195`, `IslandTerrainResidency.cpp:147-166`, and
`TextureDescriptors.cpp:362-371`. A new or stale mixed pack set can therefore
throw on first scene/island use instead of preserving the established placeholder
or soft-failure path.

The shard's frozen/live target hashes match audit baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; current status contains only the
six pre-existing `Documents/Plans/Agents/Cpp*Audit.md` edits. The missing
cross-pack resolution is unresolved, pre-existing, and outside the approved
audit work.

## Design

The author's recommendation is to keep per-type DataPacker publication intact
and make runtime reference lookup classify a missing CRC. `RequestChunkLoad`,
lazy lookup, and texture-descriptor lookup should propagate a missing-asset
result to their existing placeholder/not-ready consumers rather than allowing
`unordered_map::at` to escape. Validate scene texture and island channel
references at first use, retain the loaded-generation behavior when all
references exist, and leave a diagnosable owner/CRC record for the failure.

## Critical files

- `Engine/Source/File/PackChunks.cpp:399-421,749-751` — lazy request and lookup boundaries.
- `Engine/Source/Graphics/Objects/ModelPipeline.cpp:192-195` — scene texture references.
- `Engine/Source/Frame/IslandTerrainResidency.cpp:147-166` — island channel references.
- `Engine/Source/Graphics/Managers/TextureDescriptors.cpp:362-371` — texture descriptor map access.
- `DataPacker/Source/Main.cpp:140-168,657-665` — independent publication (read-only evidence).

## In scope

- Missing-CRC classification at the runtime request/lookup boundaries named
  above.
- Propagating that result through scene and island consumers so a missing
  reference leaves a placeholder/not-ready state and does not throw.
- Diagnostics identifying the owning asset/reference and missing CRC.

## Out of scope

- Making all DataPacker outputs one atomic publication, adding a new generation
  token, changing manifest or pack formats, or changing handshake identity.
- General pack bounds, path termination, audio-header validation, texture
  rendering, and unrelated `.at` calls on trusted in-memory maps.
- Retrying policy beyond the existing request/placeholder lifecycle.

## Risk tier and invariants

Expected Change Workflow Tier 3. The change handles opaque pack references at
a trust boundary and crosses File, Frame, and Graphics consumers during client
startup/render.

Preserve these invariants:

- A complete compatible pack set resolves every reference exactly as before.
- A missing reference never throws from the main/render path; it remains a
  classified placeholder/not-ready result that can recover when the owning
  chunk becomes available.
- Existing CRC identity, lazy release/acquire publication, request priority,
  and texture descriptor ownership remain unchanged.

## Acceptance criteria

- A mixed Scene/Island and Model/Texture pack set with one absent referenced
  CRC reaches placeholder/soft-failure handling without an uncaught map
  exception on first scene or island use.
- A complete pack generation still requests and resolves all references with
  the existing textures and scene output.
- Client and server `Debug|x64` builds clean through `/compile`.

## Coordination

`Documents/Plans/Engine/LazyAudioMetadataValidation.md` and
`Documents/Plans/Engine/PackedPathTermination.md` inspect adjacent
`PackChunks.cpp` trust-boundary code. Keep missing-CRC handling separate from
audio metadata and fixed-path validation; re-derive line ranges before editing.
No dependency is required.

## Notes

The consolidated index records no duplicate-family hint or external claim for
this candidate.
