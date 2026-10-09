# File - Runtime I/O and Packed Assets

`FileManager` (`gpFileManager`) owns platform paths, versioned atomic file writes, and packed-asset access through `PackChunks`, a private non-manager engine outside the aggregation header. `Replay` (`gpReplay`) owns server record and playback lifetime. `GridSave.h` and `Replay.h` are the only headers here that include a game header, deliberately.

## File Contracts

Each process resolves its asset data root and user AppData root once, at `FileManager` construction. Struct-layout files share one version header; one-shot writes are atomic.

- Client and server launches must use the same data root; neither process can check it.
- `FileManager` publishes `gpFileManager` only after platform directory setup and packed-asset construction succeed; code reachable during construction must not consume the global.
- Directory creation and required boot-asset failures log their exact path and reason once, then throw `std::runtime_error`. Handle that type only around `FileManager` construction and eager-load completion, with `std::system_error` rethrown first; widening it over `MainThread` misclassifies unrelated runtime failures as startup exits.
- Change an on-disk layout and its owning version together; only that version stops an old file being read as new.
- Report failure detail through out-references beside a success return or success out-param; never store a failure reason for a later query.

## Grid Saves

Server-only. The whole simulation grid is one versioned file, written atomically in sorted coord order so equal state produces equal bytes, and read into an isolated staged snapshot adopted live only after the whole stream validates. The file is a trust boundary: `ReadGridSave` bounds, cross-checks, and rejects bad adopted values ([C++ conventions](../../../.agents/references/cpp-conventions.md)). The snapshot carries the game's payload half by value, uninspected; the game side owns it ([game Network Server](../../../Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md)), and the two stage sets change together.

- A placement whose island CRC is absent from this build's island pack, whose centre lies outside the cell's base area, or whose rotation is non-finite aborts the load.

## Packed Assets

Runtime reads assets only from `.manifest`-described `.pack` files. Eager types load whole at startup; lazy chunks load on demand on loader threads that publish chunk state with release/acquire ordering. The server opens only the pack types it consumes.

- Keep the eager/lazy split and the server's accepted set aligned with what DataPacker emits; nothing at runtime checks that split.
- Eager acquisition checks file size, open, and complete read before validating or publishing chunks. The eager-load future carries failure: every lazy chunk wait consumes it before waiting on loader threads, and a startup path returning before the first such wait consumes it explicitly, so a failed task never passes as successful startup.
- `WaitForChunks` blocks until a chunk is ready, but an already queued request is not reprioritized.
- Chunk reset locks nothing; the caller owns exclusion. Device loss drains accepted whole-chunk and streaming-audio range work before resetting texture state; island eviction drains the renderer's Vulkan descriptor window. Keep the two in step, and settle exclusion before adding a third.
- Sorting, filtering, or re-emitting the ordered Islands chunk table changes the client's handshake pack integrity token ([Network](../../../Documents/Architecture/Network.md)): every client fails to connect, with an error misleadingly blaming the data build.
- `EagerChunk.iDataSize` is the chunk table's raw on-disk extent, not the chunk header size, which for scene chunks excludes the appended animation section. Bound eager reads by `iDataSize`, or animation data silently truncates.
- Corrupt runtime `.pack` chunks and pack-backed textures halt both processes, even at call sites outside this subsystem: validate and `ASSERT`; never soft-fail, skip, or substitute a placeholder.
- A new publish path validates first (as `ValidateChunkLocation` and `ValidateChunkHeader` do): once a pointer, slice, pool slot, or map entry exists, a corrupt value already addresses memory outside the pack.
- Cross-pack references are unvalidated at open and the lazy-chunk map is fixed at construction, so packs from different DataPacker runs can name a chunk absent for the whole process lifetime.
- File owns all background packed-asset reads: two below-normal loader threads service whole chunks, range reloads, and client streaming-audio ranges. Real-time queued work wins; audio and other work then alternate so neither starves. Publish every accepted job and shutdown through the atomic wake sequence.
- Client streaming-audio reads use six fixed Pack-owned 16 KiB result entries. The caller holds one request identity and exact range while polling; reset or destruction cancels it. Generation-tagged ownership makes cancelled queued and ready entries reusable at once, while an active loader keeps its entry until it acknowledges cancellation. File must outlive Audio.
- Validate a streaming-audio request against its logical chunk payload before publishing. The loader validates the physical pack extent and complete read before discarding a cancelled generation, since corruption stays fatal even for an unwanted result. Loaders write only Pack-owned result storage; the polling main thread does the final copy to Audio.
- The [Agent audio harness rig](../Agent/AGENTS.md) may inspect Pack state and use loader-queue synchronization only in client Debug builds; elsewhere that state stays private.

## Lazy-Pool Invariants

All lazy chunk data lives in one virtual-memory reservation laid out once at construction by cumulative aligned size over the full chunk map, so each chunk's pointer and size are fixed for the process lifetime and describe its reservation, not its resident bytes.

- Never compact those pointers or recompute the offsets: their fixedness lets a reader use a chunk pointer under the state acquire alone, with no lock.

## Replay Streams

Server-only. `Replay` owns writer and reader lifetime, the manifest committing a recording, staged playback adoption, and per-tick checksum comparison; `GameBase` constructs it and users reach it through `gpReplay`. Recording writes one stream set per active cell plus recording-wide grid and metadata; playback validates the whole set before replacing live state, then resimulates, comparing each tick against the recorded checksum.

- Replay operations exist only when `kbDebugInput` enables their tick-time implementation, gated in this subsystem's replay entry points; only the constant is game-defined.
- As a debug-only developer tool, replay streams are exempt from trust-boundary validation: existing replay checks may stay; no new ones are required.
- A generation is one activation lifetime, identified in its stream files and inventory by coord and activation tick.
- The manifest is the commit marker: recording start invalidates it before replacing any component, and stop publishes it only after every generation and the game metadata succeed.
- Missing retained terminal state for an evicted coord invalidates only that generation's files; other writers and metadata still attempt their writes.
- The in-memory active-coord list is not order-significant: coords dispatch in parallel and every consumer is coord-keyed.
- Replay persistence never gates an accepted live transfer: a capture failure invalidates recording state instead, and the accepted batch still completes.
- Server replay harness rig controls and observations live in the engine Agent's Replay-bound slot (`../Agent/AGENTS.md`); ordinary recording, playback, persistence, transfer capture, and generation storage stay here. Writer state and storage are public only so harness rig code can select and alter a retained generation; do not expose reader state or move replay algorithms with them.
- Whether playback is running is one engine-owned `GameBase` flag, not a reader-map query, because tick-path engine and game code read it every update. Republish it after every live or pending reader-set change, including the clears on abort, retirement, and loop transition.
- Recording and playback are mutually exclusive, a pending request counting as active: every path that sets a record-start or playback request refuses it, before changing anything, while the running-state or pending-request flags show the opposite mode. `Replay`'s `ASSERT`s on consuming those requests catch a skipped check; they are not recovery.
- A coord's later generation activates only after the retiring reader's coord is removed. Adjacent lifetimes may meet at the tick after the prior saved end; overlap or a skipped activation aborts playback. Loop only after the last reader retires.
- Loop completion and abort both stop the current fixed-tick iteration before dispatch, then diverge: completion reloads the initial state, abort keeps the current frames, restores the pre-tick clock, and resumes live simulation. Abort also runs the game state-replacement hook (`../../../Projects/BrokenEngineSandbox/Source/Save/AGENTS.md` `## Replay Contract`).
