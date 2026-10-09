# Summarizes one vcperf `/timetrace` JSON of a full BrokenEngineSandbox Client and Server rebuild for
# /optimize-build-time, and with -Baseline compares it against an earlier summary.
#
# Inputs: -Trace is the vcperf `build.json`; -BuildEnvelope is the /compile builder's envelope file,
# whose fenced `broken-engine-build-result/v1` blocks supply the wall-clock time and the worktree
# root; -Baseline is an earlier `summary.json` from this script.
#
# Stdout contract: exactly one JSON object, schema `broken-engine-build-trace-summary/v1`, also
# written as `summary.json` beside the trace. Diagnostics go to stderr. Back-end time is a total
# only: back-end passes carry no translation-unit name, and /MP nests compile passes under a shared
# CL invocation. vcperf keeps a function or template entry only at 10 ms or more, so a comparison
# row for one absent from this trace has `after` and `percent` null and `belowFloor` true.
# Exit codes: 0 pass, 2 structured block (missing input, or a trace whose verified shape did not
# hold), 1 internal error.
#
# Trace shape (vcperf TimeTrace generator and Build Insights SDK 1.5.1 names): `traceEvents` array
# order is the nesting - "B" opens a parent, "E" closes it, "X" is a leaf with `dur` - and pid/tid
# are remapped, so they are ignored. Times are microseconds.
[CmdletBinding()]
param(
	[Parameter(Mandatory)][string] $Trace,
	[Parameter(Mandatory)][string] $BuildEnvelope,
	[string] $Baseline
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot '..\..\..\scripts\AgentScriptCommon.psm1') -Force

$script:Schema = 'broken-engine-build-trace-summary/v1'
$script:BuildResultSchema = 'broken-engine-build-result/v1'
$script:RankingSize = 20
$script:BlockedCode = $null
$script:SummaryPath = $null
# Every activity name the SDK defines (CppBuildInsights.hpp Info<T>::NAME). vcperf renames only
# invocations, front-end files (absolute path), kept functions, resolved template instantiations
# (symbol name), and threads (parent name plus `Thread`), so a name outside this set that is not a
# path is a function or template symbol.
$script:SdkActivityNames = [Collections.Generic.HashSet[string]]::new([string[]]@(
		'Activity', 'Invocation', 'InvocationGroup', 'Compiler', 'Linker', 'LinkerGroup', 'CompilerPass',
		'FrontEndPass', 'CodeAnalysisPass', 'CodeAnalysisPlugins', 'AstCreation', 'EspXEngineCfgBuild',
		'EspXEnginePathSimulation', 'EspXEngineChecks', 'BackEndPass', 'FrontEndFile', 'FrontEndFileGroup',
		'TemplateInstantiation', 'TemplateInstantiationGroup', 'Function', 'DiaInterfaceCall', 'LinkerPass',
		'Pass1', 'Pass2', 'PreLTCGOptRef', 'LTCG', 'OptRef', 'OptLBR', 'OptICF', 'C1DLL', 'C2DLL',
		'WholeProgramAnalysis', 'CodeGeneration', 'BottomUp', 'TopDown', 'Thread', 'CodeAnalysisFunction',
		'CodeAnalysisPREfastFpaFunction'), [StringComparer]::Ordinal)

class PassRecord {
	[string] $TranslationUnit
	[long] $Duration
}

class InvocationRecord {
	[string] $Name
	[bool] $IsCompiler
	[bool] $Kept
	[string] $Target
	[long] $Duration
	[Collections.Generic.List[PassRecord]] $FrontEndPasses = [Collections.Generic.List[PassRecord]]::new()
	[Collections.Generic.List[PassRecord]] $BackEndPasses = [Collections.Generic.List[PassRecord]]::new()
}

class AggregateRecord {
	[long] $Duration
	[int] $Count
}

class TraceFrame {
	[string] $Name
	[long] $Start
	# 0 none, 1 invocation, 2 pass, 3 header, 4 template instantiation, 5 function.
	[int] $Role
	[InvocationRecord] $Invocation
	[PassRecord] $Pass
	# 0 outside a pass, 1 FrontEndPass, 2 BackEndPass.
	[int] $PassKind
	[bool] $InPathNamed
	[bool] $UnderTranslationUnit
}

function Write-TraceDiagnostic([string] $Message) {
	[Console]::Error.WriteLine("measure-build-trace: $Message")
}

function Stop-TraceSummary([string] $Code, [string] $Message) {
	$script:BlockedCode = $Code
	throw $Message
}

function Complete-TraceSummary([int] $ExitCode, [Collections.Specialized.OrderedDictionary] $Result) {
	$json = $Result | ConvertTo-Json -Depth 8 -Compress
	if ($null -ne $script:SummaryPath) { [IO.File]::WriteAllText($script:SummaryPath, $json, [Text.UTF8Encoding]::new($false)) }
	[Console]::Out.Write($json)
	exit $ExitCode
}

function Get-Milliseconds([long] $Microseconds) {
	return [math]::Round($Microseconds / 1000.0, 1)
}

function Get-Percent($Before, $After) {
	if ($null -eq $Before -or [double] $Before -eq 0) { return $null }
	return [math]::Round(([double] $Before - [double] $After) / [double] $Before * 100.0, 1)
}

function Add-Aggregate([Collections.Generic.Dictionary[string, AggregateRecord]] $Table, [string] $Key, [long] $Duration) {
	$record = $null
	if (-not $Table.TryGetValue($Key, [ref] $record)) {
		$record = [AggregateRecord]::new()
		$Table[$Key] = $record
	}
	$record.Duration += $Duration
	$record.Count++
}

function Get-NamedRanking([Collections.Generic.Dictionary[string, AggregateRecord]] $Table) {
	return ($Table.GetEnumerator() | Sort-Object { $_.Value.Duration } -Descending | Select-Object -First $script:RankingSize |
		ForEach-Object { [ordered]@{ name = $_.Key; ms = (Get-Milliseconds $_.Value.Duration); count = $_.Value.Count } })
}

function Get-TranslationUnitRanking([Collections.Generic.Dictionary[string, AggregateRecord]] $Table) {
	return ($Table.GetEnumerator() | Sort-Object { $_.Value.Duration } -Descending | Select-Object -First $script:RankingSize |
		ForEach-Object {
			$parts = $_.Key.Split('|', 2)
			[ordered]@{ target = $parts[0]; path = $parts[1]; ms = (Get-Milliseconds $_.Value.Duration) }
		})
}

function Get-InvocationTarget([string] $OutputPath) {
	$normalized = $OutputPath.Replace('/', '\')
	if ($normalized.IndexOf('\Build\BrokenEngineSandboxServer\', [StringComparison]::OrdinalIgnoreCase) -ge 0) { return 'Server' }
	if ($normalized.IndexOf('\Build\BrokenEngineSandbox\', [StringComparison]::OrdinalIgnoreCase) -ge 0) { return 'Client' }
	return $null
}

function Read-BuildEnvelopes([string] $Path) {
	if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { Stop-TraceSummary 'envelope.missing' "Build envelope file '$Path' does not exist." }
	$text = [IO.File]::ReadAllText($Path)
	$results = [Collections.Generic.List[object]]::new()
	foreach ($match in [regex]::Matches($text, '(?ms)^```[^\r\n]*\r?\n(.*?)\r?\n```')) {
		$parsed = $null
		try { $parsed = $match.Groups[1].Value | ConvertFrom-Json }
		catch { continue }
		if ($null -ne $parsed -and $parsed.PSObject.Properties['schemaVersion'] -and $parsed.schemaVersion -eq $script:BuildResultSchema) { $results.Add($parsed) }
	}
	if ($results.Count -eq 0) { Stop-TraceSummary 'envelope.no-build-result' "Build envelope file '$Path' holds no fenced $($script:BuildResultSchema) block." }
	return $results
}

$result = [ordered]@{
	schemaVersion = $script:Schema
	status = 'error'
	code = 'internal.error'
	message = 'Trace summary did not run.'
}

try {
	if (-not (Test-Path -LiteralPath $Trace -PathType Leaf)) { Stop-TraceSummary 'trace.missing' "Trace file '$Trace' does not exist." }
	$tracePath = [IO.Path]::GetFullPath($Trace)
	$script:SummaryPath = Join-Path ([IO.Path]::GetDirectoryName($tracePath)) 'summary.json'

	$envelopes = Read-BuildEnvelopes $BuildEnvelope
	$buildElapsedMs = [long] 0
	foreach ($envelope in $envelopes) { $buildElapsedMs += [long] $envelope.elapsedMilliseconds }
	$worktreeRoot = Get-AgentCanonicalPath ([string] $envelopes[0].worktreeRoot.path)

	$baselineSummary = $null
	if (-not [string]::IsNullOrWhiteSpace($Baseline)) {
		if (-not (Test-Path -LiteralPath $Baseline -PathType Leaf)) { Stop-TraceSummary 'baseline.missing' "Baseline summary '$Baseline' does not exist." }
		$baselineSummary = [IO.File]::ReadAllText($Baseline) | ConvertFrom-Json
		if ($baselineSummary.schemaVersion -ne $script:Schema -or $baselineSummary.status -ne 'pass') {
			Stop-TraceSummary 'baseline.invalid' "Baseline summary '$Baseline' is not a passing $($script:Schema) summary."
		}
	}

	$invocations = [Collections.Generic.List[InvocationRecord]]::new()
	$headers = [Collections.Generic.Dictionary[string, AggregateRecord]]::new([StringComparer]::OrdinalIgnoreCase)
	$templates = [Collections.Generic.Dictionary[string, AggregateRecord]]::new([StringComparer]::Ordinal)
	$functions = [Collections.Generic.Dictionary[string, AggregateRecord]]::new([StringComparer]::Ordinal)
	$stack = [Collections.Generic.Stack[TraceFrame]]::new()
	$root = [TraceFrame]::new()

	$stream = [IO.File]::OpenRead($tracePath)
	try {
		$document = [Text.Json.JsonDocument]::Parse($stream)
		try {
			foreach ($traceEvent in $document.RootElement.GetProperty('traceEvents').EnumerateArray()) {
				$phase = $traceEvent.GetProperty('ph').GetString()
				$frame = $null
				$duration = [long] 0
				if ($phase -eq 'E') {
					if ($stack.Count -eq 0) { Stop-TraceSummary 'trace.nesting' "An 'E' event has no open 'B' event." }
					$frame = $stack.Pop()
					$duration = $traceEvent.GetProperty('ts').GetInt64() - $frame.Start
				}
				else {
					$parent = if ($stack.Count -gt 0) { $stack.Peek() } else { $root }
					$name = $traceEvent.GetProperty('name').GetString()
					$frame = [TraceFrame]::new()
					$frame.Name = $name
					$frame.Start = $traceEvent.GetProperty('ts').GetInt64()
					$frame.Invocation = $parent.Invocation
					$frame.Pass = $parent.Pass
					$frame.PassKind = $parent.PassKind
					$frame.InPathNamed = $parent.InPathNamed
					$frame.UnderTranslationUnit = $parent.UnderTranslationUnit

					if ($name -match '^(CL|Link) Invocation \d+$') {
						$invocation = [InvocationRecord]::new()
						$invocation.Name = $name
						$invocation.IsCompiler = $Matches[1] -eq 'CL'
						$arguments = [Text.Json.JsonElement]::new()
						if ($traceEvent.TryGetProperty('args', [ref] $arguments)) {
							foreach ($argument in $arguments.EnumerateObject()) {
								if ($argument.Name -eq 'Working Directory') {
									$directory = Get-AgentCanonicalPath $argument.Value.GetString()
									$invocation.Kept = $directory.Equals($worktreeRoot, [StringComparison]::OrdinalIgnoreCase) -or
										$directory.StartsWith($worktreeRoot + '\', [StringComparison]::OrdinalIgnoreCase)
								}
								elseif ($argument.Name.StartsWith('File Output', [StringComparison]::Ordinal) -and $null -eq $invocation.Target) {
									$invocation.Target = Get-InvocationTarget $argument.Value.GetString()
								}
							}
						}
						$invocations.Add($invocation)
						$frame.Role = 1
						$frame.Invocation = $invocation
						$frame.Pass = $null
						$frame.PassKind = 0
						$frame.InPathNamed = $false
						$frame.UnderTranslationUnit = $false
					}
					elseif ($null -ne $frame.Invocation -and $frame.Invocation.Kept -and $frame.Invocation.IsCompiler) {
						if ($frame.PassKind -eq 0 -and ($name -eq 'FrontEndPass' -or $name -eq 'BackEndPass')) {
							$pass = [PassRecord]::new()
							if ($name -eq 'FrontEndPass') {
								$frame.Invocation.FrontEndPasses.Add($pass)
								$frame.PassKind = 1
							}
							else {
								$frame.Invocation.BackEndPasses.Add($pass)
								$frame.PassKind = 2
							}
							$frame.Role = 2
							$frame.Pass = $pass
						}
						elseif ($frame.PassKind -eq 1) {
							if ([IO.Path]::IsPathFullyQualified($name)) {
								if ($parent.UnderTranslationUnit) { $frame.Role = 3 }
								elseif (-not $parent.InPathNamed -and $null -eq $frame.Pass.TranslationUnit) {
									$frame.Pass.TranslationUnit = $name
									$frame.UnderTranslationUnit = $true
								}
								$frame.InPathNamed = $true
							}
							elseif ($name -eq 'TemplateInstantiation' -or (-not $script:SdkActivityNames.Contains($name) -and -not $name.EndsWith('Thread', [StringComparison]::Ordinal))) {
								$frame.Role = 4
							}
						}
						elseif ($frame.PassKind -eq 2 -and -not $script:SdkActivityNames.Contains($name) -and -not $name.EndsWith('Thread', [StringComparison]::Ordinal)) {
							$frame.Role = 5
						}
					}

					if ($phase -eq 'B') {
						$stack.Push($frame)
						continue
					}
					$duration = $traceEvent.GetProperty('dur').GetInt64()
				}

				switch ($frame.Role) {
					1 { $frame.Invocation.Duration = $duration }
					2 { $frame.Pass.Duration = $duration }
					3 { Add-Aggregate $headers $frame.Name $duration }
					4 { Add-Aggregate $templates $frame.Name $duration }
					5 { Add-Aggregate $functions $frame.Name $duration }
				}
			}
		}
		finally { $document.Dispose() }
	}
	finally { $stream.Dispose() }
	if ($stack.Count -ne 0) { Stop-TraceSummary 'trace.nesting' "$($stack.Count) 'B' events have no closing 'E' event." }

	$frontEndUs = [long] 0
	$backEndUs = [long] 0
	$linkUs = [long] 0
	$foreignCount = 0
	$foreignUs = [long] 0
	$keptCompilers = 0
	$translationUnits = [ordered]@{ Client = 0; Server = 0 }
	$frontEndByUnit = [Collections.Generic.Dictionary[string, AggregateRecord]]::new([StringComparer]::OrdinalIgnoreCase)
	foreach ($invocation in $invocations) {
		if (-not $invocation.Kept) {
			$foreignCount++
			$foreignUs += $invocation.Duration
			continue
		}
		if (-not $invocation.IsCompiler) {
			$linkUs += $invocation.Duration
			continue
		}
		$keptCompilers++
		$frontEndCount = $invocation.FrontEndPasses.Count
		$backEndCount = $invocation.BackEndPasses.Count
		if ($frontEndCount -eq 0 -or $backEndCount -eq 0) {
			Stop-TraceSummary 'trace.pass-names' "$($invocation.Name) has $frontEndCount FrontEndPass and $backEndCount BackEndPass entries; the SDK pass names did not hold."
		}
		if ($null -eq $invocation.Target) {
			Stop-TraceSummary 'trace.no-target' "$($invocation.Name) has no File Output under Build\BrokenEngineSandbox\ or Build\BrokenEngineSandboxServer\."
		}
		for ($index = 0; $index -lt $frontEndCount; $index++) {
			$frontEnd = $invocation.FrontEndPasses[$index]
			if ($null -eq $frontEnd.TranslationUnit) {
				Stop-TraceSummary 'trace.translation-unit' "$($invocation.Name) FrontEndPass $index has no path-named entry naming its translation unit."
			}
			$frontEndUs += $frontEnd.Duration
			$translationUnits[$invocation.Target]++
			Add-Aggregate $frontEndByUnit "$($invocation.Target)|$($frontEnd.TranslationUnit)" $frontEnd.Duration
		}
		foreach ($backEnd in $invocation.BackEndPasses) { $backEndUs += $backEnd.Duration }
	}
	if ($keptCompilers -eq 0) {
		Stop-TraceSummary 'trace.no-worktree-invocations' "No CL invocation has a Working Directory under '$worktreeRoot'."
	}

	$metrics = [ordered]@{
		buildElapsedMs = $buildElapsedMs
		frontEndMs = Get-Milliseconds $frontEndUs
		backEndMs = Get-Milliseconds $backEndUs
		linkMs = Get-Milliseconds $linkUs
	}
	$rankings = [ordered]@{
		translationUnitsByFrontEnd = @(Get-TranslationUnitRanking $frontEndByUnit)
		headers = @(Get-NamedRanking $headers)
		templates = @(Get-NamedRanking $templates)
		functions = @(Get-NamedRanking $functions)
	}

	$result.status = 'pass'
	$result.code = 'ok'
	$result.message = "Summarized $keptCompilers CL invocations under the worktree."
	$result.trace = $tracePath
	$result.buildEnvelope = [IO.Path]::GetFullPath($BuildEnvelope)
	$result.worktreeRoot = $worktreeRoot
	$result.metrics = $metrics
	$result.translationUnits = $translationUnits
	$result.foreignInvocations = [ordered]@{ count = $foreignCount; ms = (Get-Milliseconds $foreignUs) }
	$result.rankings = $rankings

	if ($null -ne $baselineSummary) {
		$metricComparison = [ordered]@{}
		foreach ($metricName in $metrics.Keys) {
			$before = $baselineSummary.metrics.$metricName
			$metricComparison[$metricName] = [ordered]@{ before = $before; after = $metrics[$metricName]; percent = (Get-Percent $before $metrics[$metricName]) }
		}
		$unitComparison = [ordered]@{}
		foreach ($target in $translationUnits.Keys) {
			$unitComparison[$target] = [ordered]@{ before = $baselineSummary.translationUnits.$target; after = $translationUnits[$target] }
		}
		# Each baseline-ranked item is looked up in the full after aggregation, so a fix's targeted item
		# is compared even when it leaves the top of the ranking.
		$rankingComparison = [ordered]@{}
		$namedTables = @{ headers = $headers; templates = $templates; functions = $functions }
		foreach ($rankingName in $rankings.Keys) {
			$rows = [Collections.Generic.List[object]]::new()
			foreach ($item in @($baselineSummary.rankings.$rankingName)) {
				$found = $null
				if ($rankingName -eq 'translationUnitsByFrontEnd') {
					$after = if ($frontEndByUnit.TryGetValue("$($item.target)|$($item.path)", [ref] $found)) { Get-Milliseconds $found.Duration } else { 0 }
					$rows.Add([ordered]@{ target = $item.target; path = $item.path; before = $item.ms; after = $after; percent = (Get-Percent $item.ms $after) })
				}
				elseif ($namedTables[$rankingName].TryGetValue($item.name, [ref] $found)) {
					$after = Get-Milliseconds $found.Duration
					$rows.Add([ordered]@{ name = $item.name; before = $item.ms; after = $after; percent = (Get-Percent $item.ms $after) })
				}
				elseif ($rankingName -eq 'headers') {
					$rows.Add([ordered]@{ name = $item.name; before = $item.ms; after = 0; percent = (Get-Percent $item.ms 0) })
				}
				else {
					$rows.Add([ordered]@{ name = $item.name; before = $item.ms; after = $null; percent = $null; belowFloor = $true })
				}
			}
			$rankingComparison[$rankingName] = $rows.ToArray()
		}
		$result.comparison = [ordered]@{
			baseline = [IO.Path]::GetFullPath($Baseline)
			metrics = $metricComparison
			translationUnits = $unitComparison
			rankings = $rankingComparison
		}
	}

	Complete-TraceSummary 0 $result
}
catch {
	$result.code = if ($null -ne $script:BlockedCode) { $script:BlockedCode } else { 'internal.error' }
	$result.status = if ($null -ne $script:BlockedCode) { 'blocked' } else { 'error' }
	$result.message = $_.Exception.Message
	Write-TraceDiagnostic "$($result.status) $($result.code) - $($result.message)"
	Complete-TraceSummary $(if ($null -ne $script:BlockedCode) { 2 } else { 1 }) $result
}
