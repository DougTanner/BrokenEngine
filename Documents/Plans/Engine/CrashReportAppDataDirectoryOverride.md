<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-13T11:47:29.244Z","dependsOn":[]} -->
# Honour the AppData root override on the crash-report path so worktrees stop overwriting each other's crash reports

## Context

`--app-data-directory` now gives each worktree its own AppData root for saves, settings, caches, and every
`F7.replay.*` artifact: `FileManager::FileManager()` uses `gLaunchOptions.appDataDirectory` in place of
`SHGetKnownFolderPath(FOLDERID_RoamingAppData)` when it is set. The crash-report path did not change and was
explicitly excluded by `Documents/Plans/Engine/AppDataDirectoryLaunchOverride.md`'s `## Out of scope`, so it is
pre-existing and outside that boundary.

`HandleException` (`Engine/Source/CrashReport.cpp:26-46`) still resolves
`SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE)` on its own, appends `game::kGameName`, and
writes a fixed filename built at `:48-51` (`<Game-Name>-Crash-Report.txt`, spaces replaced by hyphens). It never
reads `gLaunchOptions.appDataDirectory`. The path is reached from `wWinMain`'s `catch` blocks
(`Engine/Source/Main.cpp:826-837`) whenever the user answers No to the "Save crash report to desktop?" prompt,
and unconditionally for an agent-launched process once the agent-mode silent-save capability lands.

The consequence is the same class of silent cross-session destruction the AppData override just removed for
saves and replays: two worktrees running the same game crash into one fixed per-user file, so the second crash
overwrites the first session's report with no error and no warning, and the evidence a diagnosis depends on is
simply gone.

`Projects/BrokenEngineSandbox/Documents/AgentHarness.md:36` already documents the crash report as a stated
exception to the per-worktree root, so today's documentation is accurate. This Plan closes the gap; it is not
correcting a wrong statement, and that sentence has to be updated with the code.

## Design

Mirror `FileManager`'s root selection on the crash path, but read the override from fixed static storage rather
than from the `std::filesystem::path` member, because `HandleException` is reachable from the `SIGABRT` handler
during suspected heap corruption and must not build its path out of heap memory
(`Engine/Source/AGENTS.md` Crash Reporting; the `:16` and `:44` comments in `CrashReport.cpp`).

This is not a choice between designs. Reading `gLaunchOptions.appDataDirectory` directly at crash time would
dereference a heap buffer on exactly the path the documented contract forbids doing so, and adding a separate
crash-only directory option would duplicate an override the harness already passes. The snapshot is the only
shape that satisfies both the existing contract and the existing option.

- `Engine/Source/CrashReport.h`: declare `void SetCrashReportAppDataDirectory(const wchar_t* pcDirectory);`
  beside the existing declarations.
- `Engine/Source/CrashReport.cpp`: add a file-static `static wchar_t spcAppDataOverride[MAX_PATH + 1] {};`
  beside the existing `sDxDiag`. `SetCrashReportAppDataDirectory` copies the supplied directory into it with
  `wcscpy_s` when the source length fits `MAX_PATH`, and leaves the buffer empty when it does not — a truncated
  root would write the report into the wrong directory, so an over-long path keeps today's behaviour instead.
  The copy runs at startup, never on the crash path.
- `Engine/Source/CrashReport.cpp` `HandleException` `else` branch (`:26-46`): when `spcAppDataOverride[0]` is
  non-zero, `wcscpy_s` it into `spcPath` in place of the `SHGetKnownFolderPath` call and its failure fallback;
  otherwise run that existing block verbatim. The `L"\\"` + `pcGameName` append and the allocator-free
  `CreateDirectoryW` at `:41-45` stay unchanged and run in both cases, so the report keeps its distinct
  per-game subdirectory beneath the supplied root and the single-level create stays valid — the option is a
  trust boundary that already proved the root exists. The `IDYES` Desktop branch (`:21-24`) is untouched.
- `Engine/Source/Main.cpp` `wWinMain`: after `ParseLaunchOptions()` returns true and before the `try` block,
  call `engine::SetCrashReportAppDataDirectory(gLaunchOptions.appDataDirectory.c_str())` when
  `gLaunchOptions.appDataDirectory` is non-empty. This is startup code outside the tracked loop, so touching
  the path object here is fine; only the crash path is constrained.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness.md:36`: replace the sentence stating the crash report as
  an exception with one stating that the crash report is written beneath `--app-data-directory` too, naming the
  resulting `<app-data-root>\<Game Name>\<Game-Name>-Crash-Report.txt` identity the harness baselines.
- `Engine/Source/AGENTS.md` Crash Reporting: keep the fixed-buffer rule and add that the AppData override
  reaches the crash path through a startup-populated fixed buffer, never through the launch-option object.

No file format, serialized layout, replay component name, or CRC input changes; only the directory one
diagnostic text file is written into.

## Critical files

- `Engine/Source/CrashReport.cpp` — `HandleException` AppData resolution and the new snapshot buffer/setter
- `Engine/Source/CrashReport.h` — the new setter declaration
- `Engine/Source/Main.cpp` — the `wWinMain` startup call that populates the snapshot
- `Engine/Source/AGENTS.md` — Crash Reporting contract sentence
- `Projects/BrokenEngineSandbox/Documents/AgentHarness.md` — the documented crash-report identity

## In scope

- The `spcAppDataOverride` fixed buffer, `SetCrashReportAppDataDirectory`, and its declaration
- The `HandleException` `else` branch root selection only
- The single `wWinMain` startup call that populates the snapshot
- The two documentation sentences named above

## Out of scope

- The `IDYES` Desktop branch and the `SHGetKnownFolderPath` failure fallback that stays behind it
- The crash-report filename, its contents, DxDiag, callstack, and log-buffer sections
- The interactive `MessageBox` prompt and any agent-mode silent-save behaviour
- Per-run or per-session crash-report filenames; two runs in the same worktree still share one file
- `FileManager`, the temp-directory resolution, replay naming, and the harness lock
- Any change to `--app-data-directory` parsing or validation

## Risk tier and invariants

Tier 3 — the change is on the exception path, which must stay correct under heap corruption, and it moves an
artifact identity the agent-harness launch/cleanup contract baselines, so it spans independently owned
subsystems (engine crash handling and the harness documentation contract). Invariants: no heap allocation and
no heap dereference is added to `HandleException`'s path construction; every write into `spcPath` remains a
bounded fixed-buffer operation; an absent `--app-data-directory` produces byte-identical behaviour to today,
including the `SHGetKnownFolderPath` failure fallback; client and server keep distinct filenames through
`game::kGameName`.

## Acceptance criteria

- Launched without `--app-data-directory`, a forced exception with the prompt answered No writes the report to
  `%APPDATA%\<Game Name>\` exactly as today.
- Launched with `--app-data-directory <existing absolute path>`, the same forced exception writes the report to
  `<that path>\<Game Name>\` and leaves the shared per-user file untouched.
- Answering Yes still writes the report to the Desktop in both cases.
- A supplied root longer than `MAX_PATH` falls back to the unmodified per-user behaviour rather than writing a
  truncated path.
- Reviewed evidence that the crash path's added code performs no allocation and reads no heap-owned string.

## Coordination

`Documents/Features/Agent/AgentModeCrashReportHandoff.md` is a manually executed Feature covering agent-mode
silent save and harness crash discovery, and it names the same two report identities in
`Engine/Source/CrashReport.cpp`. It is not a scheduler input, so it carries no dependency edge; whichever lands
second updates the report-identity wording so the harness baselines one agreed path per launch.
