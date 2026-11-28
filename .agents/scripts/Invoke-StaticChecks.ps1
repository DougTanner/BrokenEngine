# The static checks for the Run targeted pre-review checks
# step, selected from the session change inventory and run in one pass: `validate-skill`
# for each changed skill package, `markdown-links` for every changed markdown file,
# `file-hygiene` for every changed text file: Git's whitespace errors (`git diff --check` under the
# `.gitattributes` whitespace attribute) and space-indented GLSL on added lines, and per file invalid
# UTF-8, an added BOM, and a missing final newline the baseline side had; and `line-length` for every
# changed markdown file and `Documents/<name>.txt` guide: every line of the whole file, not only the
# added lines, longer than 500 characters without its line ending (Grep elides such a line), apart
# from, in a markdown file only, fence delimiters, lines inside a fence, table rows (first
# non-space character `|`), and a line 1 starting with the Plan metadata marker prefix. Two rows cover
# the instruction-prose set (root `AGENTS.md`, `.md` under `.agents/` or `.claude/`, `.toml` under
# `.codex/agents/`): the report-only `instruction-size`, which never fails, gives each changed entry's
# baseline and head token counts as Measure-Tokens.ps1 measures a whole file, deletions included, and
# their `netTokens` sum, leaving out an entry with a side that is not valid UTF-8; `model-names` fails
# on a model family name or slug in an added line, apart from a `model` setting line in an
# `.agents/agents/`, `.claude/agents/`, or `.codex/agents/` file. `workflow-incidents` checks
# `Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md` when it changes (and exists on the
# head side), requiring `## Entries` with its fixed 5-column table header, 5-cell separator and data
# rows (literal `|` splitting), and unique normalized `(Owner, Symptom, Session branch)` rows.
# `comment-values` fails on a `//` comment in an added line of a changed `cpp`, `dual-language-header`, or
# `glsl` entry that copies a `k`-prefixed constant's value (`CommentValuePattern`), splitting each line
# with AgentScriptCommon's C++ token pattern so a `//` inside a literal is not a comment.
# `vcxproj-affinity` runs the project-pair validator (`-AllGameProjects`) when a discovered game project's
# `.vcxproj`/`.vcxproj.filters`, a `.h`/`.cpp` under a discovered game project, `Engine/Source/`,
# `Engine/Data/Shaders/`, or `Common/`, or the validator/affinity/discovery scripts change.
# `review-round-scenarios` runs every case file under `ReviewRoundScenarios/` but `ScenarioCommon.ps1` at
# once, each in its own subdirectory of one scratch directory under the worktree's `Temp/`, when a path
# there or a script the cases run changes; a case still running `ReviewRoundScenarioDeadlineSeconds` after
# the run started is stopped and blocks the row, so the run reports inside the host's foreground limit.
# `validate-skill` and `vcxproj-affinity` compose existing scripts (the bundled skill validator and the
# project-pair validator). The run reports results only — it never decides whether a failing check blocks
# a slice and never edits a file. Every other row writes nothing to disk (GIT_OPTIONAL_LOCKS=0 keeps Git
# from refreshing the index); `review-round-scenarios` writes only its scratch directory under `Temp/`,
# which it removes, so under a read-only sandbox it reports `blocked`. Stdout carries only the result
# document.
# The result document holds one `checks` row per check, each carrying a boolean `triggered` and a
# `status` (pass, fail, blocked, or skipped); a passing or failing `line-length` row's detail also
# carries the enforced `limit`. The run passes (exit 0) when no triggered row fails or is
# blocked, including when none triggers; otherwise it exits 2 if any triggered row is blocked, else 1. A
# setup failure (invalid root, missing composed script, unavailable inventory) exits 2 with no rows, and
# an unexpected error exits 1 with status `error`. `-Head <commit>` selects the changed
# files from a committed head instead of the working tree, and `-IncludeUntracked` adds the untracked
# files when checking the working tree. `markdown-links`, `file-hygiene`, `line-length`,
# `instruction-size`, `model-names`, `workflow-incidents`, and `comment-values` read content from that commit;
# `validate-skill`, `vcxproj-affinity`, and `review-round-scenarios` run against the working tree.
# Every Git read of a Projects/Coretide/ entry runs in the session Coretide clone, against the inventory's
# `coretide` baseline and head.
[CmdletBinding()]
param(
	[Parameter(Mandatory)][string] $RepositoryRoot,
	[Parameter(Mandatory)][string] $Baseline,
	[string] $Head,
	[switch] $IncludeUntracked
)

$script:RunClock = [Diagnostics.Stopwatch]::StartNew()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'AgentScriptCommon.psm1') -Force

$script:MaximumMessageLength = 256
$script:MaximumLineLength = 500
# Leaves the rest of the host's 600-second foreground command limit for stopping overdue cases, removing the
# scratch directory, and writing the result.
$script:ReviewRoundScenarioDeadlineSeconds = 540

$script:InventoryModule = Join-Path $PSScriptRoot 'SessionChangeInventory.psm1'
$script:ValidateSkillScript = Join-Path $PSScriptRoot '../skills/external-skill-creator/scripts/Validate-Skill.ps1'
$script:VcxprojPairCheckScript = Join-Path $PSScriptRoot '../skills/update-vcxproj/scripts/Invoke-VcxprojPairCheck.ps1'
$script:ScenarioCommonScript = Join-Path $PSScriptRoot 'ReviewRoundScenarios/ScenarioCommon.ps1'
$script:Utf8 = [Text.UTF8Encoding]::new($false)
$script:Root = $null
$script:HeadSha = ''
$script:HeadPaths = $null
$script:UntrackedPaths = @()
# The session Coretide clone's Root, BaselineSha, HeadSha, and HeadPaths, or null.
$script:Coretide = $null
$script:CoretidePrefix = 'Projects/Coretide/'
$script:WorkflowIncidentsPath = 'Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md'
$script:HeadingSlugs = @{}
# Inline markdown link: the target is everything up to the closing parenthesis or the optional title.
$script:LinkPattern = '\[(?:[^\[\]]*)\]\(\s*([^)\s]+)'
$script:SchemePattern = '^[A-Za-z][A-Za-z0-9+.-]*:'
# A model slug or family name, bounded as `git grep -w` bounds a word: no adjacent letter, digit, or
# underscore, so a hyphen-joined name still matches.
$script:ModelNamePattern = [regex]::new('(?<![A-Za-z0-9_])(?:gpt-[0-9][A-Za-z0-9]*(?:[.-][A-Za-z0-9]+)*|claude-[A-Za-z]+-[0-9][A-Za-z0-9]*(?:[.-][A-Za-z0-9]+)*|opus|sonnet|haiku|astra|sol|luna|fable)(?![A-Za-z0-9_])', [Text.RegularExpressions.RegexOptions]::IgnoreCase)
$script:CppTokenPattern = Get-AgentCppTokenPattern
# A `k`-prefixed constant name followed by `=` or `==` and a number: a comment copying the constant's value.
$script:CommentValuePattern = [regex]::new('\bk[a-z]*[A-Z]\w*\s*==?\s*-?[0-9]')

$result = [ordered]@{
	schemaVersion = 'broken-engine-static-checks/v10'
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

function Invoke-StaticCheckProcess([string] $FileName, [string[]] $Arguments, [string] $WorkingDirectory, [switch] $RawStdout) {
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
	# Decoding stdout as text would hide a BOM and turn invalid bytes into replacement characters, so the
	# byte checks read it raw.
	$memory = [IO.MemoryStream]::new()
	$stdoutTask = if ($RawStdout) { $process.StandardOutput.BaseStream.CopyToAsync($memory) } else { $process.StandardOutput.ReadToEndAsync() }
	$stderrTask = $process.StandardError.ReadToEndAsync()
	$process.WaitForExit()
	$stdout = $stdoutTask.GetAwaiter().GetResult()
	if ($RawStdout) { $stdout = $memory.ToArray() }
	$run = [pscustomobject] @{
		ExitCode = $process.ExitCode
		Stdout = $stdout
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

function Invoke-StaticCheckGit([string[]] $Arguments, [switch] $RawStdout, [string] $Root = $script:Root) {
	$run = Invoke-StaticCheckProcess 'git' (@('-C', $Root, '--no-pager') + $Arguments) $Root -RawStdout:$RawStdout
	if ($run.ExitCode -ne 0) { throw "git $($Arguments -join ' ') failed with exit $($run.ExitCode): $($run.Stderr.Trim())" }
	# The comma keeps a raw byte[] from being unrolled into the pipeline.
	return , $run.Stdout
}

function Test-CoretidePath([string] $Path) {
	return $null -ne $script:Coretide -and $Path.StartsWith($script:CoretidePrefix, [StringComparison]::Ordinal)
}

function Get-InventoryDocument() {
	$inventoryParams = @{ RepositoryRoot = $script:Root; Baseline = $Baseline }
	if (-not [string]::IsNullOrWhiteSpace($Head)) { $inventoryParams.Head = $Head }
	if ($IncludeUntracked) {
		# The hygiene check enumerates the same untracked set for itself.
		$script:UntrackedPaths = @((Invoke-StaticCheckGit @('ls-files', '--others', '--exclude-standard', '-z')) -split "`0" | Where-Object { -not [string]::IsNullOrEmpty($_) })
		$coretideRoot = Get-AgentCoretideRepository $script:Root
		if ($null -ne $coretideRoot) {
			$script:UntrackedPaths += @((Invoke-StaticCheckGit @('ls-files', '--others', '--exclude-standard', '-z') -Root $coretideRoot) -split "`0" |
				Where-Object { -not [string]::IsNullOrEmpty($_) } | ForEach-Object { $script:CoretidePrefix + $_ })
		}
		$inventoryParams.IncludeUntracked = $true
	}
	$document = Get-SessionChangeInventory @inventoryParams
	if ($null -eq $document -or $document.status -cne 'pass') {
		$reason = if ($null -ne $document) { "$($document.code): $($document.message)" } else { 'no result returned' }
		Complete-StaticChecks 2 'blocked' 'static-checks.inventory-unavailable' "The session change inventory did not produce a selectable result: $reason"
	}
	return $document
}

function Test-HeadPath([string] $Path) {
	# A commit-valued head is answered from its tree, so a file the working tree happens to hold but
	# that commit does not is never treated as present.
	if ([string]::IsNullOrEmpty($script:HeadSha)) { return Test-Path -LiteralPath (Join-Path $script:Root ($Path -replace '/', [IO.Path]::DirectorySeparatorChar)) -PathType Leaf }
	if (Test-CoretidePath $Path) {
		if ($null -eq $script:Coretide.HeadPaths) {
			$script:Coretide.HeadPaths = [Collections.Generic.HashSet[string]]::new([string[]] @())
			foreach ($line in ((Invoke-StaticCheckGit @('ls-tree', '-r', '--name-only', '-z', $script:Coretide.HeadSha) -Root $script:Coretide.Root) -split "`0")) {
				if (-not [string]::IsNullOrEmpty($line)) { [void] $script:Coretide.HeadPaths.Add($script:CoretidePrefix + $line) }
			}
		}
		return $script:Coretide.HeadPaths.Contains($Path)
	}
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
	if (Test-CoretidePath $Path) { return Invoke-StaticCheckGit @('show', "$($script:Coretide.HeadSha):$($Path.Substring($script:CoretidePrefix.Length))") -Root $script:Coretide.Root }
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
	foreach ($line in ((Get-AgentMarkdownSearchText ((Get-HeadText $Path) -replace "`r`n?", "`n")) -split "`n")) {
		$number++
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

function Get-SkillPackageName([string] $Path) {
	# Every file under `.agents/skills/<package>/` belongs to that package; a path elsewhere has none.
	if ([string]::IsNullOrEmpty($Path)) { return $null }
	if ($Path -cmatch '^\.agents/skills/([^/]+)/') { return $Matches[1] }
	return $null
}

function Get-HeadSkillPackageName() {
	# The sweep's candidate packages come from the head side itself — the head tree's path set under -Head,
	# and the working tree otherwise — so a package only the working tree holds is never swept for a
	# commit-valued head. Whether a candidate has a SKILL.md is still decided by Test-HeadPath.
	$names = [Collections.Generic.List[string]]::new()
	if ([string]::IsNullOrEmpty($script:HeadSha)) {
		$directory = Join-Path $script:Root ('.agents/skills' -replace '/', [IO.Path]::DirectorySeparatorChar)
		if (Test-Path -LiteralPath $directory -PathType Container) {
			foreach ($child in (Get-ChildItem -LiteralPath $directory -Directory)) {
				$names.Add($child.Name)
			}
		}
		return , $names
	}
	# Asking for any path fills the cached head path set this enumeration then reads.
	[void] (Test-HeadPath '.agents/skills')
	foreach ($path in $script:HeadPaths) {
		$name = Get-SkillPackageName $path
		if ($null -ne $name -and -not $names.Contains($name)) { $names.Add($name) }
	}
	return , $names
}

function Invoke-ValidateSkillCheck([object] $Inventory) {
	# Any changed file inside a skill package can invalidate that package, so the targets are derived from
	# the package name on both sides of every changed path — a rename out of a package can break the
	# package it left — instead of from the `skill` class, which covers SKILL.md alone. Get-ChangedPath is
	# not reusable here: it drops deletions, and a deleted bundled file is exactly a case to catch.
	$sweepPaths = @('.agents/skills/external-skill-creator/scripts/Validate-Skill.ps1', '.agents/scripts/AgentScriptCommon.psm1')
	$names = [Collections.Generic.List[string]]::new()
	$sweep = $false
	foreach ($entry in $Inventory.entries) {
		if ($sweepPaths -ccontains $entry.path -or $sweepPaths -ccontains $entry.oldPath) { $sweep = $true }
		foreach ($side in @($entry.path, $entry.oldPath)) {
			$name = Get-SkillPackageName $side
			if ($null -ne $name -and -not $names.Contains($name)) { $names.Add($name) }
		}
	}
	# A change to the validator or to the shared module it imports can change every package's verdict, so
	# it sweeps the whole tree.
	if ($sweep) {
		foreach ($name in (Get-HeadSkillPackageName)) { if (-not $names.Contains($name)) { $names.Add($name) } }
	}
	# Only a package with a head-side SKILL.md is validatable; pointing the validator at a package
	# directory without one is a setup error. Pinning the trigger after this test therefore keeps a diff
	# that touches only such a directory from triggering the check at all.
	$paths = [Collections.Generic.List[string]]::new()
	foreach ($name in $names) {
		$path = ".agents/skills/$name/SKILL.md"
		if ((Test-HeadPath $path) -and -not $paths.Contains($path)) { $paths.Add($path) }
	}
	$triggered = $paths.Count -gt 0 -or [bool] $Inventory.triggers.validateSkill
	if (-not $triggered) { return New-CheckRow 'validate-skill' $false 'skipped' $null }
	$failures = [Collections.Generic.List[object]]::new()
	$status = 'pass'
	foreach ($path in $paths) {
		$run = Invoke-StaticCheckProcess (Get-StaticCheckShell) @('-NoProfile', '-File', $script:ValidateSkillScript, '-Path', (Join-Path $script:Root ($path -replace '/', [IO.Path]::DirectorySeparatorChar))) $script:Root
		# Exit 1 is the validator's ordinary INVALID result; only its exit 2 setup error blocks the check.
		$rowStatus = switch ($run.ExitCode) { 0 { 'pass' } 1 { 'fail' } default { 'blocked' } }
		if ($rowStatus -ceq 'blocked' -or ($rowStatus -ceq 'fail' -and $status -cne 'blocked')) { $status = $rowStatus }
		# Only a package that did not pass is listed in failures, with its exit code and the validator's
		# output; a passing package only adds to packageCount, because its exit code already states the
		# result and a sweep runs every package.
		if ($rowStatus -ceq 'pass') { continue }
		$lines = @(($run.Stdout -split "`r`n|`n|`r") | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
		$failures.Add([ordered]@{ path = $path; exitCode = $run.ExitCode; lines = [object[]] $lines })
	}
	return New-CheckRow 'validate-skill' $true $status ([ordered]@{ packageCount = $paths.Count; failures = [object[]] $failures.ToArray() })
}

function Invoke-MarkdownLinkCheck([object] $Inventory) {
	# There is no markdown entry class: .md spans the skill, plan, and doc classes, and doc also holds
	# .txt, so this check selects by extension across every entry.
	$paths = Get-ChangedPath $Inventory { param($entry) $entry.path.ToLowerInvariant().EndsWith('.md') }
	if ($paths.Count -eq 0) { return New-CheckRow 'markdown-links' $false 'skipped' $null }
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

function Get-LineLengthFailure([string] $Path) {
	$failures = [Collections.Generic.List[object]]::new()
	$markdown = $Path.ToLowerInvariant().EndsWith('.md')
	$fenced = $false
	$number = 0
	foreach ($line in ((Get-HeadText $Path) -split "`r`n|`n|`r")) {
		$number++
		if ($markdown) {
			# The Get-HeadingSlug fence toggle: a fenced code line must stay copyable as one line, and a
			# table row cannot wrap without breaking the table, and the Plan scheduler reads the line-1
			# metadata marker as one line.
			if ($line -match '^\s*(?:```|~~~)') { $fenced = -not $fenced; continue }
			if ($fenced -or $line -match '^\s*\|') { continue }
			if ($number -eq 1 -and $line.StartsWith('<!-- broken-engine-plan/v2 ', [StringComparison]::Ordinal)) { continue }
		}
		if ($line.Length -gt $script:MaximumLineLength) { $failures.Add([ordered]@{ path = $Path; line = $number; length = $line.Length }) }
	}
	return , $failures
}

function Invoke-LineLengthCheck([object] $Inventory) {
	$paths = Get-ChangedPath $Inventory {
		param($entry)
		$path = $entry.path.ToLowerInvariant()
		$path.EndsWith('.md') -or $path -match '^documents/[^/]+\.txt$'
	}
	if ($paths.Count -eq 0) { return New-CheckRow 'line-length' $false 'skipped' $null }
	$failures = [Collections.Generic.List[object]]::new()
	foreach ($path in $paths) {
		foreach ($failure in (Get-LineLengthFailure $path)) { $failures.Add($failure) }
	}
	$status = if ($failures.Count -gt 0) { 'fail' } else { 'pass' }
	return New-CheckRow 'line-length' $true $status ([ordered]@{ fileCount = $paths.Count; limit = $script:MaximumLineLength; failures = [object[]] $failures.ToArray() })
}

# Root and Prefix name the repository the Git command runs in and the prefix its reported paths gain.
function Add-WhitespaceFailure([string[]] $Arguments, [int] $CleanExit, [int] $FindingsExit, [Collections.Generic.List[object]] $Failures, [string] $Root = $script:Root, [string] $Prefix = '') {
	$run = Invoke-StaticCheckProcess 'git' (@('-C', $Root, '--no-pager', '-c', 'core.quotePath=false') + $Arguments) $Root
	if ($run.ExitCode -ne $CleanExit -and $run.ExitCode -ne $FindingsExit) { throw "git $($Arguments -join ' ') failed with exit $($run.ExitCode): $($run.Stderr.Trim())" }
	# Each finding is one `<path>:<line>: <problem>.` line followed by the offending `+` lines.
	foreach ($line in ($run.Stdout -split "`n")) {
		if ($line.StartsWith('+') -or $line.TrimEnd("`r") -cnotmatch '^(.+):(\d+): (.+?)\.?$') { continue }
		$Failures.Add([ordered]@{ path = $Prefix + $Matches[1]; line = [int] $Matches[2]; problem = $Matches[3] })
	}
}

function Test-SpaceIndentedLine([string] $Text) {
	# The style-rule-1 candidate pattern and its block-comment exception (Find-SessionCandidates.ps1); a
	# tab-then-space aligned line starts with a tab and stays clear.
	return $Text -cmatch '^ +[^ ]' -and $Text -cnotmatch '^ +\*(?:\s|/|$)'
}

function Get-AddedLine([string[]] $DiffArguments, [string[]] $Paths, [string] $Root = $script:Root) {
	# Each added line of the diff over Paths, with its new-side path and line number. -M pairs a rename
	# only when Paths holds both of its paths.
	$diff = Invoke-StaticCheckGit (@('-c', 'core.quotePath=false', 'diff', '-U0', '-M', '--no-color', '--no-ext-diff') + $DiffArguments + @('--') + $Paths) -Root $Root
	$added = [Collections.Generic.List[object]]::new()
	$path = $null
	$number = 0
	$inHunk = $false
	foreach ($line in ($diff -split "`n")) {
		$line = $line.TrimEnd("`r")
		if ($line.StartsWith('diff --git ')) { $path = $null; $inHunk = $false; continue }
		if (-not $inHunk -and $line.StartsWith('+++ ')) { $path = if ($line -ceq '+++ /dev/null') { $null } else { $line.Substring(6).TrimEnd("`t") }; continue }
		if ($line -cmatch '^@@ -\d+(?:,\d+)? \+(\d+)(?:,\d+)? @@') { $inHunk = $true; $number = [int] $Matches[1]; continue }
		# With -U0 a hunk holds no context lines, and only an added line advances the new-side number.
		if (-not $inHunk -or -not $line.StartsWith('+')) { continue }
		if ($null -ne $path) { $added.Add([pscustomobject] @{ Path = $path; Line = $number; Text = $line.Substring(1) }) }
		$number++
	}
	return , $added
}

function Get-AddedLineRoute([object] $Inventory, [Collections.Generic.List[string]] $Paths) {
	# Splits Paths into the untracked ones and the engine and Projects/Coretide/ diff pathspecs, a rename's
	# old side included. -M pairs a rename only when the pathspec holds both of its paths.
	$entries = @{}
	foreach ($entry in $Inventory.entries) { if ($entry.status -cne 'D') { $entries[$entry.path] = $entry } }
	# Untracked paths exist only against the working tree; a commit-valued head has none.
	$untracked = @(if ([string]::IsNullOrEmpty($script:HeadSha)) { $script:UntrackedPaths })
	$route = [pscustomobject] @{
		Untracked = [Collections.Generic.List[string]]::new()
		Pathspec = [Collections.Generic.List[string]]::new()
		CoretidePathspec = [Collections.Generic.List[string]]::new()
	}
	foreach ($path in $Paths) {
		if ($untracked -ccontains $path) { $route.Untracked.Add($path); continue }
		$sidePaths = @($path)
		if ($null -ne $entries[$path].oldPath) { $sidePaths += $entries[$path].oldPath }
		foreach ($sidePath in $sidePaths) {
			if (Test-CoretidePath $sidePath) { $route.CoretidePathspec.Add($sidePath.Substring($script:CoretidePrefix.Length)) }
			else { $route.Pathspec.Add($sidePath) }
		}
	}
	return $route
}

function Add-AddedGlslIndentFailure([string[]] $DiffArguments, [string[]] $Paths, [Collections.Generic.List[object]] $Failures, [string] $Root = $script:Root, [string] $Prefix = '') {
	foreach ($added in (Get-AddedLine $DiffArguments $Paths $Root)) {
		if (Test-SpaceIndentedLine $added.Text) { $Failures.Add([ordered]@{ path = $Prefix + $added.Path; line = $added.Line; problem = 'indent with spaces' }) }
	}
}

function Invoke-FileHygieneCheck([object] $Inventory) {
	$failures = [Collections.Generic.List[object]]::new()
	$diffArguments = @($Inventory.baselineSha)
	if (-not [string]::IsNullOrEmpty($script:HeadSha)) { $diffArguments += $script:HeadSha }
	# Untracked paths exist only against the working tree; a commit-valued head has none.
	$untracked = @(if ([string]::IsNullOrEmpty($script:HeadSha)) { $script:UntrackedPaths })

	# Whitespace errors on added lines; -M keeps a renamed file's unchanged lines from counting as added.
	Add-WhitespaceFailure (@('diff', '--check', '-M', '--no-color') + $diffArguments) 0 2 $failures
	$coretideDiffArguments = @()
	if ($null -ne $script:Coretide) {
		$coretideDiffArguments = @($script:Coretide.BaselineSha)
		if (-not [string]::IsNullOrEmpty($script:Coretide.HeadSha)) { $coretideDiffArguments += $script:Coretide.HeadSha }
		Add-WhitespaceFailure (@('diff', '--check', '-M', '--no-color') + $coretideDiffArguments) 0 2 $failures $script:Coretide.Root $script:CoretidePrefix
	}
	$nullDevice = if ($IsWindows) { 'NUL' } else { '/dev/null' }
	foreach ($path in $untracked) {
		if (Test-CoretidePath $path) { Add-WhitespaceFailure @('diff', '--no-index', '--check', '--no-color', '--', $nullDevice, $path.Substring($script:CoretidePrefix.Length)) 1 3 $failures $script:Coretide.Root $script:CoretidePrefix }
		else { Add-WhitespaceFailure @('diff', '--no-index', '--check', '--no-color', '--', $nullDevice, $path) 1 3 $failures }
	}

	$entries = @{}
	foreach ($entry in $Inventory.entries) { if ($entry.status -cne 'D') { $entries[$entry.path] = $entry } }

	# GLSL indentation on added lines.
	$glslRoute = Get-AddedLineRoute $Inventory (Get-ChangedPath $Inventory { param($entry) $entry.class -cin @('glsl', 'dual-language-header') })
	# An empty pathspec would diff every path.
	if ($glslRoute.Pathspec.Count -gt 0) { Add-AddedGlslIndentFailure $diffArguments ([string[]] $glslRoute.Pathspec) $failures }
	if ($glslRoute.CoretidePathspec.Count -gt 0) { Add-AddedGlslIndentFailure $coretideDiffArguments ([string[]] $glslRoute.CoretidePathspec) $failures $script:Coretide.Root $script:CoretidePrefix }

	# File-level byte checks: gitlinks and symlinks carry no file bytes, and binary is Git's own decision.
	$bytePaths = Get-ChangedPath $Inventory { param($entry) $entry.class -cne 'binary' -and $entry.current.mode -cin @('100644', '100755') }
	foreach ($path in $bytePaths) {
		$entry = $entries[$path]
		# Plain assignments: an `if` expression would unroll the byte[] into the pipeline.
		$head = $null
		if ([string]::IsNullOrEmpty($script:HeadSha)) { $head = [IO.File]::ReadAllBytes((Join-Path $script:Root ($path -replace '/', [IO.Path]::DirectorySeparatorChar))) }
		elseif (Test-CoretidePath $path) { $head = Invoke-StaticCheckGit @('cat-file', 'blob', "$($script:Coretide.HeadSha):$($path.Substring($script:CoretidePrefix.Length))") -RawStdout -Root $script:Coretide.Root }
		else { $head = Invoke-StaticCheckGit @('cat-file', 'blob', "$($script:HeadSha):$path") -RawStdout }
		$baseline = $null
		if ($null -ne $entry.baseline) {
			$baselinePath = if ($null -ne $entry.oldPath) { $entry.oldPath } else { $path }
			if (Test-CoretidePath $baselinePath) { $baseline = Invoke-StaticCheckGit @('cat-file', 'blob', "$($script:Coretide.BaselineSha):$($baselinePath.Substring($script:CoretidePrefix.Length))") -RawStdout -Root $script:Coretide.Root }
			else { $baseline = Invoke-StaticCheckGit @('cat-file', 'blob', "$($Inventory.baselineSha):$baselinePath") -RawStdout }
		}
		$text = $null
		try { $text = Get-AgentNormalizedText $head }
		catch { $failures.Add([ordered]@{ path = $path; line = $null; problem = 'invalid UTF-8' }) }
		$headBom = $head.Length -ge 3 -and $head[0] -eq 0xEF -and $head[1] -eq 0xBB -and $head[2] -eq 0xBF
		$baselineBom = $null -ne $baseline -and $baseline.Length -ge 3 -and $baseline[0] -eq 0xEF -and $baseline[1] -eq 0xBB -and $baseline[2] -eq 0xBF
		if ($headBom -and -not $baselineBom) { $failures.Add([ordered]@{ path = $path; line = $null; problem = 'added BOM' }) }
		# A file whose baseline side also lacked a final newline keeps that state.
		$baselineFinal = $null -eq $baseline -or ($baseline.Length -gt 0 -and $baseline[$baseline.Length - 1] -eq 0x0A)
		if ($head.Length -gt 0 -and $head[$head.Length - 1] -ne 0x0A -and $baselineFinal) { $failures.Add([ordered]@{ path = $path; line = $null; problem = 'no newline at end of file' }) }
		if ($null -eq $text -or -not $glslRoute.Untracked.Contains($path)) { continue }
		$number = 0
		foreach ($line in ($text -split "`n")) {
			$number++
			if (Test-SpaceIndentedLine $line) { $failures.Add([ordered]@{ path = $path; line = $number; problem = 'indent with spaces' }) }
		}
	}

	$detail = [ordered]@{ fileCount = $bytePaths.Count; failures = [object[]] $failures.ToArray() }
	if ($failures.Count -gt 0) { return New-CheckRow 'file-hygiene' $true 'fail' $detail }
	if ($bytePaths.Count -eq 0) { return New-CheckRow 'file-hygiene' $false 'skipped' $null }
	return New-CheckRow 'file-hygiene' $true 'pass' $detail
}

function Test-InstructionProsePath([string] $Path) {
	# Ordinal, case-sensitive matches, so no Projects/Coretide/ path is in the set.
	return $Path -ceq 'AGENTS.md' -or $Path -cmatch '^\.(?:agents|claude)/.+\.md$' -or $Path -cmatch '^\.codex/agents/.+\.toml$'
}

function Measure-InstructionTokenCount([byte[]] $Bytes) {
	# Measure-Tokens.ps1's whole-file measure, or null for bytes that are not valid UTF-8, which
	# `file-hygiene` reports for a head side.
	try { return Measure-AgentTokenCount (Get-AgentNormalizedText $Bytes) }
	catch { return $null }
}

function Invoke-InstructionSizeCheck([object] $Inventory) {
	# Report only: a deletion counts against the sum, and a side outside the set or absent measures 0.
	$files = [Collections.Generic.List[object]]::new()
	$selectedCount = 0
	$netTokens = [int64] 0
	foreach ($entry in $Inventory.entries) {
		if (-not (Test-InstructionProsePath $entry.path) -and -not (Test-InstructionProsePath $entry.oldPath)) { continue }
		$selectedCount++
		$baselinePath = if ($null -ne $entry.oldPath) { $entry.oldPath } else { $entry.path }
		$baselineTokens = [int64] 0
		if ($null -ne $entry.baseline -and (Test-InstructionProsePath $baselinePath)) {
			$baselineTokens = Measure-InstructionTokenCount (Invoke-StaticCheckGit @('cat-file', 'blob', "$($Inventory.baselineSha):$baselinePath") -RawStdout)
		}
		$headTokens = [int64] 0
		if ($entry.status -cne 'D' -and (Test-InstructionProsePath $entry.path)) {
			# Plain assignments: an `if` expression would unroll the byte[] into the pipeline.
			$head = $null
			if ([string]::IsNullOrEmpty($script:HeadSha)) { $head = [IO.File]::ReadAllBytes((Join-Path $script:Root ($entry.path -replace '/', [IO.Path]::DirectorySeparatorChar))) }
			else { $head = Invoke-StaticCheckGit @('cat-file', 'blob', "$($script:HeadSha):$($entry.path)") -RawStdout }
			$headTokens = Measure-InstructionTokenCount $head
		}
		if ($null -eq $baselineTokens -or $null -eq $headTokens) { continue }
		$netTokens += $headTokens - $baselineTokens
		$files.Add([ordered]@{ path = $entry.path; baselineTokens = $baselineTokens; headTokens = $headTokens })
	}
	if ($selectedCount -eq 0) { return New-CheckRow 'instruction-size' $false 'skipped' $null }
	return New-CheckRow 'instruction-size' $true 'pass' ([ordered]@{ netTokens = $netTokens; files = [object[]] $files.ToArray() })
}

function Invoke-ModelNameCheck([object] $Inventory) {
	$paths = Get-ChangedPath $Inventory { param($entry) Test-InstructionProsePath $entry.path }
	if ($paths.Count -eq 0) { return New-CheckRow 'model-names' $false 'skipped' $null }
	# Test-InstructionProsePath holds no Projects/Coretide/ path, so the Coretide pathspec stays empty.
	$route = Get-AddedLineRoute $Inventory $paths
	$added = [Collections.Generic.List[object]]::new()
	# Every line of an untracked path is added.
	foreach ($path in $route.Untracked) {
		$number = 0
		foreach ($line in ((Get-HeadText $path) -split "`r`n|`n|`r")) {
			$number++
			$added.Add([pscustomobject] @{ Path = $path; Line = $number; Text = $line })
		}
	}
	$diffArguments = @($Inventory.baselineSha)
	if (-not [string]::IsNullOrEmpty($script:HeadSha)) { $diffArguments += $script:HeadSha }
	# An empty pathspec would diff every path.
	if ($route.Pathspec.Count -gt 0) { $added.AddRange((Get-AddedLine $diffArguments ([string[]] $route.Pathspec))) }
	$failures = [Collections.Generic.List[object]]::new()
	foreach ($line in $added) {
		# A role definition's model setting names a model by design.
		if ($line.Path -cmatch '^\.(?:agents|claude|codex)/agents/' -and $line.Text -cmatch '^\s*model\s*[:=]') { continue }
		foreach ($match in $script:ModelNamePattern.Matches($line.Text)) { $failures.Add([ordered]@{ path = $line.Path; line = $line.Line; term = $match.Value }) }
	}
	$status = if ($failures.Count -gt 0) { 'fail' } else { 'pass' }
	return New-CheckRow 'model-names' $true $status ([ordered]@{ failures = [object[]] $failures.ToArray() })
}

function Invoke-CommentValueCheck([object] $Inventory) {
	$paths = Get-ChangedPath $Inventory { param($entry) $entry.class -cin @('cpp', 'dual-language-header', 'glsl') }
	if ($paths.Count -eq 0) { return New-CheckRow 'comment-values' $false 'skipped' $null }
	$route = Get-AddedLineRoute $Inventory $paths
	$added = [Collections.Generic.List[object]]::new()
	# Every line of an untracked path is added.
	foreach ($path in $route.Untracked) {
		$number = 0
		foreach ($line in ((Get-HeadText $path) -split "`r`n|`n|`r")) {
			$number++
			$added.Add([pscustomobject] @{ Path = $path; Line = $number; Text = $line })
		}
	}
	$diffArguments = @($Inventory.baselineSha)
	if (-not [string]::IsNullOrEmpty($script:HeadSha)) { $diffArguments += $script:HeadSha }
	# An empty pathspec would diff every path.
	if ($route.Pathspec.Count -gt 0) { $added.AddRange((Get-AddedLine $diffArguments ([string[]] $route.Pathspec))) }
	if ($route.CoretidePathspec.Count -gt 0) {
		$coretideDiffArguments = @($script:Coretide.BaselineSha)
		if (-not [string]::IsNullOrEmpty($script:Coretide.HeadSha)) { $coretideDiffArguments += $script:Coretide.HeadSha }
		foreach ($line in (Get-AddedLine $coretideDiffArguments ([string[]] $route.CoretidePathspec) $script:Coretide.Root)) {
			$added.Add([pscustomobject] @{ Path = $script:CoretidePrefix + $line.Path; Line = $line.Line; Text = $line.Text })
		}
	}
	$failures = [Collections.Generic.List[object]]::new()
	foreach ($line in $added) {
		# The token pattern keeps a `//` inside a literal out of the comment group.
		foreach ($token in [regex]::Matches($line.Text, $script:CppTokenPattern)) {
			if (-not $token.Groups['comment'].Success -or -not $token.Value.StartsWith('//')) { continue }
			foreach ($match in $script:CommentValuePattern.Matches($token.Value)) { $failures.Add([ordered]@{ path = $line.Path; line = $line.Line; term = $match.Value }) }
		}
	}
	$status = if ($failures.Count -gt 0) { 'fail' } else { 'pass' }
	return New-CheckRow 'comment-values' $true $status ([ordered]@{ failures = [object[]] $failures.ToArray() })
}

function Split-WorkflowIncidentsRow([string] $Line) {
	$trimmed = $Line.Trim()
	if ($trimmed.Length -lt 2 -or -not $trimmed.StartsWith('|') -or -not $trimmed.EndsWith('|')) { return $null }
	$rawCells = $trimmed.Substring(1, $trimmed.Length - 2).Split('|')
	$cells = [string[]]::new($rawCells.Length)
	for ($c = 0; $c -lt $rawCells.Length; $c++) { $cells[$c] = $rawCells[$c].Trim() }
	return , $cells
}

function Invoke-WorkflowIncidentsCheck([object] $Inventory) {
	$paths = Get-ChangedPath $Inventory { param($entry) $entry.path -ceq $script:WorkflowIncidentsPath }
	if ($paths.Count -eq 0) { return New-CheckRow 'workflow-incidents' $false 'skipped' $null }
	$lines = @((Get-HeadText $script:WorkflowIncidentsPath) -split "`r`n|`n|`r")
	$failures = [Collections.Generic.List[object]]::new()
	$rowCount = 0

	$entriesIndex = -1
	for ($i = 0; $i -lt $lines.Length; $i++) {
		if ($lines[$i] -cmatch '^## Entries$') { $entriesIndex = $i; break }
	}
	if ($entriesIndex -lt 0) {
		$failures.Add([ordered]@{ path = $script:WorkflowIncidentsPath; line = $null; problem = 'missing ## Entries heading' })
		return New-CheckRow 'workflow-incidents' $true 'fail' ([ordered]@{ rowCount = 0; failures = [object[]] $failures.ToArray() })
	}

	$headerIndex = $entriesIndex + 1
	while ($headerIndex -lt $lines.Length -and [string]::IsNullOrWhiteSpace($lines[$headerIndex])) { $headerIndex++ }
	if ($headerIndex -ge $lines.Length) {
		$failures.Add([ordered]@{ path = $script:WorkflowIncidentsPath; line = ($entriesIndex + 1); problem = 'missing entries table header' })
		return New-CheckRow 'workflow-incidents' $true 'fail' ([ordered]@{ rowCount = 0; failures = [object[]] $failures.ToArray() })
	}

	$expectedHeaders = @('Owner', 'Symptom', 'Date', 'Session branch', 'Evidence')
	$headerCells = Split-WorkflowIncidentsRow $lines[$headerIndex]
	$headerValid = $null -ne $headerCells -and $headerCells.Length -eq 5
	if ($headerValid) {
		for ($c = 0; $c -lt 5; $c++) {
			if ($headerCells[$c] -cne $expectedHeaders[$c]) { $headerValid = $false; break }
		}
	}
	if (-not $headerValid) {
		$failures.Add([ordered]@{ path = $script:WorkflowIncidentsPath; line = ($headerIndex + 1); problem = 'unexpected entries table header' })
		return New-CheckRow 'workflow-incidents' $true 'fail' ([ordered]@{ rowCount = 0; failures = [object[]] $failures.ToArray() })
	}

	$sepIndex = $headerIndex + 1
	if ($sepIndex -ge $lines.Length) {
		$failures.Add([ordered]@{ path = $script:WorkflowIncidentsPath; line = ($headerIndex + 1); problem = 'missing entries table separator' })
		return New-CheckRow 'workflow-incidents' $true 'fail' ([ordered]@{ rowCount = 0; failures = [object[]] $failures.ToArray() })
	}
	$sepCells = Split-WorkflowIncidentsRow $lines[$sepIndex]
	$sepValid = $null -ne $sepCells -and $sepCells.Length -eq 5
	if ($sepValid) {
		foreach ($cell in $sepCells) {
			if ($cell -cnotmatch '^:?-+:?$') { $sepValid = $false; break }
		}
	}
	if (-not $sepValid) {
		$failures.Add([ordered]@{ path = $script:WorkflowIncidentsPath; line = ($sepIndex + 1); problem = 'unexpected entries table separator' })
		return New-CheckRow 'workflow-incidents' $true 'fail' ([ordered]@{ rowCount = 0; failures = [object[]] $failures.ToArray() })
	}

	$seen = [Collections.Generic.Dictionary[string, int]]::new([StringComparer]::Ordinal)
	for ($i = $sepIndex + 1; $i -lt $lines.Length; $i++) {
		$line = $lines[$i]
		if ($line -cmatch '^## ') { break }
		if ($line -notmatch '^\s*\|') { continue }
		$number = $i + 1
		$rowCount++
		$cells = Split-WorkflowIncidentsRow $line
		if ($null -eq $cells -or $cells.Length -ne 5) {
			$failures.Add([ordered]@{ path = $script:WorkflowIncidentsPath; line = $number; problem = 'expected 5 table cells' })
			continue
		}
		$owner = $cells[0]
		$symptom = $cells[1]
		$branch = $cells[3]
		$ownerKey = $owner.Trim().Trim('`').Trim().ToLowerInvariant()
		$symptomKey = ($symptom.Trim() -replace '\s+', ' ').ToLowerInvariant()
		$branchKey = $branch.Trim().Trim('`').Trim().ToLowerInvariant()
		$key = "$ownerKey`0$symptomKey`0$branchKey"
		if ($seen.ContainsKey($key)) {
			$failures.Add([ordered]@{ path = $script:WorkflowIncidentsPath; line = $number; firstLine = $seen[$key]; owner = $owner; symptom = $symptom; sessionBranch = $branch })
		}
		else {
			$seen[$key] = $number
		}
	}

	$status = if ($failures.Count -gt 0) { 'fail' } else { 'pass' }
	return New-CheckRow 'workflow-incidents' $true $status ([ordered]@{ rowCount = $rowCount; failures = [object[]] $failures.ToArray() })
}

function Test-VcxprojAffinityTriggerPath([string] $Path, [string[]] $GameProjects) {
	if ([string]::IsNullOrEmpty($Path)) { return $false }
	$sweepPaths = @(
		'.agents/skills/update-vcxproj/scripts/Invoke-VcxprojPairCheck.ps1',
		'.agents/scripts/SourceAffinity.psm1',
		'.agents/scripts/AgentScriptCommon.psm1'
	)
	if ($sweepPaths -icontains $Path) { return $true }
	$lower = $Path.ToLowerInvariant()
	$isCppSource = $lower.EndsWith('.h') -or $lower.EndsWith('.cpp')
	if ($isCppSource) {
		foreach ($prefix in @('Engine/Source/', 'Engine/Data/Shaders/', 'Common/')) {
			if ($Path.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { return $true }
		}
	}
	foreach ($name in $GameProjects) {
		$projectBase = "Projects/$name/Platforms/VisualStudio2026/"
		if ($Path -ieq "$projectBase$name.vcxproj" -or
			$Path -ieq "$projectBase$name.vcxproj.filters" -or
			$Path -ieq "$projectBase${name}Server.vcxproj" -or
			$Path -ieq "$projectBase${name}Server.vcxproj.filters") {
			return $true
		}
		if ($isCppSource -and $Path.StartsWith("Projects/$name/", [StringComparison]::OrdinalIgnoreCase)) {
			return $true
		}
	}
	return $false
}

function Convert-ToNonNegativeInt([object] $Value) {
	if ($null -eq $Value) { return $null }
	if ($Value -is [int] -or $Value -is [long] -or $Value -is [int16] -or $Value -is [byte]) {
		if ($Value -ge 0 -and $Value -le [int]::MaxValue) {
			return [int] $Value
		}
	}
	return $null
}

function Get-BaselineGameProjects() {
	$names = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
	$projectDirs = @((Invoke-StaticCheckGit @('ls-tree', '-d', '--name-only', '-z', $Baseline, 'Projects/')) -split "`0" |
		Where-Object { -not [string]::IsNullOrEmpty($_) -and $_.StartsWith('Projects/', [StringComparison]::OrdinalIgnoreCase) } |
		ForEach-Object { $_.Substring('Projects/'.Length) })
	if ($projectDirs.Count -gt 0) {
		$candidatePaths = [Collections.Generic.List[string]]::new()
		foreach ($name in $projectDirs) {
			$candidatePaths.Add("Projects/$name/Platforms/VisualStudio2026/$name.vcxproj")
			$candidatePaths.Add("Projects/$name/Platforms/VisualStudio2026/${name}Server.vcxproj")
		}
		$present = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
		foreach ($line in ((Invoke-StaticCheckGit (@('ls-tree', '-r', '--name-only', '-z', $Baseline, '--') + $candidatePaths.ToArray())) -split "`0")) {
			if (-not [string]::IsNullOrEmpty($line)) { [void] $present.Add($line) }
		}
		foreach ($name in $projectDirs) {
			if ($present.Contains("Projects/$name/Platforms/VisualStudio2026/$name.vcxproj") -and
				$present.Contains("Projects/$name/Platforms/VisualStudio2026/${name}Server.vcxproj")) {
				[void] $names.Add($name)
			}
		}
	}
	if ($null -ne $script:Coretide -and -not [string]::IsNullOrWhiteSpace($script:Coretide.BaselineSha)) {
		$coretidePresent = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
		foreach ($line in ((Invoke-StaticCheckGit @('ls-tree', '-r', '--name-only', '-z', $script:Coretide.BaselineSha, '--', 'Platforms/VisualStudio2026/Coretide.vcxproj', 'Platforms/VisualStudio2026/CoretideServer.vcxproj') -Root $script:Coretide.Root) -split "`0")) {
			if (-not [string]::IsNullOrEmpty($line)) { [void] $coretidePresent.Add($line) }
		}
		if ($coretidePresent.Contains('Platforms/VisualStudio2026/Coretide.vcxproj') -and
			$coretidePresent.Contains('Platforms/VisualStudio2026/CoretideServer.vcxproj')) {
			[void] $names.Add('Coretide')
		}
	}
	return @($names)
}

function Invoke-VcxprojAffinityCheck([object] $Inventory) {
	$triggerSet = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
	if ([IO.Directory]::Exists((Join-Path $script:Root 'Projects'))) {
		foreach ($name in @(Get-AgentGameProjects $script:Root)) { [void] $triggerSet.Add($name) }
	}
	foreach ($name in @(Get-BaselineGameProjects)) { [void] $triggerSet.Add($name) }
	[string[]] $gameProjects = @($triggerSet)
	if ($gameProjects.Count -eq 0) { return New-CheckRow 'vcxproj-affinity' $false 'skipped' $null }
	$triggered = $false
	foreach ($entry in $Inventory.entries) {
		if ((Test-VcxprojAffinityTriggerPath $entry.path $gameProjects) -or
			(Test-VcxprojAffinityTriggerPath $entry.oldPath $gameProjects)) {
			$triggered = $true
			break
		}
	}
	if (-not $triggered) { return New-CheckRow 'vcxproj-affinity' $false 'skipped' $null }

	$run = Invoke-StaticCheckProcess (Get-StaticCheckShell) @(
		'-NoProfile', '-NonInteractive', '-File', $script:VcxprojPairCheckScript,
		'-RepositoryRoot', $script:Root, '-AllGameProjects'
	) $script:Root

	$parsed = $null
	if (-not [string]::IsNullOrWhiteSpace($run.Stdout)) {
		try { $parsed = $run.Stdout | ConvertFrom-Json -Depth 16 }
		catch { $parsed = $null }
	}
	$hasStructure = $null -ne $parsed -and
		$null -ne $parsed.PSObject.Properties['pairs'] -and
		$null -ne $parsed.pairs -and
		$null -ne $parsed.pairs.PSObject.Properties['totalCount'] -and
		$null -ne $parsed.PSObject.Properties['violations'] -and
		$null -ne $parsed.violations -and
		$null -ne $parsed.violations.PSObject.Properties['totalCount'] -and
		$null -ne $parsed.violations.PSObject.Properties['omittedCount'] -and
		$null -ne $parsed.violations.PSObject.Properties['items']

	$pairCount = $null
	$violationTotalCount = $null
	$violationOmittedCount = $null
	if ($hasStructure) {
		$pairCount = Convert-ToNonNegativeInt $parsed.pairs.totalCount
		$violationTotalCount = Convert-ToNonNegativeInt $parsed.violations.totalCount
		$violationOmittedCount = Convert-ToNonNegativeInt $parsed.violations.omittedCount
	}
	$hasValidCounts = $null -ne $pairCount -and $null -ne $violationTotalCount -and $null -ne $violationOmittedCount

	$items = [object[]] @()
	if ($hasStructure -and $null -ne $parsed.violations.items) {
		$items = [object[]] @($parsed.violations.items)
	}

	if (($run.ExitCode -eq 0 -or $run.ExitCode -eq 2) -and $hasValidCounts) {
		$status = if ($run.ExitCode -eq 0) { 'pass' } else { 'fail' }
		return New-CheckRow 'vcxproj-affinity' $true $status ([ordered]@{
			pairCount = $pairCount
			violationTotalCount = $violationTotalCount
			violationOmittedCount = $violationOmittedCount
			failures = $items
		})
	}

	$hasStructuredError = ($run.ExitCode -ne 0 -and $run.ExitCode -ne 2) -and
		$null -ne $parsed -and
		$null -ne $parsed.PSObject.Properties['code'] -and
		-not [string]::IsNullOrWhiteSpace([string] $parsed.code)
	$blockedCode = if ($hasStructuredError) { [string] $parsed.code } else { 'unparseable-output' }
	$blockedMessage = if ($hasStructuredError -and $null -ne $parsed.PSObject.Properties['message'] -and -not [string]::IsNullOrWhiteSpace([string] $parsed.message)) {
		[string] $parsed.message
	}
	elseif (-not $hasValidCounts -and $hasStructure) {
		'Invoke-VcxprojPairCheck.ps1 returned invalid count values.'
	}
	elseif (-not [string]::IsNullOrWhiteSpace($run.Stderr)) {
		$run.Stderr.Trim()
	}
	else {
		'Invoke-VcxprojPairCheck.ps1 returned unparseable output.'
	}
	$blockedDetail = [ordered]@{
		pairCount = if ($null -ne $pairCount) { $pairCount } else { 0 }
		violationTotalCount = if ($null -ne $violationTotalCount) { $violationTotalCount } else { 0 }
		violationOmittedCount = if ($null -ne $violationOmittedCount) { $violationOmittedCount } else { 0 }
		failures = $items
		exitCode = $run.ExitCode
		code = $blockedCode
		message = $blockedMessage
	}
	return New-CheckRow 'vcxproj-affinity' $true 'blocked' $blockedDetail
}

function Test-ReviewRoundScenarioTriggerPath([string] $Path) {
	if ([string]::IsNullOrEmpty($Path)) { return $false }
	# The scripts the cases run; the modules those scripts import are not triggers.
	$scenarioScripts = @(
		'.agents/scripts/Get-ChangeWorkflowSteps.ps1',
		'.agents/scripts/ReviewRounds.psm1',
		'.agents/scripts/SessionChangeInventory.psm1',
		'.agents/scripts/Get-SessionChangeInventory.ps1',
		'.agents/skills/sweep/scripts/Invoke-Sweep.ps1',
		'.agents/skills/finalize-changes/scripts/Invoke-FinalizeCoretide.ps1'
	)
	if ($scenarioScripts -icontains $Path) { return $true }
	return $Path.StartsWith('.agents/scripts/ReviewRoundScenarios/', [StringComparison]::OrdinalIgnoreCase)
}

function Invoke-ReviewRoundScenarioCheck([object] $Inventory) {
	$triggered = $false
	foreach ($entry in $Inventory.entries) {
		if ((Test-ReviewRoundScenarioTriggerPath $entry.path) -or
			(Test-ReviewRoundScenarioTriggerPath $entry.oldPath)) {
			$triggered = $true
			break
		}
	}
	if (-not $triggered) { return New-CheckRow 'review-round-scenarios' $false 'skipped' $null }

	$cases = @(Get-ChildItem -LiteralPath (Split-Path $script:ScenarioCommonScript) -File |
		Where-Object { $_.Extension -ieq '.ps1' -and $_.Name -ine 'ScenarioCommon.ps1' } | Sort-Object Name)
	$failures = [Collections.Generic.List[object]]::new()
	$scratch = Join-Path $script:Root "Temp/review-round-scenarios-$([guid]::NewGuid().ToString('N'))"
	try { [void] [IO.Directory]::CreateDirectory($scratch) }
	catch {
		$failures.Add([ordered]@{ path = $scratch; message = $_.Exception.Message })
		return New-CheckRow 'review-round-scenarios' $true 'blocked' ([ordered]@{ caseCount = $cases.Count; passCount = 0; failures = [object[]] $failures.ToArray() })
	}
	$runs = [Collections.Generic.List[object]]::new()
	try {
		# Every case starts before the row waits on any, so the run takes as long as its slowest case.
		foreach ($case in $cases) {
			$directory = Join-Path $scratch $case.BaseName
			[void] [IO.Directory]::CreateDirectory($directory)
			$stdout = Join-Path $directory 'stdout.txt'
			# Start-Process joins ArgumentList with spaces, so each path is quoted here.
			$process = Start-Process (Get-StaticCheckShell) -PassThru -NoNewWindow -WorkingDirectory $script:Root `
				-Environment @{ GIT_OPTIONAL_LOCKS = '0' } -RedirectStandardOutput $stdout -RedirectStandardError (Join-Path $directory 'stderr.txt') `
				-ArgumentList @('-NoProfile', '-NonInteractive', '-File', "`"$($case.FullName)`"", '-ScratchRoot', "`"$directory`"")
			$runs.Add([pscustomobject] @{ Name = $case.Name; Process = $process; Stdout = $stdout; TimedOut = $false })
		}
		foreach ($run in $runs) {
			$remaining = [math]::Max(0, $script:ReviewRoundScenarioDeadlineSeconds * 1000 - $script:RunClock.ElapsedMilliseconds)
			if ($run.Process.WaitForExit([int] $remaining)) { continue }
			$run.TimedOut = $true
			$run.Process.Kill($true)
			$run.Process.WaitForExit()
		}
		# A case that exits non-zero or is stopped is blocked whatever FAIL lines it printed.
		$status = 'pass'
		$passCount = 0
		foreach ($run in $runs) {
			$lines = @(Get-Content -LiteralPath $run.Stdout)
			$passCount += @($lines | Where-Object { $_.StartsWith('PASS:') }).Count
			$failLines = @($lines | Where-Object { $_.StartsWith('FAIL:') })
			foreach ($line in $failLines) { $failures.Add([ordered]@{ caseFile = $run.Name; assertion = $line.Substring('FAIL:'.Length).Trim() }) }
			if ($run.TimedOut -or $run.Process.ExitCode -ne 0) {
				$exitCode = if ($run.TimedOut) { 'timeout' } else { $run.Process.ExitCode }
				$failures.Add([ordered]@{ caseFile = $run.Name; exitCode = $exitCode })
				$status = 'blocked'
			}
			elseif ($failLines.Count -gt 0 -and $status -cne 'blocked') { $status = 'fail' }
		}
	}
	finally {
		# A case left running by an exception above would hold files in the scratch directory.
		foreach ($run in $runs) {
			if ($run.Process.HasExited) { continue }
			$run.Process.Kill($true)
			$run.Process.WaitForExit()
		}
		# A scratch directory that cannot be removed is a run that could not complete, so the row reports blocked.
		try { Remove-Item -LiteralPath $scratch -Recurse -Force }
		catch {
			$failures.Add([ordered]@{ path = $scratch; message = $_.Exception.Message })
			$status = 'blocked'
		}
	}
	return New-CheckRow 'review-round-scenarios' $true $status ([ordered]@{ caseCount = $cases.Count; passCount = $passCount; failures = [object[]] $failures.ToArray() })
}

try {
	$script:Root = Get-AgentCanonicalPath $RepositoryRoot
	if (-not (Test-Path -LiteralPath $script:Root -PathType Container)) {
		Complete-StaticChecks 2 'blocked' 'static-checks.repository-root-invalid' "-RepositoryRoot must be an existing directory: '$RepositoryRoot'."
	}
	foreach ($composed in @($script:InventoryModule, $script:ValidateSkillScript, $script:VcxprojPairCheckScript, $script:ScenarioCommonScript)) {
		if (-not (Test-Path -LiteralPath $composed -PathType Leaf)) {
			Complete-StaticChecks 2 'blocked' 'static-checks.script-missing' "A composed script is missing: '$composed'."
		}
	}
	Import-Module $script:InventoryModule -Force
	$inventory = Get-InventoryDocument
	$script:HeadSha = if ([string]::IsNullOrWhiteSpace($inventory.headSha)) { '' } else { $inventory.headSha }
	if ($null -ne $inventory.coretide) {
		$script:Coretide = @{
			Root = Get-AgentCoretideRepository $script:Root
			BaselineSha = $inventory.coretide.baselineSha
			HeadSha = if ([string]::IsNullOrWhiteSpace($inventory.coretide.headSha)) { '' } else { $inventory.coretide.headSha }
			HeadPaths = $null
		}
	}

	# All ten rows are always present, triggered or not.
	$checks = @(
		(Invoke-ValidateSkillCheck $inventory)
		(Invoke-MarkdownLinkCheck $inventory)
		(Invoke-FileHygieneCheck $inventory)
		(Invoke-LineLengthCheck $inventory)
		(Invoke-InstructionSizeCheck $inventory)
		(Invoke-ModelNameCheck $inventory)
		(Invoke-WorkflowIncidentsCheck $inventory)
		(Invoke-CommentValueCheck $inventory)
		(Invoke-VcxprojAffinityCheck $inventory)
		(Invoke-ReviewRoundScenarioCheck $inventory)
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
