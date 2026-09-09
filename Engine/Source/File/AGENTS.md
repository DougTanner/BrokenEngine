# File - Runtime I/O and Packed Assets

`FileManager` (`gpFileManager`) owns platform paths, versioned and atomic file writes, and packed-asset access through `PackChunks`, a private packed-asset engine that is neither a manager nor an aggregation-header surface. `Replay` (`gpReplay`) owns the server's deterministic record and playback lifetime. `GridSave.h` and `Replay.h` deliberately include a game header, and are the only two here that do.

## File Contracts

Each process resolves a data root for assets and an AppData root for user files once, at `FileManager` construction. A file carrying a struct layout goes through one shared version header, and one-shot writes land atomically.

- Client and server launches must use the same data root; neither process can check that for itself.
- `FileManager` publishes `gpFileManager` only after platform directory setup and packed-asset construction succeed; code reachable during construction must not consume the global.
- Directory creation and required boot-asset failures log their exact path and reason once, then throw `std::runtime_error`. Handle that type only around `FileManager` construction and eager-load completion, with `std::system_error` rethrown first; widening the handler over `MainThread` would misclassify unrelated runtime failures as startup exits.
- Change an on-disk layout and its owning version together; that version is the only thing stopping an old file from being read as a new one.
- Report failure detail through out-references beside a success return or success out-param; never store a failure reason for a later query.

## Grid Saves

Server-only. The whole simulation grid is one versioned file, written atomically in sorted coord order so equal state produces equal bytes, and read into an isolated staged snapshot that reaches the live grid only after the entire stream validates. The file is a trust boundary, so `ReadGridSave` bounds and cross-checks the values it adopts except where a deliberate exemption is recorded here or in a bullet below; the serialized frame area is one such exemption, being derived, untrusted data recomputed from the coord. The snapshot carries the game's half of the payload by value without inspecting it; that half is owned on the game side ([game Network Server](../../../Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md)), and the two sets of stages change together.

- Island placement CRCs are deliberately unvalidated at load. A save or replay grid naming a template this build's island pack lacks is adopted, and the server then terminates at that cell's first navigation build — per-tick dispatch and paused-subscription main-thread paths alike — naming the offending CRC in the crash report. That termination is the intended detection, not a defect: add no membership check.

## Packed Assets

Runtime reads assets only from `.pack` files described by `.manifest` files. Eager pack types are read whole at startup; lazy ones are fetched on demand by background loader threads that publish each chunk's state with release/acquire ordering. The server opens only the pack types it consumes.

- Keep the eager/lazy split and the server's accepted set aligned with what DataPacker emits; nothing at runtime checks that split.
- Eager pack acquisition checks file size, open, and complete read before validating or publishing chunks. The existing eager-load future remains the failure carrier; every lazy chunk wait consumes it before waiting on loader threads, and any startup path that returns before the first such wait consumes it explicitly, so a failed task cannot be mistaken for successful startup.
- `WaitForChunks` blocks until a chunk is ready, but an already queued request is not reprioritized.
- The caller owns exclusion for the chunks it resets; the reset code itself locks nothing. Device loss drains accepted whole-chunk and streaming-audio range work before resetting texture state, while island eviction drains the renderer's Vulkan descriptor window. Keep the two in step, and settle the exclusion before adding a third.
- Sorting, filtering, or re-emitting the ordered Islands chunk table changes the pack integrity token the client sends in its connection handshake ([Network](../../../Documents/Architecture/Network.md)), so every client fails to connect while the reported error points at the data build instead.
- `EagerChunk.iDataSize` is the chunk table's raw on-disk extent, not the chunk header's size, whose scene-chunk value excludes the appended animation section. Bound eager reads against `iDataSize`, or animation data truncates silently with no crash.
- Corrupt runtime `.pack` chunks and pack-backed textures halt client and server alike, including at call sites outside this subsystem: validate and `ASSERT`; never soft-fail, skip, or substitute a placeholder.
- A new publish path validates first: once a pointer, slice, pool slot, or map entry exists, a corrupt value already addresses memory outside the pack. `ValidateChunkLocation` and `ValidateChunkHeader` are the existing instances.
- Cross-pack references go unvalidated when packs open, and the lazy-chunk map is fixed at construction, so packs published from different DataPacker runs can name a chunk that stays absent for the whole process lifetime.
- File owns all background packed-asset storage reads. Its two below-normal loader threads service whole chunks, range reloads, and client streaming-audio ranges; real-time queued work wins, then audio and other work alternate so neither can starve the other. Publish every accepted job and shutdown through the atomic wake sequence.
- Client streaming-audio reads use six fixed Pack-owned 16 KiB result entries. The caller keeps one request identity and exact range while polling; reset or destruction cancels it. Generation-tagged ownership makes queued and ready entries immediately reusable after cancellation, while an active loader retains its entry until it acknowledges cancellation. File must outlive Audio.
- Validate a streaming-audio request against its logical chunk payload before publishing it. On the loader, validate the physical pack extent and complete read before discarding a cancelled generation, because pack corruption remains fatal even when the caller no longer wants the result. Loaders write only Pack-owned result storage; the polling main thread performs the final copy to Audio.
- The [Agent audio fixture](../Agent/AGENTS.md) may inspect Pack state and use loader-queue synchronization only in client Debug builds; production state remains private elsewhere.

## Lazy-Pool Invariants

All lazy chunk data lives in one virtual-memory reservation laid out once at construction, by cumulative aligned size over the complete chunk map, so a chunk's pointer and size are fixed for the process lifetime and describe its reservation rather than its resident bytes.

- Never compact those pointers or recompute the offsets afterwards. Their being fixed is what lets a reader use a chunk pointer under the state acquire alone, with no lock.

## Replay Streams

Server-only. `Replay` owns writer and reader lifetime, the manifest that commits a recording, staged adoption of a playback, and the per-tick checksum comparison; `GameBase` constructs it and every user reaches it through `gpReplay`. Recording writes one stream set per active cell alongside recording-wide grid and metadata; playback validates the whole set before replacing live state, then resimulates and compares each tick against the recorded checksum.

- Replay operations exist only when `kbDebugInput` enables their tick-time implementation; that gate sits inside this subsystem's replay entry points, and only the constant is game-defined.
- A generation is one activation lifetime, identified in its stream files and inventory by coord and activation tick, while grid and metadata stay recording-wide.
- The manifest is the commit marker: recording start invalidates it before replacing any component, and stop publishes it only after every generation and the game metadata succeed.
- Missing retained terminal state for an evicted coord invalidates only that generation's files, without skipping the write attempts for other writers or metadata.
- The in-memory active-coord list is not order-significant, because coords dispatch in parallel and every consumer is coord-keyed.
- Replay persistence never gates an accepted live transfer: a capture failure invalidates recording state instead, and the accepted batch still completes.
- Server replay fixture controls and observations live in the engine Agent's Replay-bound slot (`../Agent/AGENTS.md`), while ordinary recording, playback, persistence, transfer capture, and generation storage stay here. The existing writer state and storage are public only so that fixture code can select and alter a retained generation for its debug controls; do not expose reader state or move replay algorithms with them.
- Whether playback is running is one engine-owned flag on `GameBase`, not a query over the reader maps, because engine and game code on the tick path read it every update. Republish it after every live or pending reader-set change, including the clears on abort, retirement, and loop transition.
- A later generation for a coord activates only after the retiring reader's coord has been removed. Adjacent lifetimes may meet at the tick after the prior saved end, while overlap or a skipped activation aborts playback. Loop only once the last reader has retired.
- Loop completion and abort both stop the current fixed-tick iteration before dispatch, then diverge: completion reloads the initial state, abort restores the pre-tick clock and resumes live simulation.

## See Also

- Game persistence (`../../../Projects/BrokenEngineSandbox/Source/Save/AGENTS.md`)
