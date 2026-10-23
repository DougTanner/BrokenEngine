<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-23T00:52:18.770Z","dependsOn":[]} -->
# Allow only one client process per app-data folder

## Context

The client is not single-instance: `kbSingleInstance` is `true` only for `BT_SERVER`
(`Engine/Source/EngineSwitches.h`, the `kbSingleInstance` block), so `ProcessMain`'s named-mutex check
(`Engine/Source/Main.cpp`, the `if constexpr (kbSingleInstance)` block keyed on `game::kGameName`) never runs in the
client. Two client processes started without `--app-data-directory`, or with the same one, therefore share one
app-data game folder (`FileManager::FileManager`, `Engine/Source/File/FileManager.cpp`: the root plus
`game::kGameName`).

Both resolve the same live crash report path (`ResolveCrashReportPaths`, `Engine/Source/CrashReport.cpp`), and
`HandleException` opens it through `common::CrashFileWriter` (`Common/FileUtils.h`), which uses `CREATE_ALWAYS`, so a
crash in one client overwrites the other's report. The change that made the server post crash reports as GitHub issues
added a startup `RenamePreviousCrashReport` (`CrashReport.cpp`, called from `ProcessMain` before any other thread
starts) that moves the live report to one shared pending file, which `PollPendingCrashReport` sends to the server. With
two clients in one folder, the second client's rename can move a report the first client has just written, and both
clients then send the same pending report, so the server stores it twice and posts it as two GitHub issues.

That change recorded this as a known pre-existing limitation outside its boundary (found by code inspection; no
`/agent-harness` run reproduced it, and the supplied evidence records no harness setup).

AgentHarness is unaffected today: an additional simultaneous client always gets its own app-data root
(`.agents/skills/agent-harness/references/launch.md`, `### Additional simultaneous client`).

## Design

Author's recommendation: a client that finds another client already using its app-data game folder exits at startup,
before `RenamePreviousCrashReport`, the same way a second server does. That removes the shared-folder state itself
instead of special-casing each file the two processes would otherwise share.

- Lock: the client opens a lock file in the folder that holds the live crash report (the same directory as
  `ReportPath()`), using `CreateFileW` with share mode `0` and `OPEN_ALWAYS`, and keeps the handle open for the process
  lifetime, the same way `ProcessMain` keeps the server's mutex handle (`ScopedHandle pMutex`). The operating system
  releases the handle when the process exits or crashes, so no stale lock survives. Recommended name:
  `<Game-Name>-Client.lock`, built in `ResolveCrashReportPaths` as another fixed buffer beside the live and pending
  paths, emptied when it does not fit. It uses the same base as those paths, including the working-directory-relative
  fallback. A file lock is recommended over a named mutex, because a mutex name cannot contain `\` and is length-limited,
  while the lock file is keyed on the folder exactly.
- Second client: when the open fails with `ERROR_SHARING_VIOLATION`, the client logs `kError` ("Another client is
  already using this app-data folder" plus the folder), shows that text in the existing
  `MessageBox(..., MB_OK | MB_ICONERROR | MB_SYSTEMMODAL)` form only when `gLaunchOptions.iAgentPort == 0` (as
  `ProcessMain`'s `FileManager` catch does), and returns `0` before `RenamePreviousCrashReport`. Any other open failure
  logs `kWarning` with the error code and continues, which keeps today's behavior.
- Server: unchanged. Its game-name mutex already prevents a second server.

Alternative considered and not recommended: letting the second client run with crash-report ownership disabled. Both
clients would still truncate the one live report and share the settings and save files.

Risk tier: Tier 2, triggered by client startup behavior in one subsystem (crash report and process start). It changes
no determinism/CRC, wire, save, `.pack`, replay, or threading surface, and it runs on the main thread before any other
thread starts. The `kiCrashReportVersion` report format is unchanged.

## Critical files

- `Engine/Source/CrashReport.cpp`, `Engine/Source/CrashReport.h`
- `Engine/Source/Main.cpp`

## In scope

- `ResolveCrashReportPaths`: build the client lock-file path.
- A new client-only function in `CrashReport.cpp`, declared in `CrashReport.h`, that opens the lock and returns whether
  this process holds the folder. The handle stays owned for the process lifetime.
- `ProcessMain` (`Main.cpp`): under `BT_CLIENT`, call it after `ResolveCrashReportPaths` and before
  `RenamePreviousCrashReport`, and exit with the log line and dialog above when another client holds the folder.
- `Engine/Source/AGENTS.md` `## Crash Reporting`: one line stating the one-client-per-folder rule, updated through
  `/update-claude-docs`.

## Out of scope

- The server's single-instance mutex and `kbSingleInstance`.
- The Desktop crash-report copy, which every client and server share by design.
- Settings, save, or cache file sharing as separate work. They are covered only as a side effect of the lock.
- AgentHarness scripts and launch rules.
- Pending-report deduplication on the server or GitHub side.

## Acceptance criteria

1. `/compile` builds `BrokenEngineSandbox` client and server `Debug|x64`.
2. With one client running against an app-data root, a second client started with the same `--app-data-directory`
   exits at startup, logs the `kError` line naming the folder, and leaves the folder's live and pending crash reports
   unchanged. The executor runs this check by hand or through `/agent-harness`. The harness's own client launches use
   separate roots, so the executor confirms that the harness accepts the shared root before choosing it.
3. After the first client exits, a client started with the same root starts normally.
4. Two clients with different `--app-data-directory` roots both start (the harness `client-b` launch).

## Notes

- `RenamePreviousCrashReport` and `PollPendingCrashReport` come from the change that made the server post crash
  reports as GitHub issues, which lands with this Plan file.
