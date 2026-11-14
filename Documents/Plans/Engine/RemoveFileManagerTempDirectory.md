<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:42:20.073Z","dependsOn":[]} -->
# Remove FileManager's unused temp directory

## Context

Found by the architecture phase of an analysis of `Engine/Source/File` (the
directory, not its subdirectories), verified against commit `7dae9a27`. It is
pre-existing debt, not an unmet acceptance criterion of any change.

`FileManager` resolves, creates, and exposes a temp directory nothing uses:

- `FileFlags::kTempDirectory` (`Engine/Source/File/FileManager.h:11`) and
  `FileManager::mTempDirectory` (`FileManager.h:173`) appear only there and
  at `Engine/Source/File/FileManager.cpp:95-102,138-140`. No caller in this
  repository passes the flag or reads the member.
- The constructor (`FileManager.cpp:93-103`) still creates
  `%TEMP%\<kGameName>` at every start and throws through
  `FailCreateDirectory` when it cannot, so a launch can fail over a directory
  `FileManager` never reads or writes.

The directory itself has one user by path:
`Engine/Source/Profile/ProfileManager.cpp:75-81` writes `ProfileDump.csv`
there. It computes the path itself, and `common::DiagnosticLog` creates the
parent (`Common/Log/DiagnosticLog.cpp:8`), so it does not depend on
`FileManager`. `Engine/Source/Graphics/Screenshot.cpp:153,407` use
`%TEMP%\Screenshots`, a different directory.

Unmet criterion: `FileManager` resolves only the roots
`Engine/Source/File/AGENTS.md` `## File Contracts` names (the asset data root
and the user AppData root).

## Design

Verification the executing session must do first; it is unverified: no other
game project uses the flag or the member. The analysis could not search the
private game project's source. Search the primary checkout's
`Projects/Coretide` source for `kTempDirectory` and `mTempDirectory`. If
either is used there, stop and report it instead of deleting.

Then, recommended:

1. Delete `FileFlags::kTempDirectory` (`FileManager.h:11`) and
   `mTempDirectory` (`FileManager.h:173`).
2. Delete the temp-directory setup at `FileManager.cpp:94-103`. Keep the
   `pcDirectory` buffer declared at line 93: `GetModuleFileNameW` reuses it
   at line 114.
3. Delete the `kTempDirectory` branch of `GetFilePath`
   (`FileManager.cpp:138-141`).

## Critical files

- `Engine/Source/File/FileManager.h`
- `Engine/Source/File/FileManager.cpp`

## In scope

- `FileFlags::kTempDirectory` and `FileManager::mTempDirectory`.
- The temp-directory block in the `FileManager` constructor.
- The `kTempDirectory` branch in `FileManager::GetFilePath`.

## Out of scope

- `ProfileManager`'s own use of `%TEMP%\<kGameName>` and the screenshot
  directory.
- The AppData and data directory setup, `FailCreateDirectory`, and the
  `GetModuleFileNameW` call.
- Every other `FileFlags` value and their numeric values.

## Risk tier

Tier 2 — scoped behavior. Trigger: one subsystem's runtime behavior — a
launch no longer creates the temp directory and can no longer fail on it —
plus a public member and flag removed from `FileManager.h`, which `Engine.h`
aggregates. No file format, wire, or determinism exposure. The header edit
rebuilds both precompiled headers once.

## Acceptance criteria

- The report records the private game project search and its result.
- No `kTempDirectory` or `mTempDirectory` remains in the repository.
- The BrokenEngineSandbox client and server and ModelViewer build without
  errors or new warnings, and the client and server start.

## Coordination

No mandatory nondirectional coordination.
