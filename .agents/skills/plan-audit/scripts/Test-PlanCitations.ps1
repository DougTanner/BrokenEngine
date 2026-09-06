# Advisory citation lookup, plus execution-card completeness, for one input file: a plan file, or a
# reviewer scope file carrying an inline execution card. It renders no verdict: a record with
# pathExists or lineExists false is a lead for the auditor, never a finding, and for a scope file the
# heading results are advisory because a scope file need not carry the plan headings. The card result is
# advisory for a plan file as well: 'present' is a whole-file phrase match, so a plan whose prose only
# mentions 'execution card' reports present true with all eight fields missing, which is not a defect.
# It must stay runnable under a read-only sandbox, so it reads only and writes its whole result to
# stdout; stderr carries diagnostics only.
[CmdletBinding()]
param(
	[Parameter(Position = 0)][string]$PlanPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:RepoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
$script:CitationPattern = '^[A-Za-z0-9_./-]+\.(h|cpp|md|ps1|py|txt|glsl)(:[0-9]+(-[0-9]+)?)?$'
$script:MaximumCitations = 48
$script:MaximumTextLength = 160
$script:MaximumMessageLength = 256
$script:MaximumOutputBytes = 32768
# The execution-card fields in the order ../../next-plan/SKILL.md '## Handoff' lists them. A Section
# field is a '###' heading, present only with a non-blank line under it; the others are bullets,
# present only with text after the colon.
$script:CardFields = @(
	[pscustomobject]@{ Name = 'whatDoesThisPlanDo'; Pattern = '^###\s+What does this plan do\?\s*$'; Section = $true }
	[pscustomobject]@{ Name = 'whyGoodForCodebase'; Pattern = '^###\s+Why this is good for the codebase\s*$'; Section = $true }
	[pscustomobject]@{ Name = 'goal'; Pattern = '^-\s+Goal:\s*\S'; Section = $false }
	[pscustomobject]@{ Name = 'outOfScope'; Pattern = '^-\s+Out of scope:\s*\S'; Section = $false }
	[pscustomobject]@{ Name = 'tierTrigger'; Pattern = '^-\s+Tier trigger:\s*\S'; Section = $false }
	[pscustomobject]@{ Name = 'interfacesAndInvariants'; Pattern = '^-\s+Interfaces and invariants:\s*\S'; Section = $false }
	[pscustomobject]@{ Name = 'acceptanceChecks'; Pattern = '^-\s+Acceptance checks:\s*\S'; Section = $false }
	[pscustomobject]@{ Name = 'roles'; Pattern = '^-\s+Roles:\s*\S'; Section = $false }
)

$script:Result = [ordered]@{
	schemaVersion = 'broken-engine-plan-citations/v1'
	status = 'error'
	code = 'internal.error'
	message = 'Plan citation lookup did not run.'
	plan = $null
	headings = $null
	card = $null
	citations = $null
	truncated = $false
}

function Write-Diagnostic
{
	param([string]$Message)

	[Console]::Error.WriteLine("PlanCitations: $Message")
}

function New-CappedText
{
	param([string]$Value, [int]$Limit)

	if ($Value.Length -le $Limit)
	{
		return [pscustomobject]@{ text = $Value; truncated = $false }
	}

	return [pscustomobject]@{ text = $Value.Substring(0, $Limit); truncated = $true }
}

function Get-RepositoryRelativePath
{
	# Returns $null for any token that canonically resolves outside the worktree root,
	# so a traversal or rooted token is treated as a non-repository token and never read.
	param([string]$RelativePath)

	if ([IO.Path]::IsPathRooted($RelativePath))
	{
		return $null
	}

	try
	{
		$fullPath = [IO.Path]::GetFullPath((Join-Path $script:RepoRoot $RelativePath))
	}
	catch
	{
		return $null
	}

	$rootWithSeparator = $script:RepoRoot.TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
	if (-not $fullPath.StartsWith($rootWithSeparator, [StringComparison]::OrdinalIgnoreCase))
	{
		return $null
	}

	return [pscustomobject]@{
		fullPath = $fullPath
		relativePath = $fullPath.Substring($rootWithSeparator.Length).Replace([IO.Path]::DirectorySeparatorChar, '/')
	}
}

function Get-FileLines
{
	param([Collections.Generic.Dictionary[string, object]]$Cache, [string]$FullPath)

	if (-not $Cache.ContainsKey($FullPath))
	{
		$lines = $null
		if ([IO.File]::Exists($FullPath))
		{
			try
			{
				$lines = [IO.File]::ReadAllLines($FullPath)
			}
			catch
			{
				Write-Diagnostic "Cited file could not be read: $FullPath"
				$lines = $null
			}
		}

		$Cache[$FullPath] = $lines
	}

	$cached = $Cache[$FullPath]
	if ($null -eq $cached)
	{
		return $null
	}

	# The unary comma keeps a one-line file's single-element array from unrolling to a bare string.
	return , $cached
}

function Get-CitationRecords
{
	param([string[]]$PlanLines)

	$records = [Collections.Generic.List[object]]::new()
	$cache = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::OrdinalIgnoreCase)
	for ($index = 0; $index -lt $PlanLines.Count; ++$index)
	{
		foreach ($match in [regex]::Matches($PlanLines[$index], '`([^`]+)`'))
		{
			$token = $match.Groups[1].Value
			if ($token -cnotmatch $script:CitationPattern)
			{
				continue
			}

			$separator = $token.IndexOf(':')
			$pathPart = if ($separator -lt 0) { $token } else { $token.Substring(0, $separator) }
			$resolved = Get-RepositoryRelativePath $pathPart
			if ($null -eq $resolved)
			{
				continue
			}

			# A range citation resolves at its start line.
			$lineNumber = $null
			if ($separator -ge 0)
			{
				$startText = $token.Substring($separator + 1).Split('-')[0]
				$parsed = 0
				if ([int]::TryParse($startText, [ref]$parsed))
				{
					$lineNumber = $parsed
				}
			}

			$lines = Get-FileLines $cache $resolved.fullPath
			$pathExists = $null -ne $lines -or [IO.File]::Exists($resolved.fullPath)
			$lineExists = $null
			if ($null -ne $lineNumber)
			{
				$lineExists = $null -ne $lines -and $lineNumber -ge 1 -and $lineNumber -le $lines.Count
			}

			$null = $records.Add([pscustomobject]@{
				citation = $token
				planLine = $index + 1
				path = $resolved.relativePath
				pathExists = $pathExists
				lineNumber = $lineNumber
				lineExists = $lineExists
			})
		}
	}

	return , $records
}

function Get-CardPayload
{
	# Surface form only, whole-file: whether each field is written down, never whether its text is any
	# good. With no 'execution card' line anywhere there is no card to judge, so every field is missing.
	param([string[]]$PlanLines)

	$present = @($PlanLines | Where-Object { $_ -match 'execution card' }).Count -gt 0
	if (-not $present)
	{
		return [ordered]@{
			present = $false
			missingFields = @($script:CardFields | ForEach-Object { $_.Name })
		}
	}

	$missing = [Collections.Generic.List[string]]::new()
	foreach ($field in $script:CardFields)
	{
		$found = $false
		for ($index = 0; $index -lt $PlanLines.Count -and -not $found; ++$index)
		{
			if ($PlanLines[$index] -cnotmatch $field.Pattern)
			{
				continue
			}
			if (-not $field.Section)
			{
				$found = $true
				continue
			}
			# A heading counts only when it has body text before the next heading or the end of the file.
			for ($scan = $index + 1; $scan -lt $PlanLines.Count -and $PlanLines[$scan] -cnotmatch '^#{1,6}\s'; ++$scan)
			{
				if (-not [string]::IsNullOrWhiteSpace($PlanLines[$scan]))
				{
					$found = $true
					break
				}
			}
		}

		if (-not $found)
		{
			$null = $missing.Add($field.Name)
		}
	}

	return [ordered]@{
		present = $true
		missingFields = @($missing)
	}
}

function Convert-ToOutputCitation
{
	param([object]$Record)

	$citation = New-CappedText $Record.citation $script:MaximumTextLength
	$path = New-CappedText $Record.path $script:MaximumTextLength
	return [pscustomobject][ordered]@{
		citation = $citation.text
		citationTruncated = $citation.truncated
		planLine = $Record.planLine
		path = $path.text
		pathTruncated = $path.truncated
		pathExists = $Record.pathExists
		lineNumber = $Record.lineNumber
		lineExists = $Record.lineExists
	}
}

function Complete-PlanCitations
{
	param([int]$ExitCode, [string]$Status, [string]$Code, [string]$Message)

	$script:Result.status = $Status
	$script:Result.code = $Code
	$script:Result.message = (New-CappedText $Message $script:MaximumMessageLength).text
	[Console]::Out.Write(($script:Result | ConvertTo-Json -Depth 8 -Compress))
	exit $ExitCode
}

function Set-CitationPayload
{
	param([Collections.Generic.List[object]]$Records)

	# Only unresolved records are emitted, so the result size tracks the leads an auditor must chase
	# instead of the citation count. The comparisons are explicit against $false because a citation
	# without a ':line' part leaves lineExists $null, and that record resolved.
	$unresolvedCount = @($Records | Where-Object { $_.pathExists -eq $false -or $_.lineExists -eq $false }).Count
	$items = [Collections.Generic.List[object]]::new()
	foreach ($record in $Records | Select-Object -First $script:MaximumCitations)
	{
		if ($record.pathExists -eq $false -or $record.lineExists -eq $false)
		{
			$null = $items.Add((Convert-ToOutputCitation $record))
		}
	}

	# Shed the lowest-priority records until the whole envelope fits the output cap. Both counts and
	# truncated speak about unresolved records only: an unresolved record the citation cap or this loop
	# dropped stays counted, and a resolved record never sets them.
	while ($true)
	{
		$script:Result.citations = [ordered]@{
			items = @($items)
			totalCount = $Records.Count
			omittedCount = $unresolvedCount - $items.Count
		}
		$script:Result.truncated = $items.Count -lt $unresolvedCount -or @($items | Where-Object { $_.citationTruncated -or $_.pathTruncated }).Count -gt 0
		$json = $script:Result | ConvertTo-Json -Depth 8 -Compress
		if ([Text.Encoding]::UTF8.GetByteCount($json) -le $script:MaximumOutputBytes -or $items.Count -eq 0)
		{
			return
		}
		$items.RemoveAt($items.Count - 1)
	}
}

[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)

try
{
	if ([string]::IsNullOrWhiteSpace($PlanPath))
	{
		Complete-PlanCitations 1 'error' 'input.plan-invalid' 'PlanPath must name one plan file.'
	}

	try
	{
		$planFullPath = if ([IO.Path]::IsPathFullyQualified($PlanPath))
		{
			[IO.Path]::GetFullPath($PlanPath)
		}
		else
		{
			[IO.Path]::GetFullPath((Join-Path $script:RepoRoot $PlanPath))
		}
	}
	catch
	{
		Complete-PlanCitations 1 'error' 'input.plan-invalid' "Plan path cannot be resolved: $PlanPath"
	}

	if (-not [IO.File]::Exists($planFullPath))
	{
		Complete-PlanCitations 1 'error' 'input.plan-missing' "Plan file does not exist: $PlanPath"
	}

	try
	{
		$planLines = [IO.File]::ReadAllLines($planFullPath)
	}
	catch
	{
		Complete-PlanCitations 1 'error' 'input.plan-read-failed' "Plan file could not be read: $PlanPath"
	}

	$planResolved = Get-RepositoryRelativePath ([IO.Path]::GetRelativePath($script:RepoRoot, $planFullPath))
	$planReportedPath = if ($null -eq $planResolved) { $planFullPath.Replace([IO.Path]::DirectorySeparatorChar, '/') } else { $planResolved.relativePath }
	$planReported = New-CappedText $planReportedPath $script:MaximumTextLength
	$script:Result.plan = [ordered]@{
		path = $planReported.text
		pathTruncated = $planReported.truncated
		lineCount = $planLines.Count
	}
	$script:Result.headings = [ordered]@{
		inScopePresent = @($planLines | Where-Object { $_ -cmatch '^##\s+In scope\s*$' }).Count -gt 0
		outOfScopePresent = @($planLines | Where-Object { $_ -cmatch '^##\s+Out of scope\s*$' }).Count -gt 0
	}
	$script:Result.card = Get-CardPayload $planLines

	Set-CitationPayload (Get-CitationRecords $planLines)
	Complete-PlanCitations 0 'pass' 'ok' 'Plan citation lookup completed.'
}
catch
{
	Write-Diagnostic $_.Exception.Message
	Complete-PlanCitations 1 'error' 'internal.error' 'The plan citation lookup encountered an internal error.'
}
