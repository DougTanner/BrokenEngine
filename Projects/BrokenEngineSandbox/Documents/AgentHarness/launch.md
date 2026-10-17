# BrokenEngineSandbox Harness Launch

[Back to AgentHarness hub](../AgentHarness.md)

## Launch

Follow the skill's generic launch requirements (`--loopback-only`, log parents under the absolute adopted worktree's `Temp`, `Start-Process -PassThru` PID retention, and the per-role process-check sequence), then launch these executables. Every fence below is its own independent shell call and they run in the order shown; assign each fence's path values locally and substitute values retained from an earlier fence as literals.

Start each launch sequence, after the claim (worker step 5) and before directory setup, by checking that no server is already running. The server is single-instance per machine, so a harness server started while any `BrokenEngineSandboxServer*` process runs exits at once. A relaunch under worker steps 28-29 skips this check, because this run's own server may still be alive; each fresh claim followed by a new launch sequence runs it again:

```powershell
Get-Process -Name 'BrokenEngineSandboxServer*' -ErrorAction SilentlyContinue | Select-Object Id, ProcessName
```

If that lists a process, launch nothing and run no `Baseline`, `Register`, or
`Check`. Release the just-taken lock through `Invoke-HarnessRelease.ps1` (worker
step 44) with no `-ServerPid`/`-ClientPid` and no `-ProcessCheckStatePath`,
because no process started and no role is registered. Return `BLOCKED` with one
`Residuals` row naming each listed PID and process name as a running server
blocking the harness server, mark every criterion `BLOCKED` citing that row, and
add no `HARNESS-F` finding. Never stop, quit, or signal a listed process; it
belongs to the user. If it lists nothing, continue with the sequence below
unchanged.

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

Baseline the server's crash-report candidates before starting it. This launch block passes `--app-data-directory`, so pass the same root as `-ConfiguredAppDataRoot`:

```powershell
pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-AgentHarnessProcessCheck.ps1 -Action Baseline -StatePath '<absolute process-check state path>' -Role 'server' -GameName 'Broken Engine Sandbox Server' -ConfiguredAppDataRoot '<absolute app-data root>'
```

Then launch the server and retain its exact PID and process-start identity. The `$ServerExe` and `$ClientExe` names below are the Debug ones: under a Profile or Release claim substitute the `.Profile.exe` or the unsuffixed `.exe` names in every such assignment in this recipe.

```powershell
$ROOT = '<absolute adopted worktree>'
$GameDataDirectory = '<normalized Data path>'
$Output = Join-Path $ROOT 'Projects\BrokenEngineSandbox\Platforms\VisualStudio2026\Output'
$ServerExe = Join-Path $Output 'BrokenEngineSandboxServer.Debug.exe'
$QuotedData = '"' + $GameDataDirectory + '"'
$QuotedAppData = '"' + '<absolute app-data root>' + '"'
$QuotedServerLog = '"' + '<absolute server log path>' + '"'
$ServerProcess = Start-Process -FilePath $ServerExe -ArgumentList @(
	'--agent-port', '27100', '--loopback-only', '--data-directory', $QuotedData,
	'--app-data-directory', $QuotedAppData,
	'--log-file', $QuotedServerLog) -WindowStyle Hidden -PassThru -ErrorAction Stop
$LifecycleRecord = [ordered]@{
	serverPid = $ServerProcess.Id
	serverStartTimeUtc = $ServerProcess.StartTime.ToUniversalTime().ToString('o')
	clientPid = $null
	clientStartTimeUtc = $null
	appDataRoot = '<absolute app-data root>'
	processCheckStatePath = '<absolute process-check state path>'
}
$LifecycleRecord | ConvertTo-Json -Compress
```

Register that exact identity immediately, substituting the snapshot's `serverPid` and `serverStartTimeUtc`:

```powershell
pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-AgentHarnessProcessCheck.ps1 -Action Register -StatePath '<absolute process-check state path>' -Role 'server' -ProcessId <server PID> -StartTimeUtc '<server start time UTC>'
```

Then check immediately, before any readiness wait, so a server that already died during startup is caught against its true pre-launch baseline:

```powershell
pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-AgentHarnessProcessCheck.ps1 -Action Check -StatePath '<absolute process-check state path>'
```

Run the same four-call sequence for the client. Its baseline uses the client's own game name:

```powershell
pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-AgentHarnessProcessCheck.ps1 -Action Baseline -StatePath '<absolute process-check state path>' -Role 'client' -GameName 'Broken Engine Sandbox' -ConfiguredAppDataRoot '<absolute app-data root>'
```

```powershell
$ROOT = '<absolute adopted worktree>'
$GameDataDirectory = '<normalized Data path>'
$Output = Join-Path $ROOT 'Projects\BrokenEngineSandbox\Platforms\VisualStudio2026\Output'
$ClientExe = Join-Path $Output 'BrokenEngineSandbox.Debug.exe'
$QuotedData = '"' + $GameDataDirectory + '"'
$QuotedAppData = '"' + '<absolute app-data root>' + '"'
$QuotedClientLog = '"' + '<absolute client log path>' + '"'
$ClientProcess = Start-Process -FilePath $ClientExe -ArgumentList @(
	'--agent-port', '27101', '--loopback-only', '--data-directory', $QuotedData,
	'--app-data-directory', $QuotedAppData,
	'--windowed', '1600x900', '--log-file', $QuotedClientLog) -WindowStyle Hidden -PassThru -ErrorAction Stop
$LifecycleRecord = [ordered]@{
	serverPid = <server PID>
	serverStartTimeUtc = '<server start time UTC>'
	clientPid = $ClientProcess.Id
	clientStartTimeUtc = $ClientProcess.StartTime.ToUniversalTime().ToString('o')
	appDataRoot = '<absolute app-data root>'
	processCheckStatePath = '<absolute process-check state path>'
}
$LifecycleRecord | ConvertTo-Json -Compress
```

Then `Register` the client with `-Role 'client'` and the snapshot's `clientPid`/`clientStartTimeUtc`, and run the same immediate `Check`. The skill owns the process-check result shape and exit codes: exit `0` healthy, exit `2` an unexpected exit whose findings name the role, report path, exception headline, and retained evidence location, exit `1` a setup or state failure.

Both processes share the absolute app-data root recorded as `appDataRoot`, so
saves, settings, caches, and replay artifacts land under this worktree instead
of the per-user AppData folder, each process in its own `game::kGameName` child.
That name separates only the client and server children, so an additional
simultaneous client gets its own root instead (see
[Additional simultaneous client](#additional-simultaneous-client)). Crash
reports are always written under the `--app-data-directory` root when the
complete identity fits the fixed path buffer:
`<app-data-root>\<Game Name>\<Game-Name>-Crash-Report.txt`; otherwise the
existing `%APPDATA%` per-user location remains. A client Yes at the
save-to-desktop prompt, which appears only in a launch without `--agent-port`,
adds a Desktop copy; the server always adds one without asking, harness launches
included. These are the identities the process check
baselines, which is why its `-GameName` is the role's `<Game Name>`
(`Broken Engine Sandbox Server` or `Broken Engine Sandbox`) and its
`-ConfiguredAppDataRoot` is the same root this block launches with. A report the
check reports as newly written stays on disk as evidence; never delete, move, or
rewrite one. A fresh root has no autosave and cold
`pipeline.cache`/`BrdfLut.cache`, so the first launch after creating it is
slower; wrapper-worktree setup pre-seeds those two caches into the shared
`Temp\AppData` root while creating the worktree, taking each file independently
from the primary checkout's `Temp\AppData\Broken Engine Sandbox` when present
and otherwise from `%APPDATA%\Broken Engine Sandbox`, skipping a file present in
neither source so that file stays cold, so the cold-cache cost applies mainly to
per-scenario roots. The stopped-server AppData recipe below reads and backs up
from `<absolute app-data root>\Broken Engine Sandbox Server`, not `%APPDATA%`.

The latest compact JSON snapshot is authoritative. Retain each snapshot outside
shell state; its `serverStartTimeUtc`/`clientStartTimeUtc` values are only for
the matching `Register` call, and its `processCheckStatePath` is the value every
later process check, wait helper, and release call uses. If the client launch
fails, use the earlier snapshot and release only its non-null `serverPid`; do
not invent a client PID. If the server launch fails, no PID argument exists.
After both launches, release with both exact non-null numeric PIDs. `reset` is
server-scoped and never clears the client's own persisted state: the client
writes `ClientState.bin` once during normal orderly shutdown under
`<absolute app-data root>\Broken Engine Sandbox`; focus/zoom changes made during
play remain in memory and may be lost on exception, crash, or hard kill. A
shared root can expose run two to the root's most recent orderly-exit snapshot
(or no state for a fresh root), so shared roots cannot prove fresh client state
and invalidate fresh before/after comparisons. A scenario that requires fresh
client state must instead use a per-scenario directory that does not exist yet,
for example `<absolute app-data root>\AppData-CameraComparison`, and record that
the entire per-scenario root was absent before the launch block's `New-Item`
created it. Scenarios that deliberately depend on persisted client or server
state keep the shared `<absolute adopted worktree>\Temp\AppData` root above,
which is also what the cold-cache note and the stopped-server recipe below
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

### Additional simultaneous client

Launch this only for a criterion that needs two clients connected at once; the
skill owns the convention, this block owns the concrete values. Run it after the
first client is ready. Create its own app-data root first — a directory holding
no state from any earlier run, so the two clients never share the
`Broken Engine Sandbox` per-client directory and a later run never inherits the
previous run's client settings. The fixed path below is reused across runs, so
remove a leftover root before creating it:

```powershell
$ROOT = '<absolute adopted worktree>'
$SecondAppDataRoot = Join-Path (Join-Path $ROOT 'Temp') 'AppData-ClientB'
if (Test-Path -LiteralPath $SecondAppDataRoot) { Remove-Item -Recurse -Force -LiteralPath $SecondAppDataRoot }
New-Item -ItemType Directory -Path $SecondAppDataRoot | Out-Null
[ordered]@{
	secondAppDataRoot = $SecondAppDataRoot
	secondClientLog = Join-Path (Join-Path $ROOT 'Temp') 'client-b-agent.log'
} | ConvertTo-Json -Compress
```

Baseline its own role against its own root, using this run's single process-check state path:

```powershell
pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-AgentHarnessProcessCheck.ps1 -Action Baseline -StatePath '<absolute process-check state path>' -Role 'client-b' -GameName 'Broken Engine Sandbox' -ConfiguredAppDataRoot '<absolute second app-data root>'
```

Then launch the same client executable with the same `--data-directory`, changing only the agent port, the app-data root, and the log file:

```powershell
$ROOT = '<absolute adopted worktree>'
$GameDataDirectory = '<normalized Data path>'
$Output = Join-Path $ROOT 'Projects\BrokenEngineSandbox\Platforms\VisualStudio2026\Output'
$ClientExe = Join-Path $Output 'BrokenEngineSandbox.Debug.exe'
$QuotedData = '"' + $GameDataDirectory + '"'
$QuotedAppData = '"' + '<absolute second app-data root>' + '"'
$QuotedClientLog = '"' + '<absolute second client log path>' + '"'
$SecondClientProcess = Start-Process -FilePath $ClientExe -ArgumentList @(
	'--agent-port', '27102', '--loopback-only', '--data-directory', $QuotedData,
	'--app-data-directory', $QuotedAppData,
	'--windowed', '1600x900', '--log-file', $QuotedClientLog) -WindowStyle Hidden -PassThru -ErrorAction Stop
[ordered]@{
	secondClientPid = $SecondClientProcess.Id
	secondClientStartTimeUtc = $SecondClientProcess.StartTime.ToUniversalTime().ToString('o')
} | ConvertTo-Json -Compress
```

`Register` that exact identity with `-Role 'client-b'`, run the same immediate `Check`, then wait for its port after the `27100` and `27101` waits:

```powershell
pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Wait-HarnessPing.ps1 -AgentHarness '<absolute AgentHarness path>' -Owner '<owner token>' -Port 27102 -TimeoutSeconds 120 -ProcessCheckStatePath '<absolute process-check state path>'
```

Address it like the first client, on port `27102`. It auto-connects the same way, and `status.clientCount` counts both.

Shut it down before the release call, because release covers only ports `27100`/`27101`:

```powershell
'{"cmd":"quit"}' | & '<absolute AgentHarness path>' --owner '<owner token>' --port 27102 --timeout-ms 3000 -
Wait-Process -Id <second client PID> -Timeout 60
if (Get-Process -Id <second client PID> -ErrorAction SilentlyContinue) { throw 'Second client did not exit.' }
```

`Wait-Process` only reports a non-terminating error when its timeout expires, so the `Get-Process` check above is what proves the exit. Do not run the fresh `Baseline` or release while that PID still exists — treat it as a failed run and stop or investigate that exact process first, otherwise releasing the lock orphans a live client.

Once the exit is confirmed, run one fresh `Baseline` for `-Role 'client-b'` with the same game name and root, clearing its registration so the intended quit is not reported as an unexpected exit, and release with the server and first-client PIDs as usual.

For an approved island-footprint or island-render criterion only, run the readiness helper after both processes launch as its own independent call. Retain the latest lifecycle snapshot and absolute artifact path outside shell state when checking the result:

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
then a harness lock claim under session `normal-launch` that is never released,
but no process check, readiness wait, heartbeat, or release, and none of the
agent-only arguments (`--agent-port`, `--app-data-directory`, `--log-file`,
`--windowed`, hidden windows). The client therefore opens a normal window that
takes physical input, its fullscreen state follows the order in
[`Engine/Source/AGENTS.md`](../../../../Engine/Source/AGENTS.md)
`## Startup and Main Loop`, and both processes use the per-user AppData, so the
user's own settings and saves apply. `--loopback-only` stays, as in every
same-machine run.

The server is single-instance per machine (a second one logs an error and exits, with no modal), so first check that no server started without `--agent-port` is running. A server carrying `--agent-port` belongs to another session's harness run, which the claim below waits out; a server whose command line cannot be read counts as started without it:

```powershell
Get-CimInstance Win32_Process -Filter "Name LIKE 'BrokenEngineSandboxServer%'" |
	Where-Object { [string]$_.CommandLine -notmatch '--agent-port' } |
	Select-Object ProcessId, Name
```

If that lists a process, report it to the user instead of stopping it; take no lock and launch nothing. Otherwise claim the harness lock in the form worker step 5 shows, under that step's run rules (foreground, maximum host timeout, identical re-invocation after a host timeout, never backgrounded), passing the same normalized Data path the launch passes to `--data-directory`, and `-Configuration <name>` only for a build other than `Debug`:

```powershell
pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1 -RepositoryRoot '<absolute adopted worktree>' -Session 'normal-launch' -GameDataDirectory '<normalized Data path>'
```

- Exit `0`: launch below. Do not keep the claim's owner token.
- `claim.foreign-owner`: follow worker step 7's `claim.foreign-owner` bullet. When step 11 classifies the holder fresh, re-invoke the claim; when stale, run steps 11-15 with session label `normal-launch`, using step 1's fixed ports (`27100`, `27101`, and `27102`) and the expected executables from the project hub step 4 reads. A completed steal continues to the launch below, and its new owner token is not kept either; a `BLOCKED` takeover is reported to the user with nothing launched.
- Any other non-zero exit: report the claim result to the user and launch nothing.

Then launch both, substituting the executable names for the compiled configuration as `## Launch` states:

```powershell
$ROOT = '<absolute adopted worktree>'
$GameDataDirectory = '<normalized Data path>'
$Output = Join-Path $ROOT 'Projects\BrokenEngineSandbox\Platforms\VisualStudio2026\Output'
$ServerExe = Join-Path $Output 'BrokenEngineSandboxServer.Debug.exe'
$ClientExe = Join-Path $Output 'BrokenEngineSandbox.Debug.exe'
$QuotedData = '"' + $GameDataDirectory + '"'
$ServerProcess = Start-Process -FilePath $ServerExe -ArgumentList @('--loopback-only', '--data-directory', $QuotedData) -PassThru -ErrorAction Stop
$ClientProcess = Start-Process -FilePath $ClientExe -ArgumentList @('--loopback-only', '--data-directory', $QuotedData) -PassThru -ErrorAction Stop
[ordered]@{ serverPid = $ServerProcess.Id; clientPid = $ClientProcess.Id } | ConvertTo-Json -Compress
```

The client connects as `## Launch` describes. If a launch fails, only report it
to the user; the claimed lock goes stale five minutes after the claim and the
takeover steps take it over. Leave both processes running for the user, who
closes them; never quit, stop, or release them, and never heartbeat or release
the lock. The claim's owner token is not kept, so after the launch this session
does not hold the harness lock and `/compile` never sends `quit` to these
processes. A harness run started meanwhile waits in its claim until the user
closes the server, then takes the stale lock over through worker steps 11-15;
when the server closed within five minutes of the claim, that run waits out the
rest of those five minutes.

## Durable caveats

- Client weapon-mode requests received during paused or other zero-tick updates and internally queued flagship navigation updates persist until the first advancing update. Client `kClientFleetNavigationDelay` requests apply immediately. To verify load-requeued flagship updates, use `load {"pauseAfterLoad":true}` and inspect `pendingFlagshipUpdateCount` before unpausing.
