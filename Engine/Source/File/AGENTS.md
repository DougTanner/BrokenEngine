# File - Runtime I/O and Packed Assets

`FileManager` (`gpFileManager`) owns platform paths, versioned atomic file
writes, and packed-asset access through `PackChunks`, a private non-manager
engine outside the aggregation header. `Replay` (`gpReplay`) owns server record
and playback lifetime. `GridSave.h` and `Replay.h` are the only headers here
that include a game header, deliberately. `DifferenceStream.h` names game frame
types, so it stays out of the aggregation header and its users include it
directly.

## File Contracts

Each process resolves its asset data root and user AppData root once, at
`FileManager` construction. Struct-layout files share one version header;
one-shot writes are atomic.

- Client and server launches must use the same data root; neither process can
  check it.
- Code reachable during `FileManager` construction must not consume
  `gpFileManager`: the global is already set while platform directory setup and
  `mpPackChunks` construction run.
- Directory creation and required boot-asset failures throw
  `std::runtime_error`; catch it only around `FileManager` construction and
  eager-load completion, rethrowing `std::system_error` first; one over
  `MainThread` misclassifies unrelated runtime failures as startup exits.
- Report failure detail through out-references beside a success return or
  success out-param; never store a failure reason for a later query.

## Grid Saves

Server-only. The whole simulation grid is one versioned file, written atomically
in sorted coord order so equal state produces equal bytes, and read into an
isolated staged snapshot adopted live only after the whole stream validates. The
file is a trust boundary: `ReadGridSave` bounds, cross-checks, and rejects bad
adopted values
([C++ conventions](../../../.agents/references/cpp-conventions.md)). The
snapshot carries the game's payload half by value, uninspected; the game side
owns it (game Network Server: the game project's own
`Source/Network/Server/AGENTS.md`), and the two stage sets change together.

- A save stores no placements: `ReadGridSave` regenerates each cell's list from
  its saved coordinate, as a fresh cell does, so no placement value crosses this
  trust boundary and no list-order check runs on load. The version rule this
  creates is in [Frame](../Frame/AGENTS.md) `## Terrain and Navigation`.
- `ServerSaveLoad` (`GameBase::mServerSaveLoad`) owns when a save, load, reset,
  or autosave runs and what happens around it, and returns the actual commit
  result to callers. A successful load keeps the saved simulation clock and
  resynchronizes clients to that state; a reset clears the game save state
  explicitly, since a fresh game has no save to restore it from. Load and reset
  discard pending raw profile samples before mutating or restoring frame state;
  this diagnostic cleanup does not alter the persisted payload.

## Packed Assets

Eager types load whole at startup; lazy chunks load on demand on loader threads
publishing chunk state with release/acquire. The server opens only the pack
types it consumes.

- Keep the eager/lazy split and the server's accepted set aligned with what
  DataPacker emits; nothing at runtime checks that split.
- Eager acquisition checks file size, open, and complete read before validating
  or publishing chunks. The eager-load future carries failure: every lazy chunk
  wait consumes it before waiting on loader threads, and a startup path
  returning before the first such wait consumes it explicitly, so a failed task
  never passes as successful startup.
- `WaitForChunks` blocks until a chunk is ready, but an already queued request
  is not reprioritized.
- Chunk reset locks nothing; the caller owns exclusion. Device loss drains
  accepted whole-chunk and streaming-audio range work before resetting texture
  state; island eviction drains the renderer's Vulkan descriptor window. Keep
  the two in step, and settle exclusion before adding a third.
- Sorting, filtering, or re-emitting the ordered Islands or Model chunk table
  changes the client's handshake pack integrity token
  ([Network](../../../Documents/Architecture/Network.md)): every client fails to
  connect, with an error misleadingly blaming the data build.
- `EagerChunk.iDataSize` is the chunk table's raw on-disk extent, not the chunk
  header size, which excludes a scene chunk's appended animation section and a
  model chunk's outline and hardpoint sections. Bound eager reads by
  `iDataSize`, or that data silently truncates. `ValidateChunkHeader` admits a
  model chunk at pack open only uncompressed and with its outline and hardpoint
  sections inside the chunk.
- Corrupt runtime `.pack` chunks and pack-backed textures halt both processes,
  even at call sites outside this subsystem: validate and `ASSERT`; never
  soft-fail, skip, or substitute a placeholder.
- A new publish path validates first (as `ValidateChunkLocation` and
  `ValidateChunkHeader` do): once a pointer, slice, buffer, or map entry
  exists, a corrupt value already addresses memory outside the pack.
- Cross-pack references are unvalidated at open and the lazy-chunk map is fixed
  at construction, so packs from different DataPacker runs can name a chunk
  absent for the whole process lifetime.
- File owns all background packed-asset reads: two below-normal loader threads
  service whole chunks and client streaming-audio ranges. Real-time queued work
  wins; audio and other work then alternate so neither starves. Publish every
  accepted job and shutdown through the atomic wake sequence.
- Client streaming-audio reads use six fixed Pack-owned 16 KiB result entries.
  The caller polls an in-flight read by its request handle alone, since the
  entry holds the range; reset or destruction cancels it. Generation-tagged
  ownership makes cancelled queued and ready entries reusable at once, while an
  active loader keeps its entry until it acknowledges cancellation. File must
  outlive Audio.
- Validate a streaming-audio request against its logical chunk payload before
  publishing: pack open rejects compressed audio, so that payload is the on-disk
  extent bounded at open. A lazy pack is locked against writes from before its
  size and header reads until shutdown but shares delete, so DataPacker can
  replace its name while the open file stays unchanged; every read, validation
  included, uses that handle, not the path, so the loader checks only for a
  complete read, before discarding a cancelled generation: corruption
  stays fatal even for an unwanted result. Loaders write only Pack-owned result
  storage; the polling main thread copies to Audio.
- The [Agent audio harness rig](../Agent/AGENTS.md) may inspect Pack state and
  use loader-queue synchronization only in gameplay client Debug builds;
  elsewhere that state stays private.
- Each lazy chunk owns its CPU buffer, allocated through mimalloc
  ([Memory](../Memory/AGENTS.md)). A loader thread allocates it at the
  whole-chunk load; texture adoption frees it on the main thread before the
  `kReady` store, so a `kReady` texture holds none; island and audio buffers
  live for the process, because island templates and XAudio2 buffers point into
  them. Readers use the pointer only after the state acquire, with no lock.
  `PackChunks::AllocateLazyChunkBuffer` and `FreeLazyChunkBuffer` are the only
  allocate and free, and keep atomic live and peak counts of requested bytes
  and estimated mimalloc arena footprint; the Profile CPU screen's `Lazy:` line
  shows the peak requested bytes, and its `Lazy arena:` line the live and peak
  footprint.

## Replay Streams

Server-only. Recording writes one stream set per active cell plus recording-wide
grid and metadata; playback validates the whole set before replacing live state,
then resimulates against recorded tick checksums.

- Replay operations exist only when `kbDebugInput` enables their tick-time
  implementation, gated in this subsystem's replay entry points.
- As a debug-only developer tool, replay streams are exempt from trust-boundary
  validation: existing replay checks may stay; no new ones are required.
- A generation is one activation lifetime, identified in its stream files and
  manifest record by coord and activation tick.
- The manifest is the commit marker: recording start invalidates it before
  replacing any component, and stop publishes it only after every generation and
  the game metadata succeed.
- The manifest carries the next-global-ID counter as of recording stop, and
  every playback adoption, each loop included, assigns it right after adopting
  the grid, so the counter stays ahead of every replayed global ID during
  playback and after an abort.
- Missing retained terminal state for an evicted coord invalidates only that
  generation's files; other writers and metadata still attempt their writes.
- The in-memory active-coord list is not order-significant: coords dispatch in
  parallel and every consumer is coord-keyed.
- Replay persistence never gates an accepted live transfer: a capture failure
  invalidates recording state instead, and the accepted batch still completes.
- Replay harness rig controls and observations live in the Agent's Replay-bound
  slot (`../Agent/AGENTS.md`); writer state and storage are public only for it
  to select and alter a retained generation. Keep reader state private and
  replay algorithms here.
- Whether playback is running is one engine-owned `GameBase` flag because
  tick-path engine and game code read it every update. Republish it after every
  live or pending reader-set change, including the clears on abort, retirement,
  and loop transition.
- Recording and playback are mutually exclusive, a pending request counting as
  active: every path that sets a record-start or playback request refuses it,
  before changing anything, while the running-state or pending-request flags
  show the opposite mode. `Replay`'s `ASSERT`s on consuming those requests catch
  a skipped check; they are not recovery.
- A coord's later generation activates only after the retiring reader's coord is
  removed. Adjacent lifetimes may meet at the tick after the prior saved end;
  overlap or a skipped activation aborts playback. Loop only after the last
  reader retires.
- Loop completion and abort both stop the current fixed-tick iteration before
  dispatch, then diverge: completion reloads the initial state, abort keeps the
  current frames, restores the pre-tick clock, and resumes live simulation.
  Abort also runs the game state-replacement hook (the game project's own
  `Source/Save/AGENTS.md` `## Replay Contract`).
- The game keeps replay state the frames do not hold through calls at recording
  start, on each playback tick, and after each recorded or replayed tick
  completes (the same game `## Replay Contract`). The playback call comes after
  every abort and loop-completion return, so it runs only on a tick that
  dispatches and an abort leaves that state matching the kept frames.
