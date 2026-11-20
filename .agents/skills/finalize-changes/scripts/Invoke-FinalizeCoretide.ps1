# Lands the session Coretide clone's changes with Git porcelain only. -Prepare commits every clone change and squashes
# the commits its upstream lacks into one commit on their merge base under the Coretide message file. -Land, under the
# landing lease the caller already holds, checks the approval receipts, rebases onto a moved upstream only for an
# identical patch, and fast-forwards the primary Coretide checkout from the clone; a rerun after landing reports none.
# It never pushes and changes no primary Coretide configuration. Result schema broken-engine-finalize-coretide/v1;
# exit 0 for pass, none, or landed, 2 for blocked, 1 for error.
[CmdletBinding()]
param(
	[Parameter(Mandatory, ParameterSetName = 'Prepare')][switch] $Prepare,
	[Parameter(Mandatory, ParameterSetName = 'Land')][switch] $Land,
	[Parameter(Mandatory)][string] $CurrentWorktree,
	[Parameter(Mandatory, ParameterSetName = 'Prepare')][string] $CommitMessageFile,
	[Parameter(Mandatory, ParameterSetName = 'Land')][string] $PrimaryWorktree,
	[Parameter(ParameterSetName = 'Land')][string] $OwnerToken,
	[Parameter(Mandatory, ParameterSetName = 'Land')][string] $ApprovedCoretideCommit,
	[Parameter(ParameterSetName = 'Land')][string] $ApprovalReviewResultFile,
	[Parameter(ParameterSetName = 'Land')][string] $ApprovedEngineCommit,
	[Parameter(ParameterSetName = 'Land')][string] $EngineApprovalReviewResultFile,
	[Parameter(ParameterSetName = 'Land')][switch] $ReleasePlanClaim
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$scripts = Join-Path $PSScriptRoot '..\..\..\scripts'
if (-not (Test-Path -LiteralPath (Join-Path $scripts 'FinalizeWorkflowCommon.psm1') -PathType Leaf)) {
	$scripts = Join-Path $PSScriptRoot '..\..\..\..\.agents\scripts'
}
Import-Module (Join-Path $scripts 'FinalizeWorkflowCommon.psm1') -Force
Import-Module (Join-Path $scripts 'AgentScriptCommon.psm1') -Force
Import-Module (Join-Path $scripts 'AgentWorktreeSession.psm1') -Force -DisableNameChecking

$result = [ordered]@{
	schemaVersion = 'broken-engine-finalize-coretide/v1'
	status = 'error'
	code = 'internal.error'
	message = 'Coretide finalization did not run.'
	coretideCommit = $null
	planClaim = [ordered]@{ requested = [bool]$ReleasePlanClaim; released = $false }
	residuals = [Collections.Generic.List[string]]::new()
}

function Complete-FinalizeCoretide([int] $ExitCode, [string] $Status, [string] $Code, [string] $Message) {
	$result.status = $Status
	$result.code = $Code
	$result.message = $Message
	Write-Output ($result | ConvertTo-Json -Depth 32 -Compress)
	exit $ExitCode
}

# The patch identity of a single-parent commit, the same for any rebase that replays it unchanged.
function Get-CoretidePatchId([string] $Repository, [string] $Commit) {
	$diff = Invoke-FinalizeGit $Repository @('diff-tree', '-p', '--no-color', '--no-ext-diff', '--full-index', '--binary', '-r', "$Commit^", $Commit)
	$start = [Diagnostics.ProcessStartInfo]::new()
	$start.FileName = 'git.exe'
	$start.WorkingDirectory = $Repository
	$start.UseShellExecute = $false
	$start.CreateNoWindow = $true
	$start.RedirectStandardInput = $true
	$start.RedirectStandardOutput = $true
	$start.RedirectStandardError = $true
	foreach ($argument in @('-C', $Repository, 'patch-id', '--stable')) { [void] $start.ArgumentList.Add($argument) }
	$process = [Diagnostics.Process]::new()
	$process.StartInfo = $start
	if (-not $process.Start()) { throw 'Could not start git patch-id.' }
	$stdoutTask = $process.StandardOutput.ReadToEndAsync()
	$stderrTask = $process.StandardError.ReadToEndAsync()
	$bytes = [Text.UTF8Encoding]::new($false).GetBytes($diff)
	$process.StandardInput.BaseStream.Write($bytes, 0, $bytes.Length)
	$process.StandardInput.Close()
	$stdout = $stdoutTask.GetAwaiter().GetResult()
	$stderr = $stderrTask.GetAwaiter().GetResult()
	$process.WaitForExit()
	$exitCode = $process.ExitCode
	$process.Dispose()
	if ($exitCode -ne 0) { throw "git patch-id failed: $($stderr.Trim())" }
	return $stdout.Trim().Split(' ')[0]
}

# The claim is machine-local bookkeeping, so a failed delete is a residual, as in Invoke-FinalizeLanding.ps1. Deleting
# an absent claim reports already-absent, so a rerun after a crash past the advance still releases it.
function Complete-PlanClaimRelease {
	if (-not $ReleasePlanClaim) { return }
	try {
		$unclaim = Invoke-FinalizeNativeText $worktreeCli @('plan', 'unclaim', '--repo', $session.CommonDirectory, '--worktree', $session.Worktree, '--owner', $sessionId, '--session', $sessionId) $session.Worktree
		$unclaimJson = $unclaim.Stdout.Trim() | ConvertFrom-Json -Depth 32 -ErrorAction Stop
		if ($unclaim.ExitCode -ne 0 -or [string]$unclaimJson.code -cnotin @('released', 'already-absent', 'none')) {
			throw "WorktreeCli reported '$($unclaimJson.code)': $($unclaimJson.message)"
		}
		$result.planClaim.released = $true
	}
	catch {
		$result.residuals.Add("Plan claim delete failed after landing; the machine-local claim expires on its own: $($_.Exception.Message)")
	}
}

function Resolve-FinalizeCommit([string] $Repository, [string] $Revision) {
	$run = Invoke-FinalizeNativeText 'git.exe' @('-C', $Repository, 'rev-parse', '--verify', '--quiet', "$Revision^{commit}") $Repository
	if ($run.ExitCode -ne 0) { return $Revision }
	return $run.Stdout.Trim()
}

try {
	$coretide = Get-AgentCoretideRepository $CurrentWorktree
	if ($null -eq $coretide) { Complete-FinalizeCoretide 0 'none' 'coretide.absent' "'$CurrentWorktree' holds no Coretide clone." }

	if ($Prepare) {
		try { Test-AgentWorktreeNoGitOperation $coretide }
		catch { Complete-FinalizeCoretide 2 'blocked' 'git.operation-active' $_.Exception.Message }
		# Resolved here because Git reads a relative -F path from the clone, not from the caller's directory.
		$messageFile = [IO.Path]::GetFullPath($CommitMessageFile)
		if (-not (Test-Path -LiteralPath $messageFile -PathType Leaf)) { Complete-FinalizeCoretide 1 'error' 'input.message-file-missing' "CommitMessageFile '$messageFile' is not a file." }
		Invoke-FinalizeGit $coretide @('add', '-A') | Out-Null
		if (-not (Test-FinalizeGitSuccess $coretide @('diff', '--cached', '--quiet'))) {
			Invoke-FinalizeGit $coretide @('commit', '-F', $messageFile) | Out-Null
		}
		if ((Invoke-FinalizeGit $coretide @('rev-list', '--count', '@{upstream}..HEAD')).Trim() -ceq '0') {
			Complete-FinalizeCoretide 0 'none' 'coretide.no-changes' 'The Coretide clone holds no commit its upstream lacks.'
		}
		$base = (Invoke-FinalizeGit $coretide @('merge-base', 'HEAD', '@{upstream}')).Trim()
		Invoke-FinalizeGit $coretide @('reset', '--soft', $base) | Out-Null
		Invoke-FinalizeGit $coretide @('commit', '-F', $messageFile) | Out-Null
		$result.coretideCommit = (Invoke-FinalizeGit $coretide @('rev-parse', 'HEAD')).Trim()
		# SmartGit opens only a repository it already knows, so the review runs in the primary Coretide checkout. Fetching
		# the commit by ID adds its objects and FETCH_HEAD there without moving a ref, HEAD, or the working tree.
		$primaryCoretide = Get-AgentCoretideRepository (Get-AgentWorktreeRecords $CurrentWorktree)[0].Path
		if ($null -eq $primaryCoretide) { Complete-FinalizeCoretide 1 'error' 'primary.coretide-absent' 'The main worktree holds no Coretide repository.' }
		Invoke-FinalizeGit $primaryCoretide @('fetch', '--no-tags', $coretide, $result.coretideCommit) | Out-Null
		Complete-FinalizeCoretide 0 'pass' 'ok' 'The Coretide changes are one commit on the merge base with the clone upstream.'
	}

	# -Land: every check runs before the first change.
	$session = Get-FinalizeGitIdentity $CurrentWorktree 'Session worktree'
	$sessionId = (Get-AgentWorktreeSessionContext -Worktree $session.Worktree).SessionId
	$worktreeCli = Join-Path $PrimaryWorktree 'Tools\WorktreeCli\Platforms\VisualStudio2026\Output\WorktreeCli.exe'
	$lock = Get-FinalizeLandingLockState $worktreeCli $session.CommonDirectory $session.Worktree
	if ($lock.Kind -cne 'live' -or [string]::IsNullOrWhiteSpace($OwnerToken) -or -not (Test-FinalizeLandingLockClaimIdentity $lock.Status $OwnerToken $sessionId $session.Worktree)) {
		Complete-FinalizeCoretide 2 'blocked' 'landing-lock.not-held' 'The supplied owner token is not a live landing lease for this session and worktree.'
	}
	Invoke-FinalizeGit $coretide @('fetch') | Out-Null
	# Contained already: a rerun after landing, including one after a crash between the advance and the final fetch.
	if (Test-FinalizeGitSuccess $coretide @('merge-base', '--is-ancestor', 'HEAD', '@{upstream}')) {
		$result.coretideCommit = (Invoke-FinalizeGit $coretide @('rev-parse', 'HEAD')).Trim()
		Complete-PlanClaimRelease
		Complete-FinalizeCoretide 0 'none' 'coretide.already-landed' 'The clone HEAD is already in its upstream.'
	}
	$approved = Resolve-FinalizeCommit $coretide $ApprovedCoretideCommit
	$head = (Invoke-FinalizeGit $coretide @('rev-parse', 'HEAD')).Trim()
	if ($head -cne $approved) { Complete-FinalizeCoretide 2 'blocked' 'coretide.unapproved-head' "The clone HEAD $head is not the approved Coretide commit $ApprovedCoretideCommit." }
	$receiptFailure = Test-FinalizeApprovalReviewReceipt $ApprovalReviewResultFile $approved
	if ($null -eq $receiptFailure -and -not ([string]::IsNullOrWhiteSpace($ApprovedEngineCommit) -and [string]::IsNullOrWhiteSpace($EngineApprovalReviewResultFile))) {
		$receiptFailure = Test-FinalizeApprovalReviewReceipt $EngineApprovalReviewResultFile (Resolve-FinalizeCommit $session.Worktree $ApprovedEngineCommit)
	}
	if ($null -ne $receiptFailure) { Complete-FinalizeCoretide 2 'blocked' $receiptFailure.Code $receiptFailure.Message }
	$branch = (Invoke-FinalizeGit $coretide @('symbolic-ref', '--short', 'HEAD')).Trim()
	$upstreamBranch = (Invoke-FinalizeGit $coretide @('config', '--get', "branch.$branch.merge")).Trim() -replace '^refs/heads/', ''
	$primaryCoretide = Join-Path $PrimaryWorktree 'Projects\Coretide'
	$primaryBranch = (Invoke-FinalizeGit $primaryCoretide @('symbolic-ref', '--short', 'HEAD')).Trim()
	if ($primaryBranch -cne $upstreamBranch) {
		Complete-FinalizeCoretide 2 'blocked' 'primary.branch-mismatch' "The primary Coretide checkout is on '$primaryBranch', not the clone upstream branch '$upstreamBranch'."
	}
	if ((Invoke-FinalizeGit $primaryCoretide @('status', '--porcelain', '-z', '--untracked-files=all')).Length -ne 0) {
		Complete-FinalizeCoretide 2 'blocked' 'primary.dirty' 'The primary Coretide checkout is not clean.'
	}

	$upstream = (Invoke-FinalizeGit $coretide @('rev-parse', '@{upstream}')).Trim()
	if ((Invoke-FinalizeGit $coretide @('rev-parse', "$approved^")).Trim() -cne $upstream) {
		$approvedPatch = Get-CoretidePatchId $coretide $approved
		if (-not (Test-FinalizeGitSuccess $coretide @('rebase', '@{upstream}'))) {
			Invoke-FinalizeGit $coretide @('rebase', '--abort') | Out-Null
			Complete-FinalizeCoretide 2 'blocked' 'rebase.conflicted' 'Rebasing the approved Coretide commit onto its moved upstream conflicted; the rebase was aborted.'
		}
		if ((Get-CoretidePatchId $coretide 'HEAD') -cne $approvedPatch) {
			Invoke-FinalizeGit $coretide @('reset', '--hard', $approved) | Out-Null
			Complete-FinalizeCoretide 2 'blocked' 'rebase.patch-not-identical' 'The rebased Coretide commit changes a different patch than the approved one; the clone was reset to the approved commit.'
		}
	}
	$advance = Invoke-FinalizeNativeText 'git.exe' @('-C', $primaryCoretide, 'pull', '--ff-only', '--no-rebase', $coretide, 'HEAD') $primaryCoretide
	if ($advance.ExitCode -ne 0) {
		Complete-FinalizeCoretide 2 'blocked' 'primary.advance-failed' "Fast-forwarding the primary Coretide checkout from the clone failed: $($advance.Stderr.Trim())"
	}
	Invoke-FinalizeGit $coretide @('fetch') | Out-Null
	$result.coretideCommit = (Invoke-FinalizeGit $coretide @('rev-parse', 'HEAD')).Trim()
	Complete-PlanClaimRelease
	Complete-FinalizeCoretide 0 'landed' 'ok' 'The primary Coretide checkout advanced to the clone commit.'
}
catch {
	Complete-FinalizeCoretide 1 'error' 'internal.error' $_.Exception.Message
}
