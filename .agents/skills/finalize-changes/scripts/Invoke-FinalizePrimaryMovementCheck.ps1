# Assesses primary movement after approval without changing any Git or worktree state.
[CmdletBinding()]
param(
	[string] $CurrentWorktree,
	[string] $PrimaryWorktree,
	[string] $CurrentBranch,
	[string] $PrimaryBranch,
	[string] $CandidateCommit,
	[string] $CandidateTree,
	[string] $CandidateParent,
	[string[]] $OwnedPaths
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$commonModule = Join-Path $PSScriptRoot '..\..\..\scripts\FinalizeWorkflowCommon.psm1'
if (-not (Test-Path -LiteralPath $commonModule -PathType Leaf)) {
	$commonModule = Join-Path $PSScriptRoot '..\..\..\..\.agents\scripts\FinalizeWorkflowCommon.psm1'
}
Import-Module $commonModule -Force -DisableNameChecking

$script:EvidenceLimit = 500

function New-Evidence([object[]] $Items) {
	$values = @()
	if ($null -ne $Items) { $values = @($Items) }
	$count = $values.Count
	$shown = @()
	if ($count -gt $script:EvidenceLimit) { $shown = @($values[0..($script:EvidenceLimit - 1)]) }
	else { $shown = @($values) }
	return [ordered]@{
		totalCount = $count
		items = $shown
		truncated = ($count -gt $script:EvidenceLimit)
	}
}

function New-Result {
	return [ordered]@{
		schemaVersion = 'broken-engine-finalize-primary-movement/v2'
		status = 'error'
		code = 'assessment.failed'
		message = 'Primary movement assessment did not complete.'
		candidate = [ordered]@{ commit = $null; tree = $null; parent = $null }
		tips = [ordered]@{ session = $null; livePrimary = $null; relation = 'unknown' }
		ownedPaths = New-Evidence @()
		foreignCommits = New-Evidence @()
		changes = [ordered]@{
			totalCount = 0
			items = @()
			truncated = $false
			overlapPaths = New-Evidence @()
		}
	}
}

$script:Result = New-Result

function Complete-Result([int] $ExitCode, [string] $Status, [string] $Code, [string] $Message) {
	$script:Result.status = $Status
	$script:Result.code = $Code
	$script:Result.message = $Message
	Write-Output ($script:Result | ConvertTo-Json -Depth 32 -Compress)
	exit $ExitCode
}

function Throw-Assessment([string] $Message) {
	$exception = [InvalidOperationException]::new($Message)
	$exception.Data['exit'] = 1
	$exception.Data['code'] = 'assessment.failed'
	throw $exception
}

function Throw-Input([string] $Message) {
	$exception = [InvalidOperationException]::new($Message)
	$exception.Data['exit'] = 1
	$exception.Data['code'] = 'input.invalid'
	throw $exception
}

function Throw-Blocked([string] $Code, [string] $Message) {
	$exception = [InvalidOperationException]::new($Message)
	$exception.Data['exit'] = 2
	$exception.Data['code'] = $Code
	throw $exception
}

function Assert-TextInput([string] $Value, [string] $Name) {
	if ([string]::IsNullOrWhiteSpace($Value) -or $Value.IndexOf([char]0, [StringComparison]::Ordinal) -ge 0 -or
		$Value.IndexOf("`r", [StringComparison]::Ordinal) -ge 0 -or $Value.IndexOf("`n", [StringComparison]::Ordinal) -ge 0) {
		Throw-Input "$Name is missing or contains invalid control characters."
	}
}

function Assert-ExactHashInput([string] $Value, [string] $Name) {
	Assert-TextInput $Value $Name
	if ($Value -cnotmatch '^[0-9a-f]{40}\z') { Throw-Input "$Name must be a lowercase 40-character object identity." }
}

function Get-OrdinalUnique([object[]] $Values) {
	$set = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
	$list = [Collections.Generic.List[string]]::new()
	foreach ($value in @($Values)) {
		if ($null -eq $value) { continue }
		$text = [string] $value
		if ($set.Add($text)) { $list.Add($text) }
	}
	$list.Sort([StringComparer]::Ordinal)
	return $list.ToArray()
}

function Get-OwnedPathList([string[]] $InputPaths) {
	$values = @()
	if ($null -ne $InputPaths) { $values = @($InputPaths) }
	if ($values.Count -eq 0) { Throw-Input 'OwnedPaths must contain at least one canonical repository-relative path.' }
	$set = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
	foreach ($value in $values) {
		if ($null -eq $value) { Throw-Input 'OwnedPaths contains a null path.' }
		$pieces = ([string] $value).Split(',', [StringSplitOptions]::None)
		foreach ($piece in $pieces) {
			if ([string]::IsNullOrEmpty($piece)) { Throw-Input 'OwnedPaths contains an empty comma-array element.' }
			try { Assert-FinalizeGitPath $piece }
			catch { Throw-Input "OwnedPaths contains an invalid path '$piece'." }
			[void] $set.Add($piece)
		}
	}
	if ($set.Count -eq 0) { Throw-Input 'OwnedPaths must contain at least one canonical repository-relative path.' }
	$list = [Collections.Generic.List[string]]::new()
	foreach ($path in $set) { $list.Add($path) }
	$list.Sort([StringComparer]::Ordinal)
	return $list.ToArray()
}

function Invoke-GitAssessment([string] $Root, [string[]] $Arguments, [string] $Operation) {
	try { return Invoke-FinalizeNativeText 'git.exe' (@('-C', $Root) + $Arguments) $Root }
	catch { Throw-Assessment "${Operation} could not start: $($_.Exception.Message)" }
}

function Get-ExactRef([string] $Root, [string] $Branch, [string] $Operation) {
	$response = Invoke-GitAssessment $Root @('rev-parse', '--verify', "refs/heads/$Branch`^{commit}") $Operation
	if ($response.ExitCode -ne 0) {
		Throw-Assessment "$Operation failed: $($response.Stderr.Trim())"
	}
	if ($response.Stdout -cnotmatch '^[0-9a-f]{40}\r?\n\z') {
		Throw-Assessment "$Operation returned malformed commit output."
	}
	return $response.Stdout.Trim()
}

function Get-ExactResolvedCommit([string] $Root, [string] $Commit) {
	$response = Invoke-GitAssessment $Root @('rev-parse', '--verify', "$Commit`^{commit}") 'candidate commit resolution'
	if ($response.ExitCode -ne 0) { Throw-Assessment "candidate commit resolution failed: $($response.Stderr.Trim())" }
	if ($response.Stdout -cnotmatch '^[0-9a-f]{40}\r?\n\z' -or $response.Stdout.Trim() -cne $Commit) {
		Throw-Assessment 'candidate commit resolution returned malformed or different output.'
	}
}

function Get-CandidateTree([string] $Root, [string] $Commit) {
	$response = Invoke-GitAssessment $Root @('rev-parse', '--verify', "$Commit`^{tree}") 'candidate tree resolution'
	if ($response.ExitCode -ne 0) { Throw-Assessment "candidate tree resolution failed: $($response.Stderr.Trim())" }
	if ($response.Stdout -cnotmatch '^[0-9a-f]{40}\r?\n\z') {
		Throw-Assessment 'candidate tree resolution returned malformed output.'
	}
	return $response.Stdout.Trim()
}

function Get-CandidateParent([string] $Root, [string] $Commit) {
	$response = Invoke-GitAssessment $Root @('show', '-s', '--format=%P', $Commit) 'candidate parent resolution'
	if ($response.ExitCode -ne 0) { Throw-Assessment "candidate parent resolution failed: $($response.Stderr.Trim())" }
	if ($response.Stdout -cnotmatch '^(?:[0-9a-f]{40}(?: [0-9a-f]{40})*)?\r?\n\z') {
		Throw-Assessment 'candidate parent resolution returned malformed output.'
	}
	$parents = $response.Stdout.Trim().Split(' ', [StringSplitOptions]::RemoveEmptyEntries)
	return @($parents)
}

function Get-ForeignCommits([string] $Root, [string] $Parent, [string] $LivePrimary) {
	$response = Invoke-GitAssessment $Root @('rev-list', '--reverse', "$Parent..$LivePrimary") 'foreign commit enumeration'
	if ($response.ExitCode -ne 0) { Throw-Assessment "foreign commit enumeration failed: $($response.Stderr.Trim())" }
	if ([string]::IsNullOrEmpty($response.Stdout)) { return @() }
	$lines = $response.Stdout.Split([char] 10)
	if ($lines.Count -eq 0 -or $lines[$lines.Count - 1] -cne '') { Throw-Assessment 'foreign commit enumeration returned malformed output.' }
	$seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
	$commits = [Collections.Generic.List[string]]::new()
	for ($index = 0; $index -lt $lines.Count - 1; $index++) {
		$line = $lines[$index]
		if ($line.EndsWith("`r", [StringComparison]::Ordinal)) { $line = $line.Substring(0, $line.Length - 1) }
		if ($line -cnotmatch '^[0-9a-f]{40}\z') { Throw-Assessment 'foreign commit enumeration returned malformed commit output.' }
		if ($seen.Add($line)) { $commits.Add($line) }
	}
	return $commits.ToArray()
}

function Test-NonZeroMode([string] $Mode) {
	return $Mode -in @('100644', '100755', '120000', '160000')
}

function Test-ZeroObject([string] $ObjectId) {
	return $ObjectId -match '^0+\z'
}

function Get-RawChanges([string] $Root, [string] $Parent, [string] $LivePrimary) {
	$response = Invoke-GitAssessment $Root @('diff', '--raw', '--no-renames', '-z', $Parent, $LivePrimary) 'raw change enumeration'
	if ($response.ExitCode -ne 0) { Throw-Assessment "raw change enumeration failed: $($response.Stderr.Trim())" }
	if ([string]::IsNullOrEmpty($response.Stdout)) { return @() }
	$tokens = $response.Stdout.Split([char] 0)
	if ($tokens.Count -lt 3 -or $tokens[$tokens.Count - 1] -cne '' -or (($tokens.Count - 1) % 2) -ne 0) {
		Throw-Assessment 'raw change enumeration returned malformed NUL-delimited output.'
	}
	$seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
	$rows = [Collections.Generic.List[object]]::new()
	for ($index = 0; $index -lt $tokens.Count - 1; $index += 2) {
		$metadata = $tokens[$index]
		$path = $tokens[$index + 1]
		$match = [regex]::Match($metadata, '^:(?<oldMode>[0-7]{6}) (?<newMode>[0-7]{6}) (?<oldObject>[0-9a-f]{4,40}) (?<newObject>[0-9a-f]{4,40}) (?<status>[ACDMTUXB])\z')
		if (-not $match.Success -or [string]::IsNullOrEmpty($path)) { Throw-Assessment 'raw change enumeration returned a malformed record.' }
		$oldMode = $match.Groups['oldMode'].Value
		$newMode = $match.Groups['newMode'].Value
		$oldObject = $match.Groups['oldObject'].Value
		$newObject = $match.Groups['newObject'].Value
		$status = $match.Groups['status'].Value
		if (($oldMode -notin @('000000', '100644', '100755', '120000', '160000')) -or
			($newMode -notin @('000000', '100644', '100755', '120000', '160000'))) {
			Throw-Assessment 'raw change enumeration returned an unsupported file mode.'
		}
		if ($status -ceq 'A' -and (-not ($oldMode -ceq '000000' -and (Test-ZeroObject $oldObject) -and (Test-NonZeroMode $newMode) -and -not (Test-ZeroObject $newObject)))) {
			Throw-Assessment 'raw change enumeration returned an invalid add record.'
		}
		if ($status -ceq 'D' -and (-not ((Test-NonZeroMode $oldMode) -and -not (Test-ZeroObject $oldObject) -and $newMode -ceq '000000' -and (Test-ZeroObject $newObject)))) {
			Throw-Assessment 'raw change enumeration returned an invalid delete record.'
		}
		if ($status -notin @('A', 'D') -and (-not ((Test-NonZeroMode $oldMode) -and (Test-NonZeroMode $newMode) -and -not (Test-ZeroObject $oldObject) -and -not (Test-ZeroObject $newObject)))) {
			Throw-Assessment 'raw change enumeration returned an invalid modified record.'
		}
		try { Assert-FinalizeGitPath $path }
		catch { Throw-Assessment 'raw change enumeration returned a non-canonical repository path.' }
		$key = $oldMode + [char] 0 + $newMode + [char] 0 + $status + [char] 0 + $path
		if ($seen.Add($key)) {
			$rows.Add([ordered]@{ oldMode = $oldMode; newMode = $newMode; status = $status; path = $path })
		}
	}
	$comparer = [Collections.Generic.Comparer[object]]::Create([Comparison[object]] {
		param($left, $right)
		$comparison = [StringComparer]::Ordinal.Compare([string] $left.Path, [string] $right.Path)
		if ($comparison -ne 0) { return $comparison }
		$comparison = [StringComparer]::Ordinal.Compare([string] $left.Status, [string] $right.Status)
		if ($comparison -ne 0) { return $comparison }
		$comparison = [StringComparer]::Ordinal.Compare([string] $left.OldMode, [string] $right.OldMode)
		if ($comparison -ne 0) { return $comparison }
		return [StringComparer]::Ordinal.Compare([string] $left.NewMode, [string] $right.NewMode)
	})
	$rows.Sort($comparer)
	return $rows.ToArray()
}

function Set-Evidence([object[]] $Foreign, [object[]] $Rows, [string[]] $Owned) {
	$rowValues = @()
	if ($null -ne $Rows) { $rowValues = @($Rows) }
	$ownedSet = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
	foreach ($path in $Owned) { [void] $ownedSet.Add($path) }
	$overlap = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
	foreach ($row in $rowValues) {
		if ($ownedSet.Contains($row.Path)) { [void] $overlap.Add($row.Path) }
	}
	$overlapPaths = @(Get-OrdinalUnique @($overlap))
	$script:Result.foreignCommits = New-Evidence @($Foreign)
	$script:Result.changes = [ordered]@{
		totalCount = $rowValues.Count
		items = (New-Evidence $rowValues).items
		truncated = ($rowValues.Count -gt $script:EvidenceLimit)
		overlapPaths = New-Evidence $overlapPaths
	}
	return [pscustomobject]@{
		OverlapPaths = $overlapPaths
		Truncated = ($script:Result.ownedPaths.truncated -or $script:Result.foreignCommits.truncated -or $script:Result.changes.truncated -or
			$script:Result.changes.overlapPaths.truncated)
	}
}

try {
	Assert-TextInput $CurrentWorktree 'CurrentWorktree'
	Assert-TextInput $PrimaryWorktree 'PrimaryWorktree'
	Assert-TextInput $CurrentBranch 'CurrentBranch'
	Assert-TextInput $PrimaryBranch 'PrimaryBranch'

	$session = $null
	$primary = $null
	try {
		$session = Get-FinalizeGitIdentity $CurrentWorktree 'Session worktree'
		$primary = Get-FinalizeGitIdentity $PrimaryWorktree 'Primary worktree'
	}
	catch { Throw-Input "Worktree identity validation failed: $($_.Exception.Message)" }
	if ($session.Worktree.Equals($primary.Worktree, [StringComparison]::OrdinalIgnoreCase)) { Throw-Input 'Session and primary worktrees must be distinct.' }
	if (-not $session.CommonDirectory.Equals($primary.CommonDirectory, [StringComparison]::OrdinalIgnoreCase)) { Throw-Input 'Session and primary worktrees must share one common Git repository.' }
	if ($session.Branch -cne $CurrentBranch -or $primary.Branch -cne $PrimaryBranch) { Throw-Input 'Checked-out branch identity does not match the supplied branch.' }

	$owned = Get-OwnedPathList $OwnedPaths
	$script:Result.ownedPaths = New-Evidence $owned

	Assert-ExactHashInput $CandidateCommit 'CandidateCommit'
	Assert-ExactHashInput $CandidateTree 'CandidateTree'
	Assert-ExactHashInput $CandidateParent 'CandidateParent'

	$sessionTip = Get-ExactRef $session.Worktree $CurrentBranch 'session ref resolution'
	$livePrimary = Get-ExactRef $primary.Worktree $PrimaryBranch 'primary ref resolution'
	$script:Result.tips.session = $sessionTip
	$script:Result.tips.livePrimary = $livePrimary
	$script:Result.candidate = [ordered]@{ commit = $CandidateCommit; tree = $CandidateTree; parent = $CandidateParent }
	Get-ExactResolvedCommit $session.Worktree $CandidateCommit
	if ($sessionTip -cne $CandidateCommit) { Throw-Blocked 'candidate.session-tip-changed' "Session tip '$sessionTip' differs from candidate commit '$CandidateCommit'." }

	$actualTree = Get-CandidateTree $session.Worktree $CandidateCommit
	if ($actualTree -cne $CandidateTree) { Throw-Blocked 'candidate.tree-mismatch' "Candidate tree '$CandidateTree' differs from the immutable candidate tree '$actualTree'." }
	$parents = @(Get-CandidateParent $session.Worktree $CandidateCommit)
	if (@($parents).Count -ne 1 -or $parents[0] -cne $CandidateParent) { Throw-Blocked 'candidate.parent-mismatch' 'Candidate commit does not have the supplied sole parent.' }

	$script:Result.ownedPaths = New-Evidence $owned
	if ($script:Result.ownedPaths.truncated) { Throw-Blocked 'primary.evidence-truncated' 'Owned path evidence exceeds the 500-item cap.' }
	if ($livePrimary -ceq $CandidateParent) {
		$script:Result.tips.relation = 'equal'
		Complete-Result 0 'pass' 'ok' 'Primary remains at the candidate parent.'
	}

	$ancestry = Invoke-GitAssessment $primary.Worktree @('merge-base', '--is-ancestor', $CandidateParent, $livePrimary) 'primary ancestry assessment'
	if ($ancestry.Stdout.Length -ne 0) { Throw-Assessment 'primary ancestry assessment returned unexpected output.' }
	if ($ancestry.ExitCode -eq 1) {
		$script:Result.tips.relation = 'not-descendant'
		Throw-Blocked 'primary.not-descendant' "Live primary '$livePrimary' is not a descendant of candidate parent '$CandidateParent'."
	}
	if ($ancestry.ExitCode -ne 0) { Throw-Assessment "primary ancestry assessment failed with exit $($ancestry.ExitCode): $($ancestry.Stderr.Trim())" }
	$script:Result.tips.relation = 'descendant'

	$foreign = Get-ForeignCommits $primary.Worktree $CandidateParent $livePrimary
	$rows = Get-RawChanges $primary.Worktree $CandidateParent $livePrimary
	$partition = Set-Evidence $foreign $rows $owned
	if ($partition.Truncated) {
		Throw-Blocked 'primary.evidence-truncated' 'Primary movement evidence exceeds the 500-item cap.'
	}
	if ($partition.OverlapPaths.Count -gt 0) {
		Throw-Blocked 'primary.path-overlap' 'Foreign primary movement overlaps an owned path.'
	}
	if (@($rows).Count -eq 0) {
		Complete-Result 0 'pass' 'primary.tree-identical' 'Primary moved by a descendant range with no net tree paths.'
	}
	Complete-Result 0 'needs-review' 'primary.disjoint-needs-review' 'Primary movement changes paths disjoint from the owned paths.'
}
catch {
	$exitCode = if ($_.Exception.Data.Contains('exit')) { [int] $_.Exception.Data['exit'] } else { 1 }
	$code = if ($_.Exception.Data.Contains('code')) { [string] $_.Exception.Data['code'] } else { 'assessment.failed' }
	$status = if ($exitCode -eq 2) { 'blocked' } else { 'error' }
	Complete-Result $exitCode $status $code $_.Exception.Message
}
