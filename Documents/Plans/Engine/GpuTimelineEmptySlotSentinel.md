<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-20T23:51:25.652Z","dependsOn":[]} -->
# GPU Timeline Ring Slots Default to an Impossible Sequence

## Context

`GpuStallTimeline::GpuFrameTimeline::iSequence` defaults to 0
(`Engine/Source/Profile/ProfileManager.h:388`), and the ring `mGpuTimeline`
(`:402`) is indexed by submission sequence modulo capacity. Submission
sequences start at 1: the only producer is `++miGpuSubmissionSequence`
(`Engine/Source/Profile/ProfileManager.cpp:382`; the member starts at 0,
`ProfileManager.h:545`, and Clear or Destroy never reset it). Because an empty
slot holds 0, a sequence of 0 would match an empty slot 0, so three readers
guard against 0 explicitly in `Engine/Source/Profile/GpuStallTimeline.cpp`:

- `FenceObserved` returns when `iSequence == 0` (`:44-47`); its sequence comes
  from `mGpuSubmissions` and is 0 after `GpuRead` clears it
  (`ProfileManager.cpp:411`, `:536`).
- `ImageWait` returns when `iNewestSequence == 0` (`:60-63`).
- `LogGraphicsStall` starts its `bSetPending` test with
  `iNewestSequence != 0 &&` (`:81`); without it, a 0 would match slot 0 and
  log "timeline after 0 retires".

`ImageWait` and `LogGraphicsStall` receive `miGpuSubmissionSequence`, which is
0 before the first submission.

Slot writers: `GpuSubmitted` assigns the whole slot with `.iSequence`
(`ProfileManager.cpp:389-396`), and `Clear()` runs `mGpuTimeline.fill({})`
(`GpuStallTimeline.cpp:25`, called at `ProfileManager.cpp:128` and `:728`).
With a default member initializer of -1, both the value-initialized array and
`fill({})` yield -1.

## Design

Change the `iSequence` default member initializer to -1, with a one-line
comment that submission sequences start at 1, so an empty or cleared slot never
matches a sequence, 0 included. Then delete the three zero guards. A 0 sequence
then never matches any slot, so each removed guard's case takes the existing
mismatch path with the same result: `FenceObserved` and `ImageWait` return, and
`bSetPending` is false.

This adds a second "empty" convention (-1) beside the 0-means-none used by
`miPendingStallSequence` and `GpuSubmission::iSequence`; the initializer
comment covers it.

## Critical files

- `Engine/Source/Profile/ProfileManager.h`
- `Engine/Source/Profile/GpuStallTimeline.cpp`

## In scope

- `GpuStallTimeline::GpuFrameTimeline::iSequence`: default -1 and its comment.
- `GpuStallTimeline::FenceObserved`: delete the `iSequence == 0` early return.
- `GpuStallTimeline::ImageWait`: delete the `iNewestSequence == 0` early
  return.
- `GpuStallTimeline::LogGraphicsStall`: delete the `iNewestSequence != 0 &&`
  term of `bSetPending`.

## Out of scope

- `miPendingStallSequence` and `GpuSubmission::iSequence`, which keep 0 as
  none.
- The retired-submission loop bound in `GpuStallTimeline.cpp`
  (`std::max(1i64, ...)`), which also keeps the modulo index non-negative.
- The two comments above `bSetPending` in `LogGraphicsStall`.

## Acceptance criteria

- Clean `/compile` builds of the BrokenEngineSandbox client and of ModelViewer
  (both compile `GpuStallTimeline.cpp`).

## Notes

- Change Workflow tier: Tier 1. Trigger: local behavior-preserving change to
  client-only `kbProfilingFrameSpike` diagnostics; no signature change.
- No CRC, wire, save, or threading exposure: all ring access is on the main
  thread.
