# Generates Projects/BrokenEngineSandbox/Source/Agent/Commands/AgentFieldNames.h, the
# kStatusChangeFieldName table naming every data member of every StatusChangeData alternative in
# Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h. Both paths are fixed relative to this script.
#
# Invocation, from the worktree root: `pwsh -NoProfile -File .agents/scripts/Write-AgentFieldNames.ps1`
# writes the header (only when its bytes change). `-Check` regenerates in memory, writes nothing, and
# compares against the checked-in header.
#
# Every `//` comment is stripped first. The alternatives come from `using StatusChangeData =
# std::variant<...>;`, in order; each alternative's `struct NAME` definition is found and, at brace depth
# one inside it, every line of the form `TYPE NAME = INIT;` or `TYPE NAME {};` (TYPE may contain `::` and
# `<...>`) is a data member, emitted in declaration order. A name starting with `operator` is never a data
# member; every other line is ignored. The output depends only on the input text: UTF-8 without a BOM,
# LF line endings, one trailing newline.
#
# Exit codes: 0 written or up to date; 1 under -Check when the header is missing or its bytes differ;
# 2 the variant declaration or an alternative's struct definition is missing, or any other error.
[CmdletBinding()]
param(
	[switch] $Check
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$script:InputPath = Join-Path $PSScriptRoot '../../Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h'
$script:OutputPath = Join-Path $PSScriptRoot '../../Projects/BrokenEngineSandbox/Source/Agent/Commands/AgentFieldNames.h'
$script:RegenerateCommand = 'pwsh -NoProfile -File .agents/scripts/Write-AgentFieldNames.ps1'
$script:Utf8 = [Text.UTF8Encoding]::new($false)
$script:VariantPattern = 'using\s+StatusChangeData\s*=\s*std::variant\s*<([^;]*)>\s*;'
$script:MemberPattern = '^\s*[A-Za-z_][\w:]*(?:<[^;=]*>)?\s+([A-Za-z_]\w*)\s*(?:=[^;]+|\{\s*\})\s*;\s*$'

function Get-StructMemberName([string[]] $Lines, [string] $StructName) {
	$start = -1
	for ($index = 0; $index -lt $Lines.Count; $index++) {
		if ($Lines[$index] -cmatch "^\s*struct\s+$StructName\b[^;]*$") { $start = $index; break }
	}
	if ($start -lt 0) { return $null }
	$names = [Collections.Generic.List[string]]::new()
	$depth = 0
	$opened = $false
	for ($index = $start; $index -lt $Lines.Count; $index++) {
		$line = $Lines[$index]
		if ($depth -eq 1 -and $line -cmatch $script:MemberPattern -and -not $Matches[1].StartsWith('operator')) { $names.Add($Matches[1]) }
		foreach ($character in $line.ToCharArray()) {
			if ($character -ceq '{') { $depth++; $opened = $true }
			elseif ($character -ceq '}') { $depth-- }
		}
		if ($opened -and $depth -eq 0) { return , $names }
	}
	return $null
}

try {
	$text = ([IO.File]::ReadAllText($script:InputPath, $script:Utf8) -replace "`r`n?", "`n") -replace '//[^\n]*', ''
	$variant = [regex]::Match($text, $script:VariantPattern)
	if (-not $variant.Success) {
		Write-Output 'StatusChange.h has no `using StatusChangeData = std::variant<...>;` declaration; AgentFieldNames.h was not written.'
		exit 2
	}
	$lines = $text -split "`n"
	$output = [Text.StringBuilder]::new()
	[void] $output.Append("// Generated from Frame/StatusChange.h; do not edit. Regenerate with ``$script:RegenerateCommand``.`n")
	[void] $output.Append("#pragma once`n`n#if defined(BT_SERVER)`n`nnamespace game`n{`n`n")
	[void] $output.Append("template <auto pMember> inline constexpr std::string_view kStatusChangeFieldName {};`n")
	foreach ($alternative in ($variant.Groups[1].Value -split ',')) {
		$structName = $alternative.Trim()
		$names = Get-StructMemberName $lines $structName
		if ($null -eq $names) {
			Write-Output "StatusChangeData alternative '$structName' has no struct definition in StatusChange.h; AgentFieldNames.h was not written."
			exit 2
		}
		[void] $output.Append("`n// $structName`n")
		foreach ($name in $names) {
			[void] $output.Append("template <> inline constexpr std::string_view kStatusChangeFieldName<&$structName::$name> = `"$name`";`n")
		}
	}
	[void] $output.Append("`n} // namespace game`n`n#endif // BT_SERVER`n")
	$bytes = $script:Utf8.GetBytes($output.ToString())

	$current = if (Test-Path -LiteralPath $script:OutputPath -PathType Leaf) { [IO.File]::ReadAllBytes($script:OutputPath) } else { $null }
	$upToDate = $null -ne $current -and [Linq.Enumerable]::SequenceEqual([byte[]] $current, [byte[]] $bytes)
	if ($Check) {
		if ($upToDate) { exit 0 }
		Write-Output "AgentFieldNames.h is missing or stale against StatusChange.h; regenerate it with ``$script:RegenerateCommand``."
		exit 1
	}
	if (-not $upToDate) { [IO.File]::WriteAllBytes($script:OutputPath, $bytes) }
	exit 0
}
catch {
	Write-Output "Write-AgentFieldNames failed: $($_.Exception.Message)"
	exit 2
}
