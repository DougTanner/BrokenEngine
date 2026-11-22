[CmdletBinding()]
param(
	[string] $ReattachWorktree,
	[switch] $PrepareOnly
)

$root = Split-Path -Parent $PSScriptRoot
& (Join-Path $root '.agents\scripts\Start-AgentWorktreeSession.ps1') -Client opencode -RepositoryRoot $root -ReattachWorktree $ReattachWorktree -PrepareOnly:$PrepareOnly
exit $LASTEXITCODE
