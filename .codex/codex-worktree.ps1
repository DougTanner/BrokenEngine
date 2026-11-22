[CmdletBinding()]
param(
	[string] $ReattachWorktree
)

$root = Split-Path -Parent $PSScriptRoot
& (Join-Path $root '.agents\scripts\Start-AgentWorktreeSession.ps1') -Client codex -RepositoryRoot $root -ReattachWorktree $ReattachWorktree
exit $LASTEXITCODE
