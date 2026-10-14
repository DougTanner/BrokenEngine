<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-13T13:09:21.632Z","dependsOn":[]} -->
# Move the replay manifest decoder out of SaveLoadReplay into a file-static reader beside its encoder

## Context

`Replay::SaveLoadReplay` (`Engine/Source/File/Replay.cpp:354-710`) decodes and self-validates `F7.replay.manifest` inline at `Replay.cpp:379-524`: open, version, initial tick, records, fullframes flag, inventory, generation digest and trailing data, then the inventory shape and identity checks and the generation digest comparison. Each field carries its own rejection, so this block is a large share of the function's branch count. The write side is already split: the encoder `AppendReplayManifestPayload` (`Replay.cpp:113-147`) and `PublishReplayManifest` (`:218-228`) are file-static functions, so the read and write sides of the same format are not parallel.

Origin: a read-only review of the top 20 functions by complexity mass at `43450112`. The user's governing rule, as relayed by the dispatching agent: a function is never split only because it is big; only a piece that is a separate job fused into its function moves. The manifest decoder was judged such a piece; the rest of `SaveLoadReplay` (stream staging, grid and metadata adoption, the catch) was judged earned and stays.

Verified at `43450112`:

- The block reads no `Replay` member. Its inputs are `engine::gpFileManager` and file-static helpers (`ReadReplayManifestValue`, `ReplayManifestRecordLess`, `ReplayInventoryEntryLess`, `BuildExpectedReplayInventory`, `ComputeReplayGenerationDigest`, `common::ValidateDeserializedCount`); its outputs are `manifest` and `generationDigest`.
- After `:524`, `SaveLoadReplay` reads only `manifest` (`:526`, `:536`) and the local `iInitialTick` (`:564`, `:606`, `:623`, `:629`, `:639`, `:659`, `:669`); `generationDigest` has no reader after the comparison at `:522`.
- Its two non-throwing exits (missing manifest at `:380-385`, version mismatch at `:391-396`) log, call `ReplayHarnessRigs::Reset(*this)`, and return. Every other rejection throws `std::ios_base::failure` (or `common::ValidateDeserializedCount` throws), which the catch at `:694-708` handles.

## Design

Author's recommendation, with rationale:

1. Add `static bool ReadReplayManifest(ReplayManifest& rManifest)` in `Replay.cpp` directly after `PublishReplayManifest`, the encoder's write-side counterpart; it calls `BuildExpectedReplayInventory` and `ComputeReplayGenerationDigest`, which are defined above it.
2. Its body is `Replay.cpp:379-524` moved unchanged except for renaming `manifest` to `rManifest` and dropping the declaration `ReplayManifest manifest;` at `:398`, which the `rManifest` parameter replaces. It returns `false` on the two non-throwing exits after their existing `LOG` lines, keeps every `throw` as is, and returns `true` after the generation digest check.
3. The generation digest stays a local of the reader. The reviewer's suggested `std::array<uint8_t, 32>& rGenerationDigest` out-parameter is dropped because, once the comparison at `:522` moves in with the decode, the caller has no use for the digest.
4. `SaveLoadReplay` replaces `:379-524` with `ReplayManifest manifest; if (!ReadReplayManifest(manifest)) { ReplayHarnessRigs::Reset(*this); return; }` followed by `int64_t iInitialTick = manifest.iInitialTick;`, so the later readers of `iInitialTick` stay untouched. The `:378` comment stays above the call.
5. The artifact-file digest loop (`:526-535`) stays in `SaveLoadReplay`: it hashes component files against the manifest rather than reading the manifest.

The manifest stream now closes when the reader returns rather than at the end of the `try` block. Nothing later in `SaveLoadReplay` opens or writes the manifest, so the order of observable effects is unchanged.

## Critical files

- `Engine/Source/File/Replay.cpp`

## In scope

- New file-static `ReadReplayManifest` in `Replay.cpp`, after `PublishReplayManifest`.
- `Replay::SaveLoadReplay`: replacing the manifest decode and self-validation block (`Replay.cpp:379-524` at `43450112`) with the call and the `iInitialTick` local.

## Out of scope

- Every other line of `SaveLoadReplay`, including the artifact-file digest loop, stream staging, grid and metadata adoption, and the catch.
- The redundant size compare at `Replay.cpp:510` (`std::ranges::equal` at `:514` already rejects a length difference): it moves with the block unchanged, because `Engine/Source/File/AGENTS.md` `## Replay Streams` lets existing replay checks stay.
- `AppendReplayManifestPayload`, `PublishReplayManifest`, `InvalidateReplayManifest`, `BuildExpectedReplayInventory`, `ComputeReplayGenerationDigest`, and `ReadReplayManifestValue`.
- The manifest layout, its domain string, and `kiReplayManifestVersion`.

## Risk tier and invariants

Expected Tier 1 (trigger: a local, behavior-preserving extraction into a file-static function with no public signature; the replay serialization surface is not altered).

- No format change: the same bytes are read in the same order, every rejection keeps its message and its exit kind (log-and-return or throw), and `kiReplayManifestVersion` does not move.
- Server-only (`BT_SERVER`); no determinism/CRC, wire, threading, or trust change.

## Acceptance criteria

- `SaveLoadReplay` no longer contains `ReadReplayManifestValue` calls; `ReadReplayManifest` contains every manifest rejection message that `Replay.cpp:379-524` contained at `43450112`, unchanged.
- The server builds through `/compile`.
- Through `/agent-harness`, a replay recorded and played back by the changed server passes the `Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md` `### Replay determinism acceptance` route, showing the manifest still validates.

## Coordination

- `Documents/Plans/Engine/SharedSha256Helper.md` edits the generation digest comparison at `Replay.cpp:522`, which this Plan moves into `ReadReplayManifest`. Whichever lands second rebases onto the other's edits.

## Notes

- Line numbers are from `43450112`.
