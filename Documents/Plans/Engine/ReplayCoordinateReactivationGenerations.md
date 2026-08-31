<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T15:30:26.164Z","dependsOn":[]} -->
# Preserve replay coordinate reactivations as distinct generations

## Context

`Replay::RetainReplayEndFrame` makes the first retired writer for a coordinate
terminal and preserves its complete end frame, while
`Replay::CaptureHarvestedTransfers` silently returns when a later Player
transfer reactivates that coordinate (`Engine/Source/File/Replay.cpp:308-365`).
The accepted transfer is absent from the recording, and the terminal writer is
also skipped on subsequent recording ticks (`Engine/Source/File/Replay.cpp:891-909`).

The omission remains reachable because a Player arrival makes an inactive
destination live and materializes its Frame before replay capture
(`Engine/Source/Network/Server/ServerTransferManager.cpp:73-105,282-320`). The
runtime later retires Frames outside the active set, while the game re-adds
every Player-containing coordinate (`Engine/Source/Network/Server/ServerSessionRuntime.cpp:287-326`;
`Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:294-310`).
A Player can therefore leave a coordinate, allow its Frame and replay writer
to retire, and re-enter the same coordinate during one recording.

The current representation cannot preserve that second lifetime. Writer and
reader state are keyed only by coordinate (`Engine/Source/File/Replay.h:66-84`);
manifest loading rejects repeated coordinates, artifact filenames and
inventory identity use only the coordinate key, and playback stages at most
one pending reader per coordinate (`Engine/Source/File/Replay.cpp:54-110,182-213,453-489,578-675`).
A local writer restart would overwrite or alias the first terminal lifetime.

The originating retained finding is S005-C017 (severity HIGH, confidence HIGH,
credible exposure), recorded against audit baseline
`a20acf1e31a24a0f61ae638e8976602b49655788`. The current-source evidence above
confirms it is unresolved, and no executable Plan owns the same root cause and
implementation boundary.

## Design

Preserve the documented live behavior that a Player transfer makes its
destination live. Replay recording observes accepted transfers; it does not
reject reactivation to fit the old replay layout. Represent every interval
between a coordinate's activation and retirement as a distinct private replay
generation identified by `(coordinate, activation tick)`. A coordinate has at
most one activation batch per tick because `ServerTransferManager` consolidates
one sorted batch per destination before calling Replay.

Use this public interface shape in `Replay`:

```cpp
enum class ReplayTransferCaptureResult : uint8_t
{
	kNotRecording,
	kCaptured,
	kRecordingInvalidated,
};

enum class ReplayTickDecision : uint8_t
{
	kDispatch,
	kReloadBeforeDispatch,
};

[[nodiscard]] ReplayTransferCaptureResult CaptureAcceptedTransfers(
	GridCoord destination,
	std::span<const game::StatusChange> sortedTransfers,
	const game::Frame& rPreTransferFrame);

void RetireCoordinate(
	GridCoord coord,
	std::unique_ptr<game::Frame> pLastCompleteFrame);

[[nodiscard]] ReplayTickDecision SyncReplayTick();
```

`ServerTransferManager::HarvestTransfers` keeps its current one-call-per-destination
shape and calls `CaptureAcceptedTransfers` after sorting and before applying
the batch:

```cpp
for (const auto& [rCoord, rTransfers] : mTransfers)
{
	const ReplayTransferCaptureResult eCapture =
		gpReplay->CaptureAcceptedTransfers(
			rCoord,
			rTransfers,
			*game::gpGame->mCoordFrames.at(rCoord).pNext);

	if (eCapture == ReplayTransferCaptureResult::kRecordingInvalidated)
	{
		LOG(kDefault, kError,
			"Replay recording invalidated while capturing accepted transfers");
	}
}

ApplyPreparedTransfers(clientTransfers, true);
```

`kNotRecording` and `kCaptured` need no caller action. If Replay cannot keep an
active recording complete, it clears replay-exclusive recording state,
invalidates the manifest through the existing invalidation path, returns
`kRecordingInvalidated`, and still allows the already accepted live transfer to
proceed. The caller logs that observable transition once; it never retries,
rejects, or partly applies the transfer.

Add `ReplayPersistenceFailurePoint::kTransferCapture` and the existing agent
failure command selector `transfer_capture`, with no coordinate parameter, as
the precise capture-time failure path. `CaptureAcceptedTransfers` consumes this
injection before creating or mutating a generation writer. It then calls one
Replay-private recording-invalidation path that clears every writer, every
open-writer locator, and replay capture diagnostics; leaves the already invalid
manifest unpublishable; calls `game::OnReplayStreamsInvalidated`; and returns
`kRecordingInvalidated`. The live batch proceeds through
`ApplyPreparedTransfers` unchanged. No other internal condition invents a
recoverable capture failure: ordinary allocation and programming failures keep
their existing failure behavior.

`ServerSessionRuntime` moves the retiring current Frame through the game hook
to `RetireCoordinate`. Replay either stores it as the open generation's
immutable terminal end Frame or destroys it when no open recording generation
exists. The old reference-taking `RetainReplayEndFrame` interface is removed.

Keep generation identity and storage private. Store every writer generation
until recording stops, plus a coordinate-keyed locator for the one open writer
for each coordinate. When `CaptureAcceptedTransfers` sees a coordinate without
an open writer during recording, create a new writer from the supplied
pre-transfer Frame with activation tick equal to that Frame's tick, then record
the sorted post-dispatch batch at the same tick. A retirement terminalizes only
the open generation, removes its locator, and never resumes or overwrites it.

Persist every generation as a separate manifest record and sibling artifact
set. Extend artifact filenames and inventory identity with the activation tick
so two generations of one coordinate cannot alias. Sort records canonically by
`(activation tick, coord.x, coord.y)` and inventory entries by their complete
generation-qualified identity. Bump the replay manifest version and generation
digest domain for the new layout. Continue rejecting old manifest versions;
do not add a compatibility reader. `Frame` and `FrameInput` stream payloads do
not change, so their versions remain unchanged.

Playback stages all generation records in canonical order rather than storing
only one pending reader per coordinate. At each tick, first consume and
checksum-validate terminal readers and remove their live coordinate ownership;
then activate every generation due at that exact tick; then load current inputs
and exact-tick post-dispatch transfer batches. The later generation's accepted
transfer is published at activation tick `E`, and that coordinate first
dispatches at `E + 1`, matching the existing transfer pipeline. Keep at most
one live reader per coordinate.

Before state adoption, validate the complete replay generation: activation
identities are unique and canonical; each artifact and inventory entry resolves
to exactly one generation; generations of one coordinate do not overlap; later
activation ticks are strictly greater than the prior generation's saved
end-frame tick; the natural adjacent case `activation == savedEnd + 1` is valid
because the prior reader retires at that terminal-consumption tick before the
new reader activates; initial-grid
membership equals exactly the records activated at the recording's initial
tick; all existing count, size, digest, stream-bound, and version checks still
pass. Any failure uses the existing corrupt-replay or expected-version-refusal
path before live state is replaced.

Update replay diagnostics and failure fixtures that select a coordinate writer
to select the open generation, or the most recently terminalized generation
when the fixture explicitly targets retained terminal state. Do not expose
generation identity through agent commands: existing replay capture counts and
ticks, stopped manifest/artifact inspection, and player/frame queries are
sufficient to distinguish the generations for the reactivation acceptance
scenario, so no public generation-selection command is added.

## Alternatives considered

The user selected the dedicated deep-module interface above after reviewing
three alternatives:

- **Generic event façade:** one `ObserveReplayEvent` entry point accepted a
  variant of transfer and retirement events. It kept internals opaque but made
  the two dominant callers construct unrelated variant payloads and obscured
  which operation transfers Frame ownership.
- **Data-local batch capture:** one call accepted a workbuffer-backed sorted
  span and exposed replay generation keys while Replay used parallel generation
  arrays. It offered contiguous scans, but the current transfer map already
  supplies one deterministic destination batch at a time; rebuilding another
  batch and maintaining parallel columns had no demonstrated consumer benefit.
- **Move-only capture permit:** transfer application consumed a permit proving
  every accepted batch had visited Replay. It made phase misuse structurally
  difficult, but coupled every transfer-application path to Replay token state,
  expected counts, and move/serial validation. The selected result enum makes
  recording invalidation observable without adding that protocol.

Forbidding terminal-coordinate reactivation was also rejected. It would change
the documented live Player-transfer behavior only while recording and would
need rejection before source-row removal to avoid treating the Player as dead.
Distinct generations preserve live simulation behavior and repair Replay at
the owning layer.

## Critical files

- `Engine/Source/File/Replay.h:27-90` — public capture/retirement/tick results and private writer/reader generation ownership.
- `Engine/Source/File/Replay.cpp:18-249` — manifest version, generation/inventory identity, filenames, digest, invalidation, and publication.
- `Engine/Source/File/Replay.cpp:291-377` — recording invalidation, accepted-transfer capture, generation creation, and retirement ownership.
- `Engine/Source/File/Replay.cpp:420-703` — staged manifest validation, reader creation, initial-grid correlation, and adoption.
- `Engine/Source/File/Replay.cpp:723-1045` — writer start/stop/update, generation publication, terminal-reader retirement, activation, and exact-tick playback.
- `Engine/Source/GameBase.cpp:369-402` — `ServerUpdate` consumption of `ReplayTickDecision` and stop-before-dispatch behavior.
- `Engine/Source/Network/Server/ServerTransferManager.cpp:73-105,282-363` — unchanged Player admission plus capture-result logging and replay/live transfer phase boundary.
- `Engine/Source/Network/Server/ServerSessionRuntime.cpp:287-326` — `SyncActiveFrames` move of the retiring current Frame and Player-driven reactivation.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.h:75-90` and `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:294-310` — `OnFrameRetiring` ownership signature/forwarding and game active-set contribution.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp:292-430,601-748` — existing replay capture diagnostics, retained-frame and persistence fixtures, and transfer fixture selectors affected by generation storage.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md` and `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md` — documented replay scenario and `transfer_capture` failure selector.
- `Engine/Source/File/AGENTS.md` — replay writer lifetime, manifest identity, failure, and activation-generation contracts.
- `Engine/Source/Network/Server/AGENTS.md` and `Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md` — transfer capture and retirement ownership contracts if the interface wording changes them.

## In scope

- `Replay` public methods and result enums named in `## Design`; the
  `ServerTransferManager::HarvestTransfers` capture-result consumer; the
  `ServerSessionRuntime::SyncActiveFrames` `std::move` handoff through the
  `ServerSession::OnFrameRetiring` declaration and implementation; and the
  `GameBase::ServerUpdate` bool-to-`ReplayTickDecision` consumer.
- Replay-private generation identity, writer ownership, open-writer lookup,
  immutable retained terminal Frames, staged/pending generation readers, and
  one-live-reader-per-coordinate ownership.
- Manifest records, inventory identity, sibling filenames, manifest version
  and digest domain, canonical ordering, bounds, authentication, and
  trust-boundary validation required for repeated coordinate generations.
- Recording and playback phase ordering needed to publish an accepted transfer
  at activation tick `E`, retire any prior generation first, and begin the new
  generation's simulation dispatch at `E + 1`.
- Existing agent replay diagnostics and failure fixtures only where their
  coordinate-only writer selection becomes incorrect, the exact
  `transfer_capture` injection/result path defined above, and the smallest
  harness-observable extension needed to prove leave/retire/re-enter behavior.
- Applicable replay and network-server contract documentation changed by the
  interface, lifetime identity, or phase ordering.

## Out of scope

- Rejecting or changing live Player transfer admission, adjacency checks,
  non-Player destination liveness, ownership relinking, or ordinary active-set
  policy.
- Wire protocol, save-game format, `Frame`/`FrameInput` payload layout,
  backward-compatible replay readers, playback interpolation, or unrelated
  replay lifecycle findings and Plans.
- Public generation-selection commands, new replay configuration, generalized
  event/capability abstractions, engine Collections/SOA columns, or unit tests.
- Performance-motivated parallel generation tables or batching without a
  measured replay hotspot.

## Risk tier and invariants

Expected Change Workflow Tier 3. Triggers: deterministic replay lifecycle and
CRC ordering; replay serialization and compatibility; and integration across
recording, playback, transfer publication, active-set retirement, and agent
verification.

Preserve these invariants:

- Every accepted Player transfer is represented exactly once at its event tick,
  or the entire recording is observably invalidated before it can publish; live
  transfer acceptance and application do not depend on recording success.
- A coordinate's first and every later terminal writer retains its own complete
  end Frame and never resumes or gets overwritten.
- `(coordinate, activation tick)` uniquely identifies every replay generation
  in memory, manifest records, inventory entries, filenames, and playback.
- Playback consumes and checksum-validates one generation's terminal state
  before activating a later generation for the same coordinate, with at most
  one live reader per coordinate.
- Canonical ordering and exact-tick post-dispatch publication produce
  deterministic replay bytes and deterministic client/server CRC results.
- Old manifest versions remain an expected refusal; corrupt current-version
  generation identities fail before reset/adoption.
- Persistent replay state remains main-thread-owned and heap-backed under the
  existing allocation-suppression boundary; borrowed transfer spans and Frames
  do not outlive their calls.

## Acceptance criteria

- `/compile` passes for the server `Debug|x64` target; if shared headers make
  client compilation reachable, the client `Debug|x64` target also passes.
- A focused `/agent-harness` client/server recording starts with a naturally
  owned Player in coordinate A, moves it into adjacent B, waits until A is
  absent from the active set and its first replay generation is terminal, then
  moves the same persistent Player back into A at a recorded tick before
  stopping. The live Player remains owned and present after both transfers.
- Stopped-server inspection of the committed replay shows two canonical,
  non-aliasing manifest records and artifact sets for A, distinguished by
  activation tick; the first generation's terminal files and digest remain the
  committed inputs to the manifest rather than being overwritten.
- Playback consumes A's first terminal reader before activating its second
  reader, publishes the returning Player transfer exactly once at its recorded
  tick, first dispatches the new A generation on the following tick, preserves
  the Player's persistent identity/ownership, and completes a replay loop with
  matching transfer counts and no CRC, skipped-activation, or missing-file
  error.
- Repeating the leave/retire/re-enter cycle produces a third sequential A
  generation without replacing either earlier generation.
- A current-version replay with a duplicate activation identity, an overlapping
  same-coordinate activation where `activation <= savedEnd`, or a
  generation-qualified inventory/file mismatch is rejected before live state
  adoption; changing only the manifest
  version to the previous value follows the expected version-refusal path.
  The adjacent control `activation == savedEnd + 1` remains valid.
- The existing ordinary nonterminal replay-transfer scenario still records
  and replays its sorted batch at the exact event tick with matching transfer
  counts and CRCs.
- Capture disabled returns `kNotRecording`; successful capture returns
  `kCaptured`; `replay_inject_persistence_failure {"stage":"transfer_capture"}`
  makes the next accepted batch return
  `kRecordingInvalidated`, logs once, allows the live transfer to complete, and
  clears writers, open-writer locators, and capture diagnostics, calls
  `OnReplayStreamsInvalidated`, and leaves no publishable partial manifest.

## Notes

This is one integration Plan because writer lifetime, on-disk identity, reader
activation, and exact-tick verification must change together to satisfy the
same replay invariant; none is independently landable. No dependency edge is
required. No `Collection<T>` or SOA member is added.
