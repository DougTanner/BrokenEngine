<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-19T14:19:00.924Z","dependsOn":[]} -->
# Fix: Tolerate an absent pack chunk when reblurring registered lighting textures

## Context

`TextureManager::ReblurAllLightingTextures`
(`Engine/Source/Graphics/Managers/TextureManager.cpp:845-853`) walks every
registered lighting CRC and resolves each one with
`gpFileManager->GetLazyChunk(crc)` before testing its chunk state. That accessor
is `mLazyChunkMap.at(crc)` (`Engine/Source/File/PackChunks.cpp:604-607`, reached
through `FileManager::GetLazyChunk` at
`Engine/Source/File/FileManager.cpp:455-458`), so a CRC with no entry in the
lazy-chunk map throws `std::out_of_range`.

A registered lighting CRC can legitimately have no entry.
`Engine/Source/File/AGENTS.md` `## Packed Assets` records that cross-pack
references go unvalidated when packs open and the lazy-chunk map is fixed at
construction, so packs published from different DataPacker runs can name a chunk
that stays absent for the whole process lifetime. Every other consumer of such a
CRC already tolerates that: `PackChunkLoader::RequestChunkLoad`
(`Engine/Source/File/PackChunkLoader.cpp:57-72`) looks the CRC up with `find` and
skips a miss, with a comment stating the absence is expected and that each
consumer classifies it; and the lighting CRC's own registration site pairs
exactly that tolerant request with the pre-blur registration
(`Engine/Source/Frame/Collections/Collection.h:136-155`,
`RegisterLightingTextureCrc` at `TextureManager.cpp:738-746`). An absent CRC is
therefore registered for pre-blur, never loaded, never adopted, and silently left
on the white placeholder — until the first lighting blur-setting change polls
through `Graphics::Refresh` (`Engine/Source/Graphics/Graphics.cpp:563-569`,
the sole caller), which throws.

This is pre-existing: the same `.at()` lookup over the same registered set was
reachable from `Graphics::Refresh` before the lighting-CRC set moved to
`TextureUploadManager`.

Impact: with a mixed-generation pack set, moving a lighting blur slider throws
out of the client's render path instead of leaving the unloaded texture alone.

## Design

Author's recommendation: classify the absence in this consumer the way the
request path already classifies it — skip the CRC — rather than changing the File
layer or promoting the absence to a failure.

Replace the `GetLazyChunk` call in the loop with a lookup into the const map
`gpFileManager->GetLazyChunkMap()` (`Engine/Source/File/FileManager.h:224`),
`continue` on a miss, and keep the existing `eState >= ChunkState::kReady` test
and `BlurLightingTexture(crc)` call for a hit. `LazyChunk::eState` is an atomic
loaded with acquire ordering, so the const reference is sufficient; the same
`GetLazyChunkMap()`-plus-lookup pattern is already used at
`Engine/Source/Frame/IslandTerrain.cpp:28` and `:155`. Carry one comment giving
the reason the miss is tolerated, pointing at the cross-pack-reference rule
rather than restating it.

Skipping is the behavior the rest of the system already produces for such a CRC:
it is never requested successfully, never reaches `kReady`, and never adopts, so
a reblur pass that skips it leaves exactly the state every other pass leaves.
This is not the "corrupt `.pack` chunk" case that `## Packed Assets` requires be
fatal — an absent chunk is a published cross-pack reference, not corruption — so
no `ASSERT` is added.

No File-layer change: `GetLazyChunk`, `PackChunks`, and `PackChunkLoader` keep
their current signatures and behavior, and the other `GetLazyChunk` call sites
keep their `.at()` contract, which their own callers satisfy.

## Critical files

- `Engine/Source/Graphics/Managers/TextureManager.cpp:845-853` —
  `ReblurAllLightingTextures` and its throwing lookup.
- `Engine/Source/File/FileManager.h:224,241` — the const map accessor to use and
  the throwing accessor to stop using here.
- `Engine/Source/File/PackChunkLoader.cpp:57-72` — the tolerant request path this
  change matches.
- `Engine/Source/File/AGENTS.md` `## Packed Assets` — the cross-pack-reference
  rule that makes the absence expected.
- `Engine/Source/Frame/IslandTerrain.cpp:28,155` — the existing
  `GetLazyChunkMap()` lookup pattern.

## In scope

- `Engine/Source/Graphics/Managers/TextureManager.cpp`, inside
  `TextureManager::ReblurAllLightingTextures` only: replacing the per-CRC
  `gpFileManager->GetLazyChunk(crc)` call with a `GetLazyChunkMap()` lookup that
  skips a CRC absent from the map, with one comment recording why the absence is
  tolerated.

## Out of scope

- Any change under `Engine/Source/File/`, including `GetLazyChunk`,
  `PackChunks`, `PackChunkLoader`, and `Engine/Source/File/AGENTS.md`; the
  existing contracts are correct and this is a consumer-side classification.
- Other `GetLazyChunk` call sites (`TextureManager.cpp:537,607,673`,
  `TextureUploadManager.cpp:115,265,347`,
  `Engine/Source/Frame/IslandTerrainResidency.cpp:44,130,251`) and the
  `GetLazyChunkMap().at()` audio sites.
- Where `ReblurAllLightingTextures` is called from, when it is called, and the
  GPU-lifetime ordering of that call.
- Lighting-CRC registration, the `kiLightingBlurSlots` capacity assertion, the
  blur algorithm, blur parameters, and bindless descriptor publication.
- Logging or an assertion for an absent registered lighting CRC, a pack
  generation or manifest consistency check, and any DataPacker change.
- Deterministic simulation, wire, save, replay, `.pack`, and `kiVersion` data.

## Risk tier and invariants

Expected Change Workflow Tier 2. Trigger: scoped runtime behavior of one unit in
one subsystem — a lookup tolerance inside a single client-only `TextureManager`
function at an existing boundary, with no determinism/CRC, wire, serialization,
save/replay, threading, or trust-boundary exposure.

Preserve these invariants:

- A registered lighting CRC whose chunk is present and at or past
  `ChunkState::kReady` is still reblurred, and one below that state is still
  skipped.
- The chunk-state read keeps its acquire ordering.
- The change is client-only and stays outside the PostRender CRC, save, replay,
  wire, and `.pack` data.
- Corrupt pack data stays fatal; only a CRC with no map entry is skipped.

## Acceptance criteria

- Inspection of `TextureManager::ReblurAllLightingTextures` shows no call that
  throws for a CRC absent from the lazy-chunk map, and shows the ready-state test
  and `BlurLightingTexture` call unchanged for a present CRC.
- The diff touches only `Engine/Source/Graphics/Managers/TextureManager.cpp` and
  only inside that function.
- Client `Debug|x64` and `Release|x64` build clean through `/compile`, with no new
  warnings.

## Notes

`Documents/Plans/Engine/LightingReblurFenceOrdering.md` is independent and is not
a prerequisite: it owns when and from where `ReblurAllLightingTextures` is
called and the GPU-lifetime window around it, which this Plan's `## Out of scope`
excludes, while this Plan owns only the per-CRC lookup inside the loop body.
Either can land first.
