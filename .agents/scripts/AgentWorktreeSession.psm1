Set-StrictMode -Version Latest

Import-Module (Join-Path $PSScriptRoot 'AgentScriptCommon.psm1')

function Get-AgentWorktreeGitValue([string] $Worktree, [string[]] $Arguments, [string] $Description) {
	$value = @(Invoke-AgentGit (@('-C', $Worktree) + $Arguments))
	if ($value.Count -ne 1 -or [string]::IsNullOrWhiteSpace($value[0])) {
		throw "Git returned no unique $Description for '$Worktree'."
	}
	return $value[0].Trim()
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

function Get-AgentWorktreePrimaryIdentity([string] $RepositoryRoot) {
	$root = Get-AgentCanonicalPath $RepositoryRoot
	$top = Get-AgentWorktreeGitValue $root @('rev-parse', '--show-toplevel') 'repository top-level'
	$top = Get-AgentCanonicalPath $top
	if (-not $root.Equals($top, [StringComparison]::OrdinalIgnoreCase)) { throw "RepositoryRoot is not repository root: '$root'." }
	$git = Get-Item -LiteralPath (Join-Path $root '.git') -Force -ErrorAction Stop
	if (-not $git.PSIsContainer -or ($git.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "RepositoryRoot is not an ordinary primary checkout: '$root'." }
	$common = Get-AgentWorktreeGitValue $root @('rev-parse', '--path-format=absolute', '--git-common-dir') 'Git common directory'
	$common = Get-AgentCanonicalPath $common
	if (-not $common.Equals((Get-AgentCanonicalPath $git.FullName), [StringComparison]::OrdinalIgnoreCase)) { throw "RepositoryRoot does not own its Git common directory: '$root'." }
	$branch = Get-AgentWorktreeGitValue $root @('branch', '--show-current') 'primary branch'
	$head = Get-AgentWorktreeGitValue $root @('rev-parse', 'HEAD') 'primary HEAD'
	if ($head -cnotmatch '^[0-9a-f]{40}$') { throw "Primary HEAD is malformed: '$head'." }
	Test-AgentWorktreeNoGitOperation $root
	return [pscustomobject]@{ Root = $root; CommonDirectory = $common; Branch = $branch; Head = $head }
}

# Session identity is entirely Git-derived: the branch names the session, the Git common directory
# names the primary checkout, and the baseline is the attribution point the session diverged from.
# A checkout not on a claude/ or codex/ session branch has no session sidecar, so it resolves with
# SessionId $null instead of failing.
# A session worktree's primary branch is the one its wrapper recorded in the session sidecar, not the
# branch the primary checkout currently has checked out.
function Get-AgentWorktreeSessionContext {
	[CmdletBinding()] param([string] $Worktree)
	if ([string]::IsNullOrWhiteSpace($Worktree)) { $Worktree = (Get-Location).Path }
	try { $top = Get-AgentCanonicalPath (Get-AgentWorktreeGitValue $Worktree @('rev-parse', '--show-toplevel') 'repository top-level') }
	catch { throw "'$Worktree' is not inside a Git worktree: $($_.Exception.Message)" }
	$branch = Get-AgentWorktreeGitValue $top @('branch', '--show-current') 'current branch'
	$sessionId = if ($branch -cmatch '^(?:claude|codex)/([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})$') { $Matches[1] } else { $null }
	$common = Get-AgentCanonicalPath (Get-AgentWorktreeGitValue $top @('rev-parse', '--path-format=absolute', '--git-common-dir') 'Git common directory')
	$primaryRoot = Get-AgentCanonicalPath (Split-Path -Parent $common)
	$primaryBranch = if ($null -ne $sessionId) {
		# The sidecar is the session's recorded landing target; the file is opaque to this code unit, so
		# validate it strictly. Live primary-branch lookup is deliberately not a fallback: the session
		# lands onto its recorded parent, not whatever the primary checkout has checked out right now.
		$sidecarPath = Join-Path $top 'Temp\session-sidecar.json'
		if (-not (Test-Path -LiteralPath $sidecarPath -PathType Leaf)) {
			throw "Session sidecar '$sidecarPath' is missing. Start or reattach this worktree through '.claude/claude-worktree.sh' or '.codex/codex-worktree.ps1', which write it."
		}
		$sidecar = $null
		try { $sidecar = [IO.File]::ReadAllText($sidecarPath) | ConvertFrom-Json -Depth 4 -ErrorAction Stop }
		catch { throw "Session sidecar '$sidecarPath' is invalid: $($_.Exception.Message). Recreate it by reattaching through '.claude/claude-worktree.sh' or '.codex/codex-worktree.ps1'." }
		# Every post-parse failure funnels into one diagnostic: reject a null or non-object parse result
		# before touching properties, and compare property names case-sensitively so a wrong-case field
		# never passes. The name check runs before any property access, so StrictMode cannot throw a raw
		# property error on unrelated fields.
		$valid = $sidecar -is [Management.Automation.PSCustomObject]
		if ($valid) {
			$names = @($sidecar.PSObject.Properties.Name)
			$valid = $names.Count -eq 2 -and $names -ccontains 'schemaVersion' -and $names -ccontains 'targetBranch' -and
				$sidecar.schemaVersion -ceq 'broken-engine-session-sidecar/v1' -and
				$sidecar.targetBranch -is [string] -and -not [string]::IsNullOrWhiteSpace($sidecar.targetBranch)
		}
		if (-not $valid) {
			throw "Session sidecar '$sidecarPath' is invalid: it must contain one JSON object with exactly schemaVersion 'broken-engine-session-sidecar/v1' and a non-empty string targetBranch. Recreate it by reattaching through '.claude/claude-worktree.sh' or '.codex/codex-worktree.ps1'."
		}
		$sidecar.targetBranch
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
	# The wrapper records the baseline in this gitignored file; a missing or empty file is simply no
	# hint. A child process can rewrite the file, which is why it replaces the former environment hint.
	$baselineHintPath = Join-Path $top 'Temp\session-baseline'
	$configured = if (Test-Path -LiteralPath $baselineHintPath -PathType Leaf) { [IO.File]::ReadAllText($baselineHintPath).Trim() } else { $null }
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

# One copy of a rare, risky Git sequence, shared by the wrapper's reattach path and the mid-session
# repair script. It is outcome-neutral: 'ancestor' means the resolved fork point is still on the
# primary tip's history, which the wrapper treats as success and the repair script as a blocker.
# --fork-point can succeed with no output, so the plain merge-base fallback runs before the guard and
# --is-ancestor never receives an unset value. A refusal beats a partial repair, so a dirty tree is
# rejected outright and a failed replay is aborted back to what was found.
function Repair-AgentWorktreeForkPoint([string] $Worktree, [string] $PrimaryBranch, [string] $PrimaryTip, [string] $Branch) {
	$forkPoint = @(& git -C $Worktree merge-base --fork-point "refs/heads/$PrimaryBranch" HEAD 2>$null)
	$baseline = if ($LASTEXITCODE -eq 0 -and $forkPoint.Count -eq 1 -and -not [string]::IsNullOrWhiteSpace($forkPoint[0])) { $forkPoint[0].Trim() }
		else { @(Invoke-AgentGit @('-C', $Worktree, 'merge-base', 'HEAD', $PrimaryTip))[0].Trim() }
	& git -C $Worktree merge-base --is-ancestor $baseline $PrimaryTip
	if ($LASTEXITCODE -eq 0) { return [pscustomobject]@{ Outcome = 'ancestor'; ForkPoint = $baseline; Baseline = $baseline } }
	$repair = "git -C '$Worktree' rebase --onto $PrimaryTip $baseline $Branch"
	$refusal = "Primary branch '$PrimaryBranch' was rewritten: fork point $baseline is no longer on its history, which is now at $PrimaryTip. Replay this session onto the new tip manually: $repair"
	if (@(Invoke-AgentGit @('-C', $Worktree, 'status', '--porcelain', '-z', '--untracked-files=all')).Count -ne 0) {
		throw "Worktree '$Worktree' has staged, unstaged, or untracked changes, so it was left untouched. $refusal"
	}
	& git -C $Worktree rebase --onto $PrimaryTip $baseline $Branch
	if ($LASTEXITCODE -ne 0) {
		& git -C $Worktree rebase --abort
		if ($LASTEXITCODE -ne 0) { throw "Replaying this session onto the rewritten primary branch failed, and rolling that replay back failed too, so the worktree is left mid-rebase. Roll it back manually: git -C '$Worktree' rebase --abort. $refusal" }
		throw "Replaying this session onto the rewritten primary branch failed and was aborted, so the worktree was left untouched. $refusal"
	}
	return [pscustomobject]@{ Outcome = 'rebased'; ForkPoint = $baseline; Baseline = $PrimaryTip }
}

Export-ModuleMember -Function Get-AgentWorktreePrimaryIdentity, Get-AgentWorktreeRecords, Test-AgentWorktreeNoGitOperation, Test-AgentWorktreeAncestor, Get-AgentWorktreeSessionContext, Repair-AgentWorktreeForkPoint
