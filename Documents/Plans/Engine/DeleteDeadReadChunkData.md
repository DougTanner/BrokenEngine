<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-09T15:35:40.782Z","dependsOn":[]} -->
# Delete the uncalled PackChunks::ReadChunkData

## Context

`PackChunks::ReadChunkData` (declared `Engine/Source/File/PackChunks.h:49`, defined `Engine/Source/File/PackChunks.cpp:582-657`) has no caller anywhere in `Engine/`, `Projects/`, `Tools/`, or `DataPacker/`: `git grep -n ReadChunkData` finds only `TryReadChunkData`, the definition, the declaration, and three comments (`PackChunks.h:131`, `PackChunks.cpp:999-1000`, `PackChunkLoader.cpp:376`).

Its not-loaded lazy branch (`PackChunks.cpp:628-654`) reopens the pack by path through `std::fstream(mPackFilePaths[eDataType])`. That contradicts `Engine/Source/File/AGENTS.md` `## Packed Assets`, which records that a lazy pack is locked before its size and headers are read and that "every read of it, validation included, goes through that lock rather than its path". Today the branch is unreachable, but any new caller would silently read an unlocked file. The contradiction was flagged by `/coherence-review` during `Documents/Plans/Engine/FileOverEngineeringCleanup.md`, whose scope excluded the function.

## Design

Delete the whole function, declaration and definition: it is dead, so deleting it is smaller than rerouting its lazy branch through the lock. Reword the three comments that name it so they stay true:

- `PackChunks.h:131-133`: the eager-map readers that acquire `mbEagerLoadComplete` are then `IsChunkReady` and the memory-stats getters.
- `PackChunks.cpp:998-1000` (`RecommitAndReloadChunkRange`): drop the sentence explaining why `ReadChunkData` cannot serve the reload; the remaining comment still says the range is re-read from the pack.
- `PackChunkLoader.cpp:376-378`: name the remaining consumers of the published `pData` bytes instead of `ReadChunkData`'s resident-copy path; the release/acquire contract sentence stays.

Author's recommendation: keep `mPackFilePaths` — it is still used by `LoadPackFiles` and by `LoadAudioRead`'s failure messages (`PackChunks.cpp:949`, `:953`).

## Critical files

- `Engine/Source/File/PackChunks.h`
- `Engine/Source/File/PackChunks.cpp`
- `Engine/Source/File/PackChunkLoader.cpp`

## In scope

- Deleting `PackChunks::ReadChunkData` at `PackChunks.h:49` and `PackChunks.cpp:582-657` (with the blank line separating it from the next definition).
- Rewording the comments at `PackChunks.h:131-133`, `PackChunks.cpp:998-1000`, and `PackChunkLoader.cpp:376-378` that name it.

## Out of scope

- `TryReadChunkData`, `RecommitAndReloadChunkRange`'s code, `LoadPackFiles`, and every other `PackChunks` or `PackChunkLoader` function.
- `mPackFilePaths`, `mbEagerLoadComplete`, and the lazy-pack lock.
- `Engine/Source/File/AGENTS.md`: its `## Packed Assets` statement becomes true without an edit.

## Risk tier and invariants

Expected Tier 1 (trigger: behavior-preserving deletion of an uncalled function and the comments naming it; the removed public declaration has no consumer).

- No `.pack` format, `kiVersion`, threading, or read-path change.

## Acceptance criteria

- `git grep -n "ReadChunkData" -- Engine Projects Tools DataPacker` matches only `TryReadChunkData`.
- The client and the server build through `/compile`.

## Notes

- Line numbers were taken from the tree as left by `Documents/Plans/Engine/FileOverEngineeringCleanup.md`.
