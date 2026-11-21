<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-20T23:51:27.173Z","dependsOn":[]} -->
# Drop the Separate Pipeline Cache Seek Check

## Context

`ReadPipelineCacheFile`
(`Engine/Source/Graphics/Managers/DeviceManager.cpp:396`) seeks back to the
beginning of the cache file and then tests the stream on its own
(`:434-439`), logging `Discarded unreadable pipeline cache ({} bytes)` and
returning empty on failure. The read that follows (`:441-447`) already answers
that case:

- The stream is good when the seek runs: the size test at `:422` passed.
- A failed `seekg(0, std::ios::beg)` sets failbit.
- The following `read` then builds a failing sentry and extracts nothing, so
  `gcount()` is 0, below `iSize` (at least the header minimum, `:428`).
- The read test at `:443-446` then logs the identical text with the same size
  and returns empty.

The only difference is that the `iSize`-byte vector (at most
`kiMaxPipelineCacheBytes`, at startup only) is allocated before the failure is
seen, on a path practically unreachable for an open regular file that just
reported its size. The separate check exists only because the Plan that wrote
this function chose one return per check; that was an agent-authored Plan
choice, not a user decision, so no decision is pending here.

## Design

Delete the post-seek stream test and its log and return, keeping the
`seekg(0, std::ios::beg)` call; the read's `gcount()` and stream test answer a
failed seek with the same log text and return value. The author recommends this
because it removes one redundant check with no log or return difference.

## Critical files

- `Engine/Source/Graphics/Managers/DeviceManager.cpp`

## In scope

- `ReadPipelineCacheFile`: delete the `if (!fileStream)` block that follows
  `seekg(0, std::ios::beg)`.

## Out of scope

- The `std::filesystem::exists` pre-check and its `filesystem_error` catch at
  the top of `ReadPipelineCacheFile`.
- Every other check, log line, and return in `ReadPipelineCacheFile`.

## Acceptance criteria

- Clean `/compile` builds of the BrokenEngineSandbox client and of ModelViewer
  (both compile `DeviceManager.cpp`).

## Notes

- Change Workflow tier: Tier 1. Trigger: local behavior-preserving change; no
  signature, log, or return-value change.
- Client-only startup path; no CRC, wire, save, or threading exposure.
