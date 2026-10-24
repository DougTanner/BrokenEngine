
[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)]
	[string] $WorktreeCliExecutable,
	[Parameter(Mandatory = $true)]
	[string] $AgentHarnessExecutable
)

$ErrorActionPreference = 'Stop'

function Get-Help([string] $Executable, [string] $Name) {
	$item = Get-Item -LiteralPath $Executable -Force -ErrorAction Stop
	if ($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -or $item.Length -eq 0) { throw "$Name executable must be a nonempty ordinary file: '$Executable'." }
	$help = @(& $Executable --help 2>&1)
	if ($LASTEXITCODE -ne 0) { throw "$Name executable does not support --help: '$Executable'." }
	return $help -join "`n"
}

function Assert-Contains([string] $Text, [string[]] $Required, [string] $Name) {
	foreach ($value in $Required) { if (-not $Text.Contains($value, [StringComparison]::Ordinal)) { throw "$Name help is missing '$value'." } }
}

$worktreeHelp = Get-Help $WorktreeCliExecutable 'WorktreeCli'
Assert-Contains $worktreeHelp @(
	'Usage: WorktreeCli.exe lock ',
	'WorktreeCli.exe plan ',
	'WorktreeCli.exe build ',
	'WorktreeCli.exe --help',
	'WorktreeCli.exe plan validate ',
	'WorktreeCli.exe plan claim-next ',
	'--primary-worktree',
	'claim-status',
	'unclaim',
	'complete',
	'reject',
	'--user-authorized-rejection'
) 'WorktreeCli'

$harnessHelp = Get-Help $AgentHarnessExecutable 'AgentHarness'
Assert-Contains $harnessHelp @(
	'Usage: AgentHarness.exe --owner TOKEN --port N [--timeout-ms 15000] -',
	'AgentHarness.exe --owner TOKEN --port N [--timeout-ms 15000] "<json>"',
	'AgentHarness.exe lock <token|claim|status|release|steal|heartbeat> ...',
	'AgentHarness.exe --help'
) 'AgentHarness'

[pscustomobject]@{ WorktreeCli = $worktreeHelp; AgentHarness = $harnessHelp } | ConvertTo-Json -Depth 3
