[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$script:Stage = 'startup'
$measure = Join-Path $PSScriptRoot 'Measure-SessionContext.ps1'
$fixtureRoot = Join-Path ([IO.Path]::GetTempPath()) "context-efficiency-fixture-$([guid]::NewGuid().ToString('N'))"
$tempRootFull = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$fixtureRootFull = [IO.Path]::GetFullPath($fixtureRoot)

function Assert-True([bool] $Condition, [string] $Message) {
	if (-not $Condition) { throw "[$script:Stage] $Message" }
}

function New-ToolUseLine([string] $Id, [string] $Name, [hashtable] $ToolInput, [bool] $Sidechain) {
	return [ordered]@{ type = 'assistant'; isSidechain = $Sidechain; message = [ordered]@{ content = @([ordered]@{ type = 'tool_use'; id = $Id; name = $Name; input = $ToolInput }) } } | ConvertTo-Json -Depth 12 -Compress
}

function New-ToolResultLine([string] $Id, $Content, [bool] $Sidechain) {
	return [ordered]@{ type = 'user'; isSidechain = $Sidechain; message = [ordered]@{ content = @([ordered]@{ type = 'tool_result'; tool_use_id = $Id; content = $Content }) } } | ConvertTo-Json -Depth 12 -Compress
}

function New-TextBlocks([int[]] $Lengths) {
	return @($Lengths | ForEach-Object { [ordered]@{ type = 'text'; text = ('x' * $_) } })
}

function Set-Transcript([string] $Path, [string[]] $Lines) {
	[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Path)) | Out-Null
	[IO.File]::WriteAllText($Path, ($Lines -join "`n") + "`n", [Text.UTF8Encoding]::new($false))
}

function Invoke-Measure([string[]] $Arguments, [int] $ExpectedExit) {
	$output = @(& (Join-Path $PSHOME 'pwsh.exe') @('-NoLogo', '-NoProfile', '-File', $measure) @Arguments 2>&1)
	$exitCode = $LASTEXITCODE
	$json = ($output | ForEach-Object { [string] $_ }) -join [Environment]::NewLine
	Assert-True ($exitCode -eq $ExpectedExit) "Expected exit $ExpectedExit, got ${exitCode}: $json"
	try { return $json | ConvertFrom-Json -Depth 30 }
	catch { throw "[$script:Stage] Measurement did not return JSON: $json" }
}

try {
	[IO.Directory]::CreateDirectory($fixtureRoot) | Out-Null

	$mixed = Join-Path $fixtureRoot 'mixed.jsonl'
	Set-Transcript $mixed @(
		(New-ToolUseLine 'use-a' 'Bash' @{ command = 'git status' } $false)
		(New-ToolResultLine 'use-a' ('a' * 300) $false)
		(New-ToolUseLine 'use-b' 'Read' @{ file_path = 'C:\repo\File.cs' } $false)
		(New-ToolResultLine 'use-b' (New-TextBlocks @(100, 50)) $false)
		(New-ToolUseLine 'use-c' 'Grep' @{ pattern = 'needle' } $false)
		(New-ToolResultLine 'use-c' ('c' * 500) $false)
		(New-ToolUseLine 'use-side' 'Bash' @{ command = 'subagent work' } $true)
		(New-ToolResultLine 'use-side' ('s' * 30000) $true)
		('{"type":"system","message":"ignored"}')
		('{"type":"user","message":{"content":"plain text turn"}}')
	)

	$script:Stage = 'mixed sizes, ranking, and exclusions'
	$response = Invoke-Measure @('-TranscriptPath', $mixed) 0
	Assert-True ($response.schemaVersion -ceq 'broken-engine-context-efficiency/v1') "Unexpected schema version: $($response.schemaVersion)"
	Assert-True ($response.sessionId -ceq 'mixed') "Unexpected session label: $($response.sessionId)"
	Assert-True ($response.lineCount -eq 10) "Expected 10 counted lines, got $($response.lineCount)."
	Assert-True ($response.toolResultCount -eq 3) "Sidechain or non-message lines were counted: $($response.toolResultCount)."
	Assert-True ($response.totalChars -eq 950) "Expected 950 total characters, got $($response.totalChars)."
	Assert-True ($response.verdict -ceq 'pass') "Expected the default threshold to pass, got $($response.verdict)."
	Assert-True ($response.overThresholdCount -eq 0) "Expected no breaches, got $($response.overThresholdCount)."
	Assert-True ($response.thresholds.perResultChars -eq 20000) "Unexpected default threshold: $($response.thresholds.perResultChars)."
	$ordered = @($response.topResults | ForEach-Object { "$($_.rank):$($_.toolName):$($_.chars)" })
	Assert-True (($ordered -join '|') -ceq '1:Grep:500|2:Bash:300|3:Read:150') "Unexpected ranking: $($ordered -join '|')"
	Assert-True ($response.topResults[0].inputSummary -ceq 'needle') "Unexpected selector: $($response.topResults[0].inputSummary)"
	Assert-True ($response.topResults[0].toolUseId -ceq 'use-c') "Unexpected tool use id: $($response.topResults[0].toolUseId)"

	$script:Stage = 'per-result breach verdict'
	$response = Invoke-Measure @('-TranscriptPath', $mixed, '-ThresholdChars', '400') 0
	Assert-True ($response.verdict -ceq 'needs-review') "Expected needs-review, got $($response.verdict)."
	Assert-True ($response.overThresholdCount -eq 1) "Expected one breach, got $($response.overThresholdCount)."

	$script:Stage = 'breaches outrank the Top cap'
	$response = Invoke-Measure @('-TranscriptPath', $mixed, '-Top', '1', '-ThresholdChars', '150') 0
	Assert-True ($response.overThresholdCount -eq 3) "Expected three breaches, got $($response.overThresholdCount)."
	# -Top never hides a breach: every above-threshold result gets its own row.
	Assert-True ($response.topResults.Count -eq 3) "Expected every breach reported, got $($response.topResults.Count) rows."
	Assert-True (@($response.topResults | Where-Object { $_.overThreshold }).Count -eq 3) 'Expected every reported row marked overThreshold.'
	Assert-True ($response.breachRowsTruncated -eq $false) 'Three breaches must not set breachRowsTruncated.'

	$script:Stage = 'below-threshold rows fill the remaining Top slots'
	$response = Invoke-Measure @('-TranscriptPath', $mixed, '-Top', '3', '-ThresholdChars', '400') 0
	$marked = @($response.topResults | ForEach-Object { "$($_.chars):$($_.overThreshold)" })
	Assert-True (($marked -join '|') -ceq '500:True|300:False|150:False') "Unexpected rows: $($marked -join '|')"

	$script:Stage = 'breach row cap'
	$manyBreaches = Join-Path $fixtureRoot 'many-breaches.jsonl'
	$breachLines = @()
	foreach ($index in 1..27) {
		$breachLines += (New-ToolUseLine "use-$index" 'Bash' @{ command = "command $index" } $false)
		$breachLines += (New-ToolResultLine "use-$index" ('b' * (200 + $index)) $false)
	}
	Set-Transcript $manyBreaches $breachLines
	$response = Invoke-Measure @('-TranscriptPath', $manyBreaches, '-ThresholdChars', '100') 0
	Assert-True ($response.overThresholdCount -eq 27) "Expected 27 breaches, got $($response.overThresholdCount)."
	Assert-True ($response.topResults.Count -eq 25) "Expected the 25-row cap, got $($response.topResults.Count) rows."
	Assert-True ($response.breachRowsTruncated -eq $true) 'Expected breachRowsTruncated for 27 breaches.'
	Assert-True (@($response.topResults | Where-Object { $_.overThreshold }).Count -eq 25) 'Expected every capped row marked overThreshold.'

	$script:Stage = 'transcript-path redaction and summary truncation'
	$redaction = Join-Path $fixtureRoot 'redaction.jsonl'
	$longCommand = 'Get-Content C:\Users\someone\.claude\projects\C--Users-someone-repo\11111111-2222-3333-4444-555555555555.jsonl ' + ('y' * 400)
	Set-Transcript $redaction @(
		(New-ToolUseLine 'use-r' 'PowerShell' @{ command = $longCommand } $false)
		(New-ToolResultLine 'use-r' 'result text' $false)
	)
	$response = Invoke-Measure @('-TranscriptPath', $redaction) 0
	$summary = [string] $response.topResults[0].inputSummary
	Assert-True ($summary -notmatch 'projects') "The selector still carries a transcript path: $summary"
	Assert-True ($summary -match '<transcript-path>') "The transcript path was not redacted: $summary"
	Assert-True ($summary.Length -le 160) "The selector was not truncated: $($summary.Length) characters."

	$script:Stage = 'missing transcript'
	$response = Invoke-Measure @('-TranscriptPath', (Join-Path $fixtureRoot 'absent.jsonl')) 2
	Assert-True ($response.status -ceq 'blocked') "Expected a blocked status, got $($response.status)."
	Assert-True ($response.code -ceq 'transcript.not-found') "Unexpected code: $($response.code)"

	$script:Stage = 'malformed line'
	$malformed = Join-Path $fixtureRoot 'malformed.jsonl'
	Set-Transcript $malformed @(
		(New-ToolUseLine 'use-m' 'Bash' @{ command = 'git status' } $false)
		'{ malformed'
	)
	$response = Invoke-Measure @('-TranscriptPath', $malformed) 1
	Assert-True ($response.status -ceq 'error') "Expected an error status, got $($response.status)."
	Assert-True ($response.code -ceq 'transcript.malformed-line') "Unexpected code: $($response.code)"

	$script:Stage = 'transcript source selection'
	$response = Invoke-Measure @('-TranscriptPath', $mixed, '-SessionId', '11111111-2222-3333-4444-555555555555') 1
	Assert-True ($response.code -ceq 'input.conflicting-source') "Unexpected code: $($response.code)"
	$response = Invoke-Measure @() 1
	Assert-True ($response.code -ceq 'input.missing-source') "Unexpected code: $($response.code)"

	Write-Output 'PASS: Measure-SessionContext fixture completed.'
}
finally {
	if ($fixtureRootFull.StartsWith($tempRootFull, [StringComparison]::OrdinalIgnoreCase) -and (Split-Path -Leaf $fixtureRootFull).StartsWith('context-efficiency-fixture-', [StringComparison]::Ordinal)) {
		Remove-Item -LiteralPath $fixtureRootFull -Force -Recurse -ErrorAction SilentlyContinue
	}
}
