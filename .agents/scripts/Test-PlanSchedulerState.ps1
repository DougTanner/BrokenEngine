# Compact scheduler health check: runs WorktreeCli `plan validate` and reports only the decision
# fields. The full plan inventory never reaches the caller, so a session context stays small no
# matter how many Plans exist; run this instead of a raw `plan validate` for any health check.
[CmdletBinding()]
param(
	[string] $RepositoryRoot
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Complete-SchedulerState([int] $ExitCode, [string] $Status, [string] $Code, [string] $Message, $Validation = $null) {
	$result = [ordered]@{
		schemaVersion = 'broken-engine-plan-scheduler-state/v1'
		status = $Status
		code = $Code
		message = $Message
		exitCode = $ExitCode
		planCount = 0
		diagnostics = @()
		notices = @()
		healedClaims = @()
	}
	if ($null -ne $Validation) {
		$properties = @($Validation.PSObject.Properties.Name)
		if ($properties -ccontains 'plans' -and $null -ne $Validation.plans) { $result.planCount = @($Validation.plans).Count }
		foreach ($name in @('diagnostics', 'notices', 'healedClaims')) {
			if ($properties -ccontains $name -and $null -ne $Validation.$name) { $result[$name] = @($Validation.$name) }
		}
	}
	$result | ConvertTo-Json -Compress -Depth 16
	exit $ExitCode
}

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
	$topLevel = @(& git rev-parse --show-toplevel 2>$null)
	if ($LASTEXITCODE -ne 0 -or $topLevel.Count -eq 0 -or [string]::IsNullOrWhiteSpace([string]$topLevel[0])) { Complete-SchedulerState 1 'error' 'git.no-repository' 'The current directory is not inside a Git repository and no -RepositoryRoot was supplied.' }
	$RepositoryRoot = ([string]$topLevel[0]).Trim()
}
$RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$worktreeCli = Join-Path $RepositoryRoot 'Tools\WorktreeCli\Platforms\VisualStudio2026\Output\WorktreeCli.exe'
if (-not (Test-Path -LiteralPath $worktreeCli -PathType Leaf)) { Complete-SchedulerState 2 'blocked' 'worktree-cli.missing' "The provisioned WorktreeCli is missing: '$worktreeCli'." }
$commonLines = @(& git -C $RepositoryRoot rev-parse --path-format=absolute --git-common-dir 2>$null)
if ($LASTEXITCODE -ne 0 -or $commonLines.Count -eq 0 -or [string]::IsNullOrWhiteSpace([string]$commonLines[0])) { Complete-SchedulerState 1 'error' 'git.no-common-directory' "The Git common directory could not be resolved for '$RepositoryRoot'." }
$commonDirectory = ([string]$commonLines[0]).Trim()

$stdout = & $worktreeCli plan validate --repo $commonDirectory --worktree $RepositoryRoot
$exitCode = $LASTEXITCODE
$validation = $null
try { $validation = (@($stdout) -join "`n").Trim() | ConvertFrom-Json -Depth 32 -ErrorAction Stop }
catch { Complete-SchedulerState 1 'error' 'worktree-cli.invalid-json' "plan validate exited $exitCode without one JSON result." }
$properties = @($validation.PSObject.Properties.Name)
$status = if ($properties -ccontains 'status') { [string]$validation.status } else { 'error' }
$code = if ($properties -ccontains 'code') { [string]$validation.code } else { 'missing' }
$message = if ($properties -ccontains 'message') { [string]$validation.message } else { '' }
Complete-SchedulerState $exitCode $status $code $message $validation
