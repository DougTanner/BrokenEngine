<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-08T23:17:02.793Z","dependsOn":[]} -->
# Remove over-engineered checks in File

## Context

A repo-wide over-engineering (YAGNI) sweep looked for useless hashing, excessively defensive checks, fallbacks for cases that cannot happen, and ultra-rare edge-case protection that should be a plain operation plus an `ASSERT` or a hard error. A low-effort finder model flagged candidates file by file, and one validation pass by a stronger model confirmed or rejected each against the code, citing callers and invariants. The main session spot-checked only two of the 27 validation groups. This Plan carries the 17 confirmed candidates under `Engine/Source/File/**`, with line numbers as of commit `7514890c`. The sweep's working files were session-local and are gone, so every fact the executor needs is restated below.

`.agents/references/cpp-conventions.md` (first bullet) assumes parameters from within the codebase are valid and puts validation only at trust boundaries; each candidate below is a check or loop inside our own code that the validator found guarded by an earlier check, by every caller, or by an API contract.

**Every candidate is unverified input, not an approved change.** The validator confirmed about 95% of what it saw, and only two groups were spot-checked. When this Plan was recorded, the user asked that the executing agent not trust these recommendations blindly.

## Design

### Per-candidate verification

For each candidate, in order:

1. Re-locate the code by its function and quoted condition; line numbers will have drifted.
2. Independently prove the claim against current code: cite the invariant (the earlier validation that already rejected the value, the single writer, the API contract) or enumerate every caller. Do not reuse the validator's evidence without re-reading it.
3. Apply the proposed simpler form only when step 2 proves it. Where the bound comes from a value read from a file, even one validated earlier, the author recommends the `ASSERT` form over plain deletion. `ASSERT` (`Common/ErrorUtils.h:20`) is active in every build configuration, so it turns a wrong proof into an immediate hard failure, never silent undefined behavior or an out-of-bounds read.
4. When the proof fails, or the change would touch a boundary listed below, drop the candidate and report it with the reason in the completion summary. A dropped candidate is a normal outcome, not a failure of this Plan.

### Boundaries that must never be removed

- Server validation of every client-to-server record and every grid save, and the rejection of bad values from packets or files under the bad-value rule in `.agents/references/cpp-conventions.md`; owners: `Engine/Source/Network/AGENTS.md` `## Corrupt Input Policy`, `Engine/Source/File/AGENTS.md` `## Grid Saves`.
- The per-tick determinism CRC and everything feeding it.
- `.pack`, manifest, save, and replay format and version checks — in this Plan, specifically `ValidateChunkLocation` and `ValidateChunkHeader` in `PackChunks.cpp`, the pack-handle open check (`:433-439`), the real overrun check at `PackChunks.cpp:1022`, and the manifest version and format checks in the replay read path. Several candidates rely on these checks having already run.
- Win32 and other OS or third-party API result checks: the `ReadFile`, `BCryptHashData`, `GetFileSizeEx`, `CreateFileW`, and `VirtualAlloc` result checks stay.

Converting a check into an `ASSERT` is an acceptable outcome for any candidate. Replay streams are a developer tool exempt from trust-boundary validation (`Engine/Source/File/AGENTS.md` `## Replay Streams`).

### Candidates

**`FileManager.cpp` — SHA-256 hashing.**

1. `Engine/Source/File/FileManager.cpp:346-349` — `FileManager::ComputeOrdinaryFileSha256`: `iBytesRead > buffer.size()` after `ReadFile`. `ReadFile` never reports more than `nNumberOfBytesToRead`, which is `buffer.size()` (`:341`). Simpler form: delete, or `ASSERT(iBytesRead <= std::ssize(buffer));`.
2. `FileManager.cpp:350-353` — same function: int64 overflow guard on the running byte count. Overflow needs a file of about 2^63 bytes, beyond the NTFS maximum. Simpler form: delete.
3. `FileManager.cpp:92` — file-local `Sha256Hasher::Update`: `std::in_range<ULONG>(bytes.size())`. Its two callers bound the size: `ComputeSha256` (`:294`) caps each chunk at `ULONG` max, and `ComputeOrdinaryFileSha256` (`:354`) passes at most its 64 KiB buffer (`:336`). Simpler form: `ASSERT(std::in_range<ULONG>(bytes.size()));` before the return and `return mbValid && ...` without that term.

**`PackChunkLoader.cpp` — loader wake.**

4. `Engine/Source/File/PackChunkLoader.cpp:323-331` — `PackChunkLoader::PublishWake`: a compare-exchange retry loop plus a wrap `ASSERT` around a counter increment. `mWakeSequence` is written only here; its readers (`:220`, `:280`) only load it and `wait` on it for inequality; 2^64 wakes are unreachable. Simpler form: `mWakeSequence.fetch_add(1, std::memory_order_release);`, keeping `mWakeSequence.notify_all();`. The executor confirms that release ordering and the reader's acquire pairing are unchanged.

**`PackChunks.cpp` — lazy chunk reads.**

5. `Engine/Source/File/PackChunks.cpp:696-700` — `PackChunks::TryReadChunkData` polling path: `miEntryIndex >= ssize(mAudioReadEntries)`. The branch runs only when `mpPackChunks == this`; the sole writer of a non-null `mpPackChunks` (`:824-825`) pairs it with a loop index in `[0, ssize)`; every clear path (`ClearRequest` `:679-687`, `CancelChunkRead` `:884-889`) resets both fields together; the Debug-public fields (`FileManager.h:172-177`) are only read by `AudioStreamingHarnessRig.cpp:708-728`. Simpler form: `ASSERT(rRequest.miEntryIndex < std::ssize(mAudioReadEntries));`.
6. `PackChunks.cpp:855-858` — `PackChunks::CancelChunkRead`: the same guard; `:851` returns unless `mpPackChunks == this`. Simpler form: as candidate 5.
7. `PackChunks.cpp:791-794` — `TryReadChunkData`: `location.uiSize < kiChunkDataOffset` re-check. The only `LazyChunk` insertion (`:350`) runs after `ValidateChunkLocation` (`:332`) rejected that case with a fatal throw (`:172-175`), and `location` is never assigned anywhere else. Simpler form: delete.
8. `PackChunks.cpp:970-977` — `PackChunks::LoadAudioRead`: `hFile == nullptr` / `INVALID_HANDLE_VALUE` checks. Every lazy pack handle is opened at `:433-439` with a fatal throw on `INVALID_HANDLE_VALUE` (`CreateFileW` never returns null on failure), `DataTypeFromFlags` equals the opened pack's type by `ValidateChunkHeader` (`:196`), handles close only in the destructor (`:72`), and the mirror whole-chunk path `PackChunkLoader.cpp:363` already uses the handle unchecked. Simpler form: delete, or one `ASSERT(hFile != nullptr && hFile != INVALID_HANDLE_VALUE);`.
9. `PackChunks.cpp:983-990` — `LoadAudioRead`: overflow tests on `uiDataFileOffset` and `uiOffset + uiDataFileOffset`. `location.uiOffset <= packFileSize` (`:176`), and `rEntry.iOffset` is written only at `:822` after `:782-803` bounded it to `[0, iPackDataSize]`. Simpler form: delete.
10. `PackChunks.cpp:994-997` — `LoadAudioRead`: `uiLength + uiPrefix` overflow; `uiLength <= 16 KiB` (`:767-770`) and `uiPrefix` is below the sector size. Simpler form: delete.
11. `PackChunks.cpp:999-1006` — `LoadAudioRead`: `uiPhysicalSize > miReadBufferSize` and `uiAlignedOffset + uiPhysicalSize` overflow. `uiPhysicalSize <= RoundUp(sector - 1 + 16 KiB, sector)` while `miReadBufferSize = RoundUp(256 KiB + sector, sector)` (`:444`); the first still depends on an OS-reported sector size (`GetDiskFreeSpaceW`, `:413`), so it stays as an `ASSERT`. Simpler form: `ASSERT(uiPhysicalSize <= static_cast<uint64_t>(miReadBufferSize));` in place of both.
12. `PackChunks.cpp:1008-1011` — `LoadAudioRead`: `uiLength + uiLogicalFileOffset` overflow; both terms are bounded as above. The real overrun check at `:1022` stays. Simpler form: delete.
13. `PackChunks.cpp:1018-1021` — `LoadAudioRead`: `fileSize.QuadPart < 0` after a successful `GetFileSizeEx`. The failure check at `:1014-1017` stays; `std::cmp_greater` at `:1022` is sign-safe. Simpler form: delete.
14. `PackChunks.cpp:369-376` — `PackChunks::LoadPackFiles`: int64 overflow guards on aligned `iDataSize` and the cumulative pool offset. `iDataSize` is bounded by the real pack size from `tellg` (`:176-183`) or `INT_MAX` (`:246`, compressed); unique CRCs (`:300-303`) matched against each header's CRC (`:200`) force distinct chunk offsets; an oversized pool already fails at the checked `VirtualAlloc` (`:383-387`). File-derived sizes, so the author recommends `ASSERT`s, not deletion. Simpler form: `ASSERT(rLazyChunk.iDataSize <= std::numeric_limits<int64_t>::max() - (common::kiAlignmentBytes - 1));` and `ASSERT(iPoolOffset <= std::numeric_limits<int64_t>::max() - iAlignedDataSize);`.

**`Replay.cpp` — manifest digest (writer side; the digest bytes do not change).**

15. `Engine/Source/File/Replay.cpp:115-124` — `AppendReplayManifestPayload`: `in_range<int64_t>` and `SIZE_MAX`-derived overflow guards on `records.size()` and `inventory.size()`. Both are live `std::vector`s whose elements are at least as large as their serialized form (16 bytes per record, 57 per inventory entry), so no allocatable vector reaches the bounds; callers are `Replay.cpp:553` and `:934`. Simpler form: delete both `if` blocks, keeping `kuiFixedBytes` for the `reserve`; the function can then return `void`, and the check of its result in `ComputeReplayGenerationDigest` (`:163-166`) becomes a plain call.
16. `Replay.cpp:167-170` — `ComputeReplayGenerationDigest`: `payload.size() > SIZE_MAX - 4 - domain.size()` on an in-memory vector (domain is 43 bytes, static_assert at `:91`). Simpler form: delete.
17. `Replay.cpp:185-188` — `BuildExpectedReplayInventory`: `records.size() > (SIZE_MAX - 2) / 4` on a resident vector of at least 16-byte records. Simpler form: delete the `if` and its `return false;`.

## Critical files

- `Engine/Source/File/FileManager.cpp`
- `Engine/Source/File/PackChunkLoader.cpp`
- `Engine/Source/File/PackChunks.cpp`
- `Engine/Source/File/Replay.cpp`

## In scope

Only the exact checks, loops, and statements named in candidates 1-17, in the functions named there, plus the return-type change of `AppendReplayManifestPayload` and its one checked call site that candidate 15 makes dead, and only the candidates the executor proves. Comment edits that a removed check makes false are in scope.

## Out of scope

- Every boundary listed under `### Boundaries that must never be removed`.
- Any check, branch, or file not listed as a candidate, including similar-looking sites the executor notices; report those as residuals instead.
- `.pack`, manifest, grid-save, and replay formats and their `kiVersion` constants; no format changes.
- `PackChunkLoader` threading beyond the one `PublishWake` rewrite: queue, mutex, shutdown, and reader wait logic.
- `DataPacker/Source/FileManager.cpp`, which the same sweep covered separately.

## Acceptance criteria

- The completion summary lists every candidate 1-17 as applied, with the executor's own proof (invariant, API contract, or every caller cited against current code), or dropped, with the reason.
- No boundary from `### Boundaries that must never be removed` is weakened.
- Client and server build clean in Debug and Release through `/compile`.
- A live `/agent-harness` run boots the client with lazy pack loading and audio streaming active, and a replay record-then-playback determinism check passes, with no `ASSERT`.

## Notes

- Risk tier: Tier 3 (`.agents/references/risk-tiers.md`). Triggers: threading — candidate 4 rewrites the cross-thread wake publication of the pack chunk loader; trust boundary — candidates 5-14 sit on the `.pack` read path, where a wrong proof would change what a file read trusts. No determinism/CRC, wire, or format change is intended.
- No other live Plan edits these functions.
