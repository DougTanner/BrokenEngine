# Writes one part of the Change Workflow context to stdout for the SessionStart
# hooks in .claude/settings.json.
# Claude Code caps a single hook output at 10,000 characters and replaces
# anything longer with a saved-file preview, so change-workflow.md carries three
# `<!-- session-context-part: <title> -->` marker lines that name where it splits
# and what each part holds. This script splits there, strips the markers, appends
# risk-tiers.md to the last part, and labels each part with its title so arrival
# order does not matter.
# Exit codes:
#   0 - prints the requested part
#   2 - the marker count is not 3, or a part exceeds the 9,500-character budget;
#       prints the reason to stderr

[CmdletBinding()]
param(
	[Parameter(Mandatory)][ValidateSet(1, 2, 3)][int] $Part
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$script:MaximumPartCharacters = 9500
$script:Utf8 = [Text.UTF8Encoding]::new($false)

function Write-Stderr([string] $Message) {
	$stderr = [Console]::OpenStandardError()
	$bytes = $script:Utf8.GetBytes($Message)
	$stderr.Write($bytes, 0, $bytes.Length)
	$stderr.Flush()
}

$referencesRoot = Join-Path (Split-Path -Parent $PSScriptRoot) 'references'
$text = [IO.File]::ReadAllText((Join-Path $referencesRoot 'change-workflow.md'))
$riskTiers = [IO.File]::ReadAllText((Join-Path $referencesRoot 'risk-tiers.md'))

$markers = [regex]::Matches($text, "(?m)^<!-- session-context-part: (?<title>.*) -->`n")
if ($markers.Count -ne 3) {
	Write-Stderr "Write-SessionWorkflowContext: change-workflow.md carries $($markers.Count) session-context-part markers; exactly 3 are required.`n"
	exit 2
}

# Each part runs from just past its marker line to the next marker, so stripping
# the markers makes the parts concatenate back to the exact source bytes.
$parts = @(0, 1, 2) | ForEach-Object {
	$start = $markers[$_].Index + $markers[$_].Length
	$end = if ($_ -lt 2) { $markers[$_ + 1].Index } else { $text.Length }
	$text.Substring($start, $end - $start)
}
$parts[2] += "`n" + $riskTiers

$outputs = @(1, 2, 3) | ForEach-Object {
	"<!-- Change Workflow context, part $_ of 3: $($markers[$_ - 1].Groups['title'].Value) -->`n" + $parts[$_ - 1]
}
$oversized = @(1, 2, 3 | Where-Object { $outputs[$_ - 1].Length -gt $script:MaximumPartCharacters })
if ($oversized.Count -gt 0) {
	$sizes = ($oversized | ForEach-Object { "part $_ is $($outputs[$_ - 1].Length) characters" }) -join ', '
	Write-Stderr "Write-SessionWorkflowContext: $sizes; each must stay at or under $script:MaximumPartCharacters.`n"
	exit 2
}

# Raw bytes rather than Write-Output: the hook text must reach the session
# byte-identical to the reference files, and PowerShell's own output re-encodes
# and terminates the stream with the platform newline.
$stdout = [Console]::OpenStandardOutput()
$bytes = $script:Utf8.GetBytes($outputs[$Part - 1])
$stdout.Write($bytes, 0, $bytes.Length)
$stdout.Flush()
