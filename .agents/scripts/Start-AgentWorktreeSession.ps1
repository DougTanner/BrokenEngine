[CmdletBinding()]
param(
	[Parameter(Mandatory)][string] $Client,
	[Parameter(Mandatory)][string] $RepositoryRoot,
	[string[]] $ClientArguments = @(switch -CaseSensitive ($Client) {
		'agy' { '--dangerously-skip-permissions' }
		'codex' { '--no-daemon', '--dangerously-bypass-approvals-and-sandbox', '--dangerously-bypass-hook-trust' }
	}),
	[string] $ReattachWorktree,
	[int] $WaitSeconds = 500
)

$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot 'AgentScriptCommon.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'WorktreeCliSessionExclusion.psm1') -Force -DisableNameChecking
Import-Module (Join-Path $PSScriptRoot 'AgentWorktreeSession.psm1') -Force -DisableNameChecking

$supportedClients = @(Get-AgentWorktreeSupportedClients)
if ($Client -cnotin $supportedClients) {
	$clientList = ($supportedClients | ForEach-Object { "'$_'" }) -join ', '
	throw "Client must be lowercase: $clientList."
}

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

$worktreeCreated = $false
$exitCode = 1
try {
	$primary = Get-AgentWorktreePrimaryCheckout $RepositoryRoot
	$root = $primary.Root
	$identity = $null
	$rebaseInProgress = $false

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
		if ($null -eq (Get-AgentWorktreeSessionId -Branch $branch -Client $Client)) { throw "Worktree '$reattachTarget' is not checked out on a $Client session branch: '$branch'." }
		$identity = [pscustomobject]@{ Worktree = $reattachTarget; Branch = $branch }
	}
	else {
		$primary = Get-AgentWorktreePrimaryIdentity $RepositoryRoot
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
		# Both per-worktree session config (config.worktree) and the per-worktree sparse configuration
		# below require extensions.worktreeConfig = true. Setting it here only when not already true
		# keeps that single primary write a deliberate, idempotent maintenance step of the creation path.
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

	if ($worktreeCreated) {
		& (Join-Path $PSScriptRoot 'Initialize-AgentWorktreeSession.ps1') -RepositoryRoot $root -Worktree $identity.Worktree -TargetBranch $identity.TargetBranch -Baseline $identity.Baseline -WaitSeconds $WaitSeconds
	}
	if ($clearClaudeArgumentTransport) { $env:BROKEN_ENGINE_CLIENT_ARGUMENTS = $null }
	# Each session opens a directory agy has not trusted; trust it for this session only.
	if ($Client -ceq 'agy') { $ClientArguments += @('--settings', (ConvertTo-Json -Compress @{ trustedWorkspaces = @($identity.Worktree) })) }
	$clientExecutable = (Get-Command $Client -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
	Write-Host ''
	$banner = if ($worktreeCreated) { "Created worktree $($identity.Worktree) on branch $($identity.Branch) at baseline $($identity.Baseline)." }
		else { "Reattached worktree $($identity.Worktree) on branch $($identity.Branch)." }
	if ($rebaseInProgress) { $banner += "`nThis worktree has a rebase in progress." }
	Write-Host $banner
	$exitCode = Invoke-WorktreeCliTrackedProcess -Executable $clientExecutable -ArgumentList $ClientArguments -WorkingDirectory $identity.Worktree
}
catch {
	[Console]::Error.WriteLine($_.Exception.Message)
	if ($worktreeCreated) { [Console]::Error.WriteLine("Preserved partial worktree '$($identity.Worktree)' and branch '$($identity.Branch)' for recovery.") }
	$exitCode = 1
}
exit $exitCode
