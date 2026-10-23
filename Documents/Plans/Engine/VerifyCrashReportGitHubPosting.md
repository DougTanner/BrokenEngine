<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-23T02:29:02.409Z","dependsOn":[]} -->
# Verify live crash-report posting to Coretide GitHub issues

## Context

The server posts crash reports as issues in the private `DougTanner/Coretide` repository when its app-data game
folder holds `GitHubCrashReportToken.txt` (`CrashReportPoster`, `Engine/Source/CrashReport.cpp`, server-only block):

- the constructor (`CrashReport.cpp:529`) reads and trims the token, logs
  `CrashReportPoster Posting crash reports as GitHub issues` (or `Crash reports are not posted: no GitHub token`),
  and starts a startup pass that posts the pending file and every stored `CrashReport-*.txt`, stopping at the first
  failure;
- `PostAsync` (`CrashReport.cpp:587`) posts each client report `Server::ClientCrashReport` stores
  (`Engine/Source/Network/Server/Server.cpp:882`) on a background task;
- `TryPostCrashReport` (`CrashReport.cpp:813`) posts the server's own report synchronously from the main-loop catch
  and from the server branch of `crash_report_harness_rig` (`Engine/Source/Agent/Commands/CrashReportHarnessRig.cpp:32`);
- `Post` (`CrashReport.cpp:644`) builds the title `<game name> <game version>: <exception line>` cut to at most 256
  bytes at a UTF-8 boundary, a body with `Player ID: <32 hex GUID>` for a stored client report or `Source: server`
  otherwise, a `File:` line, the report inside a fence longer than its longest backtick run, a truncation line when the
  report exceeds the 60,000-byte budget, and the labels `crash-report` and the server `game::kGameName`
  (`Broken Engine Sandbox Server`, `Projects/BrokenEngineSandbox/Source/Game.h:26`); it sends
  `Content-Type: application/json` explicitly. On HTTP 201, and on 400 or 422, it appends the posted trailer
  `\nThis crash report was posted as a GitHub issue.\n`; a report ending with that trailer is skipped.

The change that added posting verified everything observable without a token: the empty DxDiag section, the
`%USERPROFILE%` replacement, the stored copy equal to the client report minus its sent trailer, the corrupt-header
rejection, the pending-file rename and single send, the agent-harness process-check headline and stale handling, and
the `no GitHub token` log line. Its one unmet acceptance check is the live post itself, which the user deferred: no run
has ever created an issue, so no setup for it has been observed. The steps in `## Design` are derived from the
no-token harness runs of that change.

Outstanding external questions this run settles by observation: GitHub documents no issue body or title limit (the
60,000-byte body budget rests on a community-reported 65,536-character API error; the title cut is 256 bytes);
whether a label that does not exist is created at issue creation, and whether fine-grained Issues write counts as the
push access GitHub requires to apply labels (GitHub drops labels silently, not with an error, when it does not); and
whether GitHub accepts the request as sent (explicit `Content-Type`).

## Design

The run is outward-facing: it contacts GitHub and creates four real issues in `DougTanner/Coretide`, which the user
may delete afterwards. Run it only after the user's explicit go-ahead in the claiming session, and ask the user then
which token placement to use.

### Prerequisites the user performs

1. Create a fine-grained GitHub token limited to `DougTanner/Coretide` with Issues read/write.
2. Create the labels `crash-report` and `Broken Engine Sandbox Server` in `DougTanner/Coretide`.
3. Save the token as `%APPDATA%\Broken Engine Sandbox Server\GitHubCrashReportToken.txt`.

### Token placement (user choice at go-ahead)

- Option 1, recommended: the harness worker copies the token file, never printing or reading its content into the
  transcript, into `<app-data root>\Broken Engine Sandbox Server\GitHubCrashReportToken.txt`, where the app-data root
  is a fresh directory under the worktree's gitignored `Temp\`; it runs the checks and then deletes the copy by
  `-LiteralPath`. Recommended because the user's real app-data folder, its autosave, and any reports already stored
  there stay untouched, and the startup pass posts only reports this run created.
- Option 2: the harness server is launched with `-AppDataDirectory` set to the user's real `%APPDATA%`, so it reads the
  token in place. The server then writes its autosave and crash reports into the user's real server folder, and its
  startup pass posts every unposted report already stored there.

### Run order

Build Debug|x64 of the client and server through `/compile` (the harness rig and `send_raw_packet` need a
`kbDebugInput` build), then drive both through `/agent-harness` with one fresh app-data root (Option 1) for all phases.
The rig exits are expected process-check findings, as in the no-token runs.

1. Phase A, no token in the server folder. Launch server and client and connect. From the client, `send_raw_packet` one
   `kClientCrashReport` packet whose payload is a report with a valid current header (take the leading header lines
   from a report a rig wrote, including `Crash report version: 1` and the exception line) padded with plain text to
   more than 60,000 and at most `kiMaximumCrashReportSize` (1 MiB, `Engine/Source/Network/NetworkProtocol.h:110`)
   bytes; the packet layout is the `kClientCrashReport` row of `Documents/Architecture/Network.md`. The server log
   shows one `Server::ClientCrashReport Stored` line and no posting line. Then run server `crash_report_harness_rig`;
   the server writes `Broken-Engine-Sandbox-Server-Crash-Report.txt` and exits. Quit the client.
2. Place the token (chosen option).
3. Phase B. Launch the server only. Its startup rename moves the server report to
   `Broken-Engine-Sandbox-Server-Crash-Report-Pending.txt`; the startup pass posts the pending file and the stored large
   client report (checks 8(d) and 8(e)).
4. Launch the client and connect; run client `crash_report_harness_rig`. The server stores the client's report and
   posts it through `PostAsync` (check 8(a)).
5. Quit and relaunch the server; no new issue appears (check 8(b)).
6. Run server `crash_report_harness_rig`; the server posts its report synchronously before it exits (check 8(c)).
7. Read every created issue read-only with `gh issue view <number> --repo DougTanner/Coretide --json title,body,labels`
   (author's recommendation, covered by the same go-ahead; otherwise the user reports what the issues show).
8. Option 1: delete the token copy. Report the issue numbers so the user can delete them.

Change Workflow tier: Tier 3, triggered by the trust boundary: client report text forwarded to an external service
under a secret token. With every check passing, the run changes no tracked file beyond this Plan's removal. A failed
check is diagnosed through `/external-diagnose-bug` and fixed inside `## In scope` under that tier. No determinism,
CRC, serialization, `.pack`, replay, or wire exposure: posting runs on background tasks or after `HandleException`,
outside the simulation.

## Critical files

- `Engine/Source/CrashReport.cpp` — `CrashReportPoster::CrashReportPoster`, `CrashReportPoster::PostAsync`,
  `CrashReportPoster::Post`, `TryPostCrashReport`.
- `Engine/Source/CrashReport.h` — `CrashReportPoster`.
- `Engine/Source/Network/Server/Server.cpp` — `Server::ClientCrashReport`.

## In scope

- The live run in `## Design` and its observations.
- Only when a check fails: the fix inside `CrashReportPoster`'s constructor, `PostAsync`, and `Post`, and
  `TryPostCrashReport`, in `Engine/Source/CrashReport.cpp` and `Engine/Source/CrashReport.h`, plus the matching
  `Engine/Source/AGENTS.md` `## Crash Reporting` sentence through `/update-claude-docs`.

## Out of scope

- Re-running the no-token checks listed in `## Context`.
- Deploying the token to the hosted server VM (`Documents/Plans/ChangeWorkflow/DeployAndRunHostedServer.md`).
- One client process per app-data folder (`Documents/Plans/Engine/OneClientPerAppDataFolder.md`).
- Creating labels, tokens, or repositories from code or scripts; deleting the created issues.
- Changing the body budget, the title cut, or the 400/422 handling when every check passes.
- `Server::ClientCrashReport` receipt validation and the per-UTC-day store cap.

## Acceptance criteria

1. 8(a): the client rig crash in phase B produces exactly one Coretide issue whose title is
   `Broken Engine Sandbox <game version>: <exception line>`, whose body holds `Player ID:` with the 32-hex GUID that
   ends the stored `CrashReport-*.txt` file name, the `File:` line, and the report inside the fence; the stored file
   ends with the posted trailer; the server log shows `CrashReportPoster::Post Posted File: <that file name>`. Report
   whether `crash-report` and `Broken Engine Sandbox Server` were applied; a missing label is reported, not a failure.
2. 8(b): after the server restart, no second issue exists for any report, and the server log shows no new `Posted`
   line.
3. 8(c): the server rig produces exactly one issue with `Source: server` before the server exits, and
   `Broken-Engine-Sandbox-Server-Crash-Report.txt` ends with the posted trailer.
4. 8(d): the first token-holding start posts the phase-A stored client report and the pending server report once each,
   both files end with the posted trailer, and `Broken-Engine-Sandbox-Server-Crash-Report.txt` does not exist after
   that start until step 6 writes it.
5. 8(e): the issue for the over-budget report ends with
   `Report truncated: <shown> of <total> bytes; the full report stays on the server as <file name>.`, and GitHub
   accepted it (201, so the 60,000-byte budget fits the body limit).
6. 8(f): neither log shows a desync or CRC mismatch during the run.
7. Option 1 only: the token copy no longer exists after the run, and the token never appears in the transcript.

## Notes

- A 400 or 422 answer logs `CrashReportPoster::Post Not posted, GitHub rejected the request File: <name> Status:
  <code>` and still appends the posted trailer, so that report is never retried and stays on disk. If GitHub's
  anti-spam rejection answers 422 for a rapid issue during the run, the matching criterion fails without an issue;
  report it to the user with the status, because the accepted design trades that retry for never blocking later
  reports.
- Any other non-201 status or a WinHTTP failure logs a `kWarning` and leaves the report unposted for the next start.
