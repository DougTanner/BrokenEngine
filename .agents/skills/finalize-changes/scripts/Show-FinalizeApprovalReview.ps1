# Sole SmartGit review-tool boundary for a session landing.
#
# Main runs this script from the finalizer's handoff, after the finalizer's
# workflow step 6 filled its command line in — the earlier approval-preparation
# step having returned the approved tip, the acceptance table, and the
# primary-movement check — with the documented redirect. Main opens the window
# and asks the landing confirmation in the same exchange, so the user can
# confirm as soon as their review is done. Callers never reconstruct its SmartGit command inline.
#
# The landing route always passes -LaunchSmartGit, so a session landing always
# attempts the launch and the window opens whenever SmartGit is available.
# Without the switch it only previews the canonical command.
# It runs Git only to expand an abbreviated supplied tip to its full commit ID,
# and never mutates a ref or claims a lock.
#
# Contract: schema broken-engine-finalize-approval-review/v1. Unlike the mutating
# scripts, every preview/launch outcome exits 0 and none report status pass — the
# review outcome is non-blocking, but the attempt is not: main redirects this single-line
# stdout to Temp/finalize-approval-review-result.json, and the resumed finalizer passes
# that receipt path to the landing scripts, which read approvedTip and status from it and
# refuse to change primary when it does not record an attempted launch for the
# exact commit being landed. preview is the default;
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
	[switch] $LaunchSmartGit
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$workflowModule = Join-Path $PSScriptRoot '..\..\..\scripts\FinalizeWorkflowCommon.psm1'
if (-not (Test-Path -LiteralPath $workflowModule)) {
	$workflowModule = Join-Path $PSScriptRoot '..\..\..\..\.agents\scripts\FinalizeWorkflowCommon.psm1'
}
Import-Module $workflowModule -Force

$result = [ordered]@{
	schemaVersion = 'broken-engine-finalize-approval-review/v1'
	status = 'error'
	code = 'internal.error'
	message = 'Review-tool launch did not complete.'
	approvedTip = $null
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
	return "'" + $Value.Replace("'", "''") + "'"
}

function ConvertTo-ProcessArgument([string] $Value)
{
	if ($Value.Contains('"', [StringComparison]::Ordinal))
	{
		throw 'SmartGit arguments cannot contain a double quote.'
	}
	return '"' + $Value + '"'
}

function Invoke-SmartGit
{
	$standardExecutable = 'C:\Program Files\SmartGit\bin\smartgit.exe'
	$arguments = @('--log', $script:PrimaryIdentity, "--anchor-commit=$ApprovedTip")
	$result.arguments = $arguments
	$result.manualCommand = ((@('&', (ConvertTo-PowerShellLiteral $standardExecutable)) + @($arguments | ForEach-Object { ConvertTo-PowerShellLiteral $_ })) -join ' ')
	if (-not $LaunchSmartGit)
	{
		Complete-Review 0 'preview' 'review.preview' 'SmartGit was not launched; run manualCommand to open the candidate.'
	}
	$resolvedExecutable = $null
	if (Test-Path -LiteralPath $standardExecutable -PathType Leaf)
	{
		$resolvedExecutable = $standardExecutable
	}
	else
	{
		$command = Get-Command smartgit.exe -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
		if ($null -ne $command)
		{
			$resolvedExecutable = $command.Source
		}
	}
	if ([string]::IsNullOrWhiteSpace($resolvedExecutable) -or -not (Test-Path -LiteralPath $resolvedExecutable -PathType Leaf))
	{
		Complete-Review 0 'unavailable' 'review.unavailable' 'SmartGit executable was not found.'
	}

	$result.executable = [IO.Path]::GetFullPath($resolvedExecutable)
	try
	{
		$processArguments = @($arguments | ForEach-Object { ConvertTo-ProcessArgument $_ })
		$process = Start-Process -FilePath $result.executable -ArgumentList $processArguments -PassThru
		$result.processId = $process.Id
		$process.Dispose()
	}
	catch
	{
		Complete-Review 0 'failed' 'review.launch-failed' $_.Exception.Message
	}
	Complete-Review 0 'opened' 'ok' 'SmartGit was opened for the approval candidate.'
}

try
{
	# \z rather than $: .NET's $ also matches before a trailing newline, so a hex value carrying one would pass.
	Assert-Input ($ApprovedTip -cmatch '^[0-9a-f]{8,40}\z') 'ApprovedTip must be 8 to 40 lowercase hexadecimal characters.'
	$script:PrimaryIdentity = Get-FinalizeExistingWindowsIdentity $PrimaryWorktree 'Primary worktree'
	# Only an abbreviated tip is expanded, here, so the SmartGit anchor always carries the full ID; a full
	# 40-character tip keeps the unchanged path that never resolved it.
	if ($ApprovedTip.Length -ne 40)
	{
		$expansion = Invoke-FinalizeNativeText 'git.exe' @('-C', $script:PrimaryIdentity, 'rev-parse', '--verify', '--quiet', "$ApprovedTip^{commit}") $script:PrimaryIdentity
		$expandedTip = $expansion.Stdout.Trim()
		Assert-Input ($expansion.ExitCode -eq 0 -and $expandedTip -cmatch '^[0-9a-f]{40}\z') "ApprovedTip '$ApprovedTip' does not resolve to exactly one commit."
		$ApprovedTip = $expandedTip
	}
	$result.approvedTip = $ApprovedTip
	Invoke-SmartGit
}
catch
{
	$failure = $_.Exception
	$exitCode = if ($failure.Data.Contains('ExitCode')) { [int]$failure.Data['ExitCode'] } else { 1 }
	$code = if ($failure.Data.Contains('Code')) { [string]$failure.Data['Code'] } else { 'internal.error' }
	Complete-Review $exitCode 'error' $code $failure.Message
}
