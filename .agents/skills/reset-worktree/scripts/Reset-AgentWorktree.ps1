# Readies a landed wrapper session worktree for another run: fast-forwards the session branch to the
# primary tip (handing off to the updated script in a fresh pwsh process when HEAD advances),
# deletes the worktree's Temp directory, and reruns the setup a newly created session worktree gets.
# Run it from the session worktree root. Initial refusals run before the first change.
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
	$coretide = Get-AgentCoretideRepository $worktree
	if ($null -ne $coretide) {
		$coretideDirty = @(Invoke-AgentGit @('-C', $coretide, 'status', '--porcelain', '--untracked-files=all'))
		if ($coretideDirty.Count -ne 0) { throw "The Coretide clone has uncommitted or untracked changes, so nothing was changed: $($coretideDirty -join '; ')" }
		$coretideHeld = @(Invoke-AgentGit @('-C', $coretide, 'rev-list', '@{upstream}..HEAD'))
		if ($coretideHeld.Count -ne 0) { throw "The Coretide clone holds $($coretideHeld.Count) commit(s) its upstream does not ($($coretideHeld -join ', ')), so nothing was changed. Land them first." }
	}

	$head = @(Invoke-AgentGit @('-C', $worktree, 'rev-parse', 'HEAD'))[0].Trim()
	if ($head -cne $tip) {
		Invoke-AgentGit @('-C', $worktree, 'merge', '--ff-only', $tip) | Out-Null
		& pwsh -NoProfile -NonInteractive -File $PSCommandPath
		if ($LASTEXITCODE -ne 0) {
			[Console]::Error.WriteLine("Fast-forward to $tip completed before the updated reset script failed (exit $LASTEXITCODE); resolve the error above and rerun.")
			exit $LASTEXITCODE
		}
		exit 0
	}
	$temp = Join-Path $worktree 'Temp'
	if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp -Recurse -Force -Confirm:$false }
	& (Join-Path $scripts 'Initialize-AgentWorktreeSession.ps1') -RepositoryRoot $context.PrimaryRoot -Worktree $worktree -TargetBranch $context.PrimaryBranch -Baseline $tip
	Write-Host ''
	Write-Host "Reset worktree $worktree on branch $($context.Branch) to baseline $tip."
	exit 0
}
catch {
	[Console]::Error.WriteLine($_.Exception.Message)
	exit 1
}
