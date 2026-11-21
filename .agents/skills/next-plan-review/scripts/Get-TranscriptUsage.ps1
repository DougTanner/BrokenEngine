# Sums the usage counters one Claude Code or native `agy` JSONL transcript, given as a path, records on its assistant
# replies, per model. The transcript is untrusted data: each nonblank line is only parsed as JSON. A line that does not
# parse to one JSON object is malformed. The supported envelopes are:
# - Claude Code: `type` is `assistant` and `message` is an object; such a record with any other `message` is malformed,
#   and fragments group by `message.id`. A group resolves when every fragment has a nonempty string `message.model` and
#   the four `message.usage` counters `input_tokens`, `output_tokens`, `cache_read_input_tokens`, and
#   `cache_creation_input_tokens` as nonnegative JSON integers, and all fragments share model and input-side counters;
#   it counts once, with the largest `output_tokens`.
# - Antigravity CLI (`agy`): `type` is `PLANNER_RESPONSE`; each record counts as one fragment, resolving when `status`
#   is `DONE`, `model` is a nonempty string, and `input_tokens`, `output_tokens`, and `cache_read_tokens` are nonnegative
#   JSON integers (`cacheCreationInputTokens` is 0).
# Every other valid JSON object record is ignored; any non-resolving fragment or group is unresolved and contributes
# nothing. Output is one compact JSON object:
# `status` — `complete` (fragments found, none unresolved or malformed), `partial` (fragments found, some unresolved or
#   malformed), or `unsupported` (no fragment, or the file could not be read).
# `reason` — `no-supported-envelope` or `read-failed` when `unsupported`, otherwise null.
# `assistantFragmentCount`, `uniqueMessageCount` (resolved messages/groups), `duplicateFragmentCount` (fragments beyond
#   the first in every group), `unresolvedMessageCount`, `malformedLineCount`; the first is the sum of the next three.
# `models` — an array sorted by ordinal model name, each `model`, `messageCount`, `inputTokens`, `outputTokens`,
#   `cacheReadInputTokens`, `cacheCreationInputTokens` summed over its resolved messages.
# A read failure keeps only the malformed lines already counted. No message text, path, or error text is ever written;
# the only transcript strings in the output are model names.

[CmdletBinding()]
param(
	[Parameter(Mandatory)][string] $TranscriptPath
)

$ErrorActionPreference = 'Stop'

$sharedScripts = Join-Path $PSScriptRoot '..\..\..\scripts'
if (-not (Test-Path -LiteralPath (Join-Path $sharedScripts 'AgentTranscriptCommon.psm1'))) {
	$sharedScripts = Join-Path $PSScriptRoot '..\..\..\..\.agents\scripts'
}
Import-Module (Join-Path $sharedScripts 'AgentTranscriptCommon.psm1') -Force

function Test-NonemptyString($Value) {
	return $Value -is [string] -and $Value.Length -ne 0
}

function Test-Counter($Value) {
	return ($Value -is [long] -or $Value -is [Numerics.BigInteger]) -and $Value -ge 0
}

function New-Fragment($Message) {
	$usage = Get-AgentTranscriptField $Message 'usage'
	$fragment = [pscustomobject]@{
		Model = Get-AgentTranscriptField $Message 'model'
		Input = Get-AgentTranscriptField $usage 'input_tokens'
		Output = Get-AgentTranscriptField $usage 'output_tokens'
		CacheRead = Get-AgentTranscriptField $usage 'cache_read_input_tokens'
		CacheCreation = Get-AgentTranscriptField $usage 'cache_creation_input_tokens'
		Valid = $false
	}
	$fragment.Valid = (Test-NonemptyString $fragment.Model) -and (Test-Counter $fragment.Input) -and
		(Test-Counter $fragment.Output) -and (Test-Counter $fragment.CacheRead) -and (Test-Counter $fragment.CacheCreation)
	return $fragment
}

function Add-ModelUsage(
	[Collections.Generic.Dictionary[string, object]] $Totals,
	[string] $Model,
	[Numerics.BigInteger] $InputTokens,
	[Numerics.BigInteger] $OutputTokens,
	[Numerics.BigInteger] $CacheReadTokens,
	[Numerics.BigInteger] $CacheCreationTokens
) {
	if (-not $Totals.ContainsKey($Model)) {
		$Totals[$Model] = [pscustomobject]@{
			MessageCount = [long] 0
			Input = [Numerics.BigInteger]::Zero
			Output = [Numerics.BigInteger]::Zero
			CacheRead = [Numerics.BigInteger]::Zero
			CacheCreation = [Numerics.BigInteger]::Zero
		}
	}
	$total = $Totals[$Model]
	$total.MessageCount++
	$total.Input += $InputTokens
	$total.Output += $OutputTokens
	$total.CacheRead += $CacheReadTokens
	$total.CacheCreation += $CacheCreationTokens
}

function Write-Usage([string] $Status, [string] $Reason, [long] $Fragments, [long] $Unique, [long] $Unresolved, [long] $Malformed, $Models) {
	$result = [ordered]@{
		status = $Status
		reason = if ([string]::IsNullOrEmpty($Reason)) { $null } else { $Reason }
		assistantFragmentCount = $Fragments
		uniqueMessageCount = $Unique
		duplicateFragmentCount = $Fragments - $Unique - $Unresolved
		unresolvedMessageCount = $Unresolved
		malformedLineCount = $Malformed
		models = $Models
	}
	[Console]::Out.Write(($result | ConvertTo-Json -Compress -Depth 5))
}

$groups = [Collections.Generic.Dictionary[string, Collections.Generic.List[object]]]::new([StringComparer]::Ordinal)
$totals = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::Ordinal)
$fragmentCount = 0
$idlessCount = 0
$agyUniqueCount = 0
$agyUnresolvedCount = 0
$malformedCount = 0
$stream = $null
$reader = $null
try {
	$resolved = Resolve-AgentAgyTranscriptPath -TranscriptPath $TranscriptPath
	if ($resolved.Status -cne 'safe' -and $resolved.Status -cne 'passthrough') {
		Write-Usage 'unsupported' 'read-failed' 0 0 0 $malformedCount @()
		exit 0
	}
	$stream = [IO.FileStream]::new($resolved.Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
	$reader = [IO.StreamReader]::new($stream)
	while ($null -ne ($line = $reader.ReadLine())) {
		if ([string]::IsNullOrWhiteSpace($line)) { continue }
		try { $record = ConvertFrom-Json -InputObject $line -NoEnumerate }
		catch {
			$malformedCount++
			continue
		}
		if ($record -isnot [Management.Automation.PSCustomObject]) {
			$malformedCount++
			continue
		}
		$type = Get-AgentTranscriptField $record 'type'
		if ($type -isnot [string]) { continue }
		if ($type -ceq 'assistant') {
			$message = Get-AgentTranscriptField $record 'message'
			if ($message -isnot [Management.Automation.PSCustomObject]) {
				$malformedCount++
				continue
			}
			$fragmentCount++
			$id = Get-AgentTranscriptField $message 'id'
			if (-not (Test-NonemptyString $id)) {
				$idlessCount++
				continue
			}
			if (-not $groups.ContainsKey($id)) { $groups[$id] = [Collections.Generic.List[object]]::new() }
			$groups[$id].Add((New-Fragment $message))
		}
		elseif ($type -ceq 'PLANNER_RESPONSE') {
			$fragmentCount++
			$replyStatus = Get-AgentTranscriptField $record 'status'
			$model = Get-AgentTranscriptField $record 'model'
			$inputTokens = Get-AgentTranscriptField $record 'input_tokens'
			$outputTokens = Get-AgentTranscriptField $record 'output_tokens'
			$cacheReadTokens = Get-AgentTranscriptField $record 'cache_read_tokens'
			if ($replyStatus -is [string] -and $replyStatus -ceq 'DONE' -and (Test-NonemptyString $model) -and
				(Test-Counter $inputTokens) -and (Test-Counter $outputTokens) -and (Test-Counter $cacheReadTokens)) {
				$agyUniqueCount++
				Add-ModelUsage $totals $model ([Numerics.BigInteger] $inputTokens) ([Numerics.BigInteger] $outputTokens) ([Numerics.BigInteger] $cacheReadTokens) ([Numerics.BigInteger]::Zero)
			}
			else {
				$agyUnresolvedCount++
			}
		}
	}
}
catch {
	Write-Usage 'unsupported' 'read-failed' 0 0 0 $malformedCount @()
	exit 0
}
finally {
	if ($null -ne $reader) { $reader.Dispose() }
	if ($null -ne $stream) { $stream.Dispose() }
}

if ($fragmentCount -eq 0) {
	Write-Usage 'unsupported' 'no-supported-envelope' 0 0 0 $malformedCount @()
	exit 0
}

$uniqueCount = $agyUniqueCount
$unresolvedCount = $idlessCount + $agyUnresolvedCount
foreach ($group in $groups.Values) {
	$first = $group[0]
	$resolved = $true
	$output = [Numerics.BigInteger]::Zero
	foreach ($fragment in $group) {
		if (-not $fragment.Valid -or $fragment.Model -cne $first.Model -or $fragment.Input -ne $first.Input -or
			$fragment.CacheRead -ne $first.CacheRead -or $fragment.CacheCreation -ne $first.CacheCreation) {
			$resolved = $false
			break
		}
		if ([Numerics.BigInteger] $fragment.Output -gt $output) { $output = [Numerics.BigInteger] $fragment.Output }
	}
	if (-not $resolved) {
		$unresolvedCount++
		continue
	}
	$uniqueCount++
	Add-ModelUsage $totals $first.Model ([Numerics.BigInteger] $first.Input) $output ([Numerics.BigInteger] $first.CacheRead) ([Numerics.BigInteger] $first.CacheCreation)
}

[string[]] $modelNames = @($totals.Keys)
[Array]::Sort($modelNames, [StringComparer]::Ordinal)
$models = [Collections.Generic.List[object]]::new()
foreach ($name in $modelNames) {
	$total = $totals[$name]
	$models.Add([ordered]@{
		model = $name
		messageCount = $total.MessageCount
		inputTokens = $total.Input
		outputTokens = $total.Output
		cacheReadInputTokens = $total.CacheRead
		cacheCreationInputTokens = $total.CacheCreation
	})
}

$status = if ($unresolvedCount -eq 0 -and $malformedCount -eq 0) { 'complete' } else { 'partial' }
Write-Usage $status $null $fragmentCount $uniqueCount $unresolvedCount $malformedCount $models
