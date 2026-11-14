<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:53:52.834Z","dependsOn":[]} -->
# Share the Screenshot and Render-Target Dump Helpers

## Context

Found by a deep analysis of `Engine/Source/Graphics` (direct files only) and
verified independently. Pre-existing at commit `401e4b75`; line numbers are
from that commit, so locate by symbol if they moved. All lines are in
`Engine/Source/Graphics/Screenshot.cpp`.

The screenshot path (`SaveScreenshot`) and the render-target dump path
(`EncodeAndWriteDump` and its caller) are siblings that repeat three pieces of
code.

1. Failure report. `ReportCaptureFailure` (`:26-35`) logs one string at
   `kWarning` and, when the request publishes a result, publishes that string
   as the `error` result; seven early-return sites use it. Both `catch` blocks
   (`:194-203`, `:506-515`) re-implement it inline. They differ from the
   helper in two ways: they log at `kError`, and they log a prefixed text
   (`"SaveScreenshot failed: {}"`, `"DumpRenderTarget failed: {}"`) while
   publishing the bare `what()`. The rule that every failure publishes an
   error result so the deferred agent poll never hangs (`:386-387`) therefore
   has three hand-kept bodies.
2. Default output directory. The block that calls `GetTempPathW`, checks the
   length, appends `"Screenshots"`, and creates the directory is written twice
   (`:152-161`, `:406-415`), differing only in the failure text.
3. Red/blue swap. The expression that swaps the red and blue bytes of a
   32-bit pixel is written twice (`:123`, `:452`). The sites differ only in
   alpha: the screenshot forces `0xFF000000`, the dump keeps the source alpha.

Unmet criteria: capture failures publish through one body; the default
directory and the swap expression each have one definition. Authority: root
`AGENTS.md` `## Directives` (DRY; mirrored patterns stay parallel; extract
helpers only for current duplication — two current sites each).

## Design

Recommended:

1. Route both `catch` blocks through `ReportCaptureFailure`. This is a
   behavior change, not a pure merge, because the log level and the text
   differ. The author recommends passing the prefixed text as the one string:
   the log line stays attributable to its function, the published `error`
   gains the same prefix, and the level becomes `kWarning` like the other
   seven capture failures, since a failed developer capture halts nothing.
   The plan review at claim time confirms the level and the text; the
   alternatives are a log-level parameter on the helper, or the bare `what()`
   for both.
2. Add one file-local default-directory resolver used by both paths, taking
   what differs (the failure text) from its caller.
3. Add one file-local `static constexpr` function beside
   `IsBlueGreenRedAlpha` that swaps red and blue and keeps alpha; the dump
   site calls it, and the screenshot site calls it and then forces alpha, so
   both outputs stay byte-identical.

Do not merge the two encoders: alpha policy, resize, and output differ.

## Critical files

- `Engine/Source/Graphics/Screenshot.cpp`

## In scope

- The two `catch` blocks at `:194-203` and `:506-515`.
- The two default-directory blocks at `:152-161` and `:406-415`, and one new
  file-local resolver.
- The two swap expressions at `:123` and `:452`, and one new file-local swap
  function.

## Out of scope

- `ReportCaptureFailure`'s seven existing call sites.
- The default-file counters (`siScreenshot` a plain static, `siDump` an
  atomic): the dump encoders never overlap, so the difference is cosmetic and
  is not recorded as work.
- Flattening the screenshot's row/column loop and renaming its pixel buffers,
  which were judged polish.
- The capture-result fields other than the `error` text.

## Risk tier

Tier 2 — scoped behavior. Trigger: one subsystem's tool behavior — the log
level and text of two capture failure paths, and the `error` text the agent
harness reads back. Developer capture path only: no determinism/CRC,
serialization, wire, replay, or threading exposure.

## Acceptance criteria

- Neither `catch` block builds an `error` result itself; both call
  `ReportCaptureFailure`.
- `GetTempPathW` and the swap expression each appear once in the file.
- The BrokenEngineSandbox client and ModelViewer build.
- Through `/agent-harness`, a PNG and a JPG screenshot and a render-target
  dump of a four-channel 8-bit target are byte-identical to ones taken before
  the change, with and without an explicit output path. The supplied evidence
  records no harness setup.

## Coordination

No Plan below depends on this one, and it depends on none. Whichever lands
second re-derives its regions by symbol from the landed file.

- `Documents/Plans/Engine/ShareCommonPathToUtf8.md` removes the file-local
  `PathToString` from `Screenshot.cpp` and renames its four call sites, which
  sit in the encoders this Plan edits.

## Notes

- None.
