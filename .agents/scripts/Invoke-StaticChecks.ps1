# The Step-4 static checks an implementer runs itself, selected from the session change inventory and
# run in one pass: `validate-skill` for each changed SKILL.md, `plan-scheduler` for a changed Plan, and
# `markdown-links` for every changed markdown file. The first two compose the existing bundled scripts
# and carry their results through unaltered; only the markdown link and anchor check is new here. The
# run reports results only — it never decides whether a failing check blocks a slice, never edits a
# file, and writes nothing to disk (GIT_OPTIONAL_LOCKS=0 keeps Git from refreshing the index), so it is
# safe under a read-only sandbox. Stdout carries only the result document.
[CmdletBinding()]
param(
	[Parameter(Mandatory)][string] $RepositoryRoot,
	[Parameter(Mandatory)][string] $Baseline,
	[string] $Head,
	[switch] $IncludeUntracked
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'AgentScriptCommon.psm1') -Force

$script:MaximumMessageLength = 256

$script:InventoryScript = Join-Path $PSScriptRoot 'Get-SessionChangeInventory.ps1'
$script:SchedulerScript = Join-Path $PSScriptRoot 'Test-PlanSchedulerState.ps1'
$script:ValidateSkillScript = Join-Path $PSScriptRoot '../skills/validate-skill/scripts/Validate-Skill.ps1'
$script:Utf8 = [Text.UTF8Encoding]::new($false)
$script:Root = $null
$script:HeadSha = ''
$script:HeadPaths = $null
$script:HeadingSlugs = @{}
# Inline markdown link: the target is everything up to the closing parenthesis or the optional title.
$script:LinkPattern = '\[(?:[^\[\]]*)\]\(\s*([^)\s]+)'
$script:SchemePattern = '^[A-Za-z][A-Za-z0-9+.-]*:'

$result = [ordered]@{
	schemaVersion = 'broken-engine-static-checks/v1'
	status = 'error'
	code = 'internal.error'
	message = 'Static checks did not run.'
	truncated = $false
	checks = @()
}

function Complete-StaticChecks([int] $ExitCode, [string] $Status, [string] $Code, [string] $Message) {
	$result.status = $Status
	$result.code = $Code
	$result.message = if ($Message.Length -gt $script:MaximumMessageLength) { $Message.Substring(0, $script:MaximumMessageLength) } else { $Message }
	$stream = [Console]::OpenStandardOutput()
	$bytes = $script:Utf8.GetBytes(($result | ConvertTo-Json -Depth 32 -Compress))
	$stream.Write($bytes, 0, $bytes.Length)
	$stream.Flush()
	exit $ExitCode
}

function Invoke-StaticCheckProcess([string] $FileName, [string[]] $Arguments, [string] $WorkingDirectory) {
	$start = [Diagnostics.ProcessStartInfo]::new()
	$start.FileName = $FileName
	$start.WorkingDirectory = $WorkingDirectory
	$start.UseShellExecute = $false
	$start.CreateNoWindow = $true
	$start.RedirectStandardOutput = $true
	$start.RedirectStandardError = $true
	$start.StandardOutputEncoding = $script:Utf8
	$start.StandardErrorEncoding = $script:Utf8
	$start.Environment['GIT_OPTIONAL_LOCKS'] = '0'
	foreach ($argument in $Arguments) { [void] $start.ArgumentList.Add($argument) }
	$process = [Diagnostics.Process]::new()
	$process.StartInfo = $start
	if (-not $process.Start()) { throw "Could not start $FileName with: $($Arguments -join ' ')" }
	$stdoutTask = $process.StandardOutput.ReadToEndAsync()
	$stderrTask = $process.StandardError.ReadToEndAsync()
	$process.WaitForExit()
	$run = [pscustomobject] @{
		ExitCode = $process.ExitCode
		Stdout = $stdoutTask.GetAwaiter().GetResult()
		Stderr = $stderrTask.GetAwaiter().GetResult()
	}
	$process.Dispose()
	return $run
}

function Get-StaticCheckShell() {
	$shell = [Environment]::ProcessPath
	if ([string]::IsNullOrEmpty($shell)) { $shell = 'pwsh' }
	return $shell
}

function Invoke-StaticCheckGit([string[]] $Arguments) {
	$run = Invoke-StaticCheckProcess 'git' (@('-C', $script:Root, '--no-pager') + $Arguments) $script:Root
	if ($run.ExitCode -ne 0) { throw "git $($Arguments -join ' ') failed with exit $($run.ExitCode): $($run.Stderr.Trim())" }
	return $run.Stdout
}

function Get-InventoryDocument() {
	$arguments = @('-NoProfile', '-File', $script:InventoryScript, '-RepositoryRoot', $script:Root, '-Baseline', $Baseline)
	if (-not [string]::IsNullOrWhiteSpace($Head)) { $arguments += @('-Head', $Head) }
	if ($IncludeUntracked) {
		# The inventory reports an untracked path only when the caller lists it, so the pass-through
		# switch supplies the whole untracked set in the comma-separated form that script splits.
		$untracked = @((Invoke-StaticCheckGit @('ls-files', '--others', '--exclude-standard', '-z')) -split "`0" | Where-Object { -not [string]::IsNullOrEmpty($_) })
		if ($untracked.Count -gt 0) { $arguments += @('-IncludeUntracked', ($untracked -join ',')) }
	}
	$run = Invoke-StaticCheckProcess (Get-StaticCheckShell) $arguments $script:Root
	$document = $null
	if (-not [string]::IsNullOrWhiteSpace($run.Stdout)) { $document = $run.Stdout | ConvertFrom-Json }
	if ($run.ExitCode -ne 0 -or $null -eq $document -or $document.status -cne 'pass') {
		$reason = if ($null -ne $document) { "$($document.code): $($document.message)" } else { $run.Stderr.Trim() }
		Complete-StaticChecks 2 'blocked' 'static-checks.inventory-unavailable' "The session change inventory did not produce a selectable result: $reason"
	}
	return $document
}

function Test-HeadPath([string] $Path) {
	# A commit-valued head is answered from its tree, so a file the working tree happens to hold but
	# that commit does not is never treated as present.
	if ([string]::IsNullOrEmpty($script:HeadSha)) { return Test-Path -LiteralPath (Join-Path $script:Root ($Path -replace '/', [IO.Path]::DirectorySeparatorChar)) -PathType Leaf }
	if ($null -eq $script:HeadPaths) {
		$script:HeadPaths = [Collections.Generic.HashSet[string]]::new([string[]] @())
		foreach ($line in ((Invoke-StaticCheckGit @('ls-tree', '-r', '--name-only', '-z', $script:HeadSha)) -split "`0")) {
			if (-not [string]::IsNullOrEmpty($line)) { [void] $script:HeadPaths.Add($line) }
		}
	}
	return $script:HeadPaths.Contains($Path)
}

function Get-HeadText([string] $Path) {
	if ([string]::IsNullOrEmpty($script:HeadSha)) { return [IO.File]::ReadAllText((Join-Path $script:Root ($Path -replace '/', [IO.Path]::DirectorySeparatorChar)), $script:Utf8) }
	return Invoke-StaticCheckGit @('show', "$($script:HeadSha):$Path")
}

function Get-ChangedPath([object] $Inventory, [scriptblock] $Predicate) {
	# A deleted path has no head side to check, and a rename is checked at its new path.
	$paths = [Collections.Generic.List[string]]::new()
	foreach ($entry in $Inventory.entries) {
		if ($entry.status -ceq 'D') { continue }
		if (-not (& $Predicate $entry)) { continue }
		if (-not (Test-HeadPath $entry.path)) { continue }
		if (-not $paths.Contains($entry.path)) { $paths.Add($entry.path) }
	}
	return , $paths
}

function Get-HeadingSlug([string] $Path) {
	# GitHub's heading slug: lowercased, everything but alphanumerics, spaces, hyphens, and underscores
	# dropped, spaces turned into hyphens, and a repeated slug suffixed -1, -2, ... in document order.
	if ($script:HeadingSlugs.ContainsKey($Path)) { return , $script:HeadingSlugs[$Path] }
	$slugs = [Collections.Generic.HashSet[string]]::new([string[]] @())
	$counts = @{}
	$fenced = $false
	foreach ($line in ((Get-HeadText $Path) -split "`r`n|`n|`r")) {
		if ($line -match '^\s*(?:```|~~~)') { $fenced = -not $fenced; continue }
		if ($fenced) { continue }
		if ($line -match '^#{1,6}\s+(.*)$') {
			$text = $Matches[1].Trim().TrimEnd('#').Trim()
			$slug = ($text.ToLowerInvariant() -replace '[^a-z0-9 \-_]', '') -replace ' ', '-'
			if ($counts.ContainsKey($slug)) {
				$counts[$slug] = $counts[$slug] + 1
				$slug = "$slug-$($counts[$slug])"
			}
			else { $counts[$slug] = 0 }
			[void] $slugs.Add($slug)
		}
	}
	$script:HeadingSlugs[$Path] = $slugs
	return , $slugs
}

function Resolve-RepositoryPath([string] $Directory, [string] $Target) {
	# Repository-relative resolution stays textual, because the checked path is a Git path and never the
	# host file system's: a target that walks above the repository root has nothing to resolve to.
	$segments = [Collections.Generic.List[string]]::new()
	if (-not [string]::IsNullOrEmpty($Directory)) { foreach ($segment in ($Directory -split '/')) { $segments.Add($segment) } }
	foreach ($segment in ($Target -split '/')) {
		if ($segment -ceq '' -or $segment -ceq '.') { continue }
		if ($segment -ceq '..') {
			if ($segments.Count -eq 0) { return $null }
			$segments.RemoveAt($segments.Count - 1)
			continue
		}
		$segments.Add($segment)
	}
	if ($segments.Count -eq 0) { return $null }
	return ($segments -join '/')
}

function Get-MarkdownLinkFailure([string] $Path, [ref] $LinkCount) {
	$failures = [Collections.Generic.List[object]]::new()
	$directory = if ($Path.Contains('/')) { $Path.Substring(0, $Path.LastIndexOf('/')) } else { '' }
	$number = 0
	$fenced = $false
	foreach ($line in ((Get-HeadText $Path) -split "`r`n|`n|`r")) {
		$number++
		if ($line -match '^\s*(?:```|~~~)') { $fenced = -not $fenced; continue }
		if ($fenced) { continue }
		foreach ($match in [regex]::Matches($line, $script:LinkPattern)) {
			$target = $match.Groups[1].Value.Trim('<', '>')
			# Only repository-relative targets are checkable here; a scheme names an external resource.
			if ([string]::IsNullOrEmpty($target) -or $target -match $script:SchemePattern -or $target.StartsWith('//')) { continue }
			$LinkCount.Value++
			$hashIndex = $target.IndexOf('#')
			$targetPath = if ($hashIndex -ge 0) { $target.Substring(0, $hashIndex) } else { $target }
			$anchor = if ($hashIndex -ge 0) { $target.Substring($hashIndex + 1) } else { $null }
			# A bare anchor names a heading in the linking file itself.
			$resolvedPath = if ([string]::IsNullOrEmpty($targetPath)) { $Path } else { Resolve-RepositoryPath $directory $targetPath }
			$resolves = $null -ne $resolvedPath -and (Test-HeadPath $resolvedPath)
			$anchorPresent = $null
			if (-not [string]::IsNullOrEmpty($anchor)) {
				$anchorPresent = $resolves -and $resolvedPath.ToLowerInvariant().EndsWith('.md') -and (Get-HeadingSlug $resolvedPath).Contains($anchor.ToLowerInvariant())
			}
			if ($resolves -and ($null -eq $anchorPresent -or $anchorPresent)) { continue }
			$failures.Add([ordered]@{ path = $Path; line = $number; target = $target; resolves = $resolves; anchorPresent = $anchorPresent })
		}
	}
	return , $failures
}

function New-CheckRow([string] $Name, [bool] $Triggered, [string] $Status, $Detail) {
	return [ordered]@{ name = $Name; triggered = $Triggered; status = $Status; detail = $Detail }
}

function Invoke-ValidateSkillCheck([object] $Inventory) {
	$triggered = [bool] $Inventory.triggers.validateSkill
	if (-not $triggered) { return New-CheckRow 'validate-skill' $false 'skipped' $null }
	$paths = Get-ChangedPath $Inventory { param($entry) $entry.class -ceq 'skill' }
	if ($paths.Count -eq 0) {
		if ([bool] $Inventory.truncated) { return New-CheckRow 'validate-skill' $true 'blocked' ([ordered]@{ reason = 'The inventory truncated its entry table, so the changed skill packages could not be selected.' }) }
		# A deleted skill package still sets the trigger from its baseline side, and has no head side to validate.
		return New-CheckRow 'validate-skill' $true 'pass' ([ordered]@{ reason = 'The changed skill package left no head-side path to validate, as a deletion does.' })
	}
	$results = [Collections.Generic.List[object]]::new()
	$status = 'pass'
	foreach ($path in $paths) {
		$run = Invoke-StaticCheckProcess (Get-StaticCheckShell) @('-NoProfile', '-File', $script:ValidateSkillScript, '-Path', (Join-Path $script:Root ($path -replace '/', [IO.Path]::DirectorySeparatorChar))) $script:Root
		# Exit 1 is the validator's ordinary INVALID result; only its exit 2 setup error blocks the check.
		$rowStatus = switch ($run.ExitCode) { 0 { 'pass' } 1 { 'fail' } default { 'blocked' } }
		if ($rowStatus -ceq 'blocked' -or ($rowStatus -ceq 'fail' -and $status -cne 'blocked')) { $status = $rowStatus }
		$lines = @(($run.Stdout -split "`r`n|`n|`r") | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
		$results.Add([ordered]@{ path = $path; exitCode = $run.ExitCode; lines = [object[]] $lines })
	}
	return New-CheckRow 'validate-skill' $true $status ([ordered]@{ results = [object[]] $results.ToArray() })
}

function Invoke-PlanSchedulerCheck([object] $Inventory) {
	if (-not [bool] $Inventory.triggers.planTouched) { return New-CheckRow 'plan-scheduler' $false 'skipped' $null }
	$run = Invoke-StaticCheckProcess (Get-StaticCheckShell) @('-NoProfile', '-File', $script:SchedulerScript, '-RepositoryRoot', $script:Root) $script:Root
	$document = $null
	if (-not [string]::IsNullOrWhiteSpace($run.Stdout)) { $document = $run.Stdout | ConvertFrom-Json }
	if ($null -eq $document) { return New-CheckRow 'plan-scheduler' $true 'blocked' ([ordered]@{ exitCode = $run.ExitCode; reason = $run.Stderr.Trim() }) }
	# The scheduler check's own blocked result (an unprovisioned WorktreeCli) is a blocked row here. Its
	# other status words come from `plan validate` ('valid' for a healthy scheduler), so the pass/fail
	# decision is read from the exit code it passes through instead of from that vocabulary.
	$status = if ([string] $document.status -ceq 'blocked') { 'blocked' } elseif ($run.ExitCode -eq 0) { 'pass' } else { 'fail' }
	return New-CheckRow 'plan-scheduler' $true $status $document
}

function Invoke-MarkdownLinkCheck([object] $Inventory, [bool] $Truncated) {
	# There is no markdown entry class: .md spans the skill, plan, and doc classes, and doc also holds
	# .txt, so this check selects by extension across every entry.
	$paths = Get-ChangedPath $Inventory { param($entry) $entry.path.ToLowerInvariant().EndsWith('.md') }
	if ($paths.Count -eq 0) {
		if (-not $Truncated) { return New-CheckRow 'markdown-links' $false 'skipped' $null }
		return New-CheckRow 'markdown-links' $true 'blocked' ([ordered]@{ reason = 'The inventory truncated its entry table, so the changed markdown files could not be selected.' })
	}
	$linkCount = 0
	$failures = [Collections.Generic.List[object]]::new()
	foreach ($path in $paths) {
		$count = 0
		foreach ($failure in (Get-MarkdownLinkFailure $path ([ref] $count))) { $failures.Add($failure) }
		$linkCount += $count
	}
	$status = if ($failures.Count -gt 0) { 'fail' } else { 'pass' }
	return New-CheckRow 'markdown-links' $true $status ([ordered]@{ linkCount = $linkCount; failures = [object[]] $failures.ToArray() })
}

try {
	$script:Root = Get-AgentCanonicalPath $RepositoryRoot
	if (-not (Test-Path -LiteralPath $script:Root -PathType Container)) {
		Complete-StaticChecks 2 'blocked' 'static-checks.repository-root-invalid' "-RepositoryRoot must be an existing directory: '$RepositoryRoot'."
	}
	foreach ($composed in @($script:InventoryScript, $script:SchedulerScript, $script:ValidateSkillScript)) {
		if (-not (Test-Path -LiteralPath $composed -PathType Leaf)) {
			Complete-StaticChecks 2 'blocked' 'static-checks.script-missing' "A composed script is missing: '$composed'."
		}
	}
	$inventory = Get-InventoryDocument
	$script:HeadSha = if ([string]::IsNullOrWhiteSpace($inventory.headSha)) { '' } else { $inventory.headSha }
	$truncated = [bool] $inventory.truncated
	$result.truncated = $truncated

	# The three rows are always present, in the order the deleted static-checks table listed them.
	$checks = @(
		(Invoke-ValidateSkillCheck $inventory)
		(Invoke-PlanSchedulerCheck $inventory)
		(Invoke-MarkdownLinkCheck $inventory $truncated)
	)
	$result.checks = [object[]] $checks
	$triggeredStatuses = @($checks | Where-Object { $_.triggered } | ForEach-Object { $_.status })
	if ($triggeredStatuses -ccontains 'blocked') {
		Complete-StaticChecks 2 'blocked' 'static-checks.blocked' 'A triggered static check could not run to a result.'
	}
	if ($triggeredStatuses -ccontains 'fail') {
		Complete-StaticChecks 1 'fail' 'static-checks.failed' 'A triggered static check reported findings.'
	}
	Complete-StaticChecks 0 'pass' 'ok' "Ran $($triggeredStatuses.Count) triggered static check(s); all passed."
}
catch {
	Complete-StaticChecks 1 'error' 'internal.error' $_.Exception.Message
}
