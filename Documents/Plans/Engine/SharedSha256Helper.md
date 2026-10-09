<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-09T15:35:36.040Z","dependsOn":[]} -->
# Replace the engine and DataPacker SHA-256 hashers with one Common helper on the built-in CNG handle

## Context

Two programs carry their own SHA-256 wrapper over Windows CNG:

- Engine: the file-local `Sha256Hasher` in `Engine/Source/File/FileManager.cpp:45-108` opens a provider with `BCryptOpenAlgorithmProvider`, queries `BCRYPT_OBJECT_LENGTH`, allocates its own hash object, and records setup failure in `mbValid`; `Update` (`:90-95`) and `Finish` (`:97-100`) return `false` when setup failed or CNG fails. `FileManager::ComputeSha256` (`:280-299`, declared `FileManager.h:203`) feeds a span in `ULONG`-sized pieces and returns `bool`; `FileManager::ComputeOrdinaryFileSha256` (`:301-357`, declared `FileManager.h:204`) streams a file through the hasher in 64 KiB reads and returns `false` on a file failure (attributes, open, `GetFileInformationByHandle`, `ReadFile`) or a hasher failure (`:339-342`, `:351-354`).
- DataPacker: `DataPacker/Source/InputFingerprint.cpp:6-74` keeps a process-lifetime provider handle (`GetSha256Algorithm`, `:8-20`) and its own `Sha256Hasher` that throws `std::runtime_error` on any CNG failure, with `Update(std::span<const std::byte>)`, `Update(std::string_view)`, and a hex-string `Finish`. Its users are `HashFileContents` (`:76-136`) and the directory fingerprint (`:388-397`).

Windows already provides a pre-opened SHA-256 algorithm handle, `BCRYPT_SHA256_ALG_HANDLE`, declared in the Windows Kits `bcrypt.h` under `NTDDI_VERSION >= NTDDI_WINTHRESHOLD`, as is the one-shot `BCryptHash`. `Common/ExternalHeaders.h:40` targets `_WIN32_WINNT_WIN10`, and `Common/ExternalHeaders.h:323-327` already includes `<bcrypt.h>` and links `bcrypt.lib` for every `BT_ENGINE` or `BT_DATA_PACKER` build, so the client, the server, and DataPacker all link it today.

With that handle, provider opening, object-length queries, and hash-object allocation disappear, so the only failure source the engine's `bool` results model — hasher setup — goes away. `ComputeReplayGenerationDigest` (`Engine/Source/File/Replay.cpp:149-163`) fails only when `ComputeSha256` fails, so its `bool` result and the checks of it at `Replay.cpp:522` and `:903` become dead.

Origin: the user asked, of DataPacker's and the engine's SHA-256 code, "Shouldn't this be moved to common:: if both are using?", and of using the built-in CNG handle, "Removing code to use existing is always good, follow-up plan?" (both quoted verbatim by the authoring agent from the session that completed `Documents/Plans/Engine/FileOverEngineeringCleanup.md`). That session deferred the hasher rework out of its scope.

## Design

Author's recommendation, with rationale (`Common/AGENTS.md` places code more than one program needs in `common::`; `Common/WindowsUtils.h` is the shared Win32 wrapper surface and `Common/WindowsUtils.cpp` is already compiled into DataPacker, the client, and the server, so no project membership changes):

0. Before step 1, run `/verify-external-claims` on "with `BCRYPT_SHA256_ALG_HANDLE`, `BCryptHash`, `BCryptCreateHash` with a null hash object, `BCryptHashData`, and `BCryptFinishHash` with a 32-byte output return a failure status only for invalid arguments on Windows 10 and later". If it is not VERIFIED, stop and return the Plan for re-planning: the `bool` results in step 3 would then still carry a real failure.
1. In `Common/WindowsUtils.h`/`.cpp`, add `common::Sha256(std::span<const std::byte> bytes)` returning `std::array<uint8_t, 32>` through one `BCryptHash(BCRYPT_SHA256_ALG_HANDLE, ...)`, and a non-copyable `common::Sha256Hasher` whose constructor calls `BCryptCreateHash(BCRYPT_SHA256_ALG_HANDLE, &hash, nullptr, 0, nullptr, 0, 0)`, with `Update(std::span<const std::byte>)`, `Update(std::string_view)` (moved from DataPacker unchanged), `Finish()` returning `std::array<uint8_t, 32>` through `BCryptFinishHash`, and a destructor calling `BCryptDestroyHash`. Each CNG result is checked with `VERIFY_SUCCESS(BCRYPT_SUCCESS(status))`, gated on step 0. `Update` keeps the existing empty-span skip.
2. `common::Sha256` takes its size as one `ULONG`; `ASSERT(std::in_range<ULONG>(bytes.size()))` before the narrowing, because a silent narrowing would hash a truncated buffer instead of crashing. Its one caller is the replay root preimage.
3. Engine: delete the file-local `Sha256Hasher` and `FileManager::ComputeSha256` (declaration and definition). `ComputeReplayGenerationDigest` calls `common::Sha256(rootPreimage)` and returns the digest instead of `bool`; `Replay.cpp:522` compares the returned digest with `generationDigest`, and `:903` drops the `ComputeReplayGenerationDigest` term from its chain, passing the returned digest to `PublishReplayManifest`. `ComputeOrdinaryFileSha256` uses `common::Sha256Hasher`, keeps its `bool` result and every file-failure return, and loses only the two hasher-failure branches (`:339-342`, `:351-354`).
4. DataPacker: delete `GetSha256Algorithm` and the local `Sha256Hasher`; `HashFileContents` and the directory fingerprint use `common::Sha256Hasher`, with the existing hex loop (`:62-68`) kept as a file-local function over the returned array. A CNG failure now throws through `common::Assert` (`Common/ErrorUtils.cpp`), still a `std::runtime_error`, so the export-job aggregate catch is unchanged.

Bytes hashed must not change anywhere. Keep every call site's exact byte sequence: in particular `hasher.Update("\r\n")` (`InputFingerprint.cpp:133`) and the `std::string_view("\0", 1)` separators (`:393`, `:395`) must keep resolving to the `string_view` overload; `std::as_bytes(std::span("\r\n"))` would add the terminating NUL and change every text-mode fingerprint.

## Critical files

- `Common/WindowsUtils.h`, `Common/WindowsUtils.cpp`
- `Engine/Source/File/FileManager.cpp`, `Engine/Source/File/FileManager.h`
- `Engine/Source/File/Replay.cpp`
- `DataPacker/Source/InputFingerprint.cpp`

## In scope

- New `common::Sha256` and `common::Sha256Hasher` in `Common/WindowsUtils.h`/`.cpp`.
- `FileManager.cpp`: deleting the file-local `Sha256Hasher` and `FileManager::ComputeSha256`; the hasher use and the two hasher-failure branches in `FileManager::ComputeOrdinaryFileSha256`. `FileManager.h:203` (the `ComputeSha256` declaration).
- `Replay.cpp`: `ComputeReplayGenerationDigest`'s return type and body, and its two call sites (`:522`, `:903`).
- `InputFingerprint.cpp`: deleting `GetSha256Algorithm` and the local `Sha256Hasher` (`:6-74`); the hasher uses in `HashFileContents` and the directory fingerprint; a file-local hex formatter for the digest.
- Comments the change makes false, including `Common/ExternalHeaders.h:324` if it still names only `FileManager`.

## Out of scope

- `BuildExpectedReplayInventory` (`Replay.cpp:165-208`) and its `bool` result: it also fails on duplicate inventory entries and on file-hash failures, which remain.
- The file-failure checks in `ComputeOrdinaryFileSha256`; the replay manifest layout, its domain string, and `kiReplayManifestVersion`; the DataPacker fingerprint cache format, `kiPersistentFingerprintVersion`, and `kiFingerprintCacheVersion`.
- `Tools/ToolCommon/CoordinationStore.cpp` `HashSha256`: the AgentTools do not compile Common, and tool-only code belongs in `Tools/ToolCommon` (`.agents/references/cpp-conventions.md` code placement).
- Any other DataPacker or Engine hashing, CRC, or file I/O.

## Risk tier and invariants

Expected Tier 3 (trigger: a change spanning independently owned subsystems — Common, Engine File, and DataPacker — that touches replay-manifest digest computation and DataPacker fingerprint identity).

- Replay generation digests and file content digests stay byte-identical, so recordings made before the change still verify; no replay version changes.
- DataPacker fingerprints stay byte-identical, so no fingerprint or cache version changes. If the fix session finds any hashed byte must change, it bumps the owning version instead (no backward compatibility) and returns the Plan for re-planning first, because that forces a full re-export.
- `bcrypt.lib` linkage is already provided for all three executables by `Common/ExternalHeaders.h:326`; no project file changes.

## Acceptance criteria

- `/verify-external-claims` returns VERIFIED for the step 0 claim before any `VERIFY_SUCCESS` relies on it.
- DataPacker, the client, and the server build through `/compile`.
- A warm-cache DataPacker run over unchanged data exports nothing (`DataPacker/Source/AGENTS.md` describes the warm-cache check), which shows the fingerprints are unchanged.
- Through `/agent-harness`, a server replay recorded by the baseline build plays back under the changed server build with its manifest digest check passing, and a replay recorded by the changed build plays back too.
- No `BCryptOpenAlgorithmProvider`, `BCRYPT_OBJECT_LENGTH` query, or `Sha256Hasher` class remains in `Engine/` or `DataPacker/`.

## Notes

- `/verify-external-claims` is required, not optional: whether the built-in handle's calls can fail for valid sizes was not verified when this Plan was written.
- The engine's current `ComputeSha256` piece loop exists only because `BCryptHashData` takes a `ULONG` size; step 2's `ASSERT` replaces it for the one buffer caller.
