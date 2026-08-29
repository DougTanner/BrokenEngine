# Exclusive owner of post-confirmation landing: structural sanity, the landing lock lease,
# at most one internal rebase of the session branch onto a newly advanced primary, the guarded
# primary advance, and best-effort Plan claim deletion. Exit 0 means the caller reports LANDED;
# exit 2 may report a post-advance blocker but still carries the authoritative lock-cleanup state.
#
# The scheduler is touched only when -ReleasePlanClaim says this session holds a claim:
# without it the landing runs no `plan` command at all.
[CmdletBinding()]
param(
	[Parameter(Mandatory)][string] $CurrentWorktree,
	[Parameter(Mandatory)][string] $PrimaryWorktree,
	[Parameter(Mandatory)][string] $CurrentBranch,
	[Parameter(Mandatory)][string] $PrimaryBranch,
	[Parameter(Mandatory)][string] $ExpectedCurrentTip,
	[Parameter(Mandatory)][string] $ExpectedPrimaryTip,
	[Parameter(Mandatory)][string] $SessionLabel,
	[Parameter(Mandatory)][string] $ApprovedSessionCommit,
	[Parameter(Mandatory)][string] $ApprovedCandidateTree,
	# Approval preparation result artifact ('broken-engine-finalize-approval-preparation/v3'); the
	# landing reads the approved Contract identity from it so no digest is ever hand-copied.
	[string] $ApprovalPreparationResultFile,
	# Approval review receipt artifact ('broken-engine-finalize-approval-review/v1'); the landing
	# reads the reviewed tip and launch status from it so the SmartGit review cannot be skipped.
	[string] $ApprovalReviewResultFile,
	# Fixture-only Contract identity scalars: fixtures inject crafted identities directly; outside the
	# fixture environment the identity always comes from -ApprovalPreparationResultFile.
	[string] $HistoryContractDigest,
	[string] $HistoryContractGeneratorDigest,
	[string] $HistoryContractCaptureDigest,
	[string] $HistoryContractRuntimeDigest,
	[string] $HistoryContractPatchDigest,
	[string] $HistoryContractMode,
	[string] $HistoryContractRowDate,
	[string] $HistoryContractCoverage,
	[string] $HistoryJsonSha256,
	[string] $HistorySvgSha256,
	[string] $HistorySvgEmbeddedSha256,
	[int] $HistoryJsonBytes = 0,
	[int] $HistorySvgBytes = 0,
	# The caller's post-confirmation lease token. Supplied, the landing continues under that same
	# lease instead of minting one; omitted, a matching retained landing claim may be adopted.
	[string] $OwnerToken,
	[switch] $ReleasePlanClaim,
	# Total seconds this landing may spend waiting out foreign primary index.lock contention.
	[ValidateRange(1, 3600)][int] $IndexLockWaitSeconds = 500,
	[ValidateSet('none', 'compare-and-swap', 'post-reset', 'post-update-ref', 'bounded-diagnostic', 'retry-patch-mismatch', 'history-generate', 'history-invalid', 'history-source-race', 'history-recovery-primary-race', 'history-recovery-active-primary-race', 'history-recovery-active-primary-edit', 'history-recovery-session-race', 'history-recovery-session-staged-race')][string] $FixtureFailure = 'none'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$commonModule = Join-Path $PSScriptRoot '..\..\..\scripts\FinalizeWorkflowCommon.psm1'
if (-not (Test-Path -LiteralPath $commonModule)) {
	$commonModule = Join-Path $PSScriptRoot '..\..\..\..\.agents\scripts\FinalizeWorkflowCommon.psm1'
}
Import-Module $commonModule -Force

$exclusionModule = Join-Path $PSScriptRoot '..\..\..\scripts\WorktreeCliSessionExclusion.psm1'
if (-not (Test-Path -LiteralPath $exclusionModule)) {
	$exclusionModule = Join-Path $PSScriptRoot '..\..\..\..\.agents\scripts\WorktreeCliSessionExclusion.psm1'
}
Import-Module $exclusionModule -Force

$result = [ordered]@{
	schemaVersion = 'broken-engine-finalize-landing/v4'
	status = 'error'
	code = 'internal.error'
	message = 'Landing transaction did not complete.'
	primaryAdvanced = $false
	identities = [ordered]@{ currentWorktree = $null; primaryWorktree = $null; gitCommonDirectory = $null; currentBranch = $null; primaryBranch = $null }
	tips = [ordered]@{ approvedSession = $ApprovedSessionCommit; expectedCurrent = $ExpectedCurrentTip; expectedPrimary = $ExpectedPrimaryTip; current = $null; primary = $null }
	candidate = [ordered]@{ commit = $ApprovedSessionCommit; tree = $ApprovedCandidateTree; treeVerified = $false }
	approvedSource = [ordered]@{ commit = $ApprovedSessionCommit; tree = $ApprovedCandidateTree; parent = $null; patch = $null; metadata = $null }
	rebasedSource = [ordered]@{ commit = $null; tree = $null; parent = $null; patch = $null }
	historyContract = $null
	historyUpdate = [ordered]@{ status = 'not-run'; receipt = $null; rowDate = $null; jsonl = $null; svg = $null }
	final = [ordered]@{ commit = $null; tree = $null; parent = $null; replacement = $false }
	landed = [ordered]@{ commit = $null; tree = $null; rebaseAttempts = 0 }
	locks = [ordered]@{ landingOwner = $null; landingClaimed = $false; landingReleased = $false; claim = $null }
	planClaim = [ordered]@{ requested = [bool]$ReleasePlanClaim; released = $false }
	cleanup = [ordered]@{ worktreesClear = $null; worktreeProblems = @() }
	disposition = 'terminal'
	requiresUserAuthority = $false
	retryAfterMilliseconds = 0
	blocker = $null
	residuals = [Collections.Generic.List[string]]::new()
}
$script:WorktreeCliPath = $null
# What this invocation is actually landing right now. It starts as the user-confirmed candidate on
# the parent that candidate was prepared on and is replaced, in Invoke-LandingRebaseOntoPrimary
# alone, by the rebased commit and its new base, so every later ancestry, tree, advance, and
# rollback check reads the state the retry produced instead of the stale anchors the caller passed in.
$script:LandingPrimaryTip = $ExpectedPrimaryTip
$script:LandingCommit = $ApprovedSessionCommit
$script:LandingTree = $ApprovedCandidateTree
$script:FinalCommit = $null
$script:FinalTree = $null
$script:FrozenCommitMetadata = $null
$script:ApprovedPatchIdentity = $null
# The base every approved-source measurement is taken from: the confirmed candidate's own parent,
# which may be behind live primary when primary advanced after approval preparation.
$script:ApprovedSourceParent = $null
$script:HistoryTempRoot = $null
$script:HistoryTempParent = $null
$script:LandingOwner = $null
$script:LandingClaimed = $false
# The lease duration this landing needs to hold through the whole advance. WorktreeCli's refresh
# re-expires a lease with its own recorded duration and can never lengthen it, so a continued caller
# lease minted shorter than this could expire mid-advance and is refused instead.
$script:LandingLeaseSeconds = 3600
# Set only where the session branch could not be proven restored to the confirmed commit: while it is
# set the lease is retained unconditionally, because releasing it would expose an unproven worktree.
$script:LandingRestorationUnproven = $false
$script:PrimaryIdentity = $null
$script:CurrentIdentity = $null
$script:LandingSession = $null
$script:LandingOwnerAdopted = $false
$script:FailureExitCode = 0
$script:FailureCode = $null
$script:FailureMessage = $null
$script:LandingTransientOwner = $null
# Waiting is accumulated across the whole invocation, not per operation: the guarded checkout runs up
# to four times in one landing (advance and rollback restore, before and after the internal rebase),
# so a per-call budget would let one run wait four times the bound its caller sized a timeout for.
# The stopwatch is also the retry-path evidence: the projection emits a diagnostic when it ever ran.
$script:IndexLockWait = [Diagnostics.Stopwatch]::new()

function Get-BoundedLandingText([AllowNull()] $Value, [int] $Limit) {
	if ($null -eq $Value) { return [pscustomobject]@{ Text = $null; Length = 0; Truncated = $false } }
	$Value = [string]$Value
	return [pscustomobject]@{ Text = $(if ($Value.Length -gt $Limit) { $Value.Substring(0, $Limit) } else { $Value }); Length = $Value.Length; Truncated = ($Value.Length -gt $Limit) }
}

function Get-LandingGitObjectId([AllowNull()] $Value) {
	if ($null -ne $Value -and [string]$Value -cmatch '^[0-9a-f]{40}\z') { return [string]$Value }
	return $null
}

function Get-HistoryReceiptDigest($Value) {
	$json = $Value | ConvertTo-Json -Compress -Depth 64
	return [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.UTF8Encoding]::new($false).GetBytes($json))).ToLowerInvariant()
}

function New-LandingCollection([object[]] $Values, [scriptblock] $Project, [string] $Requery) {
	[object[]]$all = @($Values)
	for ($index = 1; $index -lt $all.Count; $index++) {
		$value = $all[$index]; $cursor = $index - 1
		while ($cursor -ge 0 -and [StringComparer]::Ordinal.Compare([string]$all[$cursor], [string]$value) -gt 0) { $all[$cursor + 1] = $all[$cursor]; $cursor-- }
		$all[$cursor + 1] = $value
	}
	$items = @($all | Select-Object -First 16 | ForEach-Object { & $Project $_ })
	$truncated = $all.Count -gt 16
	return [ordered]@{ totalCount = $all.Count; items = $items; truncated = $truncated; selector = $null; requery = $(if ($truncated) { $Requery } else { $null }) }
}

function New-LandingProjection {
	$message = Get-BoundedLandingText ([string]$result.message) 512
	$code = Get-BoundedLandingText ([string]$result.code) 128
	$diagnosticValues = [Collections.Generic.List[object]]::new()
	if ($result.status -cne 'landed') { $diagnosticValues.Add([pscustomobject]@{ source = 'Invoke-FinalizeLanding'; code = $result.code; message = $result.message }) }
	if ($script:IndexLockWait.Elapsed.TotalMilliseconds -gt 0) { $diagnosticValues.Add([pscustomobject]@{ source = 'Invoke-FinalizeLanding'; code = 'git.index-lock-wait'; message = "Foreign primary index.lock contention was waited out for $([int]$script:IndexLockWait.Elapsed.TotalSeconds) seconds." }) }
	$diagnostics = New-LandingCollection $diagnosticValues.ToArray() {
		param($entry)
		$sourceValue = if ($entry.PSObject.Properties.Name -ccontains 'source') { [string]$entry.source } else { 'WorktreeCli' }
		$codeValue = if ($entry.PSObject.Properties.Name -ccontains 'code') { [string]$entry.code } else { 'diagnostic' }
		$pathValue = if ($entry.PSObject.Properties.Name -ccontains 'path') { [string]$entry.path } elseif ($entry.PSObject.Properties.Name -ccontains 'plan') { [string]$entry.plan } else { $null }
		$messageValue = if ($entry.PSObject.Properties.Name -ccontains 'message') { [string]$entry.message } else { [string]$entry }
		$codeText = Get-BoundedLandingText $codeValue 128; $pathText = Get-BoundedLandingText $pathValue 1024; $messageText = Get-BoundedLandingText $messageValue 512
		[ordered]@{ source = $sourceValue; code = $codeText.Text; codeLength = $codeText.Length; codeTruncated = $codeText.Truncated; path = $pathText.Text; pathLength = $pathText.Length; pathTruncated = $pathText.Truncated; message = $messageText.Text; messageLength = $messageText.Length; messageTruncated = $messageText.Truncated }
	} 'Invoke-FinalizeLanding'
	$problems = New-LandingCollection @($result.cleanup.worktreeProblems) {
		param($problem)
		$pathValue = if ($problem.PSObject.Properties.Name -ccontains 'path') { [string]$problem.path } else { $null }
		$messageValue = if ($problem.PSObject.Properties.Name -ccontains 'message') { [string]$problem.message } else { [string]$problem }
		$pathText = Get-BoundedLandingText $pathValue 1024; $messageText = Get-BoundedLandingText $messageValue 512
		[ordered]@{ path = $pathText.Text; pathLength = $pathText.Length; pathTruncated = $pathText.Truncated; message = $messageText.Text; messageLength = $messageText.Length; messageTruncated = $messageText.Truncated }
	} 'Invoke-FinalizeLanding'
	$residuals = New-LandingCollection @($result.residuals) { param($residual); $text = Get-BoundedLandingText ([string]$residual) 512; [ordered]@{ message = $text.Text; messageLength = $text.Length; messageTruncated = $text.Truncated } } 'Invoke-FinalizeLanding'
	return [ordered]@{
		schemaVersion = 'broken-engine-finalize-landing/v4'; status = $result.status; code = $code.Text; message = $message.Text; messageLength = $message.Length; messageTruncated = $message.Truncated
		primaryAdvanced = [bool]$result.primaryAdvanced; candidate = [ordered]@{ commit = Get-LandingGitObjectId $result.candidate.commit; tree = Get-LandingGitObjectId $result.candidate.tree; treeVerified = [bool]$result.candidate.treeVerified }
		landed = [ordered]@{ commit = $(if ($result.status -ceq 'landed') { Get-LandingGitObjectId $result.landed.commit } else { $null }); tree = $(if ($result.status -ceq 'landed') { Get-LandingGitObjectId $result.landed.tree } else { $null }); rebaseAttempts = [int]$result.landed.rebaseAttempts }
		approvedSource = [ordered]@{ commit = Get-LandingGitObjectId $result.approvedSource.commit; tree = Get-LandingGitObjectId $result.approvedSource.tree; parent = Get-LandingGitObjectId $result.approvedSource.parent; patch = $result.approvedSource.patch; metadata = $result.approvedSource.metadata }
		rebasedSource = [ordered]@{ commit = Get-LandingGitObjectId $result.rebasedSource.commit; tree = Get-LandingGitObjectId $result.rebasedSource.tree; parent = Get-LandingGitObjectId $result.rebasedSource.parent; patch = $result.rebasedSource.patch }
		historyContract = $result.historyContract; historyUpdate = $result.historyUpdate
		final = [ordered]@{ commit = $(if ($result.status -ceq 'landed') { Get-LandingGitObjectId $result.final.commit } else { $null }); tree = $(if ($result.status -ceq 'landed') { Get-LandingGitObjectId $result.final.tree } else { $null }); parent = $(if ($result.status -ceq 'landed') { Get-LandingGitObjectId $result.final.parent } else { $null }); replacement = [bool]$result.final.replacement }
		planClaim = [ordered]@{ requested = [bool]$result.planClaim.requested; released = [bool]$result.planClaim.released }
		lock = [ordered]@{ claimed = [bool]$result.locks.landingClaimed; released = [bool]$result.locks.landingReleased; claimCode = $(if ($null -ne $result.locks.claim) { [string]$result.locks.claim.code } else { $null }); disposition = $result.disposition; requiresUserAuthority = [bool]$result.requiresUserAuthority; retryAfterMilliseconds = [int]$result.retryAfterMilliseconds; attempts = $(if ($null -ne $result.locks.claim) { [int]$result.locks.claim.attempts } else { 0 }) }
		cleanup = [ordered]@{ worktreesClear = $result.cleanup.worktreesClear; problems = $problems }
		disposition = $result.disposition; requiresUserAuthority = [bool]$result.requiresUserAuthority; retryAfterMilliseconds = [int]$result.retryAfterMilliseconds; diagnostics = $diagnostics; residuals = $residuals
	}
}

function Throw-Landing([int] $ExitCode, [string] $Code, [string] $Message, [string] $Disposition = 'terminal', [bool] $RequiresUserAuthority = $false, [int] $RetryAfterMilliseconds = 0) {
	$exception = [InvalidOperationException]::new($Message)
	$exception.Data['FinalizeExitCode'] = $ExitCode
	$exception.Data['FinalizeCode'] = $Code
	$exception.Data['FinalizeDisposition'] = $Disposition
	$exception.Data['FinalizeRequiresUserAuthority'] = $RequiresUserAuthority
	$exception.Data['FinalizeRetryAfterMilliseconds'] = $RetryAfterMilliseconds
	throw $exception
}

function Get-JsonResponse($Response, [string] $Operation) {
	if ([string]::IsNullOrWhiteSpace($Response.Stdout)) {
		Throw-Landing 1 'worktreecli.no-json' "$Operation returned no JSON. stderr: $($Response.Stderr.Trim())"
	}
	try {
		return $Response.Stdout.Trim() | ConvertFrom-Json -Depth 32 -ErrorAction Stop
	}
	catch {
		Throw-Landing 1 'worktreecli.invalid-json' "$Operation returned invalid JSON: $($Response.Stdout.Trim())"
	}
}

function Get-HistoryArtifactPath($Artifact, [string] $Kind, [string] $TempRoot) {
	if ($null -eq $Artifact -or [string]::IsNullOrWhiteSpace([string]$Artifact.path)) { Throw-Landing 2 'history.output-invalid' "Generate did not identify the $Kind output." }
	$path = [string]$Artifact.path
	if ([IO.Path]::IsPathRooted($path)) { Throw-Landing 2 'history.output-invalid' "Generate $Kind output path must be repository-relative." }
	return [IO.Path]::GetFullPath((Join-Path $script:CurrentIdentity.Worktree ($path -replace '/', '\')))
}

function Assert-HistoryTempPath([string] $Path, [string] $TempRoot, [string] $Kind) {
	$full = [IO.Path]::GetFullPath($Path)
	$root = [IO.Path]::GetFullPath($TempRoot).TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
	if (-not $full.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) { Throw-Landing 2 'history.output-invalid' "Generate $Kind output escaped ignored Temp." }
	$item = Get-Item -LiteralPath $full -Force -ErrorAction Stop
	if ($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) { Throw-Landing 2 'history.output-invalid' "Generate $Kind output is not an ordinary file." }
	if ($item.Name -ine "CodeQualityMetricsHistory.$Kind") { Throw-Landing 2 'history.output-invalid' "Generate $Kind output does not use the reserved CodeQualityMetricsHistory.$Kind filename." }
	return $item
}

function Get-HistoryPatchDigest($Patch) {
	$identity = [ordered]@{ changes = $Patch.changes; metricSupportedChanges = $Patch.metricSupportedChanges; cppChanged = $Patch.cppChanged }
	$json = $identity | ConvertTo-Json -Compress -Depth 64
	return [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.UTF8Encoding]::new($false).GetBytes($json))).ToLowerInvariant()
}

function Invoke-HistoryGenerate([string] $SourceCommit, [string] $PrimaryTip) {
	$script:HistoryTempParent = Join-Path $script:CurrentIdentity.Worktree 'Temp\FinalizeHistory'
	New-Item -ItemType Directory -Path $script:HistoryTempParent -Force | Out-Null
	$relativeTempParent = [IO.Path]::GetRelativePath($script:CurrentIdentity.Worktree, $script:HistoryTempParent).Replace('\', '/')
	if (-not (Test-FinalizeGitSuccess $script:CurrentIdentity.Worktree @('check-ignore', '-q', '--', $relativeTempParent))) { Throw-Landing 2 'history.temp-not-ignored' 'History Generate output must be written below an ignored Temp directory.' }
	$script:HistoryTempRoot = Join-Path $script:HistoryTempParent ([guid]::NewGuid().ToString('N'))
	if (Test-Path -LiteralPath $script:HistoryTempRoot) { Throw-Landing 2 'history.temp-collision' 'History Generate selected an existing output directory.' }
	$historyScript = Join-Path $script:CurrentIdentity.Worktree '.agents\skills\code-quality-metrics\scripts\Invoke-CodeQualityMetricsHistory.ps1'
	if (-not (Test-Path -LiteralPath $historyScript -PathType Leaf)) { Throw-Landing 1 'history.generate-unavailable' "The code-quality history Generate producer is missing: '$historyScript'." }
	if ($FixtureFailure -ceq 'history-generate') { Throw-Landing 2 'history.generate-failed' 'Fixture forced history Generate failure.' }
	$arguments = @('-NoProfile', '-File', $historyScript, '-Mode', 'Generate', '-RepositoryRoot', $script:CurrentIdentity.Worktree, '-BaseCommit', $PrimaryTip, '-TipCommit', $SourceCommit, '-DateUtc', $HistoryContractRowDate, '-OutputDirectory', $script:HistoryTempRoot)
	$response = Invoke-FinalizeNativeText "$PSHOME\pwsh.exe" $arguments $script:CurrentIdentity.Worktree
	if ($response.ExitCode -ne 0) { Throw-Landing 2 'history.generate-failed' "History Generate failed: $($response.Stderr.Trim())" }
	$lines = @($response.Stdout -split "`r?`n" | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
	if ($lines.Count -ne 1) { Throw-Landing 2 'history.update-invalid' 'History Generate did not return exactly one JSON receipt.' }
	try { $receipt = $lines[0] | ConvertFrom-Json -Depth 64 -ErrorAction Stop } catch { Throw-Landing 2 'history.update-invalid' "History Generate returned invalid JSON: $($_.Exception.Message)" }
	return $receipt
}

function Validate-HistoryUpdate($Receipt, [string] $SourceCommit) {
	try { [void](Assert-FinalizeHistoryUpdateReceipt $Receipt $script:LandingPrimaryTip $SourceCommit) }
	catch { Throw-Landing 2 'history.update-invalid' $_.Exception.Message }
	$actualDate = [string]$Receipt.date
	$json = $Receipt.outputs.jsonl; $svg = $Receipt.outputs.svg
	$jsonPath = Get-HistoryArtifactPath $json 'jsonl' $script:HistoryTempRoot; $svgPath = Get-HistoryArtifactPath $svg 'svg' $script:HistoryTempRoot
	$jsonItem = Assert-HistoryTempPath $jsonPath $script:HistoryTempRoot 'JSONL'; $svgItem = Assert-HistoryTempPath $svgPath $script:HistoryTempRoot 'SVG'
	$expectedJson = '.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl'; $expectedSvg = '.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.svg'
	$actualJsonHash = (Get-FileHash $jsonItem.FullName -Algorithm SHA256).Hash.ToLowerInvariant(); $actualSvgHash = (Get-FileHash $svgItem.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
	if ([string]$json.sha256 -cne $actualJsonHash) { Throw-Landing 2 'history.update-invalid' 'History Generate JSONL hash does not match its bytes.' }
	if ([string]$svg.sha256 -cne $actualSvgHash) { Throw-Landing 2 'history.update-invalid' 'History Generate SVG hash does not match its bytes.' }
	if ([int64]$json.bytes -ne $jsonItem.Length) { Throw-Landing 2 'history.update-invalid' 'History Generate JSONL size does not match its bytes.' }
	if ([int64]$svg.bytes -ne $svgItem.Length) { Throw-Landing 2 'history.update-invalid' 'History Generate SVG size does not match its bytes.' }
	$series = $Receipt.series; $row = $series.row
	$currentContractMode = if ($null -ne $result.historyContract.current) { [string]$result.historyContract.current.mode } else { $HistoryContractMode }
	if ($currentContractMode -and [string]$row.captureMode -cne $currentContractMode) { Throw-Landing 2 'history.update-contract-mismatch' 'History Generate row capture mode differs from the re-evaluated Contract.' }
	$svgText = [IO.File]::ReadAllText($svgItem.FullName, [Text.UTF8Encoding]::new($false, $true)); $seriesMatch = [regex]::Match($svgText, 'seriesDigest=([0-9a-f]{64})')
	if (-not $seriesMatch.Success -or $seriesMatch.Groups[1].Value -cne $actualJsonHash) { Throw-Landing 2 'history.update-invalid' 'History Generate SVG embedded series digest does not match JSONL bytes.' }
	if (-not [string]::IsNullOrWhiteSpace($HistorySvgEmbeddedSha256) -and $seriesMatch.Groups[1].Value -cne $HistorySvgEmbeddedSha256) { Throw-Landing 2 'history.update-contract-mismatch' 'History Generate SVG embedded digest differs from the approved recovery digest.' }
	if ($HistoryContractGeneratorDigest) { $generatorMatch = [regex]::Match($svgText, 'generatorDigest=([0-9a-f]{64})'); if (-not $generatorMatch.Success -or $generatorMatch.Groups[1].Value -cne $HistoryContractGeneratorDigest) { Throw-Landing 2 'history.update-contract-mismatch' 'History Generate SVG embedded generator digest differs from the approved Contract.' } }
	if ($null -ne $Receipt.capture) {
		$captureDigest = [string]$Receipt.capture.digest; if ($captureDigest) { $captureMatch = [regex]::Match($svgText, 'captureDigest=([0-9a-f]{64})'); if (-not $captureMatch.Success -or $captureMatch.Groups[1].Value -cne $captureDigest) { Throw-Landing 2 'history.update-invalid' 'History Generate SVG embedded capture digest does not match the receipt.' } }
		$identityDigest = [string]$Receipt.capture.bootstrapIdentityDigest; if ($identityDigest) { $identityMatch = [regex]::Match($svgText, 'identityDigest=([0-9a-f]{64})'); if (-not $identityMatch.Success -or $identityMatch.Groups[1].Value -cne $identityDigest) { Throw-Landing 2 'history.update-invalid' 'History Generate SVG embedded runtime identity digest does not match the receipt.' } }
		$scbDigest = [string]$Receipt.capture.scbContentDigest; if ($scbDigest) { $scbMatch = [regex]::Match($svgText, 'scbDigest=([0-9a-f]{64})'); if (-not $scbMatch.Success -or $scbMatch.Groups[1].Value -cne $scbDigest) { Throw-Landing 2 'history.update-invalid' 'History Generate SVG embedded scb digest does not match the receipt.' } }
	}
	if ($FixtureFailure -ceq 'history-source-race') { Throw-Landing 2 'history.source-changed' 'Fixture forced a source/history race.' }
	$publicReceipt = $Receipt | ConvertTo-Json -Depth 64 -Compress | ConvertFrom-Json -Depth 64
	$publicReceipt.outputs.jsonl.path = $expectedJson
	$publicReceipt.outputs.svg.path = $expectedSvg
	$result.historyUpdate = [ordered]@{ status = 'pass'; receipt = $publicReceipt; rowDate = $actualDate; jsonl = [ordered]@{ path = $expectedJson; bytes = [int64]$jsonItem.Length; sha256 = $actualJsonHash }; svg = [ordered]@{ path = $expectedSvg; bytes = [int64]$svgItem.Length; sha256 = $actualSvgHash; embeddedSha256 = $seriesMatch.Groups[1].Value } }
	return [pscustomobject]@{ JsonPath = $jsonItem.FullName; SvgPath = $svgItem.FullName; JsonHash = $actualJsonHash; SvgHash = $actualSvgHash; JsonBytes = [int64]$jsonItem.Length; SvgBytes = [int64]$svgItem.Length }
}

function Invoke-HistoryContractForLanding([string] $BaseCommit, [string] $TipCommit) {
	$historyScript = Join-Path $script:CurrentIdentity.Worktree '.agents\skills\code-quality-metrics\scripts\Invoke-CodeQualityMetricsHistory.ps1'
	if (-not (Test-Path -LiteralPath $historyScript -PathType Leaf)) { Throw-Landing 1 'history.contract-unavailable' "The code-quality history Contract producer is missing: '$historyScript'." }
	$arguments = @('-NoProfile', '-File', $historyScript, '-Mode', 'Contract', '-RepositoryRoot', $script:CurrentIdentity.Worktree, '-BaseCommit', $BaseCommit, '-TipCommit', $TipCommit)
	$response = Invoke-FinalizeNativeText "$PSHOME\pwsh.exe" $arguments $script:CurrentIdentity.Worktree
	if ($response.ExitCode -ne 0) { Throw-Landing 2 'history.contract-failed' "History Contract re-evaluation failed: $($response.Stderr.Trim())" }
	$lines = @($response.Stdout -split "`r?`n" | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
	if ($lines.Count -ne 1) { Throw-Landing 2 'history.contract-invalid' 'History Contract re-evaluation did not return exactly one JSON receipt.' }
	try { $receipt = $lines[0] | ConvertFrom-Json -Depth 64 -ErrorAction Stop } catch { Throw-Landing 2 'history.contract-invalid' "History Contract re-evaluation returned invalid JSON: $($_.Exception.Message)" }
	try { [void](Assert-FinalizeHistoryContractReceipt $receipt $BaseCommit $TipCommit) }
	catch { Throw-Landing 2 'history.contract-invalid' $_.Exception.Message }
	return $receipt
}

function Validate-HistoryContractForLanding([string] $BaseCommit, [string] $TipCommit) {
	$receipt = Invoke-HistoryContractForLanding $BaseCommit $TipCommit
	$generator = [string]$receipt.generator.sha256
	if ($HistoryContractGeneratorDigest -and $generator -cne $HistoryContractGeneratorDigest) { Throw-Landing 2 'history.contract-changed' 'The reviewed history generator identity changed after confirmation.' }
	$currentMode = [string]$receipt.decision.captureMode; $approvedMode = [string]$HistoryContractMode
	$modeAllowed = $currentMode -ceq $approvedMode -or ($approvedMode -ceq 'catch-up' -and $currentMode -cin @('cpp-change', 'carry-forward'))
	if (-not $modeAllowed) { Throw-Landing 2 'history.contract-changed' "History capture mode changed from '$approvedMode' to '$currentMode'." }
	$currentCapture = if ($null -ne $receipt.capture) { [string]$receipt.capture.digest } else { $null }
	$catchUpNarrowedToCarry = $approvedMode -ceq 'catch-up' -and $currentMode -ceq 'carry-forward' -and [string]::IsNullOrWhiteSpace($currentCapture)
	if (-not $catchUpNarrowedToCarry) {
		if ($currentMode -in @('catch-up', 'cpp-change') -and [string]::IsNullOrWhiteSpace($HistoryContractCaptureDigest)) { Throw-Landing 2 'history.contract-changed' 'An active history capture identity was not present in the approved Contract.' }
		if ($HistoryContractCaptureDigest -and $currentCapture -cne $HistoryContractCaptureDigest) { Throw-Landing 2 'history.contract-changed' 'The reviewed history capture/runtime identity changed after confirmation.' }
		if ($currentMode -in @('catch-up', 'cpp-change') -and [string]::IsNullOrWhiteSpace($currentCapture)) { Throw-Landing 2 'history.contract-changed' 'The current active history capture identity is missing.' }
	}
	$currentRuntime = if ($null -ne $receipt.capture) { [string]$receipt.capture.bootstrapIdentityDigest } else { $null }
	if (-not $catchUpNarrowedToCarry -and $HistoryContractRuntimeDigest -and $currentRuntime -cne $HistoryContractRuntimeDigest) { Throw-Landing 2 'history.contract-changed' 'The reviewed history runtime identity changed after confirmation.' }
	$currentPatchDigest = Get-HistoryPatchDigest $receipt.patch
	if ($HistoryContractPatchDigest -and $currentPatchDigest -cne $HistoryContractPatchDigest) { Throw-Landing 2 'history.source-changed' "The current history Contract patch digest '$currentPatchDigest' differs from the approved source patch classification '$HistoryContractPatchDigest'." }
	$currentIdentity = Get-LandingPatchIdentity $BaseCommit $TipCommit
	if ($null -ne $script:ApprovedPatchIdentity -and ($currentIdentity.PatchId -cne $script:ApprovedPatchIdentity.PatchId -or $currentIdentity.Changes -cne $script:ApprovedPatchIdentity.Changes)) { Throw-Landing 2 'history.source-changed' 'The non-history source patch changed after confirmation.' }
	if (-not $catchUpNarrowedToCarry -and $BaseCommit -ceq $script:ApprovedSourceParent -and $HistoryContractDigest -and (Get-HistoryReceiptDigest $receipt) -cne $HistoryContractDigest) { Throw-Landing 2 'history.contract-changed' 'History Contract aggregate digest changed while the approved candidate parent remained unchanged.' }
	$result.historyContract.current = [ordered]@{ digest = Get-HistoryReceiptDigest $receipt; mode = $currentMode; generatorDigest = $generator; captureDigest = $currentCapture; patch = $receipt.patch; historyBytesSha256 = [string]$receipt.series.historyBytesSha256 }
	return $receipt
}

function Invoke-WorktreeCli([string[]] $Arguments) {
	return Invoke-FinalizeNativeText $script:WorktreeCliPath $Arguments $script:CurrentIdentity.Worktree
}

function Assert-LandingSanity([string] $ExpectedSessionTip, [string] $ExpectedPrimaryTipValue) {
	$sanity = Test-FinalizeLandingSanity -SessionWorktree $CurrentWorktree -PrimaryWorktree $PrimaryWorktree `
		-SessionBranch $CurrentBranch -PrimaryBranch $PrimaryBranch -ExpectedSessionTip $ExpectedSessionTip -ExpectedPrimaryTip $ExpectedPrimaryTipValue
	if (-not $sanity.Ok) { Throw-Landing 2 "sanity.$($sanity.Code)" "Landing sanity failed: $($sanity.Message)" }
	$script:WorktreeCliPath = $sanity.WorktreeCliExecutable
	return $sanity
}

function Assert-LandingOwner {
	$response = Invoke-WorktreeCli @('lock', 'status', '--repo', $result.identities.gitCommonDirectory)
	$status = Get-JsonResponse $response 'landing lock status'
	if ($response.ExitCode -ne 0 -or $status.owner -cne $script:LandingOwner -or $status.leaseState -cne 'live') {
		Throw-Landing 2 'landing-lock.not-owned' 'Landing lock is not live and owned by this transaction.'
	}
}

function Refresh-LandingOwner {
	$response = Invoke-WorktreeCli @('lock', 'refresh', '--repo', $result.identities.gitCommonDirectory, '--owner', $script:LandingOwner)
	if ($response.ExitCode -ne 0) {
		Throw-Landing 2 'landing-lock.refresh-failed' "Landing lock refresh failed: $($response.Stdout.Trim())$($response.Stderr.Trim())"
	}
	Assert-LandingOwner
}

function Acquire-RecoveryLandingLock {
	$script:LandingSession = if ([string]::IsNullOrWhiteSpace($OwnerToken)) { "$SessionLabel/landing" } else { $SessionLabel }
	$lockState = Get-FinalizeLandingLockState $script:WorktreeCliPath $result.identities.gitCommonDirectory $script:CurrentIdentity.Worktree
	$sameActor = $false
	if ($lockState.Kind -ceq 'live' -and @($lockState.Status.PSObject.Properties.Name) -ccontains 'owner') {
		$owner = [string]$lockState.Status.owner
		$sameActor = Test-FinalizeLandingLockClaimIdentity $lockState.Status $owner $script:LandingSession $script:CurrentIdentity.Worktree
		if (-not [string]::IsNullOrWhiteSpace($OwnerToken)) { $sameActor = $sameActor -and $owner -ceq $OwnerToken }
		if ($sameActor) { $script:LandingOwner = $owner }
	}
	if (-not $sameActor) {
		$tokenResponse = Invoke-WorktreeCli @('lock', 'token')
		if ($tokenResponse.ExitCode -ne 0 -or $tokenResponse.Stdout.Trim() -cnotmatch '^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$') { Throw-Landing 1 'landing-lock.token-failed' 'WorktreeCli could not generate a canonical recovery owner token.' }
		$script:LandingOwner = $tokenResponse.Stdout.Trim()
	}
	$result.locks.landingOwner = $script:LandingOwner
	$claimOutcome = if ($sameActor) { [pscustomobject]@{ Claimed = $true; Code = 'ok'; Message = 'Recovery continues under the matching landing lease.'; Disposition = 'terminal'; RequiresUserAuthority = $false; RetryAfterMilliseconds = 0; Owner = $script:LandingOwner; Lock = $lockState.Status; Attempts = 1 } } else { Invoke-FinalizeLandingLockClaim -WorktreeCliExecutable $script:WorktreeCliPath -GitCommonDirectory $result.identities.gitCommonDirectory -Owner $script:LandingOwner -Session $script:LandingSession -Worktree $script:CurrentIdentity.Worktree -LeaseSeconds $script:LandingLeaseSeconds -WaitSeconds 300 }
	$result.locks.claim = [ordered]@{ code = $claimOutcome.Code; disposition = $claimOutcome.Disposition; requiresUserAuthority = $claimOutcome.RequiresUserAuthority; retryAfterMilliseconds = $claimOutcome.RetryAfterMilliseconds; attempts = $claimOutcome.Attempts; lock = $claimOutcome.Lock }
	if (-not $claimOutcome.Claimed) { Throw-Landing 2 'landing-lock.claim-failed' $claimOutcome.Message $claimOutcome.Disposition $claimOutcome.RequiresUserAuthority $claimOutcome.RetryAfterMilliseconds }
	$script:LandingClaimed = $true; $result.locks.landingClaimed = $true
}

function Release-LandingLockIfSafe {
	if (-not $script:LandingClaimed) { return }
	if ($script:LandingRestorationUnproven) {
		$result.residuals.Add('Landing lock retained: the confirmed session commit could not be proven restored.')
		return
	}
	$clear = Test-FinalizeAllWorktreesClear $script:PrimaryIdentity.Worktree
	$result.cleanup.worktreesClear = $clear.Clear
	$result.cleanup.worktreeProblems = @($clear.Problems)
	if (-not $clear.Clear) {
		foreach ($problem in $clear.Problems) { $result.residuals.Add("Landing lock retained: $problem") }
		return
	}
	try {
		Refresh-LandingOwner
		$response = Invoke-WorktreeCli @('lock', 'release', '--repo', $result.identities.gitCommonDirectory, '--owner', $script:LandingOwner)
		if ($response.ExitCode -ne 0) {
			$result.residuals.Add("Landing lock release failed: $($response.Stdout.Trim())$($response.Stderr.Trim())")
			return
		}
		$script:LandingClaimed = $false
		$result.locks.landingReleased = $true
	}
	catch {
		$result.residuals.Add("Landing lock release error: $($_.Exception.Message)")
	}
}

# The approved inputs bind this landing whether or not primary moved: the commit the user confirmed
# must still carry the tree the user confirmed before any advance, retry, or recovery path runs.
function Assert-ApprovedCandidateTree {
	if ((Invoke-FinalizeGit $script:PrimaryIdentity.Worktree @('rev-parse', "$ApprovedSessionCommit^{tree}")).Trim() -cne $ApprovedCandidateTree) {
		Throw-Landing 2 'candidate.tree-changed' 'Approved session commit no longer has the reviewed candidate tree.'
	}
	$result.candidate.treeVerified = $true
}

function Assert-PrimaryAdvanceState {
	$primary = Get-FinalizeGitIdentity $PrimaryWorktree 'Primary worktree'
	$current = Get-FinalizeGitIdentity $CurrentWorktree 'Session worktree'
	if ($primary.Worktree -cne $script:PrimaryIdentity.Worktree -or $current.Worktree -cne $script:CurrentIdentity.Worktree -or
		$primary.Branch -cne $PrimaryBranch -or $current.Branch -cne $CurrentBranch -or
		$primary.Head -cne $script:LandingPrimaryTip -or $current.Head -cne $script:LandingCommit) {
		Throw-Landing 2 'git.identity-changed' 'Primary or session identity changed after approval.'
	}
	if ((Invoke-FinalizeGit $primary.Worktree @('status', '--porcelain', '-z', '--untracked-files=all')).Length -ne 0) {
		Throw-Landing 2 'git.primary-dirty' 'Primary worktree is not clean immediately before landing.'
	}
	Assert-HistoryPathsClean $primary.Worktree
	if ((Invoke-FinalizeGit $primary.Worktree @('rev-parse', "$script:LandingCommit^{tree}")).Trim() -cne $script:LandingTree) {
		Throw-Landing 2 'candidate.tree-changed' 'Approved session commit no longer has the reviewed candidate tree.'
	}
	if (-not (Test-FinalizeGitSuccess $primary.Worktree @('merge-base', '--is-ancestor', $script:LandingPrimaryTip, $script:LandingCommit))) {
		Throw-Landing 2 'git.primary-not-ancestor' 'Approved session commit does not descend from the approved primary tip.'
	}
	if ((Invoke-FinalizeGit $primary.Worktree @('rev-list', "$($script:LandingCommit)..$($script:LandingPrimaryTip)")).Trim().Length -ne 0 -or
		(Invoke-FinalizeGit $primary.Worktree @('rev-list', '--min-parents=2', "$($script:LandingPrimaryTip)..$($script:LandingCommit)")).Trim().Length -ne 0) {
		Throw-Landing 2 'git.landing-history-invalid' 'Landing would replay primary commits or introduce a merge commit.'
	}
}

# git reports index.lock contention as a generic fatal with no distinct exit status, and probing for
# the lock file cannot replace that text: the competing process normally releases the lock between
# the failure and the probe, so the probe reads false for exactly the case worth retrying. On Git for
# Windows this diagnostic is byte-stable English under any locale — no gettext catalogs ship and the
# suffix comes from the CRT errno table — so an ordinal match is the reliable signal, and a false
# negative only restores today's terminal result.
function Test-LandingIndexLockContention($Response) {
	$text = "$($Response.Stderr)$($Response.Stdout)"
	return $text.Contains('index.lock', [StringComparison]::Ordinal) -and $text.Contains('File exists', [StringComparison]::Ordinal)
}

# The primary checkout update is idempotent, so a foreign process briefly holding the primary
# index.lock is waited out rather than turned into a failed landing. Git's own response is returned
# unchanged once the budget is spent, so an exhausted wait fails exactly as it does today.
function Invoke-LandingPrimaryCheckout([string] $Commit) {
	$arguments = @('-C', $script:PrimaryIdentity.Worktree, 'reset', '--hard', $Commit)
	$response = Invoke-FinalizeNativeText 'git.exe' $arguments $script:PrimaryIdentity.Worktree
	if ($response.ExitCode -eq 0 -or -not (Test-LandingIndexLockContention $response)) { return $response }
	$budgetMilliseconds = $IndexLockWaitSeconds * 1000
	$script:IndexLockWait.Start()
	try {
		while ($script:IndexLockWait.Elapsed.TotalMilliseconds -lt $budgetMilliseconds) {
			# The last sleep is clamped to what the budget still allows, so the accumulated wait lands on
			# the bound instead of one poll interval past it, and the attempt it pays for still runs.
			$remaining = $budgetMilliseconds - $script:IndexLockWait.Elapsed.TotalMilliseconds
			Start-Sleep -Milliseconds ([int][Math]::Min(500, [Math]::Ceiling($remaining)))
			$response = Invoke-FinalizeNativeText 'git.exe' $arguments $script:PrimaryIdentity.Worktree
			if ($response.ExitCode -eq 0 -or -not (Test-LandingIndexLockContention $response)) { return $response }
		}
	}
	finally { $script:IndexLockWait.Stop() }
	return $response
}

# For an overlay-producing mode, the replacement commit is built only after the lease is live. Its
# metadata is copied before any rebase, while the temporary index contains the rebased source tree
# plus exactly the two generated history blobs. No branch ref moves while these objects are being constructed.
function Get-FrozenCommitMetadata([string] $Commit) {
	return [ordered]@{
		message = Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('show', '--no-show-signature', '-s', '--format=%B', $Commit)
		authorName = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('show', '--no-show-signature', '-s', '--format=%an', $Commit)).Trim()
		authorEmail = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('show', '--no-show-signature', '-s', '--format=%ae', $Commit)).Trim()
		authorDate = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('show', '--no-show-signature', '-s', '--format=%aI', $Commit)).Trim()
		committerName = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('show', '--no-show-signature', '-s', '--format=%cn', $Commit)).Trim()
		committerEmail = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('show', '--no-show-signature', '-s', '--format=%ce', $Commit)).Trim()
		committerDate = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('show', '--no-show-signature', '-s', '--format=%cI', $Commit)).Trim()
	}
}

function Get-TreeChangeGuard([string] $BaseTree, [string] $TipTree) {
	$fields = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('diff', '--raw', '--no-renames', '-z', $BaseTree, $TipTree)).Split([char]0, [StringSplitOptions]::RemoveEmptyEntries)
	if ($fields.Count % 2 -ne 0) { throw 'git tree diff produced an incomplete record.' }
	$records = [Collections.Generic.List[string]]::new()
	for ($index = 0; $index -lt $fields.Count; $index += 2) {
		$parts = $fields[$index].Split(' ')
		if ($parts.Count -ne 5) { throw "git tree diff produced an unrecognized record: '$($fields[$index])'." }
		$records.Add("$($parts[0]) $($parts[1]) $($parts[4])`t$($fields[$index + 1])")
	}
	return $records -join "`n"
}

function Assert-NoHistorySourceChanges([string] $Base, [string] $Tip) {
	$guard = Get-LandingChangeGuard $Base $Tip
	foreach ($line in @($guard -split "`n" | Where-Object { $_ })) {
		$path = $line.Substring($line.IndexOf("`t") + 1)
		if ($path -cin @('.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl', '.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.svg')) {
			Throw-Landing 2 'history.source-changed' 'The approved source patch changes a reserved history output path.'
		}
	}
	return $guard
}

function Assert-HistoryPathsClean([string] $Worktree) {
	$status = Invoke-FinalizeGit $Worktree @('status', '--porcelain=v1', '--untracked-files=all', '--', '.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl', '.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.svg')
	if ($status.Length -ne 0) { Throw-Landing 2 'history.source-dirty' 'Reserved history JSONL/SVG paths must be clean in the real index and worktree before verification or advance.' }
}

# Recovery uses blob identity to distinguish a source commit from a replacement commit. Missing or
# differing reserved paths are a normal non-match here: the overlay recovery search owns that shape.
function Test-HistoryPathsUnchanged([string] $BaseRevision, [string] $LandedRevision) {
	$paths = @('.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl', '.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.svg')
	try {
		foreach ($path in $paths) {
			$base = Invoke-FinalizeNativeText 'git.exe' @('-C', $script:CurrentIdentity.Worktree, 'rev-parse', "${BaseRevision}:$path") $script:CurrentIdentity.Worktree
			$landed = Invoke-FinalizeNativeText 'git.exe' @('-C', $script:CurrentIdentity.Worktree, 'rev-parse', "${LandedRevision}:$path") $script:CurrentIdentity.Worktree
			if ($base.ExitCode -ne 0 -or $landed.ExitCode -ne 0) { return $false }
			$baseBlob = $base.Stdout.Trim(); $landedBlob = $landed.Stdout.Trim()
			if ($baseBlob -notmatch '^[0-9a-f]{40}$' -or $landedBlob -notmatch '^[0-9a-f]{40}$' -or $baseBlob -cne $landedBlob) { return $false }
		}
		return $true
	}
	catch { return $false }
}

function New-HistoryOverlayTree($Artifacts, [string] $SourceCommit) {
	$indexPath = Join-Path $script:HistoryTempRoot ('overlay-' + [guid]::NewGuid().ToString('N') + '.index')
	$oldIndex = $env:GIT_INDEX_FILE
	try {
		$env:GIT_INDEX_FILE = $indexPath
		Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('read-tree', $SourceCommit) | Out-Null
		$jsonBlob = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('hash-object', '-w', '--path', '.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl', $Artifacts.JsonPath)).Trim()
		$svgBlob = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('hash-object', '-w', '--path', '.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.svg', $Artifacts.SvgPath)).Trim()
		Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('update-index', '--add', '--cacheinfo', "100644,$jsonBlob,.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl") | Out-Null
		Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('update-index', '--add', '--cacheinfo', "100644,$svgBlob,.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.svg") | Out-Null
		$tree = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('write-tree')).Trim()
		$changes = Get-TreeChangeGuard (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('rev-parse', "$SourceCommit^{tree}")).Trim() $tree
		$expected = @('.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl', '.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.svg')
		$actual = @($changes -split "`n" | Where-Object { $_ } | ForEach-Object { $_.Substring($_.IndexOf("`t") + 1) } | Sort-Object)
		if (($actual -join '|') -cne (($expected | Sort-Object) -join '|')) { Throw-Landing 2 'history.overlay-invalid' 'The temporary history overlay changed a path outside the exact reserved JSONL/SVG pair.' }
		return $tree
	}
	finally {
		if ($null -eq $oldIndex) { Remove-Item Env:GIT_INDEX_FILE -ErrorAction SilentlyContinue } else { $env:GIT_INDEX_FILE = $oldIndex }
		Remove-Item -LiteralPath $indexPath -Force -ErrorAction SilentlyContinue
	}
}

function New-FrozenReplacementCommit([string] $Tree, [string] $Parent) {
	$metadata = $script:FrozenCommitMetadata
	$messagePath = Join-Path $script:HistoryTempRoot ('message-' + [guid]::NewGuid().ToString('N') + '.txt')
	[IO.File]::WriteAllText($messagePath, ([string]$metadata.message).TrimEnd("`r", "`n") + "`n", [Text.UTF8Encoding]::new($false))
	$start = [Diagnostics.ProcessStartInfo]::new()
	$start.FileName = 'git.exe'; $start.WorkingDirectory = $script:CurrentIdentity.Worktree; $start.UseShellExecute = $false; $start.CreateNoWindow = $true
	$start.RedirectStandardOutput = $true; $start.RedirectStandardError = $true
	foreach ($argument in @('-C', $script:CurrentIdentity.Worktree, 'commit-tree', $Tree, '-p', $Parent, '-F', $messagePath)) { [void]$start.ArgumentList.Add($argument) }
	$start.Environment['GIT_AUTHOR_NAME'] = $metadata.authorName; $start.Environment['GIT_AUTHOR_EMAIL'] = $metadata.authorEmail; $start.Environment['GIT_AUTHOR_DATE'] = $metadata.authorDate
	$start.Environment['GIT_COMMITTER_NAME'] = $metadata.committerName; $start.Environment['GIT_COMMITTER_EMAIL'] = $metadata.committerEmail; $start.Environment['GIT_COMMITTER_DATE'] = $metadata.committerDate
	$process = [Diagnostics.Process]::new(); $process.StartInfo = $start
	try {
		if (-not $process.Start()) { Throw-Landing 1 'git.commit-tree-start-failed' 'Could not start git commit-tree for the history replacement.' }
		$stdoutTask = $process.StandardOutput.ReadToEndAsync(); $stderrTask = $process.StandardError.ReadToEndAsync(); $process.WaitForExit()
		$stdout = $stdoutTask.GetAwaiter().GetResult().Trim(); $stderr = $stderrTask.GetAwaiter().GetResult().Trim()
		if ($process.ExitCode -ne 0 -or $stdout -cnotmatch '^[0-9a-f]{40}$') { Throw-Landing 1 'git.commit-tree-failed' "History replacement commit failed: $stderr" }
		return $stdout
	}
	finally { $process.Dispose(); Remove-Item -LiteralPath $messagePath -Force -ErrorAction SilentlyContinue }
}

function Prepare-HistoryReplacementCommit {
	try {
		Assert-HistoryPathsClean $script:PrimaryIdentity.Worktree
		[void](Validate-HistoryContractForLanding $script:LandingPrimaryTip $script:LandingCommit)
		$currentMode = [string]$result.historyContract.current.mode
		if ($currentMode -ceq 'carry-forward') {
			$result.historyUpdate = [ordered]@{ status = 'skipped'; receipt = $null; rowDate = $null; jsonl = $null; svg = $null }
			$result.final = [ordered]@{ commit = $script:LandingCommit; tree = $script:LandingTree; parent = $script:LandingPrimaryTip; replacement = $false }
			return [pscustomobject]@{ Commit = $script:LandingCommit; Tree = $script:LandingTree; Parent = $script:LandingPrimaryTip }
		}
		$updateReceipt = Invoke-HistoryGenerate $script:LandingCommit $script:LandingPrimaryTip
		$artifacts = Validate-HistoryUpdate $updateReceipt $script:LandingCommit
		$finalTree = New-HistoryOverlayTree $artifacts $script:LandingCommit
		$finalCommit = New-FrozenReplacementCommit $finalTree $script:LandingPrimaryTip
		$finalParent = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('show', '-s', '--format=%P', $finalCommit)).Trim()
		$actualTree = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('rev-parse', "$finalCommit^{tree}")).Trim()
		if ($finalParent -cne $script:LandingPrimaryTip -or $actualTree -cne $finalTree) { Throw-Landing 1 'history.replacement-invalid' 'History replacement commit does not have the current primary sole parent and validated tree.' }
		$result.final.commit = $finalCommit; $result.final.tree = $finalTree; $result.final.parent = $finalParent; $result.final.replacement = $true
		return [pscustomobject]@{ Commit = $finalCommit; Tree = $finalTree; Parent = $finalParent }
	}
	catch {
		if ($script:LandingCommit -cne $ApprovedSessionCommit) {
			$restore = Invoke-FinalizeNativeText 'git.exe' @('-C', $script:CurrentIdentity.Worktree, 'reset', '--hard', $ApprovedSessionCommit) $script:CurrentIdentity.Worktree
			$restoredRef = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('rev-parse', "refs/heads/$CurrentBranch")).Trim()
			$restoredHead = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('rev-parse', 'HEAD')).Trim()
			if ($restore.ExitCode -ne 0 -or $restoredRef -cne $ApprovedSessionCommit -or $restoredHead -cne $ApprovedSessionCommit -or (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('status', '--porcelain=v1', '-z', '--untracked-files=all')).Length -ne 0) { Throw-LandingRestorationUnproven "History replacement failed and the existing rebase could not be restored: $($_.Exception.Message)" }
			$script:LandingCommit = $ApprovedSessionCommit; $script:LandingPrimaryTip = $script:ApprovedSourceParent; $script:LandingTree = $ApprovedCandidateTree
		}
		throw
	}
}

# Returns $false only for a lost compare-and-swap, which means primary moved under the held lease
# and the caller may rebase once; every other failure is terminal for this landing.
function Advance-PrimaryExactCandidate {
	$expectedCheckout = (Invoke-FinalizeGit $script:PrimaryIdentity.Worktree @('rev-parse', 'HEAD')).Trim()
	$expectedStatus = Invoke-FinalizeGit $script:PrimaryIdentity.Worktree @('status', '--porcelain=v1', '-z', '--untracked-files=all')
	$prepared = Prepare-HistoryReplacementCommit
	$finalCommit = $prepared.Commit; $finalTree = $prepared.Tree
	$expectedForCas = if ($FixtureFailure -ceq 'compare-and-swap') { '0000000000000000000000000000000000000000' } else { $script:LandingPrimaryTip }
	$advance = Invoke-FinalizeNativeText 'git.exe' @('-C', $script:PrimaryIdentity.Worktree, 'update-ref', "refs/heads/$PrimaryBranch", $finalCommit, $expectedForCas) $script:PrimaryIdentity.Worktree
	if ($advance.ExitCode -ne 0) { return $false }
	if ($FixtureFailure -ceq 'post-update-ref') { $result.primaryAdvanced = $true; Throw-Landing 1 'fixture.crash-after-update-ref' 'Fixture stopped after the primary ref update and before its checkout reset.' }
	$result.primaryAdvanced = $true
	$script:FinalCommit = $finalCommit; $script:FinalTree = $finalTree
	$result.tips.current = $finalCommit
	$result.tips.primary = $finalCommit
	try {
		$reset = Invoke-LandingPrimaryCheckout $finalCommit
		if ($reset.ExitCode -ne 0) { throw "Primary checkout did not update to the exact final commit: $($reset.Stderr.Trim())" }
		if ($FixtureFailure -ceq 'post-reset') { throw 'Fixture forced post-reset failure.' }
		$actual = (Invoke-FinalizeGit $script:PrimaryIdentity.Worktree @('rev-parse', "refs/heads/$PrimaryBranch")).Trim()
		$actualTree = (Invoke-FinalizeGit $script:PrimaryIdentity.Worktree @('rev-parse', "$actual^{tree}")).Trim()
		if ($actual -cne $finalCommit -or $actualTree -cne $finalTree) { throw 'Primary ref does not equal the exact verified final commit and tree.' }
		$sessionRef = "refs/heads/$CurrentBranch"
		$sessionCas = Invoke-FinalizeNativeText 'git.exe' @('-C', $script:CurrentIdentity.Worktree, 'update-ref', $sessionRef, $finalCommit, $script:LandingCommit) $script:CurrentIdentity.Worktree
		if ($sessionCas.ExitCode -ne 0) { throw "Session branch could not advance to the final commit: $($sessionCas.Stderr.Trim())" }
		$sessionReset = Invoke-FinalizeNativeText 'git.exe' @('-C', $script:CurrentIdentity.Worktree, 'reset', '--hard', $finalCommit) $script:CurrentIdentity.Worktree
		if ($sessionReset.ExitCode -ne 0 -or (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('rev-parse', 'HEAD')).Trim() -cne $finalCommit -or (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('status', '--porcelain=v1', '-z', '--untracked-files=all')).Length -ne 0) { throw 'Session checkout did not update to the exact final commit.' }
	}
	catch {
		$reason = $_.Exception.Message
		$sessionRef = "refs/heads/$CurrentBranch"
		$sessionHead = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('rev-parse', "refs/heads/$CurrentBranch")).Trim()
		if ($sessionHead -ceq $finalCommit) { [void](Invoke-FinalizeNativeText 'git.exe' @('-C', $script:CurrentIdentity.Worktree, 'update-ref', $sessionRef, $script:LandingCommit, $finalCommit) $script:CurrentIdentity.Worktree) }
		$rollback = Invoke-FinalizeNativeText 'git.exe' @('-C', $script:PrimaryIdentity.Worktree, 'update-ref', "refs/heads/$PrimaryBranch", $script:LandingPrimaryTip, $finalCommit) $script:PrimaryIdentity.Worktree
		if ($rollback.ExitCode -ne 0) { Throw-Landing 1 'git.rollback-failed' "Exact candidate advance postcondition failed and guarded rollback failed: $reason" }
		$restore = Invoke-LandingPrimaryCheckout $expectedCheckout
		# The session branch goes back to the commit the user confirmed, not to the internal rebase's own
		# replacement, so the documented re-invocation with the approved arguments still passes strict sanity.
		$sessionRestore = Invoke-FinalizeNativeText 'git.exe' @('-C', $script:CurrentIdentity.Worktree, 'reset', '--hard', $ApprovedSessionCommit) $script:CurrentIdentity.Worktree
		if ($restore.ExitCode -ne 0 -or $sessionRestore.ExitCode -ne 0 -or (Invoke-FinalizeGit $script:PrimaryIdentity.Worktree @('rev-parse', "refs/heads/$PrimaryBranch")).Trim() -cne $script:LandingPrimaryTip -or (Invoke-FinalizeGit $script:PrimaryIdentity.Worktree @('rev-parse', 'HEAD')).Trim() -cne $expectedCheckout -or (Invoke-FinalizeGit $script:PrimaryIdentity.Worktree @('status', '--porcelain=v1', '-z', '--untracked-files=all')) -cne $expectedStatus -or (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('rev-parse', 'HEAD')).Trim() -cne $ApprovedSessionCommit -or (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('rev-parse', $sessionRef)).Trim() -cne $ApprovedSessionCommit -or (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('status', '--porcelain=v1', '-z', '--untracked-files=all')).Length -ne 0) { Throw-Landing 1 'git.rollback-failed' "Exact candidate advance rollback did not restore the expected primary and session checkouts: $reason" }
		$result.primaryAdvanced = $false
		Throw-Landing 2 'candidate.postcondition-failed' $reason
	}
	$result.final.commit = $finalCommit; $result.final.tree = $finalTree; $result.final.parent = $script:LandingPrimaryTip
	$result.landed.commit = $finalCommit
	$result.landed.tree = $finalTree
	return $true
}

function Start-LandingGitProcess([string[]] $Arguments, [bool] $RedirectInput) {
	$start = [Diagnostics.ProcessStartInfo]::new()
	$start.FileName = 'git.exe'
	$start.WorkingDirectory = $script:CurrentIdentity.Worktree
	$start.UseShellExecute = $false
	$start.CreateNoWindow = $true
	$start.RedirectStandardOutput = $true
	$start.RedirectStandardError = $true
	$start.RedirectStandardInput = $RedirectInput
	foreach ($argument in $Arguments) { [void] $start.ArgumentList.Add($argument) }
	$process = [Diagnostics.Process]::new()
	$process.StartInfo = $start
	if (-not $process.Start()) { throw "Could not start 'git.exe'." }
	return $process
}

# Patch text reaches git patch-id as the exact bytes git produced: the identity is compared across
# the rebase to decide whether the landing bytes are still the confirmed ones, so no text decoding
# may sit between the two commands. --verbatim keeps whitespace significant, so a commit differing
# from the confirmed one only in whitespace cannot pass as byte-identical; it implies --stable, so
# only hunk line numbers stay ignored and a rebase over another hunk still compares equal.
function Get-LandingPatchId([string] $Base, [string] $Tip, [string[]] $Pathspec = @()) {
	$worktree = $script:CurrentIdentity.Worktree
	$patch = [IO.MemoryStream]::new()
	try {
		$diffArguments = @('-C', $worktree, 'diff-tree', '-p', '--no-color', '--no-ext-diff', '--full-index', '--binary', '--no-renames', '-r', $Base, $Tip)
		if (@($Pathspec).Count -gt 0) { $diffArguments += @('--'); $diffArguments += @($Pathspec) }
		$diff = Start-LandingGitProcess $diffArguments $false
		$diffErrorTask = $diff.StandardError.ReadToEndAsync()
		$diff.StandardOutput.BaseStream.CopyTo($patch)
		$diffError = $diffErrorTask.GetAwaiter().GetResult()
		$diff.WaitForExit()
		$diffExit = $diff.ExitCode
		$diff.Dispose()
		if ($diffExit -ne 0) { throw "git diff-tree failed for the landing patch: $($diffError.Trim())" }
		$identify = Start-LandingGitProcess @('patch-id', '--verbatim') $true
		$identifyOutTask = $identify.StandardOutput.ReadToEndAsync()
		$identifyErrorTask = $identify.StandardError.ReadToEndAsync()
		$patch.Position = 0
		$patch.CopyTo($identify.StandardInput.BaseStream)
		$identify.StandardInput.BaseStream.Flush()
		$identify.StandardInput.Close()
		$identifyOut = $identifyOutTask.GetAwaiter().GetResult()
		$identifyError = $identifyErrorTask.GetAwaiter().GetResult()
		$identify.WaitForExit()
		$identifyExit = $identify.ExitCode
		$identify.Dispose()
		if ($identifyExit -ne 0) { throw "git patch-id failed for the landing patch: $($identifyError.Trim())" }
		$identityValue = $identifyOut.Trim().Split(' ')[0]
		if ([string]::IsNullOrWhiteSpace($identityValue)) { return '(empty)' }
		return $identityValue
	}
	finally { $patch.Dispose() }
}

# Path, mode, and status of every changed file, with the blob object IDs deliberately dropped: an
# upstream commit touching another hunk of the same file changes the pre-image ID without changing
# what this patch does, and content — including binary blobs — is already covered by patch-id.
function Get-LandingChangeGuard([string] $Base, [string] $Tip) {
	$fields = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('diff', '--raw', '--no-renames', '-z', $Base, $Tip)).Split([char]0, [StringSplitOptions]::RemoveEmptyEntries)
	if ($fields.Count % 2 -ne 0) { throw 'git diff --raw produced an incomplete record for the landing patch.' }
	$records = [Collections.Generic.List[string]]::new()
	for ($index = 0; $index -lt $fields.Count; $index += 2) {
		$parts = $fields[$index].Split(' ')
		if ($parts.Count -ne 5) { throw "git diff --raw produced an unrecognized record: '$($fields[$index])'." }
		$records.Add("$($parts[0]) $($parts[1]) $($parts[4])`t$($fields[$index + 1])")
	}
	return $records -join "`n"
}

# Two independent signals: patch-id proves the applied content, and the change guard proves the
# patch still touches the same paths with the same modes and statuses.
function Get-LandingPatchIdentity([string] $Base, [string] $Tip) {
	return [pscustomobject]@{
		PatchId = Get-LandingPatchId $Base $Tip
		Changes = Get-LandingChangeGuard $Base $Tip
	}
}

function Get-LandingNonHistoryPatchIdentity([string] $Base, [string] $Tip) {
	$exclude = @('.', ':(exclude).agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl', ':(exclude).agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.svg')
	$guard = Get-LandingChangeGuard $Base $Tip
	$filtered = @($guard -split "`n" | Where-Object { $_ -and $_.Substring($_.IndexOf("`t") + 1) -cnotin @('.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl', '.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.svg') }) -join "`n"
	return [pscustomobject]@{ PatchId = Get-LandingPatchId $Base $Tip $exclude; Changes = $filtered }
}

# Restoration is proven, never assumed, so an unproven session worktree also keeps the lease: only
# this transaction knows the branch state it left behind.
function Throw-LandingRestorationUnproven([string] $Message) {
	$script:LandingRestorationUnproven = $true
	Throw-Landing 2 'rebase.abort-failed' $Message
}

# A blocked retry must leave the session branch exactly as the user confirmed it, which is the
# approved commit itself even after the rebase moved the branch to its replacement.
function Restore-LandingSessionBranch([string] $Code, [string] $Reason, [string] $Disposition = 'terminal', [int] $RetryAfterMilliseconds = 0) {
	$worktree = $script:CurrentIdentity.Worktree
	foreach ($marker in @('rebase-merge', 'rebase-apply')) {
		# `git rebase --abort` exits 128 when no rebase is in progress, so only a started one is aborted.
		if (Test-Path -LiteralPath (Invoke-FinalizeGit $worktree @('rev-parse', '--path-format=absolute', '--git-path', $marker)).Trim()) {
			$abort = Invoke-FinalizeNativeText 'git.exe' @('-C', $worktree, 'rebase', '--abort') $worktree
			if ($abort.ExitCode -ne 0) { Throw-LandingRestorationUnproven "$Reason Aborting the rebase failed: $($abort.Stderr.Trim())" }
			break
		}
	}
	if ((Invoke-FinalizeGit $worktree @('rev-parse', "refs/heads/$CurrentBranch")).Trim() -cne $ApprovedSessionCommit) {
		$restore = Invoke-FinalizeNativeText 'git.exe' @('-C', $worktree, 'reset', '--hard', $ApprovedSessionCommit) $worktree
		if ($restore.ExitCode -ne 0) { Throw-LandingRestorationUnproven "$Reason Restoring the confirmed session commit failed: $($restore.Stderr.Trim())" }
	}
	if ((Invoke-FinalizeGit $worktree @('rev-parse', 'HEAD')).Trim() -cne $ApprovedSessionCommit -or
		(Invoke-FinalizeGit $worktree @('rev-parse', "refs/heads/$CurrentBranch")).Trim() -cne $ApprovedSessionCommit -or
		(Invoke-FinalizeGit $worktree @('status', '--porcelain', '-z', '--untracked-files=all')).Length -ne 0) {
		Throw-LandingRestorationUnproven "$Reason The session worktree could not be proven restored to the confirmed commit."
	}
	Throw-Landing 2 $Code $Reason $Disposition $false $RetryAfterMilliseconds
}

# The single place the landing state moves off the caller's approved anchors: only a clean rebase
# whose patch is provably identical to the confirmed one becomes the new landing candidate, so a
# retry can never advance primary with bytes the user did not confirm.
function Invoke-LandingRebaseOntoPrimary {
	$worktree = $script:CurrentIdentity.Worktree
	$approvedIdentity = Get-LandingPatchIdentity $script:LandingPrimaryTip $script:LandingCommit
	$newPrimaryTip = (Invoke-FinalizeGit $script:PrimaryIdentity.Worktree @('rev-parse', "refs/heads/$PrimaryBranch")).Trim()
	$result.landed.rebaseAttempts = [int]$result.landed.rebaseAttempts + 1
	$rebase = Invoke-FinalizeNativeText 'git.exe' @('-C', $worktree, 'rebase', '--onto', $newPrimaryTip, $script:LandingPrimaryTip, $CurrentBranch) $worktree
	if ($rebase.ExitCode -ne 0) {
		Restore-LandingSessionBranch 'rebase.conflicted' "Rebasing the confirmed candidate onto the current primary tip did not apply cleanly: $($rebase.Stderr.Trim())"
	}
	$rebasedCommit = (Invoke-FinalizeGit $worktree @('rev-parse', "refs/heads/$CurrentBranch")).Trim()
	$rebasedIdentity = Get-LandingPatchIdentity $newPrimaryTip $rebasedCommit
	if ($FixtureFailure -ceq 'retry-patch-mismatch' -or $rebasedIdentity.PatchId -cne $approvedIdentity.PatchId -or $rebasedIdentity.Changes -cne $approvedIdentity.Changes) {
		Restore-LandingSessionBranch 'rebase.patch-not-identical' 'Rebasing onto the current primary tip did not reproduce the confirmed patch.'
	}
	$script:LandingPrimaryTip = $newPrimaryTip
	$script:LandingCommit = $rebasedCommit
	$script:LandingTree = (Invoke-FinalizeGit $worktree @('rev-parse', "$rebasedCommit^{tree}")).Trim()
	$result.rebasedSource.commit = $script:LandingCommit
	$result.rebasedSource.tree = $script:LandingTree
	$result.rebasedSource.parent = $script:LandingPrimaryTip
	$result.rebasedSource.patch = Assert-NoHistorySourceChanges $script:LandingPrimaryTip $script:LandingCommit
}

function Find-LandingSourceMatch([string] $PrimaryRefTip) {
	if ([string]::IsNullOrWhiteSpace($PrimaryRefTip) -or $PrimaryRefTip -ceq $ExpectedPrimaryTip) { return $null }
	if (-not (Test-FinalizeGitSuccess $script:PrimaryIdentity.Worktree @('merge-base', '--is-ancestor', $ExpectedPrimaryTip, $PrimaryRefTip))) { return $null }
	$parents = @((Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('show', '-s', '--format=%P', $PrimaryRefTip)).Trim() -split ' ' | Where-Object { $_ })
	if ($parents.Count -ne 1) { return $null }
	$parent = $parents[0]
	$approvedIdentity = if ($null -ne $script:ApprovedPatchIdentity) { $script:ApprovedPatchIdentity } else { Get-LandingPatchIdentity $script:ApprovedSourceParent $ApprovedSessionCommit }
	if ($PrimaryRefTip -cne $ApprovedSessionCommit) {
		$landedIdentity = Get-LandingPatchIdentity $parent $PrimaryRefTip
		if ($landedIdentity.PatchId -cne $approvedIdentity.PatchId -or $landedIdentity.Changes -cne $approvedIdentity.Changes) { return $null }
	}
	return [pscustomobject]@{ Commit = $PrimaryRefTip; Parent = $parent; Tree = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('rev-parse', "$PrimaryRefTip^{tree}")).Trim(); SourceOnly = Test-HistoryPathsUnchanged $parent $PrimaryRefTip }
}

function Assert-RecoveryCheckoutMatches([string] $Worktree, [string] $ExpectedRevision) {
	$index = Invoke-FinalizeNativeText 'git.exe' @('-C', $Worktree, 'diff', '--quiet', '--cached', $ExpectedRevision) $Worktree
	$working = Invoke-FinalizeNativeText 'git.exe' @('-C', $Worktree, 'diff', '--quiet', $ExpectedRevision) $Worktree
	$untracked = Invoke-FinalizeNativeText 'git.exe' @('-C', $Worktree, 'ls-files', '--others', '--exclude-standard', '-z') $Worktree
	if ($index.ExitCode -ne 0 -or $working.ExitCode -ne 0 -or $untracked.ExitCode -ne 0 -or $untracked.Stdout.Length -ne 0) {
		Throw-Landing 2 'history.recovery-checkout-changed' 'Recovery checkout does not match the expected post-update-ref state; no checkout or ref was modified.'
	}
}

# A two-tree read-tree merge checks the current index against its old tree under Git's index lock,
# updates only the index/worktree, and never moves a branch ref. Its refusal is atomic for the late
# staged-edit case; the same primitive can therefore reverse a completed transition without losing
# an edit that arrived after the checkout precheck.
function Invoke-RecoveredCheckoutTransition([string] $Worktree, [string] $OldRevision, [string] $NewRevision) {
	return Invoke-FinalizeNativeText 'git.exe' @('-C', $Worktree, 'read-tree', '-m', '-u', $OldRevision, $NewRevision) $Worktree
}

function Invoke-RecoveredSessionCheckoutTransition([string] $OldRevision, [string] $NewRevision) {
	return Invoke-RecoveredCheckoutTransition $script:CurrentIdentity.Worktree $OldRevision $NewRevision
}

function Test-RecoveredSessionHead([string] $SessionHead, $Match, [bool] $SourceOnly) {
	if ($SessionHead -ceq $ApprovedSessionCommit -or $SessionHead -ceq $Match.Commit) { return $true }
	$parents = @((Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('show', '-s', '--format=%P', $SessionHead)).Trim() -split ' ' | Where-Object { $_ })
	if ($parents.Count -ne 1 -or $parents[0] -cne $Match.Parent) { return $false }
	$approvedIdentity = if ($null -ne $script:ApprovedPatchIdentity) { $script:ApprovedPatchIdentity } else { Get-LandingNonHistoryPatchIdentity $script:ApprovedSourceParent $ApprovedSessionCommit }
	$sessionIdentity = Get-LandingNonHistoryPatchIdentity $parents[0] $SessionHead
	if ($sessionIdentity.PatchId -cne $approvedIdentity.PatchId -or $sessionIdentity.Changes -cne $approvedIdentity.Changes) { return $false }
	if (-not (Test-HistoryPathsUnchanged $parents[0] $SessionHead)) { return $false }
	return -not $SourceOnly -or $SessionHead -ceq $Match.Commit
}

function Restore-RecoveredSessionCheckout([string] $SessionRef, [string] $PriorHead, [string] $LandedHead, [string] $FailureReason, [bool] $TransitionCompleted) {
	$currentRef = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('rev-parse', $SessionRef)).Trim()
	if ($currentRef -ceq $LandedHead) {
		$rollback = Invoke-FinalizeNativeText 'git.exe' @('-C', $script:CurrentIdentity.Worktree, 'update-ref', $SessionRef, $PriorHead, $LandedHead) $script:CurrentIdentity.Worktree
		if ($rollback.ExitCode -ne 0) {
			$script:LandingRestorationUnproven = $true
			Throw-Landing 1 'history.recovery-session-rollback-failed' "$FailureReason The session ref rollback failed: $($rollback.Stderr.Trim())"
		}
		$currentRef = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('rev-parse', $SessionRef)).Trim()
	}
	if ($currentRef -cne $PriorHead) {
		$script:LandingRestorationUnproven = $true
		Throw-Landing 1 'history.recovery-session-rollback-failed' "$FailureReason The session ref moved away before its guarded rollback could complete."
	}
	# A refused read-tree leaves the old index/worktree in place. Only a completed forward transition
	# needs a reverse merge; reset --keep is unsafe here because it can turn a staged-only late edit
	# into an unstaged edit when the old and new revisions are equal.
	$restore = if ($TransitionCompleted) { Invoke-RecoveredSessionCheckoutTransition $LandedHead $PriorHead } else { [pscustomobject]@{ ExitCode = 0; Stdout = ''; Stderr = '' } }
	$restoredRef = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('rev-parse', $SessionRef)).Trim()
	$restoredHead = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('rev-parse', 'HEAD')).Trim()
	if ($restore.ExitCode -ne 0 -or $restoredRef -cne $PriorHead -or $restoredHead -cne $PriorHead) {
		$script:LandingRestorationUnproven = $true
		Throw-Landing 1 'history.recovery-session-rollback-failed' "$FailureReason The session checkout could not be safely restored to its prior head: $($restore.Stderr.Trim())"
	}
	Throw-Landing 2 'history.recovery-session-reset-failed' $FailureReason
}

# Reconciliation is shared by source-only and overlay recovery. The matcher and mode checks happen
# before the lock; after the claim this function re-reads the primary ref and runs the same checkout,
# session-branch, result, and postcondition path for either landed shape.
function Complete-RecoveredLanding($InitialMatch, [bool] $SourceOnly, [string] $InitialPrimaryRef) {
	Acquire-RecoveryLandingLock
	$claimedPrimaryRef = (Invoke-FinalizeGit $script:PrimaryIdentity.Worktree @('rev-parse', "refs/heads/$PrimaryBranch")).Trim()
	$claimedPrimaryIdentity = Get-FinalizeGitIdentity $PrimaryWorktree 'Primary worktree'
	$match = $InitialMatch
	if ($SourceOnly) {
		$recheckedMatch = Find-LandingSourceMatch $claimedPrimaryRef
		if ($null -eq $recheckedMatch -or -not $recheckedMatch.SourceOnly) { Throw-Landing 2 'history.recovery-not-found' 'Primary changed after recovery claim and no source-only landing matching the approved patch was found.' }
		$match = $recheckedMatch
		[void](Validate-HistoryContractForLanding $match.Parent $match.Commit)
		if ([string]$result.historyContract.current.mode -cne 'carry-forward') { Throw-Landing 2 'history.contract-changed' 'Source-only recovery requires a carry-forward History Contract.' }
	}
	elseif ($claimedPrimaryRef -cne $InitialPrimaryRef) {
		$recheckedMatch = Find-HistoryReplacementMatch $claimedPrimaryRef
		if ($null -eq $recheckedMatch) { Throw-Landing 2 'history.recovery-not-found' 'Primary advanced after recovery claim and no replacement matching the frozen landing transaction was found.' }
		$match = $recheckedMatch
	}
	$script:PrimaryIdentity = $claimedPrimaryIdentity
	$primaryStatus = Invoke-FinalizeGit $script:PrimaryIdentity.Worktree @('status', '--porcelain=v1', '-z', '--untracked-files=all')
	$primaryNeedsReset = $script:PrimaryIdentity.Head -cne $claimedPrimaryRef -or $primaryStatus.Length -ne 0
	if ($primaryNeedsReset) { Assert-RecoveryCheckoutMatches $script:PrimaryIdentity.Worktree $match.Parent }
	$sessionHead = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('rev-parse', "refs/heads/$CurrentBranch")).Trim()
	if (-not (Test-RecoveredSessionHead $sessionHead $match $SourceOnly)) { Throw-Landing 2 'history.recovery-session-changed' 'The original session branch no longer points at the confirmed source or a proven equivalent rebase.' }
	Assert-RecoveryCheckoutMatches $script:CurrentIdentity.Worktree $sessionHead
	if ($FixtureFailure -ceq 'history-recovery-session-race') {
		[IO.File]::WriteAllText((Join-Path $script:CurrentIdentity.Worktree 'base.txt'), 'fixture recovery session race', [Text.UTF8Encoding]::new($false))
	}
	elseif ($FixtureFailure -ceq 'history-recovery-session-staged-race') {
		[IO.File]::WriteAllText((Join-Path $script:CurrentIdentity.Worktree 'base.txt'), 'fixture recovery session staged race', [Text.UTF8Encoding]::new($false))
		$stage = Invoke-FinalizeNativeText 'git.exe' @('-C', $script:CurrentIdentity.Worktree, 'add', '--', 'base.txt') $script:CurrentIdentity.Worktree
		if ($stage.ExitCode -ne 0) { Throw-Landing 1 'fixture.recovery-session-race-failed' "Fixture could not stage its deterministic late edit: $($stage.Stderr.Trim())" }
	}
	elseif ($FixtureFailure -ceq 'history-recovery-active-primary-edit') {
		[IO.File]::WriteAllText((Join-Path $script:PrimaryIdentity.Worktree 'base.txt'), 'fixture recovery active primary edit', [Text.UTF8Encoding]::new($false))
	}
	if (-not $SourceOnly -and $primaryNeedsReset -and $FixtureFailure -ceq 'history-recovery-active-primary-race') {
		$race = Invoke-FinalizeNativeText 'git.exe' @('-C', $script:PrimaryIdentity.Worktree, '-c', 'user.name=fixture recovery', '-c', 'user.email=fixture-recovery@example.invalid', 'commit-tree', "$claimedPrimaryRef^{tree}", '-p', $claimedPrimaryRef, '-m', 'fixture recovery active primary descendant') $script:PrimaryIdentity.Worktree
		$raceCommit = $race.Stdout.Trim()
		if ($race.ExitCode -ne 0 -or $raceCommit -cnotmatch '^[0-9a-f]{40}$') { Throw-Landing 1 'fixture.recovery-active-primary-race-failed' 'Fixture could not create its deterministic active recovery primary descendant.' }
		$raceUpdate = Invoke-FinalizeNativeText 'git.exe' @('-C', $script:PrimaryIdentity.Worktree, 'update-ref', "refs/heads/$PrimaryBranch", $raceCommit, $claimedPrimaryRef) $script:PrimaryIdentity.Worktree
		if ($raceUpdate.ExitCode -ne 0) { Throw-Landing 1 'fixture.recovery-active-primary-race-failed' 'Fixture could not advance the primary ref for the active recovery race.' }
	}
	if ($script:PrimaryIdentity.Head -cne $claimedPrimaryRef -or $primaryStatus.Length -ne 0) {
		if ($SourceOnly) {
			# A checked-out branch reset also writes its branch ref. Guard the ref first, then update only
			# the index/worktree so a foreign ref movement cannot be overwritten by recovery.
			$primaryGuard = Invoke-FinalizeNativeText 'git.exe' @('-C', $script:PrimaryIdentity.Worktree, 'update-ref', "refs/heads/$PrimaryBranch", $claimedPrimaryRef, $claimedPrimaryRef) $script:PrimaryIdentity.Worktree
			if ($primaryGuard.ExitCode -ne 0) { Throw-Landing 2 'history.recovery-primary-race' 'Primary changed while recovery was preparing its source-only checkout.' }
			$primaryReset = Invoke-FinalizeNativeText 'git.exe' @('-C', $script:PrimaryIdentity.Worktree, 'read-tree', '--reset', '-u', $claimedPrimaryRef) $script:PrimaryIdentity.Worktree
		}
		else {
			# The replacement commit's parent is the exact stale checkout proven above. Guard the symbolic
			# branch ref, then update only its index/worktree so a foreign ref movement cannot be rewritten
			# by recovery; the postcondition below catches movement after this guard.
			$primaryGuard = Invoke-FinalizeNativeText 'git.exe' @('-C', $script:PrimaryIdentity.Worktree, 'update-ref', "refs/heads/$PrimaryBranch", $claimedPrimaryRef, $claimedPrimaryRef) $script:PrimaryIdentity.Worktree
			if ($primaryGuard.ExitCode -ne 0) { Throw-Landing 2 'history.recovery-primary-race' 'Primary changed while recovery was preparing its overlay checkout.' }
			$primaryReset = Invoke-RecoveredCheckoutTransition $script:PrimaryIdentity.Worktree $match.Parent $claimedPrimaryRef
		}
		$primaryRefAfterReset = (Invoke-FinalizeGit $script:PrimaryIdentity.Worktree @('rev-parse', "refs/heads/$PrimaryBranch")).Trim()
		if ($primaryReset.ExitCode -ne 0 -or $primaryRefAfterReset -cne $claimedPrimaryRef -or (Invoke-FinalizeGit $script:PrimaryIdentity.Worktree @('rev-parse', 'HEAD')).Trim() -ne $claimedPrimaryRef -or (Invoke-FinalizeGit $script:PrimaryIdentity.Worktree @('status', '--porcelain=v1', '-z', '--untracked-files=all')).Length -ne 0) { Throw-Landing 2 'history.recovery-primary-reset-failed' 'Recovered primary ref could not be reset and verified before session recovery.' }
		$script:PrimaryIdentity = Get-FinalizeGitIdentity $PrimaryWorktree 'Primary worktree'
	}
	$sessionRef = "refs/heads/$CurrentBranch"
	$update = Invoke-FinalizeNativeText 'git.exe' @('-C', $script:CurrentIdentity.Worktree, 'update-ref', $sessionRef, $match.Commit, $sessionHead) $script:CurrentIdentity.Worktree
	if ($update.ExitCode -ne 0) { Throw-Landing 2 'history.recovery-session-ref-failed' "Could not reset the original session branch to the recovered $(if ($SourceOnly) { 'source' } else { 'replacement' }) commit." }
	$reset = Invoke-RecoveredSessionCheckoutTransition $sessionHead $match.Commit
	$resetHead = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('rev-parse', 'HEAD')).Trim()
	$resetStatus = Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('status', '--porcelain=v1', '-z', '--untracked-files=all')
	if ($reset.ExitCode -ne 0 -or $resetHead -cne $match.Commit -or $resetStatus.Length -ne 0) {
		$failureReason = "Recovered session worktree could not be safely reset to the $(if ($SourceOnly) { 'source' } else { 'replacement' }) commit. $($reset.Stderr.Trim())".Trim()
		$transitionCompleted = $reset.ExitCode -eq 0 -and $resetHead -ceq $match.Commit
		Restore-RecoveredSessionCheckout $sessionRef $sessionHead $match.Commit $failureReason $transitionCompleted
	}
	Assert-ApprovedCandidateTree
	$script:FinalCommit = $match.Commit; $script:FinalTree = $match.Tree; $script:LandingPrimaryTip = $match.Parent; $script:LandingCommit = $match.Commit; $script:LandingTree = $match.Tree
	$result.candidate.treeVerified = $true
	$result.tips.current = $match.Commit; $result.tips.primary = $claimedPrimaryRef
	$result.final = [ordered]@{ commit = $match.Commit; tree = $match.Tree; parent = $match.Parent; replacement = -not $SourceOnly }
	if ($SourceOnly) {
		$result.historyUpdate = [ordered]@{ status = 'skipped'; receipt = $null; rowDate = $null; jsonl = $null; svg = $null }
	}
	else {
		$jsonPath = '.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl'; $svgPath = '.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.svg'
		$result.historyUpdate = [ordered]@{ status = 'recovered'; rowDate = $match.RowDate; jsonl = [ordered]@{ path = $jsonPath; bytes = $match.JsonBytes; sha256 = $match.JsonSha256 }; svg = [ordered]@{ path = $svgPath; bytes = $match.SvgBytes; sha256 = $match.SvgSha256; embeddedSha256 = $match.EmbeddedDigest } }
	}
	$result.landed.commit = $match.Commit; $result.landed.tree = $match.Tree; $result.landed.rebaseAttempts = if ($SourceOnly -and $match.Commit -cne $ApprovedSessionCommit) { 1 } else { $result.landed.rebaseAttempts }; $result.primaryAdvanced = $true
	return $true
}

function Test-LandingNoOverlayRecovery([string] $PrimaryRefTip) {
	if ($HistoryContractMode -notin @('carry-forward', 'catch-up')) { return $false }
	$match = Find-LandingSourceMatch $PrimaryRefTip
	if ($null -eq $match -or -not $match.SourceOnly) { return $false }
	[void](Validate-HistoryContractForLanding $match.Parent $match.Commit)
	if ([string]$result.historyContract.current.mode -cne 'carry-forward') { return $false }
	Assert-ApprovedCandidateTree
	return (Complete-RecoveredLanding $match $true $PrimaryRefTip)
}

function Get-LandingCommitMetadata([string] $Commit) {
	return [ordered]@{
		message = Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('show', '--no-show-signature', '-s', '--format=%B', $Commit)
		authorName = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('show', '--no-show-signature', '-s', '--format=%an', $Commit)).Trim()
		authorEmail = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('show', '--no-show-signature', '-s', '--format=%ae', $Commit)).Trim()
		authorDate = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('show', '--no-show-signature', '-s', '--format=%aI', $Commit)).Trim()
		committerName = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('show', '--no-show-signature', '-s', '--format=%cn', $Commit)).Trim()
		committerEmail = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('show', '--no-show-signature', '-s', '--format=%ce', $Commit)).Trim()
		committerDate = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('show', '--no-show-signature', '-s', '--format=%cI', $Commit)).Trim()
	}
}

function Test-RecoveryHistoryFields($Row, [byte[]] $JsonBytes, [byte[]] $SvgBytes, [string] $EmbeddedDigest) {
	$jsonSha = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($JsonBytes)).ToLowerInvariant()
	$svgSha = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($SvgBytes)).ToLowerInvariant()
	try { Assert-FinalizeHistoryRow $Row 'Recovery history row' } catch { return $false }
	if ($EmbeddedDigest -cne $jsonSha) { return $false }
	if (-not [string]::IsNullOrWhiteSpace($HistoryContractRowDate) -and [string]$Row.date -cne $HistoryContractRowDate) { return $false }
	if (-not [string]::IsNullOrWhiteSpace($HistoryJsonSha256) -and $jsonSha -cne $HistoryJsonSha256) { return $false }
	if (-not [string]::IsNullOrWhiteSpace($HistorySvgSha256) -and $svgSha -cne $HistorySvgSha256) { return $false }
	if ($HistoryJsonBytes -gt 0 -and [int64]$JsonBytes.Length -ne [int64]$HistoryJsonBytes) { return $false }
	if ($HistorySvgBytes -gt 0 -and [int64]$SvgBytes.Length -ne [int64]$HistorySvgBytes) { return $false }
	if (-not [string]::IsNullOrWhiteSpace($HistorySvgEmbeddedSha256) -and $EmbeddedDigest -cne $HistorySvgEmbeddedSha256) { return $false }
	return $true
}

function Find-HistoryReplacementMatch([string] $PrimaryHead) {
	if ($PrimaryHead -ceq $ApprovedSessionCommit -or $PrimaryHead -ceq $ExpectedPrimaryTip) { return $null }
	if (-not (Test-FinalizeGitSuccess $script:PrimaryIdentity.Worktree @('merge-base', '--is-ancestor', $ExpectedPrimaryTip, $PrimaryHead))) { Throw-Landing 2 'history.recovery-nonancestor' 'Current primary does not descend from the approved primary ancestor.' }
	$expectedMetadata = if ($null -ne $script:FrozenCommitMetadata) { $script:FrozenCommitMetadata } else { Get-FrozenCommitMetadata $ApprovedSessionCommit }
	$headMetadata = Get-LandingCommitMetadata $PrimaryHead
	$headMatchesFrozen = $headMetadata.message -ceq $expectedMetadata.message -and $headMetadata.authorName -ceq $expectedMetadata.authorName -and $headMetadata.authorEmail -ceq $expectedMetadata.authorEmail -and $headMetadata.authorDate -ceq $expectedMetadata.authorDate -and $headMetadata.committerName -ceq $expectedMetadata.committerName -and $headMetadata.committerEmail -ceq $expectedMetadata.committerEmail -and $headMetadata.committerDate -ceq $expectedMetadata.committerDate
	$commits = @((Invoke-FinalizeGit $script:PrimaryIdentity.Worktree @('rev-list', '--first-parent', $PrimaryHead)).Split("`n") | Where-Object { $_ })
	$recoveryMatches = [Collections.Generic.List[object]]::new()
	foreach ($commit in $commits) {
		if ($commit -ceq $ExpectedPrimaryTip) { break }
		$parents = @((Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('show', '-s', '--format=%P', $commit)).Trim() -split ' ' | Where-Object { $_ })
		if ($parents.Count -ne 1) { continue }
		$metadata = Get-LandingCommitMetadata $commit
		$expected = if ($null -ne $script:FrozenCommitMetadata) { $script:FrozenCommitMetadata } else { Get-FrozenCommitMetadata $ApprovedSessionCommit }
		if ($metadata.message -cne $expected.message -or $metadata.authorName -cne $expected.authorName -or $metadata.authorEmail -cne $expected.authorEmail -or $metadata.authorDate -cne $expected.authorDate -or $metadata.committerName -cne $expected.committerName -or $metadata.committerEmail -cne $expected.committerEmail -or $metadata.committerDate -cne $expected.committerDate) { continue }
		$patch = Get-LandingNonHistoryPatchIdentity $parents[0] $commit
		$approved = if ($null -ne $script:ApprovedPatchIdentity) { $script:ApprovedPatchIdentity } else { [pscustomobject]@{ PatchId = Get-LandingPatchId $script:ApprovedSourceParent $ApprovedSessionCommit; Changes = Assert-NoHistorySourceChanges $script:ApprovedSourceParent $ApprovedSessionCommit } }
		if ($patch.PatchId -cne $approved.PatchId -or $patch.Changes -cne $approved.Changes) { continue }
		$jsonPath = '.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl'; $svgPath = '.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.svg'
		if (-not (Test-FinalizeGitSuccess $script:CurrentIdentity.Worktree @('cat-file', '-e', "${commit}:$jsonPath")) -or -not (Test-FinalizeGitSuccess $script:CurrentIdentity.Worktree @('cat-file', '-e', "${commit}:$svgPath"))) { continue }
		$jsonBlob = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('rev-parse', "${commit}:$jsonPath")).Trim(); $svgBlob = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('rev-parse', "${commit}:$svgPath")).Trim()
		if ($jsonBlob -notmatch '^[0-9a-f]{40}$' -or $svgBlob -notmatch '^[0-9a-f]{40}$') { continue }
		$jsonText = Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('show', "${commit}:$jsonPath")
		$svgText = Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('show', "${commit}:$svgPath")
		$jsonBytes = [Text.UTF8Encoding]::new($false).GetBytes($jsonText)
		$svgBytes = [Text.UTF8Encoding]::new($false).GetBytes($svgText)
		$seriesMatch = [regex]::Match($svgText, 'seriesDigest=([0-9a-f]{64})')
		if (-not $seriesMatch.Success -or $seriesMatch.Groups[1].Value -cne ([Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($jsonBytes)).ToLowerInvariant())) { continue }
		if ($HistoryContractGeneratorDigest) {
			$generatorMatch = [regex]::Match($svgText, 'generatorDigest=([0-9a-f]{64})')
			if (-not $generatorMatch.Success -or $generatorMatch.Groups[1].Value -cne $HistoryContractGeneratorDigest) { continue }
		}
		$rows = @($jsonText -split "`n" | Where-Object { $_ })
		if ($rows.Count -lt 2) { continue }
		try { $last = $rows[-1] | ConvertFrom-Json -Depth 32 -ErrorAction Stop } catch { continue }
		$parentCommit = $parents[0]
		$parentJsonText = Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('show', "${parentCommit}:$jsonPath")
		$parentRows = @($parentJsonText -split "`n" | Where-Object { $_ })
		if ($last.captureMode -notin @('catch-up', 'cpp-change', 'carry-forward') -or [string]$last.date -notmatch '^\d{4}-\d{2}-\d{2}$' -or [int]$last.index -ne ($parentRows.Count - 1)) { continue }
		if (-not (Test-RecoveryHistoryFields $last $jsonBytes $svgBytes $seriesMatch.Groups[1].Value)) { continue }
		$recoveryMatches.Add([pscustomobject]@{ Commit = $commit; Parent = $parents[0]; Tree = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('rev-parse', "$commit^{tree}")).Trim(); RowDate = [string]$last.date; JsonBlob = $jsonBlob; SvgBlob = $svgBlob; JsonBytes = [int64]$jsonBytes.Length; SvgBytes = [int64]$svgBytes.Length; EmbeddedDigest = $seriesMatch.Groups[1].Value; JsonSha256 = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($jsonBytes)).ToLowerInvariant(); SvgSha256 = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($svgBytes)).ToLowerInvariant() })
	}
	if ($recoveryMatches.Count -eq 0) { if (-not $headMatchesFrozen) { return $null }; Throw-Landing 2 'history.recovery-not-found' 'No first-parent replacement commit matches the frozen metadata, source patch, and exact history artifacts.' }
	if ($recoveryMatches.Count -ne 1) { Throw-Landing 2 'history.recovery-ambiguous' "Found $($recoveryMatches.Count) first-parent replacement commits matching the frozen landing transaction." }
	return $recoveryMatches[0]
}

function Test-HistoryReplacementRecovery {
	$primaryHead = (Invoke-FinalizeGit $script:PrimaryIdentity.Worktree @('rev-parse', "refs/heads/$PrimaryBranch")).Trim()
	if ($primaryHead -ceq $ApprovedSessionCommit -or $primaryHead -ceq $ExpectedPrimaryTip) { return $false }
	if (-not (Test-FinalizeGitSuccess $script:PrimaryIdentity.Worktree @('merge-base', '--is-ancestor', $ExpectedPrimaryTip, $primaryHead))) { Throw-Landing 2 'history.recovery-nonancestor' 'Current primary does not descend from the approved primary ancestor.' }
	$expectedMetadata = if ($null -ne $script:FrozenCommitMetadata) { $script:FrozenCommitMetadata } else { Get-FrozenCommitMetadata $ApprovedSessionCommit }
	$headMetadata = Get-LandingCommitMetadata $primaryHead
	$headMatchesFrozen = $headMetadata.message -ceq $expectedMetadata.message -and $headMetadata.authorName -ceq $expectedMetadata.authorName -and $headMetadata.authorEmail -ceq $expectedMetadata.authorEmail -and $headMetadata.authorDate -ceq $expectedMetadata.authorDate -and $headMetadata.committerName -ceq $expectedMetadata.committerName -and $headMetadata.committerEmail -ceq $expectedMetadata.committerEmail -and $headMetadata.committerDate -ceq $expectedMetadata.committerDate
	$commits = @((Invoke-FinalizeGit $script:PrimaryIdentity.Worktree @('rev-list', '--first-parent', $primaryHead)).Split("`n") | Where-Object { $_ })
	$recoveryMatches = [Collections.Generic.List[object]]::new()
	foreach ($commit in $commits) {
		if ($commit -ceq $ExpectedPrimaryTip) { break }
		$parents = @((Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('show', '-s', '--format=%P', $commit)).Trim() -split ' ' | Where-Object { $_ })
		if ($parents.Count -ne 1) { continue }
		$metadata = Get-LandingCommitMetadata $commit
		$expected = if ($null -ne $script:FrozenCommitMetadata) { $script:FrozenCommitMetadata } else { Get-FrozenCommitMetadata $ApprovedSessionCommit }
		if ($metadata.message -cne $expected.message -or $metadata.authorName -cne $expected.authorName -or $metadata.authorEmail -cne $expected.authorEmail -or $metadata.authorDate -cne $expected.authorDate -or $metadata.committerName -cne $expected.committerName -or $metadata.committerEmail -cne $expected.committerEmail -or $metadata.committerDate -cne $expected.committerDate) { continue }
		$patch = Get-LandingNonHistoryPatchIdentity $parents[0] $commit
		$approved = if ($null -ne $script:ApprovedPatchIdentity) { $script:ApprovedPatchIdentity } else { [pscustomobject]@{ PatchId = Get-LandingPatchId $script:ApprovedSourceParent $ApprovedSessionCommit; Changes = Assert-NoHistorySourceChanges $script:ApprovedSourceParent $ApprovedSessionCommit } }
		if ($patch.PatchId -cne $approved.PatchId -or $patch.Changes -cne $approved.Changes) { continue }
		$jsonPath = '.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl'; $svgPath = '.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.svg'
		if (-not (Test-FinalizeGitSuccess $script:CurrentIdentity.Worktree @('cat-file', '-e', "${commit}:$jsonPath")) -or -not (Test-FinalizeGitSuccess $script:CurrentIdentity.Worktree @('cat-file', '-e', "${commit}:$svgPath"))) { continue }
		$jsonBlob = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('rev-parse', "${commit}:$jsonPath")).Trim(); $svgBlob = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('rev-parse', "${commit}:$svgPath")).Trim()
		if ($jsonBlob -notmatch '^[0-9a-f]{40}$' -or $svgBlob -notmatch '^[0-9a-f]{40}$') { continue }
		$jsonText = Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('show', "${commit}:$jsonPath")
		$svgText = Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('show', "${commit}:$svgPath")
		$jsonBytes = [Text.UTF8Encoding]::new($false).GetBytes($jsonText)
		$svgBytes = [Text.UTF8Encoding]::new($false).GetBytes($svgText)
		$seriesMatch = [regex]::Match($svgText, 'seriesDigest=([0-9a-f]{64})')
		if (-not $seriesMatch.Success -or $seriesMatch.Groups[1].Value -cne ([Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($jsonBytes)).ToLowerInvariant())) { continue }
		if ($HistoryContractGeneratorDigest) {
			$generatorMatch = [regex]::Match($svgText, 'generatorDigest=([0-9a-f]{64})')
			if (-not $generatorMatch.Success -or $generatorMatch.Groups[1].Value -cne $HistoryContractGeneratorDigest) { continue }
		}
		$rows = @($jsonText -split "`n" | Where-Object { $_ })
		if ($rows.Count -lt 2) { continue }
		try { $last = $rows[-1] | ConvertFrom-Json -Depth 32 -ErrorAction Stop } catch { continue }
		$parentCommit = $parents[0]
		$parentJsonText = Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('show', "${parentCommit}:$jsonPath")
		$parentRows = @($parentJsonText -split "`n" | Where-Object { $_ })
		if ($last.captureMode -notin @('catch-up', 'cpp-change', 'carry-forward') -or [string]$last.date -notmatch '^\d{4}-\d{2}-\d{2}$' -or [int]$last.index -ne ($parentRows.Count - 1)) { continue }
		if (-not (Test-RecoveryHistoryFields $last $jsonBytes $svgBytes $seriesMatch.Groups[1].Value)) { continue }
		$recoveryMatches.Add([pscustomobject]@{ Commit = $commit; Parent = $parents[0]; Tree = (Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('rev-parse', "$commit^{tree}")).Trim(); RowDate = [string]$last.date; JsonBlob = $jsonBlob; SvgBlob = $svgBlob; JsonBytes = [int64]$jsonBytes.Length; SvgBytes = [int64]$svgBytes.Length; EmbeddedDigest = $seriesMatch.Groups[1].Value; JsonSha256 = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($jsonBytes)).ToLowerInvariant(); SvgSha256 = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($svgBytes)).ToLowerInvariant() })
	}
	if ($recoveryMatches.Count -eq 0) { if (-not $headMatchesFrozen) { return $false }; Throw-Landing 2 'history.recovery-not-found' 'No first-parent replacement commit matches the frozen metadata, source patch, and exact history artifacts.' }
	if ($recoveryMatches.Count -ne 1) { Throw-Landing 2 'history.recovery-ambiguous' "Found $($recoveryMatches.Count) first-parent replacement commits matching the frozen landing transaction." }
	$match = $recoveryMatches[0]
	if ($FixtureFailure -ceq 'history-recovery-primary-race') {
		$race = Invoke-FinalizeNativeText 'git.exe' @('-C', $script:PrimaryIdentity.Worktree, '-c', 'user.name=fixture recovery', '-c', 'user.email=fixture-recovery@example.invalid', 'commit-tree', "$primaryHead^{tree}", '-p', $primaryHead, '-m', 'fixture recovery primary descendant') $script:PrimaryIdentity.Worktree
		$raceCommit = $race.Stdout.Trim()
		if ($race.ExitCode -ne 0 -or $raceCommit -cnotmatch '^[0-9a-f]{40}$') { Throw-Landing 1 'fixture.recovery-primary-race-failed' 'Fixture could not create its deterministic primary descendant.' }
		$raceUpdate = Invoke-FinalizeNativeText 'git.exe' @('-C', $script:PrimaryIdentity.Worktree, 'update-ref', "refs/heads/$PrimaryBranch", $raceCommit, $primaryHead) $script:PrimaryIdentity.Worktree
		if ($raceUpdate.ExitCode -ne 0) { Throw-Landing 1 'fixture.recovery-primary-race-failed' 'Fixture could not advance the primary ref for the recovery race.' }
		[IO.File]::WriteAllText((Join-Path $script:PrimaryIdentity.Worktree 'recovery-primary-race.txt'), 'fixture recovery race', [Text.UTF8Encoding]::new($false))
	}
	return (Complete-RecoveredLanding $match $false $primaryHead)
}

# Returns $false when primary moved under the held lease, either before the attempt or between the
# checks and the compare-and-swap.
function Invoke-LandingAdvanceAttempt {
	if ((Invoke-FinalizeGit $script:PrimaryIdentity.Worktree @('rev-parse', "refs/heads/$PrimaryBranch")).Trim() -cne $script:LandingPrimaryTip) { return $false }
	Assert-PrimaryAdvanceState
	Refresh-LandingOwner
	return (Advance-PrimaryExactCandidate)
}

# The claim is machine-local bookkeeping, not landed state: a failed delete leaves a stale
# lease that expires on its own, so it is reported as a residual and never blocks a landing.
function Complete-LandedState {
	if ($ReleasePlanClaim) {
		try {
			$sessionModule = Join-Path $script:CurrentIdentity.Worktree '.agents\scripts\AgentWorktreeSession.psm1'
			if (-not (Test-Path -LiteralPath $sessionModule -PathType Leaf)) { $sessionModule = Join-Path $PSScriptRoot '..\..\..\scripts\AgentWorktreeSession.psm1' }
			Import-Module $sessionModule -Force -DisableNameChecking
			$context = Get-AgentWorktreeSessionContext -Worktree $script:CurrentIdentity.Worktree
			if ([string]::IsNullOrWhiteSpace($context.SessionId)) { throw "Branch '$($context.Branch)' carries no session identity to release a claim for." }
			$unclaim = Invoke-WorktreeCli @('plan', 'unclaim', '--repo', $result.identities.gitCommonDirectory, '--worktree', $script:CurrentIdentity.Worktree, '--owner', $context.SessionId, '--session', $context.SessionId)
			$unclaimJson = $unclaim.Stdout.Trim() | ConvertFrom-Json -Depth 32 -ErrorAction Stop
			if ($unclaim.ExitCode -ne 0 -or [string]$unclaimJson.code -cnotin @('released', 'already-absent', 'none')) {
				throw "WorktreeCli reported '$($unclaimJson.code)': $($unclaimJson.message)"
			}
			$result.planClaim.released = $true
		}
		catch {
			$result.residuals.Add("Plan claim delete failed after landing; the machine-local claim expires on its own: $($_.Exception.Message)")
		}
	}
	$registration = Test-FinalizeWorktreeRegistration $script:PrimaryIdentity.Worktree $script:CurrentIdentity.Worktree $CurrentBranch $(if ($null -ne $script:FinalCommit) { $script:FinalCommit } else { $script:LandingCommit })
	if (-not $registration.Registered) { Throw-Landing 2 'session.registration-invalid' $registration.Message }
	if ((Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('status', '--porcelain', '-z', '--untracked-files=all')).Length -ne 0) { Throw-Landing 2 'session.dirty' 'Session worktree is dirty after landing.' }
	$result.status = 'landed'; $result.code = 'ok'; $result.message = 'Primary advanced and post-landing finalization completed.'
}

try {
	if ($FixtureFailure -cne 'none' -and $env:BROKEN_ENGINE_FINALIZE_WORKFLOW_FIXTURE -cne '1') { Throw-Landing 1 'input.fixture-forbidden' 'Fixture-only inputs require the finalization workflow fixture environment.' }
	if ($FixtureFailure -ceq 'bounded-diagnostic') { Throw-Landing 1 (('c' * 140)) (('m' * 600)) }
	# \z rather than $: .NET's $ also matches before a trailing newline, so a hex value carrying one would pass.
	if ($ApprovedSessionCommit -cnotmatch '^[0-9a-f]{8,40}\z' -or $ExpectedCurrentTip -cnotmatch '^[0-9a-f]{8,40}\z' -or $ExpectedPrimaryTip -cnotmatch '^[0-9a-f]{8,40}\z') {
		Throw-Landing 1 'input.commit-invalid' 'Approved and expected commits must be 8 to 40 lowercase hexadecimal object ID characters.'
	}
	if ($ApprovedCandidateTree -cnotmatch '^[0-9a-f]{40}$') { Throw-Landing 1 'input.candidate-tree-invalid' 'ApprovedCandidateTree must be a lowercase 40-character object ID.' }
	$historyScalarsSupplied = @(@($HistoryContractDigest, $HistoryContractGeneratorDigest, $HistoryContractCaptureDigest, $HistoryContractRuntimeDigest, $HistoryContractPatchDigest, $HistoryContractMode, $HistoryContractCoverage) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) })
	if ($historyScalarsSupplied.Count -gt 0 -and $env:BROKEN_ENGINE_FINALIZE_WORKFLOW_FIXTURE -cne '1') { Throw-Landing 1 'input.fixture-forbidden' 'History Contract scalar arguments are fixture-only; pass -ApprovalPreparationResultFile from approval preparation.' }
	if (-not [string]::IsNullOrWhiteSpace($ApprovalPreparationResultFile)) {
		if ($historyScalarsSupplied.Count -gt 0) { Throw-Landing 1 'input.history-contract-conflict' 'ApprovalPreparationResultFile and history Contract scalar arguments are mutually exclusive.' }
		$approvalItem = Get-Item -LiteralPath $ApprovalPreparationResultFile -Force -ErrorAction SilentlyContinue
		if ($null -eq $approvalItem -or $approvalItem.PSIsContainer) { Throw-Landing 1 'input.approval-result-invalid' "ApprovalPreparationResultFile '$ApprovalPreparationResultFile' is not a readable file." }
		try { $approvalResult = [IO.File]::ReadAllText($approvalItem.FullName) | ConvertFrom-Json -Depth 64 -ErrorAction Stop }
		catch { Throw-Landing 1 'input.approval-result-invalid' "ApprovalPreparationResultFile is not valid JSON: $($_.Exception.Message)" }
		if ($null -eq $approvalResult) { Throw-Landing 1 'input.approval-result-invalid' 'ApprovalPreparationResultFile carries no result object.' }
		$approvalProperties = @($approvalResult.PSObject.Properties.Name)
		if ($approvalProperties -cnotcontains 'schemaVersion' -or $approvalProperties -cnotcontains 'status' -or $approvalProperties -cnotcontains 'historyContract' -or [string]$approvalResult.schemaVersion -cne 'broken-engine-finalize-approval-preparation/v3' -or [string]$approvalResult.status -cne 'pass' -or $null -eq $approvalResult.historyContract) {
			Throw-Landing 1 'input.approval-result-invalid' "ApprovalPreparationResultFile must be a passing 'broken-engine-finalize-approval-preparation/v3' result carrying a history Contract."
		}
		$approvalContract = $approvalResult.historyContract
		foreach ($property in @('digest', 'generatorDigest', 'captureDigest', 'runtimeDigest', 'patchDigest', 'mode')) {
			if (@($approvalContract.PSObject.Properties.Name) -cnotcontains $property) { Throw-Landing 1 'input.approval-result-invalid' "ApprovalPreparationResultFile history Contract is missing '$property'." }
		}
		$HistoryContractDigest = [string]$approvalContract.digest
		$HistoryContractGeneratorDigest = [string]$approvalContract.generatorDigest
		$HistoryContractCaptureDigest = [string]$approvalContract.captureDigest
		$HistoryContractRuntimeDigest = [string]$approvalContract.runtimeDigest
		$HistoryContractPatchDigest = [string]$approvalContract.patchDigest
		$HistoryContractMode = [string]$approvalContract.mode
	}
	$historyArgumentsSupplied = @(@($HistoryContractDigest, $HistoryContractGeneratorDigest, $HistoryContractCaptureDigest, $HistoryContractPatchDigest, $HistoryContractMode) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) })
	if ($historyArgumentsSupplied.Count -eq 0 -and $env:BROKEN_ENGINE_FINALIZE_WORKFLOW_FIXTURE -ne '1') { Throw-Landing 1 'input.history-contract-required' 'A passing history Contract identity from -ApprovalPreparationResultFile is required for postconfirmation landing.' }
	foreach ($digest in @($HistoryContractDigest, $HistoryContractGeneratorDigest, $HistoryContractCaptureDigest, $HistoryContractRuntimeDigest, $HistoryContractPatchDigest)) {
		if (-not [string]::IsNullOrWhiteSpace($digest) -and $digest -cnotmatch '^[0-9a-f]{64}$') { Throw-Landing 1 'input.history-digest-invalid' 'History Contract and artifact digests must be lowercase 64-character SHA-256 values.' }
	}
	foreach ($digest in @($HistoryJsonSha256, $HistorySvgSha256, $HistorySvgEmbeddedSha256)) {
		if (-not [string]::IsNullOrWhiteSpace($digest) -and $digest -cnotmatch '^[0-9a-f]{64}$') { Throw-Landing 1 'input.history-digest-invalid' 'Recovery history artifact digests must be lowercase 64-character SHA-256 values.' }
	}
	if ($HistoryJsonBytes -lt 0 -or $HistorySvgBytes -lt 0) { Throw-Landing 1 'input.history-bytes-invalid' 'Recovery history artifact byte counts must be non-negative.' }
	$recoveryTupleCount = @($HistoryContractRowDate, $HistoryJsonSha256, $HistorySvgSha256, $HistorySvgEmbeddedSha256) | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } | Measure-Object | Select-Object -ExpandProperty Count
	$recoveryBytesSupplied = ($HistoryJsonBytes -gt 0) -or ($HistorySvgBytes -gt 0)
	if (($recoveryTupleCount -gt 0 -or $recoveryBytesSupplied) -and ($recoveryTupleCount -ne 4 -or $HistoryJsonBytes -le 0 -or $HistorySvgBytes -le 0)) { Throw-Landing 1 'input.history-recovery-incomplete' 'Recovery history evidence must be omitted entirely or supplied as the complete row-date, JSONL/SVG hash, byte-count, and embedded-digest tuple.' }
	if (-not [string]::IsNullOrWhiteSpace($OwnerToken) -and $OwnerToken -cnotmatch '^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$') {
		Throw-Landing 1 'input.owner-token-invalid' 'OwnerToken must be a canonical WorktreeCli lock owner token.'
	}

	$script:CurrentIdentity = Get-FinalizeGitIdentity $CurrentWorktree 'Session worktree'
	$script:PrimaryIdentity = Get-FinalizeGitIdentity $PrimaryWorktree 'Primary worktree'
	if (-not $script:CurrentIdentity.CommonDirectory.Equals($script:PrimaryIdentity.CommonDirectory, [StringComparison]::OrdinalIgnoreCase)) {
		Throw-Landing 1 'identity.repository-mismatch' 'Session and primary worktrees do not share one Git common directory.'
	}
	# Only abbreviated inputs are expanded, here, so every later comparison against Git output reads a full ID;
	# a full 40-character input keeps the unchanged path that never resolved it.
	$expandedCommits = @()
	foreach ($commit in @($ApprovedSessionCommit, $ExpectedCurrentTip, $ExpectedPrimaryTip)) {
		if ($commit.Length -eq 40) {
			$expandedCommits += $commit
			continue
		}
		$expansion = Invoke-FinalizeNativeText 'git.exe' @('-C', $script:CurrentIdentity.Worktree, 'rev-parse', '--verify', '--quiet', "$commit^{commit}") $script:CurrentIdentity.Worktree
		$expandedCommit = $expansion.Stdout.Trim()
		if ($expansion.ExitCode -ne 0 -or $expandedCommit -cnotmatch '^[0-9a-f]{40}\z') {
			Throw-Landing 1 'input.commit-invalid' "Commit input '$commit' does not resolve to exactly one commit."
		}
		$expandedCommits += $expandedCommit
	}
	$ApprovedSessionCommit = $expandedCommits[0]
	$ExpectedCurrentTip = $expandedCommits[1]
	$ExpectedPrimaryTip = $expandedCommits[2]
	$result.tips.approvedSession = $ApprovedSessionCommit
	$result.tips.expectedCurrent = $ExpectedCurrentTip
	$result.tips.expectedPrimary = $ExpectedPrimaryTip
	$result.candidate.commit = $ApprovedSessionCommit
	# The SmartGit approval review is a required landing input, not prose: this receipt proves the
	# launch was attempted for exactly this commit. The gate sits after full-ID normalization and
	# before session registration, the recovery scan, and this script's own lock claims, so it blocks
	# like every other pre-landing check: the caller-owned lease is still live and the caller releases
	# it. The outcome stays non-blocking; only a missing attempt blocks.
	if ([string]::IsNullOrWhiteSpace($ApprovalReviewResultFile)) {
		if ($env:BROKEN_ENGINE_FINALIZE_WORKFLOW_FIXTURE -cne '1') { Throw-Landing 2 'approval-review.missing' 'A broken-engine-finalize-approval-review/v1 receipt from -ApprovalReviewResultFile is required for postconfirmation landing.' }
	}
	else {
		$reviewItem = Get-Item -LiteralPath $ApprovalReviewResultFile -Force -ErrorAction SilentlyContinue
		if ($null -eq $reviewItem -or $reviewItem.PSIsContainer) { Throw-Landing 2 'approval-review.missing' "ApprovalReviewResultFile '$ApprovalReviewResultFile' is not a readable file." }
		try { $reviewResult = [IO.File]::ReadAllText($reviewItem.FullName) | ConvertFrom-Json -Depth 64 -ErrorAction Stop }
		catch { Throw-Landing 2 'approval-review.missing' "ApprovalReviewResultFile is not valid JSON: $($_.Exception.Message)" }
		if ($null -eq $reviewResult) { Throw-Landing 2 'approval-review.missing' 'ApprovalReviewResultFile carries no result object.' }
		$reviewProperties = @($reviewResult.PSObject.Properties.Name)
		if ($reviewProperties -cnotcontains 'schemaVersion' -or $reviewProperties -cnotcontains 'status' -or $reviewProperties -cnotcontains 'approvedTip' -or [string]$reviewResult.schemaVersion -cne 'broken-engine-finalize-approval-review/v1') {
			Throw-Landing 2 'approval-review.missing' "ApprovalReviewResultFile must be a 'broken-engine-finalize-approval-review/v1' result carrying status and approvedTip."
		}
		if ([string]$reviewResult.approvedTip -cne $ApprovedSessionCommit) { Throw-Landing 2 'approval-review.candidate-mismatch' 'ApprovalReviewResultFile records a different reviewed commit than the approved session commit. Rerun Show-FinalizeApprovalReview.ps1 -LaunchSmartGit for the approved session commit with the documented stdout redirect so the receipt is overwritten.' }
		if ([string]$reviewResult.status -cnotin @('opened', 'unavailable', 'failed')) { Throw-Landing 2 'approval-review.not-launched' 'ApprovalReviewResultFile does not record an attempted SmartGit launch for the approved session commit. Rerun Show-FinalizeApprovalReview.ps1 -LaunchSmartGit for the approved session commit with the documented stdout redirect so the receipt is overwritten.' }
	}
	$approvedParents = @((Invoke-FinalizeGit $script:CurrentIdentity.Worktree @('show', '-s', '--format=%P', $ApprovedSessionCommit)).Trim() -split ' ' | Where-Object { $_ })
	if ($approvedParents.Count -ne 1) { Throw-Landing 1 'input.commit-invalid' 'Approved session commit must have exactly one parent.' }
	$script:ApprovedSourceParent = $approvedParents[0]
	$script:LandingPrimaryTip = $script:ApprovedSourceParent
	$script:LandingCommit = $ApprovedSessionCommit
	$result.identities.currentWorktree = $script:CurrentIdentity.Worktree
	$result.identities.primaryWorktree = $script:PrimaryIdentity.Worktree
	$result.identities.gitCommonDirectory = $script:CurrentIdentity.CommonDirectory
	$result.identities.currentBranch = $script:CurrentIdentity.Branch
	$result.identities.primaryBranch = $script:PrimaryIdentity.Branch
	$result.tips.current = $script:CurrentIdentity.Head
	$result.tips.primary = $script:PrimaryIdentity.Head
	if ([string]::IsNullOrWhiteSpace($HistoryContractMode)) { $HistoryContractMode = 'carry-forward' }
	$result.historyContract = [ordered]@{ schemaVersion = 'broken-engine-code-quality-history-contract/v1'; status = 'pass'; contractDigest = $HistoryContractDigest; generatorDigest = $HistoryContractGeneratorDigest; captureDigest = $HistoryContractCaptureDigest; runtimeDigest = $HistoryContractRuntimeDigest; patchDigest = $HistoryContractPatchDigest; mode = $HistoryContractMode; rowDate = $HistoryContractRowDate; coverage = $HistoryContractCoverage }
	$result.approvedSource.commit = $ApprovedSessionCommit; $result.approvedSource.tree = $ApprovedCandidateTree; $result.approvedSource.parent = $script:ApprovedSourceParent
	$approvedPatchChanges = Assert-NoHistorySourceChanges $script:ApprovedSourceParent $ApprovedSessionCommit
	$script:ApprovedPatchIdentity = [pscustomobject]@{ PatchId = Get-LandingPatchId $script:ApprovedSourceParent $ApprovedSessionCommit; Changes = $approvedPatchChanges }
	$result.approvedSource.patch = [ordered]@{ patchId = $script:ApprovedPatchIdentity.PatchId; changes = $approvedPatchChanges }
	$script:LandingSession = if ([string]::IsNullOrWhiteSpace($OwnerToken)) { "$SessionLabel/landing" } else { $SessionLabel }
	# A transient operation claim with a fresh per-landing owner excludes AgentTools promotion
	# from swapping WorktreeCli.exe across this multi-invocation landing transaction. Registered
	# before the first WorktreeCli.exe use; released in cleanup alongside the landing lock.
	$landingOwner = [guid]::NewGuid().ToString()
	Register-WorktreeCliSession -RepositoryRoot $script:CurrentIdentity.Worktree -Owner $landingOwner -Label 'session landing' -Worktree $script:CurrentIdentity.Worktree | Out-Null
	$script:LandingTransientOwner = $landingOwner
	# Post-advance recovery, idempotent for a crash at any point after the advance: the confirmed
	# candidate is already on primary either as itself, or as the commit this landing's own internal
	# rebase produced.
	$recovered = $false
	$primaryRefTip = (Invoke-FinalizeGit $script:PrimaryIdentity.Worktree @('rev-parse', "refs/heads/$PrimaryBranch")).Trim()
	$overlayPossible = $HistoryContractMode -cin @('catch-up', 'cpp-change')
	if ($primaryRefTip -cne $ExpectedPrimaryTip) {
		$primaryOutput = Join-Path $script:PrimaryIdentity.Worktree 'Tools\WorktreeCli\Platforms\VisualStudio2026\Output'
		$recoveryWorktreeCli = Join-Path $primaryOutput 'WorktreeCli.exe'
		$recoveryExecutable = Get-Item -LiteralPath $recoveryWorktreeCli -Force -ErrorAction SilentlyContinue
		if ($null -eq $recoveryExecutable -or $recoveryExecutable.PSIsContainer -or ($recoveryExecutable.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 -or $recoveryExecutable.Length -eq 0) { Throw-Landing 1 'worktreecli.executable-invalid' 'Recovery could not validate the canonical WorktreeCli executable.' }
		$script:WorktreeCliPath = $recoveryExecutable.FullName
	}
	if (-not $recovered -and $primaryRefTip -ceq $ApprovedSessionCommit -and
		$script:CurrentIdentity.Head -ceq $script:LandingCommit -and
		$script:PrimaryIdentity.Head -ceq $primaryRefTip -and
		(Invoke-FinalizeGit $script:PrimaryIdentity.Worktree @('status', '--porcelain=v1', '-z', '--untracked-files=all')).Length -eq 0 -and
		(Test-FinalizeGitSuccess $script:PrimaryIdentity.Worktree @('merge-base', '--is-ancestor', $script:LandingCommit, $script:PrimaryIdentity.Head))) {
		[void](Validate-HistoryContractForLanding $script:ApprovedSourceParent $ApprovedSessionCommit)
		$cheapMatch = Find-LandingSourceMatch $primaryRefTip
		if ($null -ne $cheapMatch -and $cheapMatch.SourceOnly -and [string]$result.historyContract.current.mode -ceq 'carry-forward') {
			$recovered = Complete-RecoveredLanding $cheapMatch $true $primaryRefTip
		}
	}
	if (-not $recovered) { $recovered = Test-LandingNoOverlayRecovery $primaryRefTip }
	if (-not $recovered -and $overlayPossible -and $primaryRefTip -cne $ApprovedSessionCommit) { $recovered = Test-HistoryReplacementRecovery }
	if ($recovered) {
		[void] (Assert-LandingSanity $script:LandingCommit $script:PrimaryIdentity.Head)
		$result.primaryAdvanced = $true
		$result.landed.commit = $script:LandingCommit
		$result.landed.tree = $script:LandingTree
		# The crashed invocation's lease outlives its process, so recovery adopts and releases only a
		# live claim matching the supplied raw or omitted derived identity and canonical worktree.
		# Anything else is foreign and is left alone to expire on its own.
		$recoveredLock = Get-FinalizeLandingLockState $script:WorktreeCliPath $result.identities.gitCommonDirectory $script:CurrentIdentity.Worktree
		$recoveryOwner = $OwnerToken
		if ([string]::IsNullOrWhiteSpace($OwnerToken) -and $recoveredLock.Kind -ceq 'live' -and @($recoveredLock.Status.PSObject.Properties.Name) -ccontains 'owner') {
			$recoveryOwner = [string]$recoveredLock.Status.owner
		}
		if (($recoveredLock.Kind -ceq 'live') -and -not [string]::IsNullOrWhiteSpace($recoveryOwner) -and (Test-FinalizeLandingLockClaimIdentity $recoveredLock.Status $recoveryOwner $script:LandingSession $script:CurrentIdentity.Worktree)) {
			$script:LandingOwner = $recoveryOwner
			$result.locks.landingOwner = $script:LandingOwner
			$script:LandingClaimed = $true
			$result.locks.landingClaimed = $true
			Release-LandingLockIfSafe
		}
		Complete-LandedState
		Write-Output ((New-LandingProjection) | ConvertTo-Json -Depth 10 -Compress)
		exit 0
	}

	# The primary tip is deliberately not asserted here: an advance racing this landing is answered
	# by the post-claim check and the internal rebase, not by refusing before the lock is held.
	[void] (Assert-LandingSanity $ExpectedCurrentTip '')
	if ($ExpectedCurrentTip -cne $script:LandingCommit) { Throw-Landing 2 'approval.session-tip-changed' 'Session tip is not the explicit user-approved commit.' }

	if ([string]::IsNullOrWhiteSpace($OwnerToken)) {
		# Landing claims use a derived identity so the caller's raw reconciliation lease remains distinct.
		# Refresh preserves a lease's recorded duration, so a matching short lease cannot be adopted for
		# the full landing transaction.
		$lockState = Get-FinalizeLandingLockState $script:WorktreeCliPath $result.identities.gitCommonDirectory $script:CurrentIdentity.Worktree
		$leaseDurationSeconds = if (@($lockState.Status.PSObject.Properties.Name) -ccontains 'leaseDurationSeconds') { $lockState.Status.leaseDurationSeconds } else { $null }
		$leaseDurationIsInteger = $leaseDurationSeconds -is [int] -or $leaseDurationSeconds -is [long]
		if (($lockState.Kind -ceq 'live') -and (Test-FinalizeLandingLockClaimIdentity $lockState.Status ([string]$lockState.Status.owner) $script:LandingSession $script:CurrentIdentity.Worktree) -and $leaseDurationIsInteger -and [int64]$leaseDurationSeconds -ge $script:LandingLeaseSeconds) {
			$script:LandingOwner = [string]$lockState.Status.owner
			$script:LandingOwnerAdopted = $true
		}
		else {
			$tokenResponse = Invoke-WorktreeCli @('lock', 'token')
			if ($tokenResponse.ExitCode -ne 0 -or $tokenResponse.Stdout.Trim() -cnotmatch '^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$') {
				Throw-Landing 1 'landing-lock.token-failed' 'WorktreeCli could not generate a canonical landing owner token.'
			}
			$script:LandingOwner = $tokenResponse.Stdout.Trim()
		}
		$result.locks.landingOwner = $script:LandingOwner
		$claimOutcome = Invoke-FinalizeLandingLockClaim -WorktreeCliExecutable $script:WorktreeCliPath -GitCommonDirectory $result.identities.gitCommonDirectory -Owner $script:LandingOwner -Session $script:LandingSession -Worktree $script:CurrentIdentity.Worktree -LeaseSeconds $script:LandingLeaseSeconds -WaitSeconds 300
	}
	else {
		# Lease continuity: a live lease under the supplied token whose recorded session and worktree
		# match this landing identity, and which was minted for at least the landing lease duration,
		# is the same actor continuing, so it is used as is. Anything else — another session or
		# worktree, a shorter lease, expired, or absent — stays foreign contention, and no fresh
		# token is ever minted behind the caller's back.
		$script:LandingOwner = $OwnerToken
		$result.locks.landingOwner = $script:LandingOwner
		$lockState = Get-FinalizeLandingLockState $script:WorktreeCliPath $result.identities.gitCommonDirectory $script:CurrentIdentity.Worktree
		$sameActor = ($lockState.Kind -ceq 'live') -and (Test-FinalizeLandingLockClaimIdentity $lockState.Status $script:LandingOwner $SessionLabel $script:CurrentIdentity.Worktree)
		$leaseSeconds = if ($sameActor -and @($lockState.Status.PSObject.Properties.Name) -ccontains 'leaseDurationSeconds') { $lockState.Status.leaseDurationSeconds -as [int] } else { $null }
		$claimOutcome = if ($sameActor -and $null -ne $leaseSeconds -and $leaseSeconds -ge $script:LandingLeaseSeconds) {
			[pscustomobject]@{ Claimed = $true; Code = 'ok'; Message = 'Landing continues under the caller lease already live for this session and worktree.'; Disposition = 'terminal'; RequiresUserAuthority = $false; RetryAfterMilliseconds = 0; Owner = $script:LandingOwner; Lock = $lockState.Status; Attempts = 1 }
		}
		else {
			$refusal = if ($sameActor) { "The supplied owner token's lease is shorter than the $($script:LandingLeaseSeconds)-second landing lease and cannot be extended." } else { 'The supplied owner token is not a live landing lease for this session and worktree.' }
			[pscustomobject]@{ Claimed = $false; Code = 'landing-lock.retryable-wait'; Message = $refusal; Disposition = 'retryable-wait'; RequiresUserAuthority = $false; RetryAfterMilliseconds = 500; Owner = $script:LandingOwner; Lock = $lockState.Status; Attempts = 1 }
		}
	}
	$result.locks.claim = [ordered]@{
		code = $claimOutcome.Code
		disposition = $claimOutcome.Disposition
		requiresUserAuthority = $claimOutcome.RequiresUserAuthority
		retryAfterMilliseconds = $claimOutcome.RetryAfterMilliseconds
		attempts = $claimOutcome.Attempts
		lock = $claimOutcome.Lock
	}
	if (-not $claimOutcome.Claimed) {
		$exitCode = if ($claimOutcome.Disposition -ceq 'terminal') { 1 } else { 2 }
		Throw-Landing $exitCode 'landing-lock.claim-failed' $claimOutcome.Message $claimOutcome.Disposition $claimOutcome.RequiresUserAuthority $claimOutcome.RetryAfterMilliseconds
	}
	$script:LandingClaimed = $true
	$result.locks.landingClaimed = $true
	if ($script:LandingOwnerAdopted) { Refresh-LandingOwner }
	Assert-ApprovedCandidateTree
	# Freeze all source-commit metadata and the one UTC row date while this lease is held. A later
	# rebase may change object IDs, but it must not change what the user approved or the history point.
	$script:FrozenCommitMetadata = Get-FrozenCommitMetadata $ApprovedSessionCommit
	$result.approvedSource.metadata = $script:FrozenCommitMetadata
	if ([string]::IsNullOrWhiteSpace($HistoryContractRowDate)) { $HistoryContractRowDate = [DateTime]::UtcNow.ToString('yyyy-MM-dd') }
	$result.historyContract.rowDate = $HistoryContractRowDate
	$result.rebasedSource.commit = $script:LandingCommit
	$result.rebasedSource.tree = $script:LandingTree
	$result.rebasedSource.parent = $script:LandingPrimaryTip
	$result.rebasedSource.patch = $result.approvedSource.patch

	# At most one rebase-and-retry per invocation, entirely under the held lease: a second stale
	# result means contention this landing should not keep fighting. The retry left the branch on the
	# rebased commit, so exhaustion restores the confirmed one the documented rerun expects.
	if (-not (Invoke-LandingAdvanceAttempt)) {
		Invoke-LandingRebaseOntoPrimary
		if (-not (Invoke-LandingAdvanceAttempt)) {
			Restore-LandingSessionBranch 'landing.retry-exhausted' 'Primary advanced again after the rebased candidate was prepared; this landing made its one retry.' 'retryable-wait' 500
		}
	}
	Release-LandingLockIfSafe
	if ($script:LandingClaimed) { Throw-Landing 2 'landing-lock.release-failed' 'Landing lock could not be released after the primary advance.' }
	Complete-LandedState
}
catch {
	$script:FailureExitCode = if ($_.Exception.Data.Contains('FinalizeExitCode')) { [int] $_.Exception.Data['FinalizeExitCode'] } else { 1 }
	$script:FailureCode = if ($_.Exception.Data.Contains('FinalizeCode')) { [string] $_.Exception.Data['FinalizeCode'] } else { 'internal.error' }
	$script:FailureMessage = $_.Exception.Message
	$result.disposition = if ($_.Exception.Data.Contains('FinalizeDisposition')) { [string] $_.Exception.Data['FinalizeDisposition'] } else { 'terminal' }
	$result.requiresUserAuthority = if ($_.Exception.Data.Contains('FinalizeRequiresUserAuthority')) { [bool] $_.Exception.Data['FinalizeRequiresUserAuthority'] } else { $false }
	$result.retryAfterMilliseconds = if ($_.Exception.Data.Contains('FinalizeRetryAfterMilliseconds')) { [int] $_.Exception.Data['FinalizeRetryAfterMilliseconds'] } else { 0 }
	$result.blocker = [ordered]@{
		disposition = $result.disposition
		requiresUserAuthority = $result.requiresUserAuthority
		retryAfterMilliseconds = $result.retryAfterMilliseconds
	}
}
finally {
	Release-LandingLockIfSafe
	if ($null -ne $script:LandingTransientOwner) {
		try { Unregister-WorktreeCliSession -RepositoryRoot $script:CurrentIdentity.Worktree -Owner $script:LandingTransientOwner }
		catch { $result.residuals.Add("Landing transient claim release failed: $($_.Exception.Message)") }
	}
}

if ($script:FailureExitCode -ne 0) {
	$result.status = if ($script:FailureExitCode -eq 2) { 'blocked' } else { 'error' }
	$result.code = $script:FailureCode
	$result.message = $script:FailureMessage
}
if ($script:LandingClaimed) {
	if ($result.status -eq 'landed') {
		$result.status = 'blocked'
		$result.code = 'cleanup.incomplete'
		$result.message = 'Landing completed but required lock cleanup was not proven complete.'
	}
	if ($result.status -eq 'error') { $result.code = 'cleanup.' + $result.code }
}
Write-Output ((New-LandingProjection) | ConvertTo-Json -Depth 10 -Compress)
exit $(if ($result.status -eq 'landed') { 0 } elseif ($result.status -eq 'blocked') { 2 } else { 1 })
