<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:41:44.875Z","dependsOn":[]} -->
# Loader takes the queued request when its audio claim fails

## Context

Found by the architecture phase of an analysis of `Engine/Source/File` (the
directory, not its subdirectories), verified against commit `7dae9a27`. It is
pre-existing debt of the lowest priority in that analysis, not an unmet
acceptance criterion of any change.

In `PackChunkLoader::LoadingThread`
(`Engine/Source/File/PackChunkLoader.cpp`), the locked section reads whether
an audio read is queued at line 178 (`HasQueuedAudioRead`) and claims one at
line 204 (`TryClaimAudioRead`, `Engine/Source/File/PackChunks.cpp:736-756`).
These are separate reads of the entry ownership words. The main thread's
`PackChunks::CancelChunkRead` (`PackChunks.cpp:673-709`) changes an ownership
word without `mQueueMutex` and publishes no wake, and it is a real actor
(`Engine/Source/Audio/StreamingVoice.cpp:188`).

So this interleaving is reachable: a non-realtime chunk request is queued,
`mbPreferAudio` is true, the loader sees `bAudioWork` true at line 178, the
main thread cancels the read, and the claim at line 204 fails. The loader
then has `bHaveWork` false and sleeps at line 222 with the chunk request
still queued.

This is lost throughput only, never a stall or a lost job:

- a second loader that examines the queue after the cancel sees no queued
  audio and pops the request (line 193);
- any later queued audio read publishes a wake (`PackChunks.cpp:653-660`)
  that ends the sleeper's wait;
- a realtime request bypasses the branch (lines 176, 193);
- so `WaitForChunks` and `WaitForLoadersIdle` always complete.

The cost is one loader idle until the next `PublishWake`, which for a bulk
request can be the rest of the batch. `Engine/Source/File/AGENTS.md`
`## Packed Assets` ("audio and other work then alternate so neither starves")
is not violated.

Unmet criterion: a loader never sleeps while a request is queued.

## Design

Recommended, local to the locked section at `PackChunkLoader.cpp:193-213`:
when the audio claim fails and `bExistingWork` is true, pop the queued
request in the same lock hold instead of falling through to the wait.
Rationale: the request is already known to be there, and the lock that
guards the queue is already held, so no new wake or state is needed. Keep
the `mbPreferAudio` alternation meaning as it is for the cases where the
claim succeeds.

Do not add a wake to `CancelChunkRead`: that changes the main-thread side of
the protocol for a case the loader can settle itself.

## Critical files

- `Engine/Source/File/PackChunkLoader.cpp`

## In scope

- `PackChunkLoader::LoadingThread`: the work-selection branches inside the
  `mQueueMutex` hold, lines 193-213.

## Out of scope

- `PackChunks::CancelChunkRead`, `TryClaimAudioRead`, and every ownership
  transition.
- The harness rig's loader-stall override (lines 182-192).
- The wake sequence, `miActiveLoadJobs`, and both waits.
- Request priority and the realtime path.

## Risk tier

Tier 3 — invariant/integration. Trigger: threading — it changes which job a
loader thread selects under `mQueueMutex` in the interleaving with a
main-thread cancel. Invariants to hold: every accepted job is popped exactly
once and counted in `miActiveLoadJobs` under the lock; shutdown still wins
(line 170); a stalled-loader override still yields no work; audio and other
work still alternate when both are present.

## Acceptance criteria

- By code reading: no path through the locked section leaves `bHaveWork`
  false while `mRequestQueue` is non-empty, outside the harness rig stall.
- The BrokenEngineSandbox client and server and ModelViewer build without
  errors or new warnings.
- Live, through `/agent-harness`: a client starts, loads its lazy chunks,
  and plays streamed music with no loader error in its log. The analysis
  recorded no harness setup that reproduces the interleaving, and none is to
  be added for it; the changed branch is verified by code reading.

## Coordination

No mandatory nondirectional coordination.
