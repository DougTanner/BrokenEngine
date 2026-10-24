[CmdletBinding()]
param(
	[Parameter(Mandatory)][ValidateSet('claude', 'codex', 'opencode')][string] $Client,
	[Parameter(Mandatory)][string] $RepositoryRoot,
	[string] $ClientExecutable,
	[string[]] $ClientArguments = @(),
	[string] $ReattachWorktree,
	[switch] $PrepareOnly,
	[int] $WaitSeconds = 500
)

$ErrorActionPreference = 'Stop'
if ($Client -cne 'claude' -and $Client -cne 'codex' -and $Client -cne 'opencode') { throw "Client must be lowercase 'claude', 'codex', or 'opencode'." }

# Claude carries client arguments out of band because -File treats a leading dash as a
# PowerShell parameter. Do not clear the transport variable until reattach validation succeeds:
# rejected reattach attempts must leave the caller environment unchanged.
$clearClaudeArgumentTransport = $false
if ($Client -ceq 'claude' -and -not [string]::IsNullOrEmpty($env:BROKEN_ENGINE_CLIENT_ARGUMENTS))
{
	$decoded = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($env:BROKEN_ENGINE_CLIENT_ARGUMENTS))
	$parts = @($decoded -split "`0")
	if ($parts.Count -ge 2) { $ClientArguments = @($parts[0..($parts.Count - 2)]) }
	$clearClaudeArgumentTransport = $true
}

Import-Module (Join-Path $PSScriptRoot 'AgentScriptCommon.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'WorktreeCliSessionExclusion.psm1') -Force -DisableNameChecking
Import-Module (Join-Path $PSScriptRoot 'AgentWorktreeSession.psm1') -Force -DisableNameChecking

function Assert-AgentWorktreeSkillsLink([string] $Worktree) {
	$skillsLink = Join-Path $Worktree '.claude\skills'
	$skillsItem = Get-Item -LiteralPath $skillsLink -Force -ErrorAction SilentlyContinue
	if ($null -ne $skillsItem -and $skillsItem.PSIsContainer -and ($skillsItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -and
		$null -ne (Get-ChildItem -LiteralPath $skillsLink -ErrorAction SilentlyContinue | Select-Object -First 1)) { return }
	$preservedMessage = "'.claude\skills' in worktree '$Worktree' is not a working directory link or an unchanged tracked placeholder. Local changes were preserved; resolve this path and its index entry manually, then rerun startup or reattach."
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
		throw "Could not repair '.claude\skills' in worktree '$Worktree': $repairError Enable Windows Developer Mode or use a terminal with symlink privilege, then rerun startup or reattach."
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

$worktreeCreated = $false
$exitCode = 1
try {
	$primary = Get-AgentWorktreePrimaryIdentity $RepositoryRoot
	$root = $primary.Root
	$worktreeCli = Join-Path $root 'Tools\WorktreeCli\Platforms\VisualStudio2026\Output\WorktreeCli.exe'
	$identity = $null
	$rebaseInProgress = $false
	$firstTask = $null

	if (-not [string]::IsNullOrWhiteSpace($ReattachWorktree)) {
		# Reattach identity is Git-derived: the worktree must be uniquely registered against this primary
		# checkout and sit on a session branch whose uuid is the session owner. An in-progress rebase is
		# reported, not repaired - the session resolves it itself.
		$reattachTarget = Get-AgentCanonicalPath $ReattachWorktree
		$records = @(Get-AgentWorktreeRecords $root | Where-Object { -not $_.Bare -and $_.Path.Equals($reattachTarget, [StringComparison]::OrdinalIgnoreCase) })
		if ($records.Count -ne 1 -or $records[0].Prunable) { throw "Worktree '$reattachTarget' is not uniquely registered against '$root', or is prunable." }
		$branch = $records[0].Branch
		foreach ($marker in @('rebase-merge', 'rebase-apply')) {
			$markerPath = @(Invoke-AgentGit @('-C', $reattachTarget, 'rev-parse', '--path-format=absolute', '--git-path', $marker))[0].Trim()
			if (-not (Test-Path -LiteralPath $markerPath)) { continue }
			$rebaseInProgress = $true
			# A rebase detaches HEAD, so the worktree registration reports no branch: recover the branch
			# identity from the rebase's own record of where it will land.
			$headName = Join-Path $markerPath 'head-name'
			if ([string]::IsNullOrWhiteSpace($branch) -and (Test-Path -LiteralPath $headName)) { $branch = (Get-Content -LiteralPath $headName -Raw).Trim() -replace '^refs/heads/', '' }
		}
		if ($branch -cnotmatch "^$Client/[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$") { throw "Worktree '$reattachTarget' is not checked out on a $Client session branch: '$branch'." }
		if ($rebaseInProgress) {
			$baseline = @(Invoke-AgentGit @('-C', $reattachTarget, 'merge-base', 'HEAD', $primary.Head))[0].Trim()
			$firstTask = "this worktree has a rebase in progress - resolve your rebase first. Fix the conflicted files and run 'git rebase --continue'."
		}
		else {
			# The primary branch name and the primary HEAD are read separately, so they disagree if that
			# checkout switched branches between the two reads. Resolving the tip from the branch ref binds
			# every step below - fork point, ancestor test, rebase target, baseline - to one branch, because
			# rebasing onto another branch's tip would relocate this session's work.
			$primaryTip = @(Invoke-AgentGit @('-C', $reattachTarget, 'rev-parse', "refs/heads/$($primary.Branch)"))[0].Trim()
			# Rewriting the primary branch (a daily history squash) orphans the commits this branch was
			# created from, so the shared repair recovers the real fork point and, when that fork point left
			# primary's history, replays only this session's own commits onto the new tip. Here an ancestor
			# fork point simply means no repair is needed.
			try { $baseline = (Repair-AgentWorktreeForkPoint $reattachTarget $primary.Branch $primaryTip $branch).Baseline }
			catch {
				if ($_.Exception.Data['Rebased'] -ne $true) { throw }
				# The branch already sits on the new tip, so the session starts and resolves the restore
				# itself, like an in-progress rebase.
				$baseline = $primaryTip
				$firstTask = "the reattach rebased this branch, but restoring its saved local work failed: $($_.Exception.Message) Do not run that stash apply command: before anything else, resolve any conflicts and make sure every saved change is back, taking only missing ones from that recovery stash, never reapplying what it already restored."
			}
		}
		$identity = [pscustomobject]@{ Primary = $primary; Worktree = $reattachTarget; Branch = $branch; TargetBranch = $primary.Branch; Baseline = $baseline }
	}
	else {
		$repositoryName = Split-Path -Leaf $root
		$uuid = [guid]::NewGuid().ToString()
		$branch = "$Client/$uuid"
		$clientHome = ".$Client"
		$worktreeRoot = Join-Path $HOME "$clientHome\worktrees\$repositoryName"
		$worktree = Join-Path $worktreeRoot $uuid
		if (Test-Path -LiteralPath $worktree) { throw "Generated worktree path already exists: '$worktree'." }
		if (@(Invoke-AgentGit @('-C', $root, 'branch', '--list', $branch)).Count -ne 0) { throw "Generated branch already exists: '$branch'." }
		$identity = [pscustomobject]@{ Primary = $primary; Worktree = $worktree; Branch = $branch; TargetBranch = $primary.Branch; Baseline = $primary.Head }
		New-Item -ItemType Directory -Path $worktreeRoot -Force | Out-Null
		# The per-worktree sparse configuration below requires this repository extension, which Git
		# would otherwise write into the primary config implicitly on the first 'sparse-checkout set'.
		# Setting it here, only when it is missing, keeps that single primary write a deliberate,
		# idempotent maintenance step of the creation path.
		if ((@(& git -C $root config --get extensions.worktreeConfig) -join '').Trim() -cne 'true') {
			& git -C $root config extensions.worktreeConfig true
			if ($LASTEXITCODE -ne 0) { throw "Failed to enable extensions.worktreeConfig in '$root'." }
		}
		& git -C $root worktree add --no-checkout -b $branch $worktree $primary.Head
		if ($LASTEXITCODE -ne 0) { throw "Failed to create worktree '$worktree'." }
		$worktreeCreated = $true
		# Check out everything except the tracked Engine/Data subtrees over ~100 MB that no ordinary build
		# reads - only the Local generation build does, and it restores them itself. That
		# criterion, not these two names, is what the list expresses. The worktree is created empty so the
		# skipped bytes are never written at all. Any failure of the sparse step falls back to today's full
		# checkout, and the assertion below then holds on both paths. If the fallback itself fails, session
		# start stops with the partial worktree preserved.
		try {
			& git -C $worktree sparse-checkout set --no-cone '/*' '!/Engine/Data/Islands/' '!/Engine/Data/Textures/'
			if ($LASTEXITCODE -ne 0) { throw "'git sparse-checkout set' exited $LASTEXITCODE." }
			& git -C $worktree checkout
			if ($LASTEXITCODE -ne 0) { throw "'git checkout' exited $LASTEXITCODE." }
		}
		catch {
			Write-Warning "Sparse checkout of '$worktree' failed, so it falls back to a full checkout: $($_.Exception.Message)"
			& git -C $worktree sparse-checkout disable
			if ($LASTEXITCODE -ne 0) { throw "'git sparse-checkout disable' exited $LASTEXITCODE during the full-checkout fallback for '$worktree'." }
			& git -C $worktree checkout
			if ($LASTEXITCODE -ne 0) { throw "'git checkout' exited $LASTEXITCODE during the full-checkout fallback for '$worktree'." }
		}
		if (-not (Test-Path -LiteralPath (Join-Path $worktree 'AGENTS.md') -PathType Leaf)) { throw "Worktree '$worktree' checked out no files: '$worktree\AGENTS.md' is missing." }
	}

	# Record the branch this session lands onto; Get-AgentWorktreeSessionContext refuses to resolve a
	# session worktree without it. Rewritten on every create and reattach, so it reflects the primary
	# branch observed at the most recent wrapper start.
	$sidecarDirectory = Join-Path $identity.Worktree 'Temp'
	New-Item -ItemType Directory -Path $sidecarDirectory -Force | Out-Null
	$sidecar = [ordered]@{ schemaVersion = 'broken-engine-session-sidecar/v1'; targetBranch = $identity.TargetBranch } | ConvertTo-Json
	[IO.File]::WriteAllText((Join-Path $sidecarDirectory 'session-sidecar.json'), $sidecar, [Text.UTF8Encoding]::new($false))
	# The baseline hint Get-AgentWorktreeSessionContext reads. A file, not an environment variable, so a
	# child process repairing the fork point can rewrite it. Rewritten on every create and reattach.
	[IO.File]::WriteAllText((Join-Path $sidecarDirectory 'session-baseline'), $identity.Baseline + "`n", [Text.UTF8Encoding]::new($false))
	if ($worktreeCreated) {
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
	}
	# Each step below is separated by a blank line so its output reads as its own block in the transcript.
	& (Join-Path $root '.agents\scripts\Bootstrap-AgentTools.ps1') -RepositoryRoot $root -WaitSeconds $WaitSeconds
	Write-Host ''
	& (Join-Path $root '.agents\scripts\Provision-WorktreeThirdParty.ps1') -RepositoryRoot $identity.Worktree -WaitSeconds $WaitSeconds
	if ($Client -ceq 'opencode') { Assert-AgentWorktreeOpenCodeSkills $identity.Worktree }
	else { Assert-AgentWorktreeSkillsLink $identity.Worktree }
	if ($clearClaudeArgumentTransport) { $env:BROKEN_ENGINE_CLIENT_ARGUMENTS = $null }
	if (-not $PrepareOnly -and [string]::IsNullOrWhiteSpace($ClientExecutable)) {
		$ClientExecutable = (Get-Command $Client -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
	}
	# Skip the DataPacker and ModelViewer builds while a rebase is in progress or a restore failed after the reattach rebase:
	# a tree with conflict markers cannot build, and a wrapper exit here would block the very session that
	# must resolve the conflict from launching. The session runs the builds itself after resolving it.
	# Provisioning above is conflict-insensitive.
	if ($null -eq $firstTask) {
		Write-Host ''
		& (Join-Path $root '.agents\scripts\Build-WorktreeTarget.ps1') -Target DataPacker -Worktree $identity.Worktree -WorktreeCliExecutable $worktreeCli -PrimaryCheckout $root -Baseline $identity.Baseline
		Write-Host ''
		& (Join-Path $root '.agents\scripts\Build-WorktreeTarget.ps1') -Target ModelViewer -Worktree $identity.Worktree -WorktreeCliExecutable $worktreeCli -PrimaryCheckout $root -Baseline $identity.Baseline
	}
	Write-Host ''
	$banner = "$(if ($worktreeCreated) { 'Created' } else { 'Reattached' }) worktree $($identity.Worktree) on branch $($identity.Branch) at baseline $($identity.Baseline)."
	if ($null -ne $firstTask) {
		$buildCommand = "pwsh -NoProfile -File '.agents\scripts\Build-WorktreeTarget.ps1' -Target DataPacker -Worktree '$($identity.Worktree)' -WorktreeCliExecutable '$worktreeCli' -PrimaryCheckout '$root' -Baseline '$($identity.Baseline)'"
		$viewerBuildCommand = "pwsh -NoProfile -File '.agents\scripts\Build-WorktreeTarget.ps1' -Target ModelViewer -Worktree '$($identity.Worktree)' -WorktreeCliExecutable '$worktreeCli' -PrimaryCheckout '$root' -Baseline '$($identity.Baseline)'"
		$banner = "FIRST TASK: $firstTask Then run $buildCommand to build the deferred DataPacker, followed by $viewerBuildCommand to build the deferred ModelViewer.`n" + $banner
	}
	Write-Host $banner
	if ($PrepareOnly) { $exitCode = 0 }
	else { $exitCode = Invoke-WorktreeCliTrackedProcess -Executable $ClientExecutable -ArgumentList $ClientArguments -WorkingDirectory $identity.Worktree }
}
catch {
	[Console]::Error.WriteLine($_.Exception.Message)
	if ($worktreeCreated) { [Console]::Error.WriteLine("Preserved partial worktree '$($identity.Worktree)' and branch '$($identity.Branch)' for recovery.") }
	$exitCode = 1
}
exit $exitCode
