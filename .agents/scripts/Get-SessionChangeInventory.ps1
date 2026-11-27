# Deterministic session change inventory CLI wrapper over SessionChangeInventory.psm1:
# one read-only answer to "what changed between the session baseline and the head side, and
# what artifact type is each change". The CLI requires either -OutputPath <path> or
# -EmitManifest (omitting both or combining them exits 2 with empty stdout and one stderr
# line). -OutputPath writes the full untruncated v2 inventory JSON (with exact
# truncation.outputBytes) to that file and emits one summary line naming its status, code,
# and message on stdout; the script writes no other file and no repository metadata
# (GIT_OPTIONAL_LOCKS=0 keeps Git from refreshing the index).
# In-process callers import .agents/scripts/SessionChangeInventory.psm1 and call
# Get-SessionChangeInventory directly with zero disk writes. A session Coretide clone at
# Projects/Coretide is inventoried by the same pipeline in that repository, from the merge base
# of its HEAD and upstream, and its rows are merged under that prefix; a -Head whose message
# carries a Coretide-Snapshot line (a review-round snapshot) sets the Coretide head to that commit
# instead of the clone's HEAD, and coretide.headSha reports it; its Coretide-Baseline line
# replaces that merge base.
[CmdletBinding()]
param(
	[Parameter(Mandatory)][string] $RepositoryRoot,
	[Parameter(Mandatory)][string] $Baseline,
	[string] $Head,
	[switch] $IncludeUntracked,
	[string[]] $PathPrefix,
	[switch] $Regions,
	[switch] $Landing,
	[switch] $EmitManifest,
	[string] $OutputPath,
	[string] $SavePlanPaths
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$script:MaximumOutputBytes = 131072
$script:MaximumMessageLength = 256
$script:Utf8 = [Text.UTF8Encoding]::new($false)

function Write-InventoryBytes([bool] $Error, [byte[]] $Bytes) {
	$stream = if ($Error) { [Console]::OpenStandardError() } else { [Console]::OpenStandardOutput() }
	$stream.Write($Bytes, 0, $Bytes.Length)
	$stream.Flush()
}

function Write-InventoryStream([bool] $Error, [string] $Text) {
	Write-InventoryBytes $Error ($script:Utf8.GetBytes($Text))
}

function Set-CliEnvelopeOutcome([pscustomobject] $Envelope, [string] $Status, [string] $Code, [string] $Message) {
	$text = if ($null -eq $Message) { '' } else { $Message }
	if ($text.Length -gt $script:MaximumMessageLength) { $text = $text.Substring(0, $script:MaximumMessageLength) }
	$Envelope.status = $Status
	$Envelope.code = $Code
	$Envelope.message = $text
	return $Envelope
}

function New-CliInventoryEnvelope([string] $Status, [string] $Code, [string] $Message) {
	$envelope = [pscustomobject][ordered]@{
		schemaVersion = 'broken-engine-session-change-inventory/v2'
		status = 'error'
		code = 'internal.error'
		message = ''
		baselineSha = $null
		headSha = $null
		coretide = $null
		entries = @()
		counts = $null
		triggers = $null
		regions = $null
		landing = $null
		truncated = $false
		truncation = $null
	}
	return Set-CliEnvelopeOutcome $envelope $Status $Code $Message
}

function Get-CliExitCode([string] $Status) {
	if ($Status -ceq 'pass') { return 0 }
	if ($Status -ceq 'blocked') { return 2 }
	return 1
}

try {
	$hasOutputPath = -not [string]::IsNullOrWhiteSpace($OutputPath)
	if ($hasOutputPath -and $EmitManifest) {
		Write-InventoryStream $true "blocked inventory.mode-conflict The -OutputPath result file is exclusive with -EmitManifest.`n"
		exit 2
	}
	if (-not $hasOutputPath -and -not $EmitManifest) {
		Write-InventoryStream $true "blocked inventory.output-required Supply -OutputPath <path> or -EmitManifest; for in-process callers, import .agents/scripts/SessionChangeInventory.psm1.`n"
		exit 2
	}

	Import-Module (Join-Path $PSScriptRoot 'SessionChangeInventory.psm1') -Force
	$invokeParams = @{
		RepositoryRoot = $RepositoryRoot
		Baseline = $Baseline
	}
	if ($PSBoundParameters.ContainsKey('Head')) { $invokeParams.Head = $Head }
	if ($PSBoundParameters.ContainsKey('PathPrefix')) { $invokeParams.PathPrefix = $PathPrefix }
	if ($IncludeUntracked) { $invokeParams.IncludeUntracked = $true }
	if ($Regions) { $invokeParams.Regions = $true }
	if ($Landing) { $invokeParams.Landing = $true }
	if ($EmitManifest) { $invokeParams.EmitManifest = $true }
	if ($PSBoundParameters.ContainsKey('SavePlanPaths')) { $invokeParams.SavePlanPaths = $SavePlanPaths }

	$envelope = $null
	if ($EmitManifest) { $invokeParams.Envelope = [ref] $envelope }

	$doc = Get-SessionChangeInventory @invokeParams

	if ($EmitManifest) {
		# Manifest consumers reserve stdout for one complete frozen-tree document. Any blocked or
		# error result therefore uses the existing structured envelope on stderr instead.
		if ($doc.schemaVersion -ceq 'broken-engine-cpp-manifest/v1') {
			$bytes = $script:Utf8.GetBytes(($doc | ConvertTo-Json -Depth 32 -Compress) + "`n")
			if ($bytes.Length -gt $script:MaximumOutputBytes) {
				$blocked = Set-CliEnvelopeOutcome $envelope 'blocked' 'inventory.manifest-cap-exceeded' "The manifest is $($bytes.Length) bytes, above the stdout budget of $($script:MaximumOutputBytes)."
				Write-InventoryStream $true (($blocked | ConvertTo-Json -Depth 32 -Compress) + "`n")
				exit 2
			}
			Write-InventoryBytes $false $bytes
			exit 0
		}
		Write-InventoryStream $true (($doc | ConvertTo-Json -Depth 32 -Compress) + "`n")
		exit (Get-CliExitCode $doc.status)
	}

	if ($null -ne $doc.truncation) {
		# Reporting the size of a document that carries its own size needs a fixed point, so the
		# measurement repeats until the reported value equals the byte count of the encoded bytes.
		$bytes = $null
		while ($true) {
			$bytes = $script:Utf8.GetBytes(($doc | ConvertTo-Json -Depth 32 -Compress))
			if ($bytes.Length -eq $doc.truncation.outputBytes) { break }
			$doc.truncation.outputBytes = $bytes.Length
		}
	}
	else {
		$bytes = $script:Utf8.GetBytes(($doc | ConvertTo-Json -Depth 32 -Compress))
	}
	$resolvedOutputPath = [IO.Path]::GetFullPath($OutputPath, (Get-Location).ProviderPath)
	[IO.File]::WriteAllBytes($resolvedOutputPath, $bytes)
	Write-InventoryStream $false "$($doc.status) $($doc.code) $($doc.message) -> $OutputPath`n"
	exit (Get-CliExitCode $doc.status)
}
catch {
	$envelope = New-CliInventoryEnvelope 'error' 'internal.error' $_.Exception.Message
	Write-InventoryStream $true (($envelope | ConvertTo-Json -Depth 32 -Compress) + "`n")
	exit 1
}
