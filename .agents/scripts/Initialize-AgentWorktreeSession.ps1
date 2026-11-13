[CmdletBinding()]
param(
	[Parameter(Mandatory)][ValidateSet('claude', 'codex', 'opencode')][string] $Client,
	[Parameter(Mandatory)][string] $RepositoryRoot,
	[Parameter(Mandatory)][string] $Worktree,
	[Parameter(Mandatory)][string] $TargetBranch,
	[Parameter(Mandatory)][string] $Baseline,
	[int] $WaitSeconds = 500
)

$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot 'AgentScriptCommon.psm1') -Force

function Assert-AgentWorktreeSkillsLink([string] $Worktree) {
	$skillsLink = Join-Path $Worktree '.claude\skills'
	$skillsItem = Get-Item -LiteralPath $skillsLink -Force -ErrorAction SilentlyContinue
	if ($null -ne $skillsItem -and $skillsItem.PSIsContainer -and ($skillsItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -and
		$null -ne (Get-ChildItem -LiteralPath $skillsLink -ErrorAction SilentlyContinue | Select-Object -First 1)) { return }
	$preservedMessage = "'.claude\skills' in worktree '$Worktree' is not a working directory link or an unchanged tracked placeholder. Local changes were preserved; resolve this path and its index entry manually."
	if ($null -eq $skillsItem -or $skillsItem.PSIsContainer -or ($skillsItem.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw $preservedMessage }
	$placeholderBytes = [IO.File]::ReadAllBytes($skillsLink)
	$expectedBytes = [Text.Encoding]::UTF8.GetBytes('../.agents/skills')
	if ([Convert]::ToBase64String($placeholderBytes) -cne [Convert]::ToBase64String($expectedBytes)) { throw $preservedMessage }
	$indexEntry = @(Invoke-AgentGit @('-C', $Worktree, 'ls-files', '--stage', '--', '.claude/skills'))
	if ($indexEntry.Count -ne 1 -or $indexEntry[0] -cnotmatch '^120000 ([0-9a-f]+) 0\t\.claude/skills$') { throw $preservedMessage }
	$blob = $Matches[1]
	$headEntry = @(Invoke-AgentGit @('-C', $Worktree, 'ls-tree', 'HEAD', '--', '.claude/skills'))
	if ($headEntry.Count -ne 1 -or $headEntry[0] -cne "120000 blob $blob`t.claude/skills") { throw $preservedMessage }
	$target = Invoke-AgentProcess 'git' @('-C', $Worktree, 'cat-file', 'blob', $blob) $Worktree
	if ($target.ExitCode -ne 0 -or $target.Stdout -cne '../.agents/skills') { throw $preservedMessage }
	try {
		Invoke-AgentGit @('-C', $Worktree, '-c', 'core.symlinks=true', 'checkout-index', '--force', '--', '.claude/skills') | Out-Null
		$skillsItem = Get-Item -LiteralPath $skillsLink -Force -ErrorAction SilentlyContinue
		if ($null -eq $skillsItem -or -not $skillsItem.PSIsContainer -or -not ($skillsItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -or
			$null -eq (Get-ChildItem -LiteralPath $skillsLink -ErrorAction SilentlyContinue | Select-Object -First 1)) {
			throw 'Git did not produce a working directory link.'
		}
	}
	catch {
		$repairError = $_.Exception.Message
		if ($null -eq (Get-Item -LiteralPath $skillsLink -Force -ErrorAction SilentlyContinue)) {
			try {
				$restore = [IO.File]::Open($skillsLink, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
				try { $restore.Write($placeholderBytes, 0, $placeholderBytes.Length) }
				finally { $restore.Dispose() }
			}
			catch { $repairError += " Placeholder restoration failed: $($_.Exception.Message)" }
		}
		throw "Could not repair '.claude\skills' in worktree '$Worktree': $repairError Enable Windows Developer Mode or use a terminal with symlink privilege and repair the link manually."
	}
}

function Assert-AgentWorktreeOpenCodeSkills([string] $Worktree) {
	$skillsRoot = Join-Path $Worktree '.agents\skills'
	$skillsItem = Get-Item -LiteralPath $skillsRoot -Force -ErrorAction SilentlyContinue
	$skillFile = if ($null -ne $skillsItem -and $skillsItem.PSIsContainer) {
		Get-ChildItem -LiteralPath $skillsRoot -Directory -ErrorAction SilentlyContinue |
			Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'SKILL.md') -PathType Leaf } |
			Select-Object -First 1
	}
	if ($null -eq $skillsItem -or -not $skillsItem.PSIsContainer -or $null -eq $skillFile) {
		throw "'.agents\skills' in worktree '$Worktree' does not contain an available skill package, so OpenCode skills are unavailable."
	}
}

$root = $RepositoryRoot
$worktreeCli = Join-Path $root 'Tools\WorktreeCli\Platforms\VisualStudio2026\Output\WorktreeCli.exe'

# Record the branch this session lands onto; Get-AgentWorktreeSessionContext refuses to resolve a
# session worktree without it. Creation passes the primary branch observed at startup; a reset passes
# the branch the sidecar already records.
$sidecarDirectory = Join-Path $Worktree 'Temp'
New-Item -ItemType Directory -Path $sidecarDirectory -Force | Out-Null
$sidecar = [ordered]@{ schemaVersion = 'broken-engine-session-sidecar/v1'; targetBranch = $TargetBranch } | ConvertTo-Json
[IO.File]::WriteAllText((Join-Path $sidecarDirectory 'session-sidecar.json'), $sidecar, [Text.UTF8Encoding]::new($false))
# The baseline hint Get-AgentWorktreeSessionContext reads. A file, not an environment variable, so a
# child process repairing the fork point can rewrite it.
[IO.File]::WriteAllText((Join-Path $sidecarDirectory 'session-baseline'), $Baseline + "`n", [Text.UTF8Encoding]::new($false))
# Seed the shared harness app-data root with the two caches whose cold rebuild dominates a first
# launch. The runtime validates both on load and regenerates them on any mismatch, so a stale or
# foreign copy costs nothing and needs no verification here. Seeding is a speed-up only, so any
# failure is reported and ignored rather than allowed to block session setup.
try {
	# Only client processes write BrdfLut.cache, so holding it selects each client game's folder
	# without naming any game.
	$cacheRoots = @((Join-Path $root 'Temp\AppData'), $env:APPDATA)
	$gameFolders = @($cacheRoots | Where-Object { Test-Path -LiteralPath $_ -PathType Container } |
		ForEach-Object { Get-ChildItem -LiteralPath $_ -Directory } |
		Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'BrdfLut.cache') -PathType Leaf } |
		ForEach-Object Name | Sort-Object -Unique)
	foreach ($gameFolder in $gameFolders) {
		$cacheSources = @()
		foreach ($cacheName in @('pipeline.cache', 'BrdfLut.cache')) {
			$primaryCache = Join-Path $root "Temp\AppData\$gameFolder\$cacheName"
			$userCache = Join-Path $env:APPDATA "$gameFolder\$cacheName"
			if (Test-Path -LiteralPath $primaryCache -PathType Leaf) { $cacheSources += $primaryCache }
			elseif (Test-Path -LiteralPath $userCache -PathType Leaf) { $cacheSources += $userCache }
		}
		if ($cacheSources.Count -ne 0) {
			$cacheTarget = Join-Path $sidecarDirectory "AppData\$gameFolder"
			New-Item -ItemType Directory -Path $cacheTarget -Force | Out-Null
			foreach ($cacheSource in $cacheSources) { Copy-Item -LiteralPath $cacheSource -Destination $cacheTarget -Force }
		}
	}
}
catch { Write-Warning "Seeding the worktree shader caches failed, so the first launch rebuilds them: $($_.Exception.Message)" }
# Each step below is separated by a blank line so its output reads as its own block in the transcript.
& (Join-Path $root '.agents\scripts\Bootstrap-AgentTools.ps1') -RepositoryRoot $root -WaitSeconds $WaitSeconds
Write-Host ''
& (Join-Path $root '.agents\scripts\Provision-WorktreeThirdParty.ps1') -RepositoryRoot $Worktree -WaitSeconds $WaitSeconds
if ($Client -ceq 'opencode') { Assert-AgentWorktreeOpenCodeSkills $Worktree }
else { Assert-AgentWorktreeSkillsLink $Worktree }
Write-Host ''
& (Join-Path $root '.agents\scripts\Build-WorktreeTarget.ps1') -Target DataPacker -Worktree $Worktree -WorktreeCliExecutable $worktreeCli -PrimaryCheckout $root -Baseline $Baseline
Write-Host ''
& (Join-Path $root '.agents\scripts\Build-WorktreeTarget.ps1') -Target ModelViewer -Worktree $Worktree -WorktreeCliExecutable $worktreeCli -PrimaryCheckout $root -Baseline $Baseline
