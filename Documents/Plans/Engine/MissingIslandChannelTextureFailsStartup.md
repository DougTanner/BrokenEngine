<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-19T18:22:40.555Z","dependsOn":[]} -->
# Missing island channel texture fails client startup

## Context

An island chunk header names its four channel textures (color, normals,
ambient occlusion, masks) by path CRC: DataPacker's `ExportIsland` stores
`common::Crc` of each channel's relative path in `IslandHeader::colorsCrc`,
`normalsCrc`, `ambientOcclusionCrc`, and `masksCrc` (`Common/DataFile.h`). The
textures themselves ship in the separate Texture pack. When a pack set pairs an
Islands pack with a Texture pack that lacks one of those chunks (for example
packs from different DataPacker runs), the client today substitutes a
placeholder:

- `PackChunks::LoadPackFiles` validates every chunk header inside its own pack
  but never cross-checks island channel CRCs against the Texture pack.
- `PackChunkLoader::RequestChunkLoad` silently skips an absent CRC; its comment
  lists island channel headers among the references that may be absent and
  leaves classification to each consumer.
- `TextureDescriptors::MintIslandSlot` looks each channel CRC up in
  `TextureManager::mTextureMap`; on a miss it logs `kError`
  `Island channel texture missing: islandCrc=... channel=... textureCrc=...` and
  points that slot's channel at the slot-0 placeholder.
- `IsTextureRestorationPending` (`Engine/Source/Frame/IslandTerrainResidency.cpp`)
  requires all four channel chunks ready, and `PackChunks::IsChunkReady` returns
  false for an absent CRC, so the island's elevation never leaves the slot-0
  placeholder either: the island renders as a plain placeholder for the whole
  session.

`TextureManager`'s constructor fills `mTextureMap` with exactly the lazy chunks
flagged `common::ChunkFlags::kTexture`, and the lazy-chunk map is fixed when
`LoadPackFiles` returns, so whether every island channel texture exists is fully
known at pack open.

Documentation conflict. `Engine/Source/File/AGENTS.md` `## Packed Assets` says
corrupt runtime `.pack` chunks and pack-backed textures halt and must never
"soft-fail, skip, or substitute a placeholder", while the next-but-one bullet
says cross-pack references are unvalidated at open, so packs from different
DataPacker runs can name a chunk absent for the whole process lifetime. The
island placeholder fallback was classified under the second bullet.

Recorded user decision (agent's record): at approval of the texture-streaming
stall fix the user answered "Q3Option 3", where Option 3 was presented as "Save
a plan that makes a missing island texture a hard error." The hard error for a
missing island channel texture is therefore the user's choice; where the check
runs and which failure path it uses are this Plan's recommendations below.

## Design

Recommended: validate at pack open, not at slot assignment, and fail through the
existing required-asset startup path.

1. In `PackChunks::LoadPackFiles`, client only (`#if defined(BT_CLIENT)`: the
   server opens only the Islands pack, per `IsServerChunk`, and never consumes
   channel textures), after the per-type loop has filled `mLazyChunkMap` and
   before the lazy pool layout, visit every lazy chunk flagged
   `common::ChunkFlags::kIsland` and require each of its four `islandHeader`
   channel CRCs to resolve in `mLazyChunkMap` to a chunk flagged
   `common::ChunkFlags::kTexture` — the same membership `mTextureMap` uses. On
   the first miss call `FailMissingRequiredAsset` with the Texture pack path
   (`mpPackFilePaths[data::kDataTypeTexture]`) and a reason naming the island
   chunk's `pcPath`, the channel, and the missing CRC.
2. In `TextureDescriptors::MintIslandSlot`, delete the missing-texture branch,
   its `kError` log, and the comment justifying the soft fail; assign each
   channel from `mrTextureManager.mTextureMap.at(...)`, as
   `IslandTerrainResidency::EvictTemplate` already does for the same CRCs.
3. In `PackChunkLoader::RequestChunkLoad`, drop "island channel headers" from the
   comment's list of references that may be absent; scene texture lists stay.
4. Reconcile `Engine/Source/File/AGENTS.md` `## Packed Assets`: rewrite the
   cross-pack bullet so island channel textures are the stated exception —
   on the client, pack open fails startup through the required-asset path when
   an island chunk names a channel texture absent from the Texture pack — and
   other cross-pack references remain unvalidated at open and can name a chunk
   absent for the whole process lifetime. The halt bullet then no longer
   conflicts for island channel textures.

What "hard error" means here: the existing required-asset failure path
(`FailMissingRequiredAsset`, `Engine/Source/File/AGENTS.md` `## File Contracts`,
`Engine/Source/AGENTS.md` `## Startup and Main Loop`). It logs the diagnostic at
`kError`, breaks into an attached debugger, and throws `std::runtime_error`,
which `ProcessMain`'s catch around `FileManager` construction handles: a normal
client launch shows the "A required game data file is missing or corrupt ...
Please reinstall or verify your game files." message once, an agent launch shows
no modal, and the process returns nonzero through ordinary cleanup without a
crash report. No partial-pack mode exists, so the client does not start at all.

Rationale for load time over slot assignment:

- The bad-value rule in `.agents/references/cpp-conventions.md` says to fail the
  load through the owning boundary's failure path. `LoadPackFiles` is the
  boundary that reads and validates island chunk headers, and it already answers
  duplicate CRCs and corrupt headers with `FailMissingRequiredAsset`.
- The answer is complete at open, so the failure is deterministic and immediate.
  An `ASSERT` in `MintIslandSlot` would fire only when the affected island is
  first rendered, possibly never in a session, and its `std::runtime_error`
  escapes outside the startup catch into the crash-report path.

Exposure: client-only render residency, so no determinism or CRC impact; no
`.pack`, manifest, or `kiVersion` change; no wire change, and the Islands chunk
table is read, not sorted or re-emitted, so the handshake pack integrity token
is unchanged. The check runs on the constructing thread before the eager task
starts the loader threads. Startup may allocate, so a formatted reason string is
fine. `PackChunks.cpp` is in both projects; the narrowest guard is a
`BT_CLIENT` block around the new check.

## Critical files

- `Engine/Source/File/PackChunks.cpp` — `PackChunks::LoadPackFiles`,
  `FailMissingRequiredAsset`
- `Engine/Source/Graphics/Managers/TextureDescriptors.cpp` —
  `TextureDescriptors::MintIslandSlot`
- `Engine/Source/File/PackChunkLoader.cpp` — `PackChunkLoader::RequestChunkLoad`
  comment
- `Engine/Source/File/AGENTS.md` — `## Packed Assets` cross-pack bullet

## In scope

- A client-only island channel-texture existence check in
  `PackChunks::LoadPackFiles`, failing through `FailMissingRequiredAsset`.
- Removal of the placeholder fallback, its log line, and its comment from
  `TextureDescriptors::MintIslandSlot`, with `.at` lookups.
- The `PackChunkLoader::RequestChunkLoad` comment edit.
- The `Engine/Source/File/AGENTS.md` `## Packed Assets` cross-pack bullet
  rewrite.

## Out of scope

- Other cross-pack references: scene texture lists (`ModelPipeline`,
  `PipelineDescriptorWriter`) and registered lighting-texture CRCs
  (`TextureManager::ReblurAllLightingTextures`) keep their current behavior.
- Slot-0 placeholders used while channels load and after eviction
  (`EvictIslandSlot`, `IsTextureRestorationPending`); those are residency
  states, not missing-texture fallbacks.
- DataPacker, pack formats, manifests, and any version bump.
- The server, the `IslandTerrain` constructor's channel-uniqueness `ASSERT`, and
  the startup catches in `Engine/Source/Main.cpp`.

## Acceptance criteria

| Criterion | Evidence |
|---|---|
| `LoadPackFiles` checks all four channel CRCs of every island chunk for presence with the `kTexture` flag, client only, and fails via `FailMissingRequiredAsset` | Diff |
| `MintIslandSlot` has no missing-texture branch and no `Island channel texture missing` log | Diff |
| The `RequestChunkLoad` comment no longer lists island channel headers | Diff |
| `Engine/Source/File/AGENTS.md` `## Packed Assets` states the island channel-texture exception and no longer conflicts with the halt bullet for island textures | Diff |
| Client and server build | `/compile` |
| With current Sandbox data the client starts, reaches the main menu, and renders islands; its log holds no required-asset error | `/agent-harness` launch, screenshot, log |

The negative path reuses `FailMissingRequiredAsset` unchanged, so the diff
settles it; building a deliberately mismatched pack set is not required.

## Notes

- Change Workflow tier: Tier 2. Trigger: tightening a check inside one unit at
  an existing trust boundary (pack open), with the format and the trust
  unchanged; the `MintIslandSlot` edit only removes the branch that check makes
  unreachable.
- Cite functions, not line numbers: the texture-streaming stall fix also edits
  `TextureDescriptors.cpp` near `MintIslandSlot`.
