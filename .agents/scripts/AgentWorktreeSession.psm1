Set-StrictMode -Version Latest

Import-Module (Join-Path $PSScriptRoot 'AgentScriptCommon.psm1')

$script:SupportedSessionClients = @('claude', 'codex', 'agy')
$script:SessionBranchPattern = "^($($script:SupportedSessionClients -join '|'))/([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})$"
$script:TargetBranchConfigKey = 'brokenengine.session.targetBranch'
$script:BaselineConfigKey = 'brokenengine.session.baseline'

function Get-AgentWorktreeSupportedClients {
	return $script:SupportedSessionClients
}

function Get-AgentWorktreeSessionId([string] $Branch, [string] $Client) {
	if ($Branch -cmatch $script:SessionBranchPattern -and ([string]::IsNullOrWhiteSpace($Client) -or $Matches[1] -ceq $Client)) {
		return $Matches[2]
	}
	return $null
}

function Get-AgentWorktreeGitValue([string] $Worktree, [string[]] $Arguments, [string] $Description) {
	$value = @(Invoke-AgentGit (@('-C', $Worktree) + $Arguments))
	if ($value.Count -ne 1 -or [string]::IsNullOrWhiteSpace($value[0])) {
		throw "Git returned no unique $Description for '$Worktree'."
	}
	return $value[0].Trim()
}

function Assert-AgentWorktreeConfigEnabled([string] $Worktree) {
	$value = & git -C $Worktree config --local --type=bool --get extensions.worktreeConfig 2>$null
	if ($LASTEXITCODE -ne 0 -or $value -cne 'true') {
		$quotedWorktree = [Management.Automation.Language.CodeGeneration]::EscapeSingleQuotedStringContent($Worktree)
		throw "Git worktree config is not enabled for '$Worktree' (extensions.worktreeConfig must be true). Enable it with: git -C '$quotedWorktree' config --local --replace-all extensions.worktreeConfig true"
	}
}

function Set-AgentWorktreeSessionConfig {
	[CmdletBinding()]
	param(
		[Parameter(Mandatory)][string] $Worktree,
		[Parameter(Mandatory)][string] $Baseline,
		[string] $TargetBranch
	)
	if ([string]::IsNullOrWhiteSpace($Worktree)) { throw 'Worktree path must not be blank.' }
	Assert-AgentWorktreeConfigEnabled $Worktree
	if ([string]::IsNullOrWhiteSpace($Baseline)) { throw "Session baseline for '$Worktree' must not be blank." }
	if ($PSBoundParameters.ContainsKey('TargetBranch')) {
		if ([string]::IsNullOrWhiteSpace($TargetBranch)) { throw "Session target branch for '$Worktree' must not be blank." }
		Invoke-AgentGit @('-C', $Worktree, 'config', '--worktree', '--replace-all', $script:TargetBranchConfigKey, $TargetBranch.Trim()) | Out-Null
	}
	Invoke-AgentGit @('-C', $Worktree, 'config', '--worktree', '--replace-all', $script:BaselineConfigKey, $Baseline.Trim()) | Out-Null
}

function Get-AgentWorktreeRecords([string] $RepositoryRoot) {
	$records = [Collections.Generic.List[object]]::new()
	$current = $null
	$output = Get-AgentWorktreeGitValue $RepositoryRoot @('worktree', 'list', '--porcelain', '-z') 'worktree registration list'
	foreach ($field in $output.Split([char]0, [StringSplitOptions]::RemoveEmptyEntries)) {
		if ($field.StartsWith('worktree ', [StringComparison]::Ordinal)) {
			if ($null -ne $current) { $records.Add([pscustomobject] $current) }
			$current = [ordered]@{ Path = Get-AgentCanonicalPath $field.Substring(9); Head = $null; Branch = $null; Prunable = $false; Bare = $false }
		}
		elseif ($null -ne $current -and $field.StartsWith('HEAD ', [StringComparison]::Ordinal)) { $current.Head = $field.Substring(5) }
		elseif ($null -ne $current -and $field.StartsWith('branch refs/heads/', [StringComparison]::Ordinal)) { $current.Branch = $field.Substring(18) }
		elseif ($null -ne $current -and $field.StartsWith('prunable', [StringComparison]::Ordinal)) { $current.Prunable = $true }
		elseif ($null -ne $current -and $field -ceq 'bare') { $current.Bare = $true }
	}
	if ($null -ne $current) { $records.Add([pscustomobject] $current) }
	return $records.ToArray()
}

function Test-AgentWorktreeNoGitOperation([string] $Worktree) {
	foreach ($marker in @('MERGE_HEAD', 'CHERRY_PICK_HEAD', 'REVERT_HEAD', 'BISECT_LOG', 'rebase-merge', 'rebase-apply', 'sequencer')) {
		$path = Get-AgentWorktreeGitValue $Worktree @('rev-parse', '--path-format=absolute', '--git-path', $marker) "Git operation marker '$marker'"
		if (Test-Path -LiteralPath $path) { throw "Git operation marker '$marker' is active in '$Worktree'." }
	}
}

function Test-AgentWorktreeAncestor([string] $RepositoryRoot, [string] $Ancestor, [string] $Descendant, [string] $Description) {
	& git -C $RepositoryRoot merge-base --is-ancestor $Ancestor $Descendant
	if ($LASTEXITCODE -ne 0) { throw "Commit '$Ancestor' is not an ancestor of $Description '$Descendant'." }
}

function Get-AgentWorktreePrimaryCheckout([string] $RepositoryRoot) {
	$root = Get-AgentCanonicalPath $RepositoryRoot
	$top = Get-AgentWorktreeGitValue $root @('rev-parse', '--show-toplevel') 'repository top-level'
	$top = Get-AgentCanonicalPath $top
	if (-not $root.Equals($top, [StringComparison]::OrdinalIgnoreCase)) { throw "RepositoryRoot is not repository root: '$root'." }
	$git = Get-Item -LiteralPath (Join-Path $root '.git') -Force -ErrorAction Stop
	if (-not $git.PSIsContainer -or ($git.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "RepositoryRoot is not an ordinary primary checkout: '$root'." }
	$common = Get-AgentWorktreeGitValue $root @('rev-parse', '--path-format=absolute', '--git-common-dir') 'Git common directory'
	$common = Get-AgentCanonicalPath $common
	if (-not $common.Equals((Get-AgentCanonicalPath $git.FullName), [StringComparison]::OrdinalIgnoreCase)) { throw "RepositoryRoot does not own its Git common directory: '$root'." }
	return [pscustomobject]@{ Root = $root; CommonDirectory = $common }
}

function Get-AgentWorktreePrimaryIdentity([string] $RepositoryRoot) {
	$primary = Get-AgentWorktreePrimaryCheckout $RepositoryRoot
	$root = $primary.Root
	$branch = Get-AgentWorktreeGitValue $root @('branch', '--show-current') 'primary branch'
	$head = Get-AgentWorktreeGitValue $root @('rev-parse', 'HEAD') 'primary HEAD'
	if ($head -cnotmatch '^[0-9a-f]{40}$') { throw "Primary HEAD is malformed: '$head'." }
	Test-AgentWorktreeNoGitOperation $root
	return [pscustomobject]@{ Root = $root; CommonDirectory = $primary.CommonDirectory; Branch = $branch; Head = $head }
}

# Session identity is entirely Git-derived: the branch names the session, the Git common directory
# names the primary checkout, and the baseline is the attribution point the session diverged from.
# A checkout not on a claude/, codex/, or agy/ session branch skips the worktree-config check and
# reads, resolving with SessionId $null instead of failing.
# A session worktree's primary branch is the one recorded in its per-worktree Git config
# (config.worktree), not the branch the primary checkout currently has checked out.
function Get-AgentWorktreeSessionContext {
	[CmdletBinding()] param([string] $Worktree)
	if ([string]::IsNullOrWhiteSpace($Worktree)) { $Worktree = (Get-Location).Path }
	try { $top = Get-AgentCanonicalPath (Get-AgentWorktreeGitValue $Worktree @('rev-parse', '--show-toplevel') 'repository top-level') }
	catch { throw "'$Worktree' is not inside a Git worktree: $($_.Exception.Message)" }
	$branch = Get-AgentWorktreeGitValue $top @('branch', '--show-current') 'current branch'
	$sessionId = Get-AgentWorktreeSessionId -Branch $branch
	$common = Get-AgentCanonicalPath (Get-AgentWorktreeGitValue $top @('rev-parse', '--path-format=absolute', '--git-common-dir') 'Git common directory')
	$primaryRoot = Get-AgentCanonicalPath (Split-Path -Parent $common)
	$primaryBranch = if ($null -ne $sessionId) {
		# The recorded landing target lives in Git's per-worktree config (config.worktree), outside
		# disposable Temp/. Live primary-branch lookup is deliberately not a fallback: the session lands
		# onto its recorded parent, not whatever the primary checkout has checked out right now.
		Assert-AgentWorktreeConfigEnabled $top
		try { Get-AgentWorktreeGitValue $top @('config', '--worktree', '--get-all', $script:TargetBranchConfigKey) "worktree config '$script:TargetBranchConfigKey'" }
		catch {
			$quotedTop = [Management.Automation.Language.CodeGeneration]::EscapeSingleQuotedStringContent($top)
			throw "$($_.Exception.Message) Restore '$script:TargetBranchConfigKey' with: git -C '$quotedTop' config --worktree --replace-all $script:TargetBranchConfigKey <branch>"
		}
	}
	else { Get-AgentWorktreeGitValue $primaryRoot @('branch', '--show-current') 'primary branch' }
	$primaryTip = Get-AgentWorktreeGitValue $primaryRoot @('rev-parse', "refs/heads/$primaryBranch") 'primary tip commit'
	# Rewriting the primary branch (a daily history squash) leaves the recorded baseline resolvable but
	# orphaned, and plain merge-base then resolves to before the rewritten commits, so the session diff
	# would silently include work already on primary. Accept the recorded baseline only while it is still
	# on the primary tip's history, and otherwise recover the true fork point from the primary branch's
	# reflog. --fork-point can still answer with an older ancestor once the pre-rewrite tip has expired
	# from that reflog, so the plain merge-base stays as the last resort rather than failing closed.
	# Rebasing the session onto an advanced primary is the second way the recorded baseline goes
	# stale: it stays on the primary tip's history, so it is still accepted, but it now sits
	# before commits the rebase already put under the session, and the diff would claim them.
	# The same holds once the primary tip reaches or passes the session HEAD, as after this
	# session's own landing, which then resolves the baseline to the landed commit and leaves
	# only the open stage in the diff. Substituting the merge base directly instead of falling
	# back to --fork-point keeps the answer deterministic and reflog-independent.
	$baseline = $null
	# The wrapper records the baseline hint in Git's per-worktree config (config.worktree); a missing,
	# empty, or non-unique entry is simply no hint. Fork-point repair updates it in place.
	$configured = $null
	if ($null -ne $sessionId) {
		$configuredValues = @(& git -C $top config --worktree --get-all $script:BaselineConfigKey 2>$null)
		if ($LASTEXITCODE -eq 0 -and $configuredValues.Count -eq 1) { $configured = $configuredValues[0].Trim() }
	}
	if (-not [string]::IsNullOrWhiteSpace($configured)) {
		$resolved = @(& git -C $top rev-parse --quiet --verify "$configured^{commit}" 2>$null)
		if ($LASTEXITCODE -eq 0 -and $resolved.Count -eq 1) {
			$candidate = $resolved[0].Trim()
			& git -C $top merge-base --is-ancestor $candidate $primaryTip
			if ($LASTEXITCODE -eq 0) {
				$baseline = $candidate
				$divergence = @(& git -C $top merge-base HEAD $primaryTip 2>$null)
				if ($LASTEXITCODE -eq 0 -and $divergence.Count -eq 1) {
					$divergencePoint = $divergence[0].Trim()
					if ($divergencePoint -cne $candidate) {
						& git -C $top merge-base --is-ancestor $candidate $divergencePoint
						if ($LASTEXITCODE -eq 0) { $baseline = $divergencePoint }
					}
				}
			}
		}
	}
	if ([string]::IsNullOrWhiteSpace($baseline)) {
		$forkPoint = @(& git -C $top merge-base --fork-point "refs/heads/$primaryBranch" HEAD 2>$null)
		if ($LASTEXITCODE -eq 0 -and $forkPoint.Count -eq 1) { $baseline = $forkPoint[0].Trim() }
	}
	if ([string]::IsNullOrWhiteSpace($baseline)) { $baseline = Get-AgentWorktreeGitValue $top @('merge-base', 'HEAD', $primaryTip) 'merge base with the primary tip' }
	return [pscustomobject]@{
		Worktree = $top; Branch = $branch; SessionId = $sessionId
		PrimaryRoot = $primaryRoot; PrimaryBranch = $primaryBranch; PrimaryTip = $primaryTip; Baseline = $baseline
	}
}

# Explicit fork-point repair: 'ancestor' means the resolved fork point is still on the primary
# tip's history, which the repair script treats as a blocker.
# --fork-point can succeed with no output, so the plain merge-base fallback runs before the guard and
# --is-ancestor never receives an unset value. Local work is saved before replay and restored with
# its index afterward; the uniquely named stash remains available even after successful recovery.
function Repair-AgentWorktreeForkPoint([string] $Worktree, [string] $PrimaryBranch, [string] $PrimaryTip, [string] $Branch) {
	$forkPoint = @(& git -C $Worktree merge-base --fork-point "refs/heads/$PrimaryBranch" HEAD 2>$null)
	$baseline = if ($LASTEXITCODE -eq 0 -and $forkPoint.Count -eq 1 -and -not [string]::IsNullOrWhiteSpace($forkPoint[0])) { $forkPoint[0].Trim() }
		else { @(Invoke-AgentGit @('-C', $Worktree, 'merge-base', 'HEAD', $PrimaryTip))[0].Trim() }
	& git -C $Worktree merge-base --is-ancestor $baseline $PrimaryTip
	if ($LASTEXITCODE -eq 0) { return [pscustomobject]@{ Outcome = 'ancestor'; ForkPoint = $baseline; Baseline = $baseline } }
	$repair = "git -C '$Worktree' rebase --onto $PrimaryTip $baseline $Branch"
	$refusal = "Primary branch '$PrimaryBranch' was rewritten: fork point $baseline is no longer on its history, which is now at $PrimaryTip. Replay this session onto the new tip manually: $repair"
	Test-AgentWorktreeNoGitOperation $Worktree
	$snapshot = $null
	$recovery = ''
	if (@(Invoke-AgentGit @('-C', $Worktree, 'status', '--porcelain', '-z', '--untracked-files=all')).Count -ne 0) {
		$tag = "fork-point-recovery-$([guid]::NewGuid()) worktree '$Worktree'"
		$recovery = "Recovery stash tag: $tag. No recovery stash is automatically dropped."
		try {
			$saveOutput = @(& git -C $Worktree stash push --include-untracked -m $tag 2>&1)
			$saveExit = $LASTEXITCODE
			# refs/stash is shared by linked worktrees and may have moved since push; find our UUID instead.
			$entries = @(Invoke-AgentGit @('-C', $Worktree, 'stash', 'list', '--format=%H%x09%gs') | Where-Object { $_.EndsWith($tag, [StringComparison]::Ordinal) })
			if ($entries.Count -eq 1) {
				$snapshot = $entries[0].Split("`t")[0]
				$recovery = "Retained recovery stash $snapshot (tag: $tag). After resolving any partial operation or restoration, recover manually with: git -C '$Worktree' stash apply --index $snapshot."
			}
			if ($saveExit -ne 0 -or $null -eq $snapshot) { throw "Stash save did not complete with an identified snapshot: $($saveOutput -join '; ')" }
			if (@(Invoke-AgentGit @('-C', $Worktree, 'status', '--porcelain', '-z', '--untracked-files=all')).Count -ne 0) { throw 'Stash save left local changes in the worktree.' }
		}
		catch { throw "Could not preserve local work in '$Worktree'; no rebase was started. $($_.Exception.Message) $recovery" }
	}
	$rebaseOutput = @(& git -C $Worktree rebase --onto $PrimaryTip $baseline $Branch 2>&1)
	if ($LASTEXITCODE -ne 0) {
		$aborted = $false
		foreach ($marker in @('rebase-merge', 'rebase-apply')) {
			$markerPath = Get-AgentWorktreeGitValue $Worktree @('rev-parse', '--path-format=absolute', '--git-path', $marker) "Git operation marker '$marker'"
			if (-not (Test-Path -LiteralPath $markerPath)) { continue }
			$abortOutput = @(& git -C $Worktree rebase --abort 2>&1)
			if ($LASTEXITCODE -ne 0) { throw "Replaying this session failed, and abort failed, so '$Worktree' is left mid-rebase. No saved local work was applied. Roll it back manually: git -C '$Worktree' rebase --abort. $($abortOutput -join '; ') $recovery $refusal" }
			$aborted = $true
			break
		}
		if ($null -ne $snapshot) {
			$restoreOutput = @(& git -C $Worktree stash apply --index $snapshot 2>&1)
			if ($LASTEXITCODE -ne 0) { throw "Replaying this session failed (aborted: $aborted), and restoring saved local work also failed in '$Worktree'; restoration may be partial. $($restoreOutput -join '; ') $recovery" }
		}
		throw "Replaying this session failed (aborted: $aborted); saved local work was restored when present. $($rebaseOutput -join '; ') $recovery $refusal"
	}
	if ($null -ne $snapshot) {
		$restoreOutput = @(& git -C $Worktree stash apply --index $snapshot 2>&1)
		if ($LASTEXITCODE -ne 0) {
			$failure = [Exception]::new("The rebase completed onto $PrimaryTip, but restoring saved local work failed in '$Worktree'; the rebased branch and partial restoration were preserved. $($restoreOutput -join '; ') $recovery")
			$failure.Data['Rebased'] = $true
			throw $failure
		}
		[Console]::Error.WriteLine("Restored local work in '$Worktree'. $recovery")
	}
	return [pscustomobject]@{ Outcome = 'rebased'; ForkPoint = $baseline; Baseline = $PrimaryTip }
}

Export-ModuleMember -Function Get-AgentWorktreeSupportedClients, Get-AgentWorktreeSessionId, Get-AgentWorktreePrimaryCheckout, Get-AgentWorktreePrimaryIdentity, Get-AgentWorktreeRecords, Test-AgentWorktreeNoGitOperation, Test-AgentWorktreeAncestor, Set-AgentWorktreeSessionConfig, Get-AgentWorktreeSessionContext, Repair-AgentWorktreeForkPoint
