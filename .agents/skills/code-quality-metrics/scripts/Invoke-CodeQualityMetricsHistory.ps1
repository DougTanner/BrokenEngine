[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Contract', 'Generate')]
    [string]$Mode,
    [Parameter(Mandatory = $true)]
    [string]$RepositoryRoot,
    [string]$BaseCommit,
    [string]$TipCommit,
    [string]$DateUtc,
    [string]$OutputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:PrefixBytes = 133323
$script:PrefixLines = 648
$script:PrefixSha256 = '5a39debf4be41abebd8496b9f25ee4023d109813788e95b30da8f74474fe75ed'
$script:HistoryRelativePath = '.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl'
$script:MetricExtensions = @('.h', '.cpp')
$script:ExcludedRoots = @('ThirdParty', '.agents', '.claude', 'Temp')

function Write-Diagnostic([string]$Message) { [Console]::Error.WriteLine("CodeQualityMetricsHistory: $Message") }
function Fail([string]$Message) { Write-Diagnostic $Message; exit 2 }
function Get-BytesSha256([byte[]]$Bytes) { [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($Bytes)).ToLowerInvariant() }
function Get-FileSha256([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Get-CanonicalJson([object]$Value) { $Value | ConvertTo-Json -Compress -Depth 64 }
function Get-CanonicalJsonSha256([object]$Value) {
    Get-BytesSha256 ([Text.UTF8Encoding]::new($false).GetBytes((Get-CanonicalJson $Value)))
}
function Get-CanonicalPath([string]$Path) {
    $full = [IO.Path]::GetFullPath($Path)
    $root = [IO.Path]::GetPathRoot($full)
    if ($full.Length -gt $root.Length) { return $full.TrimEnd('\', '/') }
    return $full
}
function Get-RelativePosix([string]$Root, [string]$Path) {
    $rootFull = Get-CanonicalPath $Root
    $pathFull = Get-CanonicalPath $Path
    if ($pathFull -ne $rootFull -and -not $pathFull.StartsWith($rootFull + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Output path escapes RepositoryRoot.' }
    return ([IO.Path]::GetRelativePath($rootFull, $pathFull) -replace '\\', '/')
}
function Assert-OrdinaryDirectory([string]$Path, [string]$Description) {
    $item = Get-Item -LiteralPath $Path -Force -ErrorAction Stop
    if (-not $item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "$Description must be an ordinary directory." }
}
function Invoke-Git([string]$Repository, [string[]]$Arguments) {
    $lines = @(& git -C $Repository @Arguments 2>&1)
    if ($LASTEXITCODE -ne 0) {
        $detail = ($lines -join "`n").Trim()
        if (-not $detail) { $detail = 'git command failed.' }
        throw $detail
    }
    return $lines
}
function Invoke-GitBytes([string]$Repository, [string[]]$Arguments) {
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = 'git.exe'
    $start.WorkingDirectory = $Repository
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($argument in (@('-C', $Repository) + $Arguments)) { [void]$start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $start
    if (-not $process.Start()) { throw 'Could not start git.exe.' }
    $bytes = [IO.MemoryStream]::new()
    try {
        $process.StandardOutput.BaseStream.CopyTo($bytes)
        $stderr = $process.StandardError.ReadToEnd()
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) {
            $detail = $stderr.Trim()
            if (-not $detail) { $detail = 'git command failed.' }
            throw $detail
        }
        return $bytes.ToArray()
    }
    finally {
        $bytes.Dispose()
        $process.Dispose()
    }
}
function Get-GitSha([string]$Repository, [string]$Revision) {
    $value = ((Invoke-Git $Repository @('rev-parse', '--verify', "$Revision^{commit}") | Select-Object -First 1).ToString()).Trim()
    if ($value -notmatch '^[0-9a-f]{40}$') { throw "Git returned an invalid commit identity for '$Revision'." }
    return $value
}
function Assert-CommitObject([string]$Sha, [string]$Name) {
    if ($Sha -notmatch '^[0-9a-f]{40}$') { throw "$Name must be a 40-character lowercase hexadecimal commit SHA." }
}
function Test-MetricPath([string]$Path) {
    $normalized = $Path -replace '\\', '/'
    $parts = $normalized.Split('/')
    if ($parts.Count -eq 0 -or $script:ExcludedRoots -contains $parts[0]) { return $false }
    $extension = [IO.Path]::GetExtension($normalized).ToLowerInvariant()
    if ($script:MetricExtensions -notcontains $extension) { return $false }
    for ($index = 0; $index -lt $parts.Count - 1; ++$index) {
        if ($parts[$index] -eq 'Data' -and $parts[$index + 1] -eq 'Shaders' -and
            $parts[-1] -notin @('ShaderLayouts.h', 'ShaderLayoutsBase.h')) { return $false }
    }
    return $true
}
function Test-PycachePath([string]$Path) {
    return $Path -match '(^|/)__pycache__(/|$)'
}
function Get-ExplicitUtcDate([string]$Value) {
    if (-not $Value -or $Value -notmatch '^\d{4}-\d{2}-\d{2}$') { throw 'DateUtc must be an explicit UTC date in YYYY-MM-DD form.' }
    $parsed = [datetime]::MinValue
    if (-not [datetime]::TryParseExact($Value, 'yyyy-MM-dd', [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::None, [ref]$parsed)) { throw 'DateUtc is not a valid calendar date.' }
    return $parsed.ToString('yyyy-MM-dd', [Globalization.CultureInfo]::InvariantCulture)
}
function Get-RowDateText([object]$Value) {
    if ($Value -is [datetime]) { return $Value.ToString('yyyy-MM-dd', [Globalization.CultureInfo]::InvariantCulture) }
    return [string]$Value
}
function Get-LastDate([object]$Row) {
    $dateText = Get-RowDateText $Row.date
    if ($dateText -match '^(\d{4}-\d{2}-\d{2})') { return $matches[1] }
    throw 'History row date is not an ISO date.'
}
function Assert-FiniteRange([object]$Value, [string]$Name) {
    if ($Value -is [string] -or $Value -is [bool]) { throw "$Name must be a JSON number." }
    $number = [double]$Value
    if ([double]::IsNaN($number) -or [double]::IsInfinity($number) -or $number -lt 0.0 -or $number -gt 1.0) { throw "$Name must be a finite number in [0,1]." }
}
function Assert-Count([object]$Value, [string]$Name, [int]$Maximum = [int]::MaxValue) {
    if ($Value -is [string] -or $Value -is [bool]) { throw "$Name must be a JSON integer." }
    $raw = [double]$Value
    if ([double]::IsNaN($raw) -or [double]::IsInfinity($raw) -or [Math]::Truncate($raw) -ne $raw) { throw "$Name must be a JSON integer." }
    $number = [int64]$Value
    if ($number -lt 0 -or $number -gt $Maximum) { throw "$Name must be a non-negative integer." }
    return [int]$number
}
function Assert-PropertySet([object]$Value, [string[]]$Required, [string[]]$Optional = @()) {
    if ($null -eq $Value -or $null -eq $Value.PSObject) { throw 'History row is not a JSON object.' }
    $names = @($Value.PSObject.Properties.Name)
    foreach ($name in $Required) { if ($names -notcontains $name) { throw "History row is missing '$name'." } }
    foreach ($name in $names) { if ($Required -notcontains $name -and $Optional -notcontains $name) { throw "History row contains unexpected field '$name'." } }
}
function Read-History([string]$Repository, [AllowNull()][string]$Base) {
    $path = Join-Path $Repository ($script:HistoryRelativePath -replace '/', '\')
    if ([string]::IsNullOrWhiteSpace($Base)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "History JSONL is missing: $script:HistoryRelativePath." }
        $bytes = [IO.File]::ReadAllBytes($path)
    }
    else {
        Assert-CommitObject $Base 'BaseCommit'
        $bytes = Invoke-GitBytes $Repository @('cat-file', 'blob', ($Base + ':' + $script:HistoryRelativePath))
    }
    if ($bytes.Length -lt $script:PrefixBytes) { throw 'History JSONL is shorter than the immutable prefix.' }
    $prefix = [byte[]]$bytes[0..($script:PrefixBytes - 1)]
    if ($prefix.Length -ne $script:PrefixBytes -or (Get-BytesSha256 $prefix) -ne $script:PrefixSha256) { throw 'History JSONL immutable prefix does not match the approved byte contract.' }
    if (($bytes | Where-Object { $_ -eq 10 }).Count -lt $script:PrefixLines) { throw 'History JSONL has fewer LF lines than the immutable prefix.' }
    $encoding = [Text.UTF8Encoding]::new($false, $true)
    try { $text = $encoding.GetString($bytes) } catch { throw 'History JSONL is not strict UTF-8.' }
    if ($text.StartsWith([char]0xFEFF)) { throw 'History JSONL must not contain a UTF-8 BOM.' }
    if (-not $text.EndsWith("`n")) { throw 'History JSONL must end with LF.' }
    $lines = @($text -split "`n")
    if ($lines[-1] -ne '') { throw 'History JSONL line splitting failed.' }
    $lines = $lines[0..($lines.Count - 2)]
    if ($lines.Count -lt $script:PrefixLines) { throw 'History JSONL has fewer lines than the immutable prefix.' }
    $header = $lines[0].TrimEnd("`r") | ConvertFrom-Json
    if ($header.schema -ne 'code-quality-metrics-history/v1') { throw 'History JSONL header schema is not code-quality-metrics-history/v1.' }
    $legacy = [Collections.Generic.List[object]]::new()
    $allLegacy = [Collections.Generic.List[object]]::new()
    for ($index = 1; $index -lt $script:PrefixLines; ++$index) {
        $row = $lines[$index].TrimEnd("`r") | ConvertFrom-Json
        Assert-PropertySet $row @('index', 'sha', 'date', 'cppChanging') @('verbosity', 'structuralErosion', 'supported', 'parsed')
        if ((Assert-Count $row.index "legacy[$($index - 1)].index") -ne ($index - 1)) { throw "Legacy history index is not contiguous at row $($index - 1)." }
        if ([string]$row.sha -notmatch '^[0-9a-f]{40}$') { throw "Legacy history SHA is invalid at index $($index - 1)." }
        if ($row.cppChanging -isnot [bool]) { throw "Legacy cppChanging must be a JSON boolean at index $($index - 1)." }
        $allLegacy.Add($row)
        if ([bool]$row.cppChanging -and (-not (Test-MetricPropertySet $row))) { throw "Legacy metric values are missing at index $($index - 1)." }
        if (-not [bool]$row.cppChanging) { continue }
        Assert-FiniteRange $row.verbosity "legacy[$($index - 1)].verbosity"
        Assert-FiniteRange $row.structuralErosion "legacy[$($index - 1)].structuralErosion"
        [void](Assert-Count $row.supported "legacy[$($index - 1)].supported")
        [void](Assert-Count $row.parsed "legacy[$($index - 1)].parsed" ([int]$row.supported))
        $legacy.Add($row)
    }
    $suffix = [Collections.Generic.List[object]]::new()
    for ($lineIndex = $script:PrefixLines; $lineIndex -lt $lines.Count; ++$lineIndex) {
        $row = $lines[$lineIndex].TrimEnd("`r") | ConvertFrom-Json
        Assert-PropertySet $row @('index', 'date', 'captureMode', 'verbosity', 'structuralErosion', 'supported', 'parsed')
        $expectedIndex = $script:PrefixLines - 1 + $suffix.Count
        if ((Assert-Count $row.index "live[$expectedIndex].index") -ne $expectedIndex) { throw "Live history index is not contiguous at row $expectedIndex." }
        $rowDate = Get-RowDateText $row.date
        if ($rowDate -notmatch '^\d{4}-\d{2}-\d{2}$') { throw "Live history date is not YYYY-MM-DD at index $expectedIndex." }
        [void](Get-ExplicitUtcDate $rowDate)
        if ([string]$row.captureMode -notin @('catch-up', 'cpp-change', 'carry-forward')) { throw "Live history captureMode is invalid at index $expectedIndex." }
        if ($suffix.Count -gt 0 -and [string]$row.captureMode -eq 'catch-up') { throw 'Catch-up is allowed only for the first live history row.' }
        Assert-FiniteRange $row.verbosity "live[$expectedIndex].verbosity"
        Assert-FiniteRange $row.structuralErosion "live[$expectedIndex].structuralErosion"
        $supported = Assert-Count $row.supported "live[$expectedIndex].supported"
        [void](Assert-Count $row.parsed "live[$expectedIndex].parsed" $supported)
        if ($suffix.Count -eq 0 -and [string]$row.captureMode -ne 'catch-up') { throw 'The first live history row must be the one-time catch-up point.' }
        if ($suffix.Count -gt 0 -and (Get-LastDate $row) -lt (Get-LastDate $suffix[$suffix.Count - 1])) { throw "Live history dates must be nondecreasing at index $expectedIndex." }
        $suffix.Add($row)
    }
    return [pscustomobject]@{
        Path = $path
        Bytes = $bytes
        PrefixBytes = $prefix
        Header = $header
        Legacy = $legacy
        Suffix = $suffix
        Rows = @($allLegacy + $suffix)
        PrefixEvidence = [ordered]@{ bytes = $script:PrefixBytes; lines = $script:PrefixLines; sha256 = $script:PrefixSha256 }
        HistoryBytesSha256 = Get-BytesSha256 $bytes
    }
}
function Test-MetricPropertySet([object]$Row) {
    if ($null -eq $Row -or $null -eq $Row.PSObject) { return $false }
    $names = @($Row.PSObject.Properties.Name)
    return $names -contains 'verbosity' -and $names -contains 'structuralErosion' -and $names -contains 'supported' -and $names -contains 'parsed'
}
function Get-NameStatusRecords([string]$Repository, [string]$Base, [string]$Tip) {
    $records = [Collections.Generic.List[object]]::new()
    $arguments = @('diff', '--name-status', '--find-renames=50%', '--no-ext-diff')
    if ($Base -eq $Tip) { $arguments += @($Base, '--') } else { $arguments += @($Base, $Tip, '--') }
    foreach ($line in @(Invoke-Git $Repository $arguments)) {
        $text = [string]$line
        if (-not $text.Trim()) { continue }
        $parts = $text -split "`t"
        $status = $parts[0]
        if ($status -match '^[RC]\d+$') {
            if ($parts.Count -lt 3) { throw "Git rename record is malformed: $text" }
            $records.Add([pscustomobject]@{ Status = $status.Substring(0, 1); OldPath = ($parts[1] -replace '\\', '/'); Path = ($parts[2] -replace '\\', '/') })
        }
        else {
            if ($parts.Count -lt 2) { throw "Git change record is malformed: $text" }
            $records.Add([pscustomobject]@{ Status = $status.Substring(0, 1); OldPath = $null; Path = ($parts[1] -replace '\\', '/') })
        }
    }
    if ($Base -eq $Tip) {
        foreach ($line in @(Invoke-Git $Repository @('status', '--porcelain=v1', '--untracked-files=all'))) {
            $text = [string]$line
            if ($text.Length -lt 4 -or $text.Substring(0, 2) -ne '??') { continue }
            $records.Add([pscustomobject]@{ Status = 'A'; OldPath = $null; Path = ($text.Substring(3) -replace '\\', '/') })
        }
    }
    return @($records)
}
function Get-PatchEvidence([string]$Repository, [string]$Base, [string]$Tip) {
    $records = @(Get-NameStatusRecords $Repository $Base $Tip)
    $metricRecords = @($records | Where-Object { (Test-MetricPath $_.Path) -or ($_.OldPath -and (Test-MetricPath $_.OldPath)) })
    $cppChanged = $metricRecords.Count -gt 0
    $rows = [Collections.Generic.List[object]]::new()
    foreach ($record in $records) {
        $rows.Add([ordered]@{ status = $record.Status; path = $record.Path; oldPath = $record.OldPath; metricSupported = [bool]$metricRecords.Contains($record) })
    }
    return [ordered]@{ baseCommit = $Base; tipCommit = $Tip; changes = @($rows); metricSupportedChanges = $metricRecords.Count; cppChanged = $cppChanged }
}
function Get-SourceRoot([string]$Repository) {
    $sourcePath = Join-Path $Repository 'ThirdParty\scb-check\src'
    $lockPath = Join-Path $Repository 'ThirdParty\scb-check\requirements.lock'
    $sourceItem = Get-Item -LiteralPath $sourcePath -Force -ErrorAction Stop
    $lockItem = Get-Item -LiteralPath $lockPath -Force -ErrorAction Stop
    if ($sourceItem.Attributes -band [IO.FileAttributes]::ReparsePoint) { $sourceItem = $sourceItem.ResolveLinkTarget($true) }
    if ($lockItem.Attributes -band [IO.FileAttributes]::ReparsePoint) { $lockItem = $lockItem.ResolveLinkTarget($true) }
    if ($null -eq $sourceItem -or $null -eq $lockItem -or -not ($sourceItem.Attributes -band [IO.FileAttributes]::Directory) -or ($lockItem.Attributes -band [IO.FileAttributes]::Directory)) { throw 'Provisioned scb-check source or requirements.lock is invalid.' }
    return [pscustomobject]@{ Root = (Split-Path -Parent $sourceItem.FullName); Source = $sourceItem.FullName; Lock = $lockItem.FullName }
}
function Get-CaptureManifest([string]$Repository) {
    $source = Get-SourceRoot $Repository
    $externalRoot = Get-CanonicalPath ((Invoke-Git $source.Root @('rev-parse', '--show-toplevel') | Select-Object -First 1).ToString().Trim())
    $resolvedHead = (Invoke-Git $externalRoot @('rev-parse', 'HEAD') | Select-Object -First 1).ToString().Trim().ToLowerInvariant()
    Assert-CommitObject $resolvedHead 'Resolved scb-check HEAD'
    $gitlinkLine = @(Invoke-Git $Repository @('ls-tree', 'HEAD', 'ThirdParty/scb-check')) | Select-Object -First 1
    if (-not $gitlinkLine) { throw 'Repository HEAD has no ThirdParty/scb-check gitlink.' }
    $gitlinkParts = ([string]$gitlinkLine) -split "`t"
    $gitlinkMeta = $gitlinkParts[0] -split ' '
    if ($gitlinkMeta.Count -lt 3 -or $gitlinkMeta[0] -ne '160000' -or $gitlinkMeta[1] -ne 'commit') { throw 'ThirdParty/scb-check is not a commit gitlink.' }
    $gitlinkCommit = $gitlinkMeta[2].ToLowerInvariant()
    Assert-CommitObject $gitlinkCommit 'scb-check gitlink'
    if ($resolvedHead -ne $gitlinkCommit) { throw 'Resolved scb-check HEAD does not match the repository gitlink.' }

    $tracked = [Collections.Generic.List[object]]::new()
    $trackedNames = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($line in @(Invoke-Git $externalRoot @('ls-files', '--stage', '--', 'requirements.lock', 'src'))) {
        $text = [string]$line
        if (-not $text.Trim()) { continue }
        $parts = $text -split "`t", 2
        if ($parts.Count -ne 2) { throw "scb-check membership record is malformed: $text" }
        $meta = $parts[0] -split ' '
        $relative = ($parts[1] -replace '\\', '/')
        if ($meta.Count -lt 3 -or $meta[2] -ne '0') { throw "scb-check membership has a non-stage-zero entry: $relative" }
        if ($relative -ne 'requirements.lock' -and -not $relative.StartsWith('src/', [StringComparison]::Ordinal)) { continue }
        if (Test-PycachePath $relative) { throw "Tracked scb-check __pycache__ entry is not consumable: $relative" }
        if ($meta[0] -notin @('100644', '100755')) { throw "scb-check membership is not an ordinary file: $relative" }
        $physical = if ($relative -eq 'requirements.lock') { $source.Lock } else { Join-Path $externalRoot ($relative -replace '/', '\') }
        $item = Get-Item -LiteralPath $physical -Force -ErrorAction Stop
        if ($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "Consumed scb-check entry is not an ordinary file: $relative" }
        if (-not $trackedNames.Add($relative)) { throw "Duplicate scb-check membership: $relative" }
        $tracked.Add([ordered]@{ relativePath = $relative; gitMode = $meta[0]; type = 'file'; length = [int64]$item.Length; rawSha256 = Get-FileSha256 $physical })
    }
    if (-not $trackedNames.Contains('requirements.lock')) { throw 'scb-check requirements.lock is not tracked.' }
    if (-not ($trackedNames | Where-Object { $_.StartsWith('src/', [StringComparison]::Ordinal) })) { throw 'scb-check src has no tracked files.' }

    $stack = [Collections.Generic.Stack[string]]::new(); $stack.Push($source.Source); $observed = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    while ($stack.Count -gt 0) {
        $directory = $stack.Pop()
        foreach ($entry in @(Get-ChildItem -LiteralPath $directory -Force)) {
            if ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Consumed scb-check source contains a reparse entry: '$($entry.FullName)'." }
            $relative = [IO.Path]::GetRelativePath($externalRoot, $entry.FullName) -replace '\\', '/'
            if ($entry.PSIsContainer) {
                if ($entry.Name -eq '__pycache__') { continue }
                $stack.Push($entry.FullName)
            }
            else { [void]$observed.Add($relative) }
        }
    }
    foreach ($name in $observed) { if (-not $trackedNames.Contains($name)) { throw "Untracked or ignored scb-check source extra: $name" } }
    foreach ($name in $trackedNames) { if ($name -ne 'requirements.lock' -and -not $observed.Contains($name)) { throw "Tracked scb-check source entry is missing: $name" } }
    foreach ($statusLine in @(Invoke-Git $externalRoot @('status', '--porcelain=v1', '--ignored=matching', '--untracked-files=all'))) {
        $statusText = [string]$statusLine
        if (-not $statusText.Trim()) { continue }
        $statusPath = if ($statusText.Length -gt 3) { $statusText.Substring(3) -replace '\\', '/' } else { '' }
        if (Test-PycachePath $statusPath) { continue }
        throw "scb-check repository is dirty or has an extra entry: $statusText"
    }
    $tracked.Sort([Comparison[object]] { param($left, $right) [string]::CompareOrdinal([string]$left.relativePath, [string]$right.relativePath) })
    $manifest = [ordered]@{ gitlinkCommit = $gitlinkCommit; resolvedHead = $resolvedHead; clean = $true; entries = @($tracked) }
    return [ordered]@{ manifest = $manifest; digest = Get-CanonicalJsonSha256 $manifest }
}
function Invoke-ChildJson([string]$Repository, [string]$RelativeScript, [string[]]$Arguments) {
    $output = @()
    Push-Location $Repository
    try { $output = @(& pwsh -NoProfile -File $RelativeScript @Arguments 2>&1); $exitCode = $LASTEXITCODE }
    finally { Pop-Location }
    $text = ($output | ForEach-Object { [string]$_ }) -join "`n"
    if ($exitCode -ne 0) {
        $detail = $text.Trim()
        if (-not $detail) { $detail = "Child script failed with exit $exitCode." }
        throw $detail
    }
    try { return ($text.Trim() | ConvertFrom-Json -Depth 64) } catch { throw "Child script did not return JSON: $text" }
}
function Get-BootstrapIdentity([string]$Repository) {
    Invoke-ChildJson $Repository '.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetrics.ps1' @('-Mode', 'BootstrapIdentity', '-RepositoryRoot', $Repository)
}
function Get-Snapshot([string]$Repository) {
    Invoke-ChildJson $Repository '.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetrics.ps1' @('-Mode', 'Snapshot', '-Target', 'Engine/Source', '-Scope', 'Recursive', '-RepositoryRoot', $Repository)
}
function Get-SnapshotEvidence([object]$Report) {
    if ($Report.schemaVersion -ne 'broken-engine-code-quality-metrics/v2' -or $Report.mode -ne 'Snapshot') { throw 'Snapshot did not return the v2 Snapshot report.' }
    $metrics = $Report.current.corpusMetrics
    foreach ($name in @('verbosity', 'structuralErosion')) {
        if ($null -eq $metrics.$name) { throw "Snapshot corpusMetrics is missing $name." }
        Assert-FiniteRange $metrics.$name.value "snapshot.corpusMetrics.$name.value"
    }
    $supported = Assert-Count $Report.current.corpusCounts.supported 'snapshot.corpusCounts.supported'
    $parsed = Assert-Count $Report.current.corpusCounts.parsed 'snapshot.corpusCounts.parsed' $supported
    return [ordered]@{
        verbosity = [double]$metrics.verbosity.value
        structuralErosion = [double]$metrics.structuralErosion.value
        supported = $supported
        parsed = $parsed
        coverage = [ordered]@{ corpusCounts = $Report.current.corpusCounts; targetCounts = $Report.current.targetCounts }
    }
}
function Get-NumberText([double]$Value) { $Value.ToString('0.############', [Globalization.CultureInfo]::InvariantCulture) }
function New-LiveRow([int]$Index, [string]$Date, [string]$CaptureMode, [double]$Verbosity, [double]$Erosion, [int]$Supported, [int]$Parsed) {
    [pscustomobject][ordered]@{ index = $Index; date = $Date; captureMode = $CaptureMode; verbosity = [double](Get-NumberText $Verbosity); structuralErosion = [double](Get-NumberText $Erosion); supported = $Supported; parsed = $Parsed }
}
function Get-EffectiveRows([object[]]$Rows) {
    $effective = [Collections.Generic.List[object]]::new(); $last = $null
    foreach ($row in $Rows) {
        if (Test-MetricPropertySet $row) {
            $last = [pscustomobject][ordered]@{ index = [int]$row.index; date = Get-RowDateText $row.date; verbosity = [double]$row.verbosity; structuralErosion = [double]$row.structuralErosion; supported = [int]$row.supported; parsed = [int]$row.parsed }
        }
        elseif ($null -eq $last) { throw "History row $($row.index) has no metric values to inherit." }
        else { $last = [pscustomobject][ordered]@{ index = [int]$row.index; date = Get-RowDateText $row.date; verbosity = $last.verbosity; structuralErosion = $last.structuralErosion; supported = $last.supported; parsed = $last.parsed } }
        $effective.Add($last)
    }
    return $effective.ToArray()
}
function Get-SvgEscape([string]$Value) { [Security.SecurityElement]::Escape($Value) }
function Get-Polyline([object[]]$Rows, [string]$Name, [double]$Top, [double]$Bottom, [double]$Maximum) {
    $points = [Collections.Generic.List[string]]::new(); $count = $Rows.Count; $width = 1610.0; $left = 110.0
    for ($index = 0; $index -lt $count; ++$index) {
        $x = if ($count -eq 1) { $left } else { $left + $width * $index / ($count - 1) }
        $value = if ($Name -eq 'supported') { [double]$Rows[$index].supported / $Maximum } else { [double]$Rows[$index].$Name }
        $y = $Bottom - ($Bottom - $Top) * [Math]::Min(1.0, [Math]::Max(0.0, $value))
        $points.Add("$(Get-NumberText $x),$(Get-NumberText $y)")
    }
    return ($points -join ' ')
}
function New-HistorySvg([object[]]$Rows, [string]$GeneratorDigest, [string]$CaptureDigest, [string]$IdentityDigest, [string]$ScbDigest, [string]$SeriesDigest) {
    $maxSupported = [Math]::Max(1.0, [double](($Rows | Measure-Object -Property supported -Maximum).Maximum))
    $lines = [Collections.Generic.List[string]]::new()
    $lines.Add('<?xml version="1.0" encoding="UTF-8"?>')
    $lines.Add('<svg xmlns="http://www.w3.org/2000/svg" width="1800" height="1150" viewBox="0 0 1800 1150">')
    $lines.Add('<title>Broken Engine code-quality history</title>')
    $lines.Add("<desc>seriesDigest=$SeriesDigest generatorDigest=$GeneratorDigest captureDigest=$CaptureDigest identityDigest=$IdentityDigest scbDigest=$ScbDigest</desc>")
    $lines.Add('<rect width="1800" height="1150" fill="#10151c"/>')
    $lines.Add('<g fill="none" stroke="#384555" stroke-width="1">')
    foreach ($y in @(120, 350, 580, 810)) { $lines.Add("<line x1=`"110`" y1=`"$y`" x2=`"1720`" y2=`"$y`"/>") }
    $lines.Add('</g>')
    $lines.Add("<polyline id=`"series-verbosity`" fill=`"none`" stroke=`"#65d6ff`" stroke-width=`"3`" points=`"$(Get-Polyline $Rows 'verbosity' 120 350 1)`"/>")
    $lines.Add("<polyline id=`"series-structural-erosion`" fill=`"none`" stroke=`"#ffb454`" stroke-width=`"3`" points=`"$(Get-Polyline $Rows 'structuralErosion' 350 580 1)`"/>")
    $lines.Add("<polyline id=`"series-supported`" fill=`"none`" stroke=`"#9cf28a`" stroke-width=`"3`" points=`"$(Get-Polyline $Rows 'supported' 580 810 $maxSupported)`"/>")
    $lines.Add('<g font-family="Segoe UI, sans-serif" font-size="18" fill="#d7e0ea">')
    $lines.Add('<text x="110" y="80">Code-quality history</text>')
    $lines.Add('<text x="110" y="115">verbosity</text><text x="110" y="345">structural erosion</text><text x="110" y="575">supported files</text>')
    $step = [Math]::Max(1, [int][Math]::Ceiling($Rows.Count / 12.0)); $labelIndices = [Collections.Generic.HashSet[int]]::new()
    for ($index = 0; $index -lt $Rows.Count; $index += $step) { [void]$labelIndices.Add($index) }
    [void]$labelIndices.Add($Rows.Count - 1)
    foreach ($index in ($labelIndices | Sort-Object)) {
        $x = if ($Rows.Count -eq 1) { 110.0 } else { 110.0 + 1610.0 * $index / ($Rows.Count - 1) }
        $date = Get-SvgEscape (Get-LastDate $Rows[$index])
        $lines.Add("<text x=`"$(Get-NumberText $x)`" y=`"850`" text-anchor=`"middle`">$date</text>")
    }
    $lines.Add('</g>')
    $lines.Add('</svg>')
    return (($lines -join "`n") + "`n")
}
function Assert-UniqueOutput([string]$Repository, [string]$Path) {
    $root = Get-CanonicalPath (Join-Path $Repository 'Temp')
    $full = Get-CanonicalPath $Path
    if ($full -eq $root -or -not $full.StartsWith($root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'OutputDirectory must be a unique directory beneath RepositoryRoot/Temp.' }
    $parent = Split-Path -Parent $full
    Assert-OrdinaryDirectory $parent 'OutputDirectory parent'
    if (Test-Path -LiteralPath $full) { throw "OutputDirectory already exists and is never overwritten: '$full'." }
    return $full
}
function Get-Plan([string]$Repository, [object]$History, [string]$Base, [string]$Tip) {
    $patch = Get-PatchEvidence $Repository $base $tip
    $captureMode = if ($History.Suffix.Count -eq 0) { 'catch-up' } elseif ($patch.cppChanged) { 'cpp-change' } else { 'carry-forward' }
    $generatorPath = Join-Path $Repository '.agents\skills\code-quality-metrics\scripts\Invoke-CodeQualityMetricsHistory.ps1'
    $generatorDigest = Get-FileSha256 $generatorPath
    $capture = $null
    if ($captureMode -ne 'carry-forward') {
        $identity = Get-BootstrapIdentity $Repository
        $manifest = Get-CaptureManifest $Repository
        $captureValue = [ordered]@{ bootstrapIdentity = $identity; manifest = $manifest.manifest }
        $capture = [ordered]@{ digest = Get-CanonicalJsonSha256 $captureValue; bootstrapIdentityDigest = Get-CanonicalJsonSha256 $identity; scbContentDigest = [string]$identity.scbContentDigest; manifest = $manifest.manifest; manifestDigest = $manifest.digest }
    }
    return [ordered]@{ decision = [ordered]@{ captureMode = $captureMode; reason = if ($History.Suffix.Count -eq 0) { 'no-live-suffix' } elseif ($patch.cppChanged) { 'metric-supported-cpp-change' } else { 'no-metric-supported-cpp-change' }; forceSnapshot = $captureMode -ne 'carry-forward' }; source = [ordered]@{ baseCommit = $base; tipCommit = $tip }; patch = $patch; generator = [ordered]@{ relativePath = ' .agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1'.Trim(); sha256 = $generatorDigest }; capture = $capture }
}
function New-Contract([string]$Repository, [object]$History, [object]$Plan) {
    [ordered]@{
        schemaVersion = 'broken-engine-code-quality-history-contract/v1'
        mode = 'Contract'
        source = $Plan.source
        prefix = $History.PrefixEvidence
        series = [ordered]@{ rows = $History.Rows.Count; liveRows = $History.Suffix.Count; lastIndex = [int]$History.Rows[-1].index; lastDate = Get-LastDate $History.Rows[-1]; historyBytesSha256 = $History.HistoryBytesSha256 }
        patch = $Plan.patch
        decision = $Plan.decision
        generator = $Plan.generator
        capture = $Plan.capture
        snapshot = if ($Plan.decision.forceSnapshot) { [ordered]@{ target = 'Engine/Source'; scope = 'Recursive'; coverageRequired = $true } } else { $null }
    }
}
function Assert-DateAfterHistory([object]$History, [string]$Date) {
    if ($History.Rows.Count -gt 0 -and $Date -lt (Get-LastDate $History.Rows[-1])) { throw "DateUtc must not be earlier than the latest history date $(Get-LastDate $History.Rows[-1])." }
}
function Invoke-History([string]$Repository, [object]$History, [object]$Plan, [string]$Date, [string]$Output) {
    $outputFull = Assert-UniqueOutput $Repository $Output
    [IO.Directory]::CreateDirectory($outputFull) | Out-Null
    $identityBefore = $null; $manifestBefore = $null; $snapshotEvidence = $null
    if ($Plan.decision.forceSnapshot) {
        $identityBefore = Get-BootstrapIdentity $Repository
        $manifestBefore = Get-CaptureManifest $Repository
        $snapshot = Get-Snapshot $Repository
        $snapshotEvidence = Get-SnapshotEvidence $snapshot
        $identityAfter = Get-BootstrapIdentity $Repository
        $manifestAfter = Get-CaptureManifest $Repository
        if ((Get-CanonicalJsonSha256 $identityBefore) -ne (Get-CanonicalJsonSha256 $identityAfter)) { throw 'Bootstrap identity drifted during Snapshot.' }
        if ($manifestBefore.digest -ne $manifestAfter.digest) { throw 'scb-check capture manifest drifted during Snapshot.' }
        if ($Plan.capture.bootstrapIdentityDigest -ne (Get-CanonicalJsonSha256 $identityBefore) -or $Plan.capture.manifestDigest -ne $manifestBefore.digest) { throw 'Prepared capture identity drifted before Generate.' }
    }
    else {
        $last = $History.Rows[-1]
        $snapshotEvidence = [ordered]@{ verbosity = [double]$last.verbosity; structuralErosion = [double]$last.structuralErosion; supported = [int]$last.supported; parsed = [int]$last.parsed; coverage = $null }
    }
    $index = [int]$History.Rows[-1].index + 1
    $row = New-LiveRow $index $Date $Plan.decision.captureMode $snapshotEvidence.verbosity $snapshotEvidence.structuralErosion $snapshotEvidence.supported $snapshotEvidence.parsed
    $rowText = (Get-CanonicalJson $row) + "`n"
    $rowBytes = [Text.UTF8Encoding]::new($false).GetBytes($rowText)
    $jsonlBytes = [byte[]]($History.Bytes + $rowBytes)
    $seriesDigest = Get-BytesSha256 $jsonlBytes
    $captureDigest = if ($Plan.capture) { $Plan.capture.digest } else { $null }
    $identityDigest = if ($identityBefore) { Get-CanonicalJsonSha256 $identityBefore } else { $null }
    $scbDigest = if ($Plan.capture) { $Plan.capture.scbContentDigest } else { $null }
    $svgRows = @(Get-EffectiveRows @($History.Rows)) + @($row)
    $svgText = New-HistorySvg $svgRows $Plan.generator.sha256 $captureDigest $identityDigest $scbDigest $seriesDigest
    $svgBytes = [Text.UTF8Encoding]::new($false).GetBytes($svgText)
    $jsonlPath = Join-Path $outputFull 'CodeQualityMetricsHistory.jsonl'; $svgPath = Join-Path $outputFull 'CodeQualityMetricsHistory.svg'
    [IO.File]::WriteAllBytes($jsonlPath, $jsonlBytes); [IO.File]::WriteAllBytes($svgPath, $svgBytes)
    if (-not (Test-Path -LiteralPath $jsonlPath -PathType Leaf) -or -not (Test-Path -LiteralPath $svgPath -PathType Leaf)) { throw 'History output persistence did not produce both required files.' }
    $jsonlInfo = Get-Item -LiteralPath $jsonlPath -Force; $svgInfo = Get-Item -LiteralPath $svgPath -Force
    [ordered]@{
        schemaVersion = 'broken-engine-code-quality-history-update/v1'
        mode = 'Generate'
        date = $Date
        captureMode = $Plan.decision.captureMode
        source = $Plan.source
        prefix = $History.PrefixEvidence
        patch = $Plan.patch
        generator = $Plan.generator
        capture = $Plan.capture
        series = [ordered]@{ index = $index; digest = $seriesDigest; historyBytesSha256 = $History.HistoryBytesSha256; row = $row; coverage = $snapshotEvidence.coverage }
        outputs = [ordered]@{
            jsonl = [ordered]@{ path = Get-RelativePosix $Repository $jsonlPath; bytes = [int64]$jsonlInfo.Length; sha256 = Get-FileSha256 $jsonlPath }
            svg = [ordered]@{ path = Get-RelativePosix $Repository $svgPath; bytes = [int64]$svgInfo.Length; sha256 = Get-FileSha256 $svgPath }
        }
    }
}

try {
    if (-not $RepositoryRoot) { throw 'RepositoryRoot must be an existing directory.' }
    $repository = Get-CanonicalPath $RepositoryRoot
    Assert-OrdinaryDirectory $repository 'RepositoryRoot'
    if ($Mode -eq 'Generate' -and -not $OutputDirectory) { throw 'Generate requires a unique Temp OutputDirectory.' }
    if ($Mode -eq 'Generate') { $date = Get-ExplicitUtcDate $DateUtc } elseif ($DateUtc) { $date = Get-ExplicitUtcDate $DateUtc } else { $date = $null }
    $head = Get-GitSha $repository 'HEAD'
    $base = if ($BaseCommit) { $BaseCommit.ToLowerInvariant() } else { $head }
    $tip = if ($TipCommit) { $TipCommit.ToLowerInvariant() } else { $head }
    Assert-CommitObject $base 'BaseCommit'; Assert-CommitObject $tip 'TipCommit'
    [void](Get-GitSha $repository $base); [void](Get-GitSha $repository $tip)
    # Production always supplies BaseCommit so the source table comes from that immutable Git
    # object. The working-tree fallback is retained only for standalone fixture mode.
    $history = Read-History $repository $(if ($BaseCommit) { $base } else { $null })
    if ($date) { Assert-DateAfterHistory $history $date }
    $plan = Get-Plan $repository $history $base $tip
    if ($Mode -eq 'Contract') {
        if ($OutputDirectory) { throw 'Contract is read-only and does not accept OutputDirectory.' }
        [Console]::Out.WriteLine((Get-CanonicalJson (New-Contract $repository $history $plan)))
    }
    else {
        $outputPath = if ([IO.Path]::IsPathRooted($OutputDirectory)) { $OutputDirectory } else { Join-Path $repository $OutputDirectory }
        [Console]::Out.WriteLine((Get-CanonicalJson (Invoke-History $repository $history $plan $date $outputPath)))
    }
}
catch { Fail $_.Exception.Message }
