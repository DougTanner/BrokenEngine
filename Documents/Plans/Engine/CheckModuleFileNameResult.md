<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:43:06.179Z","dependsOn":[]} -->
# Check the GetModuleFileNameW result in FileManager

## Context

Found by the in-function phase of an analysis of `Engine/Source/File` (the
directory, not its subdirectories), verified against commit `7dae9a27`. It is
pre-existing debt of low value, not an unmet acceptance criterion of any
change.

When no `--data-directory` is given, the `FileManager` constructor derives
the data directory from the executable path:
`Engine/Source/File/FileManager.cpp:114` calls `GetModuleFileNameW` with
`nullptr`, `pcDirectory`, and
`static_cast<DWORD>(std::size(pcDirectory) - 1)`, and discards the result.
The path then feeds `PackChunks` (line 121), whose first failure
would be the manifest check at `Engine/Source/File/PackChunks.cpp:317`. The
buffer is the one `GetTempPathW` filled at line 94, so a call that left it
untouched would yield a data directory derived from the temp path, and the
launch would fail with a diagnostic that names the wrong cause.

The launch already fails loudly in that case; the gain is a correct
diagnostic on a rare path. Repository precedent checks this call:
`Projects/ModelViewer/Source/ModelViewerApplication.cpp:145` and
`DataPacker/Source/Main.cpp:894-897`. `GetTempPathW` at line 94 has the same
unchecked shape.

What the API writes and returns on truncation or failure was not proven
during the analysis, so the root cause is confirmed only as "result
unchecked"; the size of the consequence depends on the verification below.

Unmet criterion: a failed executable-path query is reported as itself
(`Engine/Source/File/AGENTS.md` `## File Contracts`: directory setup
failures throw `std::runtime_error`).

## Design

Verification the executing session must do first, through
`/verify-external-claims`; it is unverified: the return value and buffer
contents of `GetModuleFileNameW` (a) when the path does not fit `nSize`, and
(b) when the call fails with a zero return. Record the verdict in the
change's report. If the verdict shows neither case can produce a wrong
directory silently, report that and make no change.

Then, recommended: test the result at line 114 for the failure and
truncation conditions the verdict names, and on either throw through the
constructor's existing `std::runtime_error` path with a message naming the
executable-path query, in the style of `FailCreateDirectory`. Follow the
form of the two precedent sites rather than inventing one.

Whether `GetTempPathW` at line 94 gets the same check is a trivial choice
for the executing session: add it only if that call still exists when this
Plan runs (another Plan removes the temp directory) and the same verdict
covers it at no extra mechanism.

## Critical files

- `Engine/Source/File/FileManager.cpp`

## In scope

- The `GetModuleFileNameW` call in the `FileManager` constructor and the
  statements that consume its buffer (lines 114-118).
- The `GetTempPathW` call at line 94, under the condition above.

## Out of scope

- The explicit `--data-directory` branch and `LaunchOptions`.
- AppData directory setup and `FailCreateDirectory`.
- Long-path support or a larger buffer.
- ModelViewer's and DataPacker's own calls.

## Risk tier

Tier 2 — scoped behavior. Trigger: one subsystem's runtime behavior — a
startup failure path gains a throw with its own diagnostic. It rides the
existing catch around `FileManager` construction
(`Engine/Source/AGENTS.md` `## Startup and Main Loop`), so no new catch or
exit route is added.

## Acceptance criteria

- The report records the external verdict.
- A failed or truncated executable-path query throws `std::runtime_error`
  from the constructor before `PackChunks` is built (code reading: nothing
  reaches it in a normal launch).
- The BrokenEngineSandbox client and server and ModelViewer build without
  errors or new warnings, and the client and server start without
  `--data-directory`.

## Coordination

No mandatory nondirectional coordination.
