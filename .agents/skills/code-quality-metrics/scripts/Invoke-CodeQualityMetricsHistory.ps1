[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Contract', 'Generate', 'Rebuild')]
    [string]$Mode,
    [Parameter(Mandatory = $true)]
    [string]$RepositoryRoot,
    [Parameter(Mandatory = $true)]
    [string]$BaseCommit,
    [string]$SeedPath,
    [string]$OutputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:HistoryRelativePath = '.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl'
$script:GeneratorRelativePath = '.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1'
$script:HeaderText = '{"schema":"code-quality-metrics-history/v2"}'
$script:RowFields = @('index', 'sha', 'date', 'measured', 'verbosity', 'structuralErosion', 'supported', 'parsed')
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
# Validates a v2 table (a commit blob or a seed file); a v1 table fails the exact header check.
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
        Assert-FiniteRange $row.structuralErosion "row[$index].structuralErosion"
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
# Oldest first: the repository root, then each first-parent commit through Base, with its committer date.
function Get-FirstParentChain([string]$Repository, [string]$Base) {
    $chain = [Collections.Generic.List[object]]::new()
    foreach ($line in @(Invoke-Git $Repository @('log', '--first-parent', '--reverse', '--format=%H%x09%cI', $Base))) {
        $parts = ([string]$line) -split "`t"
        if ($parts.Count -ne 2 -or $parts[0] -cnotmatch '^[0-9a-f]{40}$') { throw "Git first-parent record is malformed: $line" }
        Assert-CommitDate $parts[1] "Committer date of $($parts[0])"
        $chain.Add([pscustomobject]@{ Sha = $parts[0]; Date = $parts[1] })
    }
    return $chain.ToArray()
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
    if ($Report.schemaVersion -ne 'broken-engine-code-quality-metrics/v3' -or $Report.mode -ne 'Snapshot') { throw 'Snapshot did not return the v3 Snapshot report.' }
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
function New-Row([int]$Index, [string]$Sha, [string]$Date, [bool]$Measured, [object]$Values) {
    [pscustomobject][ordered]@{ index = $Index; sha = $Sha; date = $Date; measured = $Measured; verbosity = [double](Get-NumberText $Values.verbosity); structuralErosion = [double](Get-NumberText $Values.structuralErosion); supported = [int]$Values.supported; parsed = [int]$Values.parsed }
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
function Get-CommitPlan([string]$Repository, [object]$Parent, [object]$Commit) {
    $patch = Get-PatchEvidence $Repository $Parent.Sha $Commit.Sha
    return [pscustomobject]@{ Sha = $Commit.Sha; Date = $Commit.Date; CaptureMode = $(if ($patch.cppChanged) { 'cpp-change' } else { 'carry-forward' }); Patch = $patch }
}
# The per-commit step Generate and Rebuild share: carry the previous values, take a seed row, or measure.
function New-CommitRow([string]$Repository, [int]$Index, [object]$Plan, [object]$Previous, [object]$Capture, [object]$SeedRow) {
    if ($Plan.CaptureMode -eq 'carry-forward') { return [pscustomobject]@{ Kind = 'carried'; Row = (New-Row $Index $Plan.Sha $Plan.Date $false $Previous); Coverage = $null } }
    if ($null -ne $SeedRow) { return [pscustomobject]@{ Kind = 'reused'; Row = (New-Row $Index $Plan.Sha $Plan.Date ([bool]$SeedRow.measured) $SeedRow); Coverage = $null } }
    $evidence = Measure-Commit $Repository $Plan.Sha $Capture
    return [pscustomobject]@{ Kind = 'measured'; Row = (New-Row $Index $Plan.Sha $Plan.Date $true $evidence); Coverage = $evidence.coverage }
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
function New-HistorySvg([object[]]$Rows, [string]$SeriesDigest) {
    $maxSupported = [Math]::Max(1.0, [double](($Rows | Measure-Object -Property supported -Maximum).Maximum))
    $lines = [Collections.Generic.List[string]]::new()
    $lines.Add('<?xml version="1.0" encoding="UTF-8"?>')
    $lines.Add('<svg xmlns="http://www.w3.org/2000/svg" width="1800" height="1150" viewBox="0 0 1800 1150">')
    $lines.Add('<title>Broken Engine code-quality history</title>')
    $lines.Add("<desc>seriesDigest=$SeriesDigest</desc>")
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
# Lag-by-one: one plan per first-parent commit after the newest table row on Base's chain, through Base, oldest first.
function Get-GeneratePlans([string]$Repository, [object]$History, [string]$Base) {
    $chain = @(Get-FirstParentChain $Repository $Base)
    $positions = [Collections.Generic.Dictionary[string, int]]::new([StringComparer]::Ordinal)
    for ($position = 0; $position -lt $chain.Count; ++$position) { $positions[$chain[$position].Sha] = $position }
    $anchor = -1
    for ($rowIndex = $History.Rows.Count - 1; $rowIndex -ge 0 -and $anchor -lt 0; --$rowIndex) {
        $sha = [string]$History.Rows[$rowIndex].sha
        if ($positions.ContainsKey($sha)) { $anchor = $positions[$sha] }
    }
    if ($anchor -lt 0) { throw "No history table row is on BaseCommit's first-parent chain, as after a squash or rewrite of main; nothing was measured. Recover with -Mode Rebuild, passing the current table as -SeedPath." }
    $plans = [Collections.Generic.List[object]]::new()
    for ($position = $anchor + 1; $position -lt $chain.Count; ++$position) { $plans.Add((Get-CommitPlan $Repository $chain[$position - 1] $chain[$position])) }
    return $plans.ToArray()
}
function Get-Generator([string]$Repository) {
    [ordered]@{ relativePath = $script:GeneratorRelativePath; sha256 = Get-FileSha256 (Join-Path $Repository ($script:GeneratorRelativePath -replace '/', '\')) }
}
function Write-HistoryPair([string]$Repository, [string]$OutputFull, [byte[]]$JsonlBytes, [object[]]$Rows) {
    $seriesDigest = Get-BytesSha256 $JsonlBytes
    $svgBytes = [Text.UTF8Encoding]::new($false).GetBytes((New-HistorySvg $Rows $seriesDigest))
    $jsonlPath = Join-Path $OutputFull 'CodeQualityMetricsHistory.jsonl'; $svgPath = Join-Path $OutputFull 'CodeQualityMetricsHistory.svg'
    [IO.File]::WriteAllBytes($jsonlPath, $JsonlBytes); [IO.File]::WriteAllBytes($svgPath, $svgBytes)
    if (-not (Test-Path -LiteralPath $jsonlPath -PathType Leaf) -or -not (Test-Path -LiteralPath $svgPath -PathType Leaf)) { throw 'History output persistence did not produce both required files.' }
    $jsonlInfo = Get-Item -LiteralPath $jsonlPath -Force; $svgInfo = Get-Item -LiteralPath $svgPath -Force
    return [pscustomobject]@{
        Digest = $seriesDigest
        Outputs = [ordered]@{
            jsonl = [ordered]@{ path = Get-RelativePosix $Repository $jsonlPath; bytes = [int64]$jsonlInfo.Length; sha256 = Get-FileSha256 $jsonlPath }
            svg = [ordered]@{ path = Get-RelativePosix $Repository $svgPath; bytes = [int64]$svgInfo.Length; sha256 = Get-FileSha256 $svgPath }
        }
    }
}
function New-Contract([object]$History, [string]$Base, [object[]]$Plans, [object]$Generator, [object]$Capture) {
    $last = $History.Rows[-1]
    [ordered]@{
        schemaVersion = 'broken-engine-code-quality-history-contract/v2'
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
        $step = New-CommitRow $Repository ([int]$previous.index + 1) $plan $previous $capture $null
        $appended.Add($step.Row)
        $entries.Add([ordered]@{ sha = $plan.Sha; captureMode = $plan.CaptureMode; patch = $plan.Patch; row = $step.Row; coverage = $step.Coverage })
        $previous = $step.Row
    }
    $jsonlBytes = [byte[]]($History.Bytes + (Get-RowsBytes $appended.ToArray()))
    $pair = Write-HistoryPair $Repository $outputFull $jsonlBytes (@($History.Rows) + $appended.ToArray())
    [ordered]@{
        schemaVersion = 'broken-engine-code-quality-history-update/v2'
        mode = 'Generate'
        source = [ordered]@{ baseCommit = $Base }
        generator = $Generator
        capture = $capture
        rows = $entries.ToArray()
        series = [ordered]@{ digest = $pair.Digest; historyBytesSha256 = $History.HistoryBytesSha256 }
        outputs = $pair.Outputs
    }
}
# Rows before the root's seed row are copied; each first-parent commit of Base then reuses its seed row, is measured, or is carried.
function Invoke-Rebuild([string]$Repository, [string]$Base, [string]$Seed, [string]$Output) {
    $seedBytes = [IO.File]::ReadAllBytes($Seed)
    $seedHistory = Read-History $seedBytes 'Seed'
    $chain = @(Get-FirstParentChain $Repository $Base)
    $rootPosition = -1
    for ($rowIndex = 0; $rowIndex -lt $seedHistory.Rows.Count -and $rootPosition -lt 0; ++$rowIndex) { if ([string]$seedHistory.Rows[$rowIndex].sha -ceq $chain[0].Sha) { $rootPosition = $rowIndex } }
    if ($rootPosition -lt 0) { throw "Seed has no row for BaseCommit's root commit $($chain[0].Sha)." }
    $seedRows = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::Ordinal)
    for ($rowIndex = $rootPosition + 1; $rowIndex -lt $seedHistory.Rows.Count; ++$rowIndex) {
        $sha = [string]$seedHistory.Rows[$rowIndex].sha
        if ($sha -ceq $chain[0].Sha -or $seedRows.ContainsKey($sha)) { throw "Seed holds more than one row for $sha." }
        $seedRows[$sha] = $seedHistory.Rows[$rowIndex]
    }
    $plans = [Collections.Generic.List[object]]::new()
    for ($position = 1; $position -lt $chain.Count; ++$position) { $plans.Add((Get-CommitPlan $Repository $chain[$position - 1] $chain[$position])) }
    $outputFull = Assert-UniqueOutput $Repository $Output
    $capture = if (@($plans | Where-Object { $_.CaptureMode -eq 'cpp-change' -and -not $seedRows.ContainsKey($_.Sha) }).Count -gt 0) { Get-PreparedCapture $Repository } else { $null }
    [IO.Directory]::CreateDirectory($outputFull) | Out-Null
    $rows = [Collections.Generic.List[object]]::new()
    for ($rowIndex = 0; $rowIndex -lt $rootPosition; ++$rowIndex) {
        $seedRow = $seedHistory.Rows[$rowIndex]
        $rows.Add((New-Row $rowIndex ([string]$seedRow.sha) ([string]$seedRow.date) ([bool]$seedRow.measured) $seedRow))
    }
    $rootSeed = $seedHistory.Rows[$rootPosition]
    $previous = New-Row $rootPosition $chain[0].Sha $chain[0].Date ([bool]$rootSeed.measured) $rootSeed
    $rows.Add($previous)
    $counts = @{ reused = 1; measured = 0; carried = 0 }
    foreach ($plan in $plans) {
        $seedRow = if ($plan.CaptureMode -eq 'cpp-change' -and $seedRows.ContainsKey($plan.Sha)) { $seedRows[$plan.Sha] } else { $null }
        $step = New-CommitRow $Repository $rows.Count $plan $previous $capture $seedRow
        ++$counts[$step.Kind]
        $rows.Add($step.Row)
        $previous = $step.Row
    }
    $headerBytes = [Text.UTF8Encoding]::new($false).GetBytes($script:HeaderText + "`n")
    $jsonlBytes = [byte[]]($headerBytes + (Get-RowsBytes $rows.ToArray()))
    $pair = Write-HistoryPair $Repository $outputFull $jsonlBytes $rows.ToArray()
    [ordered]@{
        schemaVersion = 'broken-engine-code-quality-history-rebuild/v1'
        mode = 'Rebuild'
        source = [ordered]@{ baseCommit = $Base; seedSha256 = Get-BytesSha256 $seedBytes }
        capture = $capture
        series = [ordered]@{ rows = $rows.Count; reused = $counts.reused; measured = $counts.measured; carried = $counts.carried; digest = $pair.Digest }
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
    if ($Mode -eq 'Contract' -and ($OutputDirectory -or $SeedPath)) { throw 'Contract is read-only and accepts neither OutputDirectory nor SeedPath.' }
    if ($Mode -ne 'Contract' -and -not $OutputDirectory) { throw "$Mode requires a unique Temp OutputDirectory." }
    if ($Mode -eq 'Generate' -and $SeedPath) { throw 'Generate reads the table at BaseCommit and does not accept SeedPath.' }
    if ($Mode -eq 'Rebuild' -and -not $SeedPath) { throw 'Rebuild requires a v2 SeedPath.' }
    $base = $BaseCommit.ToLowerInvariant()
    Assert-CommitObject $base 'BaseCommit'
    [void](Get-GitSha $repository $base)
    if ($Mode -eq 'Rebuild') {
        $receipt = Invoke-Rebuild $repository $base (Get-RepositoryPath $repository $SeedPath) (Get-RepositoryPath $repository $OutputDirectory)
    }
    else {
        $history = Read-CommitHistory $repository $base
        $plans = @(Get-GeneratePlans $repository $history $base)
        $generator = Get-Generator $repository
        if ($Mode -eq 'Contract') {
            $capture = if (@($plans | Where-Object { $_.CaptureMode -eq 'cpp-change' }).Count -gt 0) { Get-PreparedCapture $repository } else { $null }
            $receipt = New-Contract $history $base $plans $generator $capture
        }
        else { $receipt = Invoke-Generate $repository $history $base $plans $generator (Get-RepositoryPath $repository $OutputDirectory) }
    }
    [Console]::Out.WriteLine((Get-CanonicalJson $receipt))
}
catch { Fail $_.Exception.Message }
