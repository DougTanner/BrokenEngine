<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-30T23:11:21.959Z","dependsOn":[]} -->
# Move cold audio range reads to background PackChunks loaders

## Context

The frozen C++ scope-boundary audit retained the reachable cold-read path
`CSB/shard-0004/001`.  At the current source baseline
`3d9191b46a917850822a4da61871ca1a423c9628`,
`StreamingVoice::FillSlot` (`Engine/Source/Audio/StreamingVoice.cpp:45-99`)
runs on the `kThreadStreamingVoiceFill` `PersistentWorker` and calls
`FileManager::ReadChunkData` at `:61`.  A cold lazy audio chunk reaches the
direct `std::fstream` open/seek/read branch of
`PackChunks::ReadChunkData` (`Engine/Source/File/PackChunks.cpp:928-1005`).
`PersistentWorker` unconditionally uses `THREAD_PRIORITY_TIME_CRITICAL`, while
the existing `PackChunks::LoadingThread` pool is assigned
`THREAD_PRIORITY_BELOW_NORMAL` (`Engine/Source/File/PackChunks.cpp:658-689`).
Startup is a separate phase: the process sets the main thread to
`THREAD_PRIORITY_TIME_CRITICAL` before constructing `FileManager`
(`Engine/Source/Main.cpp:840-859`), and `PackChunks::LoadPackFiles`
(`Engine/Source/File/PackChunks.cpp:245-408`) reads manifests and lazy-chunk
headers and opens the persistent lazy-pack handles.  Those startup
metadata/handle operations are not per-refill payload I/O and remain in place.

This composes two incompatible existing contracts: Audio assigns production to
the time-critical fill worker, while Common forbids file I/O on that worker and
File assigns lazy loading to background loaders.  The main-thread consumer,
XAudio2 callback, and stream wait-before-destruction ordering are otherwise
intentional and must remain intact.  A refill trace therefore begins only
after startup and labels startup metadata/handle work separately.  The current
source has no other caller of `ReadChunkData`; no source, build, or runtime
change is part of authoring this Plan.

## Design

The selected path is the user-directed background range-read design.  Keep
random-access, non-whole-track audio and route cold ranges through the existing
`PackChunks::LoadingThread` pool.

`FileManager` exposes one client-only nonblocking operation with the following
contract.  Its return type is `ChunkReadResult` with `kRetry`, `kPending`,
`kReady`, and `kFailed` values:

`TryReadChunkData(ChunkReadRequest&, common::crc_t, uint64_t, std::span<std::byte>)`

It returns a four-way retry/pending/ready/failed result and both initiates and
polls the request.  `ChunkReadRequest` is one opaque request value owned by each of a
`StreamingVoice`'s existing three slots; it carries no caller buffer pointer.
On an idle request, the operation validates the lazy audio chunk, offset,
length, and destination capacity.  If the fixed pool/ring has no entry, it
returns `kRetry` while leaving the request idle, the slot assignment unchanged,
and the next-read cursor unchanged.  If publication succeeds, it returns
`kPending` with the request's stable pool index/generation, unless an already
resident range can be copied immediately.  On a pending request it polls the
Pack-owned result.  On ready it re-checks the request generation and exact
CRC/offset/length, copies at most `kiBufferSize` (16 KiB) into the caller's
Audio-owned slot buffer, then retires the request.  Failure never writes the
destination.  A request cannot be repurposed for a different range until it
is terminal and retired.  Caller-contract failures (missing/wrong chunk,
overflowed or out-of-range range, zero or over-16 KiB length, or insufficient
destination) return failed without a destination write; a validated short,
truncated, or unreadable required pack remains the existing fatal trust-boundary
error rather than an audio underrun.

The representative Audio call is one request object and one fixed destination
per slot, for example
`TryReadChunkData(mReadRequests[iSlot], mpLazyChunk->location.crc, uiSlotOffset,
std::span<std::byte>(reinterpret_cast<std::byte*>(mBuffers[iSlot]),
iBytesToRead))`.  The call is made by the main-thread update; the loader sees
only its copied CRC/range and pool index.  `kRetry` causes the same slot and
cursor to be retried on the next update; `kPending` is observable proof that
the request was accepted and must not be treated as an idle retry.

Declare `ChunkReadRequest`, `ChunkReadResult`, and this operation inside the
client guard in `Engine/Source/File/FileManager.h`; the server sees none of the
Audio-only request surface while retaining the shared existing FileManager API.

`PackChunks` privately owns exactly six result entries, each with a 16 KiB
payload, for a fixed 98,304-byte result budget plus fixed request/state
metadata.  This covers the current stream and one normally fading stream.  The
existing `mPreviousStreams` list remains unchanged; requests for excess older
fades are not given a Pack-owned reservation class.  Audio owns admission and
ordering: the current stream and newest fading stream get first publication
opportunity, while queued or ready requests from older fading streams are
demoted or reset before they claim an entry.  A claimed older-fade request
remains until its loader acknowledges cancellation.  Older fades have no
finite voice cap and may stay pending or underrun until an entry is reclaimed.
No heap growth or dynamic result allocation is added.

Audio entries use a separate fixed ring of pool indices; publishing an index
is bounded and allocation-free, and Audio never locks `mQueueMutex`.  The
producer publishes the index and request metadata before the release state
transition, advances the atomic ring wake sequence, and wakes the existing
loader condition variable.  The two loader threads' wait predicate checks the
atomic queued state/ring and wake sequence, the existing queue, and shutdown
with acquire ordering, so publication before a wait cannot lose its wake.  The
existing whole-chunk/range-reload queue remains in place.
Loaders give existing `kRealtime` entries strict precedence; when no such
entry is pending, a scheduler choice serialized under `mQueueMutex` alternates
one Audio claim with one existing non-realtime queue claim while both are
nonempty, preserving existing priority ordering within the existing queue.  A
single CAS claim makes each Audio entry exclusive; if only one side has work,
it makes progress.  Shutdown wakes every loader, drains or acknowledges
claimed entries, and only then frees the ring/result storage.

Pool entries use the explicit states `Free -> Queued -> Claimed -> Loading ->
Ready/Failed`.  A single scheduler choice under `mQueueMutex` plus an atomic
CAS owns an entry exclusively.  Queued or ready cancellation invalidates its
generation immediately; a claimed/loading entry remains unreusable until its
loader acknowledges completion.  The loader may finish only into Pack-owned
result storage, then discards a stale generation.  A loader validates the chunk
metadata, logical range, and physical extent before writing that storage,
publishes ready with release/acquire ordering, and never retains an Audio
destination pointer.  Each ring token carries the pool index and generation;
the loader discards a token whose index is no longer queued or whose generation
does not match before claiming it.  Cancellation invalidates the old
generation before a new publication can reuse the index, and the same
index/generation check is repeated after claim and before ready publication, so
a stale token cannot claim or complete a republished request.

Cancellation is request reset/destruction, not a second public File API.
Reset/destruction advances the request generation through a nonblocking
File/Pack hook and invalidates queued or ready pool state immediately.  A
loader already in flight may finish only into Pack-owned result storage; its
generation check discards stale data and it can never write an Audio slot.
PackChunks teardown invalidates remaining pool entries, wakes all loaders,
waits for claimed entries to acknowledge, and joins its existing loaders before
freeing the pool or closing handles.  `ChunkReadRequest` is noncopyable and
nonmovable, stores its stable pool index and generation, and uses an explicit
invalid-index idle sentinel.  The fixed request array is owned by each
`StreamingVoice`, and `FileManager` outlives Audio by the existing `Main` scope
ordering.

`StreamingVoice` owns one next-read cursor and one request-ring index.  Each
accepted request persists its CRC, offset, and length in its slot/request;
the cursor advances only when the request is accepted (resident immediate copy
or successful pool publication).  A full pool/ring leaves the cursor and slot
assignment unchanged for retry.  Main-thread `StreamingVoices::Update` polls
each request, copies ready data, and submits only contiguous ready slots in
the existing FIFO order; a pending earlier slot blocks later ready slots.
When the cursor reaches the audio end, a zero-byte terminal slot is published.
Caller validation failure ends the stream without writing Audio data, while a
validated short/corrupt pack follows the fatal contract above.  Remove the
Audio `PersistentWorker` and its `Wait`/`Wake` fill path; no change is made to
the Common `PersistentWorker` contract.  Main-thread submission, fade/crossfade
behavior, and XAudio2's callback-only atomic completion publication remain as
they are.  The request-ring token stores the slot's pool index and generation;
after cancellation, any late token is discarded before claim and before copy,
while a republished request receives a new generation.  `kRetry` is the only
idle full-pool/ring outcome, so the ring never advances or overwrites a pending
slot on a failed publication.

`StreamingVoices` treats `mpCurrentStream` as current and the latest element
of `mPreviousStreams` after `TransitionCurrentToPrevious` as the newest fading
stream.  That Audio-owned identity determines request publication order.  On a
new transition, queued or ready requests for older fading streams are demoted
or reset and cannot reacquire a result entry ahead of current or newest fade;
claimed/loading older-fade requests finish only after their cancellation is
acknowledged.  This bounds the six-entry pool's useful admission without
adding a finite voice cap or Pack-side reservation classes.

Cold range reads use the owning `LoadingThread`'s existing
`FILE_FLAG_NO_BUFFERING` handle, sector-aligned read buffer, prefix/suffix
calculation, and rounded physical read size.  The loader validates the
requested logical range against both the logical audio extent and the physical
payload extent, rejects a zero/short/overflowed physical read as the existing
fatal pack error, and copies only the exact requested bytes into the bounded
result entry.  The 16 KiB result is never passed directly as an unaligned
`ReadFile` destination.

The project-owned client Agent adds a narrow, debug-only
`audio_streaming_fixture` command in
`Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsClient.cpp` and its
schema in `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-client.md`.
Actions are `start`, `inspect`, `clear`, `suspend`, `resume`, `hold_read`,
`release_read`, `coexistence`, and `invalid`; `start` uses the existing known
menu playlist, `coexistence` queues one whole-chunk
`data::kAudioBlaster16793__pushtobreak__earth1wavCrc` request at
`LoadPriority::kRealtime` and one whole-chunk
`data::kAudioBlaster793907__cvltiv8r__snaresbycvltiv8r301wavCrc` request at
`LoadPriority::kNormal` through the existing `RequestChunkLoad` API, and
`clear`/suspend/resume drive the existing Audio lifecycle.  Run `coexistence`
in a fresh client process after startup and before combat or one-shot requests;
expected existing-work counts are exactly one realtime enqueue/completion and
one normal enqueue/completion, with each whole range `[0, iDataSize)`.

Engine Audio/File code supplies a fixed, allocation-free telemetry/control
record and deterministic loader hold gate, surfaced by the project command.
The record reports startup-versus-refill phase, loader thread ID and priority,
slot/range, queue state, generation, cancellation acknowledgement, a
monotonically increasing event sequence, and an overflow/dropped-event count.
Partition histories by single writer: 32 records for the main-thread writer and
24 records for each fixed loader-index writer (`loader0`, `loader1`).  Each
scenario clears history before its bounded action sequence; each writer
release-publishes completed records.  A monotonic global snapshot sequence is
release-published after each completed event; `inspect` acquire-loads that
sequence and each partition's published head, copies the bounded snapshot, and
rechecks the sequence/heads so it never reports a torn record.  Any
event-history overflow or dropped event fails the evidence; it is never
silently truncated.  `hold_read` holds a known claimed read until
`release_read`; `invalid` exercises only caller-contract failure cases with
fixed destination sentinels.  The fixture is compiled only for the existing
client debug/agent path and does not create a production diagnostic system or
worker.  Device-reset ordering not directly exposed by this fixture remains a
static source check.

Before a stream is erased or its request storage can disappear, invalidate all
of its requests.  Apply that ordering to fade retirement, `Clear`, suspend,
device reset, and Audio teardown.  Keep the existing source-voice detach and
callback lifetime ordering.  The existing `AudioManager::Update`, reset, and
suspend paths remain the main-thread lifecycle entry points; they must not wait
for a cold read.

The following alternatives were considered and are closed, not open decisions:

2. A batched ticket/span table with a File-owned XAudio slab was rejected for
   its wider surface and lifetime coupling, and because its volume is
   overbuilt for three-slot range reads.
3. A `ChunkReader` plus move-only RAII `ChunkRead` lease was rejected for extra
   public types and File/XAudio pool coupling.

## Critical files

- `Engine/Source/File/FileManager.h` and `.cpp` — client-only opaque request and
  nonblocking operation forwarding; preserve server compilation.
- `Engine/Source/File/PackChunks.h` and `.cpp` — private fixed request/result
  pool, separate Audio index ring, generation publication/cancellation, aligned
  range service, and existing below-normal loader scheduling.
- `Engine/Source/Audio/StreamingVoice.h` and `.cpp` — one request per slot,
  one next-read cursor and request-ring index, three-slot read-ahead, poll/copy
  state transitions, and invalidation.
- `Engine/Source/Audio/StreamingVoices.h` and `.cpp` — remove the fill worker
  and move bounded request publication/polling into the main-thread update
  lifecycle.
- `Engine/Source/Audio/AudioManager.h` and `.cpp` — client-only fixture access
  plus verification that Update, fade, suspend, reset, and teardown ordering
  remains the lifecycle owner.
- `Engine/Source/Audio/AGENTS.md` — Runtime Contracts worker/Update/lifecycle
  bullets and Asset and Thread Ownership random-access bullet.
- `Engine/Source/File/AGENTS.md` — Packed Assets lazy-loader, random-access,
  and reset-race bullets.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsClient.cpp:566-588` —
  project-owned `audio_streaming_fixture` actions, deterministic coexistence /
  invalid-input fixture, and known menu-track start.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-client.md` —
  fixture schema, action ordering, and returned telemetry fields.

## In scope

- Replace the Audio cold lazy-read call with the client-only nonblocking
  `TryReadChunkData` request/poll contract above, including distinct `kRetry`
  for an idle full-pool/ring publication and `kPending` for an accepted request.
- Add exactly six fixed PackChunks result entries (98,304 payload bytes plus
  fixed metadata) globally, without Pack-side current/fade reservation
  classes, and a separate fixed Audio index ring to the existing lazy loader
  threads.  Audio owns current/newest-fade admission and publishes indices
  without allocation or `mQueueMutex`; the two loaders use the existing mutex
  only to serialize scheduling/claim, preserve strict existing `kRealtime`
  precedence, and alternate one Audio claim with one existing non-realtime
  claim when both are nonempty.
- Implement the explicit `Free -> Queued -> Claimed -> Loading -> Ready/Failed`
  ownership states, stable pool index/generation plus idle sentinel and
  noncopyable/nonmovable request policy, exclusive claims, no-lost-wake/
  progress/shutdown behavior, immediate queued/ready invalidation, and no reuse
  of claimed/loading entries until loader acknowledgement.  Ring tokens must be
  generation-checked before claim and before completion so cancellation followed
  by republish cannot reuse stale work.
- Use each owning loader's existing `FILE_FLAG_NO_BUFFERING` aligned scratch
  read with safe prefix/suffix, logical/physical extent, and short-read checks
  before copying the exact bounded range into Pack-owned result storage.
- Give each existing `StreamingVoice` buffer slot exactly one opaque request,
  persist its CRC/offset/length, advance one voice-owned next-read cursor only
  on accepted request publication, retry unchanged when no pool entry exists,
  block FIFO publication behind an earlier pending slot, and provide
  three-slot read-ahead without whole-track residency.
- Keep `mpCurrentStream` as the current identity and the latest
  `mPreviousStreams` element as the newest fade; demote/reset older queued/ready
  fade requests before they claim entries, while claimed/loading entries await
  acknowledgement.  Preserve the unbounded fade list and allow older fades to
  underrun.
- Remove only the Audio streaming fill `PersistentWorker` use and its waits and
  wakes; keep the existing Common worker contract and all other workers.
- Invalidate requests before fade retirement, `Clear`, suspend, device reset,
  teardown, and any stream/container destruction; stale in-flight generations
  must be discarded in Pack-owned storage.
- Return failed without writing for caller-contract errors; keep validated
  short, truncated, or unreadable required-pack data on the existing fatal
  corruption path.  Represent EOF as a zero-byte terminal slot.
- Add the narrow client-debug `audio_streaming_fixture` command and fixed,
  allocation-free telemetry/control for known-stream start/inspect/clear/
  suspend/resume, deterministic `hold_read`/`release_read`, deterministic
  existing-work `coexistence`, and caller-input `invalid` cases.  Report
  startup-versus-refill phase, loader thread ID/priority, slot/range, queue
  state, generation, cancellation acknowledgement, monotonically increasing
  event sequence, and event-history overflow/dropped count; overflow fails the
  evidence instead of being silently truncated.
- Make `coexistence` use one whole-chunk
  `data::kAudioBlaster16793__pushtobreak__earth1wavCrc` request at
  `LoadPriority::kRealtime` and one whole-chunk
  `data::kAudioBlaster793907__cvltiv8r__snaresbycvltiv8r301wavCrc` request at
  `LoadPriority::kNormal`, alongside the known
  `data::kAudioMusicdoodlewavCrc` stream, under fresh-process preconditions;
  expect one enqueue and completion for each existing request.  Make `invalid`
  cover caller CRC/range/length/destination cases only; keep corrupt-pack
  acceptance static.
- Partition fixture history into allocation-free single-writer buffers of 32
  main-thread records, 24 loader-0 records, and 24 loader-1 records.  Clear
  each history before its bounded scenario; release-publish records and
  acquire-load a coherent `inspect` snapshot with monotonic sequence and
  overflow/dropped-event accounting.
- Update the owning Audio and File AGENTS regions and run
  `/progressive-disclosure-review` after `/update-claude-docs` when those
  contracts change.
- Preserve main-thread submission, atomic-only XAudio2 completion callbacks,
  48 kHz packed-audio validation, and presentation-only Audio ownership.

## Out of scope

- Whole-track residency, changing lazy audio to eager data, or changing random
  access to sequential-only streaming.
- Changing the `PersistentWorker` contract, adding a new worker pool, changing
  the existing loader priority levels, or adding speculative configuration.
- DataPacker behavior, `.pack`/manifest/chunk layout or version changes, and
  backward-compatibility formats.
- Static voices, Audio format policy, XAudio2 callback semantics, or the main
  thread's existing buffer submission contract.
- Frame state, CRCs, wire/protocol, save/replay, shader, Collections/SOA, or
  deterministic simulation changes.
- Unit tests, a production diagnostic system, or unrelated File/Audio cleanup.

## Risk tier and invariants

Future implementation is Change Workflow Tier 3.  Trigger: it crosses the
Audio/File subsystem boundary, changes request ownership and cancellation
across threads, and changes a shared client/server File interface while
preserving client-only behavior.

Preserve these invariants:

- Startup manifest/header reads and persistent lazy-pack handle opens remain in
  `LoadPackFiles` and are labeled as startup.  Per-refill cold lazy-audio
  payload reads run only on the existing `PackChunks::LoadingThread` pool at
  `THREAD_PRIORITY_BELOW_NORMAL`; the former time-critical fill path no longer
  performs them.
- The main thread never blocks, allocates for a read, opens, seeks, or reads;
  it only publishes/polls bounded requests, copies a validated ready result,
  and submits ready XAudio buffers.  Brief underruns are allowed.
- Exactly six Pack-owned result entries hold at most 98,304 payload bytes plus
  fixed metadata globally.  Audio-owned current/newest-fade ordering gives
  those streams first publication opportunity; Pack has no current/fade
  reservation classes.  Excess older fades may remain pending or underrun,
  with no finite voice cap or dynamic result allocation.
- Audio publication uses a separate fixed index ring and the explicit
  `Free -> Queued -> Claimed -> Loading -> Ready/Failed` state machine.  Audio
  never locks `mQueueMutex` or allocates; publication metadata/state and an
  atomic wake sequence precede the shared loader wake.  The two loaders include
  the atomic queued state/ring and wake sequence in their condition predicate,
  serialize scheduler choice under `mQueueMutex`, and make exclusive CAS
  claims.  The predicate observes both queues and shutdown, and shutdown joins
  loaders only after claimed entries acknowledge.  Existing `kRealtime` work
  has strict precedence; otherwise one Audio claim alternates with one existing
  non-realtime claim while both are nonempty.
- Each slot has one opaque request, one persisted CRC/offset/length, and one
  voice-owned next-read cursor plus request-ring index.  The cursor advances
  only on accepted publication; a full pool/ring retries unchanged, an earlier
  pending slot blocks later FIFO submission, and EOF is a zero-byte terminal
  slot.  `ChunkReadResult::kRetry` is distinct from accepted `kPending`, so a
  rejected idle publication cannot change the slot, cursor, or destination.
- Queued/ready cancellation invalidates its generation immediately.  A
  claimed/loading entry remains unreusable until its loader acknowledges, and
  no stale result can write an Audio slot after fade retirement, `Clear`,
  suspend, device reset, or teardown.  `ChunkReadRequest` stores its stable
  pool index and generation, uses an invalid-index idle sentinel, is
  noncopyable/nonmovable, and is reset through a nonblocking File/Pack hook;
  FileManager outlives Audio by the existing Main scope ordering.
- Every logical and physical range and destination bound is checked before a
  write.  Caller-contract failures return failed without writing; validated
  short, truncated, or unreadable required-pack data follows the fatal
  corruption path.
- Cold reads use the owning loader's sector-aligned `FILE_FLAG_NO_BUFFERING`
  scratch with safe prefix/suffix and rounded physical reads; only the exact
  validated range is copied to the Pack result.
- The main thread remains the sole XAudio2 buffer submitter and
  `OnBufferEnd` remains an atomic-only callback publication.
- Audio continues random-access lazy reads without whole-track residency and
  remains outside deterministic Frame/CRC state.  No wire, save/replay, pack,
  or serialization identity changes.
- Client-only declarations do not break the shared FileManager header or
  server build, and existing lazy-loader shutdown ordering remains valid.
- The debug-only fixture uses fixed telemetry storage/control and does not add
  a production diagnostic system, heap activity to the Audio update, or a new
  worker.  Its bounded event/counter history is partitioned into one main
  writer (32 records), loader-0 (24), and loader-1 (24); each writer publishes
  with release ordering and `inspect` acquires a coherent snapshot.  Each
  partition exposes a monotonically increasing sequence and overflow/dropped-
  event count; an overflow invalidates the runtime evidence rather than hiding
  events.

## Coordination

This Plan has no directional prerequisite (`dependsOn: []`).  Implement File
and Audio slices only when their request/status contract is shared, then run
`/update-affected-code` across both sides.  The future execution card should
route disjoint implementation slices to implementer(s), followed by client and
server `/compile`, `/repo-code-review`, `/scope-review`, and
`/adversarial-review`; `/code-style-review` and `/update-claude-docs` apply to
the resulting C++ changes.  The Agent fixture changes existing client command
code and its project command schema, so update the project AgentHarness
documentation.  If the Audio/File owner contracts change, update the exact
Runtime Contracts/Asset and Thread Ownership regions in
`Engine/Source/Audio/AGENTS.md` and the Packed Assets regions in
`Engine/Source/File/AGENTS.md` through `/update-claude-docs`, then run a fresh
`/progressive-disclosure-review`.  No new project source membership is
expected, but `/update-vcxproj` applies if the implementation changes
whole-file target affinity.  `/agent-harness` must own the cold-read,
coexistence, invalid-input, and lifecycle runtime scenario, and
`/verify-changes`/`/finalize-changes` own the landing gate.

## Acceptance criteria

- After startup, send the existing client-only `audio_resume`, then send
  `audio_streaming_fixture {"action":"start"}` and
  `audio_streaming_fixture {"action":"inspect"}` through the existing Agent
  transport.  The bounded telemetry records startup-versus-refill `phase`,
  `threadId`, `threadPriority`, `slot`, `crc`, `offset`, `length`,
  `queueState`, `generation`, and `cancelAck`; startup manifest/header reads
  and persistent handle opens are labeled separately.  Expected result:
  per-refill payload reads have only the existing PackChunks loader identity
  and `THREAD_PRIORITY_BELOW_NORMAL`, with no refill open/seek/read on the
  main or former time-critical fill path.  Independent signal: static call
  inspection of `AudioManager::Update`/`StreamingVoice` plus the phase-tagged
  runtime record.  An induced full pool/ring publication reports
  `ChunkReadResult::kRetry` while an accepted publication reports
  `kPending`; the two outcomes leave and advance the cursor/slot state exactly
  as specified.
- A rapid-track transition that saturates the six-entry pool reports exactly
  98,304 bytes of fixed result payload plus fixed metadata, keeps the current
  stream and newest fading stream progressing through Audio-owned admission
  order, and leaves only excess older fading streams pending/underrunning until
  reclamation.  No Pack reservation class, result-pool heap growth, or finite
  voice-cap behavior appears.  Independent signal: the fixed-capacity source
  constants/size calculation plus the fixture's crossfade saturation record.
- The same fixture's
  `audio_streaming_fixture {"action":"inspect"}` result observes three slot
  requests or their bounded pending/ready states, one voice cursor with
  persisted slot ranges, contiguous FIFO submission, and ready-result copying.
  A held pending read may produce only the approved brief underrun; no
  whole-track request or residency appears.  Independent signal:
  `StreamingVoice` cursor/state source review and the slot/range telemetry
  sequence.
- The fixture's `audio_streaming_fixture {"action":"invalid"}` cases and
  source checks prove that missing CRC, offset/range overflow,
  out-of-bounds range, zero length, over-16 KiB length, and too-small
  destination return `ChunkReadResult::kFailed` before any Pack or Audio
  destination write; fixed destination sentinels remain unchanged and valid
  resident ranges copy without disk I/O.  The fixture never mutates required
  pack data.  A validated short, truncated, or unreadable required pack reaches
  the fatal corruption path, not the normal audio underrun path; that case is
  static trust-branch evidence unless a safe existing fault fixture is added.
  Independent signal: bounds/failure branches in
  `FileManager::TryReadChunkData`/`PackChunks` plus the destination sentinel
  and targeted failure log.
- In a fresh client process after startup and before combat/one-shot requests,
  run `audio_streaming_fixture {"action":"coexistence"}` with the known Audio
  read.  The fixture uses one whole-chunk
  `data::kAudioBlaster16793__pushtobreak__earth1wavCrc` request at
  `LoadPriority::kRealtime` and one whole-chunk
  `data::kAudioBlaster793907__cvltiv8r__snaresbycvltiv8r301wavCrc` request at
  `LoadPriority::kNormal` through existing `RequestChunkLoad`; each range is
  `[0, iDataSize)`.  Expected existing-work counts are exactly one realtime
  enqueue/completion and one normal enqueue/completion.  The fixed event
  histories are single-writer main/loader-0/loader-1 partitions of 32/24/24
  records, release-published and acquire-inspected coherently under one
  monotonic global snapshot sequence; every partition sequence is monotonic and
  overflow/dropped count is zero.  Loader source and events
  show the separate Audio ring publishes without allocation or blocking,
  existing `kRealtime` has strict precedence, Audio alternates one claim with
  one existing non-realtime claim while both are nonempty, claims are
  exclusive, and the predicate observes both queues, the wake sequence, and
  shutdown.  No wake is lost and both sides make progress.  Independent signal:
  partitioned history plus `LoadingThread` claim/wake/shutdown source
  inspection.
- In a fresh client process after startup, send the exact lifecycle sequence
  `audio_resume`, `audio_streaming_fixture {"action":"start"}`,
  `audio_streaming_fixture {"action":"hold_read"}`,
  `audio_streaming_fixture {"action":"coexistence"}`,
  `audio_streaming_fixture {"action":"inspect"}`, then
  `audio_streaming_fixture {"action":"release_read"}`.  Inspect the held
  claimed/loading request before release; it must show immediate generation
  invalidation for queued/ready work, no reuse of the claimed index/generation
  (including a cancel-then-republish attempt), and `cancelAck` only after
  release lets the loader finish into Pack-owned storage.  The coexistence
  action uses the fixed one-realtime/one-normal existing requests and counts
  specified above.  Run the clear/suspend/resume lifecycle as a fresh-process
  follow-up using the same order's `start`/`hold_read`/`inspect`/`release_read`
  prefix; teardown uses the normal harness `quit`/exact-PID release.
  Device-reset ordering is independently checked through `OnReset`,
  `FinishDeviceReset`, and `ClearVoices` source ordering.  Independent signal:
  lifecycle telemetry/generation records plus source destruction ordering.
- Source inspection of the owning loader's `FILE_FLAG_NO_BUFFERING` path proves
  sector-aligned scratch reads, safe prefix/suffix and rounded physical sizes,
  logical/physical extent checks, and short-read validation before copying the
  exact requested range into the 16 KiB Pack result.  The result buffer is
  never passed directly as an unaligned `ReadFile` destination.  Independent
  signal: the unchanged handle/scratch setup and the new range-read branch are
  reviewed separately.
- Main-thread allocation/blocking tracing and source inspection show that
  request publication, polling, bounded copy, and XAudio submission add no
  per-read heap allocation, wait, open, seek, or read.  The main thread remains
  the sole XAudio2 submitter and `OnBufferEnd` remains an atomic-only
  completion publication.  Independent signal: allocation/blocking trace plus
  callback source inspection.
- The client-only `TryReadChunkData` declaration and Agent fixture guards leave
  the shared FileManager/PackChunks headers valid for `BT_SERVER`; client and
  server Debug builds pass through `/compile`.  The project command schema in
  `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-client.md`
  matches `start`, `inspect`, `clear`, `suspend`, `resume`, `hold_read`,
  `release_read`, `coexistence`, and `invalid`, plus the phase/thread/priority,
  slot/range, queue, generation, cancellation, sequence, and overflow fields.
  The owning Audio/File AGENTS regions are updated and pass
  `/progressive-disclosure-review` after `/update-claude-docs`.  Independent
  signal: both-target compile results plus schema/source and fresh
  progressive-disclosure review.
- Diff and source inspection show no Frame/CRC, wire, save/replay, `.pack`,
  serialization, Collections/SOA, unrelated `PersistentWorker`, or new worker
  pool change.  The complete implementation and lifecycle evidence pass the
  required `/repo-code-review`, `/scope-review`, `/adversarial-review`,
  style/docs checks, and `/verify-changes` landing review.

## Notes

The root evidence is the frozen audit's direct source path above, corroborated
by `Common/Threading/AGENTS.md`, `Engine/Source/Audio/AGENTS.md`, and
`Engine/Source/File/AGENTS.md`.  The superseded open-choice investigation is
removed from the executable documentation set.  No exact duplicate Plan was
found by searching live Plans and Investigations for the affected symbols,
worker role, cold/lazy audio, and time-critical file-I/O outcome.
