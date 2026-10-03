# Include-order check and fix for Documents/C++StyleGuide.txt rule 47, run by /code-style-review.
# Parameters: -RepositoryRoot <absolute toplevel>, exactly one of -Path <repo-relative files> or -All, and
# -Fix. -All takes the tracked *.h/*.cpp files under Common/, DataPacker/, Engine/, Projects/ and Tools/.
# A -Path entry that does not exist or is not *.h/*.cpp is skipped, as is an excluded file; an existing
# *.h/*.cpp entry outside those five roots, or with no owning project, is an input error.
# A segment is a maximal run of #include lines and the blank lines between them; any other line ends it.
# A segment is checked at conditional depth 0 or directly inside a whole-file guard (an #if whose
# matching #endif is the last non-blank line); other segments keep their hand order and are not read.
# Groups, in order: 0 a leading "Pch.h"; 1 the corresponding header of a .cpp: the .h in the .cpp's
# directory whose name stem is the longest ordinal case-insensitive prefix of the .cpp's name stem,
# wherever it is included; then, resolving a quoted include as MSVC does (the including file's
# directory, then the owning vcxproj's AdditionalIncludeDirectories in order), 2 engine (under Engine/,
# Common/, DataPacker/ or Tools/, or an unresolved Data/ generated pack header), 3 game (under
# Projects/), 4 external (under ThirdParty/, or any angle-bracket include). Canonical form: sorted by
# group, then path compared one folder level at a time with folders before files and names ordinal
# case-insensitive; no blank line inside a group, exactly one between groups; equal keys keep their
# order.
# Violation kinds: order, blank-missing and blank-extra are fixable; unresolved is not. Without -Fix the
# script only reports. With -Fix it rewrites each file whose violations are all fixable, replacing only
# the rewritten segments' lines and keeping the BOM, line endings and final newline, and leaves every
# other file byte-identical. Stdout carries one broken-engine-include-order/v1 JSON object: status
# (pass, fail, error), message, files (scanned count), violations (path, line, kind; the rows still
# present after any rewrite), rewritten (paths), truncated (violation rows capped at 400). Exit 0 when
# no violation remains, 1 when any remains, 2 on an input error.
param(
	[Parameter(Mandatory)][string] $RepositoryRoot,
	[string[]] $Path,
	[switch] $All,
	[switch] $Fix
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'AgentScriptCommon.psm1') -Force

$script:MaximumRows = 400
$script:SourceRoots = @('Common', 'DataPacker', 'Engine', 'Projects', 'Tools')
# Aggregation and precompiled headers whose include order is load-bearing.
$script:ExcludedFiles = @(
	'Projects/BrokenEngineSandbox/Source/Pch.h'
	'DataPacker/Source/Pch.h'
	'Common/ExternalHeaders.h'
	'Common/Common.h'
	'Engine/Source/Engine.h'
)
$script:ProjectFiles = @(
	@{ Prefix = '^(?:Engine|Common|Projects/BrokenEngineSandbox)/'; Project = 'Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj' }
	@{ Prefix = '^DataPacker/'; Project = 'DataPacker/Platforms/VisualStudio2026/DataPacker.vcxproj' }
	@{ Prefix = '^Tools/(?:AgentHarness|ToolCommon)/'; Project = 'Tools/AgentHarness/Platforms/VisualStudio2026/AgentHarness.vcxproj' }
	@{ Prefix = '^Tools/WorktreeCli/'; Project = 'Tools/WorktreeCli/Platforms/VisualStudio2026/WorktreeCli.vcxproj' }
)
$script:IncludePattern = '^\s*#\s*include\s*([<"])([^>"]+)[>"]'
$script:IfPattern = '^\s*#\s*if(?:n?def)?\b'
$script:EndifPattern = '^\s*#\s*endif\b'
# Latin-1 maps every byte to one character and back, so a rewrite reproduces every untouched byte.
$script:Latin1 = [Text.Encoding]::Latin1
$script:Utf8 = [Text.UTF8Encoding]::new($false)
$script:Root = $null
$script:IncludeDirectories = @{}

$result = [ordered]@{
	schemaVersion = 'broken-engine-include-order/v1'
	status = 'error'
	message = 'Include-order check did not run.'
	files = 0
	violations = @()
	rewritten = @()
	truncated = $false
}

function Complete-IncludeOrder([int] $ExitCode, [string] $Status, [string] $Message) {
	$result.status = $Status
	$result.message = $Message
	$stream = [Console]::OpenStandardOutput()
	$bytes = $script:Utf8.GetBytes(($result | ConvertTo-Json -Depth 8 -Compress))
	$stream.Write($bytes, 0, $bytes.Length)
	$stream.Flush()
	exit $ExitCode
}

function Get-ProjectFile([string] $File) {
	foreach ($entry in $script:ProjectFiles) {
		if ($File -match $entry.Prefix) { return $entry.Project }
	}
	return $null
}

function Test-ExcludedFile([string] $File) {
	return ($script:ExcludedFiles -contains $File) -or $File -match '(?:^|/)Data/Shaders/' -or $File -match '^(?:Common/ClangTidyShims|ThirdParty)/'
}

function Get-IncludeDirectory([string] $Project) {
	if ($script:IncludeDirectories.ContainsKey($Project)) { return , $script:IncludeDirectories[$Project] }
	$projectPath = Join-Path $script:Root $Project
	$projectDirectory = Split-Path $projectPath -Parent
	# Every configuration of each first-party vcxproj carries the same list, so the first one is read.
	$match = [regex]::Match([IO.File]::ReadAllText($projectPath), '<AdditionalIncludeDirectories>([^<]*)</AdditionalIncludeDirectories>')
	if (-not $match.Success) { throw "No AdditionalIncludeDirectories in '$Project'." }
	$directories = [Collections.Generic.List[string]]::new()
	foreach ($entry in $match.Groups[1].Value -split ';') {
		if ($entry -notmatch '^\$\(ProjectDir\)') { continue }
		$directories.Add([IO.Path]::GetFullPath($projectDirectory + '\' + $entry.Substring('$(ProjectDir)'.Length)))
	}
	$script:IncludeDirectories[$Project] = [string[]] $directories.ToArray()
	return , $script:IncludeDirectories[$Project]
}

# Returns the repository-relative path the quoted include resolves to, 'outside' for a file outside the
# repository, or $null when it resolves nowhere.
function Resolve-Include([string] $File, [string] $Spelled) {
	$directories = @((Split-Path (Join-Path $script:Root $File) -Parent)) + (Get-IncludeDirectory (Get-ProjectFile $File))
	$rootPrefix = $script:Root + '\'
	foreach ($directory in $directories) {
		$candidate = [IO.Path]::GetFullPath([IO.Path]::Combine($directory, ($Spelled -replace '/', '\')))
		if (-not [IO.File]::Exists($candidate)) { continue }
		if (-not $candidate.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase)) { return 'outside' }
		return $candidate.Substring($rootPrefix.Length) -replace '\\', '/'
	}
	return $null
}

function Compare-IncludeKey([string] $Left, [string] $Right) {
	$leftParts = $Left -split '/'
	$rightParts = $Right -split '/'
	$count = [Math]::Min($leftParts.Count, $rightParts.Count)
	for ($index = 0; $index -lt $count; $index++) {
		$leftFolder = $index -lt $leftParts.Count - 1
		$rightFolder = $index -lt $rightParts.Count - 1
		if ($leftFolder -ne $rightFolder) { return $(if ($leftFolder) { -1 } else { 1 }) }
		$compare = [string]::Compare($leftParts[$index], $rightParts[$index], [StringComparison]::OrdinalIgnoreCase)
		if ($compare -ne 0) { return $compare }
	}
	return $leftParts.Count - $rightParts.Count
}

function Get-IncludeGroup([string] $File, [string] $Delimiter, [string] $Spelled, [bool] $FirstInclude, [string] $CorrespondingHeader) {
	if ($FirstInclude -and $Delimiter -eq '"' -and $Spelled -eq 'Pch.h') { return 0 }
	if ($Delimiter -eq '<') { return 4 }
	$resolved = Resolve-Include $File $Spelled
	if ($null -eq $resolved) { return $(if ($Spelled -match '^Data/') { 2 } else { $null }) }
	if ($CorrespondingHeader -and $resolved -ieq $CorrespondingHeader) { return 1 }
	if ($resolved -match '^(?:Engine|Common|DataPacker|Tools)/') { return 2 }
	if ($resolved -match '^Projects/') { return 3 }
	if ($resolved -match '^ThirdParty/') { return 4 }
	return $null
}

function Test-IncludeFile([string] $File) {
	$fullPath = Join-Path $script:Root $File
	$bytes = [IO.File]::ReadAllBytes($fullPath)
	$bom = $bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF
	$offset = if ($bom) { 3 } else { 0 }
	$text = $script:Latin1.GetString($bytes, $offset, $bytes.Length - $offset)
	$newline = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
	$lines = $text.Split([string[]] @($newline), [StringSplitOptions]::None)

	$lastNonBlank = $lines.Count - 1
	while ($lastNonBlank -ge 0 -and $lines[$lastNonBlank].Trim().Length -eq 0) { $lastNonBlank-- }
	$guardOpen = -1
	$depth = [int[]]::new($lines.Count)
	$open = [Collections.Generic.Stack[int]]::new()
	for ($index = 0; $index -lt $lines.Count; $index++) {
		$depth[$index] = $open.Count
		if ($lines[$index] -match $script:IfPattern) { $open.Push($index) }
		elseif ($lines[$index] -match $script:EndifPattern -and $open.Count -gt 0) {
			$opener = $open.Pop()
			if ($index -eq $lastNonBlank -and $open.Count -eq 0) { $guardOpen = $opener }
		}
	}

	$correspondingHeader = ''
	if ($File -match '\.cpp$') {
		$stem = [IO.Path]::GetFileNameWithoutExtension($File)
		$bestLength = 0
		foreach ($header in [IO.Directory]::EnumerateFiles((Split-Path $fullPath -Parent), '*.h')) {
			$headerStem = [IO.Path]::GetFileNameWithoutExtension($header)
			if ([IO.Path]::GetExtension($header) -cne '.h' -or $headerStem.Length -le $bestLength) { continue }
			if (-not $stem.StartsWith($headerStem, [StringComparison]::OrdinalIgnoreCase)) { continue }
			$bestLength = $headerStem.Length
			$correspondingHeader = ($File -replace '[^/]+$', '') + [IO.Path]::GetFileName($header)
		}
	}

	$violations = [Collections.Generic.List[object]]::new()
	$replacements = @{}
	$fixable = $true
	$firstInclude = -1
	$index = 0
	while ($index -lt $lines.Count) {
		if ($lines[$index] -notmatch $script:IncludePattern) { $index++; continue }
		if ($firstInclude -lt 0) { $firstInclude = $index }
		$start = $index
		$end = $index
		for ($next = $index + 1; $next -lt $lines.Count; $next++) {
			if ($lines[$next] -match $script:IncludePattern) { $end = $next }
			elseif ($lines[$next].Trim().Length -ne 0) { break }
		}
		$index = $end + 1
		$checked = $depth[$start] -eq 0 -or ($depth[$start] -eq 1 -and $guardOpen -ge 0 -and $start -gt $guardOpen)
		if (-not $checked) { continue }

		$items = [Collections.Generic.List[object]]::new()
		$unresolved = $false
		for ($line = $start; $line -le $end; $line++) {
			$match = [regex]::Match($lines[$line], $script:IncludePattern)
			if (-not $match.Success) { continue }
			$group = Get-IncludeGroup $File $match.Groups[1].Value $match.Groups[2].Value ($line -eq $firstInclude) $correspondingHeader
			if ($null -eq $group) {
				$violations.Add([ordered]@{ path = $File; line = $line + 1; kind = 'unresolved' })
				$unresolved = $true
			}
			$items.Add([pscustomobject] @{ Group = $group; Key = $match.Groups[2].Value; Line = $line; Order = $items.Count })
		}
		if ($unresolved) {
			$fixable = $false
			continue
		}

		$sorted = [Collections.Generic.List[object]]::new($items)
		$sorted.Sort([Comparison[object]] {
			param($left, $right)
			if ($left.Group -ne $right.Group) { return $left.Group - $right.Group }
			$compare = Compare-IncludeKey $left.Key $right.Key
			if ($compare -ne 0) { return $compare }
			return $left.Order - $right.Order
		})
		$segmentViolations = [Collections.Generic.List[object]]::new()
		for ($position = 0; $position -lt $items.Count; $position++) {
			if ($items[$position].Order -ne $sorted[$position].Order) {
				$segmentViolations.Add([ordered]@{ path = $File; line = $items[$position].Line + 1; kind = 'order' })
				break
			}
		}
		for ($position = 1; $position -lt $items.Count; $position++) {
			$previous = $items[$position - 1]
			$current = $items[$position]
			$blanks = $current.Line - $previous.Line - 1
			if ($previous.Group -ne $current.Group -and $blanks -eq 0) {
				$segmentViolations.Add([ordered]@{ path = $File; line = $current.Line + 1; kind = 'blank-missing' })
			}
			elseif (($previous.Group -eq $current.Group -and $blanks -gt 0) -or $blanks -gt 1) {
				$extraLine = if ($previous.Group -eq $current.Group) { $previous.Line + 2 } else { $previous.Line + 3 }
				$segmentViolations.Add([ordered]@{ path = $File; line = $extraLine; kind = 'blank-extra' })
			}
		}
		if ($segmentViolations.Count -eq 0) { continue }
		$violations.AddRange($segmentViolations)
		$canonical = [Collections.Generic.List[string]]::new()
		for ($position = 0; $position -lt $sorted.Count; $position++) {
			if ($position -gt 0 -and $sorted[$position - 1].Group -ne $sorted[$position].Group) { $canonical.Add('') }
			$canonical.Add($lines[$sorted[$position].Line])
		}
		$replacements[$start] = [pscustomobject] @{ End = $end; Lines = $canonical }
	}

	$newBytes = $null
	if ($fixable -and $replacements.Count -gt 0) {
		$output = [Collections.Generic.List[string]]::new()
		$index = 0
		while ($index -lt $lines.Count) {
			if ($replacements.ContainsKey($index)) {
				$output.AddRange($replacements[$index].Lines)
				$index = $replacements[$index].End + 1
				continue
			}
			$output.Add($lines[$index])
			$index++
		}
		$body = $script:Latin1.GetBytes([string]::Join($newline, $output))
		$newBytes = if ($bom) { [byte[]] (@(0xEF, 0xBB, 0xBF) + $body) } else { $body }
	}
	return [pscustomobject] @{ Violations = $violations; NewBytes = $newBytes }
}

try {
	$script:Root = Get-AgentCanonicalPath $RepositoryRoot
	if (-not (Test-Path -LiteralPath $script:Root -PathType Container)) {
		Complete-IncludeOrder 2 'error' "-RepositoryRoot must be an existing directory: '$RepositoryRoot'."
	}
	if ($PSBoundParameters.ContainsKey('Path') -eq [bool] $All) {
		Complete-IncludeOrder 2 'error' 'Pass exactly one of -Path or -All.'
	}

	$files = [Collections.Generic.List[string]]::new()
	if ($All) {
		$run = Invoke-AgentProcess 'git' (@('-C', $script:Root, 'ls-files', '-z', '--') + $script:SourceRoots) $script:Root
		if ($run.ExitCode -ne 0) { Complete-IncludeOrder 2 'error' "git ls-files failed: $($run.Stderr.Trim())" }
		foreach ($file in $run.Stdout -split "`0") {
			if ($file -cmatch '\.(?:h|cpp)$' -and -not (Test-ExcludedFile $file)) { $files.Add($file) }
		}
	}
	else {
		foreach ($entry in $Path) {
			$file = ($entry -replace '\\', '/') -replace '^\./', ''
			if ($file -cnotmatch '\.(?:h|cpp)$' -or -not [IO.File]::Exists([IO.Path]::Combine($script:Root, $file))) { continue }
			$inRoot = $false
			foreach ($sourceRoot in $script:SourceRoots) { if ($file.StartsWith("$sourceRoot/", [StringComparison]::OrdinalIgnoreCase)) { $inRoot = $true } }
			if (-not $inRoot) {
				Complete-IncludeOrder 2 'error' "-Path entries must be repository-relative files under $($script:SourceRoots -join '/, ')/: '$entry'."
			}
			if ((Test-ExcludedFile $file) -or $files.Contains($file)) { continue }
			$files.Add($file)
		}
	}
	foreach ($file in $files) {
		if ($null -eq (Get-ProjectFile $file)) { Complete-IncludeOrder 2 'error' "No owning vcxproj for '$file'." }
	}

	$violations = [Collections.Generic.List[object]]::new()
	$rewritten = [Collections.Generic.List[string]]::new()
	foreach ($file in $files) {
		$fileResult = Test-IncludeFile $file
		if ($Fix -and $null -ne $fileResult.NewBytes) {
			[IO.File]::WriteAllBytes((Join-Path $script:Root $file), $fileResult.NewBytes)
			$rewritten.Add($file)
			continue
		}
		$violations.AddRange($fileResult.Violations)
	}

	$result.files = $files.Count
	$result.violations = [object[]] @($violations | Select-Object -First $script:MaximumRows)
	$result.rewritten = [string[]] $rewritten.ToArray()
	$result.truncated = $violations.Count -gt $script:MaximumRows
	$violationFiles = @($violations | ForEach-Object { $_.path } | Select-Object -Unique).Count
	$message = "Scanned $($files.Count) file(s): $($violations.Count) violation(s) in $violationFiles file(s); rewrote $($rewritten.Count) file(s)."
	if ($violations.Count -gt 0) { Complete-IncludeOrder 1 'fail' $message }
	Complete-IncludeOrder 0 'pass' $message
}
catch {
	Complete-IncludeOrder 2 'error' $_.Exception.Message
}
