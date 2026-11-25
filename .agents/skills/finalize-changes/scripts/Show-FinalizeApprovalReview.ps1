# Sole SmartGit review-tool boundary for a session landing.
#
# Main runs this script from the finalizer's handoff, after the finalizer's
# workflow step 6 filled its command line in — the earlier approval-preparation
# step having returned the approved tip, the acceptance table, and the
# primary-movement check — with the documented redirect. Main opens the window
# and asks the landing confirmation in the same exchange, so the user can
# confirm as soon as their review is done. Callers never reconstruct its SmartGit command inline.
#
# The script attempts the launch whenever SmartGit is available. Handoff-only
# checks pair a normal launcher exit with a surviving pre-existing instance.
# It runs Git only to resolve the supplied tip to its full commit ID and compute its
# reviewed diff identity, and never mutates a ref or claims a lock.
#
# Contract: schema broken-engine-finalize-approval-review/v2. Unlike the mutating
# scripts, every launch outcome exits 0 and none report status pass — the
# review outcome is non-blocking, but the attempt is not: main redirects this single-line
# stdout to Temp/finalize-approval-review-result.json, and the landing finalizer passes
# that receipt path to the landing scripts, which verify reviewedRepository, approvedTip
# (or rebase-equivalent reviewedPatchId/reviewedChanges), and status from it and refuse
# to change primary when it does not record an attempted launch for the reviewed diff.
# opened is the launch success path; unavailable/failed are non-blocking,
# and the caller copies message and the exact manualCommand into the approval response while keeping the landing gate in
# force. Only invalid input exits 1, with status error and a code naming the cause
# (input.invalid for a malformed tip or a tip that resolves to no commit;
# internal.error for a primary worktree that will not resolve).
# A clean identical post-confirmation rebase that
# preserves the existing confirmation must not reopen the review tool the user
# already saw: that path gets no fresh review run. A material change
# requiring a refreshed confirmation gets one, against the newly
# reviewed candidate, from a refreshed launch line main runs before that refreshed confirmation.
[CmdletBinding()]
param(
	[Parameter(Mandatory)][string] $PrimaryWorktree,
	[Parameter(Mandatory)][string] $ApprovedTip,
	[switch] $HandoffOnly
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$workflowModule = Join-Path $PSScriptRoot '..\..\..\scripts\FinalizeWorkflowCommon.psm1'
if (-not (Test-Path -LiteralPath $workflowModule)) {
	$workflowModule = Join-Path $PSScriptRoot '..\..\..\..\.agents\scripts\FinalizeWorkflowCommon.psm1'
}
Import-Module $workflowModule -Force

$result = [ordered]@{
	schemaVersion = 'broken-engine-finalize-approval-review/v2'
	status = 'error'
	code = 'internal.error'
	message = 'Review-tool launch did not complete.'
	approvedTip = $null
	reviewedRepository = $null
	reviewedPatchId = $null
	reviewedChanges = $null
	executable = $null
	arguments = @()
	manualCommand = $null
	processId = $null
}

$script:PrimaryIdentity = $null

function Complete-Review([int] $ExitCode, [string] $Status, [string] $Code, [string] $Message)
{
	$result.status = $Status
	$result.code = $Code
	$result.message = $Message
	Write-Output ($result | ConvertTo-Json -Depth 12 -Compress)
	exit $ExitCode
}

function Throw-Review([int] $ExitCode, [string] $Code, [string] $Message)
{
	$exception = [InvalidOperationException]::new($Message)
	$exception.Data['ExitCode'] = $ExitCode
	$exception.Data['Code'] = $Code
	throw $exception
}

function Assert-Input([bool] $Condition, [string] $Message)
{
	if (-not $Condition)
	{
		Throw-Review 1 'input.invalid' $Message
	}
}

function ConvertTo-PowerShellLiteral([string] $Value)
{
	return "'" + ($Value -replace "['\u2018-\u201B]", '$0$0') + "'"
}

function Get-SmartGitReceivers([string] $Executable)
{
	try
	{
		$sessionId = (Get-Process -Id $PID).SessionId
		$candidates = [Diagnostics.Process]::GetProcessesByName([IO.Path]::GetFileNameWithoutExtension($Executable))
	}
	catch
	{
		return
	}
	foreach ($candidate in $candidates)
	{
		try
		{
			if ($candidate.SessionId -eq $sessionId)
			{
				# Hold the receiver's identity through the launcher wait.
				[void] $candidate.SafeHandle
				if (-not $candidate.HasExited -and [string]::Equals($candidate.MainModule.FileName, $Executable, [StringComparison]::OrdinalIgnoreCase))
				{
					$candidate
					continue
				}
			}
		}
		catch
		{
			# A process can exit or deny inspection before its image is verified.
		}
		$candidate.Dispose()
	}
}

function Invoke-SmartGit
{
	$standardExecutable = 'C:\Program Files\SmartGit\bin\smartgit.exe'
	$arguments = @('--log', $script:PrimaryIdentity, "--anchor-commit=$ApprovedTip")
	$result.arguments = $arguments
	if (Test-Path -LiteralPath $standardExecutable -PathType Leaf)
	{
		$result.executable = $standardExecutable
	}
	else
	{
		$command = Get-Command smartgit.exe -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
		if ($null -ne $command -and (Test-Path -LiteralPath $command.Source -PathType Leaf))
		{
			$result.executable = [IO.Path]::GetFullPath($command.Source)
		}
	}
	$manualExecutable = $result.executable ?? $standardExecutable
	$manualArguments = @($manualExecutable) + $arguments
	$result.manualCommand = '& ' + (($manualArguments | ForEach-Object { ConvertTo-PowerShellLiteral $_ }) -join ' ')
	if ($null -eq $result.executable)
	{
		Complete-Review 0 'unavailable' 'review.unavailable' 'SmartGit executable was not found.'
	}

	$receivers = @()
	$launcher = $null
	$message = 'SmartGit was opened for the approval candidate.'
	try
	{
		if ($HandoffOnly)
		{
			$receivers = @(Get-SmartGitReceivers $result.executable)
		}
		$start = [Diagnostics.ProcessStartInfo]::new($result.executable)
		$start.UseShellExecute = $true
		$start.WorkingDirectory = $PWD.ProviderPath
		foreach ($argument in $arguments)
		{
			$start.ArgumentList.Add($argument)
		}
		$launcher = [Diagnostics.Process]::Start($start)
		if ($null -eq $launcher)
		{
			throw 'SmartGit did not return a launcher process.'
		}
		$result.processId = $launcher.Id
		if ($HandoffOnly)
		{
			if ($receivers.Count -eq 0)
			{
				throw 'No matching SmartGit process was identified in this session before launch.'
			}
			if (-not $launcher.WaitForExit(10000))
			{
				throw 'The SmartGit launcher did not exit before the handoff deadline.'
			}
			if ($launcher.ExitCode -ne 0)
			{
				throw "The SmartGit launcher exited with code $($launcher.ExitCode)."
			}
			$receiver = $receivers | Where-Object { -not $_.HasExited } | Select-Object -First 1
			if ($null -eq $receiver)
			{
				throw 'No matching pre-existing SmartGit process survived the launcher exit.'
			}
			$message = "The SmartGit launcher exited with code 0 while pre-existing SmartGit PID $($receiver.Id) was still running in this session."
		}
	}
	catch
	{
		Complete-Review 0 'failed' 'review.launch-failed' $_.Exception.Message
	}
	finally
	{
		if ($null -ne $launcher)
		{
			$launcher.Dispose()
		}
		foreach ($receiver in $receivers)
		{
			$receiver.Dispose()
		}
	}
	Complete-Review 0 'opened' 'ok' $message
}

try
{
	# \z rather than $: .NET's $ also matches before a trailing newline, so a hex value carrying one would pass.
	Assert-Input ($ApprovedTip -cmatch '^[0-9a-f]{8,40}\z') 'ApprovedTip must be 8 to 40 lowercase hexadecimal characters.'
	$script:PrimaryIdentity = Get-FinalizeExistingWindowsIdentity $PrimaryWorktree 'Primary worktree'
	$expansion = Invoke-FinalizeNativeText 'git.exe' @('-C', $script:PrimaryIdentity, 'rev-parse', '--verify', '--quiet', "$ApprovedTip^{commit}") $script:PrimaryIdentity
	$expandedTip = $expansion.Stdout.Trim()
	Assert-Input ($expansion.ExitCode -eq 0 -and $expandedTip -cmatch '^[0-9a-f]{40}\z') "ApprovedTip '$ApprovedTip' does not resolve to exactly one commit."
	$ApprovedTip = $expandedTip
	try
	{
		$reviewIdentity = Get-FinalizeReviewIdentity $script:PrimaryIdentity $ApprovedTip
	}
	catch
	{
		Throw-Review 1 'input.invalid' $_.Exception.Message
	}
	$result.approvedTip = $ApprovedTip
	$result.reviewedRepository = $script:PrimaryIdentity
	$result.reviewedPatchId = $reviewIdentity.PatchId
	$result.reviewedChanges = $reviewIdentity.Changes
	Invoke-SmartGit
}
catch
{
	$failure = $_.Exception
	$exitCode = if ($failure.Data.Contains('ExitCode')) { [int]$failure.Data['ExitCode'] } else { 1 }
	$code = if ($failure.Data.Contains('Code')) { [string]$failure.Data['Code'] } else { 'internal.error' }
	Complete-Review $exitCode 'error' $code $failure.Message
}
