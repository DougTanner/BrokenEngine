# Agent Harness Launch

The launch recipe every game project shares. Its project values come from the
selected project's hub, `Projects/<Project>/Documents/AgentHarness.md`:
`<Project>`, `<server process-name prefix>`, `<server game name>`, and
`<client game name>` from its `## Launch values` table, and the `<hub ...>`
output directory and executable names from its
`## Machine-readable launch configuration`. The project's own
`Projects/<Project>/Documents/AgentHarness/launch.md` holds its game-specific
launch notes and launch caveats.

## Launch

Launch each role through the skill's launch script, `Invoke-HarnessLaunch.ps1`, which owns the generic launch requirements, the game argument set, the process-check sequence, and the readiness wait; this document supplies only each call's values. Every fence below is its own independent shell call and they run in the order shown; assign each fence's path values locally and substitute values retained from an earlier fence as literals.

The server is single-instance per machine, so a harness server started while any `<server process-name prefix>*` process runs exits at once. Every server launch call therefore refuses with `launch.server-running`, starting nothing, while such a process runs; that includes a relaunch under worker steps 25-26, which first waits for this run's own server PID to exit.

On that result launch nothing more. Release the claimed lock through
`Invoke-HarnessRelease.ps1` (worker step 39) with the `pid` of each role of this
run still running and, only when one of them is registered,
`-ProcessCheckStatePath`; on the run's first launch that is no
`-ServerPid`/`-ClientPid` and no `-ProcessCheckStatePath`, because no process
started and no role is registered. Return `BLOCKED` with one `Residuals` row
naming each PID and process name the result's message lists as a running server
blocking the harness server, mark every criterion `BLOCKED` citing that row, and
add no `HARNESS-F` finding. Never stop, quit, or signal a listed process; it
belongs to the user.

The native single-instance policy is owned by `Engine/Source/AGENTS.md` `## Crash Reporting`.
Before a runtime session, enumerate sandbox clients across worktrees and build
configurations (`BrokenEngineSandbox.exe`, `BrokenEngineSandbox.Debug.exe`, and
`BrokenEngineSandbox.Profile.exe`) and viewers (`ModelViewer.exe` and
`ModelViewer.Debug.exe`). Wait until none are running and recheck
immediately before the first client launch; never stop a foreign process. Run
one owned client/server pair and require the server to report exactly one
connected client. A foreign join invalidates an isolated comparison.

A criterion requiring simultaneous clients is `BLOCKED` under this policy;
another app-data directory cannot enable it. Verify lock exclusion through
startup-code inspection and required client/server builds. Do not launch a
second client to test refusal: the harness treats a registered process exit
as a failure and has no expected-refusal path.

Next create the directories and fix this run's single process-check state path. The timestamp makes that path new for every run, so no earlier run's registrations are ever inherited:

```powershell
$ROOT = '<absolute adopted worktree>'
$TempDir = Join-Path $ROOT 'Temp'
New-Item -ItemType Directory -Force -Path $TempDir | Out-Null
$AppDataRoot = Join-Path $TempDir 'AppData'
New-Item -ItemType Directory -Force -Path $AppDataRoot | Out-Null
$RunStamp = [datetime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff')
$LaunchPaths = [ordered]@{
	appDataRoot = $AppDataRoot
	processCheckStatePath = Join-Path $TempDir "harness-process-check-$RunStamp.json"
	serverLog = Join-Path $TempDir 'server-agent.log'
	clientLog = Join-Path $TempDir 'client-agent.log'
}
$LaunchPaths | ConvertTo-Json -Compress
```

Retain that record outside shell state. Every process-check action, wait helper, and release call of this run uses that one `processCheckStatePath`, and no later run reuses it; a per-scenario run that uses its own AppData root generates its own state path the same way.

Launch the server, then the client, each with one call that returns once its port answers ping. Each call passes the claim result's `gameDataDirectory` (or the derived copy worker step 5 permits), the recorded `appDataRoot` and `processCheckStatePath`, the role's log path from that record, and the role's crash-report game name:

```powershell
pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-HarnessLaunch.ps1 -RepositoryRoot '<absolute adopted worktree>' -Role 'server' -GameDataDirectory '<normalized Data path>' -AgentHarness '<absolute AgentHarness path>' -Owner '<owner token>' -ProcessCheckStatePath '<absolute process-check state path>' -AppDataDirectory '<absolute app-data root>' -LogFile '<absolute server log path>' -GameName '<server game name>'
```

```powershell
pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-HarnessLaunch.ps1 -RepositoryRoot '<absolute adopted worktree>' -Role 'client' -GameDataDirectory '<normalized Data path>' -AgentHarness '<absolute AgentHarness path>' -Owner '<owner token>' -ProcessCheckStatePath '<absolute process-check state path>' -AppDataDirectory '<absolute app-data root>' -LogFile '<absolute client log path>' -GameName '<client game name>'
```

The skill's launch step owns the launch result's shape and codes.

Both processes share the absolute app-data root recorded as `appDataRoot`, so
saves, settings, caches, and replay artifacts land under this worktree instead
of the per-user AppData folder, each process in its own `game::kGameName` child.
Crash reports are always written under the `--app-data-directory` root when the
complete identity fits the fixed path buffer:
`<app-data-root>\<Game Name>\<Game-Name>-Crash-Report.txt`; otherwise the
existing `%APPDATA%` per-user location remains. A crash before the game
resolves those paths writes `Crash-Report.txt` to its working directory, which
the launch script sets to the app-data root: `<app-data-root>\Crash-Report.txt`.
That file is shared by every role launched on the root rather than tied to one,
so the process check can report it under more than one exited role.
A client Yes at the
save-to-desktop prompt, which appears only in a launch without `--agent-port`,
adds a Desktop copy; the server always adds one without asking, harness launches
included. These are the identities the process check
baselines, which is why its `-GameName` is the role's `<Game Name>`
(`<server game name>` or `<client game name>`) and its
`-ConfiguredAppDataRoot` is the same root this block launches with. A
`<Game-Name>-Crash-Report.txt` the check reports as newly written stays on disk
as evidence until the role's next launch with the same root renames it to
`<Game-Name>-Crash-Report-Pending.txt` beside it, and a reported
`<app-data-root>\Crash-Report.txt` stays there because no launch renames it;
never delete, move, or rewrite one. A fresh root has no autosave and cold
`pipeline.cache`/`BrdfLut.cache`, so the first launch after creating it is
slower; wrapper-worktree setup pre-seeds those two caches into the shared
`Temp\AppData` root while creating the worktree, taking each file independently
from the primary checkout's `Temp\AppData\<client game name>` when present
and otherwise from `%APPDATA%\<client game name>`, skipping a file present in
neither source so that file stays cold, so the cold-cache cost applies mainly to
per-scenario roots. A stopped-server AppData recipe, such as the manifest matrix
in [`replay.md`](replay.md), reads and backs up from
`<absolute app-data root>\<server game name>`, not `%APPDATA%`.

Retain each role's launch result and its `pid` as worker step 21 states. `reset`
is server-scoped and never clears the client's own persisted state, which the
client writes under `<absolute app-data root>\<client game name>` (the project's
own `launch.md` names that state and when it is written). A
shared root can expose run two to the root's most recent orderly-exit snapshot
(or no state for a fresh root), so shared roots cannot prove fresh client state
and invalidate fresh before/after comparisons. A scenario that requires fresh
client state must instead use a per-scenario directory that does not exist yet,
for example `<absolute app-data root>\AppData-CameraComparison`, and record that
the entire per-scenario root was absent before the launch block's `New-Item`
created it. Scenarios that deliberately depend on persisted client or server
state keep the shared `<absolute adopted worktree>\Temp\AppData` root above,
which is also what the cold-cache note and the stopped-server recipe above
assume.

After both generic deadline-limited `Wait-HarnessPing.ps1` readiness checks succeed (server port `27100`, then client port `27101`), restore the minimized agent-mode client and require a successful response confirming `result.minimized:false` before relying on Debug/Profile UI auto-connect:

```powershell
$WindowStateResponse = '{"cmd":"window_state","params":{"minimized":false}}' |
	& '<absolute AgentHarness path>' --owner '<owner token>' --port 27101 -
if ($LASTEXITCODE -ne 0) { throw 'window_state restore failed.' }
$WindowStateResponse = $WindowStateResponse | ConvertFrom-Json
if ($WindowStateResponse.ok -ne $true -or $WindowStateResponse.result.minimized -isnot [bool] -or
	$WindowStateResponse.result.minimized) {
	throw 'window_state did not confirm result.minimized:false.'
}
```

Keep the client visible for the scenario. The Debug and Profile clients
auto-connect: once server discovery finds the local server the client enters
gameplay by itself, so the main menu and its `LOCAL SERVER` button are gone
without any click, and clicking that label there fails with
`no widget matches label` against in-gameplay HUD candidates. Only a Release
client stays on the main menu and needs `click "LOCAL SERVER"`, and only after
discovery replaces the disabled `SCANNING...` button with it. The server loads
its exit autosave, so use `reset` when the scenario needs fresh state.

For an approved island-footprint or island-render criterion only, run the readiness helper after both processes launch as its own independent call. Retain the absolute artifact path outside shell state when checking the result:

```powershell
pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Wait-IslandSceneReady.ps1 -AgentHarness '<absolute AgentHarness path>' -Owner '<owner token>' -ClientPort 27101 -TimeoutSeconds 120 -ArtifactPath '<absolute artifact path>' -ProcessCheckStatePath '<absolute process-check state path>'
```

Require exit `0` from that call; on any other exit the readiness gate failed — inspect `<absolute artifact path>` and report the criterion blocked, and treat artifact `Code:ProcessUnexpectedExit` as a crashed client rather than a slow one. Then require the artifact evidence itself:

```powershell
$IslandReadinessArtifact = '<absolute artifact path>'
$IslandReadiness = Get-Content -Raw -LiteralPath $IslandReadinessArtifact | ConvertFrom-Json -Depth 100
if ($IslandReadiness.SchemaVersion -cne 'broken-engine-island-scene-readiness/v1' -or
	$IslandReadiness.Status -cne 'success' -or $IslandReadiness.Code -cne 'ready' -or
	$IslandReadiness.Ready -isnot [bool] -or -not $IslandReadiness.Ready) {
	throw "Invalid island readiness evidence: '$IslandReadinessArtifact'."
}
```

This is a criterion-specific gate, not general launch readiness. It holds the client visible and proves a ready client tick plus two stable complete footprint samples.

## Normal launch

For a user request to launch the game for their own play or testing. These are
the `## Launch` executables without the harness run: a running-server check,
then a harness lock claim under session `normal-launch` that is released once
both processes have started, but no process check, readiness wait, or
heartbeat, and none of the agent-only arguments (`--agent-port`,
`--app-data-directory`, `--log-file`, `--windowed`, hidden windows). The client
therefore opens a normal window that takes physical input, its fullscreen state
follows the order in
[`Engine/Source/AGENTS.md`](../../../../Engine/Source/AGENTS.md)
`## Startup and Main Loop`, and both processes use the per-user AppData, so the
user's own settings and saves apply. `--loopback-only` stays, as in every
same-machine run.

The server is single-instance per machine (a second one logs an error and exits, with no modal), so first check that no server started without `--agent-port` is running. A server carrying `--agent-port` belongs to another session's harness run, which the claim below waits out; a server whose command line cannot be read counts as started without it:

```powershell
Get-CimInstance Win32_Process -Filter "Name LIKE '<server process-name prefix>%'" |
	Where-Object { [string]$_.CommandLine -notmatch '--agent-port' } |
	Select-Object ProcessId, Name
```

If that lists a process, report it to the user instead of stopping it; take no lock and launch nothing. Otherwise claim the harness lock in the form worker step 5 shows, under that step's run rules (foreground, maximum host timeout, identical re-invocation after a host timeout, never backgrounded), adding `-Configuration <name>` only for a build other than `Debug`:

```powershell
pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1 -RepositoryRoot '<absolute adopted worktree>' -Session 'normal-launch' -Project '<Project>'
```

- Exit `0`: launch below. Keep the result's `owner` and `agentHarness` until the release below, and its `gameDataDirectory`.
- `claim.foreign-owner`: follow worker step 7's `claim.foreign-owner` bullet.
  When step 11 classifies the holder fresh, re-invoke the claim; when stale, run
  steps 11-15 with session label `normal-launch`, using step 1's fixed ports
  (`27100` and `27101`) and the expected executables from the project
  hub step 4 reads. A completed steal continues to the launch below with the
  `claim.foreign-owner` result's `gameDataDirectory`, keeping the steal's new
  owner token and that result's `agentHarness` until the release below; a
  `BLOCKED` takeover is reported to the user with nothing launched.
- `claim.wait-timeout`: re-invoke the claim.
- Any other non-zero exit: report the claim result to the user and launch nothing.

Then launch both. Take the `$Output`, `$ServerExe`, and `$ClientExe` values from the `## Machine-readable launch configuration` of the project hub, `Projects/<Project>/Documents/AgentHarness.md`; its executable names are the Debug ones, so under a Profile or Release claim substitute the `.Profile.exe` or the unsuffixed `.exe` names:

```powershell
$ROOT = '<absolute adopted worktree>'
$GameDataDirectory = '<claim result gameDataDirectory>'
$Output = Join-Path $ROOT '<hub Output path>'
$ServerExe = Join-Path $Output '<hub ServerExe name>'
$ClientExe = Join-Path $Output '<hub ClientExe name>'
$QuotedData = '"' + $GameDataDirectory + '"'
$ServerProcess = Start-Process -FilePath $ServerExe -ArgumentList @('--loopback-only', '--data-directory', $QuotedData) -PassThru -ErrorAction Stop
$ClientProcess = Start-Process -FilePath $ClientExe -ArgumentList @('--loopback-only', '--data-directory', $QuotedData) -PassThru -ErrorAction Stop
[ordered]@{ serverPid = $ServerProcess.Id; clientPid = $ClientProcess.Id } | ConvertTo-Json -Compress
```

Then release the lock with the kept owner token, as its own shell call that runs
even when the launch block threw:

```powershell
& '<absolute AgentHarness path>' lock release --key default --owner '<owner token>'
```

Exit `0` released the lock. Exit `2` means the lock is absent or another owner
took it over; leave it alone. On any other exit, report the release result to
the user. Then discard the owner token. Never release through
`Invoke-HarnessRelease.ps1`, which quits and force-stops processes.

The client connects as `## Launch` describes. If a launch fails, report it to
the user after the release. Leave both processes running for the user, who
closes them; never quit or stop them. After the release this session holds no
owner token, so `/compile` never sends `quit` to these processes. A harness run
started meanwhile claims as worker steps 5 and 7 describe; while the user's
client stays open, it waits as `## Launch` requires.
