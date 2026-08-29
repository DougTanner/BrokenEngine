# Sole SmartGit review-tool boundary for a session landing.
#
# Invoked last in workflow step 4 — after approval preparation returned the
# approved tip — as the final tool invocation before the landing summary renders.
# Opening the log window here means the user returns from their review to a
# finished confirmation question rather than a working agent. Callers never
# reconstruct its SmartGit command inline.
#
# The verification gate below, not the caller's discipline, is what guarantees that
# ordering: -LaunchSmartGit only reaches a launch when -VerificationPromptFile and
# -VerificationOutFile prove a /verify-changes PASS bound to this exact tip, so a
# window cannot open before the verification it is supposed to follow.
#
# The landing route always passes -LaunchSmartGit, so a session landing always
# attempts the launch and the window opens whenever SmartGit is available.
# Without the switch it only previews the canonical command, requires neither
# verification file, and reports verification as null.
# It runs Git only to expand an abbreviated supplied tip to its full commit ID,
# and never mutates a ref or claims a lock.
#
# Contract: schema broken-engine-finalize-approval-review/v1. Unlike the mutating
# scripts, every preview/launch outcome exits 0 and none report status pass — the
# review outcome is non-blocking, but the attempt is not: the caller redirects this
# single-line stdout to Temp/finalize-approval-review-result.json and passes that
# receipt to the landing scripts, which read approvedTip, status, and verification
# from it and refuse to change primary when it does not prove a verified launch
# attempt for the exact commit being landed. preview is the default;
# opened is the launch success path; unavailable/failed are non-blocking,
# and the caller copies message and the exact manualCommand into the approval response while keeping the landing gate in
# force. Only invalid input exits 1, with status error and a code naming the cause
# (input.invalid for a malformed tip, a tip that resolves to no commit, or a
# fixture gate; verification.missing, verification.tip-mismatch, and
# verification.not-pass for the launch-time verification gate; internal.error for a
# primary worktree that will not resolve).
# A clean identical post-confirmation rebase that
# preserves the existing confirmation must not reopen the review tool the user
# already saw: that path does not invoke this script at all. A material change
# requiring a refreshed confirmation does invoke it again, against the newly
# reviewed candidate, before that refreshed confirmation.
[CmdletBinding()]
param(
	[Parameter(Mandatory)][string] $PrimaryWorktree,
	[Parameter(Mandatory)][string] $ApprovedTip,
	[switch] $LaunchSmartGit,
	[AllowEmptyString()][string] $VerificationPromptFile,
	[AllowEmptyString()][string] $VerificationOutFile,
	[AllowEmptyString()][string] $FixtureSmartGitExecutable,
	[ValidateSet('none', 'smartgit-launch')][string] $FixtureFailure = 'none'
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
	verification = $null
	executable = $null
	arguments = @()
	manualCommand = $null
	processId = $null
}

$script:PrimaryIdentity = $null
$script:HasFixtureSmartGitExecutable = $PSBoundParameters.ContainsKey('FixtureSmartGitExecutable')

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

function Get-VerificationFile([string] $Path, [string] $ParameterName)
{
	if ([string]::IsNullOrWhiteSpace($Path))
	{
		Throw-Review 1 'verification.missing' "-LaunchSmartGit requires -$ParameterName from the /verify-changes dispatch that returned PASS for this tip."
	}
	$item = Get-Item -LiteralPath $Path -Force -ErrorAction SilentlyContinue
	if ($null -eq $item -or $item.PSIsContainer)
	{
		Throw-Review 1 'verification.missing' "-$ParameterName '$Path' is not a readable file."
	}
	return $item
}

# The generated evidence section is the only part of the prompt this script trusts for the head:
# the manager-authored scope section is free prose and could name any commit.
function Get-VerificationEvidence([string[]] $PromptLines)
{
	for ($index = 0; $index -lt $PromptLines.Count; $index++)
	{
		if ($PromptLines[$index] -cne '# (c) Evidence')
		{
			continue
		}
		$body = @()
		for ($scan = $index + 1; $scan -lt $PromptLines.Count -and -not $PromptLines[$scan].StartsWith('# ', [StringComparison]::Ordinal); $scan++)
		{
			$body += $PromptLines[$scan]
		}
		return ($body -join "`n")
	}
	return $null
}

# Binds the launch to a /verify-changes PASS for exactly this tip. Runs after the tip is expanded to
# its full ID, so every comparison here reads the same 40-character commit the SmartGit anchor uses.
function Assert-Verification
{
	$promptItem = Get-VerificationFile $VerificationPromptFile 'VerificationPromptFile'
	$outItem = Get-VerificationFile $VerificationOutFile 'VerificationOutFile'
	$promptText = [IO.File]::ReadAllText($promptItem.FullName)
	$outText = [IO.File]::ReadAllText($outItem.FullName)
	$evidence = Get-VerificationEvidence ($promptText -split "`r?`n")
	if ($null -eq $evidence -or $evidence -cnotmatch "(?m)^Head: $ApprovedTip$")
	{
		Throw-Review 1 'verification.tip-mismatch' "The verification prompt's generated evidence section does not record 'Head: $ApprovedTip'."
	}
	if ($promptText -cnotmatch '(?m)^Read and execute the Broken Engine `verify-changes` skill')
	{
		Throw-Review 1 'verification.tip-mismatch' 'The verification prompt does not carry the verify-changes reviewer role line.'
	}
	if (-not $outText.Contains($ApprovedTip, [StringComparison]::Ordinal))
	{
		Throw-Review 1 'verification.tip-mismatch' "The verification output never restates the reviewed head $ApprovedTip."
	}
	# Same bold-stripped last-line verdict the routed reviewer contract enforces in .codex/codex-review.ps1.
	$outLines = $outText -split "`r?`n"
	$lastIndex = $outLines.Count - 1
	while ($lastIndex -ge 0 -and [string]::IsNullOrWhiteSpace($outLines[$lastIndex]))
	{
		$lastIndex--
	}
	$verdict = if ($lastIndex -lt 0) { '' } else { $outLines[$lastIndex].Trim() -replace '^\*\*', '' -replace '\*\*$', '' }
	if ($verdict -cne 'PASS')
	{
		Throw-Review 1 'verification.not-pass' "The verification output's final verdict is '$verdict', not PASS."
	}
	if ($outItem.LastWriteTimeUtc -lt $promptItem.LastWriteTimeUtc)
	{
		Throw-Review 1 'verification.not-pass' 'The verification output predates its prompt, so it cannot be that review''s result.'
	}
	$result.verification = [ordered]@{
		promptFile = $promptItem.FullName
		outFile = $outItem.FullName
		head = $ApprovedTip
		verdict = $verdict
	}
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
	if ($script:HasFixtureSmartGitExecutable)
	{
		$resolvedExecutable = $FixtureSmartGitExecutable
	}
	elseif (Test-Path -LiteralPath $standardExecutable -PathType Leaf)
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
		if ($FixtureFailure -ceq 'smartgit-launch')
		{
			throw 'Fixture forced a SmartGit launch failure.'
		}
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
	if ($FixtureFailure -cne 'none' -or $script:HasFixtureSmartGitExecutable)
	{
		Assert-Input ($env:BROKEN_ENGINE_FINALIZE_APPROVAL_PREPARATION_FIXTURE -ceq '1') 'Fixture-only inputs require the finalization preparation fixture environment.'
	}
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
	if ($LaunchSmartGit)
	{
		Assert-Verification
	}

	Invoke-SmartGit
}
catch
{
	$failure = $_.Exception
	$exitCode = if ($failure.Data.Contains('ExitCode')) { [int]$failure.Data['ExitCode'] } else { 1 }
	$code = if ($failure.Data.Contains('Code')) { [string]$failure.Data['Code'] } else { 'internal.error' }
	Complete-Review $exitCode 'error' $code $failure.Message
}
