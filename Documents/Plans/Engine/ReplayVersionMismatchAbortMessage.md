<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-13T12:21:09.830Z","dependsOn":[]} -->
# Report a version-rejected replay as a version mismatch, not as corrupt data

## Context

When a replay recorded by an older build is refused because `game::Frame::kiVersion` changed, the refusal is
correct but the user-visible abort message says the data is **corrupt**. Observed during acceptance
verification of `Documents/Plans/Frame/DestroyElementReverseWalkSkip.md`, which bumped the version constant:

```
  [Tick: 1717] DifferenceStreamReader SAVED_TYPE version mismatch: file 199 0, expected 200 568
  [Tick: 1717] SaveLoadReplay aborted: corrupt replay data: ReplayManifest stream bounds
```

The first line is accurate and comes from `Engine/Source/File/DifferenceStream.h:227`, where
`DifferenceStreamReader` fails `ReadAndValidateVersionHeader<SAVED_TYPE>` and returns early at `kWarning`. That
early return leaves `mbLoaded` false (`DifferenceStream.h:399` is the only place it is set true; `Loaded()` is
`:509-512`), so the reader is left in the same observable state as a genuinely truncated or damaged stream.

The second line is the misleading one. `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp:690-694`
checks `!pReader->Loaded() || GetStartTick() != iActivationTick || GetSavedEnd().interpolate.iTick <
iActivationTick` and, for all three failures alike, throws
`common::CorruptStreamException("ReplayManifest stream bounds")` (`Common/Serialization.h:13-17`). The catch at
`GameSaveLoad.cpp:799` then logs it verbatim as `SaveLoadReplay aborted: corrupt replay data: {}`. A version
mismatch — the one case that is expected, benign, and fully explained by the accurate `kWarning` two lines
above — is therefore surfaced with the wording reserved for damaged files.

Two facts make this worth recording rather than tolerating. First, the `kWarning` that carries the real reason
is below the default runtime threshold of `kInfo` (root `AGENTS.md` log-level contract), so an ordinary user or
an agent reading only `kError` output sees the corruption claim with no version line beside it. Second, this
recurs on **every** future `kiVersion` bump, and version bumps are the deliberate, documented mechanism for
invalidating old saves and replays — so the misleading message is guaranteed to be seen again.

Proven pre-existing and out of scope where it was found. The rejection path itself predates
`DestroyElementReverseWalkSkip.md`; that Plan only supplied the version bump that made it observable. Its
`## In scope` lists only `DestroyElement`, `DestroySweep`, three forward-walk call sites, the `123` → `124`
constant, and the Frame Collections AGENTS.md sentences; its `## Out of scope` names "any collection-level
`kiVersion`, wire format, or save/replay backward-compatibility path". `DifferenceStream.h`'s reader state and
`GameSaveLoad.cpp`'s abort path are in neither list, so changing them there would have been unauthorized. The
Plan's own acceptance criterion — that a pre-change replay is "rejected with the existing version-mismatch
error rather than producing a checksum mismatch" — was met: the rejection is correct, only its wording is
wrong.

## Design

Do not change what is rejected. The single defect is that the reason for the rejection is discarded between
the reader and the abort log, so the abort path has nothing to report but its worst-case assumption.

Carry the reason forward. `DifferenceStreamReader` already distinguishes the version-mismatch bail
(`DifferenceStream.h:227` and the sibling `DIFFERENCE_TYPE` check just below it) from every other load failure,
but exposes only the boolean `Loaded()`. Give the reader one additional query that reports whether its load
failed specifically on a version header, and have `GameSaveLoad.cpp:690-694` consult it: when the staged
reader failed on a version header, abort with a message that names the version mismatch and the two versions
involved instead of throwing `CorruptStreamException`; when it failed for any other reason, or on either tick
bound check, keep the existing `CorruptStreamException("ReplayManifest stream bounds")` throw and the existing
catch at `:799` unchanged.

The abort must remain an abort: the same replay is refused, the same reader and pending-reader state is torn
down, the same early return happens, and the message stays at `kError` so it is visible at the default runtime
threshold. Only the sentence a user reads changes, from a corruption claim to a version-mismatch statement that
includes the file's version and the expected version, so a single `kError` line explains the refusal without
needing the `kWarning` line above it.

The accurate `kWarning` at `DifferenceStream.h:227` stays exactly as it is: it is the correct diagnostic for
its own layer and its message text carries the sizes as well as the versions.

## Critical files

- `Engine/Source/File/DifferenceStream.h` — the `SAVED_TYPE` version bail at `:227` and its `DIFFERENCE_TYPE`
  sibling immediately below, the `mbLoaded` assignment at `:399`, `Loaded()` at `:509-512`, and the member
  declaration at `:564`. This is where the discarded reason must be retained.
- `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp` — the staged-reader validation and throw at
  `:690-694`, and the catch that logs `SaveLoadReplay aborted: corrupt replay data` at `:799`.
- `Common/Serialization.h` — `CorruptStreamException` at `:13-17`, read as the contract the version case must
  stop borrowing; it is not expected to change.

## In scope

- One additional reason query on `engine::DifferenceStreamReader` plus the member that backs it, set at the two
  version-header bail points in `DifferenceStream.h` and nowhere else.
- The staged-reader failure branch in `GameSaveLoad.cpp:690-694`: distinguishing the version-mismatch failure
  from the other failures and emitting a `kError` version-mismatch abort message that names the file version
  and the expected version.
- The existing `CorruptStreamException("ReplayManifest stream bounds")` path and its `:799` catch, which stay
  in place for every non-version failure.

## Out of scope

- What is accepted or rejected: no replay, save, or version that is refused today may load after this change,
  and none that loads today may be refused.
- `Frame::kiVersion`, any collection `kiVersion`, version-header layout, `ReadAndValidateVersionHeader`, and
  any backward-compatibility or migration path for old replays or saves.
- Replay recording, playback stepping, checksum computation and validation, the `.checksums` file, the replay
  manifest format, and `ReplayManifestRecord`.
- The `kWarning` message text, level, or category at `DifferenceStream.h:227` and its sibling.
- Applying the same treatment to the non-replay save-load paths, to `ReadVersionedFile` callers, or to any
  other `CorruptStreamException` throw site in the repository.
- Adding a new log category, a harness command, or any agent-visible query for load-failure reasons.

## Risk tier and invariants

Change Workflow **Tier 2** (scoped behavior in the replay load path). The Tier-3 save/replay-compatibility
trigger is deliberately not reached: no serialized byte, header layout, or version constant changes, and the
accept/reject decision is bit-for-bit the one made today. The change is confined to which message describes an
already-decided refusal.

Escalate to Tier 3 and re-plan rather than widening the change if implementation finds it must alter
`ReadAndValidateVersionHeader`, a version constant, a serialized layout, or the set of streams that load.

Invariants to preserve:

- Client and server behave identically; the reader is shared code and must not diverge per build.
- Every refusal remains a refusal, with the same teardown of `mReplayReaders`, `mPendingReplayReaders`, and
  `mReplayTransferCaptureInfo`, and the same early return.
- The abort message stays at `kError` so it is visible at the default runtime threshold.
- Genuine corruption — truncation, damaged bytes, out-of-range tick bounds — still reports as corrupt data
  through the unchanged `CorruptStreamException` path.

## Acceptance criteria

- A replay recorded before a `Frame::kiVersion` bump, replayed after it, produces a `kError` abort line that
  states a version mismatch and includes both the file's version and the expected version, and that line no
  longer contains the words "corrupt replay data".
- That same run still refuses the replay: no ticks are replayed, and the run produces no checksum-mismatch or
  desync line.
- A replay file deliberately truncated mid-stream, at a matching version, still aborts with the existing
  `SaveLoadReplay aborted: corrupt replay data: ReplayManifest stream bounds` line, proving the corruption path
  is intact.
- A replay recorded and played back on the same build still records and replays cleanly with matching per-tick
  checksums, proving the success path is untouched.
- Client and server compile.
