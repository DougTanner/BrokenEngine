[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Contract', 'Generate')]
    [string]$Mode,
    [Parameter(Mandatory = $true)]
    [string]$RepositoryRoot,
    [Parameter(Mandatory = $true)]
    [string]$BaseCommit,
    [string]$OutputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:HistoryRelativePath = '.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl'
$script:GeneratorRelativePath = '.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1'
$script:HeaderText = '{"schema":"code-quality-metrics-history/v3"}'
$script:AreaFields = @('verbosityEngine', 'verbosityCommon', 'verbosityDataPacker', 'verbosityGame', 'verbosityAgentTools')
$script:RowFields = @('index', 'sha', 'date', 'measured', 'verbosity') + $script:AreaFields + @('supported', 'parsed')
# History areas (HistoryContract.md): the first matching prefix wins, so both Agent/ folders precede Engine/ and Projects/.
$script:HistoryAreaPrefixes = @(
    @('Engine/Source/Agent/', 'verbosityAgentTools'),
    @('Projects/BrokenEngineSandbox/Source/Agent/', 'verbosityAgentTools'),
    @('Tools/', 'verbosityAgentTools'),
    @('Engine/', 'verbosityEngine'),
    @('Common/', 'verbosityCommon'),
    @('DataPacker/', 'verbosityDataPacker'),
    @('Projects/', 'verbosityGame'))
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
function Assert-CommitDate([object]$Value, [string]$Name) {
    $parsed = [DateTimeOffset]::MinValue
    if ($Value -isnot [string] -or $Value -cnotmatch '^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(Z|[+-]\d{2}:\d{2})$' -or
        -not [DateTimeOffset]::TryParse($Value, [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::None, [ref]$parsed)) { throw "$Name must be a strict ISO 8601 date with offset." }
}
function Get-LastDate([object]$Row) {
    if ([string]$Row.date -match '^(\d{4}-\d{2}-\d{2})') { return $matches[1] }
    throw 'History row date is not an ISO date.'
}
function Assert-FiniteRange([object]$Value, [string]$Name) {
    if ($null -eq $Value -or $Value -is [string] -or $Value -is [bool]) { throw "$Name must be a JSON number." }
    $number = [double]$Value
    if ([double]::IsNaN($number) -or [double]::IsInfinity($number) -or $number -lt 0.0 -or $number -gt 1.0) { throw "$Name must be a finite number in [0,1]." }
}
function Assert-Count([object]$Value, [string]$Name, [int]$Maximum = [int]::MaxValue) {
    if ($null -eq $Value -or $Value -is [string] -or $Value -is [bool]) { throw "$Name must be a JSON integer." }
    $raw = [double]$Value
    if ([double]::IsNaN($raw) -or [double]::IsInfinity($raw) -or [Math]::Truncate($raw) -ne $raw) { throw "$Name must be a JSON integer." }
    $number = [int64]$Value
    if ($number -lt 0 -or $number -gt $Maximum) { throw "$Name must be a non-negative integer." }
    return [int]$number
}
function Assert-PropertySet([object]$Value, [string[]]$Required) {
    if ($null -eq $Value -or $null -eq $Value.PSObject) { throw 'History row is not a JSON object.' }
    $names = @($Value.PSObject.Properties.Name)
    foreach ($name in $Required) { if ($names -notcontains $name) { throw "History row is missing '$name'." } }
    foreach ($name in $names) { if ($Required -notcontains $name) { throw "History row contains unexpected field '$name'." } }
}
# Validates a v3 table (a commit blob); an older table fails the exact header check.
function Read-History([byte[]]$Bytes, [string]$Name) {
    $encoding = [Text.UTF8Encoding]::new($false, $true)
    try { $text = $encoding.GetString($Bytes) } catch { throw "$Name is not strict UTF-8." }
    if ($text.StartsWith([char]0xFEFF)) { throw "$Name must not contain a UTF-8 BOM." }
    if ($text.Contains("`r")) { throw "$Name must use LF line endings." }
    if (-not $text.EndsWith("`n")) { throw "$Name must end with LF." }
    $lines = @($text.Substring(0, $text.Length - 1) -split "`n")
    if ($lines[0] -cne $script:HeaderText) { throw "$Name header is not exactly $($script:HeaderText)." }
    $rows = [Collections.Generic.List[object]]::new()
    for ($lineIndex = 1; $lineIndex -lt $lines.Count; ++$lineIndex) {
        $index = $lineIndex - 1
        $row = $lines[$lineIndex] | ConvertFrom-Json -DateKind String
        Assert-PropertySet $row $script:RowFields
        if ((Assert-Count $row.index "row[$index].index") -ne $index) { throw "$Name index is not contiguous at row $index." }
        if ($row.sha -isnot [string] -or $row.sha -cnotmatch '^[0-9a-f]{40}$') { throw "$Name sha is invalid at row $index." }
        Assert-CommitDate $row.date "$Name date at row $index"
        if ($row.measured -isnot [bool]) { throw "$Name measured must be a JSON boolean at row $index." }
        Assert-FiniteRange $row.verbosity "row[$index].verbosity"
        foreach ($field in $script:AreaFields) { if ($null -ne $row.$field) { Assert-FiniteRange $row.$field "row[$index].$field" } }
        $supported = Assert-Count $row.supported "row[$index].supported"
        [void](Assert-Count $row.parsed "row[$index].parsed" $supported)
        $rows.Add($row)
    }
    if ($rows.Count -eq 0) { throw "$Name has no rows." }
    return [pscustomobject]@{ Bytes = $Bytes; Rows = $rows.ToArray(); HistoryBytesSha256 = Get-BytesSha256 $Bytes }
}
function Read-CommitHistory([string]$Repository, [string]$Commit) {
    Read-History (Invoke-GitBytes $Repository @('cat-file', 'blob', ($Commit + ':' + $script:HistoryRelativePath))) "History JSONL at $Commit"
}
function Get-NameStatusRecords([string]$Repository, [string]$Base, [string]$Tip) {
    $records = [Collections.Generic.List[object]]::new()
    foreach ($line in @(Invoke-Git $Repository @('diff', '--name-status', '--find-renames=50%', '--no-ext-diff', $Base, $Tip, '--'))) {
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
function Get-CommitSnapshot([string]$Repository, [string]$Commit) {
    Invoke-ChildJson $Repository '.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetrics.ps1' @('-Mode', 'Snapshot', '-Target', 'Engine/Source', '-Scope', 'Recursive', '-Commit', $Commit, '-RepositoryRoot', $Repository)
}
function Get-SnapshotEvidence([object]$Report) {
    if ($Report.schemaVersion -ne 'broken-engine-code-quality-metrics/v4' -or $Report.mode -ne 'Snapshot') { throw 'Snapshot did not return the v4 Snapshot report.' }
    $verbosity = $Report.current.corpusMetrics.verbosity
    if ($null -eq $verbosity) { throw 'Snapshot corpusMetrics is missing verbosity.' }
    Assert-FiniteRange $verbosity.value 'snapshot.corpusMetrics.verbosity.value'
    $supported = Assert-Count $Report.current.corpusCounts.supported 'snapshot.corpusCounts.supported'
    $parsed = Assert-Count $Report.current.corpusCounts.parsed 'snapshot.corpusCounts.parsed' $supported
    $numerators = @{}; $denominators = @{}
    foreach ($field in $script:AreaFields) { $numerators[$field] = [int64]0; $denominators[$field] = [int64]0 }
    foreach ($file in @($Report.current.files)) {
        $path = [string]$file.path
        $match = $script:HistoryAreaPrefixes | Where-Object { $path.StartsWith($_[0], [StringComparison]::Ordinal) } | Select-Object -First 1
        if ($null -eq $match) { continue }
        $denominator = Assert-Count $file.metrics.verbosity.denominator "snapshot.files[$path].verbosity.denominator"
        $numerators[$match[1]] += Assert-Count $file.metrics.verbosity.numerator "snapshot.files[$path].verbosity.numerator" $denominator
        $denominators[$match[1]] += $denominator
    }
    $evidence = [ordered]@{ verbosity = [double]$verbosity.value }
    foreach ($field in $script:AreaFields) { $evidence[$field] = if ($denominators[$field] -eq 0) { $null } else { [double]$numerators[$field] / $denominators[$field] } }
    $evidence.supported = $supported
    $evidence.parsed = $parsed
    $evidence.coverage = [ordered]@{ corpusCounts = $Report.current.corpusCounts; targetCounts = $Report.current.targetCounts }
    return $evidence
}
function Get-NumberText([double]$Value) { $Value.ToString('0.############', [Globalization.CultureInfo]::InvariantCulture) }
# Values is Snapshot evidence or, for carry-forward, the previous row; a null history-area value stays null.
function New-Row([int]$Index, [string]$Sha, [string]$Date, [bool]$Measured, [object]$Values) {
    $row = [ordered]@{ index = $Index; sha = $Sha; date = $Date; measured = $Measured; verbosity = [double](Get-NumberText $Values.verbosity) }
    foreach ($field in $script:AreaFields) { $row[$field] = if ($null -eq $Values.$field) { $null } else { [double](Get-NumberText $Values.$field) } }
    $row.supported = [int]$Values.supported
    $row.parsed = [int]$Values.parsed
    return [pscustomobject]$row
}
function Get-RowsBytes([object[]]$Rows) {
    $builder = [Text.StringBuilder]::new()
    foreach ($row in $Rows) { [void]$builder.Append((Get-CanonicalJson $row)).Append("`n") }
    return , [Text.UTF8Encoding]::new($false).GetBytes($builder.ToString())
}
function Get-PreparedCapture([string]$Repository) {
    $identity = Get-BootstrapIdentity $Repository
    $manifest = Get-CaptureManifest $Repository
    $captureValue = [ordered]@{ bootstrapIdentity = $identity; manifest = $manifest.manifest }
    return [ordered]@{ digest = Get-CanonicalJsonSha256 $captureValue; bootstrapIdentityDigest = Get-CanonicalJsonSha256 $identity; scbContentDigest = [string]$identity.scbContentDigest; manifest = $manifest.manifest; manifestDigest = $manifest.digest }
}
# One commit Snapshot inside the BootstrapIdentity/manifest window, checked against the capture prepared once per run.
function Measure-Commit([string]$Repository, [string]$Sha, [object]$Capture) {
    $identityBefore = Get-BootstrapIdentity $Repository
    $manifestBefore = Get-CaptureManifest $Repository
    if ($Capture.bootstrapIdentityDigest -ne (Get-CanonicalJsonSha256 $identityBefore) -or $Capture.manifestDigest -ne $manifestBefore.digest) { throw "Prepared capture identity drifted before the Snapshot of $Sha." }
    $evidence = Get-SnapshotEvidence (Get-CommitSnapshot $Repository $Sha)
    $identityAfter = Get-BootstrapIdentity $Repository
    $manifestAfter = Get-CaptureManifest $Repository
    if ((Get-CanonicalJsonSha256 $identityBefore) -ne (Get-CanonicalJsonSha256 $identityAfter)) { throw "Bootstrap identity drifted during the Snapshot of $Sha." }
    if ($manifestBefore.digest -ne $manifestAfter.digest) { throw "scb-check capture manifest drifted during the Snapshot of $Sha." }
    return $evidence
}
function Get-CommitPlan([string]$Repository, [string]$Comparison, [string]$Sha, [string]$Date) {
    $patch = Get-PatchEvidence $Repository $Comparison $Sha
    return [pscustomobject]@{ Sha = $Sha; Date = $Date; CaptureMode = $(if ($patch.cppChanged) { 'cpp-change' } else { 'carry-forward' }); Patch = $patch }
}
# Generate's per-row step: carry the previous values or measure.
function New-CommitRow([string]$Repository, [int]$Index, [object]$Plan, [object]$Previous, [object]$Capture) {
    if ($Plan.CaptureMode -eq 'carry-forward') { return [pscustomobject]@{ Row = (New-Row $Index $Plan.Sha $Plan.Date $false $Previous); Coverage = $null } }
    $evidence = Measure-Commit $Repository $Plan.Sha $Capture
    return [pscustomobject]@{ Row = (New-Row $Index $Plan.Sha $Plan.Date $true $evidence); Coverage = $evidence.coverage }
}
function Get-SvgEscape([string]$Value) { [Security.SecurityElement]::Escape($Value) }
function Get-Polyline([object[]]$Rows, [string]$Name, [double]$Top, [double]$Bottom, [double]$Low, [double]$High) {
    $points = [Collections.Generic.List[string]]::new(); $count = $Rows.Count; $width = 3440.0; $left = 300.0
    for ($index = 0; $index -lt $count; ++$index) {
        $value = $Rows[$index].$Name
        if ($null -eq $value) { continue }
        $x = if ($count -eq 1) { $left } else { $left + $width * $index / ($count - 1) }
        $y = $Bottom - ($Bottom - $Top) * ([double]$value - $Low) / ($High - $Low)
        $points.Add("$(Get-NumberText $x),$(Get-NumberText $y)")
    }
    return ($points -join ' ')
}
$script:HistorySeries = @(
    @{ Id = 'series-verbosity'; Name = 'verbosity'; Title = 'verbosity: overall (all code)'; Color = '#65d6ff' },
    @{ Id = 'series-verbosity-engine'; Name = 'verbosityEngine'; Title = 'verbosity: Engine'; Color = '#ffb454' },
    @{ Id = 'series-verbosity-common'; Name = 'verbosityCommon'; Title = 'verbosity: Common'; Color = '#9be564' },
    @{ Id = 'series-verbosity-data-packer'; Name = 'verbosityDataPacker'; Title = 'verbosity: DataPacker'; Color = '#c792ea' },
    @{ Id = 'series-verbosity-game'; Name = 'verbosityGame'; Title = 'verbosity: Game'; Color = '#ff6b81' },
    @{ Id = 'series-verbosity-agent-tools'; Name = 'verbosityAgentTools'; Title = 'verbosity: AgentTools'; Color = '#f2e85c' })
# One y range over every non-null value of all six series, so every band and the overlay share one scale.
function Get-HistoryRange([object[]]$Rows) {
    $values = @(foreach ($row in $Rows) { foreach ($series in $script:HistorySeries) { $row.($series.Name) } })
    $range = $values | Where-Object { $null -ne $_ } | Measure-Object -Minimum -Maximum
    $minimum = [double]$range.Minimum; $maximum = [double]$range.Maximum; $margin = 0.05 * ($maximum - $minimum)
    return [pscustomobject]@{ Minimum = $minimum; Maximum = $maximum; Low = $minimum - $margin; High = $maximum + $margin }
}
function Get-SvgHeader([string]$Title, [string]$SeriesDigest) {
    @('<?xml version="1.0" encoding="UTF-8"?>',
        '<svg xmlns="http://www.w3.org/2000/svg" width="3840" height="2160" viewBox="0 0 3840 2160">',
        "<title>$Title</title>",
        "<desc>seriesDigest=$SeriesDigest</desc>",
        '<rect width="3840" height="2160" fill="#10151c"/>')
}
# Five grid lines and value labels for one plot area.
function Get-ValueAxis([object]$Range, [double]$Top, [double]$Bottom) {
    $grid = [Collections.Generic.List[string]]::new(); $labels = [Collections.Generic.List[string]]::new()
    for ($k = 0; $k -le 4; ++$k) {
        $value = ($Range.Minimum * (4 - $k) + $Range.Maximum * $k) / 4.0
        $y = Get-NumberText ($Bottom - ($Bottom - $Top) * ($value - $Range.Low) / ($Range.High - $Range.Low))
        $grid.Add("<line x1=`"300`" y1=`"$y`" x2=`"3740`" y2=`"$y`"/>")
        $labels.Add("<text x=`"280`" y=`"$y`" text-anchor=`"end`" dominant-baseline=`"middle`">$($value.ToString('0.0000', [Globalization.CultureInfo]::InvariantCulture))</text>")
    }
    return [pscustomobject]@{ Grid = $grid.ToArray(); Labels = $labels.ToArray() }
}
function Get-DateLabels([object[]]$Rows, [double]$Y) {
    $step = [Math]::Max(1, [int][Math]::Ceiling($Rows.Count / 12.0)); $labelIndices = [Collections.Generic.HashSet[int]]::new()
    for ($index = 0; $index -lt $Rows.Count; $index += $step) { [void]$labelIndices.Add($index) }
    [void]$labelIndices.Add($Rows.Count - 1)
    foreach ($index in ($labelIndices | Sort-Object)) {
        $x = if ($Rows.Count -eq 1) { 300.0 } else { 300.0 + 3440.0 * $index / ($Rows.Count - 1) }
        $date = Get-SvgEscape (Get-LastDate $Rows[$index])
        "<text x=`"$(Get-NumberText $x)`" y=`"$(Get-NumberText $Y)`" text-anchor=`"middle`">$date</text>"
    }
}
function New-HistorySvg([object[]]$Rows, [string]$SeriesDigest) {
    $series = $script:HistorySeries
    # Six 230-unit bands separated by 86-unit title gaps fill the plot area y 230-2040.
    $bands = @(for ($k = 0; $k -lt $series.Count; ++$k) { $series[$k] + @{ Top = 230.0 + 316.0 * $k; Bottom = 460.0 + 316.0 * $k } })
    $range = Get-HistoryRange $Rows
    $lines = [Collections.Generic.List[string]]::new()
    $lines.AddRange([string[]](Get-SvgHeader 'Broken Engine code-quality history' $SeriesDigest))
    $lines.Add('<g fill="none" stroke="#384555" stroke-width="2">')
    foreach ($y in @($bands | ForEach-Object { $_.Top; $_.Bottom })) { $lines.Add("<line x1=`"300`" y1=`"$y`" x2=`"3740`" y2=`"$y`"/>") }
    $lines.Add('</g>')
    foreach ($band in $bands) {
        $axis = Get-ValueAxis $range $band.Top $band.Bottom
        $lines.Add('<g fill="none" stroke="#222b36" stroke-width="2">'); $lines.AddRange($axis.Grid); $lines.Add('</g>')
        $lines.Add("<polyline id=`"$($band.Id)`" fill=`"none`" stroke=`"$($band.Color)`" stroke-width=`"6`" points=`"$(Get-Polyline $Rows $band.Name $band.Top $band.Bottom $range.Low $range.High)`"/>")
        $lines.Add('<g font-family="Segoe UI, sans-serif" font-size="32" fill="#d7e0ea">')
        $lines.Add("<text x=`"300`" y=`"$(Get-NumberText ($band.Top - 24))`" font-size=`"40`">$($band.Title)</text>")
        $lines.AddRange($axis.Labels); $lines.Add('</g>')
    }
    $lines.Add('<g font-family="Segoe UI, sans-serif" font-size="32" fill="#d7e0ea">')
    $lines.Add('<text x="300" y="110" font-size="48">Code-quality history</text>')
    $lines.AddRange([string[]](Get-DateLabels $Rows 2100))
    $lines.Add('</g>')
    $lines.Add('</svg>')
    return (($lines -join "`n") + "`n")
}
# All six series in one plot area y 230-1880, date labels below it, and a legend along the bottom.
function New-HistoryOverlaySvg([object[]]$Rows, [string]$SeriesDigest) {
    $series = $script:HistorySeries
    $top = 230.0; $bottom = 1880.0
    $range = Get-HistoryRange $Rows
    $axis = Get-ValueAxis $range $top $bottom
    $lines = [Collections.Generic.List[string]]::new()
    $lines.AddRange([string[]](Get-SvgHeader 'Broken Engine code-quality history overlay' $SeriesDigest))
    $lines.Add('<g fill="none" stroke="#384555" stroke-width="2">')
    foreach ($y in @($top, $bottom)) { $lines.Add("<line x1=`"300`" y1=`"$y`" x2=`"3740`" y2=`"$y`"/>") }
    $lines.Add('</g>')
    $lines.Add('<g fill="none" stroke="#222b36" stroke-width="2">'); $lines.AddRange($axis.Grid); $lines.Add('</g>')
    foreach ($entry in $series) {
        $lines.Add("<polyline id=`"$($entry.Id)`" fill=`"none`" stroke=`"$($entry.Color)`" stroke-width=`"6`" points=`"$(Get-Polyline $Rows $entry.Name $top $bottom $range.Low $range.High)`"/>")
    }
    $lines.Add('<g font-family="Segoe UI, sans-serif" font-size="32" fill="#d7e0ea">')
    $lines.Add('<text x="300" y="110" font-size="48">Code-quality history</text>')
    $lines.AddRange($axis.Labels)
    $lines.AddRange([string[]](Get-DateLabels $Rows 1940))
    # One 573-unit legend slot per series across the 3440-unit plot width.
    for ($k = 0; $k -lt $series.Count; ++$k) {
        $x = 300.0 + 573.0 * $k
        $lines.Add("<rect x=`"$(Get-NumberText $x)`" y=`"2064`" width=`"60`" height=`"12`" fill=`"$($series[$k].Color)`"/>")
        $lines.Add("<text x=`"$(Get-NumberText ($x + 76))`" y=`"2070`" dominant-baseline=`"middle`">$($series[$k].Title)</text>")
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
# One row per landing, Base's, compared with the newest row's commit because carry-forward copies that row. Archive
# rows and commits squashed off main do not resolve here; the empty tree is then the comparison, so the row is measured.
function Get-GeneratePlans([string]$Repository, [object]$History, [string]$Base) {
    $date = (Invoke-Git $Repository @('log', '-1', '--format=%cI', $Base) | Select-Object -First 1).ToString().Trim()
    Assert-CommitDate $date "Committer date of $Base"
    $comparison = [string]$History.Rows[-1].sha
    & git -C $Repository cat-file -e "$comparison^{commit}" 2>$null
    if ($LASTEXITCODE -ne 0) { $comparison = '4b825dc642cb6eb9a060e54bf8d69288fbee4904' }
    return Get-CommitPlan $Repository $comparison $Base $date
}
function Get-Generator([string]$Repository) {
    [ordered]@{ relativePath = $script:GeneratorRelativePath; sha256 = Get-FileSha256 (Join-Path $Repository ($script:GeneratorRelativePath -replace '/', '\')) }
}
function Write-HistoryPair([string]$Repository, [string]$OutputFull, [byte[]]$JsonlBytes, [object[]]$Rows) {
    $seriesDigest = Get-BytesSha256 $JsonlBytes
    $svgBytes = [Text.UTF8Encoding]::new($false).GetBytes((New-HistorySvg $Rows $seriesDigest))
    $overlayBytes = [Text.UTF8Encoding]::new($false).GetBytes((New-HistoryOverlaySvg $Rows $seriesDigest))
    $jsonlPath = Join-Path $OutputFull 'CodeQualityMetricsHistory.jsonl'; $svgPath = Join-Path $OutputFull 'CodeQualityMetricsHistory.svg'
    $overlayPath = Join-Path $OutputFull 'CodeQualityMetricsHistoryOverlay.svg'
    [IO.File]::WriteAllBytes($jsonlPath, $JsonlBytes); [IO.File]::WriteAllBytes($svgPath, $svgBytes); [IO.File]::WriteAllBytes($overlayPath, $overlayBytes)
    if (@($jsonlPath, $svgPath, $overlayPath | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) }).Count -gt 0) { throw 'History output persistence did not produce all three required files.' }
    $jsonlInfo = Get-Item -LiteralPath $jsonlPath -Force; $svgInfo = Get-Item -LiteralPath $svgPath -Force; $overlayInfo = Get-Item -LiteralPath $overlayPath -Force
    return [pscustomobject]@{
        Digest = $seriesDigest
        Outputs = [ordered]@{
            jsonl = [ordered]@{ path = Get-RelativePosix $Repository $jsonlPath; bytes = [int64]$jsonlInfo.Length; sha256 = Get-FileSha256 $jsonlPath }
            svg = [ordered]@{ path = Get-RelativePosix $Repository $svgPath; bytes = [int64]$svgInfo.Length; sha256 = Get-FileSha256 $svgPath }
            overlaySvg = [ordered]@{ path = Get-RelativePosix $Repository $overlayPath; bytes = [int64]$overlayInfo.Length; sha256 = Get-FileSha256 $overlayPath }
        }
    }
}
function New-Contract([object]$History, [string]$Base, [object[]]$Plans, [object]$Generator, [object]$Capture) {
    $last = $History.Rows[-1]
    [ordered]@{
        schemaVersion = 'broken-engine-code-quality-history-contract/v3'
        mode = 'Contract'
        source = [ordered]@{ baseCommit = $Base }
        series = [ordered]@{ rows = $History.Rows.Count; lastIndex = [int]$last.index; lastDate = [string]$last.date; historyBytesSha256 = $History.HistoryBytesSha256 }
        rows = @($Plans | ForEach-Object { [ordered]@{ sha = $_.Sha; captureMode = $_.CaptureMode; patch = $_.Patch } })
        generator = $Generator
        capture = $Capture
        snapshot = if ($null -ne $Capture) { [ordered]@{ target = 'Engine/Source'; scope = 'Recursive'; coverageRequired = $true } } else { $null }
    }
}
function Invoke-Generate([string]$Repository, [object]$History, [string]$Base, [object[]]$Plans, [object]$Generator, [string]$Output) {
    $outputFull = Assert-UniqueOutput $Repository $Output
    $capture = if (@($Plans | Where-Object { $_.CaptureMode -eq 'cpp-change' }).Count -gt 0) { Get-PreparedCapture $Repository } else { $null }
    [IO.Directory]::CreateDirectory($outputFull) | Out-Null
    $previous = $History.Rows[-1]
    $appended = [Collections.Generic.List[object]]::new(); $entries = [Collections.Generic.List[object]]::new()
    foreach ($plan in $Plans) {
        $step = New-CommitRow $Repository ([int]$previous.index + 1) $plan $previous $capture
        $appended.Add($step.Row)
        $entries.Add([ordered]@{ sha = $plan.Sha; captureMode = $plan.CaptureMode; patch = $plan.Patch; row = $step.Row; coverage = $step.Coverage })
        $previous = $step.Row
    }
    $jsonlBytes = [byte[]]($History.Bytes + (Get-RowsBytes $appended.ToArray()))
    $pair = Write-HistoryPair $Repository $outputFull $jsonlBytes (@($History.Rows) + $appended.ToArray())
    [ordered]@{
        schemaVersion = 'broken-engine-code-quality-history-update/v4'
        mode = 'Generate'
        source = [ordered]@{ baseCommit = $Base }
        generator = $Generator
        capture = $capture
        rows = $entries.ToArray()
        series = [ordered]@{ digest = $pair.Digest; historyBytesSha256 = $History.HistoryBytesSha256 }
        outputs = $pair.Outputs
    }
}
function Get-RepositoryPath([string]$Repository, [string]$Path) {
    if ([IO.Path]::IsPathRooted($Path)) { return $Path }
    return (Join-Path $Repository $Path)
}

try {
    if (-not $RepositoryRoot) { throw 'RepositoryRoot must be an existing directory.' }
    $repository = Get-CanonicalPath $RepositoryRoot
    Assert-OrdinaryDirectory $repository 'RepositoryRoot'
    if ($Mode -eq 'Contract' -and $OutputDirectory) { throw 'Contract is read-only and does not accept OutputDirectory.' }
    if ($Mode -eq 'Generate' -and -not $OutputDirectory) { throw 'Generate requires a unique Temp OutputDirectory.' }
    $base = $BaseCommit.ToLowerInvariant()
    Assert-CommitObject $base 'BaseCommit'
    [void](Get-GitSha $repository $base)
    $history = Read-CommitHistory $repository $base
    $plans = @(Get-GeneratePlans $repository $history $base)
    $generator = Get-Generator $repository
    if ($Mode -eq 'Contract') {
        $capture = if (@($plans | Where-Object { $_.CaptureMode -eq 'cpp-change' }).Count -gt 0) { Get-PreparedCapture $repository } else { $null }
        $receipt = New-Contract $history $base $plans $generator $capture
    }
    else { $receipt = Invoke-Generate $repository $history $base $plans $generator (Get-RepositoryPath $repository $OutputDirectory) }
    [Console]::Out.WriteLine((Get-CanonicalJson $receipt))
}
catch { Fail $_.Exception.Message }
