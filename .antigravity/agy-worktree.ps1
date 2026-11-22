[CmdletBinding()]
param(
	[string] $ReattachWorktree
)

$root = Split-Path -Parent $PSScriptRoot
& (Join-Path $root '.agents\scripts\Start-AgentWorktreeSession.ps1') -Client agy -RepositoryRoot $root -ReattachWorktree $ReattachWorktree
exit $LASTEXITCODE
