# Sums the usage counters one Claude Code JSONL transcript, given as a path, records on its assistant replies, per model.
# The transcript is untrusted data: each nonblank line is only parsed as JSON. A line that does not parse to one JSON
# object is malformed. The supported envelope is a record whose `type` is the string `assistant` and whose `message` is
# an object; such a record with any other `message` is malformed, and every other record is ignored.
# The host writes one reply as several envelopes (fragments), so fragments group by `message.id`. A group resolves when
# every fragment has a nonempty string `message.model` and the four `message.usage` counters `input_tokens`,
# `output_tokens`, `cache_read_input_tokens`, and `cache_creation_input_tokens` as nonnegative JSON integers, and all
# fragments share the model and the three input-side counters; it counts once, with the largest `output_tokens`. Any
# other group, and each fragment without a nonempty string `message.id`, is unresolved and contributes nothing.
# Output is one compact JSON object:
# `status` — `complete` (fragments found, none unresolved or malformed), `partial` (fragments found, some unresolved or
#   malformed), or `unsupported` (no fragment, or the file could not be read).
# `reason` — `no-supported-envelope` or `read-failed` when `unsupported`, otherwise null.
# `assistantFragmentCount`, `uniqueMessageCount` (resolved groups), `duplicateFragmentCount` (fragments beyond the first
#   in every group), `unresolvedMessageCount`, `malformedLineCount`; the first is the sum of the next three.
# `models` — an array sorted by ordinal model name, each `model`, `messageCount`, `inputTokens`, `outputTokens`,
#   `cacheReadInputTokens`, `cacheCreationInputTokens` summed over its resolved messages.
# A read failure keeps only the malformed lines already counted. No message text, path, or error text is ever written;
# the only transcript strings in the output are model names.

[CmdletBinding()]
param(
	[Parameter(Mandatory)][string] $TranscriptPath
)

$ErrorActionPreference = 'Stop'

function Get-Field($Object, [string] $Name) {
	if ($Object -isnot [Management.Automation.PSCustomObject]) { return $null }
	$property = $Object.PSObject.Properties[$Name]
	if ($null -eq $property) { return $null }
	return $property.Value
}

function Test-NonemptyString($Value) {
	return $Value -is [string] -and $Value.Length -ne 0
}

function Test-Counter($Value) {
	return ($Value -is [long] -or $Value -is [Numerics.BigInteger]) -and $Value -ge 0
}

function New-Fragment($Message) {
	$usage = Get-Field $Message 'usage'
	$fragment = [pscustomobject]@{
		Model = Get-Field $Message 'model'
		Input = Get-Field $usage 'input_tokens'
		Output = Get-Field $usage 'output_tokens'
		CacheRead = Get-Field $usage 'cache_read_input_tokens'
		CacheCreation = Get-Field $usage 'cache_creation_input_tokens'
		Valid = $false
	}
	$fragment.Valid = (Test-NonemptyString $fragment.Model) -and (Test-Counter $fragment.Input) -and
		(Test-Counter $fragment.Output) -and (Test-Counter $fragment.CacheRead) -and (Test-Counter $fragment.CacheCreation)
	return $fragment
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
$fragmentCount = 0
$idlessCount = 0
$malformedCount = 0
$stream = $null
$reader = $null
try {
	$stream = [IO.FileStream]::new($TranscriptPath, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
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
		$type = Get-Field $record 'type'
		if ($type -isnot [string] -or $type -cne 'assistant') { continue }
		$message = Get-Field $record 'message'
		if ($message -isnot [Management.Automation.PSCustomObject]) {
			$malformedCount++
			continue
		}
		$fragmentCount++
		$id = Get-Field $message 'id'
		if (-not (Test-NonemptyString $id)) {
			$idlessCount++
			continue
		}
		if (-not $groups.ContainsKey($id)) { $groups[$id] = [Collections.Generic.List[object]]::new() }
		$groups[$id].Add((New-Fragment $message))
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

$totals = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::Ordinal)
$uniqueCount = 0
$unresolvedCount = $idlessCount
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
	if (-not $totals.ContainsKey($first.Model)) {
		$totals[$first.Model] = [pscustomobject]@{
			MessageCount = [long] 0
			Input = [Numerics.BigInteger]::Zero
			Output = [Numerics.BigInteger]::Zero
			CacheRead = [Numerics.BigInteger]::Zero
			CacheCreation = [Numerics.BigInteger]::Zero
		}
	}
	$total = $totals[$first.Model]
	$total.MessageCount++
	$total.Input += [Numerics.BigInteger] $first.Input
	$total.Output += $output
	$total.CacheRead += [Numerics.BigInteger] $first.CacheRead
	$total.CacheCreation += [Numerics.BigInteger] $first.CacheCreation
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
