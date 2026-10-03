# Checks every .codex/agents/*.toml model pin, plus any -Model slug, against the live Codex model
# catalog (`codex debug models`) and prints one compact JSON receipt. A slug is blocked when the
# catalog lacks it, or when a list-visibility slug of the same gpt-<version>-<family> carries a
# numerically higher version; hidden entries never block. Exit 0 pass, 2 blocked, 1 error.
[CmdletBinding()]
param(
	[string] $Model
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$sharedScripts = Join-Path $PSScriptRoot '..\..\..\scripts'
if (-not (Test-Path -LiteralPath (Join-Path $sharedScripts 'AgentScriptCommon.psm1'))) {
	$sharedScripts = Join-Path $PSScriptRoot '..\..\..\..\.agents\scripts'
}
Import-Module (Join-Path $sharedScripts 'AgentScriptCommon.psm1') -Force

$result = [ordered]@{
	schemaVersion = 'broken-engine-codex-models/v1'
	status = 'error'
	checks = @()
	message = $null
}

# Null when the slug is not gpt-<version>-<family>, such as gpt-5.5 or codex-auto-review.
function Get-ModelFamily([string] $Slug) {
	if ($Slug -cmatch '^gpt-(\d+(?:\.\d+)*)-([a-z][a-z0-9-]*)$') {
		return [pscustomobject] @{ Version = $Matches[1]; Family = $Matches[2] }
	}

	return $null
}

# Part by part with a missing part read as 0, so 6 equals 6.0, 6 < 6.1, and 5.10 > 5.9.
function Compare-ModelVersion([string] $Left, [string] $Right) {
	$leftParts = @($Left.Split('.') | ForEach-Object { [long] $_ })
	$rightParts = @($Right.Split('.') | ForEach-Object { [long] $_ })
	for ($index = 0; $index -lt [Math]::Max($leftParts.Count, $rightParts.Count); $index++) {
		$leftPart = if ($index -lt $leftParts.Count) { $leftParts[$index] } else { 0 }
		$rightPart = if ($index -lt $rightParts.Count) { $rightParts[$index] } else { 0 }
		if ($leftPart -ne $rightPart) {
			return [Math]::Sign($leftPart - $rightPart)
		}
	}

	return 0
}

$exitCode = 1
try {
	$repositoryRoot = Get-AgentCanonicalPath (Join-Path $sharedScripts '..\..')
	$slugs = [System.Collections.Generic.List[string]]::new()
	foreach ($preset in @(Get-ChildItem -LiteralPath (Join-Path $repositoryRoot '.codex\agents') -Filter '*.toml' -File)) {
		$assignments = @(Get-Content -LiteralPath $preset.FullName | Where-Object { $_ -match '^\s*model\s*=' })
		if (($assignments.Count -ne 1) -or ($assignments[0] -notmatch '^\s*model\s*=\s*"([^"]+)"\s*(?:#.*)?$')) {
			throw "agent configuration '$($preset.FullName)' must contain exactly one quoted model assignment"
		}

		$slugs.Add($Matches[1])
	}

	if ($Model) {
		$slugs.Add($Model)
	}

	$codex = Get-Command codex -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
	if (-not $codex) {
		throw 'codex CLI not found on PATH'
	}

	$run = Invoke-AgentProcess $codex.Source @('debug', 'models') $repositoryRoot
	if ($run.ExitCode -ne 0) {
		throw "codex debug models exited $($run.ExitCode)"
	}

	$catalog = @(($run.Stdout | ConvertFrom-Json).models)
	$blocked = $false
	foreach ($slug in @($slugs | Sort-Object -Unique)) {
		$reason = $null
		if (-not @($catalog | Where-Object { $_.slug -ceq $slug })) {
			$reason = 'absent from the catalog'
		}

		$newest = $slug
		$pinned = Get-ModelFamily $slug
		if ($null -ne $pinned) {
			$newestVersion = $pinned.Version
			foreach ($entry in @($catalog | Where-Object { $_.visibility -ceq 'list' })) {
				$candidate = Get-ModelFamily $entry.slug
				if (($null -ne $candidate) -and ($candidate.Family -ceq $pinned.Family) -and ((Compare-ModelVersion $candidate.Version $newestVersion) -gt 0)) {
					$newest = $entry.slug
					$newestVersion = $candidate.Version
				}
			}

			if (($null -eq $reason) -and ($newest -cne $slug)) {
				$reason = 'a newer listed version exists'
			}
		}

		$blocked = $blocked -or ($null -ne $reason)
		$result.checks += [ordered]@{ slug = $slug; newest = $newest; reason = $reason }
	}

	$result.status = if ($blocked) { 'blocked' } else { 'pass' }
	$exitCode = if ($blocked) { 2 } else { 0 }
}
catch {
	$result.status = 'error'
	$result.message = $_.Exception.Message
	$exitCode = 1
}

[Console]::Out.Write(($result | ConvertTo-Json -Depth 8 -Compress))
exit $exitCode
