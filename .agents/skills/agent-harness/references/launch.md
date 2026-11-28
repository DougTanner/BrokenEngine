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

Run `Invoke-HarnessLaunch.ps1` directly on a working host, or via granted
per-command approval. Before launch, it calls `Test-AgentKillOnCloseJob` and
exits `2` (`blocked`, `launch.kill-on-close-job`) without starting or
registering on an immediate kill-on-close Job Object without silent breakaway.
Proceeding launches prove survival with a post-exit owned ping and identity
check. On refusal or an unsupported host, give the user the desktop block below
with actual values. Run from the adopted worktree root and double apostrophes
in PowerShell single-quoted literals.

The server is single-instance per machine, so a harness server started while any
`<server process-name prefix>*` process runs exits at once. Every server launch
call therefore refuses with `launch.server-running`, starting nothing, while
such a process runs; that includes a relaunch under worker steps 25-26, which
first waits for this run's own server PID to exit.

On that result launch nothing more. Follow the release procedure below for
this run's roles and claim. Return `BLOCKED` with one `Residuals` row
naming each PID and process name the result's message lists as a running server
blocking the harness server, mark every criterion `BLOCKED` citing that row, and
add no `HARNESS-F` finding. Never stop, quit, or signal a listed process; it
belongs to the user.

The native single-instance policy is owned by `Engine/Source/AGENTS.md`
`## Crash Reporting`. Before a runtime session, enumerate sandbox clients across
worktrees and build configurations (`BrokenEngineSandbox.exe`,
`BrokenEngineSandbox.Debug.exe`, and `BrokenEngineSandbox.Profile.exe`) and
viewers (`ModelViewer.exe` and `ModelViewer.Debug.exe`). Wait until none are
running and recheck immediately before the first client launch; never stop a
foreign process. Run one owned client/server pair and require the server to
report exactly one connected client. A foreign join invalidates an isolated
comparison.

A criterion requiring simultaneous clients is `BLOCKED` under this policy;
another app-data directory cannot enable it. Verify lock exclusion through
startup-code inspection and required client/server builds. Do not launch a
second client to test refusal: the harness treats a registered process exit
as a failure and has no expected-refusal path.

Next create the directories and fix this run's single process-check state path.
The timestamp makes that path new for every run, so no earlier run's
registrations are ever inherited:

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

Retain that record outside shell state. Use its `processCheckStatePath` for
every process-check action and wait helper, and follow the release block below.
Generate a fresh path the same way for every run, including a per-scenario run
with its own AppData root.

Run this call for `server`, then after its ping succeeds for `client`. Use
the claim's `gameDataDirectory` or the copy worker step 5 permits, the recorded
paths, the role's log and game name, and the applicable configuration and
switches from worker step 21:

```powershell
pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-HarnessLaunch.ps1 -RepositoryRoot '<absolute adopted worktree>' -Role '<role>' -GameDataDirectory '<normalized Data path>' -AgentHarness '<absolute AgentHarness path>' -Owner '<owner token>' -ProcessCheckStatePath '<absolute process-check state path>' -AppDataDirectory '<absolute app-data root>' -LogFile '<absolute role log path>' -GameName '<role game name>'
```

For desktop launch (including after `launch.kill-on-close-job`), keep the live
claim and heartbeat, put that launch line after the guard below in one
`& { ... }` block with the live token and run paths, redirect stdout to a
fresh `Temp` receipt, and print `$LASTEXITCODE`. On guard failure, keep its
output and follow ownership rules before any next command:

```powershell
$PSNativeCommandUseErrorActionPreference = $false
& '<absolute AgentHarness path>' lock heartbeat --key default --owner '<owner token>'
if ($LASTEXITCODE -ne 0) { throw "Harness ownership check failed (exit $LASTEXITCODE)." }
```

Any separate Register call uses the same durable context, original role and
state, PID, and start time.

Keep every launch result, including a failed Register or readiness wait. Use
the following release block for cleanup across calls under worker steps 39–42,
including desktop-launch abandonment or completion. Use both latest receipts,
including an earlier successful role, and the project hub's executable names
for their launch configurations without `.exe`. Use `0` and an empty timestamp
where a role never started. For `<absolute process-check state path or empty>`,
supply the original run path when the launch, Register, or Check receipts show
any role registered or leave registration uncertain, as with a failed Register
or unreadable state. Use an empty string only when the receipts establish that
no role registered, such as when no role started or only Baseline ran.

```powershell
& {
	$PSNativeCommandUseErrorActionPreference = $false
	$harness = '<absolute AgentHarness path>'
	$owner = '<owner token>'
	$state = '<absolute process-check state path or empty>'
	$roles = @(
		@{ argument = '-ServerPid'; name = '<server process name>'
			pid = <retained server PID or 0>; started = '<server startTimeUtc or empty>' }
		@{ argument = '-ClientPid'; name = '<client process name>'
			pid = <retained client PID or 0>; started = '<client startTimeUtc or empty>' }
	)
	$release = @('-AgentHarness', $harness, '-Owner', $owner)
	$held = @()
	try {
		foreach ($role in $roles) {
			if ($role.pid -eq 0) { continue }
			$process = Get-Process -Id $role.pid -ErrorAction SilentlyContinue
			if ($null -eq $process) { continue }
			$held += $process
			$name = $process.ProcessName
			if ($name -and $name -ne $role.name) { continue }
			try { $handle = $process.Handle; $started = $process.StartTime }
			catch { if ($process.HasExited) { continue }; throw }
			if ($process.HasExited) { continue }
			if ($null -ne $started -and $started.ToUniversalTime().ToString('o') -cne $role.started) { continue }
			if ($null -eq $handle -or $null -eq $started) { throw "Could not verify $($role.name) PID $($role.pid)." }
			$release += @($role.argument, $role.pid)
		}
		if ($state) { $release += @('-ProcessCheckStatePath', $state) }
		& $harness lock heartbeat --key default --owner $owner
		if ($LASTEXITCODE -ne 0) { throw "Harness ownership check failed (exit $LASTEXITCODE)." }
		pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-HarnessRelease.ps1 @release
		"Release exit: $LASTEXITCODE"
	} finally { $held | ForEach-Object { $_.Dispose() } }
}
```

For a desktop release redirect the script's stdout to its own fresh receipt.
Keep the release call even when both role arguments are omitted. If an
operation's result is missing or ambiguous, reconcile its receipt, ownership
and role identities before retrying.

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

After both generic deadline-limited `Wait-HarnessPing.ps1` readiness checks
succeed (server port `27100`, then client port `27101`), restore the minimized
agent-mode client and require a successful response confirming
`result.minimized:false` before relying on Debug/Profile UI auto-connect:

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

For an approved island-footprint or island-render criterion only, run the
readiness helper after both processes launch as its own independent call. Retain
the absolute artifact path outside shell state when checking the result:

```powershell
pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Wait-IslandSceneReady.ps1 -AgentHarness '<absolute AgentHarness path>' -Owner '<owner token>' -ClientPort 27101 -TimeoutSeconds 120 -ArtifactPath '<absolute artifact path>' -ProcessCheckStatePath '<absolute process-check state path>'
```

Require exit `0` from that call; on any other exit the readiness gate failed —
inspect `<absolute artifact path>` and report the criterion blocked, and treat
artifact `Code:ProcessUnexpectedExit` as a crashed client rather than a slow
one. Then require the artifact evidence itself:

```powershell
$IslandReadinessArtifact = '<absolute artifact path>'
$IslandReadiness = Get-Content -Raw -LiteralPath $IslandReadinessArtifact | ConvertFrom-Json -Depth 100
if ($IslandReadiness.SchemaVersion -cne 'broken-engine-island-scene-readiness/v1' -or
	$IslandReadiness.Status -cne 'success' -or $IslandReadiness.Code -cne 'ready' -or
	$IslandReadiness.Ready -isnot [bool] -or -not $IslandReadiness.Ready) {
	throw "Invalid island readiness evidence: '$IslandReadinessArtifact'."
}
```

This is a criterion-specific gate, not general launch readiness. It holds the
client visible and proves a ready client tick plus two stable complete footprint
samples.

## Normal launch

For a user request to launch the game for their own play or testing, check
for a running Normal server, claim under session `normal-launch`, then start
the server and client with ordinary visible windows, loopback and per-user
settings. The client's fullscreen state follows
[`Engine/Source/AGENTS.md`](../../../../Engine/Source/AGENTS.md)
`## Startup and Main Loop`. Run directly on a working host, or via granted
desktop execution if needed for window lifetime. When neither is available or
after kill-on-close refusal, give the user the blocks below from the adopted
worktree and collect fresh `Temp` receipts and exit codes before any next
command.

The claim waits out harness servers. Check first for a Normal server, including
one whose command line is unreadable. Report any listed processes and end the
attempt. Otherwise use worker step 5's foreground wait and claim cap.
Debug is the default. For `/analyze-gpu-stall`, add `-Configuration Profile`
for the slow-operation warnings (`kbProfilingFrameSpike`) and use the Profile
executables and absolute client log in the pair start below:

```powershell
& {
	$conflicts = @(Get-CimInstance Win32_Process -Filter "Name LIKE '<server process-name prefix>%'" -ErrorAction Stop |
		Where-Object { [string]$_.CommandLine -notmatch '--agent-port' })
	if ($conflicts.Count) { $conflicts | Select-Object ProcessId, Name; return }
	pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1 -RepositoryRoot '<absolute adopted worktree>' -Session 'normal-launch' -Project '<Project>'
}
```

- Host timeout or missing claim receipt: follow worker step 5's wait rule for a
  running call and use its exit code and receipt if recovered. Without a
  receipt, confirm that the original invocation and any in-flight `lock claim`
  have exited, then run worker step 44's `lock status` command. An absent lock
  or confirmed foreign holder permits another claim within the cap, with its
  result routed below. A different session or canonical worktree, or a
  previously confirmed foreign token, identifies a foreign holder. If the
  original call's exit, lock status, or holder's identity remains uncertain,
  report the timeout or missing receipt and the available lock evidence, then
  end the attempt. Obtain launch ownership through the claim result or
  conditional takeover below.
- Exit `0`: keep the result's `owner`, `agentHarness` and `gameDataDirectory`
  for the pair start below.
- `claim.foreign-owner`: follow worker step 7's `claim.foreign-owner` bullet.
  When step 11 classifies the holder fresh, re-invoke the claim within worker
  step 5's claim cap; when stale, run
  steps 11-15 with session label `normal-launch`, using step 1's fixed ports
  (`27100` and `27101`) and the expected executables from the project
  hub step 4 reads. A completed steal continues to the launch below with the
  `claim.foreign-owner` result's `gameDataDirectory`, keeping the steal's new
  owner token and that result's `agentHarness` until the release below; a
  `BLOCKED` takeover is reported to the user with nothing launched.
- `claim.wait-timeout`: re-invoke the claim within worker step 5's claim cap.
  When the cap prevents a retry on any route, report the latest lock status or
  claim result, including `currentOwner` or the server PIDs when present, and
  end the attempt.
- Any other non-zero exit: report the claim result to the user and launch
  nothing.

On proven acquisition, run the same pair start and lock-only release together
on both paths. Use the project hub's mapping to resolve the two absolute
executables for the claimed configuration. Set `$clientLog` to the exact
absolute log path for Profile and to `''` for Debug:

```powershell
& {
	$PSNativeCommandUseErrorActionPreference = $false
	$harness = '<absolute AgentHarness path>'
	$owner = '<current owner token>'
	$clientLog = '<absolute Profile client log path, or empty for Debug>'
	$gameArguments = @('--loopback-only', '--data-directory', ('"' + '<claim result gameDataDirectory>' + '"'))
	$receipt = [ordered]@{
		ownershipExitCode = $null; ownershipOutput = $null
		serverPid = $null; serverStartTimeUtc = $null; clientPid = $null; clientStartTimeUtc = $null
		launchError = $null; releaseExitCode = $null; releaseOutput = $null; releaseError = $null
	}
	try {
		$receipt.ownershipOutput = @(& $harness lock heartbeat --key default --owner $owner)
		$receipt.ownershipExitCode = $LASTEXITCODE
		if ($receipt.ownershipExitCode -ne 0) { throw 'Harness ownership check failed.' }
		$conflicts = @(Get-CimInstance Win32_Process -Filter "Name LIKE '<server process-name prefix>%'" -ErrorAction Stop |
			Where-Object { [string]$_.CommandLine -notmatch '--agent-port' })
		if ($conflicts.Count -gt 0) { throw "A Normal server is running (PID $($conflicts.ProcessId -join ', '))." }
		Import-Module './.agents/scripts/AgentScriptCommon.psm1'
		if (Test-AgentKillOnCloseJob) { throw 'Immediate kill-on-close Job Object; re-claim and use desktop launch.' }
		$server = Start-Process -FilePath '<absolute mapped server executable>' -ArgumentList $gameArguments -PassThru -ErrorAction Stop
		$receipt.serverPid = $server.Id
		$receipt.serverStartTimeUtc = $server.StartTime.ToUniversalTime().ToString('o')
		if ($clientLog) { $gameArguments += @('--log-file', ('"' + $clientLog + '"')) }
		$client = Start-Process -FilePath '<absolute mapped client executable>' -ArgumentList $gameArguments -PassThru -ErrorAction Stop
		$receipt.clientPid = $client.Id
		$receipt.clientStartTimeUtc = $client.StartTime.ToUniversalTime().ToString('o')
	} catch { $receipt.launchError = $_.Exception.Message }
	finally {
		if ($receipt.ownershipExitCode -eq 0) {
			try {
				$receipt.releaseOutput = @(& $harness lock release --key default --owner $owner)
				$receipt.releaseExitCode = $LASTEXITCODE
			} catch { $receipt.releaseError = $_.Exception.Message }
		}
	}
	$receipt | ConvertTo-Json -Compress
}
```

Read `releaseExitCode` (`ToolCliCommon.h`: `kiExitOk` released;
`kiExitStateConflict` absent or foreign). Report other exits or exceptions with
release output; discard the token once settled. On kill-on-close refusal,
reconcile ambiguous release, re-run the conflict check and claim for a fresh
token, and give the user the desktop pair-start block with it. Report other
launch failures and started processes, reconciling PIDs, start times, claim,
and release first. Leave Normal windows for the user to close; the client
connects as `## Launch` describes.
