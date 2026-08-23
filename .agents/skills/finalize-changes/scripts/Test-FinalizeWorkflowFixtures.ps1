# Scratch-repository coverage for candidate creation, approval preparation, the
# landing lock lease, one session landing, and idempotent post-advance recovery.
# Uses the supplied WorktreeCli only inside disposable Output and never touches a
# real primary checkout, queue, or canonical executable.
[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)]
	[string] $WorktreeCliExecutable
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$sharedScripts = Join-Path $PSScriptRoot '..\..\..\scripts'
if (-not (Test-Path -LiteralPath (Join-Path $sharedScripts 'WorktreeCliSessionExclusion.psm1'))) {
	$sharedScripts = Join-Path $PSScriptRoot '..\..\..\..\.agents\scripts'
}
Import-Module (Join-Path $sharedScripts 'WorktreeCliSessionExclusion.psm1') -Force -DisableNameChecking
Import-Module (Join-Path $sharedScripts 'FinalizeWorkflowCommon.psm1') -Force -DisableNameChecking

$script:Failures = [Collections.Generic.List[string]]::new()
$landingScript = Join-Path $PSScriptRoot 'Invoke-FinalizeLanding.ps1'
$approvalPreparationScript = Join-Path $PSScriptRoot 'Invoke-FinalizeApprovalPreparation.ps1'
$candidateScript = Join-Path $PSScriptRoot 'Invoke-FinalizeCandidateCommit.ps1'
$lockClaimScript = Join-Path $PSScriptRoot 'Invoke-FinalizeLockClaim.ps1'
$approvalReviewScript = Join-Path $PSScriptRoot 'Show-FinalizeApprovalReview.ps1'
$moduleSource = $sharedScripts
$WorktreeCliExecutable = (Get-Item -LiteralPath $WorktreeCliExecutable -Force -ErrorAction Stop).FullName

function Assert-True([bool] $Condition, [string] $Name) {
	if ($Condition) { Write-Host "pass $Name" } else { $script:Failures.Add($Name); Write-Host "FAIL $Name" }
}

function Invoke-JsonScript([string] $Script, [string[]] $Arguments) {
	$stdout = @(& "$PSHOME\pwsh.exe" -NoProfile -File $Script @Arguments 2>$null)
	$exitCode = $LASTEXITCODE
	$text = ($stdout -join "`n").Trim()
	$json = $null
	try { if (-not [string]::IsNullOrWhiteSpace($text)) { $json = $text | ConvertFrom-Json -Depth 100 -ErrorAction Stop } } catch { }
	return [pscustomobject]@{ ExitCode = $exitCode; Json = $json; Text = $text }
}

function Invoke-JsonScriptWithSplat([string] $Script, [Collections.IDictionary] $Parameters, [string] $ScratchRoot) {
	$invocationRoot = Join-Path $ScratchRoot 'splat-invocation'
	New-Item -ItemType Directory -Force $invocationRoot | Out-Null
	$payloadPath = Join-Path $invocationRoot ([guid]::NewGuid().ToString('N') + '.json')
	$wrapperPath = Join-Path $invocationRoot 'Invoke-WithSplat.ps1'
	$wrapper = @'
param([string] $TargetScript, [string] $PayloadPath)
$payload = Get-Content -LiteralPath $PayloadPath -Raw | ConvertFrom-Json -Depth 32 -ErrorAction Stop
$parameters = @{}
foreach ($property in $payload.PSObject.Properties) { $parameters[$property.Name] = $property.Value }
& $TargetScript @parameters
exit $LASTEXITCODE
'@
	[IO.File]::WriteAllText($wrapperPath, $wrapper, [Text.UTF8Encoding]::new($false))
	[IO.File]::WriteAllText($payloadPath, (ConvertTo-Json -InputObject $Parameters -Depth 8 -Compress), [Text.UTF8Encoding]::new($false))
	$stderrPath = Join-Path $invocationRoot ([guid]::NewGuid().ToString('N') + '.stderr')
	$stdout = @(& "$PSHOME\pwsh.exe" -NoProfile -File $wrapperPath -TargetScript $Script -PayloadPath $payloadPath 2>$stderrPath)
	$exitCode = $LASTEXITCODE
	$stderr = ''
	if (Test-Path -LiteralPath $stderrPath) {
		$stderr = [IO.File]::ReadAllText($stderrPath)
		Remove-Item -LiteralPath $stderrPath -Force
	}
	$text = ($stdout -join "`n").Trim()
	$json = $null
	try { if (-not [string]::IsNullOrWhiteSpace($text)) { $json = $text | ConvertFrom-Json -Depth 100 -ErrorAction Stop } } catch { }
	return [pscustomobject]@{ ExitCode = $exitCode; Json = $json; Text = $text; Stderr = $stderr }
}

# A live foreign landing lease makes production landing wait for its documented 300-second bound.
# Start the child separately so these fixtures can observe that it did not adopt the lease, then
# terminate and reap it before releasing the scratch lease; production wait values stay untouched.
function Start-JsonScriptWithSplat([string] $Script, [Collections.IDictionary] $Parameters, [string] $ScratchRoot) {
	$invocationRoot = Join-Path $ScratchRoot 'splat-invocation'
	New-Item -ItemType Directory -Force $invocationRoot | Out-Null
	$suffix = [guid]::NewGuid().ToString('N')
	$payloadPath = Join-Path $invocationRoot ($suffix + '.json')
	$wrapperPath = Join-Path $invocationRoot ($suffix + '.ps1')
	$wrapper = @'
param([string] $TargetScript, [string] $PayloadPath)
$payload = Get-Content -LiteralPath $PayloadPath -Raw | ConvertFrom-Json -Depth 32 -ErrorAction Stop
$parameters = @{}
foreach ($property in $payload.PSObject.Properties) { $parameters[$property.Name] = $property.Value }
& $TargetScript @parameters
exit $LASTEXITCODE
'@
	[IO.File]::WriteAllText($wrapperPath, $wrapper, [Text.UTF8Encoding]::new($false))
	[IO.File]::WriteAllText($payloadPath, (ConvertTo-Json -InputObject $Parameters -Depth 8 -Compress), [Text.UTF8Encoding]::new($false))
	$start = [Diagnostics.ProcessStartInfo]::new()
	$start.FileName = "$PSHOME\pwsh.exe"
	$start.WorkingDirectory = $ScratchRoot
	$start.UseShellExecute = $false
	$start.CreateNoWindow = $true
	$start.RedirectStandardOutput = $true
	$start.RedirectStandardError = $true
	[void] $start.ArgumentList.Add('-NoProfile')
	[void] $start.ArgumentList.Add('-File')
	[void] $start.ArgumentList.Add($wrapperPath)
	[void] $start.ArgumentList.Add('-TargetScript')
	[void] $start.ArgumentList.Add($Script)
	[void] $start.ArgumentList.Add('-PayloadPath')
	[void] $start.ArgumentList.Add($payloadPath)
	$process = [Diagnostics.Process]::new()
	$process.StartInfo = $start
	if (-not $process.Start()) { throw "Could not start '$PSHOME\pwsh.exe'." }
	return [pscustomobject]@{
		Process = $process
		StdoutTask = $process.StandardOutput.ReadToEndAsync()
		StderrTask = $process.StandardError.ReadToEndAsync()
	}
}

function Stop-JsonScript($Started) {
	try {
		if (-not $Started.Process.WaitForExit(0)) {
			try { $Started.Process.Kill($true) } catch {
				if (-not $Started.Process.HasExited) { try { $Started.Process.Kill() } catch { } }
			}
			if (-not $Started.Process.WaitForExit(5000)) { throw "Timed out reaping JSON script process $($Started.Process.Id)." }
		}
		$Started.StdoutTask.GetAwaiter().GetResult() | Out-Null
		$Started.StderrTask.GetAwaiter().GetResult() | Out-Null
	}
	finally { $Started.Process.Dispose() }
}

function Invoke-WorktreeCli([string[]] $Arguments, [int] $ExpectedExitCode = 0) {
	$stdout = @(& $WorktreeCliExecutable @Arguments 2>&1)
	if ($LASTEXITCODE -ne $ExpectedExitCode) {
		throw "WorktreeCli $($Arguments -join ' ') exited $LASTEXITCODE, expected $ExpectedExitCode`: $($stdout -join '; ')"
	}
	return $stdout
}

function Assert-Outcome($Run, [string] $Case, [int] $ExpectedExit, [string] $ExpectedStatus, [string] $ExpectedCode) {
	Assert-True ($null -ne $Run.Json) "$Case emitted JSON"
	if ($null -eq $Run.Json) {
		Write-Host "  stdout: $($Run.Text)"
		if (($Run.PSObject.Properties.Name -ccontains 'Stderr') -and -not [string]::IsNullOrWhiteSpace($Run.Stderr)) { Write-Host "  stderr: $($Run.Stderr.Trim())" }
		return
	}
	Assert-True ($Run.ExitCode -eq $ExpectedExit) "$Case exit=$ExpectedExit (was $($Run.ExitCode))"
	Assert-True ($Run.Json.status -ceq $ExpectedStatus) "$Case status=$ExpectedStatus (was $($Run.Json.status))"
	Assert-True ($Run.Json.code -ceq $ExpectedCode) "$Case code=$ExpectedCode (was $($Run.Json.code))"
	if ($Run.ExitCode -ne $ExpectedExit -or $Run.Json.code -cne $ExpectedCode) { Write-Host "  message: $($Run.Json.message)" }
}

function Assert-ExactProperties($Value, [string[]] $Expected, [string] $Case) {
	$actual = @($Value.PSObject.Properties.Name | Sort-Object)
	$wanted = @($Expected | Sort-Object)
	Assert-True (($actual -join '|') -ceq ($wanted -join '|')) "$Case exact properties"
}

function Set-ExpiredLandingLease([string] $LocalAppData, [string] $Owner) {
	$lease = @(Get-ChildItem -LiteralPath $LocalAppData -Recurse -Filter '*.lock' -File -Force | Where-Object {
		try {
			$metadata = Get-Content -LiteralPath $_.FullName -Raw | ConvertFrom-Json -Depth 16 -ErrorAction Stop
			return $metadata.domain -ceq 'landing' -and $metadata.owner -ceq $Owner
		}
		catch { return $false }
	})
	if ($lease.Count -ne 1) { throw "Could not locate one scratch landing lease for '$Owner'." }
	$metadata = Get-Content -LiteralPath $lease[0].FullName -Raw | ConvertFrom-Json -Depth 16 -ErrorAction Stop
	[IO.File]::SetAttributes($lease[0].FullName, [IO.FileAttributes]::Normal)
	$now = [DateTime]::UtcNow
	$heartbeat = $now.AddSeconds(-120).ToString('yyyy-MM-ddTHH:mm:ss.fffZ', [Globalization.CultureInfo]::InvariantCulture)
	$expires = $now.AddSeconds(-60).ToString('yyyy-MM-ddTHH:mm:ss.fffZ', [Globalization.CultureInfo]::InvariantCulture)
	$metadata.claimedAt = $heartbeat
	$metadata.heartbeatAt = $heartbeat
	$metadata.expiresAt = $expires
	[IO.File]::WriteAllText($lease[0].FullName, ($metadata | ConvertTo-Json -Depth 16 -Compress), [Text.UTF8Encoding]::new($false))
}

function Set-NearExpiryLandingLease([string] $LocalAppData, [string] $Owner, [int] $RemainingMilliseconds = 1500) {
	$lease = @(Get-ChildItem -LiteralPath $LocalAppData -Recurse -Filter '*.lock' -File -Force | Where-Object {
		try {
			$metadata = Get-Content -LiteralPath $_.FullName -Raw | ConvertFrom-Json -Depth 16 -ErrorAction Stop
			return $metadata.domain -ceq 'landing' -and $metadata.owner -ceq $Owner
		}
		catch { return $false }
	})
	if ($lease.Count -ne 1) { throw "Could not locate one scratch landing lease for '$Owner'." }
	$metadata = Get-Content -LiteralPath $lease[0].FullName -Raw | ConvertFrom-Json -Depth 16 -ErrorAction Stop
	$durationSeconds = [int]$metadata.leaseDurationSeconds
	$now = [DateTime]::UtcNow
	$heartbeat = $now.AddMilliseconds(-($durationSeconds * 1000 - $RemainingMilliseconds))
	$expires = $heartbeat.AddSeconds($durationSeconds)
	[IO.File]::SetAttributes($lease[0].FullName, [IO.FileAttributes]::Normal)
	$metadata.claimedAt = $heartbeat.ToString('yyyy-MM-ddTHH:mm:ss.fffZ', [Globalization.CultureInfo]::InvariantCulture)
	$metadata.heartbeatAt = $metadata.claimedAt
	$metadata.expiresAt = $expires.ToString('yyyy-MM-ddTHH:mm:ss.fffZ', [Globalization.CultureInfo]::InvariantCulture)
	[IO.File]::WriteAllText($lease[0].FullName, ($metadata | ConvertTo-Json -Depth 16 -Compress), [Text.UTF8Encoding]::new($false))
	return [pscustomobject]@{ Path = $lease[0].FullName; ExpiresAt = [DateTimeOffset]::Parse($metadata.expiresAt, [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::AssumeUniversal) }
}

function Set-UnverifiableLandingLease([string] $LocalAppData, [string] $Owner) {
	$lease = @(Get-ChildItem -LiteralPath $LocalAppData -Recurse -Filter '*.lock' -File -Force | Where-Object {
		try {
			$metadata = Get-Content -LiteralPath $_.FullName -Raw | ConvertFrom-Json -Depth 16 -ErrorAction Stop
			return $metadata.domain -ceq 'landing' -and $metadata.owner -ceq $Owner
		}
		catch { return $false }
	})
	if ($lease.Count -ne 1) { throw "Could not locate one scratch landing lease for '$Owner'." }
	$metadata = Get-Content -LiteralPath $lease[0].FullName -Raw | ConvertFrom-Json -Depth 16 -ErrorAction Stop
	[IO.File]::SetAttributes($lease[0].FullName, [IO.FileAttributes]::Normal)
	$metadata.schemaVersion = 0
	[IO.File]::WriteAllText($lease[0].FullName, ($metadata | ConvertTo-Json -Depth 16 -Compress), [Text.UTF8Encoding]::new($false))
}

$scratchParent = Join-Path ([IO.Path]::GetTempPath()) 'BrokenEngineFinalizeWorkflowFixtures'
$scratchLeaf = [guid]::NewGuid().ToString('N')
$scratchBase = Assert-SafeScratchRoot $scratchParent (Join-Path $scratchParent $scratchLeaf) $scratchLeaf
$primary = Join-Path $scratchBase 'primary'
$session = Join-Path $scratchBase 'session'
$localAppData = Join-Path $scratchBase 'local-app-data'
$previousEnvironment = @{}
$fixtureEnvironment = $null
$fixtureExitCode = 0
$script:PrimaryAdvanceOwner = $null
$script:PrimaryHistoryContract = $null
$historyProducerText = Get-Content -Raw (Join-Path $PSScriptRoot '..\..\code-quality-metrics\scripts\Invoke-CodeQualityMetricsHistory.ps1')
$approvalProducerText = Get-Content -Raw (Join-Path $PSScriptRoot 'Invoke-FinalizeApprovalPreparation.ps1')
$landingProducerText = Get-Content -Raw (Join-Path $PSScriptRoot 'Invoke-FinalizeLanding.ps1')
Assert-True ($historyProducerText -match '\[string\]\$RepositoryRoot' -and $historyProducerText -match '\[string\]\$BaseCommit' -and $historyProducerText -match '\[string\]\$TipCommit' -and $historyProducerText -match '\[string\]\$DateUtc' -and $historyProducerText -match '\[string\]\$OutputDirectory') 'history producer exposes the exact canonical parameter interface'
Assert-True ($approvalProducerText -notmatch 'parameterNames|Get-Command -Name \$historyScript|Get-HistoryProperty' -and $landingProducerText -notmatch 'parameterNames|Get-Command -Name \$historyScript|Get-HistoryProperty') 'finalizer history calls contain no alternate parameter or receipt reflection'

try {
New-Item -ItemType Directory -Force $primary | Out-Null
New-Item -ItemType Directory -Force $localAppData | Out-Null
$fixtureEnvironment = [ordered]@{ LOCALAPPDATA = $localAppData; BROKEN_ENGINE_FINALIZE_WORKFLOW_FIXTURE = '1'; BROKEN_ENGINE_FINALIZE_APPROVAL_PREPARATION_FIXTURE = '1'; BROKEN_ENGINE_FINALIZE_HISTORY_RECEIPT = $null }
foreach ($entry in $fixtureEnvironment.GetEnumerator()) { $previousEnvironment[$entry.Key] = [Environment]::GetEnvironmentVariable($entry.Key); [Environment]::SetEnvironmentVariable($entry.Key, $entry.Value) }
Invoke-ScratchGit $primary @('init', '-b', 'main') | Out-Null
Invoke-ScratchGit $primary @('config', 'core.autocrlf', 'false') | Out-Null
# The landing script runs its internal rebase through plain git.exe, so the committer identity has to
# live in the scratch repository instead of the -c arguments Invoke-ScratchGit injects.
Invoke-ScratchGit $primary @('config', 'user.name', 'fixture') | Out-Null
Invoke-ScratchGit $primary @('config', 'user.email', 'fixture@example.com') | Out-Null
New-Item -ItemType Directory -Force (Join-Path $primary '.agents\scripts') | Out-Null
foreach ($module in @('AgentScriptCommon.psm1', 'WorktreeCliSessionExclusion.psm1', 'AgentWorktreeSession.psm1')) {
	Copy-Item -LiteralPath (Join-Path $moduleSource $module) -Destination (Join-Path $primary ".agents\scripts\$module") -Force
}
# The production history producer is deliberately not run for every scratch case: this exact public
# Contract/Generate shape is a deterministic fixture producer, while the real script is exercised by
# its own metrics fixtures. It keeps the finalizer cases focused on receipt validation, overlay, CAS,
# recovery, and lock atomicity without paying for a full analyzer capture each time.
$historyScriptRoot = Join-Path $primary '.agents\skills\code-quality-metrics\scripts'
$historyDataRoot = Join-Path $primary '.agents\skills\code-quality-metrics\references\history'
New-Item -ItemType Directory -Force $historyScriptRoot, $historyDataRoot | Out-Null
$historyFixtureScript = @'
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Contract','Generate')][string]$Mode,[Parameter(Mandatory)][string]$RepositoryRoot,[string]$BaseCommit,[string]$TipCommit,[string]$DateUtc,[string]$OutputDirectory)
$ErrorActionPreference='Stop'; Set-StrictMode -Version Latest
$generator='0000000000000000000000000000000000000000000000000000000000000000'; $zero40='0000000000000000000000000000000000000000'; $zero64='0000000000000000000000000000000000000000000000000000000000000000'; $prefix=[ordered]@{bytes=133323;lines=648;sha256='5a39debf4be41abebd8496b9f25ee4023d109813788e95b30da8f74474fe75ed'}
$jsonRelative='.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl'; $svgRelative='.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.svg'
$rawChanges=@(& git -C $RepositoryRoot diff --name-status --find-renames=50% $BaseCommit $TipCommit 2>$null); $changeRows=[Collections.Generic.List[object]]::new(); foreach($line in $rawChanges){if([string]::IsNullOrWhiteSpace([string]$line)){continue}; $parts=([string]$line) -split "`t"; $status=([string]$parts[0]).Substring(0,1); $path=([string]$parts[$parts.Count-1]).Replace('\','/'); $oldPath=$null; if($status -in @('R','C')){$oldPath=([string]$parts[1]).Replace('\','/')}; $metric=($path -match '\.(c|cc|cpp|cxx|h|hh|hpp|hxx)$') -or ($oldPath -and $oldPath -match '\.(c|cc|cpp|cxx|h|hh|hpp|hxx)$'); $changeRows.Add([ordered]@{status=$status;path=$path;oldPath=$oldPath;metricSupported=[bool]$metric})}; $metricCount=@($changeRows|Where-Object{$_.metricSupported}).Count; $cpp=$metricCount -gt 0
$captureMode=if($cpp){'cpp-change'}else{'carry-forward'}; $reason=if($cpp){'metric-supported-cpp-change'}else{'no-metric-supported-cpp-change'}; $capture=$null; $snapshot=$null
if($cpp){$manifest=[ordered]@{gitlinkCommit=$zero40;resolvedHead=$zero40;clean=$true;entries=@([ordered]@{relativePath='fixture.txt';gitMode='100644';type='file';length=0;rawSha256=$zero64})}; $manifestDigest=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.UTF8Encoding]::new($false).GetBytes(($manifest|ConvertTo-Json -Compress -Depth 32)))).ToLowerInvariant(); $capture=[ordered]@{digest=$zero64;bootstrapIdentityDigest=$zero64;scbContentDigest=$zero64;manifest=$manifest;manifestDigest=$manifestDigest}; $snapshot=[ordered]@{target='Engine/Source';scope='Recursive';coverageRequired=$true}}
if($env:BROKEN_ENGINE_FINALIZE_HISTORY_MODE -eq 'cpp-change' -and -not $cpp){$captureMode='cpp-change';$reason='metric-supported-cpp-change';$manifest=[ordered]@{gitlinkCommit=$zero40;resolvedHead=$zero40;clean=$true;entries=@([ordered]@{relativePath='fixture.txt';gitMode='100644';type='file';length=0;rawSha256=$zero64})};$manifestDigest=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.UTF8Encoding]::new($false).GetBytes(($manifest|ConvertTo-Json -Compress -Depth 32)))).ToLowerInvariant();$capture=[ordered]@{digest=$zero64;bootstrapIdentityDigest=$zero64;scbContentDigest=$zero64;manifest=$manifest;manifestDigest=$manifestDigest};$snapshot=[ordered]@{target='Engine/Source';scope='Recursive';coverageRequired=$true}}
$patch=[ordered]@{baseCommit=$BaseCommit;tipCommit=$TipCommit;changes=@($changeRows);metricSupportedChanges=$metricCount;cppChanged=[bool]$cpp}; $decision=[ordered]@{captureMode=$captureMode;reason=$reason;forceSnapshot=[bool]($captureMode -ne 'carry-forward')}; $source=[ordered]@{baseCommit=$BaseCommit;tipCommit=$TipCommit}; $seriesContract=[ordered]@{rows=1;liveRows=1;lastIndex=0;lastDate='2026-08-10';historyBytesSha256=$zero64}
if($Mode -eq 'Contract') {
  $contract=[ordered]@{schemaVersion='broken-engine-code-quality-history-contract/v1';mode='Contract';source=$source;prefix=$prefix;series=$seriesContract;patch=$patch;decision=$decision;generator=[ordered]@{relativePath='.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1';sha256=$generator};capture=$capture;snapshot=$snapshot}
  if($env:BROKEN_ENGINE_FINALIZE_HISTORY_RECEIPT -eq 'contract-nested-unknown'){$contract.source=[ordered]@{baseCommit=$BaseCommit;tipCommit=$TipCommit;unexpected='malformed'}}
  [Console]::Out.WriteLine(($contract|ConvertTo-Json -Compress -Depth 32)); exit 0
}
if(-not $OutputDirectory){throw 'Generate requires OutputDirectory.'}; if(Test-Path -LiteralPath $OutputDirectory){throw 'OutputDirectory must not already exist.'}; New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
if($env:BROKEN_ENGINE_FINALIZE_HISTORY_GENERATE_MARKER){[IO.File]::AppendAllText($env:BROKEN_ENGINE_FINALIZE_HISTORY_GENERATE_MARKER,"Generate`n")}
$date=if($DateUtc){$DateUtc}else{[DateTime]::UtcNow.ToString('yyyy-MM-dd')}; $historyPath=Join-Path $RepositoryRoot ($jsonRelative -replace '/','\'); $prefixText=if(Test-Path -LiteralPath $historyPath){[IO.File]::ReadAllText($historyPath,[Text.UTF8Encoding]::new($false,$true))}else{''}; $prefixRows=@($prefixText -split [char]10 | Where-Object { $_ }); $row=[ordered]@{index=[Math]::Max(0,$prefixRows.Count-1);date=$date;captureMode=$captureMode;verbosity=0.1;structuralErosion=0.1;supported=1;parsed=1}; $rowText=(($row|ConvertTo-Json -Compress)+[char]10); $jsonText=$prefixText+$rowText; $jsonBytes=[Text.UTF8Encoding]::new($false).GetBytes($jsonText); $series=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($jsonBytes)).ToLowerInvariant(); $svgDescription="seriesDigest=$series generatorDigest=$generator"; if($capture){$svgDescription += " captureDigest=$($capture.digest) identityDigest=$($capture.bootstrapIdentityDigest) scbDigest=$($capture.scbContentDigest)"}; $svgText="<svg xmlns='http://www.w3.org/2000/svg'><desc>$svgDescription</desc></svg>"+[char]10; $jsonPath=Join-Path $OutputDirectory 'CodeQualityMetricsHistory.jsonl'; $svgPath=Join-Path $OutputDirectory 'CodeQualityMetricsHistory.svg'; [IO.File]::WriteAllBytes($jsonPath,$jsonBytes); [IO.File]::WriteAllText($svgPath,$svgText,[Text.UTF8Encoding]::new($false)); $relJson=([IO.Path]::GetRelativePath($RepositoryRoot,$jsonPath).Replace([char]92,[char]47)); $relSvg=([IO.Path]::GetRelativePath($RepositoryRoot,$svgPath).Replace([char]92,[char]47)); $jsonHash=([Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($jsonBytes)).ToLowerInvariant()); $svgHash=(Get-FileHash $svgPath -Algorithm SHA256).Hash.ToLowerInvariant(); $coverage=if($capture){[ordered]@{corpusCounts=[ordered]@{supported=1;parsed=1;omitted=0};targetCounts=[ordered]@{supported=1;parsed=1;omitted=0}}}else{$null}; $update=[ordered]@{schemaVersion='broken-engine-code-quality-history-update/v1';mode='Generate';date=$date;captureMode=$captureMode;source=$source;prefix=$prefix;patch=$patch;generator=[ordered]@{relativePath='.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1';sha256=$generator};capture=$capture;series=[ordered]@{index=$row.index;digest=$series;historyBytesSha256=$zero64;row=$row;coverage=$coverage};outputs=[ordered]@{jsonl=[ordered]@{path=$relJson;bytes=$jsonBytes.Length;sha256=$jsonHash};svg=[ordered]@{path=$relSvg;bytes=([IO.File]::ReadAllBytes($svgPath)).Length;sha256=$svgHash}}}; if($env:BROKEN_ENGINE_FINALIZE_HISTORY_RECEIPT -eq 'update-row-unknown'){$update.series.row=[ordered]@{index=$row.index;date=$date;captureMode=$captureMode;verbosity=0.1;structuralErosion=0.1;supported=1;parsed=1;unexpected='malformed'}}; [Console]::Out.WriteLine(($update|ConvertTo-Json -Compress -Depth 32)); exit 0
'@
[IO.File]::WriteAllText((Join-Path $historyScriptRoot 'Invoke-CodeQualityMetricsHistory.ps1'), $historyFixtureScript, [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText((Join-Path $historyDataRoot 'CodeQualityMetricsHistory.jsonl'), "{`"schema`":`"code-quality-metrics-history/v1`"}`n{`"index`":0,`"date`":`"2026-08-10`",`"captureMode`":`"catch-up`",`"verbosity`":0.1,`"structuralErosion`":0.1,`"supported`":1,`"parsed`":1}`n", [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText((Join-Path $historyDataRoot 'CodeQualityMetricsHistory.svg'), "<svg xmlns='http://www.w3.org/2000/svg'><desc>fixture baseline</desc></svg>`n", [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText((Join-Path $primary '.gitignore'), "Temp/`nTools/WorktreeCli/Platforms/VisualStudio2026/Output/`n", [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText((Join-Path $primary 'base.txt'), 'base', [Text.UTF8Encoding]::new($false))
$gitlinkSource = Join-Path $scratchBase 'gitlink-source'
New-Item -ItemType Directory -Force $gitlinkSource | Out-Null
Invoke-ScratchGit $gitlinkSource @('init', '-b', 'main') | Out-Null
[IO.File]::WriteAllText((Join-Path $gitlinkSource 'submodule.txt'), 'gitlink fixture', [Text.UTF8Encoding]::new($false))
Invoke-ScratchGit $gitlinkSource @('add', 'submodule.txt') | Out-Null
Invoke-ScratchGit $gitlinkSource @('commit', '-m', 'gitlink fixture') | Out-Null
Invoke-ScratchGit $primary @('-c', 'protocol.file.allow=always', 'submodule', 'add', $gitlinkSource, 'fixture-gitlink') | Out-Null
Invoke-ScratchGit $primary @('add', '-A') | Out-Null
Invoke-ScratchGit $primary @('commit', '-m', 'fixture base') | Out-Null
$baseline = (@(Invoke-ScratchGit $primary @('rev-parse', 'HEAD')))[0].Trim()

$primaryOutput = Join-Path $primary 'Tools\WorktreeCli\Platforms\VisualStudio2026\Output'
New-Item -ItemType Directory -Force $primaryOutput | Out-Null
Copy-Item -LiteralPath $WorktreeCliExecutable -Destination (Join-Path $primaryOutput 'WorktreeCli.exe') -Force
$uuid = [guid]::NewGuid().ToString()
$sessionBranch = "codex/$uuid"
Invoke-ScratchGit $primary @('worktree', 'add', '-b', $sessionBranch, $session, $baseline) | Out-Null
New-Item -ItemType Directory -Force (Join-Path $session 'Temp') | Out-Null
New-Item -ItemType Directory -Force (Join-Path $primary 'Temp') | Out-Null
$sessionOutputParent = Join-Path $session 'Tools\WorktreeCli\Platforms\VisualStudio2026'
New-Item -ItemType Directory -Force $sessionOutputParent | Out-Null
New-Item -ItemType Junction -Path (Join-Path $sessionOutputParent 'Output') -Target $primaryOutput | Out-Null
[IO.File]::WriteAllText((Join-Path $session 'change.txt'), 'session change', [Text.UTF8Encoding]::new($false))
Invoke-ScratchGit $session @('add', 'change.txt') | Out-Null
Invoke-ScratchGit $session @('commit', '-m', 'fixture change') | Out-Null
# Align primary onto the session tip so every landing scenario below starts from an
# equal session/primary tree.
$sessionTip = (@(Invoke-ScratchGit $session @('rev-parse', 'HEAD')))[0].Trim()
Invoke-ScratchGit $primary @('merge', '--ff-only', $sessionTip) | Out-Null
$baseline = (@(Invoke-ScratchGit $primary @('rev-parse', 'HEAD')))[0].Trim()
$candidateMessage = Join-Path $scratchBase 'candidate-message.txt'
[IO.File]::WriteAllText($candidateMessage, "fixture candidate`n", [Text.UTF8Encoding]::new($false))
$literalBracketPath = 'Engine/Data/Textures/Water/[BC4]FoamNoiseAbstract.png'
$historyJsonFixturePath = '.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl'
$historySvgFixturePath = '.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.svg'

function New-ReservedHistoryCommit([string] $Root, [string] $Parent, [string] $Path, [bool] $Delete, [string] $Content) {
	$fullPath = Join-Path $Root ($Path.Replace('/', '\'))
	if ($Delete) {
		Remove-Item -LiteralPath $fullPath -Force -ErrorAction SilentlyContinue
	}
	else {
		$parentDirectory = Split-Path -Parent $fullPath
		New-Item -ItemType Directory -Force $parentDirectory | Out-Null
		[IO.File]::WriteAllText($fullPath, $Content, [Text.UTF8Encoding]::new($false))
	}
	$temporaryIndex = Join-Path $scratchBase ('reserved-history-' + [guid]::NewGuid().ToString('N') + '.index')
	$oldIndex = $env:GIT_INDEX_FILE
	try {
		$env:GIT_INDEX_FILE = $temporaryIndex
		Invoke-ScratchGit $Root @('read-tree', $Parent) | Out-Null
		Invoke-ScratchGit $Root @('add', '-A', '--', $Path) | Out-Null
		$tree = (@(Invoke-ScratchGit $Root @('write-tree')))[0].Trim()
		$commit = (@(Invoke-ScratchGit $Root @('commit-tree', $tree, '-p', $Parent, '-F', $candidateMessage)))[0].Trim()
		return [pscustomobject]@{ Commit = $commit; Tree = $tree }
	}
	finally {
		if ($null -eq $oldIndex) { Remove-Item Env:GIT_INDEX_FILE -ErrorAction SilentlyContinue } else { $env:GIT_INDEX_FILE = $oldIndex }
		Remove-Item -LiteralPath $temporaryIndex -Force -ErrorAction SilentlyContinue
	}
}

function Get-FixtureJsonDigest($Value) {
	$bytes = [Text.UTF8Encoding]::new($false).GetBytes(($Value | ConvertTo-Json -Compress -Depth 64))
	return [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant()
}

# Session candidates do not produce a history Contract; approval preparation obtains it from the
# read-only producer after candidate creation. Reconstruct that same compact identity for each
# carry-forward or recovery invocation.
function Get-FixtureHistoryContract([string] $BaseCommit, [string] $TipCommit) {
	$contractRun = Invoke-JsonScript (Join-Path $historyScriptRoot 'Invoke-CodeQualityMetricsHistory.ps1') @('-Mode', 'Contract', '-RepositoryRoot', $primary, '-BaseCommit', $BaseCommit, '-TipCommit', $TipCommit)
	if ($contractRun.ExitCode -ne 0 -or $null -eq $contractRun.Json) {
		throw "Fixture history Contract failed for '$BaseCommit' to '$TipCommit'."
	}
	$receipt = $contractRun.Json
	$patchIdentity = [ordered]@{ changes = $receipt.patch.changes; metricSupportedChanges = $receipt.patch.metricSupportedChanges; cppChanged = $receipt.patch.cppChanged }
	return [pscustomobject]@{
		digest = Get-FixtureJsonDigest $receipt
		generatorDigest = [string]$receipt.generator.sha256
		captureDigest = $(if ($null -ne $receipt.capture) { [string]$receipt.capture.digest } else { $null })
		runtimeDigest = $(if ($null -ne $receipt.capture) { [string]$receipt.capture.bootstrapIdentityDigest } else { $null })
		patchDigest = Get-FixtureJsonDigest $patchIdentity
		mode = [string]$receipt.decision.captureMode
	}
}

function Add-FixtureHistoryParameters([Collections.IDictionary] $Parameters, $Contract) {
	$Parameters.HistoryContractDigest = $Contract.digest
	$Parameters.HistoryContractGeneratorDigest = $Contract.generatorDigest
	$Parameters.HistoryContractCaptureDigest = $Contract.captureDigest
	$Parameters.HistoryContractRuntimeDigest = $Contract.runtimeDigest
	$Parameters.HistoryContractPatchDigest = $Contract.patchDigest
	$Parameters.HistoryContractMode = $Contract.mode
	return $Parameters
}

# Candidate construction is deliberately before verification. This isolated coverage
# exercises the Git boundary and its guarded rollbacks.
[IO.File]::WriteAllText((Join-Path $session 'candidate-session.txt'), 'session candidate', [Text.UTF8Encoding]::new($false))
$run = Invoke-JsonScript $candidateScript @('-Route','session-landing','-CurrentWorktree',$session,'-PrimaryWorktree',$primary,'-CurrentBranch',$sessionBranch,'-PrimaryBranch','main','-Baseline',$baseline,'-ExpectedCurrentTip',$sessionTip,'-ExpectedPrimaryTip',$baseline,'-OwnedPaths','*.txt','-CommitMessageFile',$candidateMessage)
Assert-Outcome $run 'candidate-rejects-pathspec-owned-path' 1 'error' 'input.path-invalid'
$traversalSessionRefBefore = (@(Invoke-ScratchGit $session @('rev-parse',"refs/heads/$sessionBranch")))[0].Trim()
$traversalPrimaryRefBefore = (@(Invoke-ScratchGit $primary @('rev-parse','refs/heads/main')))[0].Trim()
$traversalSessionIndexBefore = (@(Invoke-ScratchGit $session @('ls-files','-s')) -join "`n")
$traversalPrimaryIndexBefore = (@(Invoke-ScratchGit $primary @('ls-files','-s')) -join "`n")
$traversalSessionStatusBefore = (@(Invoke-ScratchGit $session @('status','--porcelain=v1','-z','--untracked-files=all')) -join "`n")
$traversalPrimaryStatusBefore = (@(Invoke-ScratchGit $primary @('status','--porcelain=v1','-z','--untracked-files=all')) -join "`n")
$traversalCandidateTextBefore = [IO.File]::ReadAllText((Join-Path $session 'candidate-session.txt'), [Text.UTF8Encoding]::new($false,$true))
$traversalPrimaryBaseTextBefore = [IO.File]::ReadAllText((Join-Path $primary 'base.txt'), [Text.UTF8Encoding]::new($false,$true))
$assertInvalidPathState = {
	param([string] $Case)
	Assert-True ($traversalSessionRefBefore -ceq ((@(Invoke-ScratchGit $session @('rev-parse',"refs/heads/$sessionBranch")))[0].Trim()) -and $traversalPrimaryRefBefore -ceq ((@(Invoke-ScratchGit $primary @('rev-parse','refs/heads/main')))[0].Trim())) "$Case preserves both refs"
	Assert-True ($traversalSessionIndexBefore -ceq (@(Invoke-ScratchGit $session @('ls-files','-s')) -join "`n") -and $traversalPrimaryIndexBefore -ceq (@(Invoke-ScratchGit $primary @('ls-files','-s')) -join "`n") -and $traversalSessionStatusBefore -ceq (@(Invoke-ScratchGit $session @('status','--porcelain=v1','-z','--untracked-files=all')) -join "`n") -and $traversalPrimaryStatusBefore -ceq (@(Invoke-ScratchGit $primary @('status','--porcelain=v1','-z','--untracked-files=all')) -join "`n")) "$Case preserves indexes and disjoint status"
	Assert-True ($traversalCandidateTextBefore -ceq [IO.File]::ReadAllText((Join-Path $session 'candidate-session.txt'), [Text.UTF8Encoding]::new($false,$true)) -and $traversalPrimaryBaseTextBefore -ceq [IO.File]::ReadAllText((Join-Path $primary 'base.txt'), [Text.UTF8Encoding]::new($false,$true))) "$Case preserves worktree bytes"
}
$run = Invoke-JsonScript $candidateScript @('-Route','session-landing','-CurrentWorktree',$session,'-PrimaryWorktree',$primary,'-CurrentBranch',$sessionBranch,'-PrimaryBranch','main','-Baseline',$baseline,'-ExpectedCurrentTip',$sessionTip,'-ExpectedPrimaryTip',$baseline,'-OwnedPaths','foo/..','-CommitMessageFile',$candidateMessage)
Assert-Outcome $run 'candidate-rejects-terminal-traversal-owned-path' 1 'error' 'input.path-invalid'
& $assertInvalidPathState 'terminal traversal rejection'
foreach ($dotPath in @('./file.txt', 'dir/./file.txt', 'dir/.')) {
	$run = Invoke-JsonScript $candidateScript @('-Route','session-landing','-CurrentWorktree',$session,'-PrimaryWorktree',$primary,'-CurrentBranch',$sessionBranch,'-PrimaryBranch','main','-Baseline',$baseline,'-ExpectedCurrentTip',$sessionTip,'-ExpectedPrimaryTip',$baseline,'-OwnedPaths',$dotPath,'-CommitMessageFile',$candidateMessage)
	Assert-Outcome $run "candidate-rejects-dot-owned-path-$dotPath" 1 'error' 'input.path-invalid'
	& $assertInvalidPathState "dot path '$dotPath' rejection"
}
$run = Invoke-JsonScript $candidateScript @('-Route','session-landing','-CurrentWorktree',$session,'-PrimaryWorktree',$primary,'-CurrentBranch',$sessionBranch,'-PrimaryBranch','main','-Baseline',$baseline,'-ExpectedCurrentTip',$sessionTip,'-ExpectedPrimaryTip',$baseline,'-OwnedPaths','never-tracked-anywhere.txt','-CommitMessageFile',$candidateMessage)
Assert-Outcome $run 'candidate-rejects-owned-path-in-no-tree' 1 'error' 'input.path-not-single-entry'
& $assertInvalidPathState 'unknown owned path rejection'
$literalSessionDiskPath = Join-Path $session ($literalBracketPath.Replace('/','\'))
New-Item -ItemType Directory -Force (Split-Path -Parent $literalSessionDiskPath) | Out-Null
[IO.File]::WriteAllText($literalSessionDiskPath, 'literal session candidate', [Text.UTF8Encoding]::new($false))
$run = Invoke-JsonScript $candidateScript @('-Route','session-landing','-CurrentWorktree',$session,'-PrimaryWorktree',$primary,'-CurrentBranch',$sessionBranch,'-PrimaryBranch','main','-Baseline',$baseline,'-ExpectedCurrentTip',$sessionTip,'-ExpectedPrimaryTip',$baseline,'-OwnedPaths',$literalBracketPath,'-CommitMessageFile',$candidateMessage)
Assert-Outcome $run 'session-candidate-accepts-bracketed-literal-path' 0 'pass' 'candidate.created'
if ($null -ne $run.Json) {
	Assert-True (@($run.Json.ownedPaths).Count -eq 1 -and @($run.Json.ownedPaths)[0] -ceq $literalBracketPath) 'session bracketed candidate owns exactly the literal path'
	$sessionChangedPaths = @(@(Invoke-ScratchGit $session @('diff-tree','--no-commit-id','--name-only','-r',$sessionTip,$run.Json.candidate.commit)) | ForEach-Object { $_.Trim() } | Where-Object { -not [string]::IsNullOrEmpty($_) })
	Assert-True ($sessionChangedPaths.Count -eq 1 -and $sessionChangedPaths[0] -ceq $literalBracketPath) 'session bracketed candidate changes exactly the literal path'
}
Invoke-ScratchGit $session @('reset','--hard',$sessionTip) | Out-Null
Remove-Item -LiteralPath (Join-Path $session 'Engine') -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force (Join-Path $session 'owned-directory') | Out-Null
[IO.File]::WriteAllText((Join-Path $session 'owned-directory\first.txt'), 'first', [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText((Join-Path $session 'owned-directory\second.txt'), 'second', [Text.UTF8Encoding]::new($false))
$run = Invoke-JsonScript $candidateScript @('-Route','session-landing','-CurrentWorktree',$session,'-PrimaryWorktree',$primary,'-CurrentBranch',$sessionBranch,'-PrimaryBranch','main','-Baseline',$baseline,'-ExpectedCurrentTip',$sessionTip,'-ExpectedPrimaryTip',$baseline,'-OwnedPaths','owned-directory','-CommitMessageFile',$candidateMessage)
Assert-Outcome $run 'candidate-rejects-directory-owned-path' 1 'error' 'input.path-directory'
Remove-Item -LiteralPath (Join-Path $session 'owned-directory') -Recurse -Force
$run = Invoke-JsonScript $candidateScript @('-Route','session-landing','-CurrentWorktree',$session,'-PrimaryWorktree',$primary,'-CurrentBranch',$sessionBranch,'-PrimaryBranch','main','-Baseline',$baseline,'-ExpectedCurrentTip',$sessionTip,'-ExpectedPrimaryTip',$baseline,'-OwnedPaths','fixture-gitlink','-CommitMessageFile',$candidateMessage)
Assert-Outcome $run 'candidate-allows-tracked-gitlink-owned-path' 0 'pass' 'candidate.created'
Invoke-ScratchGit $session @('reset','--hard',$sessionTip) | Out-Null
$run = Invoke-JsonScript $candidateScript @('-Route','session-landing','-CurrentWorktree',$session,'-PrimaryWorktree',$primary,'-CurrentBranch','wrong-branch','-PrimaryBranch','main','-Baseline',$baseline,'-ExpectedCurrentTip',$sessionTip,'-ExpectedPrimaryTip',$baseline,'-OwnedPaths','candidate-session.txt','-CommitMessageFile',$candidateMessage)
Assert-Outcome $run 'candidate-rejects-wrong-checked-out-branch' 2 'blocked' 'identity.branch-mismatch'
$run = Invoke-JsonScript $candidateScript @('-Route','session-landing','-CurrentWorktree',$session,'-PrimaryWorktree',$primary,'-CurrentBranch',$sessionBranch,'-PrimaryBranch','main','-Baseline',$baseline,'-ExpectedCurrentTip',$sessionTip,'-ExpectedPrimaryTip',$baseline,'-OwnedPaths','candidate-session.txt','-CommitMessageFile',$candidateMessage)
Assert-Outcome $run 'session-candidate-before-verification' 0 'pass' 'candidate.created'
if ($null -ne $run.Json) {
	Assert-True $run.Json.candidate.singleParent 'session candidate has one parent'
	Assert-True ($run.Json.candidate.parent -ceq $baseline) 'session candidate parent is reconciled primary'
	Assert-True ($run.Json.candidate.tree -ceq ((@(Invoke-ScratchGit $session @('rev-parse', "$($run.Json.candidate.commit)^{tree}")))[0].Trim())) 'session candidate tree identity is exact'
	Assert-True (@($run.Json.ownedPaths) -ccontains 'candidate-session.txt' -and @($run.Json.ownedPaths).Count -eq 1) 'session candidate owns exactly the caller-declared paths'
}
Invoke-ScratchGit $session @('reset','--hard',$baseline) | Out-Null
[IO.File]::WriteAllText((Join-Path $session 'candidate-session.txt'), 'session candidate', [Text.UTF8Encoding]::new($false))
$sessionReservedHistoryPath = Join-Path $session '.agents\skills\code-quality-metrics\references\history\CodeQualityMetricsHistory.jsonl'
$sessionReservedHistoryBytes = [IO.File]::ReadAllBytes($sessionReservedHistoryPath)
[IO.File]::WriteAllBytes($sessionReservedHistoryPath, [byte[]]($sessionReservedHistoryBytes + [Text.UTF8Encoding]::new($false).GetBytes("dirty`n")))
$run = Invoke-JsonScript $candidateScript @('-Route','session-landing','-CurrentWorktree',$session,'-PrimaryWorktree',$primary,'-CurrentBranch',$sessionBranch,'-PrimaryBranch','main','-Baseline',$baseline,'-ExpectedCurrentTip',$baseline,'-ExpectedPrimaryTip',$baseline,'-OwnedPaths','candidate-session.txt','-CommitMessageFile',$candidateMessage)
Assert-Outcome $run 'session-blocks-dirty-reserved-history' 2 'blocked' 'history.source-dirty'
[IO.File]::WriteAllBytes($sessionReservedHistoryPath, $sessionReservedHistoryBytes)

# A primary-only reserved-history commit must not be attributed to the session patch. The
# session remains at the validated baseline while primary advances, then the candidate adds
# only an ordinary caller-owned path. The same topology still rejects committed session-side
# history modification and deletion without changing either checkout's refs, indexes, or status.
Invoke-ScratchGit $session @('reset','--hard',$baseline) | Out-Null
Invoke-ScratchGit $session @('clean','-fd') | Out-Null
Invoke-ScratchGit $primary @('reset','--hard',$baseline) | Out-Null
$primaryOverlay = New-ReservedHistoryCommit $primary $baseline $historyJsonFixturePath $false 'primary-only history overlay`n'
Invoke-ScratchGit $primary @('update-ref','refs/heads/main',$primaryOverlay.Commit) | Out-Null
Invoke-ScratchGit $primary @('reset','--hard',$primaryOverlay.Commit) | Out-Null
[IO.File]::WriteAllText((Join-Path $session 'overlay-owned.txt'), 'session overlay candidate', [Text.UTF8Encoding]::new($false))
$run = Invoke-JsonScript $candidateScript @('-Route','session-landing','-CurrentWorktree',$session,'-PrimaryWorktree',$primary,'-CurrentBranch',$sessionBranch,'-PrimaryBranch','main','-Baseline',$baseline,'-ExpectedCurrentTip',$baseline,'-ExpectedPrimaryTip',$primaryOverlay.Commit,'-OwnedPaths','overlay-owned.txt','-CommitMessageFile',$candidateMessage)
Assert-Outcome $run 'session-candidate-allows-primary-only-history-overlay' 0 'pass' 'candidate.created'
if ($null -ne $run.Json) {
	$overlayChangedPaths = @(@(Invoke-ScratchGit $session @('diff-tree','--no-commit-id','--name-only','-r',$baseline,$run.Json.candidate.commit)) | ForEach-Object { $_.Trim() } | Where-Object { -not [string]::IsNullOrEmpty($_) })
	Assert-True ($overlayChangedPaths.Count -eq 1 -and $overlayChangedPaths[0] -ceq 'overlay-owned.txt') 'primary-only history overlay candidate changes exactly the owned path'
	Assert-True (@($overlayChangedPaths | Where-Object { $_ -ceq $historyJsonFixturePath -or $_ -ceq $historySvgFixturePath }).Count -eq 0) 'primary-only history overlay candidate introduces no reserved history path'
}
foreach ($overlayReservedCase in @(
	[pscustomobject]@{ Name = 'modify'; Path = $historyJsonFixturePath; Delete = $false; Content = 'session overlay history modification`n' },
	[pscustomobject]@{ Name = 'delete'; Path = $historySvgFixturePath; Delete = $true; Content = $null }
)) {
	Invoke-ScratchGit $session @('reset','--hard',$baseline) | Out-Null
	Invoke-ScratchGit $session @('clean','-fd') | Out-Null
	[IO.File]::WriteAllText((Join-Path $session 'overlay-owned.txt'), 'session overlay candidate', [Text.UTF8Encoding]::new($false))
	$overlayBadSource = New-ReservedHistoryCommit $session $baseline $overlayReservedCase.Path $overlayReservedCase.Delete $overlayReservedCase.Content
	Invoke-ScratchGit $session @('update-ref',"refs/heads/$sessionBranch",$overlayBadSource.Commit) | Out-Null
	Invoke-ScratchGit $session @('reset','--hard',$overlayBadSource.Commit) | Out-Null
	$overlaySessionRefBefore = (@(Invoke-ScratchGit $session @('rev-parse',"refs/heads/$sessionBranch")))[0].Trim()
	$overlayPrimaryRefBefore = (@(Invoke-ScratchGit $primary @('rev-parse','refs/heads/main')))[0].Trim()
	$overlaySessionIndexBefore = (@(Invoke-ScratchGit $session @('ls-files','-s')) -join "`n")
	$overlayPrimaryIndexBefore = (@(Invoke-ScratchGit $primary @('ls-files','-s')) -join "`n")
	$overlaySessionStatusBefore = (@(Invoke-ScratchGit $session @('status','--porcelain=v1','-z','--untracked-files=all')) -join "`n")
	$overlayPrimaryStatusBefore = (@(Invoke-ScratchGit $primary @('status','--porcelain=v1','-z','--untracked-files=all')) -join "`n")
	$overlaySessionHistoryPath = Join-Path $session ($overlayReservedCase.Path.Replace('/','\'))
	$overlayOwnedPath = Join-Path $session 'overlay-owned.txt'
	$overlayPrimaryHistoryJsonPath = Join-Path $primary ($historyJsonFixturePath.Replace('/','\'))
	$overlayPrimaryHistorySvgPath = Join-Path $primary ($historySvgFixturePath.Replace('/','\'))
	$overlaySessionHistoryExistsBefore = Test-Path -LiteralPath $overlaySessionHistoryPath -PathType Leaf
	$overlaySessionHistoryBytesBefore = if ($overlaySessionHistoryExistsBefore) { [IO.File]::ReadAllBytes($overlaySessionHistoryPath) } else { @() }
	$overlayOwnedBytesBefore = [IO.File]::ReadAllBytes($overlayOwnedPath)
	$overlayPrimaryHistoryJsonBytesBefore = [IO.File]::ReadAllBytes($overlayPrimaryHistoryJsonPath)
	$overlayPrimaryHistorySvgBytesBefore = [IO.File]::ReadAllBytes($overlayPrimaryHistorySvgPath)
	$run = Invoke-JsonScript $candidateScript @('-Route','session-landing','-CurrentWorktree',$session,'-PrimaryWorktree',$primary,'-CurrentBranch',$sessionBranch,'-PrimaryBranch','main','-Baseline',$baseline,'-ExpectedCurrentTip',$overlayBadSource.Commit,'-ExpectedPrimaryTip',$primaryOverlay.Commit,'-OwnedPaths','overlay-owned.txt','-CommitMessageFile',$candidateMessage)
	Assert-Outcome $run "session-candidate-blocks-primary-overlay-session-history-$($overlayReservedCase.Name)" 2 'blocked' 'history.source-changed'
	Assert-True ($overlaySessionRefBefore -ceq ((@(Invoke-ScratchGit $session @('rev-parse',"refs/heads/$sessionBranch")))[0].Trim()) -and $overlayPrimaryRefBefore -ceq ((@(Invoke-ScratchGit $primary @('rev-parse','refs/heads/main')))[0].Trim())) "primary-overlay-session-history-$($overlayReservedCase.Name) preserves session and primary refs"
	Assert-True ($overlaySessionIndexBefore -ceq (@(Invoke-ScratchGit $session @('ls-files','-s')) -join "`n") -and $overlayPrimaryIndexBefore -ceq (@(Invoke-ScratchGit $primary @('ls-files','-s')) -join "`n")) "primary-overlay-session-history-$($overlayReservedCase.Name) preserves session and primary indexes"
	Assert-True ($overlaySessionStatusBefore -ceq (@(Invoke-ScratchGit $session @('status','--porcelain=v1','-z','--untracked-files=all')) -join "`n") -and $overlayPrimaryStatusBefore -ceq (@(Invoke-ScratchGit $primary @('status','--porcelain=v1','-z','--untracked-files=all')) -join "`n")) "primary-overlay-session-history-$($overlayReservedCase.Name) preserves session and primary worktree status"
	$overlaySessionHistoryExistsAfter = Test-Path -LiteralPath $overlaySessionHistoryPath -PathType Leaf
	$overlaySessionHistoryBytesAfter = if ($overlaySessionHistoryExistsAfter) { [IO.File]::ReadAllBytes($overlaySessionHistoryPath) } else { @() }
	Assert-True ($overlaySessionHistoryExistsBefore -eq $overlaySessionHistoryExistsAfter -and ((-not $overlaySessionHistoryExistsBefore) -or [Linq.Enumerable]::SequenceEqual([byte[]]$overlaySessionHistoryBytesBefore,[byte[]]$overlaySessionHistoryBytesAfter))) "primary-overlay-session-history-$($overlayReservedCase.Name) preserves session history worktree bytes"
	Assert-True ([Linq.Enumerable]::SequenceEqual([byte[]]$overlayOwnedBytesBefore,[byte[]](Get-Content -LiteralPath $overlayOwnedPath -AsByteStream))) "primary-overlay-session-history-$($overlayReservedCase.Name) preserves owned worktree bytes"
	Assert-True ([Linq.Enumerable]::SequenceEqual([byte[]]$overlayPrimaryHistoryJsonBytesBefore,[byte[]](Get-Content -LiteralPath $overlayPrimaryHistoryJsonPath -AsByteStream)) -and [Linq.Enumerable]::SequenceEqual([byte[]]$overlayPrimaryHistorySvgBytesBefore,[byte[]](Get-Content -LiteralPath $overlayPrimaryHistorySvgPath -AsByteStream))) "primary-overlay-session-history-$($overlayReservedCase.Name) preserves primary history worktree bytes"
}
Invoke-ScratchGit $session @('reset','--hard',$baseline) | Out-Null
Invoke-ScratchGit $session @('clean','-fd') | Out-Null
Invoke-ScratchGit $primary @('reset','--hard',$baseline) | Out-Null

# A reserved history path in the approved source commit is a source-change blocker, not a
# recoverable landing. Exercise both modification and deletion while keeping primary and the
# source checkout at the old tree after each rejection.
Invoke-ScratchGit $session @('reset','--hard',$baseline) | Out-Null
Invoke-ScratchGit $session @('clean','-fd') | Out-Null
$sessionHistoryRefBefore = (@(Invoke-ScratchGit $session @('rev-parse', "refs/heads/$sessionBranch")))[0].Trim()
$primaryHistoryRefBefore = (@(Invoke-ScratchGit $primary @('rev-parse', 'refs/heads/main')))[0].Trim()
$primaryJsonBlobBefore = (@(Invoke-ScratchGit $primary @('rev-parse', "$primaryHistoryRefBefore`:$historyJsonFixturePath")))[0].Trim()
$primarySvgBlobBefore = (@(Invoke-ScratchGit $primary @('rev-parse', "$primaryHistoryRefBefore`:$historySvgFixturePath")))[0].Trim()
foreach ($reservedCase in @(
	[pscustomobject]@{ Name = 'modify'; Path = $historyJsonFixturePath; Delete = $false; Content = 'approved source history modification`n' },
	[pscustomobject]@{ Name = 'delete'; Path = $historySvgFixturePath; Delete = $true; Content = $null }
)) {
	Invoke-ScratchGit $session @('reset','--hard',$baseline) | Out-Null
	Invoke-ScratchGit $session @('clean','-fd') | Out-Null
	$badSource = New-ReservedHistoryCommit $session $baseline $reservedCase.Path $reservedCase.Delete $reservedCase.Content
	Invoke-ScratchGit $session @('update-ref', "refs/heads/$sessionBranch", $badSource.Commit) | Out-Null
	Invoke-ScratchGit $session @('reset','--hard',$badSource.Commit) | Out-Null
	$badLandingParameters = [ordered]@{ CurrentWorktree=$session; PrimaryWorktree=$primary; CurrentBranch=$sessionBranch; PrimaryBranch='main'; ExpectedCurrentTip=$badSource.Commit; ExpectedPrimaryTip=$baseline; SessionLabel='finalize-fixture'; ApprovedSessionCommit=$badSource.Commit; ApprovedCandidateTree=$badSource.Tree; HistoryContractMode='carry-forward' }
	$badLanding = Invoke-JsonScriptWithSplat $landingScript $badLandingParameters $scratchBase
	Assert-Outcome $badLanding "session-approved-reserved-$($reservedCase.Name)" 2 'blocked' 'history.source-changed'
	Assert-True ($primaryHistoryRefBefore -ceq ((@(Invoke-ScratchGit $primary @('rev-parse','refs/heads/main')))[0].Trim()) -and $primaryHistoryRefBefore -ceq ((@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim())) "session-approved-reserved-$($reservedCase.Name) leaves primary ref and checkout unchanged"
	Assert-True ($primaryJsonBlobBefore -ceq ((@(Invoke-ScratchGit $primary @('rev-parse', "$primaryHistoryRefBefore`:$historyJsonFixturePath")))[0].Trim()) -and $primarySvgBlobBefore -ceq ((@(Invoke-ScratchGit $primary @('rev-parse', "$primaryHistoryRefBefore`:$historySvgFixturePath")))[0].Trim())) "session-approved-reserved-$($reservedCase.Name) leaves primary history blobs unchanged"
	Invoke-ScratchGit $session @('reset','--hard',$baseline) | Out-Null
	Invoke-ScratchGit $session @('clean','-fd') | Out-Null
	Assert-True ($sessionHistoryRefBefore -ceq ((@(Invoke-ScratchGit $session @('rev-parse', "refs/heads/$sessionBranch")))[0].Trim()) -and $baseline -ceq ((@(Invoke-ScratchGit $session @('rev-parse','HEAD')))[0].Trim())) "session-approved-reserved-$($reservedCase.Name) restores source branch and checkout"
}

[IO.File]::WriteAllText((Join-Path $session 'staged-unrelated.txt'), 'staged unrelated', [Text.UTF8Encoding]::new($false))
Invoke-ScratchGit $session @('add','staged-unrelated.txt') | Out-Null
[IO.File]::WriteAllText((Join-Path $session 'base.txt'), 'unstaged unrelated', [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText((Join-Path $session 'untracked-unrelated.txt'), 'untracked unrelated', [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText((Join-Path $session 'owned-state.txt'), 'owned candidate', [Text.UTF8Encoding]::new($false))
$stateBefore = ((@(Invoke-ScratchGit $session @('status','--porcelain=v1','-z','--untracked-files=all')) -join "`n"))
$disjointBefore = (($stateBefore -replace [string][char]0,"`n") -split "`n" | Where-Object { $_ -match 'unrelated' } | Sort-Object) -join "`n"
$indexBefore = ((@(Invoke-ScratchGit $session @('ls-files','-s')) -join "`n"))
$run = Invoke-JsonScript $candidateScript @('-Route','session-landing','-CurrentWorktree',$session,'-PrimaryWorktree',$primary,'-CurrentBranch',$sessionBranch,'-PrimaryBranch','main','-Baseline',$baseline,'-ExpectedCurrentTip',$baseline,'-ExpectedPrimaryTip',$baseline,'-OwnedPaths','owned-state.txt','-CommitMessageFile',$candidateMessage)
Assert-Outcome $run 'session-candidate-preserves-disjoint-state' 0 'pass' 'candidate.created'
Assert-True ($indexBefore -cne ((@(Invoke-ScratchGit $session @('ls-files','-s')) -join "`n")) -and [string]::IsNullOrWhiteSpace((@(Invoke-ScratchGit $session @('status','--porcelain','--','owned-state.txt')) -join "`n"))) 'session candidate reconciles only owned real-index state cleanly'
$stateAfter = ((@(Invoke-ScratchGit $session @('status','--porcelain=v1','-z','--untracked-files=all')) -join "`n"))
$disjointAfter = (($stateAfter -replace [string][char]0,"`n") -split "`n" | Where-Object { $_ -match 'unrelated' } | Sort-Object) -join "`n"
Assert-True ($disjointBefore -ceq $disjointAfter) 'session candidate preserves disjoint staged unstaged and untracked status'
Invoke-ScratchGit $session @('reset','--hard',$baseline) | Out-Null
Remove-Item -LiteralPath (Join-Path $session 'staged-unrelated.txt'),(Join-Path $session 'untracked-unrelated.txt'),(Join-Path $session 'owned-state.txt') -Force -ErrorAction SilentlyContinue
[IO.File]::WriteAllText((Join-Path $session 'rollback-unrelated.txt'), 'staged unrelated', [Text.UTF8Encoding]::new($false))
Invoke-ScratchGit $session @('add','rollback-unrelated.txt') | Out-Null
[IO.File]::WriteAllText((Join-Path $session 'rollback-active-owned.txt'), 'owned candidate', [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText((Join-Path $session 'rollback-staged-owned.txt'), 'owned staged', [Text.UTF8Encoding]::new($false))
Invoke-ScratchGit $session @('add','rollback-staged-owned.txt') | Out-Null
$rollbackSessionHead = (@(Invoke-ScratchGit $session @('rev-parse','HEAD')))[0].Trim()
$rollbackSessionIndex = (@(Invoke-ScratchGit $session @('ls-files','-s')) -join "`n")
$rollbackSessionStatus = (@(Invoke-ScratchGit $session @('status','--porcelain=v1','-z','--untracked-files=all')) -join "`n")
$rollbackStagedIndex = (@(Invoke-ScratchGit $session @('ls-files','--stage','--','rollback-staged-owned.txt')) -join "`n")
$rollbackActiveWorktree = [IO.File]::ReadAllText((Join-Path $session 'rollback-active-owned.txt'), [Text.UTF8Encoding]::new($false,$true))
$rollbackStagedWorktree = [IO.File]::ReadAllText((Join-Path $session 'rollback-staged-owned.txt'), [Text.UTF8Encoding]::new($false,$true))
$rollbackUnrelatedWorktree = [IO.File]::ReadAllText((Join-Path $session 'rollback-unrelated.txt'), [Text.UTF8Encoding]::new($false,$true))
$run = Invoke-JsonScript $candidateScript @('-Route','session-landing','-CurrentWorktree',$session,'-PrimaryWorktree',$primary,'-CurrentBranch',$sessionBranch,'-PrimaryBranch','main','-Baseline',$baseline,'-ExpectedCurrentTip',$baseline,'-ExpectedPrimaryTip',$baseline,'-OwnedPaths','rollback-active-owned.txt,rollback-staged-owned.txt','-CommitMessageFile',$candidateMessage,'-FixtureFailure','post-index-mutation')
Assert-Outcome $run 'session-candidate-post-index-rollback' 2 'blocked' 'candidate.postcondition-failed'
Assert-True ($rollbackSessionHead -ceq ((@(Invoke-ScratchGit $session @('rev-parse','HEAD')))[0].Trim())) 'session post-index rollback restores the guarded ref'
Assert-True ($rollbackSessionIndex -ceq (@(Invoke-ScratchGit $session @('ls-files','-s')) -join "`n")) 'session post-index rollback restores owned and unrelated index entries'
Assert-True ($rollbackSessionStatus -ceq (@(Invoke-ScratchGit $session @('status','--porcelain=v1','-z','--untracked-files=all')) -join "`n")) 'session post-index rollback preserves owned and unrelated worktree status'
$rollbackStagedIndexAfter = (@(Invoke-ScratchGit $session @('ls-files','--stage','--','rollback-staged-owned.txt')) -join "`n")
Assert-True ($rollbackStagedIndex -ceq $rollbackStagedIndexAfter) 'session post-index rollback restores staged owned mode object and stage exactly'
Assert-True ($rollbackActiveWorktree -ceq [IO.File]::ReadAllText((Join-Path $session 'rollback-active-owned.txt'), [Text.UTF8Encoding]::new($false,$true)) -and $rollbackStagedWorktree -ceq [IO.File]::ReadAllText((Join-Path $session 'rollback-staged-owned.txt'), [Text.UTF8Encoding]::new($false,$true))) 'session post-index rollback preserves owned worktree bytes'
Assert-True ($rollbackUnrelatedWorktree -ceq [IO.File]::ReadAllText((Join-Path $session 'rollback-unrelated.txt'), [Text.UTF8Encoding]::new($false,$true))) 'session post-index rollback preserves unrelated worktree bytes'
Invoke-ScratchGit $session @('reset','--hard',$baseline) | Out-Null
Remove-Item -LiteralPath (Join-Path $session 'rollback-unrelated.txt'),(Join-Path $session 'rollback-active-owned.txt'),(Join-Path $session 'rollback-staged-owned.txt') -Force -ErrorAction SilentlyContinue
[IO.File]::WriteAllText((Join-Path $session 'mixed-owned.txt'), 'staged', [Text.UTF8Encoding]::new($false)); Invoke-ScratchGit $session @('add','mixed-owned.txt') | Out-Null
[IO.File]::WriteAllText((Join-Path $session 'mixed-owned.txt'), 'unstaged after staged', [Text.UTF8Encoding]::new($false))
$run = Invoke-JsonScript $candidateScript @('-Route','session-landing','-CurrentWorktree',$session,'-PrimaryWorktree',$primary,'-CurrentBranch',$sessionBranch,'-PrimaryBranch','main','-Baseline',$baseline,'-ExpectedCurrentTip',$baseline,'-ExpectedPrimaryTip',$baseline,'-OwnedPaths','mixed-owned.txt','-CommitMessageFile',$candidateMessage)
Assert-Outcome $run 'session-candidate-blocks-mixed-owned-state' 2 'blocked' 'git.owned-path-mixed-state'
Assert-True ($null -ne $run.Json -and $run.Json.message -clike '*git add -- mixed-owned.txt*') 'mixed owned state blocker names the git add remedy for the mixed path'
Invoke-ScratchGit $session @('reset','--hard',$baseline) | Out-Null
Remove-Item -LiteralPath (Join-Path $session 'mixed-owned.txt') -Force -ErrorAction SilentlyContinue
$literalPrimaryDiskPath = Join-Path $primary ($literalBracketPath.Replace('/','\'))
New-Item -ItemType Directory -Force (Split-Path -Parent $literalPrimaryDiskPath) | Out-Null
[IO.File]::WriteAllText($literalPrimaryDiskPath, 'literal primary candidate', [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText((Join-Path $primary 'literal-disjoint-staged.txt'), 'staged disjoint', [Text.UTF8Encoding]::new($false))
Invoke-ScratchGit $primary @('add','literal-disjoint-staged.txt') | Out-Null
[IO.File]::WriteAllText((Join-Path $primary 'base.txt'), 'literal disjoint unstaged', [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText((Join-Path $primary 'literal-disjoint-untracked.txt'), 'untracked disjoint', [Text.UTF8Encoding]::new($false))
$literalPrimaryIndexBefore = (@(Invoke-ScratchGit $primary @('ls-files','-s')) -join "`n")
$literalPrimaryStatusBefore = (@(Invoke-ScratchGit $primary @('status','--porcelain=v1','-z','--untracked-files=all')) -join "`n")
$literalPrimaryParameters = [ordered]@{ Route='primary-commit'; CurrentWorktree=$primary; PrimaryWorktree=$primary; CurrentBranch='main'; PrimaryBranch='main'; Baseline=$baseline; ExpectedCurrentTip=$baseline; ExpectedPrimaryTip=$baseline; OwnedPaths=@($literalBracketPath); CommitMessageFile=$candidateMessage }
$previousHistoryReceiptMode = [Environment]::GetEnvironmentVariable('BROKEN_ENGINE_FINALIZE_HISTORY_RECEIPT')
[Environment]::SetEnvironmentVariable('BROKEN_ENGINE_FINALIZE_HISTORY_RECEIPT', 'contract-nested-unknown')
$malformedContract = Invoke-JsonScriptWithSplat $candidateScript $literalPrimaryParameters $scratchBase
Assert-Outcome $malformedContract 'primary-candidate-rejects-malformed-nested-contract-receipt' 2 'blocked' 'history.contract-invalid'
[Environment]::SetEnvironmentVariable('BROKEN_ENGINE_FINALIZE_HISTORY_RECEIPT', $previousHistoryReceiptMode)
$run = Invoke-JsonScriptWithSplat $candidateScript $literalPrimaryParameters $scratchBase
Assert-Outcome $run 'primary-candidate-accepts-bracketed-literal-path' 0 'pass' 'candidate.created'
if ($null -ne $run.Json) {
	Assert-True (@($run.Json.ownedPaths).Count -eq 1 -and @($run.Json.ownedPaths)[0] -ceq $literalBracketPath) 'primary bracketed candidate owns exactly the literal path'
	Assert-True ($run.Json.historyContract.receipt.schemaVersion -ceq 'broken-engine-code-quality-history-contract/v1') 'primary candidate carries the history Contract before verification'
	$primaryChangedPaths = @(@(Invoke-ScratchGit $primary @('diff-tree','--no-commit-id','--name-only','-r',$baseline,$run.Json.candidate.commit)) | ForEach-Object { $_.Trim() } | Where-Object { -not [string]::IsNullOrEmpty($_) })
	Assert-True ($primaryChangedPaths.Count -eq 1 -and $primaryChangedPaths[0] -ceq $literalBracketPath) 'primary bracketed candidate changes exactly the literal path'
	Assert-True ($literalPrimaryIndexBefore -ceq (@(Invoke-ScratchGit $primary @('ls-files','-s')) -join "`n") -and $literalPrimaryStatusBefore -ceq (@(Invoke-ScratchGit $primary @('status','--porcelain=v1','-z','--untracked-files=all')) -join "`n")) 'primary bracketed candidate preserves real index and disjoint staged unstaged untracked state'
}
Invoke-ScratchGit $primary @('reset','--hard',$baseline) | Out-Null
Invoke-ScratchGit $primary @('clean','-fd') | Out-Null
$reservedHistoryFixturePath = Join-Path $primary '.agents\skills\code-quality-metrics\references\history\CodeQualityMetricsHistory.jsonl'
$reservedHistoryFixtureBytes = [IO.File]::ReadAllBytes($reservedHistoryFixturePath)
[IO.File]::WriteAllBytes($reservedHistoryFixturePath, [byte[]]($reservedHistoryFixtureBytes + [Text.UTF8Encoding]::new($false).GetBytes("dirty`n")))
$run = Invoke-JsonScript $candidateScript @('-Route','primary-commit','-CurrentWorktree',$primary,'-PrimaryWorktree',$primary,'-CurrentBranch','main','-PrimaryBranch','main','-Baseline',$baseline,'-ExpectedCurrentTip',$baseline,'-ExpectedPrimaryTip',$baseline,'-OwnedPaths','base.txt','-CommitMessageFile',$candidateMessage)
Assert-Outcome $run 'direct-primary-blocks-dirty-reserved-history' 2 'blocked' 'history.source-dirty'
[IO.File]::WriteAllBytes($reservedHistoryFixturePath, $reservedHistoryFixtureBytes)
[IO.File]::WriteAllText((Join-Path $primary 'primary-active-owned.cpp'), 'primary candidate', [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText((Join-Path $primary 'primary-staged-owned.txt'), 'primary staged', [Text.UTF8Encoding]::new($false))
Invoke-ScratchGit $primary @('add','primary-staged-owned.txt') | Out-Null
[IO.File]::WriteAllText((Join-Path $primary 'primary-disjoint-staged.txt'), 'staged', [Text.UTF8Encoding]::new($false))
Invoke-ScratchGit $primary @('add','primary-disjoint-staged.txt') | Out-Null
[IO.File]::WriteAllText((Join-Path $primary 'base.txt'), 'disjoint unstaged', [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText((Join-Path $primary 'primary-disjoint-untracked.txt'), 'untracked', [Text.UTF8Encoding]::new($false))
$primaryDisjointBefore = (@(Invoke-ScratchGit $primary @('status','--porcelain=v1','--untracked-files=all')) -join "`n")
$beforePrimaryIndex = (@(Invoke-ScratchGit $primary @('ls-files','-s')) -join "`n")
$primaryStagedOwnedIndex = (@(Invoke-ScratchGit $primary @('ls-files','--stage','--','primary-staged-owned.txt')) -join "`n")
$primaryActiveOwnedWorktree = [IO.File]::ReadAllText((Join-Path $primary 'primary-active-owned.cpp'), [Text.UTF8Encoding]::new($false,$true))
$primaryStagedOwnedWorktree = [IO.File]::ReadAllText((Join-Path $primary 'primary-staged-owned.txt'), [Text.UTF8Encoding]::new($false,$true))
$primaryUnrelatedWorktree = [IO.File]::ReadAllText((Join-Path $primary 'primary-disjoint-untracked.txt'), [Text.UTF8Encoding]::new($false,$true))
# `pwsh -File` binds only the first token of a multi-value parameter, so this two-owned-path route is invoked
# through the splat wrapper, which passes the array intact.
function New-PrimaryCandidateParameters([Collections.IDictionary] $Extra = @{}) {
	$parameters = [ordered]@{ Route='primary-commit'; CurrentWorktree=$primary; PrimaryWorktree=$primary; CurrentBranch='main'; PrimaryBranch='main'; Baseline=$baseline; ExpectedCurrentTip=$baseline; ExpectedPrimaryTip=$baseline; OwnedPaths=@('primary-active-owned.cpp','primary-staged-owned.txt'); CommitMessageFile=$candidateMessage }
	if ($null -ne $script:PrimaryAdvanceOwner) { $parameters.OwnerToken=$script:PrimaryAdvanceOwner; $parameters.SessionLabel='finalize-fixture'; $parameters.HistoryContractDigest=$script:PrimaryHistoryContract.digest; $parameters.HistoryContractGeneratorDigest=$script:PrimaryHistoryContract.generatorDigest; $parameters.HistoryContractCaptureDigest=$script:PrimaryHistoryContract.captureDigest; $parameters.HistoryContractRuntimeDigest=$script:PrimaryHistoryContract.runtimeDigest; $parameters.HistoryContractPatchDigest=$script:PrimaryHistoryContract.patchDigest; $parameters.HistoryContractMode=$script:PrimaryHistoryContract.mode; $parameters.WorktreeCliExecutable=(Join-Path $primaryOutput 'WorktreeCli.exe') }
	foreach ($entry in $Extra.GetEnumerator()) { $parameters[$entry.Key] = $entry.Value }
	return $parameters
}
$run = Invoke-JsonScriptWithSplat $candidateScript (New-PrimaryCandidateParameters) $scratchBase
Assert-Outcome $run 'primary-candidate-temporary-index' 0 'pass' 'candidate.created'
if ($null -ne $run.Json) {
	$verifiedCandidate = $run.Json.candidate.commit; $verifiedTree = $run.Json.candidate.tree; $script:PrimaryHistoryContract = $run.Json.historyContract; $script:PrimaryAdvanceOwner = [guid]::NewGuid().ToString(); $primaryCommonDirectory = ((@(Invoke-ScratchGit $primary @('rev-parse','--path-format=absolute','--git-common-dir')))[0].Trim()); Invoke-WorktreeCli @('lock','claim','--repo',$primaryCommonDirectory,'--owner',$script:PrimaryAdvanceOwner,'--session','finalize-fixture','--worktree',$primary,'--lease-seconds','3600') | Out-Null
	Assert-True ($beforePrimaryIndex -ceq ((@(Invoke-ScratchGit $primary @('ls-files','-s')) -join "`n"))) 'temporary index preserves real index'
	Assert-True ($primaryDisjointBefore -ceq (@(Invoke-ScratchGit $primary @('status','--porcelain=v1','--untracked-files=all')) -join "`n")) 'primary candidate preserves disjoint staged unstaged and untracked state'
	$rollback = Invoke-JsonScriptWithSplat $candidateScript (New-PrimaryCandidateParameters @{ VerifiedCandidateCommit=$verifiedCandidate; VerifiedCandidateTree=$verifiedTree; AdvancePrimary=$true; FixtureFailure='postcondition' }) $scratchBase
	Assert-Outcome $rollback 'primary-candidate-postcondition-rollback' 2 'blocked' 'candidate.postcondition-failed'
	Assert-True ($baseline -ceq ((@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim())) 'guarded rollback restores expected old primary only from candidate'
	Assert-True ($beforePrimaryIndex -ceq ((@(Invoke-ScratchGit $primary @('ls-files','-s')) -join "`n"))) 'guarded rollback preserves real index'
	$indexRollback = Invoke-JsonScriptWithSplat $candidateScript (New-PrimaryCandidateParameters @{ VerifiedCandidateCommit=$verifiedCandidate; VerifiedCandidateTree=$verifiedTree; AdvancePrimary=$true; FixtureFailure='post-index-mutation' }) $scratchBase
	Assert-Outcome $indexRollback 'primary-candidate-post-index-rollback' 2 'blocked' 'candidate.postcondition-failed'
	Assert-True ($baseline -ceq ((@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim())) 'primary post-index rollback restores expected old ref'
	Assert-True ($beforePrimaryIndex -ceq ((@(Invoke-ScratchGit $primary @('ls-files','-s')) -join "`n"))) 'primary post-index rollback restores owned and unrelated index entries'
	Assert-True ($primaryDisjointBefore -ceq (@(Invoke-ScratchGit $primary @('status','--porcelain=v1','--untracked-files=all')) -join "`n")) 'primary post-index rollback restores owned and unrelated status'
	Assert-True ($primaryStagedOwnedIndex -ceq (@(Invoke-ScratchGit $primary @('ls-files','--stage','--','primary-staged-owned.txt')) -join "`n")) 'primary post-index rollback restores staged owned mode object and stage exactly'
	Assert-True ($primaryActiveOwnedWorktree -ceq [IO.File]::ReadAllText((Join-Path $primary 'primary-active-owned.cpp'), [Text.UTF8Encoding]::new($false,$true)) -and $primaryStagedOwnedWorktree -ceq [IO.File]::ReadAllText((Join-Path $primary 'primary-staged-owned.txt'), [Text.UTF8Encoding]::new($false,$true))) 'primary post-index rollback preserves owned worktree bytes'
	Assert-True ($primaryUnrelatedWorktree -ceq [IO.File]::ReadAllText((Join-Path $primary 'primary-disjoint-untracked.txt'), [Text.UTF8Encoding]::new($false,$true))) 'primary post-index rollback preserves unrelated worktree bytes'
	$advance = Invoke-JsonScriptWithSplat $candidateScript (New-PrimaryCandidateParameters @{ VerifiedCandidateCommit=$verifiedCandidate; VerifiedCandidateTree=$verifiedTree; AdvancePrimary=$true }) $scratchBase
	Assert-Outcome $advance 'primary-candidate-atomic-advance' 0 'pass' 'candidate.advanced'
	Assert-True ($advance.Json.final.replacement -and $advance.Json.final.commit -ceq ((@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim())) 'primary branch equals deterministic history replacement'
	Assert-True ($advance.Json.final.tree -ceq ((@(Invoke-ScratchGit $primary @('rev-parse',"$($advance.Json.final.commit)^{tree}")))[0].Trim())) 'primary tree equals deterministic history replacement tree'
}
Invoke-WorktreeCli @('lock','release','--repo',$primaryCommonDirectory,'--owner',$script:PrimaryAdvanceOwner) | Out-Null
Invoke-ScratchGit $primary @('reset','--hard',$baseline) | Out-Null
Remove-Item -LiteralPath (Join-Path $primary 'primary-active-owned.cpp'),(Join-Path $primary 'primary-staged-owned.txt'),(Join-Path $primary 'primary-disjoint-staged.txt'),(Join-Path $primary 'primary-disjoint-untracked.txt') -Force -ErrorAction SilentlyContinue

# A verified primary candidate is also rejected when its tree changes a reserved history path.
# Build those two candidate objects through a temporary fixture index, restore the real checkout,
# then resume the primary route under a real landing lease so the rejection is before CAS/Temp.
foreach ($reservedCase in @(
	[pscustomobject]@{ Name = 'modify'; Path = $historyJsonFixturePath; Delete = $false; Content = 'verified primary history modification`n' },
	[pscustomobject]@{ Name = 'delete'; Path = $historySvgFixturePath; Delete = $true; Content = $null }
)) {
	Invoke-ScratchGit $primary @('reset','--hard',$baseline) | Out-Null
	Invoke-ScratchGit $primary @('clean','-fd') | Out-Null
	$badCandidate = New-ReservedHistoryCommit $primary $baseline $reservedCase.Path $reservedCase.Delete $reservedCase.Content
	Invoke-ScratchGit $primary @('reset','--hard',$baseline) | Out-Null
	Invoke-ScratchGit $primary @('clean','-fd') | Out-Null
	$contractRun = Invoke-JsonScript (Join-Path $historyScriptRoot 'Invoke-CodeQualityMetricsHistory.ps1') @('-Mode','Contract','-RepositoryRoot',$primary,'-BaseCommit',$baseline,'-TipCommit',$badCandidate.Commit)
	Assert-True ($contractRun.ExitCode -eq 0 -and $null -ne $contractRun.Json) "primary-reserved-$($reservedCase.Name)-contract is valid"
	if ($null -ne $contractRun.Json) {
		$badContract = $contractRun.Json
		$badPatchDigest = Get-FixtureJsonDigest ([ordered]@{ changes = $badContract.patch.changes; metricSupportedChanges = $badContract.patch.metricSupportedChanges; cppChanged = $badContract.patch.cppChanged })
		$badPrimaryCommon = ((@(Invoke-ScratchGit $primary @('rev-parse','--path-format=absolute','--git-common-dir')))[0].Trim())
		$badOwner = [guid]::NewGuid().ToString()
		Invoke-WorktreeCli @('lock','claim','--repo',$badPrimaryCommon,'--owner',$badOwner,'--session','finalize-fixture','--worktree',$primary,'--lease-seconds','3600') | Out-Null
		$badPrimaryRefBefore = (@(Invoke-ScratchGit $primary @('rev-parse','refs/heads/main')))[0].Trim()
		$badPrimaryStatusBefore = (@(Invoke-ScratchGit $primary @('status','--porcelain=v1','-z','--untracked-files=all')) -join '')
		$badSessionRefBefore = (@(Invoke-ScratchGit $session @('rev-parse',"refs/heads/$sessionBranch")))[0].Trim()
		$badSessionStatusBefore = (@(Invoke-ScratchGit $session @('status','--porcelain=v1','-z','--untracked-files=all')) -join '')
		$badAdvanceParameters = [ordered]@{ Route='primary-commit'; CurrentWorktree=$primary; PrimaryWorktree=$primary; CurrentBranch='main'; PrimaryBranch='main'; Baseline=$baseline; ExpectedCurrentTip=$baseline; ExpectedPrimaryTip=$baseline; OwnedPaths=@($reservedCase.Path); CommitMessageFile=$candidateMessage; VerifiedCandidateCommit=$badCandidate.Commit; VerifiedCandidateTree=$badCandidate.Tree; HistoryContractDigest=(Get-FixtureJsonDigest $badContract); HistoryContractGeneratorDigest=$badContract.generator.sha256; HistoryContractCaptureDigest=$null; HistoryContractRuntimeDigest=$null; HistoryContractPatchDigest=$badPatchDigest; HistoryContractMode=$badContract.decision.captureMode; WorktreeCliExecutable=(Join-Path $primaryOutput 'WorktreeCli.exe'); SessionLabel='finalize-fixture'; OwnerToken=$badOwner; AdvancePrimary=$true }
		$badAdvance = Invoke-JsonScriptWithSplat $candidateScript $badAdvanceParameters $scratchBase
		Assert-Outcome $badAdvance "primary-verified-reserved-$($reservedCase.Name)" 2 'blocked' 'history.source-changed'
		Assert-True ($badPrimaryRefBefore -ceq ((@(Invoke-ScratchGit $primary @('rev-parse','refs/heads/main')))[0].Trim()) -and $badPrimaryRefBefore -ceq ((@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim())) "primary-verified-reserved-$($reservedCase.Name) leaves primary ref and checkout unchanged"
		Assert-True ($badPrimaryStatusBefore -ceq ((@(Invoke-ScratchGit $primary @('status','--porcelain=v1','-z','--untracked-files=all')) -join ''))) "primary-verified-reserved-$($reservedCase.Name) leaves primary index and worktree unchanged"
		Assert-True ($badSessionRefBefore -ceq ((@(Invoke-ScratchGit $session @('rev-parse',"refs/heads/$sessionBranch")))[0].Trim()) -and $badSessionStatusBefore -ceq ((@(Invoke-ScratchGit $session @('status','--porcelain=v1','-z','--untracked-files=all')) -join ''))) "primary-verified-reserved-$($reservedCase.Name) leaves session checkout unchanged"
		Invoke-WorktreeCli @('lock','release','--repo',$badPrimaryCommon,'--owner',$badOwner) | Out-Null
	}
	Invoke-ScratchGit $primary @('reset','--hard',$baseline) | Out-Null
	Invoke-ScratchGit $primary @('clean','-fd') | Out-Null
}

# The direct primary-commit route carries a non-C++ candidate through the same source-only path.
$primaryCarryMarker = Join-Path $scratchBase 'primary-carry-forward-generate.marker'
$previousPrimaryCarryMarker = [Environment]::GetEnvironmentVariable('BROKEN_ENGINE_FINALIZE_HISTORY_GENERATE_MARKER')
Remove-Item -LiteralPath $primaryCarryMarker -Force -ErrorAction SilentlyContinue
[Environment]::SetEnvironmentVariable('BROKEN_ENGINE_FINALIZE_HISTORY_GENERATE_MARKER', $primaryCarryMarker)
[IO.File]::WriteAllText((Join-Path $primary 'primary-carry-forward.txt'), 'primary carry-forward source', [Text.UTF8Encoding]::new($false))
$primaryCarryParameters = [ordered]@{ Route='primary-commit'; CurrentWorktree=$primary; PrimaryWorktree=$primary; CurrentBranch='main'; PrimaryBranch='main'; Baseline=$baseline; ExpectedCurrentTip=$baseline; ExpectedPrimaryTip=$baseline; OwnedPaths=@('primary-carry-forward.txt'); CommitMessageFile=$candidateMessage }
$primaryCarryCandidate = Invoke-JsonScriptWithSplat $candidateScript $primaryCarryParameters $scratchBase
Assert-Outcome $primaryCarryCandidate 'primary-carry-forward-candidate' 0 'pass' 'candidate.created'
if ($null -ne $primaryCarryCandidate.Json) {
	$primaryCarryContract = $primaryCarryCandidate.Json.historyContract
	$primaryCarryOwner = [guid]::NewGuid().ToString()
	$primaryCarryCommon = ((@(Invoke-ScratchGit $primary @('rev-parse','--path-format=absolute','--git-common-dir')))[0].Trim())
	Invoke-WorktreeCli @('lock','claim','--repo',$primaryCarryCommon,'--owner',$primaryCarryOwner,'--session','finalize-fixture','--worktree',$primary,'--lease-seconds','3600') | Out-Null
	$primaryCarryParameters.VerifiedCandidateCommit = $primaryCarryCandidate.Json.candidate.commit
	$primaryCarryParameters.VerifiedCandidateTree = $primaryCarryCandidate.Json.candidate.tree
	$primaryCarryParameters = Add-FixtureHistoryParameters $primaryCarryParameters $primaryCarryContract
	$primaryCarryParameters.WorktreeCliExecutable = Join-Path $primaryOutput 'WorktreeCli.exe'
	$primaryCarryParameters.SessionLabel = 'finalize-fixture'
	$primaryCarryParameters.OwnerToken = $primaryCarryOwner
	$primaryCarryParameters.AdvancePrimary = $true
	$primaryCarryJsonBlobBefore = (@(Invoke-ScratchGit $primary @('rev-parse', "$baseline`:$historyJsonFixturePath")))[0].Trim()
	$primaryCarrySvgBlobBefore = (@(Invoke-ScratchGit $primary @('rev-parse', "$baseline`:$historySvgFixturePath")))[0].Trim()
	$primaryCarryAdvance = Invoke-JsonScriptWithSplat $candidateScript $primaryCarryParameters $scratchBase
	Assert-Outcome $primaryCarryAdvance 'primary-carry-forward-advance' 0 'pass' 'candidate.advanced'
	if ($null -ne $primaryCarryAdvance.Json) {
		Assert-True ($primaryCarryAdvance.Json.final.commit -ceq $primaryCarryCandidate.Json.candidate.commit -and $primaryCarryAdvance.Json.final.tree -ceq $primaryCarryCandidate.Json.candidate.tree -and -not $primaryCarryAdvance.Json.final.replacement -and $primaryCarryAdvance.Json.historyUpdate.status -ceq 'skipped') 'primary carry-forward advances the source candidate without replacement or history update'
		Assert-True (-not (Test-Path -LiteralPath $primaryCarryMarker) -and ((@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()) -ceq $primaryCarryCandidate.Json.candidate.commit) 'primary carry-forward skips Generate and advances the source candidate'
		$primaryCarryJsonBlobAfter = (@(Invoke-ScratchGit $primary @('rev-parse', "$($primaryCarryCandidate.Json.candidate.commit):$historyJsonFixturePath")))[0].Trim()
		$primaryCarrySvgBlobAfter = (@(Invoke-ScratchGit $primary @('rev-parse', "$($primaryCarryCandidate.Json.candidate.commit):$historySvgFixturePath")))[0].Trim()
		Assert-True ($primaryCarryJsonBlobBefore -ceq $primaryCarryJsonBlobAfter -and $primaryCarrySvgBlobBefore -ceq $primaryCarrySvgBlobAfter) 'primary carry-forward preserves both reserved history blobs'
	}
	Invoke-WorktreeCli @('lock','release','--repo',$primaryCarryCommon,'--owner',$primaryCarryOwner) | Out-Null
}
[Environment]::SetEnvironmentVariable('BROKEN_ENGINE_FINALIZE_HISTORY_GENERATE_MARKER', $previousPrimaryCarryMarker)
Invoke-ScratchGit $primary @('reset','--hard',$baseline) | Out-Null
Invoke-ScratchGit $primary @('clean','-fd') | Out-Null

# A resumed invocation carries the whole caller-owned landing set, so both routes must
# accept an authorized path this session already committed as a deletion and still stage
# the path that is genuinely dirty.
[IO.File]::WriteAllText((Join-Path $session 'resumed-deleted.txt'), 'resumed deletion source', [Text.UTF8Encoding]::new($false))
Invoke-ScratchGit $session @('add','resumed-deleted.txt') | Out-Null
Invoke-ScratchGit $session @('commit','-m','fixture resumed baseline') | Out-Null
$resumedBaseline = (@(Invoke-ScratchGit $session @('rev-parse','HEAD')))[0].Trim()
Invoke-ScratchGit $session @('rm','resumed-deleted.txt') | Out-Null
Invoke-ScratchGit $session @('commit','-m','fixture resumed deletion') | Out-Null
$resumedTip = (@(Invoke-ScratchGit $session @('rev-parse','HEAD')))[0].Trim()
[IO.File]::WriteAllText((Join-Path $session 'resumed-dirty.txt'), 'resumed dirty', [Text.UTF8Encoding]::new($false))
$run = Invoke-JsonScript $candidateScript @('-Route','session-landing','-CurrentWorktree',$session,'-PrimaryWorktree',$primary,'-CurrentBranch',$sessionBranch,'-PrimaryBranch','main','-Baseline',$resumedBaseline,'-ExpectedCurrentTip',$resumedTip,'-ExpectedPrimaryTip',$baseline,'-OwnedPaths','resumed-deleted.txt,resumed-dirty.txt','-CommitMessageFile',$candidateMessage)
Assert-Outcome $run 'session-candidate-accepts-already-committed-deletion' 0 'pass' 'candidate.created'
if ($null -ne $run.Json) {
	$resumedChangedPaths = @(@(Invoke-ScratchGit $session @('diff-tree','--no-commit-id','--name-only','-r',$resumedTip,$run.Json.candidate.commit)) | ForEach-Object { $_.Trim() } | Where-Object { -not [string]::IsNullOrEmpty($_) })
	Assert-True ($resumedChangedPaths.Count -eq 1 -and $resumedChangedPaths[0] -ceq 'resumed-dirty.txt') 'resumed session candidate changes only the still-dirty authorized path'
}
Invoke-ScratchGit $session @('reset','--hard',$baseline) | Out-Null
Remove-Item -LiteralPath (Join-Path $session 'resumed-dirty.txt') -Force -ErrorAction SilentlyContinue
[IO.File]::WriteAllText((Join-Path $primary 'resumed-primary-deleted.txt'), 'resumed deletion source', [Text.UTF8Encoding]::new($false))
Invoke-ScratchGit $primary @('add','resumed-primary-deleted.txt') | Out-Null
Invoke-ScratchGit $primary @('commit','-m','fixture resumed primary baseline') | Out-Null
$resumedPrimaryBaseline = (@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()
Invoke-ScratchGit $primary @('rm','resumed-primary-deleted.txt') | Out-Null
Invoke-ScratchGit $primary @('commit','-m','fixture resumed primary deletion') | Out-Null
$resumedPrimaryTip = (@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()
[IO.File]::WriteAllText((Join-Path $primary 'resumed-primary-dirty.txt'), 'resumed dirty', [Text.UTF8Encoding]::new($false))
$run = Invoke-JsonScript $candidateScript @('-Route','primary-commit','-CurrentWorktree',$primary,'-PrimaryWorktree',$primary,'-CurrentBranch','main','-PrimaryBranch','main','-Baseline',$resumedPrimaryBaseline,'-ExpectedCurrentTip',$resumedPrimaryTip,'-ExpectedPrimaryTip',$resumedPrimaryTip,'-OwnedPaths','resumed-primary-deleted.txt,resumed-primary-dirty.txt','-CommitMessageFile',$candidateMessage)
Assert-Outcome $run 'primary-candidate-accepts-already-committed-deletion' 0 'pass' 'candidate.created'
if ($null -ne $run.Json) {
	$resumedPrimaryChangedPaths = @(@(Invoke-ScratchGit $primary @('diff-tree','--no-commit-id','--name-only','-r',$resumedPrimaryTip,$run.Json.candidate.commit)) | ForEach-Object { $_.Trim() } | Where-Object { -not [string]::IsNullOrEmpty($_) })
	Assert-True ($resumedPrimaryChangedPaths.Count -eq 1 -and $resumedPrimaryChangedPaths[0] -ceq 'resumed-primary-dirty.txt') 'resumed primary candidate changes only the still-dirty authorized path'
}
Invoke-ScratchGit $primary @('reset','--hard',$baseline) | Out-Null
Remove-Item -LiteralPath (Join-Path $primary 'resumed-primary-dirty.txt') -Force -ErrorAction SilentlyContinue

# Preview is deterministic even when executable discovery differs. Explicit launch
# uses a bounded command stub, so the actual Start-Process path is exercised safely.
$canonicalManualCommand = "& 'C:\Program Files\SmartGit\bin\smartgit.exe' '--log' '$primary' '--anchor-commit=$baseline'"
$run = Invoke-JsonScript $approvalReviewScript @('-PrimaryWorktree',$primary,'-ApprovedTip',$baseline)
Assert-Outcome $run 'approval-review-preview' 0 'preview' 'review.preview'
if ($null -ne $run.Json) {
	Assert-True ($run.Json.manualCommand -ceq $canonicalManualCommand) 'approval-review-preview returns exact canonical manual command'
	Assert-True ($null -eq $run.Json.processId) 'approval-review-preview starts no process'
}
$reviewExecutable = Join-Path $primary 'fixture-smartgit.cmd'
$reviewArguments = Join-Path $primary 'fixture-smartgit-arguments.txt'
[IO.File]::WriteAllText($reviewExecutable, "@echo off`r`necho %* > `"$reviewArguments`"`r`n", [Text.UTF8Encoding]::new($false))
$run = Invoke-JsonScript $approvalReviewScript @('-PrimaryWorktree',$primary,'-ApprovedTip',$baseline,'-LaunchSmartGit','-FixtureSmartGitExecutable',$reviewExecutable)
Assert-Outcome $run 'approval-review-explicit-launch' 0 'opened' 'ok'
if ($null -ne $run.Json) {
	Assert-True ($null -ne $run.Json.processId) 'approval-review fixture launch reaches Start-Process'
	$deadline = [DateTime]::UtcNow.AddSeconds(5)
	while (-not (Test-Path -LiteralPath $reviewArguments) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 25 }
	Assert-True (Test-Path -LiteralPath $reviewArguments) 'approval-review fixture launch completes boundedly'
	if (Test-Path -LiteralPath $reviewArguments) {
		$capturedReviewArguments = [IO.File]::ReadAllText($reviewArguments).Trim()
		$expectedReviewArguments = ('"--log" "' + $primary + '" "--anchor-commit=' + $baseline + '"')
		Assert-True ($capturedReviewArguments -ceq $expectedReviewArguments) 'approval-review fixture receives exact forwarded arguments'
	}
}
Remove-Item -LiteralPath $reviewExecutable -Force
Remove-Item -LiteralPath $reviewArguments -Force -ErrorAction SilentlyContinue

$commonDirectory = ((@(Invoke-ScratchGit $primary @('rev-parse', '--path-format=absolute', '--git-common-dir')))[0].Trim())

# Reconciliation uses the claim script; landing calls the same common helper.
$reconcileOwner = [guid]::NewGuid().ToString()
$lockClaimArguments = @('-WorktreeCliExecutable', (Join-Path $primaryOutput 'WorktreeCli.exe'), '-GitCommonDirectory', $commonDirectory, '-SessionLabel', 'finalize-fixture', '-Worktree', $session, '-LandingOwner', $reconcileOwner, '-LeaseSeconds', '60')
$run = Invoke-JsonScript $lockClaimScript $lockClaimArguments
Assert-Outcome $run 'reconcile-lock-claim' 0 'pass' 'ok'
if ($null -ne $run.Json) {
	Assert-True ($run.Json.owner -ceq $reconcileOwner) 'reconcile lock preserves supplied owner'
	Assert-True ($null -eq $run.Json.blocker) 'reconcile lock success has no blocker disposition'
}
$lockReleaseArguments = @('-WorktreeCliExecutable', (Join-Path $primaryOutput 'WorktreeCli.exe'), '-GitCommonDirectory', $commonDirectory, '-SessionLabel', 'finalize-fixture', '-Worktree', $session)
$run = Invoke-JsonScript $lockClaimScript ($lockReleaseArguments + @('-LandingOwner', $reconcileOwner, '-Release'))
Assert-Outcome $run 'reconcile-lock-release' 0 'pass' 'ok'
if ($null -ne $run.Json) {
	Assert-ExactProperties $run.Json @('schemaVersion','status','code','message','owner','lock','attempts','disposition','requiresUserAuthority','retryAfterMilliseconds','blocker') 'reconcile-lock-release'
	Assert-True ($null -eq $run.Json.blocker) 'reconcile lock release success has no blocker disposition'
}
$releasedStatus = (@(Invoke-WorktreeCli @('lock', 'status', '--repo', $commonDirectory) 2) -join '')
Assert-True ((($releasedStatus | ConvertFrom-Json -Depth 16).held) -eq $false) 'script release leaves the landing lock not held'
$run = Invoke-JsonScript $lockClaimScript ($lockReleaseArguments + @('-LandingOwner', $reconcileOwner, '-Release'))
Assert-Outcome $run 'reconcile-lock-release-idempotent' 0 'pass' 'ok'

$run = Invoke-JsonScript $lockClaimScript ($lockReleaseArguments + @('-Release'))
Assert-Outcome $run 'reconcile-lock-release-blank-owner' 1 'error' 'landing-lock.release-owner-required'
if ($null -ne $run.Json) {
	Assert-True ($run.Json.attempts -eq 0) 'blank-owner release reports no WorktreeCli attempts'
}
$releasedStatus = (@(Invoke-WorktreeCli @('lock', 'status', '--repo', $commonDirectory) 2) -join '')
Assert-True ((($releasedStatus | ConvertFrom-Json -Depth 16).held) -eq $false) 'blank-owner release mints no token and creates no lease'

$run = Invoke-JsonScript $lockClaimScript ($lockReleaseArguments + @('-LandingOwner', 'not-a-canonical-guid'))
Assert-Outcome $run 'reconcile-lock-claim-invalid-owner' 1 'error' 'landing-lock.owner-token-invalid'
if ($null -ne $run.Json) {
	Assert-True ($run.Json.attempts -eq 0) 'invalid-owner claim reports no WorktreeCli attempts'
}
$releasedStatus = (@(Invoke-WorktreeCli @('lock', 'status', '--repo', $commonDirectory) 2) -join '')
Assert-True ((($releasedStatus | ConvertFrom-Json -Depth 16).held) -eq $false) 'invalid-owner claim creates no lease'

$foreignLeaseOwner = [guid]::NewGuid().ToString()
Invoke-WorktreeCli @('lock', 'claim', '--repo', $commonDirectory, '--owner', $foreignLeaseOwner, '--session', 'foreign-fixture', '--worktree', $session, '--lease-seconds', '60') | Out-Null
$run = Invoke-JsonScript $lockClaimScript (@('-WorktreeCliExecutable', (Join-Path $primaryOutput 'WorktreeCli.exe'), '-GitCommonDirectory', $commonDirectory, '-SessionLabel', 'finalize-fixture', '-Worktree', $session, '-LandingOwner', ([guid]::NewGuid().ToString()), '-LeaseSeconds', '60', '-WaitSeconds', '1', '-PollMilliseconds', '50'))
Assert-Outcome $run 'reconcile-lock-live-contention' 2 'blocked' 'landing-lock.retryable-wait'
if ($null -ne $run.Json) {
	Assert-True ($run.Json.disposition -ceq 'retryable-wait') 'live foreign lease exposes top-level retryable disposition'
	Assert-True ($run.Json.blocker.disposition -ceq 'retryable-wait') 'live foreign lease is retryable'
	Assert-True (-not $run.Json.blocker.requiresUserAuthority) 'live foreign lease needs no authority'
}
$run = Invoke-JsonScript $lockClaimScript ($lockReleaseArguments + @('-LandingOwner', ([guid]::NewGuid().ToString()), '-Release'))
Assert-Outcome $run 'reconcile-lock-release-denied' 1 'error' 'landing-lock.release-denied'
if ($null -ne $run.Json) {
	Assert-ExactProperties $run.Json @('schemaVersion','status','code','message','owner','lock','attempts','disposition','requiresUserAuthority','retryAfterMilliseconds','blocker') 'reconcile-lock-release-denied'
	Assert-True ($run.Json.lock.owner -ceq $foreignLeaseOwner) 'denied release reports the live foreign lease owner'
}
Invoke-WorktreeCli @('lock', 'release', '--repo', $commonDirectory, '--owner', $foreignLeaseOwner) | Out-Null

$sameOwner = [guid]::NewGuid().ToString()
Invoke-WorktreeCli @('lock', 'claim', '--repo', $commonDirectory, '--owner', $sameOwner, '--session', 'wrong-session', '--worktree', $session, '--lease-seconds', '60') | Out-Null
$run = Invoke-JsonScript $lockClaimScript (@('-WorktreeCliExecutable', (Join-Path $primaryOutput 'WorktreeCli.exe'), '-GitCommonDirectory', $commonDirectory, '-SessionLabel', 'finalize-fixture', '-Worktree', $session, '-LandingOwner', $sameOwner, '-LeaseSeconds', '60', '-WaitSeconds', '1', '-PollMilliseconds', '50'))
Assert-Outcome $run 'reconcile-lock-same-owner-wrong-session' 2 'blocked' 'landing-lock.retryable-wait'
Invoke-WorktreeCli @('lock', 'release', '--repo', $commonDirectory, '--owner', $sameOwner) | Out-Null

$sameOwner = [guid]::NewGuid().ToString()
Invoke-WorktreeCli @('lock', 'claim', '--repo', $commonDirectory, '--owner', $sameOwner, '--session', 'finalize-fixture', '--worktree', $primary, '--lease-seconds', '60') | Out-Null
$run = Invoke-JsonScript $lockClaimScript (@('-WorktreeCliExecutable', (Join-Path $primaryOutput 'WorktreeCli.exe'), '-GitCommonDirectory', $commonDirectory, '-SessionLabel', 'finalize-fixture', '-Worktree', $session, '-LandingOwner', $sameOwner, '-LeaseSeconds', '60', '-WaitSeconds', '1', '-PollMilliseconds', '50'))
Assert-Outcome $run 'reconcile-lock-same-owner-wrong-worktree' 2 'blocked' 'landing-lock.retryable-wait'
Invoke-WorktreeCli @('lock', 'release', '--repo', $commonDirectory, '--owner', $sameOwner) | Out-Null

$expiredLeaseOwner = [guid]::NewGuid().ToString()
Invoke-WorktreeCli @('lock', 'claim', '--repo', $commonDirectory, '--owner', $expiredLeaseOwner, '--session', 'expired-fixture', '--worktree', $session, '--lease-seconds', '60') | Out-Null
Set-ExpiredLandingLease $localAppData $expiredLeaseOwner
$recoveredOwner = [guid]::NewGuid().ToString()
$run = Invoke-JsonScript $lockClaimScript (@('-WorktreeCliExecutable', (Join-Path $primaryOutput 'WorktreeCli.exe'), '-GitCommonDirectory', $commonDirectory, '-SessionLabel', 'finalize-fixture', '-Worktree', $session, '-LandingOwner', $recoveredOwner, '-LeaseSeconds', '60'))
Assert-Outcome $run 'reconcile-lock-expired-recovery' 0 'pass' 'ok'
if ($null -ne $run.Json) { Assert-True ($run.Json.lock.owner -ceq $recoveredOwner) 'expired lease recovers through exact owner compare-and-swap' }
Invoke-WorktreeCli @('lock', 'release', '--repo', $commonDirectory, '--owner', $recoveredOwner) | Out-Null

$unverifiableLeaseOwner = [guid]::NewGuid().ToString()
Invoke-WorktreeCli @('lock', 'claim', '--repo', $commonDirectory, '--owner', $unverifiableLeaseOwner, '--session', 'unverifiable-fixture', '--worktree', $session, '--lease-seconds', '60') | Out-Null
Set-UnverifiableLandingLease $localAppData $unverifiableLeaseOwner
$run = Invoke-JsonScript $lockClaimScript (@('-WorktreeCliExecutable', (Join-Path $primaryOutput 'WorktreeCli.exe'), '-GitCommonDirectory', $commonDirectory, '-SessionLabel', 'finalize-fixture', '-Worktree', $session, '-LandingOwner', ([guid]::NewGuid().ToString()), '-LeaseSeconds', '60'))
Assert-Outcome $run 'reconcile-lock-unverifiable' 2 'blocked' 'landing-lock.unverifiable'
if ($null -ne $run.Json) {
	Assert-True ($run.Json.disposition -ceq 'authority-required') 'unverifiable lease exposes top-level authority disposition'
	Assert-True ($run.Json.blocker.disposition -ceq 'authority-required') 'unverifiable lease needs authority'
	Assert-True $run.Json.blocker.requiresUserAuthority 'unverifiable lease explicitly requires authority'
}
Invoke-WorktreeCli @('lock', 'release', '--repo', $commonDirectory, '--owner', $unverifiableLeaseOwner) | Out-Null

# One claim-free candidate carried through approval preparation, the guarded landing
# failures, the landing itself, and idempotent post-advance recovery.
Invoke-ScratchGit $session @('reset','--hard',$baseline) | Out-Null
Invoke-ScratchGit $session @('clean','-fd') | Out-Null
[IO.File]::WriteAllText((Join-Path $session 'landing-change.cpp'), 'landing change', [Text.UTF8Encoding]::new($false))
$run = Invoke-JsonScript $candidateScript @('-Route','session-landing','-CurrentWorktree',$session,'-PrimaryWorktree',$primary,'-CurrentBranch',$sessionBranch,'-PrimaryBranch','main','-Baseline',$baseline,'-ExpectedCurrentTip',$baseline,'-ExpectedPrimaryTip',$baseline,'-OwnedPaths','landing-change.cpp','-CommitMessageFile',$candidateMessage)
Assert-Outcome $run 'landing-candidate-created' 0 'pass' 'candidate.created'
if ($null -ne $run.Json) {
	$approvalParameters = [ordered]@{ CurrentWorktree=$session; PrimaryWorktree=$primary; CurrentBranch=$sessionBranch; PrimaryBranch='main'; ExpectedCurrentTip=$run.Json.candidate.commit; ExpectedPrimaryTip=$baseline; VerifiedCandidateCommit=$run.Json.candidate.commit; VerifiedCandidateTree=$run.Json.candidate.tree }
	$approvalInvalidIdentityParameters = [ordered]@{}
	foreach ($parameter in $approvalParameters.GetEnumerator()) { $approvalInvalidIdentityParameters[$parameter.Key] = $parameter.Value }
	$approvalInvalidIdentityParameters.ExpectedCurrentTip = 'f' * 1500
	$approvalInvalidIdentity = Invoke-JsonScriptWithSplat $approvalPreparationScript $approvalInvalidIdentityParameters $scratchBase
	Assert-Outcome $approvalInvalidIdentity 'approval-preparation-invalid-oversized-identity' 1 'error' 'input.invalid'
	Assert-True ($null -eq $approvalInvalidIdentity.Json.session.originalTip -and $approvalInvalidIdentity.Json.session.primaryTip -ceq $baseline -and $null -eq $approvalInvalidIdentity.Json.candidate.commit -and $null -eq $approvalInvalidIdentity.Json.candidate.parent) 'approval preparation nulls invalid oversized identities and preserves valid exact identities'
	$approvalParameters.FixtureFailure = 'bounded-diagnostic'
	$approvalFailure = Invoke-JsonScriptWithSplat $approvalPreparationScript $approvalParameters $scratchBase
	Assert-True ($approvalFailure.ExitCode -eq 1 -and $approvalFailure.Json.status -ceq 'error') 'approval preparation bounded failure emits error'
	Assert-ExactProperties $approvalFailure.Json @('schemaVersion','status','code','message','messageLength','messageTruncated','session','candidate','squash','sanity','verifiedCandidate','historyContract','diagnostics') 'approval preparation failure'
	Assert-ExactProperties $approvalFailure.Json.diagnostics @('totalCount','items','truncated','selector','requery') 'approval preparation failure diagnostics'
	Assert-ExactProperties $approvalFailure.Json.diagnostics.items[0] @('source','code','codeLength','codeTruncated','path','pathLength','pathTruncated','message','messageLength','messageTruncated') 'approval preparation failure diagnostic item'
	Assert-True ($approvalFailure.Json.code.Length -eq 128 -and $approvalFailure.Json.message.Length -eq 512 -and $approvalFailure.Json.messageLength -eq 600 -and $approvalFailure.Json.messageTruncated -and $approvalFailure.Json.diagnostics.requery -ceq 'Invoke-FinalizeApprovalPreparation') 'approval preparation failure is bounded with canonical requery'
	$approvalParameters.FixtureFailure = 'none'
	$approval = Invoke-JsonScriptWithSplat $approvalPreparationScript $approvalParameters $scratchBase
	Assert-Outcome $approval 'approval-preparation-normal-success' 0 'pass' 'ok'
	Assert-ExactProperties $approval.Json @('schemaVersion','status','code','message','messageLength','messageTruncated','session','candidate','squash','sanity','verifiedCandidate','historyContract','diagnostics') 'approval preparation success'
	Assert-ExactProperties $approval.Json.session @('originalTip','currentTip','primaryTip') 'approval preparation session'
	Assert-ExactProperties $approval.Json.candidate @('commit','tree','parent') 'approval preparation candidate'
	Assert-ExactProperties $approval.Json.squash @('disposition','commitCount','refUpdated','rollback') 'approval preparation squash'
	Assert-ExactProperties $approval.Json.sanity @('initial','final') 'approval preparation sanity'
	Assert-ExactProperties $approval.Json.verifiedCandidate @('supplied','matched') 'approval preparation verified candidate'
	Assert-ExactProperties $approval.Json.historyContract @('receipt','digest','generatorDigest','captureDigest','runtimeDigest','patchDigest','mode','patch','coverage') 'approval preparation history contract'
	Assert-True ($approval.Json.schemaVersion -ceq 'broken-engine-finalize-approval-preparation/v3' -and $approval.Json.candidate.commit -ceq $run.Json.candidate.commit -and $approval.Json.candidate.tree -ceq $run.Json.candidate.tree -and $approval.Json.candidate.parent -ceq $baseline -and $approval.Json.sanity.initial -ceq 'pass' -and $approval.Json.sanity.final -ceq 'pass' -and $approval.Json.verifiedCandidate.supplied -and $approval.Json.verifiedCandidate.matched -and $approval.Json.historyContract.receipt.schemaVersion -ceq 'broken-engine-code-quality-history-contract/v1') 'approval preparation success projects exact identities and pass states'
	Assert-True ($approval.Json.PSObject.Properties.Name -cnotcontains 'tips' -and $approval.Json.PSObject.Properties.Name -cnotcontains 'identities') 'approval preparation hides raw identities'
	# The message override has to rebuild even this single-commit range, so the range is
	# restored afterwards for the landing scenarios that follow.
	$overrideMessagePath = Join-Path $scratchBase 'approval-override-message.txt'
	[IO.File]::WriteAllText($overrideMessagePath, "fixture override message`n", [Text.UTF8Encoding]::new($false))
	$approvalOverrideParameters = [ordered]@{}
	foreach ($parameter in $approvalParameters.GetEnumerator()) { $approvalOverrideParameters[$parameter.Key] = $parameter.Value }
	$approvalOverrideParameters.CommitMessageFile = $overrideMessagePath
	$approvalOverride = Invoke-JsonScriptWithSplat $approvalPreparationScript $approvalOverrideParameters $scratchBase
	Assert-Outcome $approvalOverride 'approval-preparation-message-override' 0 'pass' 'ok'
	if ($null -ne $approvalOverride.Json) {
		Assert-True ($approvalOverride.Json.squash.disposition -ceq 'message-replaced' -and $approvalOverride.Json.squash.commitCount -eq 1 -and $approvalOverride.Json.squash.refUpdated) 'single-commit message override replaces the commit'
		Assert-True ($approvalOverride.Json.candidate.commit -cne $run.Json.candidate.commit -and $approvalOverride.Json.candidate.tree -ceq $run.Json.candidate.tree -and $approvalOverride.Json.candidate.parent -ceq $baseline) 'message override keeps the verified tree on a new commit'
		$overrideCommitMessage = ((@(Invoke-ScratchGit $session @('show','--no-show-signature','-s','--format=%B',$approvalOverride.Json.candidate.commit)) -join "`n")).TrimEnd("`r","`n")
		Assert-True ($overrideCommitMessage -ceq ([IO.File]::ReadAllText($overrideMessagePath, [Text.UTF8Encoding]::new($false,$true)).TrimEnd("`r","`n"))) 'message override commit carries the supplied message'
	}
	Invoke-ScratchGit $session @('reset','--hard',$run.Json.candidate.commit) | Out-Null
	$landingParameters = [ordered]@{ CurrentWorktree=$session; PrimaryWorktree=$primary; CurrentBranch=$sessionBranch; PrimaryBranch='main'; ExpectedCurrentTip=$run.Json.candidate.commit; ExpectedPrimaryTip=$baseline; SessionLabel='finalize-fixture'; ApprovedSessionCommit=$run.Json.candidate.commit; ApprovedCandidateTree=$run.Json.candidate.tree; HistoryContractDigest=$approval.Json.historyContract.digest; HistoryContractGeneratorDigest=$approval.Json.historyContract.generatorDigest; HistoryContractCaptureDigest=$approval.Json.historyContract.captureDigest; HistoryContractRuntimeDigest=$approval.Json.historyContract.runtimeDigest; HistoryContractPatchDigest=$approval.Json.historyContract.patchDigest; HistoryContractMode=$approval.Json.historyContract.mode }
	[Environment]::SetEnvironmentVariable('BROKEN_ENGINE_FINALIZE_HISTORY_RECEIPT', 'update-row-unknown')
	$malformedUpdate = Invoke-JsonScriptWithSplat $landingScript $landingParameters $scratchBase
	Assert-Outcome $malformedUpdate 'landing-rejects-malformed-nested-update-receipt' 2 'blocked' 'history.update-invalid'
	[Environment]::SetEnvironmentVariable('BROKEN_ENGINE_FINALIZE_HISTORY_RECEIPT', $previousHistoryReceiptMode)
	$landingInvalidIdentityParameters = [ordered]@{}
	foreach ($parameter in $landingParameters.GetEnumerator()) { $landingInvalidIdentityParameters[$parameter.Key] = $parameter.Value }
	$landingInvalidIdentityParameters.ApprovedSessionCommit = 'f' * 1500
	$landingInvalidIdentity = Invoke-JsonScriptWithSplat $landingScript $landingInvalidIdentityParameters $scratchBase
	Assert-Outcome $landingInvalidIdentity 'landing-invalid-oversized-identity' 1 'error' 'input.commit-invalid'
	Assert-True ($null -eq $landingInvalidIdentity.Json.candidate.commit -and $landingInvalidIdentity.Json.candidate.tree -ceq $run.Json.candidate.tree) 'landing nulls invalid oversized identities and preserves valid exact identities'
	$landingParameters.FixtureFailure = 'bounded-diagnostic'
	$landingBoundedFailure = Invoke-JsonScriptWithSplat $landingScript $landingParameters $scratchBase
	Assert-True ($landingBoundedFailure.ExitCode -eq 1 -and $landingBoundedFailure.Json.status -ceq 'error') 'landing bounded failure emits error'
	Assert-ExactProperties $landingBoundedFailure.Json @('schemaVersion','status','code','message','messageLength','messageTruncated','primaryAdvanced','candidate','landed','approvedSource','rebasedSource','historyContract','historyUpdate','final','planClaim','lock','cleanup','disposition','requiresUserAuthority','retryAfterMilliseconds','diagnostics','residuals') 'landing failure'
	Assert-ExactProperties $landingBoundedFailure.Json.diagnostics @('totalCount','items','truncated','selector','requery') 'landing failure diagnostics'
	Assert-ExactProperties $landingBoundedFailure.Json.diagnostics.items[0] @('source','code','codeLength','codeTruncated','path','pathLength','pathTruncated','message','messageLength','messageTruncated') 'landing failure diagnostic item'
	Assert-True ($landingBoundedFailure.Json.code.Length -eq 128 -and $landingBoundedFailure.Json.message.Length -eq 512 -and $landingBoundedFailure.Json.messageLength -eq 600 -and $landingBoundedFailure.Json.messageTruncated) 'landing failure top-level text is bounded'
	# A forced compare-and-swap failure is the one stale-primary signal this scenario can raise: primary
	# never moved, so the internal rebase is a no-op and the second forced failure exhausts the single
	# retry instead of reporting the lost swap.
	$landingParameters.FixtureFailure = 'compare-and-swap'
	$landingCasMismatch = Invoke-JsonScriptWithSplat $landingScript $landingParameters $scratchBase
	Assert-Outcome $landingCasMismatch 'landing-retry-exhausted' 2 'blocked' 'landing.retry-exhausted'
	Assert-True ($landingCasMismatch.Json.disposition -ceq 'retryable-wait' -and $landingCasMismatch.Json.retryAfterMilliseconds -eq 500 -and -not $landingCasMismatch.Json.primaryAdvanced) 'landing retry exhaustion is retryable and advanced nothing'
	Assert-True ($landingCasMismatch.Json.landed.rebaseAttempts -eq 1 -and $null -eq $landingCasMismatch.Json.landed.commit) 'landing retry exhaustion reports its one rebase and lands nothing'
	Assert-True ($baseline -ceq ((@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim())) 'landing retry exhaustion leaves primary ref unchanged'
	Assert-True ($run.Json.candidate.commit -ceq ((@(Invoke-ScratchGit $session @('rev-parse',"refs/heads/$sessionBranch")))[0].Trim())) 'landing retry exhaustion leaves the confirmed session commit in place'
	Assert-True ([string]::IsNullOrWhiteSpace((@(Invoke-ScratchGit $primary @('status','--porcelain=v1','-z','--untracked-files=all')) -join ''))) 'landing retry exhaustion preserves primary checkout'
	$landingParameters.FixtureFailure = 'post-reset'
	$landingRollback = Invoke-JsonScriptWithSplat $landingScript $landingParameters $scratchBase
	Assert-Outcome $landingRollback 'landing-post-reset-rollback' 2 'blocked' 'candidate.postcondition-failed'
	Assert-True ($baseline -ceq ((@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim())) 'landing post-reset rollback restores primary ref and checkout head'
	Assert-True ([string]::IsNullOrWhiteSpace((@(Invoke-ScratchGit $primary @('status','--porcelain=v1','-z','--untracked-files=all')) -join ''))) 'landing post-reset rollback restores primary index and worktree'
	$landingParameters.FixtureFailure = 'none'
	$landing = Invoke-JsonScriptWithSplat $landingScript $landingParameters $scratchBase
	Assert-Outcome $landing 'exact-candidate-landing-success' 0 'landed' 'ok'
	Assert-ExactProperties $landing.Json @('schemaVersion','status','code','message','messageLength','messageTruncated','primaryAdvanced','candidate','landed','approvedSource','rebasedSource','historyContract','historyUpdate','final','planClaim','lock','cleanup','disposition','requiresUserAuthority','retryAfterMilliseconds','diagnostics','residuals') 'landing success'
	Assert-ExactProperties $landing.Json.candidate @('commit','tree','treeVerified') 'landing candidate'
	Assert-ExactProperties $landing.Json.landed @('commit','tree','rebaseAttempts') 'landing landed'
	Assert-ExactProperties $landing.Json.approvedSource @('commit','tree','parent','patch','metadata') 'landing approved source'
	Assert-ExactProperties $landing.Json.rebasedSource @('commit','tree','parent','patch') 'landing rebased source'
	Assert-ExactProperties $landing.Json.final @('commit','tree','parent','replacement') 'landing final'
	Assert-ExactProperties $landing.Json.planClaim @('requested','released') 'landing Plan claim'
	Assert-ExactProperties $landing.Json.lock @('claimed','released','claimCode','disposition','requiresUserAuthority','retryAfterMilliseconds','attempts') 'landing lock'
	Assert-ExactProperties $landing.Json.cleanup @('worktreesClear','problems') 'landing cleanup'
	Assert-ExactProperties $landing.Json.cleanup.problems @('totalCount','items','truncated','selector','requery') 'landing cleanup problems'
	Assert-ExactProperties $landing.Json.residuals @('totalCount','items','truncated','selector','requery') 'landing residuals'
	Assert-True ($landing.Json.schemaVersion -ceq 'broken-engine-finalize-landing/v4' -and $landing.Json.candidate.commit -ceq $run.Json.candidate.commit -and $landing.Json.candidate.tree -ceq $run.Json.candidate.tree -and $landing.Json.candidate.treeVerified -and $landing.Json.final.replacement -and $landing.Json.historyUpdate.status -ceq 'pass' -and $landing.Json.lock.claimed -and $landing.Json.lock.released -and $landing.Json.cleanup.worktreesClear) 'landing success projects exact candidate, lock, history, and cleanup proof'
	Assert-True ($landing.Json.landed.commit -ne $run.Json.candidate.commit -and $landing.Json.landed.commit -ceq $landing.Json.final.commit -and $landing.Json.landed.tree -ceq $landing.Json.final.tree -and $landing.Json.landed.rebaseAttempts -eq 0) 'a mint-fresh landing lands the deterministic history replacement with no rebase'
	Assert-True (-not $landing.Json.planClaim.requested -and -not $landing.Json.planClaim.released) 'a claim-free landing touches no Plan claim'
	Assert-True ($landing.Json.PSObject.Properties.Name -cnotcontains 'identities' -and $landing.Json.PSObject.Properties.Name -cnotcontains 'tips' -and $landing.Json.PSObject.Properties.Name -cnotcontains 'locks' -and $landing.Json.PSObject.Properties.Name -cnotcontains 'blocker') 'landing hides checkout, lock-owner, and raw blocker objects'
	Assert-True ($landing.Json.primaryAdvanced -and ((@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()) -ceq $landing.Json.final.commit) 'landing primary ref equals the deterministic final commit exactly'
	$landingPublicText = $landing.Text
	Assert-True ($landingPublicText -notmatch 'tempPath|FinalizeHistory-|[A-Za-z]:\\|(?<![0-9a-f])[0-9a-f]{32}(?![0-9a-f])') 'landing v4 result exposes no absolute or volatile Temp identity'
	# Recovery keeps the original approved primary ancestor; the replacement commit's parent is
	# that source candidate's original parent, not the source candidate itself.
	$landingParameters.ExpectedPrimaryTip = $baseline
	$recoveryOwner = [guid]::NewGuid().ToString()
	Invoke-WorktreeCli @('lock', 'claim', '--repo', $commonDirectory, '--owner', $recoveryOwner, '--session', 'finalize-fixture/landing', '--worktree', $session, '--lease-seconds', '3600') | Out-Null
	$recovery = Invoke-JsonScriptWithSplat $landingScript $landingParameters $scratchBase
	Assert-Outcome $recovery 'exact-candidate-post-advance-recovery' 0 'landed' 'ok'
	if ($null -ne $recovery.Json) {
		Assert-True ($recovery.Json.lock.claimed -and $recovery.Json.lock.released) 'omitted-token recovery adopts and releases the matching retained claim'
	}
	# Deterministic crash equivalent: leave primary's ref advanced while its checkout still points at
	# the old tree, then let recovery acquire/adopt the lease, reset primary, and finish normally.
	$crashTip = (@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()
	Invoke-ScratchGit $session @('reset','--hard',$crashTip) | Out-Null
	Invoke-ScratchGit $session @('clean','-fd') | Out-Null
	[IO.File]::WriteAllText((Join-Path $session 'crash-after-ref.txt'), 'crash after ref update', [Text.UTF8Encoding]::new($false))
	$crashCandidate = Invoke-JsonScript $candidateScript @('-Route','session-landing','-CurrentWorktree',$session,'-PrimaryWorktree',$primary,'-CurrentBranch',$sessionBranch,'-PrimaryBranch','main','-Baseline',$crashTip,'-ExpectedCurrentTip',$crashTip,'-ExpectedPrimaryTip',$crashTip,'-OwnedPaths','crash-after-ref.txt','-CommitMessageFile',$candidateMessage)
	Assert-Outcome $crashCandidate 'crash-after-ref-candidate' 0 'pass' 'candidate.created'
	if ($null -ne $crashCandidate.Json) {
		$crashParameters = [ordered]@{ CurrentWorktree=$session; PrimaryWorktree=$primary; CurrentBranch=$sessionBranch; PrimaryBranch='main'; ExpectedCurrentTip=$crashCandidate.Json.candidate.commit; ExpectedPrimaryTip=$crashTip; SessionLabel='finalize-fixture'; ApprovedSessionCommit=$crashCandidate.Json.candidate.commit; ApprovedCandidateTree=$crashCandidate.Json.candidate.tree; FixtureFailure='post-update-ref' }
		$crash = Invoke-JsonScriptWithSplat $landingScript $crashParameters $scratchBase
		Assert-Outcome $crash 'crash-after-ref-update' 1 'error' 'fixture.crash-after-update-ref'
		$crashPrimaryRef = (@(Invoke-ScratchGit $primary @('rev-parse','refs/heads/main')))[0].Trim(); $crashPrimaryHead = (@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()
		Assert-True ($crashPrimaryRef -cne $crashTip) "crash equivalent advances the primary ref (ref=$crashPrimaryRef old=$crashTip)"
		$crashPrimaryStatus = (@(Invoke-ScratchGit $primary @('status','--porcelain=v1','-z','--untracked-files=all')) -join '')
		Assert-True ($crashPrimaryStatus.Length -gt 0) "crash equivalent leaves the primary checkout/index behind its ref (head=$crashPrimaryHead ref=$crashPrimaryRef)"
		$crashParameters.FixtureFailure = 'none'
		$crashRecovery = Invoke-JsonScriptWithSplat $landingScript $crashParameters $scratchBase
		Assert-Outcome $crashRecovery 'crash-after-ref-recovery' 0 'landed' 'ok'
		if ($null -ne $crashRecovery.Json) { Assert-True ($crashRecovery.Json.lock.released -and $crashRecovery.Json.primaryAdvanced -and ((@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()) -ceq $crashRecovery.Json.final.commit) 'crash recovery resets and verifies primary before releasing the lease' }
	}
}

function New-CarryForwardCandidate([string] $FileName, [string] $Content, [string] $Case) {
	$tip = (@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()
	Invoke-ScratchGit $session @('reset','--hard',$tip) | Out-Null
	Invoke-ScratchGit $session @('clean','-fd') | Out-Null
	[IO.File]::WriteAllText((Join-Path $session $FileName), $Content, [Text.UTF8Encoding]::new($false))
	$candidate = Invoke-JsonScript $candidateScript @('-Route','session-landing','-CurrentWorktree',$session,'-PrimaryWorktree',$primary,'-CurrentBranch',$sessionBranch,'-PrimaryBranch','main','-Baseline',$tip,'-ExpectedCurrentTip',$tip,'-ExpectedPrimaryTip',$tip,'-OwnedPaths',$FileName,'-CommitMessageFile',$candidateMessage)
	Assert-Outcome $candidate "$Case-candidate" 0 'pass' 'candidate.created'
	return [pscustomobject]@{ PrimaryTip = $tip; Commit = $candidate.Json.candidate.commit; Tree = $candidate.Json.candidate.tree; Contract = Get-FixtureHistoryContract $tip $candidate.Json.candidate.commit }
}

# Active-overlay recovery must not let a foreign ref advance between its post-lock checkout
# precheck and the ref-neutral reconciliation overwrite that newer ref.
$activeRecoveryRaceCandidate = New-CarryForwardCandidate 'active-recovery-primary-race.cpp' 'active recovery primary race source' 'active-recovery-primary-race'
$activeRecoveryRaceParameters = [ordered]@{ CurrentWorktree=$session; PrimaryWorktree=$primary; CurrentBranch=$sessionBranch; PrimaryBranch='main'; ExpectedCurrentTip=$activeRecoveryRaceCandidate.Commit; ExpectedPrimaryTip=$activeRecoveryRaceCandidate.PrimaryTip; SessionLabel='finalize-fixture'; ApprovedSessionCommit=$activeRecoveryRaceCandidate.Commit; ApprovedCandidateTree=$activeRecoveryRaceCandidate.Tree; FixtureFailure='post-update-ref' }
$activeRecoveryRaceParameters = Add-FixtureHistoryParameters $activeRecoveryRaceParameters $activeRecoveryRaceCandidate.Contract
$activeRecoveryRaceCrash = Invoke-JsonScriptWithSplat $landingScript $activeRecoveryRaceParameters $scratchBase
Assert-Outcome $activeRecoveryRaceCrash 'active-recovery-primary-race-crash' 1 'error' 'fixture.crash-after-update-ref'
if ($null -ne $activeRecoveryRaceCrash.Json) {
	$activeRecoveryRaceRefBefore = (@(Invoke-ScratchGit $primary @('rev-parse','refs/heads/main')))[0].Trim()
	$activeRecoveryRaceStatusBefore = (@(Invoke-ScratchGit $primary @('status','--porcelain=v1','-z','--untracked-files=all')) -join '')
	$activeRecoveryRaceSessionRefBefore = (@(Invoke-ScratchGit $session @('rev-parse',"refs/heads/$sessionBranch")))[0].Trim()
	$activeRecoveryRaceSessionHeadBefore = (@(Invoke-ScratchGit $session @('rev-parse','HEAD')))[0].Trim()
	$activeRecoveryRaceSessionStatusBefore = (@(Invoke-ScratchGit $session @('status','--porcelain=v1','-z','--untracked-files=all')) -join '')
	$activeRecoveryRaceParameters.FixtureFailure = 'history-recovery-active-primary-race'
	$activeRecoveryRaceRecovery = Invoke-JsonScriptWithSplat $landingScript $activeRecoveryRaceParameters $scratchBase
	Assert-Outcome $activeRecoveryRaceRecovery 'active-recovery-primary-race-blocked' 2 'blocked' 'history.recovery-primary-race'
	if ($null -ne $activeRecoveryRaceRecovery.Json) {
		$activeRecoveryRaceForeignRef = (@(Invoke-ScratchGit $primary @('rev-parse','refs/heads/main')))[0].Trim()
		Assert-True ($activeRecoveryRaceForeignRef -cne $activeRecoveryRaceRefBefore -and -not $activeRecoveryRaceRecovery.Json.primaryAdvanced -and $null -eq $activeRecoveryRaceRecovery.Json.final.commit) 'active recovery primary race preserves the foreign ref and reports no landing'
		Assert-True ($activeRecoveryRaceStatusBefore -ceq (@(Invoke-ScratchGit $primary @('status','--porcelain=v1','-z','--untracked-files=all')) -join '') -and $activeRecoveryRaceSessionRefBefore -ceq ((@(Invoke-ScratchGit $session @('rev-parse',"refs/heads/$sessionBranch")))[0].Trim()) -and $activeRecoveryRaceSessionHeadBefore -ceq ((@(Invoke-ScratchGit $session @('rev-parse','HEAD')))[0].Trim()) -and $activeRecoveryRaceSessionStatusBefore -ceq (@(Invoke-ScratchGit $session @('status','--porcelain=v1','-z','--untracked-files=all')) -join '')) 'active recovery primary race preserves both stale checkout states'
	}
	$activeRecoveryRaceLock = ((@(Invoke-WorktreeCli @('lock','status','--repo',$commonDirectory) 2) -join '') | ConvertFrom-Json -Depth 16)
	if ($activeRecoveryRaceLock.held) { Invoke-WorktreeCli @('lock','release','--repo',$commonDirectory,'--owner',[string]$activeRecoveryRaceLock.owner) | Out-Null }
}
Invoke-ScratchGit $primary @('reset','--hard',$activeRecoveryRaceCandidate.Commit) | Out-Null
Invoke-ScratchGit $session @('reset','--hard',$activeRecoveryRaceCandidate.Commit) | Out-Null

# The two-tree transition must also refuse a late primary edit instead of hard-resetting over it.
$activeRecoveryEditCandidate = New-CarryForwardCandidate 'active-recovery-primary-edit.cpp' 'active recovery primary edit source' 'active-recovery-primary-edit'
$activeRecoveryEditParameters = [ordered]@{ CurrentWorktree=$session; PrimaryWorktree=$primary; CurrentBranch=$sessionBranch; PrimaryBranch='main'; ExpectedCurrentTip=$activeRecoveryEditCandidate.Commit; ExpectedPrimaryTip=$activeRecoveryEditCandidate.PrimaryTip; SessionLabel='finalize-fixture'; ApprovedSessionCommit=$activeRecoveryEditCandidate.Commit; ApprovedCandidateTree=$activeRecoveryEditCandidate.Tree; FixtureFailure='post-update-ref' }
$activeRecoveryEditParameters = Add-FixtureHistoryParameters $activeRecoveryEditParameters $activeRecoveryEditCandidate.Contract
$activeRecoveryEditCrash = Invoke-JsonScriptWithSplat $landingScript $activeRecoveryEditParameters $scratchBase
Assert-Outcome $activeRecoveryEditCrash 'active-recovery-primary-edit-crash' 1 'error' 'fixture.crash-after-update-ref'
if ($null -ne $activeRecoveryEditCrash.Json) {
	$activeRecoveryEditRefBefore = (@(Invoke-ScratchGit $primary @('rev-parse','refs/heads/main')))[0].Trim()
	$activeRecoveryEditSessionRefBefore = (@(Invoke-ScratchGit $session @('rev-parse',"refs/heads/$sessionBranch")))[0].Trim()
	$activeRecoveryEditSessionHeadBefore = (@(Invoke-ScratchGit $session @('rev-parse','HEAD')))[0].Trim()
	$activeRecoveryEditParameters.FixtureFailure = 'history-recovery-active-primary-edit'
	$activeRecoveryEditRecovery = Invoke-JsonScriptWithSplat $landingScript $activeRecoveryEditParameters $scratchBase
	Assert-Outcome $activeRecoveryEditRecovery 'active-recovery-primary-edit-blocked' 2 'blocked' 'history.recovery-primary-reset-failed'
	if ($null -ne $activeRecoveryEditRecovery.Json) {
		Assert-True (-not $activeRecoveryEditRecovery.Json.primaryAdvanced -and $null -eq $activeRecoveryEditRecovery.Json.final.commit -and $activeRecoveryEditRefBefore -ceq ((@(Invoke-ScratchGit $primary @('rev-parse','refs/heads/main')))[0].Trim()) -and $activeRecoveryEditSessionRefBefore -ceq ((@(Invoke-ScratchGit $session @('rev-parse',"refs/heads/$sessionBranch")))[0].Trim()) -and $activeRecoveryEditSessionHeadBefore -ceq ((@(Invoke-ScratchGit $session @('rev-parse','HEAD')))[0].Trim())) 'active recovery primary edit leaves refs unchanged and reports no landing'
		Assert-True ([IO.File]::ReadAllText((Join-Path $primary 'base.txt'), [Text.UTF8Encoding]::new($false,$true)) -ceq 'fixture recovery active primary edit' -and (@(Invoke-ScratchGit $primary @('status','--porcelain=v1','-z','--untracked-files=all')) -join '').Length -gt 0) 'active recovery primary edit preserves the late bytes'
	}
	$activeRecoveryEditLock = ((@(Invoke-WorktreeCli @('lock','status','--repo',$commonDirectory) 2) -join '') | ConvertFrom-Json -Depth 16)
	if ($activeRecoveryEditLock.held) { Invoke-WorktreeCli @('lock','release','--repo',$commonDirectory,'--owner',[string]$activeRecoveryEditLock.owner) | Out-Null }
}
Invoke-ScratchGit $primary @('reset','--hard',$activeRecoveryEditCandidate.Commit) | Out-Null
Invoke-ScratchGit $session @('reset','--hard',$activeRecoveryEditCandidate.Commit) | Out-Null

# A clean active-overlay crash still completes through the ref-neutral two-tree transition.
$activeRecoveryStaleCandidate = New-CarryForwardCandidate 'active-recovery-stale.cpp' 'active recovery stale checkout source' 'active-recovery-stale'
$activeRecoveryStaleParameters = [ordered]@{ CurrentWorktree=$session; PrimaryWorktree=$primary; CurrentBranch=$sessionBranch; PrimaryBranch='main'; ExpectedCurrentTip=$activeRecoveryStaleCandidate.Commit; ExpectedPrimaryTip=$activeRecoveryStaleCandidate.PrimaryTip; SessionLabel='finalize-fixture'; ApprovedSessionCommit=$activeRecoveryStaleCandidate.Commit; ApprovedCandidateTree=$activeRecoveryStaleCandidate.Tree; FixtureFailure='post-update-ref' }
$activeRecoveryStaleParameters = Add-FixtureHistoryParameters $activeRecoveryStaleParameters $activeRecoveryStaleCandidate.Contract
$activeRecoveryStaleCrash = Invoke-JsonScriptWithSplat $landingScript $activeRecoveryStaleParameters $scratchBase
Assert-Outcome $activeRecoveryStaleCrash 'active-recovery-stale-crash' 1 'error' 'fixture.crash-after-update-ref'
if ($null -ne $activeRecoveryStaleCrash.Json) {
	$activeRecoveryStaleParameters.FixtureFailure = 'none'
	$activeRecoveryStaleRecovery = Invoke-JsonScriptWithSplat $landingScript $activeRecoveryStaleParameters $scratchBase
	Assert-Outcome $activeRecoveryStaleRecovery 'active-recovery-stale-landed' 0 'landed' 'ok'
	if ($null -ne $activeRecoveryStaleRecovery.Json) {
		Assert-True ($activeRecoveryStaleRecovery.Json.final.replacement -and $activeRecoveryStaleRecovery.Json.primaryAdvanced -and $activeRecoveryStaleRecovery.Json.final.commit -ceq ((@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()) -and $activeRecoveryStaleRecovery.Json.final.commit -ceq ((@(Invoke-ScratchGit $session @('rev-parse','HEAD')))[0].Trim())) 'active recovery stale checkout lands the replacement through the guarded transition'
		Assert-True ([string]::IsNullOrWhiteSpace((@(Invoke-ScratchGit $primary @('status','--porcelain=v1','-z','--untracked-files=all')) -join '')) -and [string]::IsNullOrWhiteSpace((@(Invoke-ScratchGit $session @('status','--porcelain=v1','-z','--untracked-files=all')) -join ''))) 'active recovery stale checkout finishes with both checkouts clean'
	}
}
Invoke-ScratchGit $primary @('reset','--hard',$activeRecoveryStaleCandidate.Commit) | Out-Null
Invoke-ScratchGit $session @('reset','--hard',$activeRecoveryStaleCandidate.Commit) | Out-Null

# Carry-forward landing advances the reviewed source commit directly. It does not invoke Generate,
# create a replacement commit, or change either reserved history blob.
$generateMarker = Join-Path $scratchBase 'carry-forward-generate.marker'
$previousGenerateMarker = [Environment]::GetEnvironmentVariable('BROKEN_ENGINE_FINALIZE_HISTORY_GENERATE_MARKER')
Remove-Item -LiteralPath $generateMarker -Force -ErrorAction SilentlyContinue
[Environment]::SetEnvironmentVariable('BROKEN_ENGINE_FINALIZE_HISTORY_GENERATE_MARKER', $generateMarker)
$carryCandidate = New-CarryForwardCandidate 'carry-forward-session.txt' 'carry-forward session source' 'carry-forward-session'
$carryParameters = [ordered]@{ CurrentWorktree=$session; PrimaryWorktree=$primary; CurrentBranch=$sessionBranch; PrimaryBranch='main'; ExpectedCurrentTip=$carryCandidate.Commit; ExpectedPrimaryTip=$carryCandidate.PrimaryTip; SessionLabel='finalize-fixture'; ApprovedSessionCommit=$carryCandidate.Commit; ApprovedCandidateTree=$carryCandidate.Tree }
$carryParameters = Add-FixtureHistoryParameters $carryParameters $carryCandidate.Contract
$carryJsonBlobBefore = (@(Invoke-ScratchGit $primary @('rev-parse', "$($carryCandidate.PrimaryTip):$historyJsonFixturePath")))[0].Trim()
$carrySvgBlobBefore = (@(Invoke-ScratchGit $primary @('rev-parse', "$($carryCandidate.PrimaryTip):$historySvgFixturePath")))[0].Trim()
$carryLanding = Invoke-JsonScriptWithSplat $landingScript $carryParameters $scratchBase
Assert-Outcome $carryLanding 'carry-forward-session-landing' 0 'landed' 'ok'
if ($null -ne $carryLanding.Json) {
	Assert-True ($carryLanding.Json.final.commit -ceq $carryCandidate.Commit -and $carryLanding.Json.final.tree -ceq $carryCandidate.Tree -and $carryLanding.Json.final.parent -ceq $carryCandidate.PrimaryTip -and -not $carryLanding.Json.final.replacement -and $carryLanding.Json.historyUpdate.status -ceq 'skipped') 'carry-forward session lands the reviewed source without replacement or history update'
	Assert-True ($null -eq $carryLanding.Json.historyUpdate.receipt -and $null -eq $carryLanding.Json.historyUpdate.jsonl -and $null -eq $carryLanding.Json.historyUpdate.svg) 'carry-forward session leaves history receipt and output projections null'
	Assert-True (-not (Test-Path -LiteralPath $generateMarker) -and $carryLanding.Json.primaryAdvanced -and ((@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()) -ceq $carryCandidate.Commit) 'carry-forward session does not invoke Generate and advances primary to the source commit'
	$carryJsonBlobAfter = (@(Invoke-ScratchGit $primary @('rev-parse', "$($carryCandidate.Commit):$historyJsonFixturePath")))[0].Trim()
	$carrySvgBlobAfter = (@(Invoke-ScratchGit $primary @('rev-parse', "$($carryCandidate.Commit):$historySvgFixturePath")))[0].Trim()
	Assert-True ($carryJsonBlobBefore -ceq $carryJsonBlobAfter -and $carrySvgBlobBefore -ceq $carrySvgBlobAfter) 'carry-forward session preserves both reserved history blobs'
	Assert-True ([string]::IsNullOrWhiteSpace((@(Invoke-ScratchGit $primary @('status','--porcelain=v1','-z','--untracked-files=all')) -join '')) -and [string]::IsNullOrWhiteSpace((@(Invoke-ScratchGit $session @('status','--porcelain=v1','-z','--untracked-files=all')) -join ''))) 'carry-forward session reconciles both checkouts cleanly'
}

# A plain post-update-ref crash leaves the primary ref at the source commit while the checkout is
# stale. Recovery starts from that primary ref and must report the same source-only landing.
$plainRecoveryCandidate = New-CarryForwardCandidate 'carry-forward-plain-recovery.txt' 'plain recovery source' 'carry-forward-plain-recovery'
$plainRecoveryParameters = [ordered]@{ CurrentWorktree=$session; PrimaryWorktree=$primary; CurrentBranch=$sessionBranch; PrimaryBranch='main'; ExpectedCurrentTip=$plainRecoveryCandidate.Commit; ExpectedPrimaryTip=$plainRecoveryCandidate.PrimaryTip; SessionLabel='finalize-fixture'; ApprovedSessionCommit=$plainRecoveryCandidate.Commit; ApprovedCandidateTree=$plainRecoveryCandidate.Tree; FixtureFailure='post-update-ref' }
$plainRecoveryParameters = Add-FixtureHistoryParameters $plainRecoveryParameters $plainRecoveryCandidate.Contract
Remove-Item -LiteralPath $generateMarker -Force -ErrorAction SilentlyContinue
$plainCrash = Invoke-JsonScriptWithSplat $landingScript $plainRecoveryParameters $scratchBase
Assert-Outcome $plainCrash 'carry-forward-plain-post-update-ref-crash' 1 'error' 'fixture.crash-after-update-ref'
$plainRefAfterCrash = (@(Invoke-ScratchGit $primary @('rev-parse','refs/heads/main')))[0].Trim()
$plainPrimaryStatus = (@(Invoke-ScratchGit $primary @('status','--porcelain=v1','-z','--untracked-files=all')) -join '')
Assert-True ($plainRefAfterCrash -ceq $plainRecoveryCandidate.Commit -and [string]::IsNullOrWhiteSpace((@(Invoke-ScratchGit $session @('status','--porcelain=v1','-z','--untracked-files=all')) -join '')) -and $plainPrimaryStatus.Length -gt 0) "plain carry-forward crash leaves the source ref advanced and primary checkout stale (status=$plainPrimaryStatus)"
$plainRecoveryParameters.FixtureFailure = 'none'
$plainOlderPrimaryRefBefore = (@(Invoke-ScratchGit $primary @('rev-parse','refs/heads/main')))[0].Trim()
$plainOlderPrimaryHeadBefore = (@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()
$plainOlderPrimaryStatusBefore = (@(Invoke-ScratchGit $primary @('status','--porcelain=v1','-z','--untracked-files=all')) -join '')
Invoke-ScratchGit $session @('reset','--hard',$plainRecoveryCandidate.PrimaryTip) | Out-Null
$plainOlderSessionRefBefore = (@(Invoke-ScratchGit $session @('rev-parse',"refs/heads/$sessionBranch")))[0].Trim()
$plainOlderSessionHeadBefore = (@(Invoke-ScratchGit $session @('rev-parse','HEAD')))[0].Trim()
$plainOlderSessionStatusBefore = (@(Invoke-ScratchGit $session @('status','--porcelain=v1','-z','--untracked-files=all')) -join '')
$plainOlderRecovery = Invoke-JsonScriptWithSplat $landingScript $plainRecoveryParameters $scratchBase
Assert-Outcome $plainOlderRecovery 'carry-forward-clean-older-session-recovery-blocked' 2 'blocked' 'history.recovery-session-changed'
if ($null -ne $plainOlderRecovery.Json) {
	Assert-True (-not $plainOlderRecovery.Json.primaryAdvanced -and $null -eq $plainOlderRecovery.Json.final.commit -and $plainOlderRecovery.Json.historyUpdate.status -ceq 'not-run') 'clean older session recovery does not report a landed source-only result'
	Assert-True ($plainOlderPrimaryRefBefore -ceq ((@(Invoke-ScratchGit $primary @('rev-parse','refs/heads/main')))[0].Trim()) -and $plainOlderPrimaryHeadBefore -ceq ((@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()) -and $plainOlderPrimaryStatusBefore -ceq (@(Invoke-ScratchGit $primary @('status','--porcelain=v1','-z','--untracked-files=all')) -join '') -and $plainOlderSessionRefBefore -ceq ((@(Invoke-ScratchGit $session @('rev-parse',"refs/heads/$sessionBranch")))[0].Trim()) -and $plainOlderSessionHeadBefore -ceq ((@(Invoke-ScratchGit $session @('rev-parse','HEAD')))[0].Trim()) -and $plainOlderSessionStatusBefore -ceq (@(Invoke-ScratchGit $session @('status','--porcelain=v1','-z','--untracked-files=all')) -join '')) 'clean older session recovery preserves both refs and checkouts'
}
Invoke-ScratchGit $session @('reset','--hard',$plainRecoveryCandidate.Commit) | Out-Null
$plainRecovery = Invoke-JsonScriptWithSplat $landingScript $plainRecoveryParameters $scratchBase
Assert-Outcome $plainRecovery 'carry-forward-plain-post-update-ref-recovery' 0 'landed' 'ok'
if ($null -ne $plainRecovery.Json) {
	Assert-True ($plainRecovery.Json.final.commit -ceq $plainRecoveryCandidate.Commit -and -not $plainRecovery.Json.final.replacement -and $plainRecovery.Json.historyUpdate.status -ceq 'skipped' -and $plainRecovery.Json.primaryAdvanced) 'plain carry-forward recovery reports source-only landing'
	Assert-True (((@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()) -ceq $plainRecoveryCandidate.Commit -and [string]::IsNullOrWhiteSpace((@(Invoke-ScratchGit $primary @('status','--porcelain=v1','-z','--untracked-files=all')) -join '')) -and [string]::IsNullOrWhiteSpace((@(Invoke-ScratchGit $session @('status','--porcelain=v1','-z','--untracked-files=all')) -join ''))) 'plain carry-forward recovery reconciles both checkouts cleanly'
	Assert-True (-not (Test-Path -LiteralPath $generateMarker)) 'plain carry-forward recovery does not invoke Generate'
}

# Recovery must not treat a user edit made after the ref update as the stale parent checkout.
$plainPrimaryEditCandidate = New-CarryForwardCandidate 'carry-forward-primary-edit.txt' 'primary edit recovery source' 'carry-forward-primary-edit'
$plainPrimaryEditParameters = [ordered]@{ CurrentWorktree=$session; PrimaryWorktree=$primary; CurrentBranch=$sessionBranch; PrimaryBranch='main'; ExpectedCurrentTip=$plainPrimaryEditCandidate.Commit; ExpectedPrimaryTip=$plainPrimaryEditCandidate.PrimaryTip; SessionLabel='finalize-fixture'; ApprovedSessionCommit=$plainPrimaryEditCandidate.Commit; ApprovedCandidateTree=$plainPrimaryEditCandidate.Tree; FixtureFailure='post-update-ref' }
$plainPrimaryEditParameters = Add-FixtureHistoryParameters $plainPrimaryEditParameters $plainPrimaryEditCandidate.Contract
$plainPrimaryEditCrash = Invoke-JsonScriptWithSplat $landingScript $plainPrimaryEditParameters $scratchBase
Assert-Outcome $plainPrimaryEditCrash 'carry-forward-primary-edit-crash' 1 'error' 'fixture.crash-after-update-ref'
$plainPrimaryEditRefBefore = (@(Invoke-ScratchGit $primary @('rev-parse','refs/heads/main')))[0].Trim()
$plainPrimaryEditHeadBefore = (@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()
$plainPrimaryEditSessionRefBefore = (@(Invoke-ScratchGit $session @('rev-parse',"refs/heads/$sessionBranch")))[0].Trim()
$plainPrimaryEditSessionHeadBefore = (@(Invoke-ScratchGit $session @('rev-parse','HEAD')))[0].Trim()
[IO.File]::WriteAllText((Join-Path $primary 'base.txt'), 'user primary recovery edit', [Text.UTF8Encoding]::new($false))
$plainPrimaryEditStatusBefore = (@(Invoke-ScratchGit $primary @('status','--porcelain=v1','-z','--untracked-files=all')) -join '')
$plainPrimaryEditSessionStatusBefore = (@(Invoke-ScratchGit $session @('status','--porcelain=v1','-z','--untracked-files=all')) -join '')
$plainPrimaryEditParameters.FixtureFailure = 'none'
$plainPrimaryEditRecovery = Invoke-JsonScriptWithSplat $landingScript $plainPrimaryEditParameters $scratchBase
Assert-Outcome $plainPrimaryEditRecovery 'carry-forward-primary-edit-recovery-blocked' 2 'blocked' 'history.recovery-checkout-changed'
if ($null -ne $plainPrimaryEditRecovery.Json) {
	Assert-True ($plainPrimaryEditRecovery.Json.message -ceq 'Recovery checkout does not match the expected post-update-ref state; no checkout or ref was modified.') 'primary recovery edit reports the stable no-mutation message'
	Assert-True ($plainPrimaryEditRefBefore -ceq ((@(Invoke-ScratchGit $primary @('rev-parse','refs/heads/main')))[0].Trim()) -and $plainPrimaryEditHeadBefore -ceq ((@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()) -and $plainPrimaryEditSessionRefBefore -ceq ((@(Invoke-ScratchGit $session @('rev-parse',"refs/heads/$sessionBranch")))[0].Trim()) -and $plainPrimaryEditSessionHeadBefore -ceq ((@(Invoke-ScratchGit $session @('rev-parse','HEAD')))[0].Trim())) 'primary recovery edit leaves both refs and checkout heads unchanged'
	Assert-True ($plainPrimaryEditStatusBefore -ceq (@(Invoke-ScratchGit $primary @('status','--porcelain=v1','-z','--untracked-files=all')) -join '') -and $plainPrimaryEditSessionStatusBefore -ceq (@(Invoke-ScratchGit $session @('status','--porcelain=v1','-z','--untracked-files=all')) -join '') -and ([IO.File]::ReadAllText((Join-Path $primary 'base.txt'), [Text.UTF8Encoding]::new($false,$true)) -ceq 'user primary recovery edit')) 'primary recovery edit preserves both checkout states and user bytes'
}
$plainPrimaryEditLock = ((@(Invoke-WorktreeCli @('lock','status','--repo',$commonDirectory) 2) -join '') | ConvertFrom-Json -Depth 16)
if ($plainPrimaryEditLock.held) { Invoke-WorktreeCli @('lock','release','--repo',$commonDirectory,'--owner',[string]$plainPrimaryEditLock.owner) | Out-Null }
Invoke-ScratchGit $primary @('reset','--hard',$plainPrimaryEditCandidate.Commit) | Out-Null
Invoke-ScratchGit $session @('reset','--hard',$plainPrimaryEditCandidate.Commit) | Out-Null

# The session checkout is also guarded before primary reconciliation, so an edit there cannot be
# erased after a source-only crash even when the primary still has the exact expected stale tree.
$plainSessionEditCandidate = New-CarryForwardCandidate 'carry-forward-session-edit.txt' 'session edit recovery source' 'carry-forward-session-edit'
$plainSessionEditParameters = [ordered]@{ CurrentWorktree=$session; PrimaryWorktree=$primary; CurrentBranch=$sessionBranch; PrimaryBranch='main'; ExpectedCurrentTip=$plainSessionEditCandidate.Commit; ExpectedPrimaryTip=$plainSessionEditCandidate.PrimaryTip; SessionLabel='finalize-fixture'; ApprovedSessionCommit=$plainSessionEditCandidate.Commit; ApprovedCandidateTree=$plainSessionEditCandidate.Tree; FixtureFailure='post-update-ref' }
$plainSessionEditParameters = Add-FixtureHistoryParameters $plainSessionEditParameters $plainSessionEditCandidate.Contract
$plainSessionEditCrash = Invoke-JsonScriptWithSplat $landingScript $plainSessionEditParameters $scratchBase
Assert-Outcome $plainSessionEditCrash 'carry-forward-session-edit-crash' 1 'error' 'fixture.crash-after-update-ref'
$plainSessionEditRefBefore = (@(Invoke-ScratchGit $primary @('rev-parse','refs/heads/main')))[0].Trim()
$plainSessionEditHeadBefore = (@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()
$plainSessionEditSessionRefBefore = (@(Invoke-ScratchGit $session @('rev-parse',"refs/heads/$sessionBranch")))[0].Trim()
$plainSessionEditSessionHeadBefore = (@(Invoke-ScratchGit $session @('rev-parse','HEAD')))[0].Trim()
[IO.File]::WriteAllText((Join-Path $session 'base.txt'), 'user session recovery edit', [Text.UTF8Encoding]::new($false))
$plainSessionEditStatusBefore = (@(Invoke-ScratchGit $primary @('status','--porcelain=v1','-z','--untracked-files=all')) -join '')
$plainSessionEditSessionStatusBefore = (@(Invoke-ScratchGit $session @('status','--porcelain=v1','-z','--untracked-files=all')) -join '')
$plainSessionEditParameters.FixtureFailure = 'none'
$plainSessionEditRecovery = Invoke-JsonScriptWithSplat $landingScript $plainSessionEditParameters $scratchBase
Assert-Outcome $plainSessionEditRecovery 'carry-forward-session-edit-recovery-blocked' 2 'blocked' 'history.recovery-checkout-changed'
if ($null -ne $plainSessionEditRecovery.Json) {
	Assert-True ($plainSessionEditRecovery.Json.message -ceq 'Recovery checkout does not match the expected post-update-ref state; no checkout or ref was modified.') 'session recovery edit reports the stable no-mutation message'
	Assert-True ($plainSessionEditRefBefore -ceq ((@(Invoke-ScratchGit $primary @('rev-parse','refs/heads/main')))[0].Trim()) -and $plainSessionEditHeadBefore -ceq ((@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()) -and $plainSessionEditSessionRefBefore -ceq ((@(Invoke-ScratchGit $session @('rev-parse',"refs/heads/$sessionBranch")))[0].Trim()) -and $plainSessionEditSessionHeadBefore -ceq ((@(Invoke-ScratchGit $session @('rev-parse','HEAD')))[0].Trim())) 'session recovery edit leaves both refs and checkout heads unchanged'
	Assert-True ($plainSessionEditStatusBefore -ceq (@(Invoke-ScratchGit $primary @('status','--porcelain=v1','-z','--untracked-files=all')) -join '') -and $plainSessionEditSessionStatusBefore -ceq (@(Invoke-ScratchGit $session @('status','--porcelain=v1','-z','--untracked-files=all')) -join '') -and ([IO.File]::ReadAllText((Join-Path $session 'base.txt'), [Text.UTF8Encoding]::new($false,$true)) -ceq 'user session recovery edit')) 'session recovery edit preserves both checkout states and user bytes'
}
$plainSessionEditLock = ((@(Invoke-WorktreeCli @('lock','status','--repo',$commonDirectory) 2) -join '') | ConvertFrom-Json -Depth 16)
if ($plainSessionEditLock.held) { Invoke-WorktreeCli @('lock','release','--repo',$commonDirectory,'--owner',[string]$plainSessionEditLock.owner) | Out-Null }
Invoke-ScratchGit $primary @('reset','--hard',$plainSessionEditCandidate.Commit) | Out-Null
Invoke-ScratchGit $session @('reset','--hard',$plainSessionEditCandidate.Commit) | Out-Null

# A tracked edit arriving after recovery's checkout precheck must make the guarded session reset
# refuse/rollback, rather than letting the old hard reset overwrite the user's bytes.
$sessionRaceCandidate = New-CarryForwardCandidate 'carry-forward-session-race.cpp' 'session race recovery source' 'carry-forward-session-race'
$sessionRaceParameters = [ordered]@{ CurrentWorktree=$session; PrimaryWorktree=$primary; CurrentBranch=$sessionBranch; PrimaryBranch='main'; ExpectedCurrentTip=$sessionRaceCandidate.Commit; ExpectedPrimaryTip=$sessionRaceCandidate.PrimaryTip; SessionLabel='finalize-fixture'; ApprovedSessionCommit=$sessionRaceCandidate.Commit; ApprovedCandidateTree=$sessionRaceCandidate.Tree; FixtureFailure='post-update-ref' }
$sessionRaceParameters = Add-FixtureHistoryParameters $sessionRaceParameters $sessionRaceCandidate.Contract
$sessionRaceCrash = Invoke-JsonScriptWithSplat $landingScript $sessionRaceParameters $scratchBase
Assert-Outcome $sessionRaceCrash 'carry-forward-session-race-crash' 1 'error' 'fixture.crash-after-update-ref'
if ($null -ne $sessionRaceCrash.Json) {
	$sessionRaceSessionRefBefore = (@(Invoke-ScratchGit $session @('rev-parse',"refs/heads/$sessionBranch")))[0].Trim()
	$sessionRaceSessionHeadBefore = (@(Invoke-ScratchGit $session @('rev-parse','HEAD')))[0].Trim()
	$sessionRaceParameters.FixtureFailure = 'history-recovery-session-race'
	$sessionRaceRecovery = Invoke-JsonScriptWithSplat $landingScript $sessionRaceParameters $scratchBase
	Assert-Outcome $sessionRaceRecovery 'carry-forward-session-race-recovery-blocked' 2 'blocked' 'history.recovery-session-reset-failed'
	if ($null -ne $sessionRaceRecovery.Json) {
		Assert-True ($sessionRaceSessionRefBefore -ceq ((@(Invoke-ScratchGit $session @('rev-parse',"refs/heads/$sessionBranch")))[0].Trim()) -and $sessionRaceSessionHeadBefore -ceq ((@(Invoke-ScratchGit $session @('rev-parse','HEAD')))[0].Trim())) 'session race rollback restores the prior session ref and checkout head'
		Assert-True ([IO.File]::ReadAllText((Join-Path $session 'base.txt'), [Text.UTF8Encoding]::new($false,$true)) -ceq 'fixture recovery session race' -and (@(Invoke-ScratchGit $session @('status','--porcelain=v1','-z','--untracked-files=all')) -join '').Length -gt 0) 'session race preserves the late tracked edit after guarded reset refusal'
	}
	$sessionRaceLock = ((@(Invoke-WorktreeCli @('lock','status','--repo',$commonDirectory) 2) -join '') | ConvertFrom-Json -Depth 16)
	if ($sessionRaceLock.held) { Invoke-WorktreeCli @('lock','release','--repo',$commonDirectory,'--owner',[string]$sessionRaceLock.owner) | Out-Null }
	Invoke-ScratchGit $session @('reset','--hard',$sessionRaceCandidate.Commit) | Out-Null
}

# A staged edit arriving after recovery's checkout precheck must remain staged when the guarded
# session transition refuses or rolls back; reset --keep used to silently turn this into an unstaged
# edit when the old and new session revisions were equal.
$sessionStagedRaceCandidate = New-CarryForwardCandidate 'carry-forward-session-staged-race.cpp' 'session staged race recovery source' 'carry-forward-session-staged-race'
$sessionStagedRaceParameters = [ordered]@{ CurrentWorktree=$session; PrimaryWorktree=$primary; CurrentBranch=$sessionBranch; PrimaryBranch='main'; ExpectedCurrentTip=$sessionStagedRaceCandidate.Commit; ExpectedPrimaryTip=$sessionStagedRaceCandidate.PrimaryTip; SessionLabel='finalize-fixture'; ApprovedSessionCommit=$sessionStagedRaceCandidate.Commit; ApprovedCandidateTree=$sessionStagedRaceCandidate.Tree; FixtureFailure='post-update-ref' }
$sessionStagedRaceParameters = Add-FixtureHistoryParameters $sessionStagedRaceParameters $sessionStagedRaceCandidate.Contract
$sessionStagedRaceCrash = Invoke-JsonScriptWithSplat $landingScript $sessionStagedRaceParameters $scratchBase
Assert-Outcome $sessionStagedRaceCrash 'carry-forward-session-staged-race-crash' 1 'error' 'fixture.crash-after-update-ref'
if ($null -ne $sessionStagedRaceCrash.Json) {
	$sessionStagedRacePrimaryRefBefore = (@(Invoke-ScratchGit $primary @('rev-parse','refs/heads/main')))[0].Trim()
	$sessionStagedRaceSessionRefBefore = (@(Invoke-ScratchGit $session @('rev-parse',"refs/heads/$sessionBranch")))[0].Trim()
	$sessionStagedRaceSessionHeadBefore = (@(Invoke-ScratchGit $session @('rev-parse','HEAD')))[0].Trim()
	$sessionStagedRaceSessionStatusBefore = (@(Invoke-ScratchGit $session @('status','--porcelain=v1','-z','--untracked-files=all')) -join '')
	$sessionStagedRaceParameters.FixtureFailure = 'history-recovery-session-staged-race'
	$sessionStagedRaceRecovery = Invoke-JsonScriptWithSplat $landingScript $sessionStagedRaceParameters $scratchBase
	Assert-Outcome $sessionStagedRaceRecovery 'carry-forward-session-staged-race-recovery-blocked' 2 'blocked' 'history.recovery-session-reset-failed'
	if ($null -ne $sessionStagedRaceRecovery.Json) {
		$sessionStagedRaceStatusAfter = (@(Invoke-ScratchGit $session @('status','--porcelain=v1','-z','--untracked-files=all')) -join '')
		$sessionStagedRaceCachedAfter = (@(Invoke-ScratchGit $session @('diff','--cached','--name-status','--')) -join '').Trim()
		$sessionStagedRaceUnstagedAfter = (@(Invoke-ScratchGit $session @('diff','--name-status','--')) -join '').Trim()
		$sessionStagedRaceUntrackedAfter = (@(Invoke-ScratchGit $session @('ls-files','--others','--exclude-standard','-z')) -join '')
		Assert-True ($sessionStagedRacePrimaryRefBefore -ceq ((@(Invoke-ScratchGit $primary @('rev-parse','refs/heads/main')))[0].Trim()) -and $sessionStagedRaceSessionRefBefore -ceq ((@(Invoke-ScratchGit $session @('rev-parse',"refs/heads/$sessionBranch")))[0].Trim()) -and $sessionStagedRaceSessionHeadBefore -ceq ((@(Invoke-ScratchGit $session @('rev-parse','HEAD')))[0].Trim())) 'staged session race rollback preserves prior refs and session checkout head'
		Assert-True ($sessionStagedRaceSessionStatusBefore.Length -eq 0 -and $sessionStagedRaceStatusAfter -ceq "M  base.txt$([char]0)" -and $sessionStagedRaceCachedAfter -ceq "M`tbase.txt" -and $sessionStagedRaceUnstagedAfter.Length -eq 0 -and $sessionStagedRaceUntrackedAfter.Length -eq 0) 'staged session race preserves the exact late index/status state'
		Assert-True ([IO.File]::ReadAllText((Join-Path $session 'base.txt'), [Text.UTF8Encoding]::new($false,$true)) -ceq 'fixture recovery session staged race') 'staged session race preserves the late staged bytes'
	}
	$sessionStagedRaceLock = ((@(Invoke-WorktreeCli @('lock','status','--repo',$commonDirectory) 2) -join '') | ConvertFrom-Json -Depth 16)
	if ($sessionStagedRaceLock.held) { Invoke-WorktreeCli @('lock','release','--repo',$commonDirectory,'--owner',[string]$sessionStagedRaceLock.owner) | Out-Null }
	Invoke-ScratchGit $primary @('reset','--hard',$sessionStagedRaceCandidate.Commit) | Out-Null
	Invoke-ScratchGit $session @('reset','--hard',$sessionStagedRaceCandidate.Commit) | Out-Null
}

# An internally rebased carry-forward crash has a primary ref that is neither the approved source
# nor its original parent. Recovery must match the rebased source commit from the primary ref.
$rebasedRecoveryCandidate = New-CarryForwardCandidate 'carry-forward-rebased-recovery.txt' 'internally rebased recovery source' 'carry-forward-rebased-recovery'
$rebasedRecoveryParameters = [ordered]@{ CurrentWorktree=$session; PrimaryWorktree=$primary; CurrentBranch=$sessionBranch; PrimaryBranch='main'; ExpectedCurrentTip=$rebasedRecoveryCandidate.Commit; ExpectedPrimaryTip=$rebasedRecoveryCandidate.PrimaryTip; SessionLabel='finalize-fixture'; ApprovedSessionCommit=$rebasedRecoveryCandidate.Commit; ApprovedCandidateTree=$rebasedRecoveryCandidate.Tree; FixtureFailure='post-update-ref' }
$rebasedRecoveryParameters = Add-FixtureHistoryParameters $rebasedRecoveryParameters $rebasedRecoveryCandidate.Contract
[IO.File]::WriteAllText((Join-Path $primary 'carry-forward-recovery-upstream.txt'), 'upstream primary source', [Text.UTF8Encoding]::new($false))
Invoke-ScratchGit $primary @('add','carry-forward-recovery-upstream.txt') | Out-Null
Invoke-ScratchGit $primary @('commit','-m','carry-forward recovery upstream') | Out-Null
$rebasedUpstream = (@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()
Remove-Item -LiteralPath $generateMarker -Force -ErrorAction SilentlyContinue
$rebasedCrash = Invoke-JsonScriptWithSplat $landingScript $rebasedRecoveryParameters $scratchBase
Assert-Outcome $rebasedCrash 'carry-forward-rebased-post-update-ref-crash' 1 'error' 'fixture.crash-after-update-ref'
$rebasedCrashRef = (@(Invoke-ScratchGit $primary @('rev-parse','refs/heads/main')))[0].Trim()
$rebasedPrimaryStatus = (@(Invoke-ScratchGit $primary @('status','--porcelain=v1','-z','--untracked-files=all')) -join '')
Assert-True ($rebasedCrashRef -cne $rebasedUpstream -and $rebasedCrashRef -cne $rebasedRecoveryCandidate.Commit -and $rebasedPrimaryStatus.Length -gt 0) "internally rebased carry-forward crash leaves the rebased source ref ahead of the stale checkout (status=$rebasedPrimaryStatus)"
$rebasedRecoveryParameters.FixtureFailure = 'none'
$rebasedRecovery = Invoke-JsonScriptWithSplat $landingScript $rebasedRecoveryParameters $scratchBase
Assert-Outcome $rebasedRecovery 'carry-forward-rebased-post-update-ref-recovery' 0 'landed' 'ok'
if ($null -ne $rebasedRecovery.Json) {
	Assert-True ($rebasedRecovery.Json.final.commit -ceq $rebasedCrashRef -and -not $rebasedRecovery.Json.final.replacement -and $rebasedRecovery.Json.historyUpdate.status -ceq 'skipped') 'internally rebased carry-forward recovery reports source-only landing'
	Assert-True (((@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()) -ceq $rebasedCrashRef -and ((@(Invoke-ScratchGit $session @('rev-parse',"refs/heads/$sessionBranch")))[0].Trim()) -ceq $rebasedCrashRef -and [string]::IsNullOrWhiteSpace((@(Invoke-ScratchGit $primary @('status','--porcelain=v1','-z','--untracked-files=all')) -join '')) -and [string]::IsNullOrWhiteSpace((@(Invoke-ScratchGit $session @('status','--porcelain=v1','-z','--untracked-files=all')) -join ''))) 'internally rebased carry-forward recovery reconciles both checkouts cleanly'
	Assert-True (-not (Test-Path -LiteralPath $generateMarker)) 'internally rebased carry-forward recovery does not invoke Generate'
}

# A carry-forward crash that is reclassified as an active capture after the internal rebase must not
# fall through to the old head/patch-only recovery result. The approved catch-up identity permits the
# current active Contract to be re-evaluated, while the source-only result still requires carry-forward.
$reclassCandidate = New-CarryForwardCandidate 'carry-forward-reclassified-recovery.txt' 'reclassified recovery source' 'carry-forward-reclassified-recovery'
$reclassParameters = [ordered]@{ CurrentWorktree=$session; PrimaryWorktree=$primary; CurrentBranch=$sessionBranch; PrimaryBranch='main'; ExpectedCurrentTip=$reclassCandidate.Commit; ExpectedPrimaryTip=$reclassCandidate.PrimaryTip; SessionLabel='finalize-fixture'; ApprovedSessionCommit=$reclassCandidate.Commit; ApprovedCandidateTree=$reclassCandidate.Tree; FixtureFailure='post-update-ref' }
$reclassParameters = Add-FixtureHistoryParameters $reclassParameters $reclassCandidate.Contract
$previousHistoryMode = [Environment]::GetEnvironmentVariable('BROKEN_ENGINE_FINALIZE_HISTORY_MODE')
[Environment]::SetEnvironmentVariable('BROKEN_ENGINE_FINALIZE_HISTORY_MODE', 'cpp-change')
$reclassActiveContract = Get-FixtureHistoryContract $reclassCandidate.PrimaryTip $reclassCandidate.Commit
[Environment]::SetEnvironmentVariable('BROKEN_ENGINE_FINALIZE_HISTORY_MODE', $previousHistoryMode)
$reclassParameters.HistoryContractMode = 'catch-up'
$reclassParameters.HistoryContractDigest = $reclassActiveContract.digest
$reclassParameters.HistoryContractCaptureDigest = $reclassActiveContract.captureDigest
$reclassParameters.HistoryContractRuntimeDigest = $reclassActiveContract.runtimeDigest
[IO.File]::WriteAllText((Join-Path $primary 'carry-forward-reclassified-upstream.txt'), 'reclassified upstream source', [Text.UTF8Encoding]::new($false))
Invoke-ScratchGit $primary @('add','carry-forward-reclassified-upstream.txt') | Out-Null
Invoke-ScratchGit $primary @('commit','-m','carry-forward reclassified upstream') | Out-Null
$reclassUpstream = (@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()
$reclassCrash = Invoke-JsonScriptWithSplat $landingScript $reclassParameters $scratchBase
Assert-Outcome $reclassCrash 'carry-forward-reclassified-post-update-ref-crash' 1 'error' 'fixture.crash-after-update-ref'
$reclassCrashRef = (@(Invoke-ScratchGit $primary @('rev-parse','refs/heads/main')))[0].Trim()
$reclassCrashSessionRef = (@(Invoke-ScratchGit $session @('rev-parse',"refs/heads/$sessionBranch")))[0].Trim()
Assert-True ($reclassCrashRef -cne $reclassUpstream -and $reclassCrashRef -cne $reclassCandidate.Commit -and $reclassCrashSessionRef -ceq $reclassCrashRef) 'reclassification crash leaves the internally rebased source on both refs'
$reclassPrimaryRefBefore = (@(Invoke-ScratchGit $primary @('rev-parse','refs/heads/main')))[0].Trim()
$reclassPrimaryHeadBefore = (@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()
$reclassPrimaryStatusBefore = (@(Invoke-ScratchGit $primary @('status','--porcelain=v1','-z','--untracked-files=all')) -join '')
$reclassSessionRefBefore = (@(Invoke-ScratchGit $session @('rev-parse',"refs/heads/$sessionBranch")))[0].Trim()
$reclassSessionHeadBefore = (@(Invoke-ScratchGit $session @('rev-parse','HEAD')))[0].Trim()
$reclassSessionStatusBefore = (@(Invoke-ScratchGit $session @('status','--porcelain=v1','-z','--untracked-files=all')) -join '')
$reclassParameters.FixtureFailure = 'none'
[Environment]::SetEnvironmentVariable('BROKEN_ENGINE_FINALIZE_HISTORY_MODE', 'cpp-change')
$reclassRecovery = Invoke-JsonScriptWithSplat $landingScript $reclassParameters $scratchBase
Assert-Outcome $reclassRecovery 'carry-forward-reclassified-recovery-blocked' 2 'blocked' 'sanity.git.session-tip-changed'
if ($null -ne $reclassRecovery.Json) {
	Assert-True (-not $reclassRecovery.Json.primaryAdvanced -and $null -eq $reclassRecovery.Json.final.commit -and $reclassRecovery.Json.historyUpdate.status -ceq 'not-run') 'reclassified carry-forward recovery cannot report a source-only landed result'
	Assert-True ($reclassPrimaryRefBefore -ceq ((@(Invoke-ScratchGit $primary @('rev-parse','refs/heads/main')))[0].Trim()) -and $reclassPrimaryHeadBefore -ceq ((@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()) -and $reclassPrimaryStatusBefore -ceq (@(Invoke-ScratchGit $primary @('status','--porcelain=v1','-z','--untracked-files=all')) -join '') -and $reclassSessionRefBefore -ceq ((@(Invoke-ScratchGit $session @('rev-parse',"refs/heads/$sessionBranch")))[0].Trim()) -and $reclassSessionHeadBefore -ceq ((@(Invoke-ScratchGit $session @('rev-parse','HEAD')))[0].Trim()) -and $reclassSessionStatusBefore -ceq (@(Invoke-ScratchGit $session @('status','--porcelain=v1','-z','--untracked-files=all')) -join '')) 'reclassified recovery leaves both checkouts and refs unchanged'
}
[Environment]::SetEnvironmentVariable('BROKEN_ENGINE_FINALIZE_HISTORY_MODE', $previousHistoryMode)
Invoke-ScratchGit $primary @('reset','--hard',$reclassCandidate.Commit) | Out-Null
Invoke-ScratchGit $session @('reset','--hard',$reclassCandidate.Commit) | Out-Null
[Environment]::SetEnvironmentVariable('BROKEN_ENGINE_FINALIZE_HISTORY_GENERATE_MARKER', $previousGenerateMarker)

# Lease continuity and the single internal rebase-and-retry. Every scenario below changes the last
# line of one twenty-line tracked file, so an upstream commit can touch a distant line of the same
# file and produce a clean rebase, or the same line and produce a conflict.
function New-RetryFileText([string] $First, [string] $Last) {
	return ((@($First) + @(2..19 | ForEach-Object { "line $_" }) + @($Last)) -join "`n") + "`n"
}
function New-RetryCandidate([string] $Text, [string] $Case) {
	$tip = (@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()
	Invoke-ScratchGit $session @('reset','--hard',$tip) | Out-Null
	Invoke-ScratchGit $session @('clean','-fd') | Out-Null
	[IO.File]::WriteAllText((Join-Path $session 'retry-file.cpp'), $Text, [Text.UTF8Encoding]::new($false))
	$candidate = Invoke-JsonScript $candidateScript @('-Route','session-landing','-CurrentWorktree',$session,'-PrimaryWorktree',$primary,'-CurrentBranch',$sessionBranch,'-PrimaryBranch','main','-Baseline',$tip,'-ExpectedCurrentTip',$tip,'-ExpectedPrimaryTip',$tip,'-OwnedPaths','retry-file.cpp','-CommitMessageFile',$candidateMessage)
	Assert-Outcome $candidate "$Case-candidate" 0 'pass' 'candidate.created'
	return [pscustomobject]@{ PrimaryTip = $tip; Commit = $candidate.Json.candidate.commit; Tree = $candidate.Json.candidate.tree; Contract = Get-FixtureHistoryContract $tip $candidate.Json.candidate.commit }
}
function New-RetryLandingParameters($Candidate) {
	$parameters = [ordered]@{ CurrentWorktree=$session; PrimaryWorktree=$primary; CurrentBranch=$sessionBranch; PrimaryBranch='main'; ExpectedCurrentTip=$Candidate.Commit; ExpectedPrimaryTip=$Candidate.PrimaryTip; SessionLabel='finalize-fixture'; ApprovedSessionCommit=$Candidate.Commit; ApprovedCandidateTree=$Candidate.Tree }
	return Add-FixtureHistoryParameters $parameters $Candidate.Contract
}
function Add-UpstreamPrimaryCommit([string] $Text) {
	[IO.File]::WriteAllText((Join-Path $primary 'retry-file.cpp'), $Text, [Text.UTF8Encoding]::new($false))
	Invoke-ScratchGit $primary @('add','retry-file.cpp') | Out-Null
	Invoke-ScratchGit $primary @('commit','-m','upstream change') | Out-Null
	return (@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()
}
function Test-SessionRebaseMarkersAbsent {
	foreach ($marker in @('rebase-merge','rebase-apply')) {
		if (Test-Path -LiteralPath (@(Invoke-ScratchGit $session @('rev-parse','--path-format=absolute','--git-path',$marker)))[0].Trim()) { return $false }
	}
	return $true
}

$retryHead = 'line 1'
$retryTail = 'line 20'
[IO.File]::WriteAllText((Join-Path $primary 'retry-file.cpp'), (New-RetryFileText $retryHead $retryTail), [Text.UTF8Encoding]::new($false))
Invoke-ScratchGit $primary @('add','retry-file.cpp') | Out-Null
Invoke-ScratchGit $primary @('commit','-m','retry fixture base') | Out-Null

$retryTail = 'continuity tail'
$continuityCandidate = New-RetryCandidate (New-RetryFileText $retryHead $retryTail) 'lease-continuity'
$continuityOwner = [guid]::NewGuid().ToString()
$continuityClaim = Invoke-JsonScript $lockClaimScript (@('-WorktreeCliExecutable', (Join-Path $primaryOutput 'WorktreeCli.exe'), '-GitCommonDirectory', $commonDirectory, '-SessionLabel', 'finalize-fixture', '-Worktree', $session, '-LandingOwner', $continuityOwner, '-LeaseSeconds', '3600'))
Assert-Outcome $continuityClaim 'lease-continuity-claim' 0 'pass' 'ok'
$continuityParameters = New-RetryLandingParameters $continuityCandidate
$continuityParameters.OwnerToken = $continuityOwner
$continuity = Invoke-JsonScriptWithSplat $landingScript $continuityParameters $scratchBase
Assert-Outcome $continuity 'landing-continues-caller-lease' 0 'landed' 'ok'
if ($null -ne $continuity.Json) {
	Assert-True ($continuity.Json.lock.claimed -and $continuity.Json.lock.claimCode -ceq 'ok' -and $continuity.Json.lock.attempts -eq 1 -and $continuity.Json.lock.released) 'landing continues under the caller lease without a fresh claim round'
	Assert-True ($continuity.Json.landed.commit -ceq $continuity.Json.final.commit -and $continuity.Json.final.replacement -and $continuity.Json.landed.rebaseAttempts -eq 0) 'a continued lease lands the deterministic replacement with no rebase'
	Assert-True ($continuity.Json.primaryAdvanced -and ((@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()) -ceq $continuity.Json.final.commit) 'a continued lease advances primary to the deterministic final commit'
}

# A continued lease must be able to outlast the advance, and WorktreeCli's refresh keeps a lease's
# own duration, so a token minted with a shorter lease is refused instead of continued.
$shortLeaseCandidate = New-RetryCandidate (New-RetryFileText $retryHead 'short lease tail') 'short-lease'
$shortLeaseOwner = [guid]::NewGuid().ToString()
$shortLeaseClaim = Invoke-JsonScript $lockClaimScript (@('-WorktreeCliExecutable', (Join-Path $primaryOutput 'WorktreeCli.exe'), '-GitCommonDirectory', $commonDirectory, '-SessionLabel', 'finalize-fixture', '-Worktree', $session, '-LandingOwner', $shortLeaseOwner, '-LeaseSeconds', '60'))
Assert-Outcome $shortLeaseClaim 'short-lease-continuity-claim' 0 'pass' 'ok'
$shortLeasePrimaryBefore = (@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()
$shortLeaseParameters = New-RetryLandingParameters $shortLeaseCandidate
$shortLeaseParameters.OwnerToken = $shortLeaseOwner
$shortLease = Invoke-JsonScriptWithSplat $landingScript $shortLeaseParameters $scratchBase
Assert-Outcome $shortLease 'landing-refuses-short-caller-lease' 2 'blocked' 'landing-lock.claim-failed'
if ($null -ne $shortLease.Json) {
	Assert-True (-not $shortLease.Json.lock.claimed -and $shortLease.Json.lock.claimCode -ceq 'landing-lock.retryable-wait' -and $shortLease.Json.disposition -ceq 'retryable-wait') 'a caller lease shorter than the landing lease is refused as retryable contention'
	Assert-True (-not $shortLease.Json.primaryAdvanced -and ((@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()) -ceq $shortLeasePrimaryBefore) 'a refused short lease leaves primary unchanged'
}
Invoke-WorktreeCli @('lock', 'release', '--repo', $commonDirectory, '--owner', $shortLeaseOwner) | Out-Null

# Independent signal: the same supplied token, live, but recorded against another worktree stays
# foreign contention, so the landing refuses instead of continuing or minting behind the caller.
$foreignCandidate = New-RetryCandidate (New-RetryFileText $retryHead 'foreign lease tail') 'foreign-lease'
$foreignOwner = [guid]::NewGuid().ToString()
Invoke-WorktreeCli @('lock', 'claim', '--repo', $commonDirectory, '--owner', $foreignOwner, '--session', 'finalize-fixture', '--worktree', $primary, '--lease-seconds', '600') | Out-Null
$foreignPrimaryBefore = (@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()
$foreignParameters = New-RetryLandingParameters $foreignCandidate
$foreignParameters.OwnerToken = $foreignOwner
$foreign = Invoke-JsonScriptWithSplat $landingScript $foreignParameters $scratchBase
Assert-Outcome $foreign 'landing-refuses-foreign-lease' 2 'blocked' 'landing-lock.claim-failed'
if ($null -ne $foreign.Json) {
	Assert-True (-not $foreign.Json.lock.claimed -and $foreign.Json.lock.claimCode -ceq 'landing-lock.retryable-wait' -and $foreign.Json.disposition -ceq 'retryable-wait') 'a lease recorded for another worktree is refused as retryable contention'
	Assert-True (-not $foreign.Json.primaryAdvanced -and ((@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()) -ceq $foreignPrimaryBefore) 'a refused foreign lease leaves primary unchanged'
}
Invoke-WorktreeCli @('lock', 'release', '--repo', $commonDirectory, '--owner', $foreignOwner) | Out-Null

# An omitted-token landing adopts only a retained claim under its derived identity. Foreign live
# leases are observed in a bounded child process so the production 300-second wait is not shortened;
# the child is reaped before the fixture releases the foreign owner. Unverifiable metadata must fail
# immediately with authority required and remain untouched.
function Assert-OmittedForeignLease([string] $Case, [string] $LeaseSession, [string] $LeaseWorktree, [bool] $Unverifiable = $false, [int] $LeaseSeconds = 600) {
	$candidate = New-RetryCandidate (New-RetryFileText $retryHead "$Case tail") $Case
	$owner = [guid]::NewGuid().ToString()
	Invoke-WorktreeCli @('lock', 'claim', '--repo', $commonDirectory, '--owner', $owner, '--session', $LeaseSession, '--worktree', $LeaseWorktree, '--lease-seconds', [string]$LeaseSeconds) | Out-Null
	if ($Unverifiable) { Set-UnverifiableLandingLease $localAppData $owner }
	$primaryBefore = (@(Invoke-ScratchGit $primary @('rev-parse', 'HEAD')))[0].Trim()
	$parameters = New-RetryLandingParameters $candidate
	if ($Unverifiable) {
		$run = Invoke-JsonScriptWithSplat $landingScript $parameters $scratchBase
		Assert-Outcome $run "$Case-omitted-token" 2 'blocked' 'landing-lock.claim-failed'
		if ($null -ne $run.Json) {
			Assert-True ($run.Json.disposition -ceq 'authority-required' -and $run.Json.requiresUserAuthority -and -not $run.Json.primaryAdvanced) "$Case unverifiable lease requires authority and leaves primary unchanged"
		}
		$status = ((@(Invoke-WorktreeCli @('lock', 'status', '--repo', $commonDirectory)) -join '') | ConvertFrom-Json -Depth 16)
		Assert-True ($status.held -eq $true -and $status.leaseState -ceq 'unverifiable') "$Case unverifiable lease remains untouched"
	}
	else {
		$started = Start-JsonScriptWithSplat $landingScript $parameters $scratchBase
		try {
			Start-Sleep -Seconds 2
			Assert-True (-not $started.Process.HasExited) "$Case foreign lease remains a bounded wait"
			Assert-True (((@(Invoke-ScratchGit $primary @('rev-parse', 'HEAD')))[0].Trim()) -ceq $primaryBefore) "$Case foreign lease leaves primary unchanged"
			$status = ((@(Invoke-WorktreeCli @('lock', 'status', '--repo', $commonDirectory)) -join '') | ConvertFrom-Json -Depth 16)
			Assert-True ($status.held -eq $true -and $status.owner -ceq $owner -and $status.leaseDurationSeconds -eq $LeaseSeconds) "$Case foreign lease is not adopted and keeps its recorded duration"
		}
		finally { Stop-JsonScript $started }
	}
	Invoke-WorktreeCli @('lock', 'release', '--repo', $commonDirectory, '--owner', $owner) | Out-Null
}

# A matching retained lease with only 60 seconds recorded is not adopted by an omitted-token
# landing: refresh preserves that duration, so continuing could expire during rebase or advance.
Assert-OmittedForeignLease 'omitted-short-derived-lease' 'finalize-fixture/landing' $session $false 60

$omittedCandidate = New-RetryCandidate (New-RetryFileText $retryHead 'omitted adoption tail') 'omitted-adoption'
$omittedOwner = [guid]::NewGuid().ToString()
Invoke-WorktreeCli @('lock', 'claim', '--repo', $commonDirectory, '--owner', $omittedOwner, '--session', 'finalize-fixture/landing', '--worktree', $session, '--lease-seconds', '3600') | Out-Null
$omitted = Invoke-JsonScriptWithSplat $landingScript (New-RetryLandingParameters $omittedCandidate) $scratchBase
Assert-Outcome $omitted 'landing-adopts-omitted-token-retained-claim' 0 'landed' 'ok'
if ($null -ne $omitted.Json) {
	Assert-True ($omitted.Json.lock.claimed -and $omitted.Json.lock.claimCode -ceq 'ok' -and $omitted.Json.lock.released) 'omitted-token landing adopts and releases the derived retained claim'
	Assert-True ($omitted.Json.primaryAdvanced -and $omitted.Json.final.replacement -and ((@(Invoke-ScratchGit $primary @('rev-parse', 'HEAD')))[0].Trim()) -ceq $omitted.Json.final.commit) 'omitted-token adoption lands the deterministic final commit'
}

# A matching retained lease whose full 3600-second duration is almost spent must be refreshed
# before a primary race sends landing through its rebase. Poll the scratch metadata while the child
# is active and require the approved session tip when the extension is observed, so the fixture
# proves the refresh happened before the rebase rather than only during its later retry.
$nearExpiryCandidate = New-RetryCandidate (New-RetryFileText $retryHead 'omitted near-expiry tail') 'omitted-near-expiry'
$nearExpiryUpstream = Add-UpstreamPrimaryCommit (New-RetryFileText 'omitted near-expiry upstream' 'omitted adoption tail')
$nearExpiryOwner = [guid]::NewGuid().ToString()
Invoke-WorktreeCli @('lock', 'claim', '--repo', $commonDirectory, '--owner', $nearExpiryOwner, '--session', 'finalize-fixture/landing', '--worktree', $session, '--lease-seconds', '3600') | Out-Null
$nearExpiryLease = Set-NearExpiryLandingLease $localAppData $nearExpiryOwner 30000
$nearExpiryStarted = Start-JsonScriptWithSplat $landingScript (New-RetryLandingParameters $nearExpiryCandidate) $scratchBase
$nearExpiryRefreshed = $false
$nearExpiryCompleted = $false
$nearExpiryExitCode = $null
try {
	$nearExpiryDeadline = [DateTime]::UtcNow.AddSeconds(60)
	while ([DateTime]::UtcNow -lt $nearExpiryDeadline) {
		if (Test-Path -LiteralPath $nearExpiryLease.Path) {
			try {
				$metadata = Get-Content -LiteralPath $nearExpiryLease.Path -Raw | ConvertFrom-Json -Depth 16 -ErrorAction Stop
				$sessionTip = (@(Invoke-ScratchGit $session @('rev-parse', "refs/heads/$sessionBranch")))[0].Trim()
				if ($metadata.owner -ceq $nearExpiryOwner -and $sessionTip -ceq $nearExpiryCandidate.Commit -and ([DateTimeOffset]::Parse($metadata.expiresAt, [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::AssumeUniversal) -gt $nearExpiryLease.ExpiresAt.AddSeconds(60))) {
					$nearExpiryRefreshed = $true
				}
			}
			catch { }
		}
		if ($nearExpiryStarted.Process.WaitForExit(0)) {
			$nearExpiryCompleted = $true
			break
		}
		Start-Sleep -Milliseconds 25
	}
	if (-not $nearExpiryCompleted -and $nearExpiryStarted.Process.WaitForExit(0)) { $nearExpiryCompleted = $true }
	if ($nearExpiryCompleted) {
		$nearExpiryExitCode = $nearExpiryStarted.Process.ExitCode
	}
}
finally { Stop-JsonScript $nearExpiryStarted }
Assert-True $nearExpiryRefreshed 'omitted-token adoption refreshes a near-expiry matching lease before rebase work'
$nearExpiryHead = (@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()
$nearExpiryParent = (@(Invoke-ScratchGit $primary @('rev-parse', "$nearExpiryHead^")))[0].Trim()
Assert-True ($nearExpiryCompleted -and $nearExpiryExitCode -eq 0 -and $nearExpiryHead -cne $nearExpiryCandidate.Commit -and $nearExpiryParent -ceq $nearExpiryUpstream) 'near-expiry omitted-token adoption lands after the primary race'
$retryHead = 'omitted near-expiry upstream'
$retryTail = 'omitted near-expiry tail'
try { Invoke-WorktreeCli @('lock', 'release', '--repo', $commonDirectory, '--owner', $nearExpiryOwner) | Out-Null } catch { }

Assert-OmittedForeignLease 'omitted-raw-session' 'finalize-fixture' $session
Assert-OmittedForeignLease 'omitted-foreign-session' 'foreign-fixture' $session
Assert-OmittedForeignLease 'omitted-foreign-worktree' 'finalize-fixture/landing' $primary
Assert-OmittedForeignLease 'omitted-unverifiable' 'finalize-fixture/landing' $session $true

$identicalTail = 'identical retry tail'
$identicalCandidate = New-RetryCandidate (New-RetryFileText $retryHead $identicalTail) 'identical-retry'
$retryHead = 'upstream identical head'
$identicalUpstream = Add-UpstreamPrimaryCommit (New-RetryFileText $retryHead $retryTail)
$identicalRetry = Invoke-JsonScriptWithSplat $landingScript (New-RetryLandingParameters $identicalCandidate) $scratchBase
Assert-Outcome $identicalRetry 'landing-retries-identical-patch' 0 'landed' 'ok'
$retryTail = $identicalTail
if ($null -ne $identicalRetry.Json) {
	$rebasedTip = (@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()
	Assert-True ($identicalRetry.Json.landed.rebaseAttempts -eq 1 -and $identicalRetry.Json.landed.commit -cne $identicalCandidate.Commit -and $identicalRetry.Json.landed.commit -ceq $rebasedTip) 'one internal rebase lands the rebased commit instead of the stale candidate'
	Assert-True ($identicalRetry.Json.landed.tree -ceq ((@(Invoke-ScratchGit $primary @('rev-parse',"$rebasedTip^{tree}")))[0].Trim())) 'the landed tree is the rebased commit tree'
	Assert-True (((@(Invoke-ScratchGit $primary @('rev-parse',"$rebasedTip^")))[0].Trim()) -ceq $identicalUpstream) 'the rebased tip descends from the upstream commit that advanced primary'
	Assert-True (([IO.File]::ReadAllText((Join-Path $primary 'retry-file.cpp'), [Text.UTF8Encoding]::new($false,$true))) -ceq (New-RetryFileText $retryHead $retryTail)) 'the landed primary content carries both the upstream and the confirmed change'
	Assert-True ([string]::IsNullOrWhiteSpace((@(Invoke-ScratchGit $session @('status','--porcelain=v1','-z','--untracked-files=all')) -join '')) -and (Test-SessionRebaseMarkersAbsent)) 'the internal rebase leaves the session worktree clean with no rebase markers'
}

# A crash after an internally rebased advance is rerun with the same original approved inputs: the
# landing must recognize its own rebased commit on primary and report the landed outcome again.
$rebasedRecoveryTip = (@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()
$rebasedRecovery = Invoke-JsonScriptWithSplat $landingScript (New-RetryLandingParameters $identicalCandidate) $scratchBase
Assert-Outcome $rebasedRecovery 'landing-recovers-internally-rebased-advance' 0 'landed' 'ok'
if ($null -ne $rebasedRecovery.Json) {
	Assert-True ($rebasedRecovery.Json.landed.commit -ceq $rebasedRecoveryTip -and $rebasedRecovery.Json.candidate.treeVerified) 'rebased-advance recovery reports the deterministic replacement as landed'
	Assert-True ($rebasedRecovery.Json.primaryAdvanced -and ((@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()) -ceq $rebasedRecoveryTip) 'rebased-advance recovery leaves primary unchanged'
}

# The crashed invocation's own landing lease is still live when the recovery rerun succeeds, so that
# rerun must release it instead of leaving every other session waiting out the full lease.
$recoveryLeaseOwner = [guid]::NewGuid().ToString()
$recoveryLeaseClaim = Invoke-JsonScript $lockClaimScript (@('-WorktreeCliExecutable', (Join-Path $primaryOutput 'WorktreeCli.exe'), '-GitCommonDirectory', $commonDirectory, '-SessionLabel', 'finalize-fixture', '-Worktree', $session, '-LandingOwner', $recoveryLeaseOwner, '-LeaseSeconds', '3600'))
Assert-Outcome $recoveryLeaseClaim 'recovery-lease-claim' 0 'pass' 'ok'
$recoveryLeaseParameters = New-RetryLandingParameters $identicalCandidate
$recoveryLeaseParameters.OwnerToken = $recoveryLeaseOwner
$recoveryLease = Invoke-JsonScriptWithSplat $landingScript $recoveryLeaseParameters $scratchBase
Assert-Outcome $recoveryLease 'landing-recovery-releases-caller-lease' 0 'landed' 'ok'
if ($null -ne $recoveryLease.Json) {
	Assert-True ($recoveryLease.Json.landed.commit -ceq $rebasedRecoveryTip -and $recoveryLease.Json.primaryAdvanced) 'recovery under the caller lease reports the already-landed tip'
	Assert-True ($recoveryLease.Json.lock.claimed -and $recoveryLease.Json.lock.released) 'recovery under the caller lease releases that same-actor lease'
}
$recoveryReclaimOwner = [guid]::NewGuid().ToString()
$recoveryReclaim = Invoke-JsonScript $lockClaimScript (@('-WorktreeCliExecutable', (Join-Path $primaryOutput 'WorktreeCli.exe'), '-GitCommonDirectory', $commonDirectory, '-SessionLabel', 'finalize-fixture', '-Worktree', $session, '-LandingOwner', $recoveryReclaimOwner, '-LeaseSeconds', '60', '-WaitSeconds', '1', '-PollMilliseconds', '50'))
Assert-Outcome $recoveryReclaim 'landing-lock-free-after-recovery' 0 'pass' 'ok'
Invoke-WorktreeCli @('lock', 'release', '--repo', $commonDirectory, '--owner', $recoveryReclaimOwner) | Out-Null

$mismatchCandidate = New-RetryCandidate (New-RetryFileText $retryHead 'mismatch tail') 'patch-mismatch'
$retryHead = 'upstream mismatch head'
[void] (Add-UpstreamPrimaryCommit (New-RetryFileText $retryHead $retryTail))
$mismatchPrimaryBefore = (@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()
$mismatchParameters = New-RetryLandingParameters $mismatchCandidate
$mismatchParameters.FixtureFailure = 'retry-patch-mismatch'
$mismatch = Invoke-JsonScriptWithSplat $landingScript $mismatchParameters $scratchBase
Assert-Outcome $mismatch 'landing-aborts-non-identical-rebase' 2 'blocked' 'rebase.patch-not-identical'
if ($null -ne $mismatch.Json) {
	Assert-True (-not $mismatch.Json.primaryAdvanced -and ((@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()) -ceq $mismatchPrimaryBefore) 'a non-identical rebase leaves the primary ref byte-identical'
	Assert-True ($mismatch.Json.landed.rebaseAttempts -eq 1 -and $null -eq $mismatch.Json.landed.commit) 'a non-identical rebase reports its attempt and lands nothing'
	Assert-True (((@(Invoke-ScratchGit $session @('rev-parse',"refs/heads/$sessionBranch")))[0].Trim()) -ceq $mismatchCandidate.Commit -and ((@(Invoke-ScratchGit $session @('rev-parse','HEAD')))[0].Trim()) -ceq $mismatchCandidate.Commit) 'a non-identical rebase restores the confirmed session commit'
}

# A real rebase followed by a lost compare-and-swap exhausts the single retry with the session branch
# already moved, so the rerun the retryable result documents needs the confirmed commit restored.
$exhaustedCandidate = New-RetryCandidate (New-RetryFileText $retryHead 'retry exhausted tail') 'retry-exhausted'
$retryHead = 'upstream exhausted head'
[void] (Add-UpstreamPrimaryCommit (New-RetryFileText $retryHead $retryTail))
$exhaustedPrimaryBefore = (@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()
$exhaustedParameters = New-RetryLandingParameters $exhaustedCandidate
$exhaustedParameters.FixtureFailure = 'compare-and-swap'
$exhausted = Invoke-JsonScriptWithSplat $landingScript $exhaustedParameters $scratchBase
Assert-Outcome $exhausted 'landing-restores-branch-on-retry-exhaustion' 2 'blocked' 'landing.retry-exhausted'
if ($null -ne $exhausted.Json) {
	Assert-True ($exhausted.Json.disposition -ceq 'retryable-wait' -and $exhausted.Json.landed.rebaseAttempts -eq 1 -and -not $exhausted.Json.primaryAdvanced) 'a real rebase that then loses the advance stays retryable after its one attempt'
	Assert-True (((@(Invoke-ScratchGit $session @('rev-parse',"refs/heads/$sessionBranch")))[0].Trim()) -ceq $exhaustedCandidate.Commit -and ((@(Invoke-ScratchGit $session @('rev-parse','HEAD')))[0].Trim()) -ceq $exhaustedCandidate.Commit) 'retry exhaustion after a real rebase restores the confirmed session commit'
	Assert-True (((@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()) -ceq $exhaustedPrimaryBefore -and (Test-SessionRebaseMarkersAbsent)) 'retry exhaustion after a real rebase leaves primary unchanged with no rebase markers'
}

$conflictCandidate = New-RetryCandidate (New-RetryFileText $retryHead 'conflict session tail') 'rebase-conflict'
$retryTail = 'conflict upstream tail'
[void] (Add-UpstreamPrimaryCommit (New-RetryFileText $retryHead $retryTail))
$conflictPrimaryBefore = (@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()
$conflict = Invoke-JsonScriptWithSplat $landingScript (New-RetryLandingParameters $conflictCandidate) $scratchBase
Assert-Outcome $conflict 'landing-aborts-conflicting-rebase' 2 'blocked' 'rebase.conflicted'
if ($null -ne $conflict.Json) {
	Assert-True ($conflict.Json.cleanup.worktreesClear -eq $true -and (Test-SessionRebaseMarkersAbsent)) 'a conflicting rebase is aborted and leaves no active Git markers'
	Assert-True ($conflict.Json.lock.released -and -not $conflict.Json.primaryAdvanced -and ((@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()) -ceq $conflictPrimaryBefore) 'a conflicting rebase releases the lease and leaves primary unchanged'
	Assert-True (((@(Invoke-ScratchGit $session @('rev-parse',"refs/heads/$sessionBranch")))[0].Trim()) -ceq $conflictCandidate.Commit) 'a conflicting rebase restores the confirmed session commit'
}
$reclaimOwner = [guid]::NewGuid().ToString()
$reclaim = Invoke-JsonScript $lockClaimScript (@('-WorktreeCliExecutable', (Join-Path $primaryOutput 'WorktreeCli.exe'), '-GitCommonDirectory', $commonDirectory, '-SessionLabel', 'finalize-fixture', '-Worktree', $session, '-LandingOwner', $reclaimOwner, '-LeaseSeconds', '60', '-WaitSeconds', '1', '-PollMilliseconds', '50'))
Assert-Outcome $reclaim 'landing-lock-free-after-conflict-abort' 0 'pass' 'ok'
Invoke-WorktreeCli @('lock', 'release', '--repo', $commonDirectory, '--owner', $reclaimOwner) | Out-Null

# The sanity gate re-reads a dirty primary before blocking. The landing registers its WorktreeCli
# session immediately before that gate, so waiting for this child's own ledger claim orders the
# removal after the first dirty read, and the captured retry warning proves that read was dirty.
function Test-LandingSessionRegistered([int] $ProcessId) {
	foreach ($file in @(Get-ChildItem -LiteralPath (Join-Path $localAppData 'BrokenEngineLocks') -Filter 'worktreecli-sessions-*.json' -File -Force -ErrorAction SilentlyContinue)) {
		try {
			$ledger = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json -Depth 16 -ErrorAction Stop
			if (@($ledger.sessions | Where-Object { [int]$_.pid -eq $ProcessId }).Count -ne 0) { return $true }
		}
		catch { }
	}
	return $false
}
function Measure-PrimaryDirtyRetry([string] $Text) {
	return @([regex]::Matches($Text, 'FinalizeLandingSanity: primary read dirty; re-reading \(attempt \d of 4\)\.')).Count
}

$transientCandidate = New-RetryCandidate (New-RetryFileText $retryHead 'transient dirty tail') 'transient-primary-dirty'
$transientDirtyPath = Join-Path $primary 'transient-primary-dirty.txt'
[IO.File]::WriteAllText($transientDirtyPath, 'transient dirty', [Text.UTF8Encoding]::new($false))
$transientStarted = Start-JsonScriptWithSplat $landingScript (New-RetryLandingParameters $transientCandidate) $scratchBase
$transientExitCode = $null
$transientStdout = ''
$transientStderr = ''
try {
	$transientDeadline = [DateTime]::UtcNow.AddSeconds(60)
	while ([DateTime]::UtcNow -lt $transientDeadline -and -not $transientStarted.Process.WaitForExit(0) -and -not (Test-LandingSessionRegistered $transientStarted.Process.Id)) {
		Start-Sleep -Milliseconds 25
	}
	Start-Sleep -Seconds 2
	Remove-Item -LiteralPath $transientDirtyPath -Force -ErrorAction SilentlyContinue
	if ($transientStarted.Process.WaitForExit(120000)) { $transientExitCode = $transientStarted.Process.ExitCode }
	$transientStdout = $transientStarted.StdoutTask.GetAwaiter().GetResult()
	$transientStderr = $transientStarted.StderrTask.GetAwaiter().GetResult()
}
finally { Stop-JsonScript $transientStarted }
Remove-Item -LiteralPath $transientDirtyPath -Force -ErrorAction SilentlyContinue
$transientJson = $null
try { if (-not [string]::IsNullOrWhiteSpace($transientStdout)) { $transientJson = $transientStdout.Trim() | ConvertFrom-Json -Depth 100 -ErrorAction Stop } } catch { }
$transient = [pscustomobject]@{ ExitCode = $transientExitCode; Json = $transientJson; Text = $transientStdout.Trim(); Stderr = $transientStderr }
Assert-Outcome $transient 'landing-proceeds-after-transient-primary-dirty' 0 'landed' 'ok'
Assert-True ((Measure-PrimaryDirtyRetry $transientStderr) -ge 1) 'a transient dirty primary is diagnosed and re-read instead of blocking'
if ($null -ne $transient.Json) {
	Assert-True ($transient.Json.primaryAdvanced -and $transient.Json.final.replacement -and ((@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()) -ceq $transient.Json.final.commit) 'the retried landing advances primary to the deterministic final commit'
}

$persistentCandidate = New-RetryCandidate (New-RetryFileText $retryHead 'persistent dirty tail') 'persistent-primary-dirty'
$persistentDirtyPath = Join-Path $primary 'persistent-primary-dirty.txt'
[IO.File]::WriteAllText($persistentDirtyPath, 'persistent dirty', [Text.UTF8Encoding]::new($false))
$persistent = Invoke-JsonScriptWithSplat $landingScript (New-RetryLandingParameters $persistentCandidate) $scratchBase
Assert-Outcome $persistent 'landing-blocks-persistent-primary-dirty' 2 'blocked' 'sanity.git.primary-dirty'
Assert-True ((Measure-PrimaryDirtyRetry $persistent.Stderr) -eq 3) 'a persistently dirty primary exhausts the full retry bound before blocking'
if ($null -ne $persistent.Json) {
	Assert-True (-not $persistent.Json.lock.claimed -and -not $persistent.Json.primaryAdvanced -and ((@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()) -ceq $persistentCandidate.PrimaryTip) 'a persistently dirty primary blocks before the landing lock and leaves primary unchanged'
}
Remove-Item -LiteralPath $persistentDirtyPath -Force -ErrorAction SilentlyContinue

# A foreign process holding the primary index.lock only briefly must be waited out instead of ending
# the landing. The lock is released only after the compare-and-swap advanced primary, which is the
# statement immediately before the guarded checkout, so the checkout provably failed at least once.
function Get-IndexLockWaitDiagnosticCount($Json) {
	if ($null -eq $Json) { return -1 }
	return @($Json.diagnostics.items | Where-Object { $_.code -ceq 'git.index-lock-wait' }).Count
}

$primaryIndexLockPath = Join-Path $primary '.git\index.lock'
$transientLockCandidate = New-RetryCandidate (New-RetryFileText $retryHead 'transient index lock tail') 'transient-index-lock'
[IO.File]::WriteAllText($primaryIndexLockPath, '', [Text.UTF8Encoding]::new($false))
$transientLockStarted = Start-JsonScriptWithSplat $landingScript (New-RetryLandingParameters $transientLockCandidate) $scratchBase
$transientLockExitCode = $null
$transientLockStdout = ''
$transientLockStderr = ''
try {
	$transientLockDeadline = [DateTime]::UtcNow.AddSeconds(120)
	while ([DateTime]::UtcNow -lt $transientLockDeadline -and -not $transientLockStarted.Process.WaitForExit(0) -and
		((@(Invoke-ScratchGit $primary @('rev-parse','refs/heads/main')))[0].Trim()) -ceq $transientLockCandidate.PrimaryTip) {
		Start-Sleep -Milliseconds 25
	}
	Start-Sleep -Milliseconds 1200
	Remove-Item -LiteralPath $primaryIndexLockPath -Force -ErrorAction SilentlyContinue
	if ($transientLockStarted.Process.WaitForExit(120000)) { $transientLockExitCode = $transientLockStarted.Process.ExitCode }
	$transientLockStdout = $transientLockStarted.StdoutTask.GetAwaiter().GetResult()
	$transientLockStderr = $transientLockStarted.StderrTask.GetAwaiter().GetResult()
}
finally { Stop-JsonScript $transientLockStarted }
Remove-Item -LiteralPath $primaryIndexLockPath -Force -ErrorAction SilentlyContinue
$transientLockJson = $null
try { if (-not [string]::IsNullOrWhiteSpace($transientLockStdout)) { $transientLockJson = $transientLockStdout.Trim() | ConvertFrom-Json -Depth 100 -ErrorAction Stop } } catch { }
$transientLock = [pscustomobject]@{ ExitCode = $transientLockExitCode; Json = $transientLockJson; Text = $transientLockStdout.Trim(); Stderr = $transientLockStderr }
Assert-Outcome $transientLock 'landing-waits-out-transient-primary-index-lock' 0 'landed' 'ok'
if ($null -ne $transientLock.Json) {
	Assert-True ($transientLock.Json.landed.commit -ceq $transientLock.Json.final.commit -and $transientLock.Json.final.replacement -and ((@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()) -ceq $transientLock.Json.final.commit) 'a waited-out index lock still lands the deterministic final commit on primary'
	Assert-True ((Get-IndexLockWaitDiagnosticCount $transientLock.Json) -eq 1 -and $transientLock.Json.diagnostics.totalCount -eq 1) 'a landed run that waited reports exactly one index-lock-wait diagnostic'
}

# Contention outliving the budget must still fail truthfully: the rollback restore is refused by the
# same lock, so the terminal result reports the unrestored checkout and lands nothing.
$persistentLockCandidate = New-RetryCandidate (New-RetryFileText $retryHead 'persistent index lock tail') 'persistent-index-lock'
$persistentLockPrimaryBefore = (@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()
[IO.File]::WriteAllText($primaryIndexLockPath, '', [Text.UTF8Encoding]::new($false))
$persistentLockParameters = New-RetryLandingParameters $persistentLockCandidate
$persistentLockParameters.IndexLockWaitSeconds = 1
$persistentLock = Invoke-JsonScriptWithSplat $landingScript $persistentLockParameters $scratchBase
Remove-Item -LiteralPath $primaryIndexLockPath -Force -ErrorAction SilentlyContinue
Assert-Outcome $persistentLock 'landing-fails-truthfully-on-persistent-primary-index-lock' 1 'error' 'git.rollback-failed'
if ($null -ne $persistentLock.Json) {
	Assert-True ($persistentLock.Json.primaryAdvanced -and $null -eq $persistentLock.Json.landed.commit -and $null -eq $persistentLock.Json.landed.tree) 'an exhausted index-lock wait reports the advance truthfully and lands nothing'
	Assert-True ((Get-IndexLockWaitDiagnosticCount $persistentLock.Json) -eq 1) 'an exhausted index-lock wait reports its one index-lock-wait diagnostic'
}
Invoke-ScratchGit $primary @('reset','--hard',$persistentLockPrimaryBefore) | Out-Null
Invoke-ScratchGit $primary @('clean','-fd') | Out-Null

# The wait is for index.lock contention alone: a checkout git refuses for any other reason stays a
# first-attempt failure, so an unwritable tracked file is never retried.
$openFileCandidate = New-RetryCandidate (New-RetryFileText $retryHead 'open file tail') 'open-file-checkout'
$openFilePrimaryBefore = (@(Invoke-ScratchGit $primary @('rev-parse','HEAD')))[0].Trim()
$openFileHandle = [IO.FileStream]::new((Join-Path $primary 'retry-file.cpp'), [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
try { $openFile = Invoke-JsonScriptWithSplat $landingScript (New-RetryLandingParameters $openFileCandidate) $scratchBase }
finally { $openFileHandle.Dispose() }
Assert-True ($null -ne $openFile.Json) 'open-file checkout failure emitted JSON'
if ($null -ne $openFile.Json) {
	Assert-True ($openFile.ExitCode -ne 0 -and @('error','blocked') -ccontains $openFile.Json.status) 'a checkout failure git does not attribute to index.lock still fails the landing'
	Assert-True ((Get-IndexLockWaitDiagnosticCount $openFile.Json) -eq 0) 'a non-lock checkout failure reports no index-lock-wait diagnostic'
}
Invoke-ScratchGit $primary @('reset','--hard',$openFilePrimaryBefore) | Out-Null
Invoke-ScratchGit $primary @('clean','-fd') | Out-Null

Write-Host ''
if ($script:Failures.Count -gt 0) {
	Write-Host "Finalize workflow fixtures FAILED ($($script:Failures.Count) assertion(s))."
	$fixtureExitCode = 1
}
else {
	Write-Host 'Finalize workflow fixtures passed.'
}
}
finally {
	if ($null -ne $fixtureEnvironment) {
		foreach ($entry in $fixtureEnvironment.GetEnumerator()) { [Environment]::SetEnvironmentVariable($entry.Key, $previousEnvironment[$entry.Key]) }
	}
	$validatedScratch = Assert-SafeScratchRoot $scratchParent $scratchBase $scratchLeaf
	if (Test-Path -LiteralPath $validatedScratch) {
		Remove-Item -LiteralPath $validatedScratch -Recurse -Force -Confirm:$false
	}
}
exit $fixtureExitCode
