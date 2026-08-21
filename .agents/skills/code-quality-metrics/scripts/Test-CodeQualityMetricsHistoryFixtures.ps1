[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$RepositoryRoot,
    [switch]$RunCapture
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-Fixture([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "History fixture failed: $Message" }
}
function Read-StrictUtf8([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    Assert-Fixture ($bytes.Length -gt 0) "$Path is empty."
    Assert-Fixture ($bytes[0] -ne 0xEF -or $bytes[1] -ne 0xBB -or $bytes[2] -ne 0xBF) "$Path has a UTF-8 BOM."
    [Text.UTF8Encoding]::new($false, $true).GetString($bytes)
}
function Test-PrefixAndSuffix([string]$Root) {
    $path = Join-Path $Root '.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl'
    $bytes = [IO.File]::ReadAllBytes($path)
    Assert-Fixture ($bytes.Length -eq 133323) 'tracked JSONL is not the immutable 133323-byte prefix.'
    Assert-Fixture (((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()) -eq '5a39debf4be41abebd8496b9f25ee4023d109813788e95b30da8f74474fe75ed') 'tracked JSONL prefix digest changed.'
    Assert-Fixture ((@($bytes | Where-Object { $_ -eq 10 })).Count -eq 648) 'tracked JSONL LF count changed.'
    $rows = @((Read-StrictUtf8 $path) -split "`n")
    Assert-Fixture ($rows.Count -eq 649 -and $rows[-1] -eq '') 'tracked JSONL does not contain exactly 648 LF-terminated lines.'
    Assert-Fixture (-not (Test-Path -LiteralPath (Join-Path $Root '.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.png'))) 'legacy PNG still exists.'
}
function Test-ClassifierAndExclusions([string]$Root) {
    $scriptText = Get-Content -Raw (Join-Path $Root '.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1')
    Assert-Fixture ($scriptText -match "MetricExtensions = @\('\.h', '\.cpp'\)") 'classifier is broader than .h/.cpp.'
    Assert-Fixture ($scriptText -match 'ShaderLayouts\.h' -and $scriptText -match 'ShaderLayoutsBase\.h') 'ShaderLayouts exception is absent.'
    Assert-Fixture ($scriptText -match 'Data.*Shaders' -and $scriptText -match 'Test-MetricPath') 'pure GLSL exclusion is absent.'
    Assert-Fixture ($scriptText -match 'captureMode = if \(\$History\.Suffix\.Count -eq 0\) \{ ''catch-up'' \}') 'catch-up decision is absent.'
    Assert-Fixture ($scriptText -match "'carry-forward'") 'carry-forward decision is absent.'
}
function Test-SchemasAndDeterminism([string]$Root) {
    $historyScript = Get-Content -Raw (Join-Path $Root '.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1')
    $metricsScript = Get-Content -Raw (Join-Path $Root '.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetrics.ps1')
    Assert-Fixture ($historyScript -match 'broken-engine-code-quality-history-contract/v1') 'history Contract schema is absent.'
    Assert-Fixture ($historyScript -match 'broken-engine-code-quality-history-update/v1') 'history Generate schema is absent.'
    Assert-Fixture ($metricsScript -match 'broken-engine-code-quality-bootstrap-identity/v1') 'BootstrapIdentity schema is absent.'
    Assert-Fixture ($historyScript -match '1800' -and $historyScript -match '1150' -and $historyScript -match 'encoding="UTF-8"') 'fixed SVG contract is absent.'
    Assert-Fixture ($historyScript -notmatch 'DateTime\.Now|Get-Date|New-Guid') 'history generator contains volatile time or random metadata.'
    Assert-Fixture ($historyScript -match 'Get-CanonicalJsonSha256' -and $historyScript -match 'Get-BytesSha256') 'series/generator digest wiring is absent.'
}
function Test-GeneratedOutput([string]$Root) {
    $temp = Join-Path $Root 'Temp'
    if (-not (Test-Path -LiteralPath $temp -PathType Container)) { return }
    foreach ($directory in @(Get-ChildItem -LiteralPath $temp -Directory -Force -ErrorAction SilentlyContinue | Where-Object Name -like 'history-fixture-*')) {
        $jsonl = Join-Path $directory.FullName 'CodeQualityMetricsHistory.jsonl'
        $svg = Join-Path $directory.FullName 'CodeQualityMetricsHistory.svg'
        if (-not (Test-Path -LiteralPath $jsonl) -or -not (Test-Path -LiteralPath $svg)) { continue }
        $jsonText = Read-StrictUtf8 $jsonl
        $jsonBytes = [IO.File]::ReadAllBytes($jsonl)
        Assert-Fixture $jsonText.EndsWith("`n") "$jsonl does not end with LF."
        $jsonRows = @($jsonText -split "`n" | Where-Object { $_ })
        Assert-Fixture ($jsonRows.Count -ge 649) "$jsonl does not contain the immutable prefix plus a live row."
        $svgXml = [xml](Read-StrictUtf8 $svg)
        Assert-Fixture ($svgXml.svg.width -eq '1800' -and $svgXml.svg.height -eq '1150') "$svg dimensions are not fixed."
        Assert-Fixture ($svgXml.svg.viewBox -eq '0 0 1800 1150') "$svg viewBox is not fixed."
        Assert-Fixture ((Get-Content -Raw $svg) -notmatch 'C:\\Users|202[0-9]-[0-9]{2}-[0-9]{2}T') "$svg contains an absolute or timestamped value."
    }
}
function Test-BootstrapIdentityNoAnalyzer([string]$Root) {
    $scriptText = Get-Content -Raw (Join-Path $Root '.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetrics.ps1')
    $branchStart = $scriptText.IndexOf("if (`$Mode -eq 'BootstrapIdentity')")
    $branchEnd = $scriptText.IndexOf("else {", $branchStart)
    Assert-Fixture ($branchStart -ge 0 -and $branchEnd -gt $branchStart) 'BootstrapIdentity branch is not separate from analyzer execution.'
    $branch = $scriptText.Substring($branchStart, $branchEnd - $branchStart)
    Assert-Fixture ($branch -notmatch 'Analyze-CodeQualityMetrics\.py|analyzerProcess') 'BootstrapIdentity branch invokes the analyzer.'
    Assert-Fixture ($branch -notmatch 'RepositoryRoot|captureRoot|analyzerSource') 'BootstrapIdentity output branch persists an absolute path.'
}
function Test-CarryForwardZeroRuntime([string]$Root) {
    $scriptText = Get-Content -Raw (Join-Path $Root '.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1')
    Assert-Fixture ($scriptText -match 'snapshotEvidence = \[ordered\]@\{ verbosity = \[double\]\$last\.verbosity') 'carry-forward value copy is not present.'
    Assert-Fixture ($scriptText -match 'captureMode = if \(\$History\.Suffix\.Count -eq 0\)') 'carry-forward decision is not guarded by the live suffix.'
}
function Invoke-FixtureChild([string]$FixtureRoot, [string[]]$Arguments) {
    Push-Location $FixtureRoot
    try { $output = @(& pwsh -NoProfile -File '.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1' @Arguments 2>&1); $exitCode = $LASTEXITCODE }
    finally { Pop-Location }
    return [pscustomobject]@{ ExitCode = $exitCode; Text = ($output -join "`n") }
}
function Test-CarryDateContainmentAndRepeat([string]$Root) {
    $fixtureRoot = Join-Path $Root ("Temp/history-fixture-runtime-$([guid]::NewGuid().ToString('N'))")
    New-Item -ItemType Directory -Force -Path (Join-Path $fixtureRoot '.agents/skills/code-quality-metrics/scripts') | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $fixtureRoot '.agents/skills/code-quality-metrics/references/history') | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $fixtureRoot 'Temp') | Out-Null
    Copy-Item (Join-Path $Root '.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1') (Join-Path $fixtureRoot '.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1')
    $source = Join-Path $Root '.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl'
    $sourceRows = @(Read-StrictUtf8 $source) -split "`n"
    $last = $sourceRows[-2] | ConvertFrom-Json
    $live = [ordered]@{ index = 647; date = '2026-08-20'; captureMode = 'catch-up'; verbosity = [double]$last.verbosity; structuralErosion = [double]$last.structuralErosion; supported = [int]$last.supported; parsed = [int]$last.parsed }
    $fixtureHistory = Join-Path $fixtureRoot '.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl'
    $prefix = [IO.File]::ReadAllBytes($source); $rowBytes = [Text.UTF8Encoding]::new($false).GetBytes(($live | ConvertTo-Json -Compress) + "`n")
    [IO.File]::WriteAllBytes($fixtureHistory, [byte[]]($prefix + $rowBytes))
    git -C $fixtureRoot init -q
    git -C $fixtureRoot config user.email fixture@example.invalid
    git -C $fixtureRoot config user.name Fixture
    git -C $fixtureRoot add .
    git -C $fixtureRoot commit -qm fixture
    $contract = Invoke-FixtureChild $fixtureRoot @('-Mode', 'Contract', '-RepositoryRoot', $fixtureRoot)
    Assert-Fixture ($contract.ExitCode -eq 0) "carry Contract failed: $($contract.Text)"
    $contractJson = $contract.Text.Trim() | ConvertFrom-Json -Depth 64
    Assert-Fixture ($contractJson.decision.captureMode -eq 'carry-forward' -and $null -eq $contractJson.capture) 'carry Contract did not avoid runtime capture.'
    $first = Invoke-FixtureChild $fixtureRoot @('-Mode', 'Generate', '-RepositoryRoot', $fixtureRoot, '-DateUtc', '2026-08-21', '-OutputDirectory', 'Temp/out-a')
    $firstJson = $first.Text.Trim() | ConvertFrom-Json -Depth 64
    Assert-Fixture ($first.ExitCode -eq 0 -and $firstJson.series.row.captureMode -eq 'carry-forward' -and $null -eq $firstJson.series.coverage) 'carry Generate emitted a capture or coverage.'
    [IO.File]::WriteAllBytes($fixtureHistory, [IO.File]::ReadAllBytes((Join-Path $fixtureRoot 'Temp/out-a/CodeQualityMetricsHistory.jsonl')))
    $second = Invoke-FixtureChild $fixtureRoot @('-Mode', 'Generate', '-RepositoryRoot', $fixtureRoot, '-DateUtc', '2026-08-21', '-OutputDirectory', 'Temp/out-b')
    Assert-Fixture ($second.ExitCode -eq 0) 'same-date consecutive carry Generate did not succeed.'
    $secondJson = $second.Text.Trim() | ConvertFrom-Json -Depth 64
    Assert-Fixture ($firstJson.series.row.index -eq 648 -and $secondJson.series.row.index -eq 649 -and $firstJson.series.row.date -eq $secondJson.series.row.date) 'same-date Generate points are not consecutive and date-stable.'
    $firstSvg = Read-StrictUtf8 (Join-Path $fixtureRoot 'Temp/out-a/CodeQualityMetricsHistory.svg'); $secondSvg = Read-StrictUtf8 (Join-Path $fixtureRoot 'Temp/out-b/CodeQualityMetricsHistory.svg')
    $firstPoints = [regex]::Match($firstSvg, 'id="series-verbosity"[^>]*points="([^"]+)"').Groups[1].Value.Split(' ', [StringSplitOptions]::RemoveEmptyEntries)
    $secondPoints = [regex]::Match($secondSvg, 'id="series-verbosity"[^>]*points="([^"]+)"').Groups[1].Value.Split(' ', [StringSplitOptions]::RemoveEmptyEntries)
    Assert-Fixture ($firstPoints.Count -eq 649 -and $secondPoints.Count -eq 650) 'same-date SVG point counts do not include both consecutive rows.'
    $outside = Join-Path $Root 'history-fixture-outside'
    $contained = Invoke-FixtureChild $fixtureRoot @('-Mode', 'Generate', '-RepositoryRoot', $fixtureRoot, '-DateUtc', '2026-08-22', '-OutputDirectory', $outside)
    Assert-Fixture ($contained.ExitCode -eq 2) 'outside-Temp output was not rejected.'
}
function Test-BaseCommitHistoryBinding([string]$Root) {
    $fixtureRoot = Join-Path $Root ("Temp/history-fixture-base-$([guid]::NewGuid().ToString('N'))")
    New-Item -ItemType Directory -Force -Path (Join-Path $fixtureRoot '.agents/skills/code-quality-metrics/scripts'), (Join-Path $fixtureRoot '.agents/skills/code-quality-metrics/references/history'), (Join-Path $fixtureRoot 'Temp') | Out-Null
    Copy-Item (Join-Path $Root '.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1') (Join-Path $fixtureRoot '.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1')
    $source = Join-Path $Root '.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl'
    $sourceBytes = [IO.File]::ReadAllBytes($source); $sourceRows = @(Read-StrictUtf8 $source) -split "`n"; $last = $sourceRows[-2] | ConvertFrom-Json
    $baseRow = [ordered]@{ index = 647; date = '2026-08-20'; captureMode = 'catch-up'; verbosity = [double]$last.verbosity; structuralErosion = [double]$last.structuralErosion; supported = [int]$last.supported; parsed = [int]$last.parsed }
    $baseRowBytes = [Text.UTF8Encoding]::new($false).GetBytes(($baseRow | ConvertTo-Json -Compress) + "`n")
    $historyPath = Join-Path $fixtureRoot '.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl'; [IO.File]::WriteAllBytes($historyPath, [byte[]]($sourceBytes + $baseRowBytes))
    git -C $fixtureRoot init -q; git -C $fixtureRoot config user.email fixture@example.invalid; git -C $fixtureRoot config user.name Fixture; git -C $fixtureRoot add .; git -C $fixtureRoot commit -qm fixture
    $base = (git -C $fixtureRoot rev-parse HEAD).Trim(); $workingRow = [ordered]@{ index = 648; date = '2026-08-21'; captureMode = 'carry-forward'; verbosity = [double]$last.verbosity; structuralErosion = [double]$last.structuralErosion; supported = [int]$last.supported; parsed = [int]$last.parsed }; $workingRowBytes = [Text.UTF8Encoding]::new($false).GetBytes(($workingRow | ConvertTo-Json -Compress) + "`n"); [IO.File]::WriteAllBytes($historyPath, [byte[]]($sourceBytes + $baseRowBytes + $workingRowBytes))
    $contract = Invoke-FixtureChild $fixtureRoot @('-Mode', 'Contract', '-RepositoryRoot', $fixtureRoot, '-BaseCommit', $base, '-TipCommit', $base)
    Assert-Fixture ($contract.ExitCode -eq 0) "BaseCommit Contract failed: $($contract.Text)"; $json = $contract.Text.Trim() | ConvertFrom-Json -Depth 64; $baseBytes = [IO.File]::ReadAllBytes($historyPath); $baseBytes = [byte[]]($baseBytes[0..($baseBytes.Length - $workingRowBytes.Length - 1)]); $expectedHash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($baseBytes)).ToLowerInvariant()
    Assert-Fixture ($json.series.lastIndex -eq 647 -and $json.series.lastDate -eq '2026-08-20' -and $json.series.historyBytesSha256 -eq $expectedHash) 'Contract did not bind to the exact BaseCommit history bytes.'
}
function Test-CaptureRaceAndScbGuards([string]$Root) {
    $bootstrap = Get-Content -Raw (Join-Path $Root '.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetrics.ps1')
    $history = Get-Content -Raw (Join-Path $Root '.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1')
    Assert-Fixture ($bootstrap -match 'Test-BootstrapEnvironment' -and $bootstrap -match 'Mutex' -and $bootstrap -match 'requirements\.lock') 'bootstrap race/cache identity guards are absent.'
    Assert-Fixture ($history -match 'gitlinkCommit' -and $history -match 'resolvedHead' -and $history -match 'status.*ignored=matching') 'scb identity/dirty guard is absent.'
    Assert-Fixture ($history -match 'Test-PycachePath' -and $history -match 'reparse entry' -and $history -match 'untracked or ignored') 'scb extra-entry policy is absent.'
    Assert-Fixture ($history -match 'Bootstrap identity drifted during Snapshot' -and $history -match 'capture manifest drifted during Snapshot') 'runtime/scb drift checks are absent.'
}

try {
    $root = [IO.Path]::GetFullPath($RepositoryRoot)
    Test-PrefixAndSuffix $root
    Test-ClassifierAndExclusions $root
    Test-SchemasAndDeterminism $root
    Test-BootstrapIdentityNoAnalyzer $root
    Test-CarryForwardZeroRuntime $root
    Test-CarryDateContainmentAndRepeat $root
    Test-BaseCommitHistoryBinding $root
    Test-CaptureRaceAndScbGuards $root
    Test-GeneratedOutput $root
    if ($RunCapture) {
        $output = @(& pwsh -NoProfile -File '.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1' -Mode Contract -RepositoryRoot $root 2>&1)
        if ($LASTEXITCODE -ne 0) { throw ($output -join "`n") }
        $contract = (($output -join "`n").Trim() | ConvertFrom-Json -Depth 64)
        Assert-Fixture ($contract.schemaVersion -eq 'broken-engine-code-quality-history-contract/v1') 'Contract invocation returned the wrong schema.'
        Assert-Fixture ($contract.prefix.bytes -eq 133323 -and $contract.prefix.lines -eq 648) 'Contract prefix evidence is wrong.'
    }
    [Console]::Out.WriteLine('{"schemaVersion":"broken-engine-code-quality-history-fixtures/v1","status":"pass"}')
}
catch { [Console]::Error.WriteLine("CodeQualityMetricsHistoryFixtures: $($_.Exception.Message)"); exit 2 }
