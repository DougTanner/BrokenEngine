Set-StrictMode -Version Latest

# Exports are Agent-prefixed so they can never shadow a host script's local Invoke-Git/Get-CanonicalPath
# when this module is imported by a script invoked in-process from that host (same convention as
# FinalizeWorkflowCommon's Finalize-prefixed exports).

function Get-AgentCanonicalPath([string] $Path) {
	$full = [System.IO.Path]::GetFullPath($Path)
	$root = [System.IO.Path]::GetPathRoot($full)
	if ($full.Length -gt $root.Length) { return $full.TrimEnd('\', '/') }
	return $full
}

function Invoke-AgentGit([string[]] $Arguments) {
	$output = @(& git @Arguments 2>&1)
	if ($LASTEXITCODE -ne 0) { throw "git $($Arguments -join ' ') failed: $($output -join '; ')" }
	return $output
}

# Runs a child process with captured UTF-8 stdout/stderr and GIT_OPTIONAL_LOCKS=0; the other scripts keep
# their own ProcessStartInfo runners until they are next changed.
function Invoke-AgentProcess([string] $FileName, [string[]] $Arguments, [string] $WorkingDirectory) {
	$utf8 = [Text.UTF8Encoding]::new($false)
	$start = [Diagnostics.ProcessStartInfo]::new()
	$start.FileName = $FileName
	$start.WorkingDirectory = $WorkingDirectory
	$start.UseShellExecute = $false
	$start.CreateNoWindow = $true
	$start.RedirectStandardOutput = $true
	$start.RedirectStandardError = $true
	$start.StandardOutputEncoding = $utf8
	$start.StandardErrorEncoding = $utf8
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

function Get-AgentNormalizedText([byte[]] $Bytes) {
	$offset = 0
	if ($Bytes.Length -ge 3 -and $Bytes[0] -eq 0xEF -and $Bytes[1] -eq 0xBB -and $Bytes[2] -eq 0xBF)
	{
		$offset = 3
	}

	$strictUtf8 = [Text.UTF8Encoding]::new($false, $true)
	$text = $strictUtf8.GetString($Bytes, $offset, $Bytes.Length - $offset)
	return $text.Replace("`r`n", "`n").Replace("`r", "`n")
}

function Measure-AgentTokenCount([string] $Text) {
	$byteCount = [Text.UTF8Encoding]::new($false).GetByteCount($Text)
	return [int64](($byteCount + 3) -shr 2)
}

function Get-AgentMarkdownSearchText
{
	param([string] $Body)

	$characters = $Body.ToCharArray()
	$inFence = $false
	$fenceCharacter = [char] 0
	$fenceLength = 0
	$offset = 0
	while ($offset -lt $Body.Length)
	{
		$newline = $Body.IndexOf("`n", $offset)
		$lineEnd = if ($newline -lt 0) { $Body.Length } else { $newline }
		$line = $Body.Substring($offset, $lineEnd - $offset)
		$maskLine = $inFence
		if ($inFence)
		{
			$closingPattern = '^ {{0,3}}{0}{{{1},}}[ \t]*$' -f [regex]::Escape([string] $fenceCharacter), $fenceLength
			if ($line -match $closingPattern)
			{
				$inFence = $false
			}
		}
		else
		{
			$fenceMatch = [regex]::Match($line, '^ {0,3}(?<fence>`{3,}|~{3,})')
			if ($fenceMatch.Success)
			{
				$fence = $fenceMatch.Groups['fence'].Value
				$fenceCharacter = $fence[0]
				$fenceLength = $fence.Length
				$inFence = $true
				$maskLine = $true
			}
		}
		if ($maskLine)
		{
			for ($i = $offset; $i -lt $lineEnd; $i++)
			{
				$characters[$i] = ' '
			}
		}
		$offset = if ($newline -lt 0) { $Body.Length } else { $newline + 1 }
	}

	for ($i = 0; $i -lt $characters.Length; $i++)
	{
		if ($characters[$i] -ne '`')
		{
			continue
		}
		$openingStart = $i
		while ($i -lt $characters.Length -and $characters[$i] -eq '`')
		{
			$i++
		}
		$openingLength = $i - $openingStart
		$closingStart = -1
		for ($candidate = $i; $candidate -lt $characters.Length; $candidate++)
		{
			if ($characters[$candidate] -ne '`')
			{
				continue
			}
			$runStart = $candidate
			while ($candidate -lt $characters.Length -and $characters[$candidate] -eq '`')
			{
				$candidate++
			}
			if ($candidate - $runStart -eq $openingLength)
			{
				$closingStart = $runStart
				break
			}
			$candidate--
		}
		if ($closingStart -lt 0)
		{
			$i--
			continue
		}
		$closingEnd = $closingStart + $openingLength
		for ($masked = $openingStart; $masked -lt $closingEnd; $masked++)
		{
			if ($characters[$masked] -ne "`n")
			{
				$characters[$masked] = ' '
			}
		}
		$i = $closingEnd - 1
	}
	return [string]::new($characters)
}

# Null when the slug is not gpt-<version>-<family>, such as gpt-5.5 or codex-auto-review.
function Get-AgentModelFamily([string] $Slug) {
	if ($Slug -cmatch '^gpt-(\d+(?:\.\d+)*)-([a-z][a-z0-9-]*)$') {
		return [pscustomobject] @{ Version = $Matches[1]; Family = $Matches[2] }
	}

	return $null
}

# Part by part with a missing part read as 0, so 6 equals 6.0, 6 < 6.1, and 5.10 > 5.9.
function Compare-AgentModelVersion([string] $Left, [string] $Right) {
	$leftParts = @($Left.Split('.') | ForEach-Object { [long] $_ })
	$rightParts = @($Right.Split('.') | ForEach-Object { [long] $_ })
	for ($index = 0; $index -lt [Math]::Max($leftParts.Count, $rightParts.Count); $index++) {
		$leftPart = if ($index -lt $leftParts.Count) { $leftParts[$index] } else { 0 }
		$rightPart = if ($index -lt $rightParts.Count) { $rightParts[$index] } else { 0 }
		if ($leftPart -ne $rightPart) {
			return [Math]::Sign($leftPart - $rightPart)
		}
	}

	return 0
}

# The game projects under Root: each Projects/<Name> directory holding Platforms/VisualStudio2026/<Name>.vcxproj,
# read from the filesystem so an untracked project counts. Names are in ordinal order.
function Get-AgentGameProjects([string] $Root) {
	[string[]] $names = @([IO.Directory]::GetDirectories((Join-Path $Root 'Projects')) | ForEach-Object { [IO.Path]::GetFileName($_) } |
		Where-Object { [IO.File]::Exists((Join-Path $Root "Projects/$_/Platforms/VisualStudio2026/$_.vcxproj")) })
	[Array]::Sort($names, [StringComparer]::Ordinal)
	return $names
}

# The data inputs under Root that differ from commit Base, committed, staged, unstaged, or untracked: paths under
# DataPacker, Engine/Data, and Projects/<Project>/Data, plus Common/DataFile.h, '/'-separated, unique, in ordinal order.
# Repository agent documentation is never a data-pipeline input, so an AGENTS.md leaf is dropped. The Raw carve-out
# mirrors ExportRaw, which packs any file under a directory segment named Raw whatever its extension.
function Get-AgentDataInputChanges([string] $Root, [string] $Base, [string] $Project) {
	[string[]] $inputs = @('DataPacker', 'Engine/Data', "Projects/$Project/Data", 'Common/DataFile.h')
	$paths = [Collections.Generic.SortedSet[string]]::new([StringComparer]::Ordinal)
	foreach ($arguments in @(
		[string[]](@('-c', 'core.quotepath=false', '-C', $Root, 'diff', '--name-only', $Base, '--') + $inputs),
		[string[]](@('-c', 'core.quotepath=false', '-C', $Root, 'ls-files', '--others', '--exclude-standard', '--') + $inputs))) {
		# Stdout only: a git warning on stderr, such as a line-ending notice, must never read as a path.
		$run = Invoke-AgentProcess 'git' $arguments $Root
		if ($run.ExitCode -ne 0) { throw "git $($arguments -join ' ') failed with exit $($run.ExitCode): $($run.Stderr.Trim())" }
		foreach ($line in ($run.Stdout -split "`n")) {
			$path = $line.Trim().Replace('\', '/')
			if ($path.Length -eq 0) { continue }
			$segments = $path.Split('/')
			if ($segments[-1].Equals('AGENTS.md', [StringComparison]::OrdinalIgnoreCase) -and
				@($segments | Select-Object -SkipLast 1 | Where-Object { $_.Equals('Raw', [StringComparison]::OrdinalIgnoreCase) }).Count -eq 0) { continue }
			[void] $paths.Add($path)
		}
	}
	return [string[]]@($paths)
}

# The commit the bootstrap Shared-data refresh last exported the Project's data from, beside its Output\Data under Root.
function Get-AgentSharedDataStampPath([string] $Root, [string] $Project) {
	return Join-Path $Root "Projects\$Project\Platforms\VisualStudio2026\Output\SharedDataStamp.txt"
}

# The record every successful, non-selective game build of Target and Configuration writes: the format marker, the
# GameDataDirectory it passed to MSBuild, and, for a Shared build only, that directory's listing. It sits under the
# gitignored per-worktree game Output.
function Get-AgentSharedDataRecordPath([string] $Root, [string] $Project, [string] $Target, [string] $Configuration) {
	return Join-Path $Root "Projects\$Project\Platforms\VisualStudio2026\Output\SharedDataRecord.$Target.$Configuration.txt"
}

# A record written before the marker existed starts with a listing line instead, so it reads as missing.
$SharedDataRecordFormat = 'broken-engine-shared-data-record/v2'

function Write-AgentSharedDataRecord([string] $Root, [string] $Project, [string] $Target, [string] $Configuration, [string] $GameDataDirectory, [string[]] $Listing) {
	[IO.File]::WriteAllLines((Get-AgentSharedDataRecordPath $Root $Project $Target $Configuration), [string[]]@($SharedDataRecordFormat, $GameDataDirectory) + $Listing)
}

# The record's GameDataDirectory and Listing, or nothing when the record is missing or not in the current format.
function Get-AgentSharedDataRecord([string] $Root, [string] $Project, [string] $Target, [string] $Configuration) {
	$recordPath = Get-AgentSharedDataRecordPath $Root $Project $Target $Configuration
	if (-not [IO.File]::Exists($recordPath)) { return }
	[string[]] $lines = [IO.File]::ReadAllLines($recordPath)
	if ($lines.Count -lt 2 -or $lines[0] -cne $SharedDataRecordFormat) { return }
	return [pscustomobject] @{ GameDataDirectory = $lines[1]; Listing = [string[]]@($lines | Select-Object -Skip 2) }
}

# One line per file at any depth, hidden included: relative path, size, and last-write time in UTC ticks, tab-separated
# and sorted ordinally. DataPacker publishes every data file by temp-file rename, so each publish moves the time. Only
# directory metadata is read, so no file is opened and DataPacker's publish probe never meets a handle held here.
function Get-AgentDataListing([string] $Directory) {
	if (-not [IO.Directory]::Exists($Directory)) { return }
	$directoryInfo = [IO.DirectoryInfo]::new($Directory)
	$options = [IO.EnumerationOptions]::new()
	$options.RecurseSubdirectories = $true
	$options.AttributesToSkip = [IO.FileAttributes]::None
	[string[]] $lines = @(foreach ($file in $directoryInfo.EnumerateFiles('*', $options)) {
		"$([IO.Path]::GetRelativePath($directoryInfo.FullName, $file.FullName))`t$($file.Length)`t$($file.LastWriteTimeUtc.Ticks)"
	})
	[Array]::Sort($lines, [StringComparer]::Ordinal)
	return $lines
}

# The one Shared data comparison the harness claim and launch scripts run, against the listing in the Target and
# Configuration record. Nothing when the data directory is under the worktree root (Local output, or a derived copy under
# Temp, which the worktree owns) or the record's listing equals the current listing; otherwise a Code and Message result.
function Compare-AgentSharedDataRecord([string] $Root, [string] $DataDirectory, [string] $Project, [string] $Target, [string] $Configuration) {
	if ((Get-AgentCanonicalPath $DataDirectory).StartsWith((Get-AgentCanonicalPath $Root) + '\', [StringComparison]::OrdinalIgnoreCase)) { return }
	$dataRecord = Get-AgentSharedDataRecord $Root $Project $Target $Configuration
	if ($null -eq $dataRecord) {
		return [pscustomobject] @{
			Code = 'shared-data.record-missing'
			Message = "No current-format data record exists for $Target $Configuration; /compile of $Target $Configuration writes it."
		}
	}
	$record = [Collections.Generic.HashSet[string]]::new($dataRecord.Listing, [StringComparer]::Ordinal)
	$listing = [Collections.Generic.HashSet[string]]::new([string[]]@(Get-AgentDataListing $DataDirectory), [StringComparer]::Ordinal)
	[string[]] $differing = @(@($record | Where-Object { -not $listing.Contains($_) }) + @($listing | Where-Object { -not $record.Contains($_) }))
	if ($differing.Count -eq 0) { return }
	[Array]::Sort($differing, [StringComparer]::Ordinal)
	return [pscustomobject] @{
		Code = 'shared-data.changed'
		Message = ("The Shared data differs from this worktree's $Target $Configuration record: $($differing.Count) line(s) are in only one " +
			"of the record and the listing, first '$(($differing[0] -split "`t", 2)[0])'. The Shared data MUST be re-exported; stop and ask the user.")
	}
}

# The harness launch configuration for a game data directory: the project its
# Projects\<Name>\Platforms\VisualStudio2026\Output\Data suffix names, and the Output directory and executables the
# project hub's machine-readable launch configuration assigns, so the executables can never belong to a different game
# than the data. Only those three assignments are read, and the hub's hardcoded Debug suffix becomes the requested
# configuration's suffix: none for Release, whose TargetName is the bare project name. Failure is null on success,
# otherwise 'project-underivable' or 'launch-doc-unreadable' with its Message, which the caller maps to its own code.
function Get-AgentHarnessLaunchConfiguration([string] $Root, [string] $DataDirectory, [string] $Configuration) {
	$launch = [pscustomobject] @{
		Failure = $null; Message = $null; Project = $null; OutputRelative = $null; OutputDirectory = $null
		ServerExe = $null; ClientExe = $null; ServerBaseName = $null
	}
	$segments = (Get-AgentCanonicalPath $DataDirectory).Split('\')
	if ($segments.Count -lt 6 -or $segments[-6] -cne 'Projects' -or $segments[-4] -cne 'Platforms' -or
		$segments[-3] -cne 'VisualStudio2026' -or $segments[-2] -cne 'Output' -or $segments[-1] -cne 'Data') {
		$launch.Failure = 'project-underivable'
		$launch.Message = "GameDataDirectory '$(Get-AgentCanonicalPath $DataDirectory)' does not end in " +
			'Projects\<Name>\Platforms\VisualStudio2026\Output\Data, so its game project cannot be derived.'
		return $launch
	}
	$launch.Project = $segments[-5]
	$harnessDocument = Join-Path $Root "Projects\$($launch.Project)\Documents\AgentHarness.md"
	$launchText = ''
	if (Test-Path -LiteralPath $harnessDocument -PathType Leaf) {
		# A read failure leaves the text empty so it reports the same unreadable-doc failure as a parse failure.
		$launchText = [string](Get-Content -LiteralPath $harnessDocument -Raw -ErrorAction SilentlyContinue)
	}
	$outputMatch = [regex]::Match($launchText, '(?m)^\s*\$Output\s*=\s*Join-Path \$ROOT ''([^'']+)''\s*$')
	$serverMatch = [regex]::Match($launchText, '(?m)^\s*\$ServerExe\s*=\s*Join-Path \$Output ''([^'']+)''\s*$')
	$clientMatch = [regex]::Match($launchText, '(?m)^\s*\$ClientExe\s*=\s*Join-Path \$Output ''([^'']+)''\s*$')
	if (-not $outputMatch.Success -or -not $serverMatch.Success -or -not $clientMatch.Success) {
		$launch.Failure = 'launch-doc-unreadable'
		$launch.Message = "Output, ServerExe, and ClientExe could not be read from '$harnessDocument'."
		return $launch
	}
	$launch.OutputRelative = $outputMatch.Groups[1].Value
	$launch.OutputDirectory = Join-Path $Root $launch.OutputRelative
	$suffix = if ($Configuration -ieq 'Release') { '' } else { ".$Configuration" }
	$launch.ServerExe = [regex]::Replace($serverMatch.Groups[1].Value, '\.Debug\.exe$', "$suffix.exe")
	$launch.ClientExe = [regex]::Replace($clientMatch.Groups[1].Value, '\.Debug\.exe$', "$suffix.exe")
	# A running-server probe on the base name matches every configuration's server.
	$launch.ServerBaseName = [regex]::Replace($serverMatch.Groups[1].Value, '\.Debug\.exe$', '')
	return $launch
}

# The Documents/Plans/AGENTS.md `## Plan files` layout violations in the index and working tree, as {rule, path} rows
# sorted ordinally. The index stands for presence, so a staged Plan and its staged support files count as present.
# Every path deeper than <Area>/<Name>.md lies in the support folder <Area>/<Name>/, a nested Plan included. With
# -Folder (a support-folder path), only the support-markdown and support-uncommitted rules apply, over that folder.
function Get-AgentPlansLayoutViolation([string] $RepositoryRoot, [string] $Folder) {
	$root = [IO.Path]::GetFullPath($RepositoryRoot, (Get-Location).ProviderPath)
	$scope = if ($Folder) { ':(literal)' + $Folder + '/' } else { 'Documents/Plans' }
	$listing = Invoke-AgentProcess 'git' @('-C', $root, 'ls-files', '-z', '--', $scope) $root
	if ($listing.ExitCode -ne 0) { throw "git ls-files failed for '$scope': $($listing.Stderr.Trim())" }
	$status = Invoke-AgentProcess 'git' @('-C', $root, 'status', '--porcelain=v1', '-z', '--untracked-files=all', '--ignored=matching', '--', $scope) $root
	if ($status.ExitCode -ne 0) { throw "git status failed for '$scope': $($status.Stderr.Trim())" }

	$indexed = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
	foreach ($path in ($listing.Stdout -split "`0")) { if ($path.Length -ne 0) { [void] $indexed.Add($path) } }
	$paths = [Collections.Generic.HashSet[string]]::new($indexed, [StringComparer]::Ordinal)
	$uncommitted = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
	$fields = @($status.Stdout -split "`0")
	for ($index = 0; $index -lt $fields.Count; $index++) {
		$record = $fields[$index]
		if ($record.Length -lt 4) { continue }
		$state = $record.Substring(0, 2)
		$path = $record.Substring(3)
		[void] $paths.Add($path)
		if ($state -ceq '??' -or $state -ceq '!!' -or $state[1] -ne ' ') { [void] $uncommitted.Add($path) }
		# A rename or copy record carries one extra field, its original path.
		if ($state.Contains('R') -or $state.Contains('C')) { $index++ }
	}

	$keys = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
	foreach ($path in $paths) {
		$segments = $path.Substring('Documents/Plans/'.Length).Split('/')
		if ($segments.Count -eq 1) {
			if (-not $Folder -and $segments[0] -cne 'AGENTS.md') { [void] $keys.Add("$path`0root-file") }
			continue
		}
		if ($segments.Count -eq 2) {
			if (-not $Folder -and -not $segments[1].EndsWith('.md', [StringComparison]::Ordinal)) { [void] $keys.Add("$path`0area-file") }
			continue
		}
		$supportFolder = "Documents/Plans/$($segments[0])/$($segments[1])"
		if ($path.EndsWith('.md', [StringComparison]::Ordinal)) { [void] $keys.Add("$path`0support-markdown") }
		if ($uncommitted.Contains($path)) { [void] $keys.Add("$path`0support-uncommitted") }
		if (-not $Folder -and -not $indexed.Contains("$supportFolder.md")) { [void] $keys.Add("$supportFolder`0support-orphan") }
	}
	# NUL sorts below every path character, so sorting the keys orders the rows by path, then rule.
	[string[]] $sorted = @($keys)
	[Array]::Sort($sorted, [StringComparer]::Ordinal)
	return @($sorted | ForEach-Object { $parts = $_.Split("`0"); [pscustomobject] @{ rule = $parts[1]; path = $parts[0] } })
}

Export-ModuleMember -Function Get-AgentCanonicalPath, Invoke-AgentGit, Invoke-AgentProcess, Get-AgentNormalizedText, Measure-AgentTokenCount, Get-AgentMarkdownSearchText, Get-AgentModelFamily, Compare-AgentModelVersion, Get-AgentGameProjects, Get-AgentDataInputChanges, Get-AgentSharedDataStampPath, Get-AgentSharedDataRecordPath, Write-AgentSharedDataRecord, Get-AgentSharedDataRecord, Get-AgentDataListing, Compare-AgentSharedDataRecord, Get-AgentHarnessLaunchConfiguration, Get-AgentPlansLayoutViolation
