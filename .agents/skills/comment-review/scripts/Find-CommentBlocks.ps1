# Comment-block scanner for /comment-review: it lists every run of consecutive `//` lines in surviving
# session-changed C++/shader files (Session mode, with per-block `overlapsRegion`) or in the files and
# directories a caller names (WholeFile mode, `-Path`), with the regex kinds each run hits, so a review
# never has to hand-hunt comment blocks. The scan reports candidates only — it never decides whether a
# block is a finding, never edits a file, and writes nothing to disk, so it is safe under a read-only
# sandbox. Stdout carries only the result document.
[CmdletBinding(DefaultParameterSetName = 'Session')]
param(
	[Parameter(Mandatory, ParameterSetName = 'Session')][string] $RepositoryRoot,
	[Parameter(Mandatory, ParameterSetName = 'Session')][string] $Baseline,
	[Parameter(ParameterSetName = 'Session')][switch] $IncludeUntracked,
	[Parameter(ParameterSetName = 'Session')][string[]] $PathPrefix,
	[Parameter(Mandatory, ParameterSetName = 'WholeFile')][string[]] $Path
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$script:MaximumBlocks = 400
$script:MaximumTextLength = 200
$script:MaximumMessageLength = 256
$script:MaximumOutputBytes = 131072

$script:CppExtensions = @('.cpp', '.h', '.inl')
# GLSL source extensions, matching the list .agents/skills/glsl-review/SKILL.md owns in its `paths` frontmatter. A
# shader header carries the `.h` extension the C++ list above already scans, so `Shaders` needs no case.
$script:ShaderExtensions = @('.vert', '.frag', '.comp', '.geom', '.tesc', '.tese', '.glsl', '.mesh', '.task', '.rgen', '.rmiss', '.rchit', '.rahit', '.rint', '.rcall')
$script:ScannedExtensions = $script:CppExtensions + $script:ShaderExtensions
# A block is a run of consecutive lines whose first non-whitespace characters are `//`; the anchor allows
# leading whitespace because an in-function comment is indented.
$script:CommentPattern = '^\s*//'
# A line's comment text: the line minus its leading whitespace, the `//` marker, and the whitespace after the
# marker; trailing whitespace stays.
$script:CommentPrefixPattern = '^\s*//\s*'
# One entry per candidate kind. A block reports every kind any of its lines matches, in this order.
$script:KindPatterns = @(
	@{ Kind = 'banner'; Pattern = '^\s*//\s*[=\-*]{10,}' }
	@{ Kind = 'template-field'; Pattern = '^\s*//\s*(Parameters|Returns|Thread-safety):' }
	@{ Kind = 'history'; Pattern = '\b(previous(ly)?|no longer|used to|was |instead of|replaced|now )\b' }
	@{ Kind = 'speculative'; Pattern = '\b(would need|if .* ever|future|hypothetical|TODO)\b' }
	@{ Kind = 'navigation'; Pattern = 'AGENTS\.md' }
)
$script:InventoryModule = Join-Path $PSScriptRoot '..\..\..\scripts\SessionChangeInventory.psm1'
$script:SessionClasses = @('cpp', 'dual-language-header', 'glsl')
$script:PathComparer = if ($IsWindows) { [StringComparer]::OrdinalIgnoreCase } else { [StringComparer]::Ordinal }
$script:Utf8 = [Text.UTF8Encoding]::new($false)

$result = [ordered]@{
	schemaVersion = 'broken-engine-comment-blocks/v3'
	status = 'error'
	code = 'internal.error'
	message = 'Comment block scan did not run.'
	blocks = @()
	counts = $null
	truncated = $false
}

function Complete-CommentBlocks([int] $ExitCode, [string] $Status, [string] $Code, [string] $Message) {
	$result.status = $Status
	$result.code = $Code
	$result.message = if ($Message.Length -gt $script:MaximumMessageLength) { $Message.Substring(0, $script:MaximumMessageLength) } else { $Message }
	$stream = [Console]::OpenStandardOutput()
	$bytes = $script:Utf8.GetBytes(($result | ConvertTo-Json -Depth 32 -Compress))
	$stream.Write($bytes, 0, $bytes.Length)
	$stream.Flush()
	exit $ExitCode
}

function Get-RelativePath([string] $FullPath, [string] $Base) {
	$relative = [IO.Path]::GetRelativePath($Base, $FullPath)
	return ($relative -replace '\\', '/')
}

function Get-ScannedFile([string[]] $Targets, [string] $Base) {
	$files = [Collections.Generic.List[object]]::new()
	$seen = [Collections.Generic.HashSet[string]]::new($script:PathComparer)
	foreach ($target in $Targets) {
		$full = [IO.Path]::GetFullPath($target, $Base)
		if (Test-Path -LiteralPath $full -PathType Container) {
			foreach ($item in (Get-ChildItem -LiteralPath $full -File -Recurse)) {
				if ($script:ScannedExtensions -contains $item.Extension.ToLowerInvariant()) {
					$canonical = [IO.Path]::GetFullPath($item.FullName)
					if ($seen.Add($canonical)) {
						$files.Add([pscustomobject] @{ FullPath = $canonical; RelativePath = (Get-RelativePath $canonical $Base) })
					}
				}
			}
			continue
		}
		if (-not (Test-Path -LiteralPath $full -PathType Leaf)) {
			Complete-CommentBlocks 2 'blocked' 'comments.path-invalid' "-Path must name an existing file or directory: '$target'."
		}
		if ($script:ScannedExtensions -contains ([IO.Path]::GetExtension($full)).ToLowerInvariant()) {
			if ($seen.Add($full)) {
				$files.Add([pscustomobject] @{ FullPath = $full; RelativePath = (Get-RelativePath $full $Base) })
			}
		}
	}
	# The comma keeps a one-file result a list instead of unrolling it to a single file record.
	return , $files
}

function Get-SessionFile([object] $Inventory, [string] $RepoRoot) {
	$files = [Collections.Generic.List[object]]::new()
	$seen = [Collections.Generic.HashSet[string]]::new($script:PathComparer)
	foreach ($entry in $Inventory.entries) {
		if ($entry.status -ceq 'D') { continue }
		if ($script:SessionClasses -cnotcontains $entry.class) { continue }
		$full = [IO.Path]::GetFullPath(($entry.path -replace '/', [IO.Path]::DirectorySeparatorChar), $RepoRoot)
		if (-not $seen.Add($full)) { continue }
		if (-not (Test-Path -LiteralPath $full -PathType Leaf)) {
			Complete-CommentBlocks 2 'blocked' 'comments.path-invalid' "Session file does not exist on disk: '$($entry.path)'."
		}
		$files.Add([pscustomobject] @{ FullPath = $full; RelativePath = $entry.path })
	}
	return , $files
}

function Get-RegionIndex([object] $Inventory) {
	$byPath = [Collections.Generic.Dictionary[string, Collections.Generic.List[object]]]::new([StringComparer]::Ordinal)
	foreach ($region in $Inventory.regions) {
		if ($null -eq $region.startLine -or $null -eq $region.endLine) { continue }
		if (-not $byPath.ContainsKey($region.path)) {
			$byPath[$region.path] = [Collections.Generic.List[object]]::new()
		}
		$byPath[$region.path].Add($region)
	}
	return , $byPath
}

function Test-BlockOverlapsRegion([int] $StartLine, [int] $EndLine, [Collections.Generic.List[object]] $Regions) {
	if ($null -eq $Regions) { return $false }
	foreach ($region in $Regions) {
		if ($StartLine -le [int] $region.endLine -and [int] $region.startLine -le $EndLine) { return $true }
	}
	return $false
}

function Get-CommentBlock([string] $FullPath, [string] $Relative, [Collections.Generic.List[object]] $Regions = $null) {
	$blocks = [Collections.Generic.List[object]]::new()
	$lines = @([IO.File]::ReadAllText($FullPath, $script:Utf8) -split "`r`n|`n|`r")
	$start = 0
	$kinds = [Collections.Generic.List[string]]::new()
	$first = ''
	$charCount = 0
	for ($number = 1; $number -le $lines.Count + 1; $number++) {
		$line = if ($number -le $lines.Count) { $lines[$number - 1] } else { '' }
		$isComment = $number -le $lines.Count -and $line -match $script:CommentPattern
		if ($isComment) {
			if ($start -eq 0) {
				$start = $number
				$kinds.Clear()
				$first = $line.Trim()
				if ($first.Length -gt $script:MaximumTextLength) { $first = $first.Substring(0, $script:MaximumTextLength) }
				$charCount = 0
			}
			$charCount += ($line -replace $script:CommentPrefixPattern, '').Length
			foreach ($kind in $script:KindPatterns) {
				if ($line -match $kind.Pattern -and -not $kinds.Contains($kind.Kind)) { $kinds.Add($kind.Kind) }
			}
			continue
		}
		if ($start -eq 0) { continue }
		$end = $number - 1
		# Every block is emitted: the length threshold belongs to references/comment-classes.md, not here.
		$blocks.Add([ordered]@{
			path = $Relative
			startLine = $start
			endLine = $end
			charCount = $charCount
			kinds = [string[]] $kinds.ToArray()
			firstLine = $first
			overlapsRegion = Test-BlockOverlapsRegion $start $end $Regions
		})
		$start = 0
	}
	return , $blocks
}

try {
	$regionsByPath = $null
	if ($PSCmdlet.ParameterSetName -ceq 'WholeFile') {
		$base = (Get-Location).ProviderPath
		$scanned = Get-ScannedFile $Path $base
	}
	else {
		try {
			Import-Module $script:InventoryModule -Force
		}
		catch {
			Complete-CommentBlocks 2 'blocked' 'comments.inventory-unavailable' $_.Exception.Message
		}
		$inventoryParams = @{ RepositoryRoot = $RepositoryRoot; Baseline = $Baseline; Regions = $true }
		if ($IncludeUntracked) { $inventoryParams.IncludeUntracked = $true }
		if ($PathPrefix) { $inventoryParams.PathPrefix = $PathPrefix }
		$inventory = Get-SessionChangeInventory @inventoryParams
		if ($inventory.status -cne 'pass') {
			Complete-CommentBlocks 2 'blocked' 'comments.inventory-unavailable' $inventory.message
		}
		$repoRoot = [IO.Path]::GetFullPath($RepositoryRoot)
		$scanned = Get-SessionFile $inventory $repoRoot
		$regionsByPath = Get-RegionIndex $inventory
	}

	$found = [Collections.Generic.List[object]]::new()
	foreach ($file in $scanned) {
		$fileRegions = if ($null -ne $regionsByPath -and $regionsByPath.ContainsKey($file.RelativePath)) { $regionsByPath[$file.RelativePath] } else { $null }
		foreach ($block in (Get-CommentBlock $file.FullPath $file.RelativePath $fileRegions)) { $found.Add($block) }
	}

	# Counts always describe the complete scan, never the truncated emission.
	$counts = [ordered]@{ files = $scanned.Count; total = $found.Count }
	foreach ($kind in $script:KindPatterns) { $counts[$kind.Kind] = @($found | Where-Object { $_.kinds -contains $kind.Kind }).Count }
	$result.counts = $counts
	$emitted = [Collections.Generic.List[object]]::new()
	foreach ($block in ($found | Select-Object -First $script:MaximumBlocks)) { $emitted.Add($block) }
	while ($true) {
		$result.blocks = [object[]] $emitted.ToArray()
		$result.truncated = $emitted.Count -lt $found.Count
		if ($script:Utf8.GetByteCount(($result | ConvertTo-Json -Depth 32 -Compress)) -le $script:MaximumOutputBytes) { break }
		if ($emitted.Count -eq 0) { break }
		$drop = [Math]::Max(1, [int] [Math]::Ceiling($emitted.Count * 0.1))
		$emitted.RemoveRange($emitted.Count - $drop, $drop)
	}
	Complete-CommentBlocks 0 'pass' 'ok' "Scanned $($scanned.Count) file(s) and found $($found.Count) comment block(s)."
}
catch {
	Complete-CommentBlocks 1 'error' 'internal.error' $_.Exception.Message
}
