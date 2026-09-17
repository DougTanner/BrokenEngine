[CmdletBinding()]
param(
	[string] $ReattachWorktree,
	[switch] $PrepareOnly
)

if ($MyInvocation.InvocationName -eq '.') { return }

function Invoke-OpenCodeWorktree([string] $ReattachWorktree, [switch] $PrepareOnly) {
	$repositoryRoot = git rev-parse --show-toplevel 2>$null

	if (($LASTEXITCODE -ne 0) -or [string]::IsNullOrWhiteSpace($repositoryRoot)) {
		throw 'opencode-worktree must be run inside a Git repository.'
	}

	$repositoryRoot = [System.IO.Path]::GetFullPath($repositoryRoot.Trim())
	$startArguments = @{
		Client = 'opencode'; RepositoryRoot = $repositoryRoot
		ClientExecutable = (Get-Command pwsh -CommandType Application -ErrorAction Stop).Source
		# PSModulePath is cleared because inherited PowerShell 7 module paths break OpenCode's Windows
		# PowerShell archive helper, which breaks its Grep and Skill tools.
		ClientArguments = @('-NoProfile', '-Command', '$env:PSModulePath = $null; & opencode.cmd; exit $LASTEXITCODE')
		PrepareOnly = $PrepareOnly
	}
	if (-not [string]::IsNullOrWhiteSpace($ReattachWorktree)) { $startArguments.ReattachWorktree = $ReattachWorktree }
	& (Join-Path $repositoryRoot '.agents\scripts\Start-AgentWorktreeSession.ps1') @startArguments
	exit $LASTEXITCODE
}

Invoke-OpenCodeWorktree -ReattachWorktree $ReattachWorktree -PrepareOnly:$PrepareOnly
