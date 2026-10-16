# Adds Engine/ and Common/ files to every game client/server project and filters file found by the
# Projects/AGENTS.md standard layout, taking projects, item type, and filter from the
# Resolve-VcxprojMembership.ps1 record; with -Remove, deletes the items of paths no longer on disk.
# Edits are whole-line insertions or deletions, so every other byte, the BOM, and LF endings stay.
[CmdletBinding(PositionalBinding = $false)]
param(
	# Collected like the resolver's paths so 'pwsh -NoProfile -File' can pass several.
	[Parameter(ValueFromRemainingArguments)][string[]] $FilePath,
	# Validated in the body: a binding-time failure would emit raw PowerShell errors instead of the envelope.
	[string] $ShaderHeaderMembership,
	[switch] $Remove
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$sharedScripts = Join-Path $PSScriptRoot '..\..\..\scripts'
if (-not (Test-Path -LiteralPath (Join-Path $sharedScripts 'AgentScriptCommon.psm1'))) {
	$sharedScripts = Join-Path $PSScriptRoot '..\..\..\..\.agents\scripts'
}
Import-Module (Join-Path $sharedScripts 'AgentScriptCommon.psm1') -Force

$script:MaximumRecords = 16
$script:MaximumMessageLength = 256
$script:MaximumOutputBytes = 8192
$script:DataPackerNote = 'DataPacker membership is per-file scoped authority and was left untouched.'
$script:Utf8 = [Text.UTF8Encoding]::new($false, $true)

$script:Records = [System.Collections.Generic.List[object]]::new()
$script:RequestedCount = 0

$result = [ordered]@{
	schemaVersion = 'broken-engine-vcxproj-item-update/v1'
	status = 'error'
	code = 'internal.error'
	message = 'Project item update did not run.'
	mode = if ($Remove) { 'remove' } else { 'add' }
	files = [ordered]@{
		items = @()
		totalCount = 0
		omittedCount = 0
		truncated = $false
	}
}

function Complete-EngineVcxprojItem
{
	param([int] $ExitCode, [string] $Status, [string] $Code, [string] $Message)

	if ($Message.Length -gt $script:MaximumMessageLength)
	{
		$Message = $Message.Substring(0, $script:MaximumMessageLength)
	}

	# A failed run writes nothing, so it reports no per-path outcome.
	if ($Status -cne 'pass')
	{
		$script:Records.Clear()
	}
	$items = [System.Collections.Generic.List[object]]::new($script:Records)
	$omitted = $script:RequestedCount - $script:Records.Count
	while ($true)
	{
		$result.status = $Status
		$result.code = $Code
		$result.message = $Message
		$result.files.items = @($items)
		$result.files.totalCount = $script:RequestedCount
		$result.files.omittedCount = $omitted
		$result.files.truncated = $omitted -gt 0
		$json = $result | ConvertTo-Json -Depth 8 -Compress
		if ([Text.Encoding]::UTF8.GetByteCount($json) -le $script:MaximumOutputBytes -or $items.Count -eq 0)
		{
			break
		}

		$items.RemoveAt($items.Count - 1)
		$omitted++
	}

	if ($Status -cne 'pass')
	{
		[Console]::Error.WriteLine("Update-EngineVcxprojItem: $Code — $Message")
	}
	[Console]::Out.Write($json)
	exit $ExitCode
}

function Stop-EngineVcxprojItem
{
	param([string] $Code, [string] $Message)

	Complete-EngineVcxprojItem 1 'error' $Code $Message
}

function Get-RepositoryRelativePath
{
	param([string] $Path)

	return $Path.Substring($script:RootWithSeparator.Length).Replace([IO.Path]::DirectorySeparatorChar, '/')
}

# Line-level view of one project or filters file: every ItemGroup at two-space indentation with the
# line span of each child element. Any other shape inside an ItemGroup fails the run, never guessed.
function Read-ProjectFile
{
	param([string] $Path)

	$bytes = [IO.File]::ReadAllBytes($Path)
	$hasBom = $bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF
	$offset = if ($hasBom) { 3 } else { 0 }
	$lines = $script:Utf8.GetString($bytes, $offset, $bytes.Length - $offset).Split("`n")
	$relativePath = Get-RepositoryRelativePath $Path

	$groups = [System.Collections.Generic.List[object]]::new()
	$index = 0
	while ($index -lt $lines.Count)
	{
		if ($lines[$index] -notmatch '^  <ItemGroup(?<attributes>\s[^>]*)?>$')
		{
			$index++
			continue
		}

		$group = [pscustomobject]@{
			Labeled = $Matches.ContainsKey('attributes') -and $Matches.attributes -match '\sLabel="'
			CloseIndex = -1
			Children = [System.Collections.Generic.List[object]]::new()
		}
		$index++
		while ($group.CloseIndex -lt 0)
		{
			if ($index -ge $lines.Count)
			{
				Stop-EngineVcxprojItem 'project.unrecognized-layout' "'$relativePath' has an unterminated ItemGroup."
			}
			$line = $lines[$index]
			if ($line -ceq '  </ItemGroup>')
			{
				$group.CloseIndex = $index
				continue
			}
			if ([string]::IsNullOrWhiteSpace($line))
			{
				$index++
				continue
			}
			if ($line -notmatch '^    <(?<name>[A-Za-z_][\w.-]*)(?<attributes>(?:\s+[\w:.-]+="[^"]*")*)\s*(?<selfClose>/?)>$')
			{
				Stop-EngineVcxprojItem 'project.unrecognized-layout' "'$relativePath' line $($index + 1) is not a recognized ItemGroup child."
			}

			$name = $Matches.name
			$selfClosing = $Matches.selfClose -ceq '/'
			$include = if ($Matches.attributes -match '\sInclude="(?<value>[^"]*)"') { [Net.WebUtility]::HtmlDecode($Matches.value) } else { $null }
			$start = $index
			if (-not $selfClosing)
			{
				$closeTag = "    </$name>"
				while ($lines[$index] -cne $closeTag)
				{
					$index++
					if ($index -ge $lines.Count -or $lines[$index] -ceq '  </ItemGroup>')
					{
						Stop-EngineVcxprojItem 'project.unrecognized-layout' "'$relativePath' line $($start + 1) has no '$closeTag' inside its ItemGroup."
					}
				}
			}
			$null = $group.Children.Add([pscustomobject]@{ Name = $name; Include = $include; Start = $start; End = $index })
			$index++
		}
		$null = $groups.Add($group)
		$index++
	}
	# CRLF endings or other indentation leave no recognized group, which -Remove would report as absent.
	if ($groups.Count -eq 0)
	{
		Stop-EngineVcxprojItem 'project.unrecognized-layout' "'$relativePath' has no two-space-indented, LF-terminated ItemGroup."
	}

	$guids = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
	foreach ($match in [regex]::Matches(($lines -join "`n"), '<UniqueIdentifier>(?<guid>[^<]*)</UniqueIdentifier>'))
	{
		$null = $guids.Add($match.Groups['guid'].Value)
	}

	return [pscustomobject]@{
		Path = $Path
		RelativePath = $relativePath
		Directory = [IO.Path]::GetDirectoryName($Path)
		HasBom = $hasBom
		Lines = $lines
		Groups = $groups
		Guids = $guids
		Insertions = [System.Collections.Generic.Dictionary[int, System.Collections.Generic.List[string]]]::new()
		Deletions = [System.Collections.Generic.HashSet[int]]::new()
		PlannedFilters = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
	}
}

function Get-MatchingItems
{
	param([object] $File, [string] $Include)

	$items = [System.Collections.Generic.List[object]]::new()
	foreach ($group in $File.Groups)
	{
		foreach ($child in $group.Children)
		{
			if ($child.Name -cne 'Filter' -and $null -ne $child.Include -and $child.Include -ieq $Include)
			{
				$null = $items.Add($child)
			}
		}
	}

	return ,$items
}

# The one unlabeled ItemGroup whose children are all of the given element name; never created or chosen.
function Get-TargetGroup
{
	param([object] $File, [string] $Name)

	$candidates = @($File.Groups | Where-Object {
		$group = $_
		-not $group.Labeled -and $group.Children.Count -gt 0 -and @($group.Children | Where-Object { $_.Name -cne $Name }).Count -eq 0
	})
	if ($candidates.Count -ne 1)
	{
		Stop-EngineVcxprojItem 'project.target-group-not-unique' "'$($File.RelativePath)' has $($candidates.Count) unlabeled ItemGroups holding only '$Name' items; exactly one is required."
	}

	return $candidates[0]
}

function Add-Insertion
{
	param([object] $File, [object] $Group, [string[]] $Lines)

	if (-not $File.Insertions.ContainsKey($Group.CloseIndex))
	{
		$File.Insertions.Add($Group.CloseIndex, [System.Collections.Generic.List[string]]::new())
	}
	$File.Insertions[$Group.CloseIndex].AddRange($Lines)
}

function Test-FilterDeclared
{
	param([object] $File, [string] $Filter)

	if ($File.PlannedFilters.Contains($Filter))
	{
		return $true
	}
	foreach ($group in $File.Groups)
	{
		foreach ($child in $group.Children)
		{
			if ($child.Name -ceq 'Filter' -and $child.Include -ceq $Filter)
			{
				return $true
			}
		}
	}

	return $false
}

function New-FilterGuid
{
	param([object] $File)

	do
	{
		$guid = [guid]::NewGuid().ToString('B').ToLowerInvariant()
	}
	while (-not $File.Guids.Add($guid))

	return $guid
}

# Fails on a pair whose two files disagree about the item; returns the project and filters matches.
function Get-PairMatches
{
	param([object] $Pair, [string] $Include, [string] $ReportedPath)

	$projectItems = Get-MatchingItems $Pair.Project $Include
	$filterItems = Get-MatchingItems $Pair.Filters $Include
	if (($projectItems.Count -gt 0) -ne ($filterItems.Count -gt 0))
	{
		Stop-EngineVcxprojItem 'item.one-sided-pair' "'$ReportedPath' is listed in only one file of the '$($Pair.Project.RelativePath)' pair."
	}

	return [pscustomobject]@{ Project = $projectItems; Filters = $filterItems }
}

function Get-Games
{
	$games = [System.Collections.Generic.List[object]]::new()
	foreach ($name in (Get-AgentGameProjects $script:Root))
	{
		$directory = Join-Path $script:Root "Projects\$name\Platforms\VisualStudio2026"
		$clientPath = Join-Path $directory "$name.vcxproj"
		$serverPath = Join-Path $directory "${name}Server.vcxproj"
		foreach ($required in @("$clientPath.filters", $serverPath, "$serverPath.filters"))
		{
			if (-not [IO.File]::Exists($required))
			{
				Stop-EngineVcxprojItem 'layout.pair-incomplete' "Game '$name' is missing '$(Get-RepositoryRelativePath $required)'."
			}
		}

		$null = $games.Add([pscustomobject]@{
			client = [pscustomobject]@{ Project = (Read-ProjectFile $clientPath); Filters = (Read-ProjectFile "$clientPath.filters") }
			server = [pscustomobject]@{ Project = (Read-ProjectFile $serverPath); Filters = (Read-ProjectFile "$serverPath.filters") }
		})
	}
	if ($games.Count -eq 0)
	{
		Stop-EngineVcxprojItem 'layout.no-game' 'No Projects/<Name>/Platforms/VisualStudio2026/<Name>.vcxproj game project was found.'
	}

	return ,$games
}

function Get-ResolverRecords
{
	param([string[]] $RelativePaths)

	$shell = [Environment]::ProcessPath
	if ([string]::IsNullOrEmpty($shell)) { $shell = 'pwsh' }
	$resolver = Join-Path $PSScriptRoot 'Resolve-VcxprojMembership.ps1'
	$run = Invoke-AgentProcess $shell (@('-NoProfile', '-File', $resolver) + $RelativePaths) $script:Root
	$document = $null
	try { $document = $run.Stdout | ConvertFrom-Json } catch { $document = $null }
	if ($null -eq $document)
	{
		Stop-EngineVcxprojItem 'resolver.failed' "Resolve-VcxprojMembership.ps1 exited $($run.ExitCode) without a JSON envelope."
	}
	if ($run.ExitCode -ne 0 -or $document.status -cne 'pass')
	{
		Stop-EngineVcxprojItem 'resolver.failed' "Resolve-VcxprojMembership.ps1: $($document.code) — $($document.message)"
	}
	if ($document.files.truncated -or @($document.files.items).Count -ne $RelativePaths.Count)
	{
		Stop-EngineVcxprojItem 'resolver.truncated' 'Resolve-VcxprojMembership.ps1 omitted records; pass fewer paths per run.'
	}

	return ,@($document.files.items)
}

function Add-PlannedItem
{
	param([object] $Pair, [string] $Include, [string] $ItemType, [string] $Filter)

	$escapedInclude = [Security.SecurityElement]::Escape($Include)
	Add-Insertion $Pair.Project (Get-TargetGroup $Pair.Project $ItemType) @("    <$ItemType Include=`"$escapedInclude`" />")
	Add-Insertion $Pair.Filters (Get-TargetGroup $Pair.Filters $ItemType) @(
		"    <$ItemType Include=`"$escapedInclude`">"
		"      <Filter>$([Security.SecurityElement]::Escape($Filter))</Filter>"
		"    </$ItemType>"
	)

	$segments = $Filter -split '\\'
	for ($count = 1; $count -le $segments.Count; $count++)
	{
		$declaration = $segments[0..($count - 1)] -join '\'
		if (Test-FilterDeclared $Pair.Filters $declaration)
		{
			continue
		}

		Add-Insertion $Pair.Filters (Get-TargetGroup $Pair.Filters 'Filter') @(
			"    <Filter Include=`"$([Security.SecurityElement]::Escape($declaration))`">"
			"      <UniqueIdentifier>$(New-FilterGuid $Pair.Filters)</UniqueIdentifier>"
			'    </Filter>'
		)
		$null = $Pair.Filters.PlannedFilters.Add($declaration)
	}
}

function Write-ProjectFile
{
	param([object] $File)

	if ($File.Insertions.Count -eq 0 -and $File.Deletions.Count -eq 0)
	{
		return
	}

	$lines = [System.Collections.Generic.List[string]]::new()
	for ($index = 0; $index -lt $File.Lines.Count; $index++)
	{
		if ($File.Insertions.ContainsKey($index))
		{
			$lines.AddRange($File.Insertions[$index])
		}
		if (-not $File.Deletions.Contains($index))
		{
			$lines.Add($File.Lines[$index])
		}
	}

	$body = $script:Utf8.GetBytes($lines -join "`n")
	$bytes = if ($File.HasBom) { [byte[]](0xEF, 0xBB, 0xBF) + $body } else { $body }
	[IO.File]::WriteAllBytes($File.Path, [byte[]] $bytes)
}

try
{
	$script:Root = Get-AgentCanonicalPath (Join-Path $PSScriptRoot '..\..\..\..')
	$script:RootWithSeparator = $script:Root.TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar

	if ($null -eq $FilePath -or $FilePath.Count -eq 0)
	{
		Stop-EngineVcxprojItem 'input.no-file-paths' 'No file path was supplied; pass at least one Engine/ or Common/ path.'
	}
	$script:RequestedCount = $FilePath.Count
	if ($FilePath.Count -gt $script:MaximumRecords)
	{
		Stop-EngineVcxprojItem 'input.too-many-paths' "$($FilePath.Count) paths exceed the $($script:MaximumRecords)-path cap; run in smaller batches."
	}
	$membershipSupplied = $PSBoundParameters.ContainsKey('ShaderHeaderMembership')
	if ($membershipSupplied -and $Remove)
	{
		Stop-EngineVcxprojItem 'input.parameter-conflict' '-ShaderHeaderMembership cannot be combined with -Remove.'
	}
	if ($membershipSupplied -and @('None', 'Client', 'Server', 'Both') -cnotcontains $ShaderHeaderMembership)
	{
		Stop-EngineVcxprojItem 'input.shader-header-membership-invalid' "-ShaderHeaderMembership must be None, Client, Server, or Both, not '$ShaderHeaderMembership'."
	}

	$entries = [System.Collections.Generic.List[object]]::new()
	$seen = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
	foreach ($requestedPath in $FilePath)
	{
		$absolutePath = Get-AgentCanonicalPath $(if ([IO.Path]::IsPathRooted($requestedPath)) { $requestedPath } else { Join-Path $script:Root $requestedPath })
		$relativePath = if ($absolutePath.StartsWith($script:RootWithSeparator, [StringComparison]::OrdinalIgnoreCase)) { Get-RepositoryRelativePath $absolutePath } else { $null }
		if ($null -eq $relativePath -or $relativePath -notmatch '^(Engine|Common)/')
		{
			Stop-EngineVcxprojItem 'input.path-outside-engine-common' "'$requestedPath' is not under Engine/ or Common/; game, tool, and DataPacker membership stay hand edits."
		}
		if (-not $seen.Add($relativePath))
		{
			Stop-EngineVcxprojItem 'input.duplicate-path' "'$relativePath' was supplied more than once."
		}
		if ($Remove -and (Test-Path -LiteralPath $absolutePath))
		{
			Stop-EngineVcxprojItem 'input.file-exists' "'$relativePath' still exists on disk; -Remove takes only deleted paths and a rename's old name."
		}
		if (-not $Remove -and -not (Test-Path -LiteralPath $absolutePath -PathType Leaf))
		{
			Stop-EngineVcxprojItem 'input.file-missing' "'$relativePath' is not an existing file."
		}
		$null = $entries.Add([pscustomobject]@{ Absolute = $absolutePath; Relative = $relativePath })
	}

	$games = Get-Games

	if ($Remove)
	{
		foreach ($entry in $entries)
		{
			$projectFiles = [System.Collections.Generic.List[object]]::new()
			foreach ($game in $games)
			{
				foreach ($pair in @($game.client, $game.server))
				{
					$include = [IO.Path]::GetRelativePath($pair.Project.Directory, $entry.Absolute).Replace('/', '\')
					$pairMatches = Get-PairMatches $pair $include $entry.Relative
					$outcome = if ($pairMatches.Project.Count -gt 0) { 'removed' } else { 'absent' }
					foreach ($side in @(@{ File = $pair.Project; Items = $pairMatches.Project }, @{ File = $pair.Filters; Items = $pairMatches.Filters }))
					{
						foreach ($item in $side.Items)
						{
							for ($index = $item.Start; $index -le $item.End; $index++)
							{
								$null = $side.File.Deletions.Add($index)
							}
						}
						$null = $projectFiles.Add([ordered]@{ file = $side.File.RelativePath; outcome = $outcome })
					}
				}
			}
			$null = $script:Records.Add([ordered]@{
				path = $entry.Relative
				itemType = $null
				filter = $null
				projectFiles = @($projectFiles)
				note = if ($entry.Relative -match '^Common/') { $script:DataPackerNote } else { $null }
			})
		}
	}
	else
	{
		$resolverRecords = Get-ResolverRecords @($entries | ForEach-Object { $_.Relative })
		$membershipUsed = $false
		for ($entryIndex = 0; $entryIndex -lt $entries.Count; $entryIndex++)
		{
			$entry = $entries[$entryIndex]
			$record = $resolverRecords[$entryIndex]
			$note = $null
			$itemType = $record.itemType
			$sides = @($record.projects)
			$accepted = $true
			switch ("$($record.mapping)|$($record.mappingCode)")
			{
				'non-member|mapping.deliberate-non-member' { $accepted = $false; $note = 'non-member: deliberately never a project item; skipped.' }
				'resolved|mapping.engine-source' { }
				'resolved|mapping.shader-stage' { }
				'conditional|mapping.common-datapacker-conditional' { $note = $script:DataPackerNote }
				'conditional|mapping.shader-tree-header-conditional'
				{
					if (-not $membershipSupplied)
					{
						Stop-EngineVcxprojItem 'input.shader-header-membership-required' "'$($entry.Relative)' is a shader-tree header; pass -ShaderHeaderMembership None, Client, Server, or Both."
					}
					$membershipUsed = $true
					$itemType = if ($ShaderHeaderMembership -ceq 'None') { 'None' } else { 'ClInclude' }
					$sides = switch ($ShaderHeaderMembership)
					{
						'None' { @('client') }
						'Client' { @('client') }
						'Server' { @('server') }
						'Both' { @('client', 'server') }
					}
				}
				default
				{
					Stop-EngineVcxprojItem 'record.not-accepted' "'$($entry.Relative)' resolved as '$($record.mapping)' ($($record.mappingCode)); the agent decides its membership."
				}
			}

			$projectFiles = [System.Collections.Generic.List[object]]::new()
			if ($accepted)
			{
				foreach ($game in $games)
				{
					foreach ($side in $sides)
					{
						$pair = $game.$side
						$include = [IO.Path]::GetRelativePath($pair.Project.Directory, $entry.Absolute).Replace('/', '\')
						$pairMatches = Get-PairMatches $pair $include $entry.Relative
						$otherType = @(@($pairMatches.Project) + @($pairMatches.Filters) | Where-Object { $_.Name -cne $itemType })
						if ($otherType.Count -gt 0)
						{
							Stop-EngineVcxprojItem 'item.other-type' "'$($entry.Relative)' is a '$($otherType[0].Name)' item, not '$itemType', in the '$($pair.Project.RelativePath)' pair."
						}
						$outcome = 'present'
						if ($pairMatches.Project.Count -eq 0)
						{
							Add-PlannedItem $pair $include $itemType $record.filter
							$outcome = 'added'
						}
						$null = $projectFiles.Add([ordered]@{ file = $pair.Project.RelativePath; outcome = $outcome })
						$null = $projectFiles.Add([ordered]@{ file = $pair.Filters.RelativePath; outcome = $outcome })
					}
				}
			}
			$null = $script:Records.Add([ordered]@{
				path = $entry.Relative
				itemType = if ($accepted) { $itemType } else { $null }
				filter = if ($accepted) { $record.filter } else { $null }
				projectFiles = @($projectFiles)
				note = $note
			})
		}
		if ($membershipSupplied -and -not $membershipUsed)
		{
			Stop-EngineVcxprojItem 'input.shader-header-membership-unneeded' '-ShaderHeaderMembership was supplied, but no path is a header under Engine/Data/Shaders/.'
		}
	}

	# Every edit is planned before the first write, so a failing run leaves every file unchanged.
	foreach ($game in $games)
	{
		foreach ($pair in @($game.client, $game.server))
		{
			Write-ProjectFile $pair.Project
			Write-ProjectFile $pair.Filters
		}
	}

	Complete-EngineVcxprojItem 0 'pass' 'ok' 'Applied the requested items to every game client/server project pair; each projectFiles outcome names what changed.'
}
catch
{
	Stop-EngineVcxprojItem 'internal.error' $_.Exception.Message
}
