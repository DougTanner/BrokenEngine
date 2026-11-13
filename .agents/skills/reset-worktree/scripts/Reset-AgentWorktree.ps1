# Readies a landed wrapper session worktree for another run: fast-forwards the session branch to the
# primary tip, deletes the worktree's Temp directory, and reruns the setup a newly created session
# worktree gets. Run it from the session worktree root. Every refusal runs before the first change.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$scripts = Join-Path $PSScriptRoot '..\..\..\scripts'
Import-Module (Join-Path $scripts 'AgentScriptCommon.psm1') -Force
Import-Module (Join-Path $scripts 'AgentWorktreeSession.psm1') -Force -DisableNameChecking

try {
	$context = Get-AgentWorktreeSessionContext
	if ($null -eq $context.SessionId) { throw "'$($context.Worktree)' is on branch '$($context.Branch)', not a wrapper session branch, so nothing was changed." }
	$worktree = $context.Worktree
	$tip = $context.PrimaryTip
	Test-AgentWorktreeNoGitOperation $worktree
	Get-AgentWorktreePrimaryIdentity $context.PrimaryRoot | Out-Null
	$dirty = @(Invoke-AgentGit @('-C', $worktree, 'status', '--porcelain', '--untracked-files=all'))
	if ($dirty.Count -ne 0) { throw "The worktree has uncommitted or untracked changes, so nothing was changed: $($dirty -join '; ')" }
	& git -C $worktree merge-base --is-ancestor HEAD $tip
	if ($LASTEXITCODE -gt 1) { throw "git merge-base --is-ancestor HEAD $tip failed." }
	if ($LASTEXITCODE -ne 0) {
		$held = @(Invoke-AgentGit @('-C', $worktree, 'rev-list', "$tip..HEAD"))
		throw "Branch '$($context.Branch)' holds $($held.Count) commit(s) primary tip $tip does not ($($held -join ', ')), so nothing was changed. Land them first; if the primary branch was rewritten under this session, run pwsh -NoProfile -File .agents/scripts/Repair-SessionForkPoint.ps1 from the session worktree root instead."
	}

	Invoke-AgentGit @('-C', $worktree, 'merge', '--ff-only', $tip) | Out-Null
	$temp = Join-Path $worktree 'Temp'
	if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Recurse -Force -Confirm:$false }
	$client = $context.Branch.Split('/')[0]
	& (Join-Path $scripts 'Initialize-AgentWorktreeSession.ps1') -Client $client -RepositoryRoot $context.PrimaryRoot -Worktree $worktree -TargetBranch $context.PrimaryBranch -Baseline $tip
	Write-Host ''
	Write-Host "Reset worktree $worktree on branch $($context.Branch) to baseline $tip."
	exit 0
}
catch {
	[Console]::Error.WriteLine($_.Exception.Message)
	exit 1
}
