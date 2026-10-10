# The static checks for the Run targeted pre-review checks
# step, selected from the session change inventory and run in one pass: `validate-skill`
# for each changed skill package, `markdown-links` for every changed markdown file, and
# `file-hygiene` for every changed text file: Git's whitespace errors (`git diff --check` under the
# `.gitattributes` whitespace attribute) and space-indented GLSL on added lines, and per file invalid
# UTF-8, an added BOM, and a missing final newline the baseline side had. The
# first composes an existing script (the bundled skill validator). The
# run reports results only — it never decides whether a failing check blocks a slice, never edits a
# file, and writes nothing to disk (GIT_OPTIONAL_LOCKS=0 keeps Git from refreshing the index), so it is
# safe under a read-only sandbox. Stdout carries only the result document.
# The result document holds one `checks` row per check, each carrying a boolean `triggered` and a
# `status` (pass, fail, blocked, or skipped). The run passes (exit 0) when no triggered row fails or is
# blocked, including when none triggers; otherwise it exits 2 if any triggered row is blocked, else 1. A
# setup failure (invalid root, missing composed script, unavailable inventory) exits 2 with no rows, and
# an unexpected error exits 1 with status `error`. `-Head <commit>` selects the changed
# files from a committed head instead of the working tree, and `-IncludeUntracked` adds the untracked
# files when checking the working tree. `markdown-links` and `file-hygiene` read content from that
# commit; `validate-skill` validates the working tree's copy of each selected package.
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
$script:ValidateSkillScript = Join-Path $PSScriptRoot '../skills/external-skill-creator/scripts/Validate-Skill.ps1'
$script:Utf8 = [Text.UTF8Encoding]::new($false)
$script:Root = $null
$script:HeadSha = ''
$script:HeadPaths = $null
$script:UntrackedPaths = @()
$script:HeadingSlugs = @{}
# Inline markdown link: the target is everything up to the closing parenthesis or the optional title.
$script:LinkPattern = '\[(?:[^\[\]]*)\]\(\s*([^)\s]+)'
$script:SchemePattern = '^[A-Za-z][A-Za-z0-9+.-]*:'

$result = [ordered]@{
	schemaVersion = 'broken-engine-static-checks/v2'
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

function Invoke-StaticCheckGit([string[]] $Arguments, [switch] $RawStdout) {
	$run = Invoke-StaticCheckProcess 'git' (@('-C', $script:Root, '--no-pager') + $Arguments) $script:Root -RawStdout:$RawStdout
	if ($run.ExitCode -ne 0) { throw "git $($Arguments -join ' ') failed with exit $($run.ExitCode): $($run.Stderr.Trim())" }
	# The comma keeps a raw byte[] from being unrolled into the pipeline.
	return , $run.Stdout
}

function Get-InventoryDocument() {
	$arguments = @('-NoProfile', '-File', $script:InventoryScript, '-RepositoryRoot', $script:Root, '-Baseline', $Baseline)
	if (-not [string]::IsNullOrWhiteSpace($Head)) { $arguments += @('-Head', $Head) }
	if ($IncludeUntracked) {
		# The inventory reports an untracked path only when the caller lists it, so the pass-through
		# switch supplies the whole untracked set in the comma-separated form that script splits.
		$script:UntrackedPaths = @((Invoke-StaticCheckGit @('ls-files', '--others', '--exclude-standard', '-z')) -split "`0" | Where-Object { -not [string]::IsNullOrEmpty($_) })
		if ($script:UntrackedPaths.Count -gt 0) { $arguments += @('-IncludeUntracked', ($script:UntrackedPaths -join ',')) }
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
	if ($paths.Count -eq 0) {
		if ([bool] $Inventory.truncated) { return New-CheckRow 'validate-skill' $true 'blocked' ([ordered]@{ reason = 'The inventory truncated its entry table, so the changed skill packages could not be selected.' }) }
	}
	$results = [Collections.Generic.List[object]]::new()
	$status = 'pass'
	$passedCount = 0
	foreach ($path in $paths) {
		$run = Invoke-StaticCheckProcess (Get-StaticCheckShell) @('-NoProfile', '-File', $script:ValidateSkillScript, '-Path', (Join-Path $script:Root ($path -replace '/', [IO.Path]::DirectorySeparatorChar))) $script:Root
		# Exit 1 is the validator's ordinary INVALID result; only its exit 2 setup error blocks the check.
		$rowStatus = switch ($run.ExitCode) { 0 { 'pass' } 1 { 'fail' } default { 'blocked' } }
		if ($rowStatus -ceq 'blocked' -or ($rowStatus -ceq 'fail' -and $status -cne 'blocked')) { $status = $rowStatus }
		# Every package names itself and its exit code, but only the packages that did not pass carry the
		# validator's output; a passing package would otherwise contribute an unbounded block of lines for a
		# result its exit code already states, and a sweep runs every package.
		if ($rowStatus -ceq 'pass') { $passedCount++; $results.Add([ordered]@{ path = $path; exitCode = $run.ExitCode }); continue }
		$lines = @(($run.Stdout -split "`r`n|`n|`r") | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
		$results.Add([ordered]@{ path = $path; exitCode = $run.ExitCode; lines = [object[]] $lines })
	}
	return New-CheckRow 'validate-skill' $true $status ([ordered]@{ passedCount = $passedCount; results = [object[]] $results.ToArray() })
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

function Add-WhitespaceFailure([string[]] $Arguments, [int] $CleanExit, [int] $FindingsExit, [Collections.Generic.List[object]] $Failures) {
	$run = Invoke-StaticCheckProcess 'git' (@('-C', $script:Root, '--no-pager', '-c', 'core.quotePath=false') + $Arguments) $script:Root
	if ($run.ExitCode -ne $CleanExit -and $run.ExitCode -ne $FindingsExit) { throw "git $($Arguments -join ' ') failed with exit $($run.ExitCode): $($run.Stderr.Trim())" }
	# Each finding is one `<path>:<line>: <problem>.` line followed by the offending `+` lines.
	foreach ($line in ($run.Stdout -split "`n")) {
		if ($line.StartsWith('+') -or $line.TrimEnd("`r") -cnotmatch '^(.+):(\d+): (.+?)\.?$') { continue }
		$Failures.Add([ordered]@{ path = $Matches[1]; line = [int] $Matches[2]; problem = $Matches[3] })
	}
}

function Test-SpaceIndentedLine([string] $Text) {
	# The style-rule-1 candidate pattern and its block-comment exception (Find-SessionCandidates.ps1); a
	# tab-then-space aligned line starts with a tab and stays clear.
	return $Text -cmatch '^ +[^ ]' -and $Text -cnotmatch '^ +\*(?:\s|/|$)'
}

function Add-AddedGlslIndentFailure([string[]] $DiffArguments, [string[]] $Paths, [Collections.Generic.List[object]] $Failures) {
	$diff = Invoke-StaticCheckGit (@('-c', 'core.quotePath=false', 'diff', '-U0', '-M', '--no-color', '--no-ext-diff') + $DiffArguments + @('--') + $Paths)
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
		if ($null -ne $path -and (Test-SpaceIndentedLine $line.Substring(1))) { $Failures.Add([ordered]@{ path = $path; line = $number; problem = 'indent with spaces' }) }
		$number++
	}
}

function Invoke-FileHygieneCheck([object] $Inventory, [bool] $Truncated) {
	$failures = [Collections.Generic.List[object]]::new()
	$diffArguments = @($Inventory.baselineSha)
	if (-not [string]::IsNullOrEmpty($script:HeadSha)) { $diffArguments += $script:HeadSha }
	# Untracked paths exist only against the working tree; a commit-valued head has none.
	$untracked = @(if ([string]::IsNullOrEmpty($script:HeadSha)) { $script:UntrackedPaths })

	# Whitespace errors on added lines; -M keeps a renamed file's unchanged lines from counting as added.
	Add-WhitespaceFailure (@('diff', '--check', '-M', '--no-color') + $diffArguments) 0 2 $failures
	$nullDevice = if ($IsWindows) { 'NUL' } else { '/dev/null' }
	foreach ($path in $untracked) { Add-WhitespaceFailure @('diff', '--no-index', '--check', '--no-color', '--', $nullDevice, $path) 1 3 $failures }

	$entries = @{}
	foreach ($entry in $Inventory.entries) { if ($entry.status -cne 'D') { $entries[$entry.path] = $entry } }

	# GLSL indentation on added lines. -M pairs a rename only when the pathspec holds both of its paths.
	$glslPaths = Get-ChangedPath $Inventory { param($entry) $entry.class -cin @('glsl', 'dual-language-header') }
	$untrackedGlsl = [Collections.Generic.HashSet[string]]::new([string[]] @())
	$pathspec = [Collections.Generic.List[string]]::new()
	foreach ($path in $glslPaths) {
		if ($untracked -ccontains $path) { [void] $untrackedGlsl.Add($path); continue }
		$pathspec.Add($path)
		if ($null -ne $entries[$path].oldPath) { $pathspec.Add($entries[$path].oldPath) }
	}
	# An empty pathspec would diff every path.
	if ($pathspec.Count -gt 0) { Add-AddedGlslIndentFailure $diffArguments ([string[]] $pathspec) $failures }

	# File-level byte checks: gitlinks and symlinks carry no file bytes, and binary is Git's own decision.
	$bytePaths = Get-ChangedPath $Inventory { param($entry) $entry.class -cne 'binary' -and $entry.current.mode -cin @('100644', '100755') }
	foreach ($path in $bytePaths) {
		$entry = $entries[$path]
		# Plain assignments: an `if` expression would unroll the byte[] into the pipeline.
		$head = $null
		if ([string]::IsNullOrEmpty($script:HeadSha)) { $head = [IO.File]::ReadAllBytes((Join-Path $script:Root ($path -replace '/', [IO.Path]::DirectorySeparatorChar))) }
		else { $head = Invoke-StaticCheckGit @('cat-file', 'blob', "$($script:HeadSha):$path") -RawStdout }
		$baseline = $null
		if ($null -ne $entry.baseline) {
			$baselinePath = if ($null -ne $entry.oldPath) { $entry.oldPath } else { $path }
			$baseline = Invoke-StaticCheckGit @('cat-file', 'blob', "$($Inventory.baselineSha):$baselinePath") -RawStdout
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
		if ($null -eq $text -or -not $untrackedGlsl.Contains($path)) { continue }
		$number = 0
		foreach ($line in ($text -split "`n")) {
			$number++
			if (Test-SpaceIndentedLine $line) { $failures.Add([ordered]@{ path = $path; line = $number; problem = 'indent with spaces' }) }
		}
	}

	$detail = [ordered]@{ fileCount = $bytePaths.Count; failures = [object[]] $failures.ToArray() }
	if ($failures.Count -gt 0) { return New-CheckRow 'file-hygiene' $true 'fail' $detail }
	if ($bytePaths.Count -eq 0) {
		if (-not $Truncated) { return New-CheckRow 'file-hygiene' $false 'skipped' $null }
		return New-CheckRow 'file-hygiene' $true 'blocked' ([ordered]@{ reason = 'The inventory truncated its entry table, so the changed text files could not be selected.' })
	}
	return New-CheckRow 'file-hygiene' $true 'pass' $detail
}

try {
	$script:Root = Get-AgentCanonicalPath $RepositoryRoot
	if (-not (Test-Path -LiteralPath $script:Root -PathType Container)) {
		Complete-StaticChecks 2 'blocked' 'static-checks.repository-root-invalid' "-RepositoryRoot must be an existing directory: '$RepositoryRoot'."
	}
	foreach ($composed in @($script:InventoryScript, $script:ValidateSkillScript)) {
		if (-not (Test-Path -LiteralPath $composed -PathType Leaf)) {
			Complete-StaticChecks 2 'blocked' 'static-checks.script-missing' "A composed script is missing: '$composed'."
		}
	}
	$inventory = Get-InventoryDocument
	$script:HeadSha = if ([string]::IsNullOrWhiteSpace($inventory.headSha)) { '' } else { $inventory.headSha }
	$truncated = [bool] $inventory.truncated
	$result.truncated = $truncated

	# All three rows are always present, triggered or not.
	$checks = @(
		(Invoke-ValidateSkillCheck $inventory)
		(Invoke-MarkdownLinkCheck $inventory $truncated)
		(Invoke-FileHygieneCheck $inventory $truncated)
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
