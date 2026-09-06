# Repairs a session branch in place after the primary branch was rewritten under it: replays the
# session's own commits onto the new primary tip and rewrites the session baseline file. Run it from
# the session worktree root. `/finalize-changes` and the diverged-session block message of
# `/next-plan`'s claim script are its documented callers.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Complete-ForkPointRepair([int] $ExitCode, [string] $Status, [string] $Code, [string] $Message, $Context = $null, $ForkPoint = $null, $Baseline = $null, [bool] $Rebased = $false) {
	[ordered]@{
		schemaVersion = 'broken-engine-fork-point-repair/v1'
		status = $Status
		code = $Code
		message = $Message
		exitCode = $ExitCode
		forkPoint = $ForkPoint
		primaryTip = if ($null -ne $Context) { $Context.PrimaryTip } else { $null }
		branch = if ($null -ne $Context) { $Context.Branch } else { $null }
		baseline = $Baseline
		rebased = $Rebased
	} | ConvertTo-Json -Compress -Depth 8
	exit $ExitCode
}

Import-Module (Join-Path $PSScriptRoot 'AgentWorktreeSession.psm1') -Force -DisableNameChecking

$context = $null
try { $context = Get-AgentWorktreeSessionContext }
catch { Complete-ForkPointRepair 1 'error' 'session.unresolved' "The session worktree could not be resolved: $($_.Exception.Message)" }

$repair = $null
try { $repair = Repair-AgentWorktreeForkPoint $context.Worktree $context.PrimaryBranch $context.PrimaryTip $context.Branch }
catch { Complete-ForkPointRepair 2 'blocked' 'fork-point.repair-refused' $_.Exception.Message $context }

# Unlike the wrapper, this caller runs only because the primary branch was rewritten. An ancestor fork
# point therefore means the pre-rewrite tip has expired from the reflog and the true fork point is
# unrecoverable, which is a blocker to report rather than a baseline to guess from.
if ($repair.Outcome -cne 'rebased') {
	Complete-ForkPointRepair 2 'blocked' 'fork-point.not-recoverable' "Fork point $($repair.ForkPoint) is still an ancestor of primary tip $($context.PrimaryTip), so the pre-rewrite fork point is unrecoverable and nothing was changed." $context $repair.ForkPoint $repair.Baseline $false
}
[IO.File]::WriteAllText((Join-Path $context.Worktree 'Temp\session-baseline'), $repair.Baseline + "`n", [Text.UTF8Encoding]::new($false))
Complete-ForkPointRepair 0 'pass' 'ok' "Replayed branch '$($context.Branch)' from fork point $($repair.ForkPoint) onto primary tip $($context.PrimaryTip), and recorded that tip as the new baseline." $context $repair.ForkPoint $repair.Baseline $true
