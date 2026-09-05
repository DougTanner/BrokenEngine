# Comment-block scanner for /comment-review: it lists every run of consecutive `//` lines in the C++ and
# shader files a caller names, with the regex kinds each run hits, so a review never has to hand-hunt
# comment blocks. The scan reports candidates only — it never decides whether a block is a finding, never
# edits a file, and writes nothing to disk, so it is safe under a read-only sandbox. Stdout carries only
# the result document.
[CmdletBinding()]
param(
	[Parameter(Mandatory)][string[]] $Path
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
# One entry per candidate kind. A block reports every kind any of its lines matches, in this order.
$script:KindPatterns = @(
	@{ Kind = 'banner'; Pattern = '^\s*//\s*[=\-*]{10,}' }
	@{ Kind = 'template-field'; Pattern = '^\s*//\s*(Parameters|Returns|Thread-safety):' }
	@{ Kind = 'history'; Pattern = '\b(previous(ly)?|no longer|used to|was |instead of|replaced|now )\b' }
	@{ Kind = 'speculative'; Pattern = '\b(would need|if .* ever|future|hypothetical|TODO)\b' }
	@{ Kind = 'navigation'; Pattern = '(AGENTS|CLAUDE)\.md' }
)
$script:Utf8 = [Text.UTF8Encoding]::new($false)

$result = [ordered]@{
	schemaVersion = 'broken-engine-comment-blocks/v1'
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
	$files = [Collections.Generic.List[string]]::new()
	foreach ($target in $Targets) {
		$full = [IO.Path]::GetFullPath($target, $Base)
		if (Test-Path -LiteralPath $full -PathType Container) {
			foreach ($item in (Get-ChildItem -LiteralPath $full -File -Recurse)) {
				if ($script:ScannedExtensions -contains $item.Extension.ToLowerInvariant()) { $files.Add($item.FullName) }
			}
			continue
		}
		if (-not (Test-Path -LiteralPath $full -PathType Leaf)) {
			Complete-CommentBlocks 2 'blocked' 'comments.path-invalid' "-Path must name an existing file or directory: '$target'."
		}
		if ($script:ScannedExtensions -contains ([IO.Path]::GetExtension($full)).ToLowerInvariant()) { $files.Add($full) }
	}
	# The comma keeps a one-file result a list instead of unrolling it to a bare string.
	return , $files
}

function Get-CommentBlock([string] $FullPath, [string] $Relative) {
	$blocks = [Collections.Generic.List[object]]::new()
	$lines = @([IO.File]::ReadAllText($FullPath, $script:Utf8) -split "`r`n|`n|`r")
	$start = 0
	$kinds = [Collections.Generic.List[string]]::new()
	$first = ''
	for ($number = 1; $number -le $lines.Count + 1; $number++) {
		$line = if ($number -le $lines.Count) { $lines[$number - 1] } else { '' }
		$isComment = $number -le $lines.Count -and $line -match $script:CommentPattern
		if ($isComment) {
			if ($start -eq 0) {
				$start = $number
				$kinds.Clear()
				$first = $line.Trim()
				if ($first.Length -gt $script:MaximumTextLength) { $first = $first.Substring(0, $script:MaximumTextLength) }
			}
			foreach ($kind in $script:KindPatterns) {
				if ($line -match $kind.Pattern -and -not $kinds.Contains($kind.Kind)) { $kinds.Add($kind.Kind) }
			}
			continue
		}
		if ($start -eq 0) { continue }
		# Every block is emitted: the length threshold belongs to references/comment-classes.md, not here.
		$blocks.Add([ordered]@{
			path = $Relative
			startLine = $start
			lineCount = $number - $start
			kinds = [string[]] $kinds.ToArray()
			firstLine = $first
		})
		$start = 0
	}
	return , $blocks
}

try {
	$base = (Get-Location).ProviderPath
	$scanned = Get-ScannedFile $Path $base
	$found = [Collections.Generic.List[object]]::new()
	foreach ($file in $scanned) {
		foreach ($block in (Get-CommentBlock $file (Get-RelativePath $file $base))) { $found.Add($block) }
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
