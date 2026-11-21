[CmdletBinding()]
param(
	[string] $ReattachWorktree
)

function Invoke-AgyWorktree([string] $ReattachWorktree) {
	$repositoryRoot = git rev-parse --show-toplevel 2>$null

	if (($LASTEXITCODE -ne 0) -or [string]::IsNullOrWhiteSpace($repositoryRoot)) {
		throw 'agy-worktree must be run inside a Git repository.'
	}

	$repositoryRoot = [System.IO.Path]::GetFullPath($repositoryRoot.Trim())
	$startArguments = @{
		Client = 'agy'; RepositoryRoot = $repositoryRoot
		ClientArguments = @('--dangerously-skip-permissions')
	}
	if (-not [string]::IsNullOrWhiteSpace($ReattachWorktree)) { $startArguments.ReattachWorktree = $ReattachWorktree }
	& (Join-Path $repositoryRoot '.agents\scripts\Start-AgentWorktreeSession.ps1') @startArguments
	exit $LASTEXITCODE
}

if ($MyInvocation.InvocationName -eq '.') { return }

Invoke-AgyWorktree -ReattachWorktree $ReattachWorktree
