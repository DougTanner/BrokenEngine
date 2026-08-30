# Audio Streaming on a Time-Critical Persistent Worker

Status: Open investigation; no implementation decision has been made.

Area: Engine

Record type: Non-executable reference material. This document intentionally has
no scheduler metadata and is not a Plan input.

Audit source: `CSB/shard-0004/001` in the frozen C++ scope-boundary audit.

Frozen audit commit: `80896f33661aaab99cf180a96db54600099be652`

## Context

Audio playback is client-only presentation. `AudioManager::Update` runs on the
main thread and owns XAudio2 pumping and buffer submission. A streaming fill
worker produces buffer data, the main thread consumes and submits it, and the
XAudio2 completion callback publishes only an atomic completion count. Streaming
music intentionally reads ranges from lazy audio chunks instead of loading a
whole track into memory.

The Common Threading contract gives every `PersistentWorker` thread
`THREAD_PRIORITY_TIME_CRITICAL` and explicitly excludes background or file-I/O
work because a long or blocking operation can starve the rest of the machine.

## Finding under investigation

The Audio streaming fill worker is a `common::PersistentWorker` constructed with
`kThreadStreamingVoiceFill` (`Engine/Source/Audio/StreamingVoices.h:50-51`).
`StreamingVoices::Update` wakes it to fill empty slots
(`Engine/Source/Audio/StreamingVoices.cpp:88-125`), and the worker reaches
`StreamingVoice::FillSlot` (`Engine/Source/Audio/StreamingVoice.cpp:45-99`).
`FillSlot` calls `PackChunks::ReadChunkData` synchronously
(`StreamingVoice.cpp:61`). When the lazy chunk is not resident, the
`ReadChunkData` branch opens the pack with `std::fstream`, seeks to the range,
and reads it before returning (`Engine/Source/File/PackChunks.cpp:953-1005`).

The normal update route is reachable from `AudioManager::Update`
(`Engine/Source/Audio/AudioManager.cpp:544-552`). The resident-copy branch does
not dominate the cold path: a not-yet-loaded or reset lazy audio chunk reaches
the direct pack read. The result is synchronous file open/seek/read on the
time-critical thread, crossing the Common placement boundary. A storage stall
can therefore block a thread above ordinary application threads and starve
unrelated machine work.

## Evidence and root boundary

- `Common/Threading/AGENTS.md:15-17` owns the `Wake`/`Wait` lifetime contract
  and prohibits background or file-I/O work on a `PersistentWorker`.
- `Common/Threading/PersistentWorker.cpp:6-10` unconditionally sets the worker
  priority to `THREAD_PRIORITY_TIME_CRITICAL`.
- `Engine/Source/Audio/AGENTS.md:11-12,24-26` assigns production to the
  streaming fill worker, consumption to the main thread, lock-free callback
  completion, and random-access lazy audio ownership.
- `Engine/Source/File/AGENTS.md:27-30` assigns lazy loading to background
  loaders and explicitly keeps audio as random-access reads without whole-chunk
  residency. It also identifies the audio fill worker as a racing lazy-chunk
  reader during reset ordering.
- `Engine/Source/Audio/StreamingVoice.cpp:204-207` shows the completion
  callback is only an atomic increment. `StreamingVoices.cpp:133-170` waits for
  the fill worker before destroying streaming state.

The root boundary is the composition of two otherwise intentional contracts:
the Common worker type promises time-critical placement with no file I/O, while
Audio/File assign that worker a cold lazy-pack read. The issue is not the main
thread consumer, callback publication, or stream lifetime ordering; those must
remain intact while the ownership and scheduling choice is resolved.

## Open design choices

No choice is selected. The frozen evidence proves the boundary violation but
does not establish the acceptable memory budget, I/O queue latency, or whether a
worker-contract change is intended.

1. **Move cold reads to a lower-priority I/O worker.** This directly removes
   file I/O from the time-critical fill worker and preserves random-access
   streaming and the main-thread consumer. It requires a defined request/result
   handoff, buffer ownership and lifetime rules, cold-read latency behavior,
   cancellation, and teardown/device-reset ordering.
2. **Make audio data resident before a time-critical fill.** This keeps the
   current fill-worker shape and removes its cold disk path. It increases memory
   use and may add startup or transition latency; eviction and device-loss
   recovery must also prove that a fill never falls back to synchronous disk
   access, while the current lazy whole-track constraint may need an explicit
   change.
3. **Redesign or explicitly narrow the worker contract.** A new worker type or
   documented exception could permit this I/O, but it weakens or splits the
   Common starvation guarantee. It would need an authority decision covering
   priority, all existing `PersistentWorker` users, blocking limits, and the
   resulting scheduling invariant rather than silently exempting Audio.

The current record makes no recommendation: choosing among these requires the
missing latency, residency, lifetime, and authority decisions above.

## Critical files

- `Common/Threading/AGENTS.md`
- `Common/Threading/PersistentWorker.cpp`
- `Engine/Source/Audio/AGENTS.md`
- `Engine/Source/Audio/AudioManager.cpp`
- `Engine/Source/Audio/StreamingVoices.h`
- `Engine/Source/Audio/StreamingVoices.cpp`
- `Engine/Source/Audio/StreamingVoice.cpp`
- `Engine/Source/File/AGENTS.md`
- `Engine/Source/File/PackChunks.cpp`

## In scope

- Determine who owns cold lazy-audio reads and which thread/priority may perform
  them.
- Compare the three choices above, including request/result handoff, latency,
  residency, reset/device-loss, cancellation, and destruction consequences.
- Trace the cold `LazyChunk` state transition and the streaming lifetime from
  `AudioManager::Update` through fill, main-thread submission, callback
  completion, `Clear`, and teardown.
- Define an observable acceptance scenario for a cold lazy-audio chunk.

## Out of scope

- Any source, shader, build, project-membership, scheduler, or data-format
  change.
- Other `PersistentWorker` users, including Graphics submission and Present.
- Audio format validation, 48 kHz behavior, static voices, one-shot playback,
  or deterministic Frame/CRC state.
- Changing the main-thread buffer consumer, callback-only atomic publication,
  or streaming wait-before-destruction lifetime contract.
- Treating this record as permission to add a priority exception or a fallback
  implementation.

## Invariants

- A cold lazy-audio refill must not perform synchronous file open, seek, or read
  on a `THREAD_PRIORITY_TIME_CRITICAL` `PersistentWorker`.
- The main thread remains the XAudio2 buffer consumer and submitter.
- XAudio2 completion callbacks remain lock-free and publish completion only by
  atomic state.
- Streaming containers are not mutated or destroyed until in-flight fill work
  has completed.
- Any selected design must state whether random-access, non-whole-track audio
  residency remains an invariant or is intentionally changed.
- Audio remains presentation-only and must not enter deterministic Frame state
  or CRCs.

## Decisive questions and checks

- Which thread owns a cold read, what priority does it run at, and what bounds
  storage-stall time without blocking a time-critical worker?
- If reads move to another worker, who owns requests, destination buffers,
  completion publication, cancellation, and errors across `Clear`, stream
  transitions, device loss, and teardown?
- If residency is chosen, what memory and transition budget is acceptable, and
  can eviction/reset prove that no cold refill falls back to disk on the fill
  worker?
- If the worker contract changes, which Common and subsystem authorities are
  updated, and does the rule remain safe for every existing `PersistentWorker`?
- In a focused cold-chunk scenario, can tracing or an equivalent deterministic
  hook prove that no synchronous open/seek/read executes on the time-critical
  fill worker while buffer submission, callback completion, stream destruction,
  and device-reset ordering remain correct?

The investigation earns an executable Plan only after one design is selected,
its ownership and authority changes are decided, and the acceptance evidence is
specific enough to implement without further architectural choices.

## Provenance

- Candidate: `CSB/shard-0004/001` — synchronous lazy-pack audio I/O on the
  `PersistentWorker` time-critical thread.
- Triage: `Temp/CppScopeBoundaryAudit/80896f33661aaab99cf180a96db54600099be652/triage-0001.md`
  (`Sol Triage 0001`, COMPLETE).
- Frozen source tree: `98a34f7ffae57858863b90f7d1f9c32be268ac5a`.
- Frozen source report: `Temp/CppScopeBoundaryAudit/80896f33661aaab99cf180a96db54600099be652/shard-0004.md`;
  manifest SHA-256 `dc285239dfeef29a28c4fd4f551e99eace320bd40c882b5bc9d89da536dc6225`.
- Frozen source SHA: `80896f33661aaab99cf180a96db54600099be652`.
- No exact duplicate was present in the live `Documents/Plans/` or
  `Documents/Investigations/` trees before this record was created; the search
  covered the candidate symbols, worker role, cold/lazy audio, and the
  time-critical file-I/O outcome.
- No source, shader, build, or scheduler change is part of this investigation.
