# Mechanical discovery for the /update-claude-docs sync workflow: the governing
# AGENTS.md chain of each caller-supplied changed path, the bt-token-v1 size of
# every document in those chains, and the repository-wide AGENTS.md/CLAUDE.md
# stub-pairing sweep. Reports only; every edit decision stays with the agent.
[CmdletBinding()]
param(
	# Remaining arguments bind here so `-File <script> -ChangedPath a b c` works:
	# under -File a comma-separated `a,b,c` would arrive as one string.
	[Parameter(Mandatory = $true, Position = 0, ValueFromRemainingArguments = $true)]
	[ValidateNotNullOrEmpty()]
	[string[]] $ChangedPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Measure-Tokens.ps1 exit codes are mapped into this script's own envelope, so a
# nonzero child exit must not raise before that mapping runs.
$PSNativeCommandUseErrorActionPreference = $false

$script:RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
$script:MeasureTokensScript = Join-Path $script:RepositoryRoot '.agents\scripts\Measure-Tokens.ps1'
$script:MeasureCodeTokensScript = Join-Path $script:RepositoryRoot '.agents\scripts\Measure-CodeTokens.ps1'
$script:AgentScriptCommon = Join-Path $script:RepositoryRoot '.agents\scripts\AgentScriptCommon.psm1'
$script:MaximumOutputBytes = 8192
$script:MaximumMessageLength = 256

# Repository-relative globs kept out of the stub-pairing sweep.
$script:StubSweepExclusions = @(
	'ThirdParty/*',
	'Documents/Plans/*',
	'Documents/Features/*',
	'CLAUDE.local.md',
	'*/CLAUDE.local.md'
)

$script:DocumentFileNames = @('AGENTS.md', 'CLAUDE.md', 'CLAUDE.local.md')
$script:StubBody = '@AGENTS.md'
$script:BudgetFloor = 1000
$script:CodeTokenSlope = 35
$script:DirectChildDocumentAllowance = 150
$script:BudgetOverrides = [Collections.Generic.Dictionary[string, int64]]::new([StringComparer]::Ordinal)
$script:BudgetOverrides.Add('Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md', 2003)
# The repository-root AGENTS.md is imported into every session and subagent, so
# it carries a fixed budget.
$script:RootHubTokenTarget = 8000
$script:ChainTokenTarget = 15000
$script:ChainTokenWarning = 20000

$result = [ordered]@{
	schemaVersion = 'broken-engine-affected-agents-docs/v1'
	status = 'error'
	code = 'internal.error'
	message = 'Affected AGENTS.md discovery did not run.'
	chains = [ordered]@{ items = @(); totalCount = 0; omittedCount = 0 }
	sizes = [ordered]@{ items = @(); totalCount = 0; omittedCount = 0 }
	stubPairs = [ordered]@{ items = @(); totalCount = 0; omittedCount = 0 }
}

function Complete-AffectedAgentsDocs
{
	param(
		[int] $ExitCode,
		[string] $Status,
		[string] $Code,
		[string] $Message,
		[object[]] $ChainItems = @(),
		[int] $ChainTotalCount = 0,
		[object[]] $SizeItems = @(),
		[int] $SizeTotalCount = 0,
		[object[]] $StubItems = @(),
		[int] $StubTotalCount = 0
	)

	if ($Message.Length -gt $script:MaximumMessageLength)
	{
		$Message = $Message.Substring(0, $script:MaximumMessageLength)
	}

	$chains = [Collections.Generic.List[object]]::new($ChainItems)
	$sizes = [Collections.Generic.List[object]]::new($SizeItems)
	$stubs = [Collections.Generic.List[object]]::new($StubItems)
	while ($true)
	{
		$result.status = $Status
		$result.code = $Code
		$result.message = $Message
		$result.chains = [ordered]@{ items = @($chains); totalCount = $ChainTotalCount; omittedCount = $ChainTotalCount - $chains.Count }
		$result.sizes = [ordered]@{ items = @($sizes); totalCount = $SizeTotalCount; omittedCount = $SizeTotalCount - $sizes.Count }
		$result.stubPairs = [ordered]@{ items = @($stubs); totalCount = $StubTotalCount; omittedCount = $StubTotalCount - $stubs.Count }
		$json = $result | ConvertTo-Json -Depth 8 -Compress
		if ([Text.Encoding]::UTF8.GetByteCount($json) -le $script:MaximumOutputBytes)
		{
			break
		}

		if ($stubs.Count -gt 0) { $stubs.RemoveAt($stubs.Count - 1); continue }
		if ($sizes.Count -gt 0) { $sizes.RemoveAt($sizes.Count - 1); continue }
		if ($chains.Count -gt 0) { $chains.RemoveAt($chains.Count - 1); continue }
		if ($Message.Length -gt 0) { $Message = ''; continue }
		break
	}

	[Console]::Out.Write($json)
	exit $ExitCode
}

function Convert-ToRepositoryRelativePath
{
	param([string] $FullPath)

	$separator = $script:RepositoryRoot.TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
	if (-not $FullPath.StartsWith($separator, [StringComparison]::OrdinalIgnoreCase))
	{
		return $null
	}

	return $FullPath.Substring($separator.Length).Replace([IO.Path]::DirectorySeparatorChar, '/')
}

# Windows path comparison is case-insensitive, so a caller may supply casing that
# does not match the tree. Every reported path and every document lookup uses the
# on-disk casing, so each existing segment is replaced by its real name; segments
# past the last existing one (a deleted file) keep the supplied spelling.
function Get-CanonicalRelativePath
{
	param([string] $RelativePath)

	$directory = [IO.DirectoryInfo]::new($script:RepositoryRoot)
	$canonical = [Collections.Generic.List[string]]::new()
	foreach ($segment in $RelativePath.Split('/'))
	{
		$entry = $null
		if ($null -ne $directory)
		{
			foreach ($candidate in $directory.GetFileSystemInfos())
			{
				# Case-insensitive by default, which is exactly the on-disk rule.
				if ($candidate.Name -eq $segment) { $entry = $candidate; break }
			}
		}

		if ($null -eq $entry)
		{
			$canonical.Add($segment)
			$directory = $null
			continue
		}

		$canonical.Add($entry.Name)
		$directory = if ($entry -is [IO.DirectoryInfo]) { $entry } else { $null }
	}

	return ($canonical -join '/')
}

function Get-ParentDirectory
{
	param([string] $RelativePath)

	if ($RelativePath -ceq '')
	{
		return $null
	}

	$index = $RelativePath.LastIndexOf('/')
	if ($index -lt 0)
	{
		return ''
	}

	return $RelativePath.Substring(0, $index)
}

function Get-DirectoryAgentsDocument
{
	param([string] $RelativeDirectory)

	if ($RelativeDirectory -ceq '')
	{
		return 'AGENTS.md'
	}

	return "$RelativeDirectory/AGENTS.md"
}

function Get-DescendantPrefix
{
	param([string] $AgentsPath)

	$directory = Get-ParentDirectory $AgentsPath
	if ($directory -ceq '')
	{
		return ''
	}

	return "$directory/"
}

# Enumerates every AGENTS.md and CLAUDE.md on disk, including untracked ones, so
# a document added by the current change is discovered. Reparse points are
# skipped because .claude/skills links back into .agents/skills.
function Get-DocumentPath
{
	$documents = [Collections.Generic.List[string]]::new()
	$pending = [Collections.Generic.Stack[IO.DirectoryInfo]]::new()
	$pending.Push([IO.DirectoryInfo]::new($script:RepositoryRoot))
	while ($pending.Count -gt 0)
	{
		$directory = $pending.Pop()
		foreach ($child in $directory.GetDirectories())
		{
			if ($child.Name -ceq '.git') { continue }
			if ($child.Attributes.HasFlag([IO.FileAttributes]::ReparsePoint)) { continue }
			$pending.Push($child)
		}

		foreach ($file in $directory.GetFiles())
		{
			if ($script:DocumentFileNames -ccontains $file.Name)
			{
				$documents.Add((Convert-ToRepositoryRelativePath $file.FullName))
			}
		}
	}

	$documents.Sort([StringComparer]::Ordinal)
	return ,$documents
}

function Test-StubSweepExcluded
{
	param([string] $RelativePath)

	foreach ($exclusion in $script:StubSweepExclusions)
	{
		if ($RelativePath -like $exclusion)
		{
			return $true
		}
	}

	return $false
}

function Get-GoverningChain
{
	param([string] $RelativeDirectory)

	$chain = [Collections.Generic.List[string]]::new()
	$directory = $RelativeDirectory
	while ($null -ne $directory)
	{
		$candidate = Get-DirectoryAgentsDocument $directory
		if ($script:DocumentSet.Contains($candidate))
		{
			$chain.Add($candidate)
		}

		$directory = Get-ParentDirectory $directory
	}

	$chain.Reverse()
	return ,$chain
}

# A descendant AGENTS.md is immediate when the supplied hub is the nearest
# AGENTS.md above it.
function Get-HubCandidate
{
	param([string] $AgentsPath)

	$prefix = Get-DescendantPrefix $AgentsPath
	$candidates = [Collections.Generic.List[string]]::new()
	foreach ($other in $script:AgentsPaths)
	{
		if ($other -ceq $AgentsPath) { continue }
		if (-not $other.StartsWith($prefix, [StringComparison]::Ordinal)) { continue }
		$chain = Get-GoverningChain (Get-ParentDirectory $other)
		if ($chain.Count -ge 2 -and $chain[$chain.Count - 2] -ceq $AgentsPath)
		{
			$candidates.Add($other)
		}
	}

	return ,$candidates
}

function Get-DirectChildDocumentCount
{
	param([string] $AgentsPath)

	$directory = Get-ParentDirectory $AgentsPath
	$count = 0
	foreach ($other in $script:AgentsPaths)
	{
		if ($other -ceq $AgentsPath) { continue }
		$otherDirectory = Get-ParentDirectory $other
		if ((Get-ParentDirectory $otherDirectory) -ceq $directory)
		{
			$count++
		}
	}

	return $count
}

# Delegates the bt-token-v1 estimate to the existing Measure-Tokens.ps1 rather
# than reimplementing it. The -Command form is required: under -File a
# comma-separated list binds to -Path as a single string.
function Measure-DocumentToken
{
	param([string[]] $RelativePath)

	$measured = @{}
	if ($RelativePath.Count -eq 0)
	{
		return $measured
	}

	$quoted = ($RelativePath | ForEach-Object { "'" + (Join-Path $script:RepositoryRoot $_).Replace("'", "''") + "'" }) -join ','
	$command = "& '" + $script:MeasureTokensScript.Replace("'", "''") + "' -Path $quoted -Json"
	$output = (& pwsh -NoProfile -Command $command 2>&1) -join "`n"
	if ($LASTEXITCODE -ne 0)
	{
		throw "Measure-Tokens.ps1 exited with $LASTEXITCODE`: $output"
	}

	$parsed = $output | ConvertFrom-Json -Depth 8
	$entries = if ($parsed.PSObject.Properties.Name -ccontains 'Files') { @($parsed.Files) } else { @($parsed) }
	if ($entries.Count -ne $RelativePath.Count)
	{
		throw "Measure-Tokens.ps1 returned $($entries.Count) entries for $($RelativePath.Count) paths."
	}

	for ($index = 0; $index -lt $RelativePath.Count; $index++)
	{
		$measured[$RelativePath[$index]] = [int64] $entries[$index].Tokens
	}

	return $measured
}

# Measures every chain-document directory in one child invocation and maps the
# ordered folder results back to their documents.
function Measure-CodeToken
{
	param([string[]] $RelativePath)

	$measured = @{}
	if ($RelativePath.Count -eq 0)
	{
		return $measured
	}

	$folders = @($RelativePath | ForEach-Object {
		$directory = Get-ParentDirectory $_
		if ($directory -ceq '') { $script:RepositoryRoot } else { Join-Path $script:RepositoryRoot $directory }
	})
	$quoted = ($folders | ForEach-Object { "'" + $_.Replace("'", "''") + "'" }) -join ','
	$command = "& '" + $script:MeasureCodeTokensScript.Replace("'", "''") + "' -Path $quoted"
	$output = (& pwsh -NoProfile -Command $command 2>&1) -join "`n"
	if ($LASTEXITCODE -ne 0)
	{
		throw "Measure-CodeTokens.ps1 exited with $LASTEXITCODE`: $output"
	}

	$parsed = $output | ConvertFrom-Json -Depth 8
	if ($parsed.metric -cne 'bt-token-v1' -or $null -eq $parsed.folders)
	{
		throw 'Measure-CodeTokens.ps1 returned a malformed envelope.'
	}

	$entries = @($parsed.folders)
	if ($entries.Count -ne $RelativePath.Count)
	{
		throw "Measure-CodeTokens.ps1 returned $($entries.Count) entries for $($RelativePath.Count) paths."
	}

	for ($index = 0; $index -lt $RelativePath.Count; $index++)
	{
		$entry = $entries[$index]
		if ($null -eq $entry.path -or $null -eq $entry.codeFileCount -or $null -eq $entry.tokens -or
			(Get-AgentCanonicalPath $entry.path) -cne (Get-AgentCanonicalPath $folders[$index]) -or
			[int64] $entry.codeFileCount -lt 0 -or [int64] $entry.tokens -lt 0)
		{
			throw "Measure-CodeTokens.ps1 returned a malformed entry at index $index."
		}

		$measured[$RelativePath[$index]] = [int64] $entry.tokens
	}

	return $measured
}

try
{
	[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
	Import-Module $script:AgentScriptCommon -Force

	# Relative input resolves against the repository root, never the caller's
	# working directory, so the same list means the same thing from any shell.
	$changedRelativePaths = [Collections.Generic.List[string]]::new()
	foreach ($suppliedPath in $ChangedPath)
	{
		$rooted = if ([IO.Path]::IsPathRooted($suppliedPath)) { $suppliedPath } else { Join-Path $script:RepositoryRoot $suppliedPath }
		$relative = Convert-ToRepositoryRelativePath (Get-AgentCanonicalPath $rooted)
		if ($null -eq $relative)
		{
			Complete-AffectedAgentsDocs 1 'error' 'input.path-outside-repository' "Changed path resolves outside the repository: '$suppliedPath'."
		}

		$relative = Get-CanonicalRelativePath $relative
		if (-not $changedRelativePaths.Contains($relative))
		{
			$changedRelativePaths.Add($relative)
		}
	}

	$documentPaths = Get-DocumentPath
	$script:DocumentSet = [Collections.Generic.HashSet[string]]::new([string[]] $documentPaths, [StringComparer]::Ordinal)
	$script:AgentsPaths = @($documentPaths | Where-Object { $_ -ceq 'AGENTS.md' -or $_.EndsWith('/AGENTS.md', [StringComparison]::Ordinal) })

	$chainItems = [Collections.Generic.List[object]]::new()
	$chainDocuments = [Collections.Generic.List[string]]::new()
	foreach ($relative in $changedRelativePaths)
	{
		$isAgentsDocument = $script:DocumentSet.Contains($relative) -and (Get-DirectoryAgentsDocument (Get-ParentDirectory $relative)) -ceq $relative
		# A changed directory is governed by its own AGENTS.md; a file starts one level up.
		$startDirectory = if ([IO.Directory]::Exists((Join-Path $script:RepositoryRoot $relative))) { $relative } else { Get-ParentDirectory $relative }
		$chain = Get-GoverningChain $startDirectory
		$hubCandidates = if ($isAgentsDocument) { Get-HubCandidate $relative } else { [Collections.Generic.List[string]]::new() }
		foreach ($document in $chain)
		{
			if (-not $chainDocuments.Contains($document)) { $chainDocuments.Add($document) }
		}

		$chainItems.Add([ordered]@{
			changedPath = $relative
			governing = if ($chain.Count -gt 0) { $chain[$chain.Count - 1] } else { $null }
			documents = @($chain)
			hubCandidates = @($hubCandidates)
		})
	}

	$chainDocuments.Sort([StringComparer]::Ordinal)
	$measured = Measure-DocumentToken ([string[]] $chainDocuments)
	$codeMeasured = Measure-CodeToken ([string[]] $chainDocuments)
	$sizeItems = [Collections.Generic.List[object]]::new()
	foreach ($document in $chainDocuments)
	{
		$tokens = $measured[$document]
		$codeTokens = $codeMeasured[$document]
		$directChildDocumentCount = Get-DirectChildDocumentCount $document
		if ($document -ceq 'AGENTS.md')
		{
			$budgetRule = 'root'
			$budget = $script:RootHubTokenTarget
		}
		elseif ($script:BudgetOverrides.ContainsKey($document))
		{
			$budgetRule = 'override'
			$budget = $script:BudgetOverrides[$document]
		}
		else
		{
			$budgetRule = 'formula'
			$rawBudget = $script:BudgetFloor + $script:CodeTokenSlope * ($codeTokens / 1000.0) + $script:DirectChildDocumentAllowance * $directChildDocumentCount
			$budget = [int64] [Math]::Round($rawBudget, 0, [MidpointRounding]::AwayFromZero)
		}

		$sizeItems.Add([ordered]@{
			path = $document
			codeTokens = $codeTokens
			directChildDocumentCount = $directChildDocumentCount
			budget = $budget
			tokens = $tokens
			budgetRule = $budgetRule
			verdict = if ($tokens -le $budget) { 'ok' } else { 'over-target' }
		})
	}

	# Chain totals are advisory: they never authorize trimming prose.
	foreach ($item in $chainItems)
	{
		$total = [int64] 0
		foreach ($document in $item.documents) { $total += $measured[$document] }
		$item.totalTokens = $total
		$item.verdict = if ($total -gt $script:ChainTokenWarning) { 'warning' } elseif ($total -ge $script:ChainTokenTarget) { 'over-target' } else { 'ok' }
	}

	$stubItems = [Collections.Generic.List[object]]::new()
	foreach ($document in $documentPaths)
	{
		if (Test-StubSweepExcluded $document) { continue }

		$directory = Get-ParentDirectory $document
		if ($document -ceq (Get-DirectoryAgentsDocument $directory))
		{
			$stub = if ($directory -ceq '') { 'CLAUDE.md' } else { "$directory/CLAUDE.md" }
			if (-not $script:DocumentSet.Contains($stub))
			{
				$stubItems.Add([ordered]@{ path = $document; code = 'stub.missing'; detail = "No sibling CLAUDE.md at '$stub'." })
			}

			continue
		}

		$owner = Get-DirectoryAgentsDocument $directory
		if (-not $script:DocumentSet.Contains($owner))
		{
			$stubItems.Add([ordered]@{ path = $document; code = 'stub.orphan'; detail = "No same-directory AGENTS.md at '$owner'." })
			continue
		}

		$bytes = [IO.File]::ReadAllBytes((Join-Path $script:RepositoryRoot $document))
		$text = [Text.Encoding]::UTF8.GetString($bytes)
		if ($text -cne "$script:StubBody`n" -and $text -cne "$script:StubBody`r`n")
		{
			$stubItems.Add([ordered]@{ path = $document; code = 'stub.malformed'; detail = "Stub must be '$script:StubBody' plus one line ending; file is $($bytes.Length) bytes." })
		}
	}

	Complete-AffectedAgentsDocs 0 'pass' 'ok' 'Affected AGENTS.md discovery completed.' @($chainItems) $chainItems.Count @($sizeItems) $sizeItems.Count @($stubItems) $stubItems.Count
}
catch
{
	Complete-AffectedAgentsDocs 1 'error' 'internal.error' $_.Exception.Message
}
