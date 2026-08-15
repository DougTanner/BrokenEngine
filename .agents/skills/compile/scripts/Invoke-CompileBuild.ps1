# The single documented /compile build entry point. Every invocation is independently valid in a
# fresh shell: identities, data mode, directories, and tool paths are re-derived per call, and the
# only non-derivable state (which oracle receipts were issued, whether a Local generation already
# ran) lives in the compile-owned worktree state file described below.
#
# Stdout contract: when WorktreeCli runs, its `broken-engine-build-result/v1` object is the only
# thing on stdout - this script never captures, rewrites, or impersonates it, and its exit code is
# returned unchanged (exit 4 is the single reserved exception below). When the build never runs,
# stdout carries exactly one `broken-engine-compile-invoke-result/v1` object instead; the schema
# name, not the exit code, is what distinguishes the two.
# Exit codes: WorktreeCli's own code once the build ran, 4 when the build succeeded but post-build
# oracle work (verification, receipt issuance, or state persistence) failed, 2 for a structured
# pre-invocation block, 1 for an internal error.
[CmdletBinding()]
param(
	[Parameter(Mandatory)][ValidateSet('ThirdParty', 'DataPacker', 'Client', 'Server', 'WorktreeCli', 'AgentHarness')][string] $Target,
	[ValidateSet('Debug', 'Profile', 'Release')][string] $Configuration,
	[string[]] $Files,
	[ValidateSet('Shared', 'Local')][string] $DataBuildMode,
	[switch] $RunDataPacker,
	[switch] $AllowGaeaExport,
	[switch] $AcceptDeletionOnlyException,
	[switch] $ReissueSharedReceipt,
	[switch] $Prefast,
	[string] $RepositoryRoot,
	[string] $PrimaryCheckout,
	[string] $Baseline
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$sharedScripts = Join-Path $PSScriptRoot '..\..\..\scripts'
if (-not (Test-Path -LiteralPath (Join-Path $sharedScripts 'AgentScriptCommon.psm1'))) {
	$sharedScripts = Join-Path $PSScriptRoot '..\..\..\..\.agents\scripts'
}
Import-Module (Join-Path $sharedScripts 'AgentScriptCommon.psm1') -Force

# ValidateSet binds any casing, while every routing comparison below is case-sensitive, so each
# bound value is normalized once to the exact spelling its own ValidateSet declares.
foreach ($parameterName in @('Target', 'Configuration', 'DataBuildMode')) {
	$boundValue = [string](Get-Variable -Name $parameterName -ValueOnly)
	if ([string]::IsNullOrWhiteSpace($boundValue)) { continue }
	$declared = @($PSCmdlet.MyInvocation.MyCommand.Parameters[$parameterName].Attributes |
		Where-Object { $_ -is [Management.Automation.ValidateSetAttribute] } |
		ForEach-Object { $_.ValidValues })
	Set-Variable -Name $parameterName -Value @($declared | Where-Object { $_ -eq $boundValue })[0]
}

$script:GameTargets = @('Client', 'Server')
$script:ReleaseOnlyTargets = @('DataPacker', 'WorktreeCli', 'AgentHarness')
$script:AgentToolsTargets = @('WorktreeCli', 'AgentHarness')
$script:StateSchema = 'broken-engine-compile-oracle-state/v1'
$script:LocalInputsFraming = 'broken-engine-compile-local-inputs/v1'
$script:GaeaGuardVariable = 'BT_DATAPACKER_FORBID_GAEA_EXPORT'
$script:TargetFiles = @{
	ThirdParty = 'ThirdParty\Prebuilts\Platforms\VisualStudio2026\ThirdParty.sln'
	DataPacker = 'DataPacker\Platforms\VisualStudio2026\DataPacker.sln'
	Client = 'Projects\BrokenEngineSandbox\Platforms\VisualStudio2026\BrokenEngineSandbox.sln'
	Server = 'Projects\BrokenEngineSandbox\Platforms\VisualStudio2026\BrokenEngineSandboxServer.sln'
	WorktreeCli = 'Tools\WorktreeCli\Platforms\VisualStudio2026\WorktreeCli.sln'
	AgentHarness = 'Tools\AgentHarness\Platforms\VisualStudio2026\AgentHarness.sln'
}
# --files requires a .vcxproj target, so a selective compile builds the project, not the solution.
$script:SelectiveTargetFiles = @{
	Client = 'Projects\BrokenEngineSandbox\Platforms\VisualStudio2026\BrokenEngineSandbox.vcxproj'
	Server = 'Projects\BrokenEngineSandbox\Platforms\VisualStudio2026\BrokenEngineSandboxServer.vcxproj'
}

$script:BlockedCode = $null
# Set the moment WorktreeCli's inherited stdout can carry its envelope: after that, nothing this
# script does may write a second stdout object, so every later failure is stderr plus an exit code.
$script:BuildStreamed = $false
$script:BuildExitCode = 0
$script:Summary = [Collections.Generic.List[string]]::new()
# The running pwsh image is the interpreter for every child script invocation.
$script:PowerShellPath = [Diagnostics.Process]::GetCurrentProcess().MainModule.FileName

function Write-CompileDiagnostic([string] $Message) {
	[Console]::Error.WriteLine("compile-build: $Message")
}

function Complete-CompileInvoke([int] $ExitCode, [string] $Status, [string] $Code, [string] $Message) {
	$result = [ordered]@{
		schemaVersion = 'broken-engine-compile-invoke-result/v1'
		status = $Status
		code = $Code
		message = $Message
		target = $Target
		configuration = $script:ResolvedConfiguration
	}
	[Console]::Out.Write(($result | ConvertTo-Json -Depth 6 -Compress))
	Write-CompileDiagnostic "$Status $Code - $Message"
	exit $ExitCode
}

function Stop-CompileInvoke([string] $Code, [string] $Message) {
	$script:BlockedCode = $Code
	throw $Message
}

function Start-CompileChild([string] $FilePath, [string[]] $Arguments, [bool] $CaptureStandardOutput) {
	$info = [Diagnostics.ProcessStartInfo]::new()
	$info.FileName = $FilePath
	foreach ($argument in $Arguments) { [void] $info.ArgumentList.Add($argument) }
	$info.UseShellExecute = $false
	$info.RedirectStandardOutput = $CaptureStandardOutput
	$info.WorkingDirectory = $PSScriptRoot
	$process = [Diagnostics.Process]::Start($info)
	try {
		# Standard handles this script does not capture are inherited, so a child's stdout reaches
		# the caller byte-verbatim and its stderr interleaves with these diagnostics.
		# Only a launched inherited-stdout child can own the caller's stdout, so a launch failure
		# still reaches the typed pre-build envelope instead of exiting with empty stdout.
		if (-not $CaptureStandardOutput) { $script:BuildStreamed = $true }
		$standardOutput = if ($CaptureStandardOutput) { $process.StandardOutput.ReadToEnd() } else { '' }
		$process.WaitForExit()
		return [pscustomobject]@{ ExitCode = $process.ExitCode; Stdout = $standardOutput }
	}
	finally { $process.Dispose() }
}

function Invoke-CompileScript([string] $ScriptName, [string[]] $Arguments) {
	$scriptPath = Join-Path $PSScriptRoot $ScriptName
	return Start-CompileChild $script:PowerShellPath (@('-NoProfile', '-File', $scriptPath) + $Arguments) $true
}

function Invoke-SharedScript([string] $ScriptName, [string[]] $Arguments) {
	$scriptPath = Join-Path $sharedScripts $ScriptName
	return Start-CompileChild $script:PowerShellPath (@('-NoProfile', '-File', $scriptPath) + $Arguments) $true
}

function ConvertFrom-ChildJson([string] $Text, [string] $Label) {
	if ([string]::IsNullOrWhiteSpace($Text)) { Stop-CompileInvoke 'child.output-invalid' "$Label produced no JSON result." }
	try { return $Text | ConvertFrom-Json }
	catch { Stop-CompileInvoke 'child.output-invalid' "$Label produced an unparsable JSON result." }
}

function Test-JsonProperty([object] $Object, [string] $Name) {
	return $null -ne $Object -and $null -ne $Object.PSObject.Properties[$Name]
}

function Write-AtomicUtf8NoBom([string] $Path, [string] $Text) {
	$directory = [IO.Path]::GetDirectoryName($Path)
	if (-not (Test-Path -LiteralPath $directory)) { [void] (New-Item -ItemType Directory -Path $directory -Force) }
	$temporary = Join-Path $directory ('.' + [IO.Path]::GetFileName($Path) + '.' + [guid]::NewGuid().ToString('N') + '.tmp')
	$bytes = [Text.UTF8Encoding]::new($false, $true).GetBytes($Text)
	try {
		[IO.File]::WriteAllBytes($temporary, $bytes)
		if (Test-Path -LiteralPath $Path) { [IO.File]::Replace($temporary, $Path, [System.Management.Automation.Language.NullString]::Value, $true) }
		else { [IO.File]::Move($temporary, $Path) }
	}
	finally {
		if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force }
	}
}

# --- Oracle state file -------------------------------------------------------------------------

function New-CompileOracleState {
	return [ordered]@{
		schemaVersion = $script:StateSchema
		selectedMode = $null
		receipts = [ordered]@{ selected = $null; shared = $null }
		generationCompleted = $false
		localInputsIdentity = $null
	}
}

function ConvertTo-ReceiptEntry([object] $Value, [string] $Label) {
	if ($null -eq $Value) { return $null }
	foreach ($name in @('receiptPath', 'receiptSha256', 'dataRoot', 'mode', 'baseline', 'issuedAtUtc', 'breached')) {
		if (-not (Test-JsonProperty $Value $name)) { Stop-CompileInvoke 'oracle-state.invalid' "$Label is missing '$name'." }
	}
	if ($Value.mode -cnotin @('Local', 'Shared')) { Stop-CompileInvoke 'oracle-state.invalid' "$Label has an invalid mode." }
	if ([string]$Value.receiptSha256 -cnotmatch '^[0-9a-f]{64}$') { Stop-CompileInvoke 'oracle-state.invalid' "$Label has an invalid receipt SHA-256." }
	if ([string]$Value.baseline -cnotmatch '^[0-9a-f]{40}$') { Stop-CompileInvoke 'oracle-state.invalid' "$Label has an invalid baseline." }
	# A JSON string casts to $true whatever it says, so only a real boolean may decide a breach.
	if ($Value.breached -isnot [bool]) { Stop-CompileInvoke 'oracle-state.invalid' "$Label has a non-boolean 'breached'." }
	$entry = [ordered]@{
		receiptPath = [string]$Value.receiptPath
		receiptSha256 = [string]$Value.receiptSha256
		dataRoot = [string]$Value.dataRoot
		mode = [string]$Value.mode
		baseline = [string]$Value.baseline
		issuedAtUtc = [string]$Value.issuedAtUtc
		breached = [bool]$Value.breached
	}
	if (Test-JsonProperty $Value 'breachedAtUtc') { $entry.breachedAtUtc = [string]$Value.breachedAtUtc }
	# Optional: a state file written before an entry recorded content identity still loads unchanged.
	foreach ($name in @('aggregateDigest', 'preGenerationAggregateDigest')) {
		if (Test-JsonProperty $Value $name) { $entry[$name] = [string]$Value.$name }
	}
	if (Test-JsonProperty $Value 'generationDeltaChanged') {
		if ($Value.generationDeltaChanged -isnot [bool]) { Stop-CompileInvoke 'oracle-state.invalid' "$Label has a non-boolean 'generationDeltaChanged'." }
		$entry.generationDeltaChanged = [bool]$Value.generationDeltaChanged
	}
	return $entry
}

function Read-CompileOracleState([string] $Path) {
	if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return New-CompileOracleState }
	$text = $null
	try { $text = [IO.File]::ReadAllText($Path) }
	catch { Stop-CompileInvoke 'oracle-state.invalid' "Compile oracle state file '$Path' could not be read; delete it and re-issue receipts." }
	$parsed = $null
	try { $parsed = $text | ConvertFrom-Json }
	catch { Stop-CompileInvoke 'oracle-state.invalid' "Compile oracle state file '$Path' is not valid JSON; delete it and re-issue receipts." }
	if (-not (Test-JsonProperty $parsed 'schemaVersion') -or $parsed.schemaVersion -cne $script:StateSchema) {
		Stop-CompileInvoke 'oracle-state.invalid' "Compile oracle state file '$Path' is not $($script:StateSchema); delete it and re-issue receipts."
	}
	foreach ($name in @('selectedMode', 'receipts', 'generationCompleted', 'localInputsIdentity')) {
		if (-not (Test-JsonProperty $parsed $name)) { Stop-CompileInvoke 'oracle-state.invalid' "Compile oracle state file '$Path' is missing '$name'; delete it and re-issue receipts." }
	}
	$state = New-CompileOracleState
	$state.selectedMode = if ($null -eq $parsed.selectedMode) { $null } else { [string]$parsed.selectedMode }
	if ($parsed.generationCompleted -isnot [bool]) { Stop-CompileInvoke 'oracle-state.invalid' "Compile oracle state file '$Path' has a non-boolean 'generationCompleted'; delete it and re-issue receipts." }
	$state.generationCompleted = [bool]$parsed.generationCompleted
	$state.localInputsIdentity = if ($null -eq $parsed.localInputsIdentity) { $null } else { [string]$parsed.localInputsIdentity }
	foreach ($slot in @('selected', 'shared')) {
		if (-not (Test-JsonProperty $parsed.receipts $slot)) { Stop-CompileInvoke 'oracle-state.invalid' "Compile oracle state file '$Path' is missing 'receipts.$slot'; delete it and re-issue receipts." }
		$state.receipts[$slot] = ConvertTo-ReceiptEntry $parsed.receipts.$slot "Compile oracle state 'receipts.$slot'"
	}
	return $state
}

function Save-CompileOracleState {
	Write-AtomicUtf8NoBom $script:StatePath (($script:State | ConvertTo-Json -Depth 8))
}

# --- Local input identity ----------------------------------------------------------------------

# A changed Git submodule is reported as its directory path, so a leaf-only test would hash every
# submodule revision identically to a deletion. The checked-out submodule's own HEAD is exactly what
# the baseline diff compared the recorded gitlink against, so contributing it holds the invariant
# that the identity changes whenever a submodule revision changes. Requiring the reported top level
# to be the directory itself is what proves the directory is its own repository: without that check
# `git -C` walks up to the enclosing worktree and reports this repository's HEAD for any directory.
function Get-ChangedDirectoryCommit([string] $FullPath) {
	$response = Start-CompileChild 'git.exe' @('-C', $FullPath, 'rev-parse', '--path-format=absolute', '--show-toplevel', 'HEAD') $true
	if ($response.ExitCode -ne 0) { return $null }
	$lines = @($response.Stdout -split "`n" | ForEach-Object { $_.Trim() } | Where-Object { $_.Length -gt 0 })
	if ($lines.Count -ne 2 -or $lines[1] -cnotmatch '^[0-9a-f]{40}$') { return $null }
	if (-not (Get-AgentCanonicalPath $lines[0]).Equals((Get-AgentCanonicalPath $FullPath), [StringComparison]::OrdinalIgnoreCase)) { return $null }
	return $lines[1]
}

# Digest of the freshly derived changed-input state. A persisted Local receipt is reusable only
# while this digest still matches, so a fresh shell can never inherit a stale Local identity. Every
# changed path counts, not just the path triggers: a judgment-forced Local build is motivated by a
# file the path rules do not recognize, and that file must still invalidate the receipt. Every
# reported path contributes exactly one marker, so the digest stays total over the changed set.
function Get-LocalInputsIdentity([object] $Context, [string] $Root) {
	$paths = @(@($Context.changedPaths | ForEach-Object { [string]$_ }) | Sort-Object -CaseSensitive)
	$utf8 = [Text.UTF8Encoding]::new($false, $true)
	$hash = [Security.Cryptography.IncrementalHash]::CreateHash([Security.Cryptography.HashAlgorithmName]::SHA256)
	try {
		$hash.AppendData($utf8.GetBytes($script:LocalInputsFraming))
		$hash.AppendData([byte[]]@(0))
		foreach ($path in $paths) {
			$hash.AppendData($utf8.GetBytes($path))
			$hash.AppendData([byte[]]@(0))
			$full = Join-Path $Root ($path.Replace('/', '\'))
			if (Test-Path -LiteralPath $full -PathType Leaf) {
				# The file can still vanish between the test and the read; that is a deletion, not an error.
				$content = $null
				try { $content = [Security.Cryptography.SHA256]::HashData([IO.File]::ReadAllBytes($full)) }
				catch [IO.FileNotFoundException] { $content = $null }
				catch [IO.DirectoryNotFoundException] { $content = $null }
				if ($null -ne $content) {
					$hash.AppendData($utf8.GetBytes('f'))
					$hash.AppendData($content)
				}
				else { $hash.AppendData($utf8.GetBytes('x')) }
			}
			elseif (Test-Path -LiteralPath $full -PathType Container) {
				$hash.AppendData($utf8.GetBytes('d'))
				$commit = Get-ChangedDirectoryCommit $full
				if ($null -ne $commit) { $hash.AppendData($utf8.GetBytes($commit)) }
			}
			else { $hash.AppendData($utf8.GetBytes('x')) }
			$hash.AppendData([byte[]]@(0))
		}
		return [Convert]::ToHexString($hash.GetHashAndReset()).ToLowerInvariant()
	}
	finally { $hash.Dispose() }
}

# --- Oracle receipts ---------------------------------------------------------------------------

function New-OracleReceiptEntry([string] $DataRoot, [string] $Mode, [string] $BaselineCommit, [string] $ReceiptPath) {
	$response = Invoke-CompileScript 'New-DataOracleReceipt.ps1' @('-DataRoot', $DataRoot, '-Mode', $Mode, '-Baseline', $BaselineCommit, '-ReceiptPath', $ReceiptPath)
	$result = ConvertFrom-ChildJson $response.Stdout 'New-DataOracleReceipt.ps1'
	if ($response.ExitCode -ne 0 -or $result.status -cne 'pass' -or $result.code -cne 'ok') {
		Stop-CompileInvoke 'data-oracle.issue-failed' "$Mode oracle receipt could not be issued for '$DataRoot': $($result.message)"
	}
	$script:Summary.Add("$Mode receipt issued: path=$($result.receiptPath) sha256=$($result.receiptSha256) dataRoot=$DataRoot baseline=$BaselineCommit aggregate=$($result.aggregateDigest)")
	return [ordered]@{
		receiptPath = [string]$result.receiptPath
		receiptSha256 = [string]$result.receiptSha256
		dataRoot = $DataRoot
		mode = $Mode
		baseline = $BaselineCommit
		issuedAtUtc = [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ss.fffffffZ', [Globalization.CultureInfo]::InvariantCulture)
		breached = $false
		aggregateDigest = [string]$result.aggregateDigest
	}
}

function Test-OracleReceiptEntry([object] $Entry) {
	$response = Invoke-CompileScript 'Test-DataOracleReceipt.ps1' @(
		'-ReceiptPath', $Entry.receiptPath,
		'-ReceiptSha256', $Entry.receiptSha256,
		'-ExpectedDataRoot', $Entry.dataRoot,
		'-ExpectedMode', $Entry.mode,
		'-ExpectedBaseline', $Entry.baseline)
	$result = ConvertFrom-ChildJson $response.Stdout 'Test-DataOracleReceipt.ps1'
	$passed = $response.ExitCode -eq 0 -and $result.status -ceq 'pass' -and $result.code -ceq 'ok'
	if ($passed) {
		$script:Summary.Add("$($Entry.mode) receipt verified: path=$($Entry.receiptPath) sha256=$($Entry.receiptSha256) dataRoot=$($Entry.dataRoot) baseline=$($Entry.baseline) aggregate=$($result.aggregateDigest)")
	}
	return [pscustomobject]@{ Passed = $passed; Message = [string]$result.message }
}

function Test-EntryMatches([object] $Entry, [string] $Mode, [string] $DataRoot, [string] $BaselineCommit) {
	return $null -ne $Entry -and $Entry.mode -ceq $Mode -and $Entry.baseline -ceq $BaselineCommit -and
		$Entry.dataRoot.Equals($DataRoot, [StringComparison]::OrdinalIgnoreCase)
}

function Set-ReceiptBreached([string] $Slot) {
	$entry = $script:State.receipts[$Slot]
	if ($null -eq $entry) { return }
	$entry.breached = $true
	$entry.breachedAtUtc = [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ss.fffffffZ', [Globalization.CultureInfo]::InvariantCulture)
	Save-CompileOracleState
}

# Shared receipt state machine, shared by Shared mode and by Local mode's independent primary check.
function Resolve-SharedReceipt([string] $Slot, [string] $DataRoot, [string] $BaselineCommit) {
	$entry = $script:State.receipts[$Slot]
	if (Test-EntryMatches $entry 'Shared' $DataRoot $BaselineCommit) {
		if ($entry.breached -and -not $ReissueSharedReceipt) {
			Stop-CompileInvoke 'data-oracle.breached' "The persisted Shared receipt for '$DataRoot' is marked breached by an earlier post-build verification. Only an explicit -ReissueSharedReceipt clears it."
		}
		if (-not $ReissueSharedReceipt) {
			$verification = Test-OracleReceiptEntry $entry
			if ($verification.Passed) { return }
			Stop-CompileInvoke 'shared-data.drift' "Primary Shared data changed under the persisted receipt for '$DataRoot': $($verification.Message). Confirm a wrapper session-start DataPacker refresh explains it, then re-run with -ReissueSharedReceipt; an unconfirmed primary write fails the workflow."
		}
	}
	$script:State.receipts[$Slot] = New-OracleReceiptEntry $DataRoot 'Shared' $BaselineCommit (Join-Path $script:ReceiptDirectory "$Slot.json")
	Save-CompileOracleState
}

# --- Build invocation --------------------------------------------------------------------------

function Get-AnalysisArguments {
	if ($Prefast) {
		# EnableMicrosoftCodeAnalysis=true only guards against an upstream default change.
		return @('/p:EnableClangTidyCodeAnalysis=false', '/p:EnableMicrosoftCodeAnalysis=true', '/p:RunCodeAnalysis=true', '/verbosity:minimal')
	}
	return @('/p:EnableClangTidyCodeAnalysis=false', '/p:RunCodeAnalysis=false', '/verbosity:minimal')
}

function Invoke-WorktreeCliBuild([string[]] $DataProperties) {
	$arguments = [Collections.Generic.List[string]]::new()
	$arguments.Add('build')
	if ($script:SelectedFiles.Count -gt 0) {
		$arguments.Add('--files')
		foreach ($file in $script:SelectedFiles) { $arguments.Add($file) }
		$arguments.Add('--')
	}
	$arguments.Add($script:TargetPath)
	$arguments.Add("/p:Configuration=$($script:ResolvedConfiguration)")
	$arguments.Add('/p:Platform=x64')
	if ($Target -cin $script:AgentToolsTargets) {
		# The default Output is the shared immutable primary Output the running driver holds.
		$arguments.Add("/p:OutDir=$($script:Root)\Temp\AgentToolsCandidate\")
	}
	foreach ($property in $DataProperties) { $arguments.Add($property) }
	foreach ($argument in (Get-AnalysisArguments)) { $arguments.Add($argument) }
	$script:Summary.Add("WorktreeCli build arguments: $($arguments -join ' ')")
	# Stdout is not redirected: WorktreeCli's result envelope is the caller's stdout, byte-verbatim.
	$script:BuildExitCode = (Start-CompileChild $script:WorktreeCliPath $arguments.ToArray() $false).ExitCode
	return $script:BuildExitCode
}

function Complete-CompileBuild([int] $BuildExitCode, [string[]] $BreachedSlots) {
	foreach ($line in $script:Summary) { Write-CompileDiagnostic $line }
	if ($BreachedSlots.Count -gt 0) {
		foreach ($slot in $BreachedSlots) { Set-ReceiptBreached $slot }
		Write-CompileDiagnostic "blocked data-oracle.post-build-failed - post-build oracle verification failed for: $($BreachedSlots -join ', '). The receipt is marked breached and every later build fails closed until the authorized clearing transition runs."
		if ($BuildExitCode -eq 0) { exit 4 }
	}
	exit $BuildExitCode
}

# --- Main --------------------------------------------------------------------------------------

$script:ResolvedConfiguration = $Configuration
$script:SelectedFiles = @()
try {
	# Parameter contract first: every one of these blocks before any child process runs.
	$isGameTarget = $Target -cin $script:GameTargets
	if ($Target -cin $script:ReleaseOnlyTargets) {
		if (-not [string]::IsNullOrWhiteSpace($Configuration) -and $Configuration -cne 'Release') {
			Stop-CompileInvoke 'parameter.configuration-invalid' "$Target builds Release only; -Configuration $Configuration is not permitted."
		}
		$script:ResolvedConfiguration = 'Release'
	}
	elseif ([string]::IsNullOrWhiteSpace($Configuration)) {
		Stop-CompileInvoke 'parameter.configuration-required' "-Configuration is required for $Target."
	}
	if ($Prefast -and (-not $isGameTarget -or $script:ResolvedConfiguration -cne 'Release')) {
		Stop-CompileInvoke 'parameter.prefast-invalid' 'PREfast verification applies to Client or Server Release builds only.'
	}
	if (-not $isGameTarget) {
		foreach ($pair in @(
			@{ Set = $null -ne $Files -and $Files.Count -gt 0; Name = '-Files' },
			@{ Set = -not [string]::IsNullOrWhiteSpace($DataBuildMode); Name = '-DataBuildMode' },
			@{ Set = [bool]$RunDataPacker; Name = '-RunDataPacker' },
			@{ Set = [bool]$AllowGaeaExport; Name = '-AllowGaeaExport' },
			@{ Set = [bool]$AcceptDeletionOnlyException; Name = '-AcceptDeletionOnlyException' },
			@{ Set = [bool]$ReissueSharedReceipt; Name = '-ReissueSharedReceipt' })) {
			if ($pair.Set) { Stop-CompileInvoke 'parameter.game-only' "$($pair.Name) applies to Client or Server builds only." }
		}
	}
	if ($AllowGaeaExport -and -not $RunDataPacker) {
		Stop-CompileInvoke 'parameter.gaea-invalid' '-AllowGaeaExport applies only to the authorized -RunDataPacker generation build.'
	}
	if ($null -ne $Files) {
		# Elements split on ',' so the single-argument -File form carries a whole list.
		$script:SelectedFiles = @($Files | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_.Length -gt 0 })
		if ($script:SelectedFiles.Count -eq 0) { Stop-CompileInvoke 'parameter.files-invalid' '-Files was supplied without any source path.' }
	}

	$contextArguments = [Collections.Generic.List[string]]::new()
	if (-not [string]::IsNullOrWhiteSpace($RepositoryRoot)) { $contextArguments.AddRange([string[]]@('-RepositoryRoot', $RepositoryRoot)) }
	if (-not [string]::IsNullOrWhiteSpace($PrimaryCheckout)) { $contextArguments.AddRange([string[]]@('-PrimaryCheckout', $PrimaryCheckout)) }
	if (-not [string]::IsNullOrWhiteSpace($Baseline)) { $contextArguments.AddRange([string[]]@('-Baseline', $Baseline)) }
	if ($RunDataPacker) { $contextArguments.Add('-IncludeDevEnvDir') }
	$contextResponse = Invoke-CompileScript 'Resolve-CompileContext.ps1' $contextArguments.ToArray()
	$context = ConvertFrom-ChildJson $contextResponse.Stdout 'Resolve-CompileContext.ps1'
	if ($contextResponse.ExitCode -ne 0) {
		Stop-CompileInvoke 'context.unresolved' "Compile context resolution failed with $($context.code): $($context.message)"
	}
	$script:Root = [string]$context.repositoryRoot
	$primary = [string]$context.primaryCheckout
	$baselineCommit = [string]$context.baseline
	$script:StatePath = Join-Path $script:Root 'Temp\compile-oracle-state.json'
	$script:ReceiptDirectory = Join-Path $script:Root 'Temp\CompileDataOracle'

	# The effective data mode decides which authorizations hold, so it resolves from the read-only
	# context alone and its guards block before provisioning or any other side effect can run.
	if ($isGameTarget) {
		$reportedMode = [string]$context.dataBuildMode
		$selectedMode = if ([string]::IsNullOrWhiteSpace($DataBuildMode)) { $reportedMode } else { $DataBuildMode }
		# The effective mode decides this, not the override alone: a derived Shared mode would otherwise
		# let generation write into the primary Shared data directory.
		if ($RunDataPacker -and $selectedMode -cne 'Local') {
			Stop-CompileInvoke 'data-mode.rundatapacker-requires-local' "-RunDataPacker requires an effective Local mode; the effective mode is $selectedMode, and generation is a Local-mode operation."
		}
		if ($selectedMode -ceq 'Shared' -and $reportedMode -ceq 'Local') {
			$triggerPaths = @($context.triggerMatches | ForEach-Object { [string]$_.path })
			$deletionOnly = @($context.deletionOnlyCandidates | ForEach-Object { [string]$_ })
			$nonDeletion = @($triggerPaths | Where-Object { $_ -cnotin $deletionOnly })
			# The exception covers source assets only. The trigger label carries the context's own
			# separator and case normalization, so it is matched instead of the raw path.
			$nonAsset = @($context.triggerMatches | Where-Object { [string]$_.trigger -cnotin @('Engine/Data/**', 'Projects/BrokenEngineSandbox/Data/**') } | ForEach-Object { [string]$_.path })
			if (-not $AcceptDeletionOnlyException) {
				Stop-CompileInvoke 'data-mode.shared-over-local' "Shared was requested over a Local path trigger ($($triggerPaths -join ', ')). Only the deletion-only exception permits this, through -AcceptDeletionOnlyException with its recorded reference-search evidence."
			}
			if ($nonDeletion.Count -gt 0) {
				Stop-CompileInvoke 'deletion-only.unsupported' "The deletion-only exception does not cover these changed paths, which are not baseline deletions: $($nonDeletion -join ', ')."
			}
			if ($nonAsset.Count -gt 0) {
				Stop-CompileInvoke 'deletion-only.unsupported' "The deletion-only exception covers source assets under Engine/Data/** and Projects/BrokenEngineSandbox/Data/** only; these changed paths are outside it: $($nonAsset -join ', ')."
			}
		}
	}

	$provisionResponse = Invoke-SharedScript 'Provision-WorktreeThirdParty.ps1' @('-RepositoryRoot', $script:Root)
	if ($provisionResponse.ExitCode -ne 0) {
		# Provisioning stdout is captured so it can never reach the build result stream; surface it here.
		if (-not [string]::IsNullOrWhiteSpace($provisionResponse.Stdout)) { Write-CompileDiagnostic $provisionResponse.Stdout.Trim() }
		Stop-CompileInvoke 'provisioning.failed' "Provision-WorktreeThirdParty.ps1 exited $($provisionResponse.ExitCode) for '$($script:Root)'."
	}

	$script:WorktreeCliPath = Join-Path $script:Root 'Tools\WorktreeCli\Platforms\VisualStudio2026\Output\WorktreeCli.exe'
	$agentHarnessPath = Join-Path $script:Root 'Tools\AgentHarness\Platforms\VisualStudio2026\Output\AgentHarness.exe'
	foreach ($tool in @($script:WorktreeCliPath, $agentHarnessPath)) {
		if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) {
			Stop-CompileInvoke 'agent-tools.missing' "AgentTools output is incomplete: '$tool' is missing. Wrappers own bootstrap; this build never creates it."
		}
	}

	$relativeTarget = if ($script:SelectedFiles.Count -gt 0) { $script:SelectiveTargetFiles[$Target] } else { $script:TargetFiles[$Target] }
	$script:TargetPath = Get-AgentCanonicalPath (Join-Path $script:Root $relativeTarget)
	if (-not (Test-Path -LiteralPath $script:TargetPath -PathType Leaf)) {
		Stop-CompileInvoke 'target.missing' "Build target does not exist: '$($script:TargetPath)'."
	}
	$resolvedFiles = [Collections.Generic.List[string]]::new()
	foreach ($file in $script:SelectedFiles) {
		$candidate = if ([IO.Path]::IsPathFullyQualified($file)) { $file } else { Join-Path $script:Root $file }
		$resolvedFiles.Add((Get-AgentCanonicalPath $candidate))
	}
	$script:SelectedFiles = $resolvedFiles.ToArray()

	if (-not $isGameTarget) {
		$script:Summary.Add("Target $Target ($($script:ResolvedConfiguration)) at $($script:TargetPath); no runtime data mode applies.")
		Complete-CompileBuild (Invoke-WorktreeCliBuild @()) @()
	}

	# Game target: the effective data mode is already resolved and guarded, so this runs the
	# mode's directories and the oracle state machine.
	if ($selectedMode -ceq $reportedMode) {
		$gameDataDirectory = [string]$context.gameDataDirectory
		$generatedDataIncludeRoot = [string]$context.generatedDataIncludeRoot
	}
	else {
		# Mode and directories always move together, so an overridden mode re-derives both.
		$owner = if ($selectedMode -ceq 'Local') { $script:Root } else { $primary }
		$gameDataDirectory = Get-AgentCanonicalPath (Join-Path $owner 'Projects\BrokenEngineSandbox\Platforms\VisualStudio2026\Output\Data')
		$generatedDataIncludeRoot = Get-AgentCanonicalPath ([IO.Path]::GetDirectoryName($gameDataDirectory))
	}
	$sharedDataDirectory = Get-AgentCanonicalPath (Join-Path $primary 'Projects\BrokenEngineSandbox\Platforms\VisualStudio2026\Output\Data')
	$script:Summary.Add("DataBuildMode=$selectedMode (path rules reported $reportedMode) GameDataDirectory=$gameDataDirectory GeneratedDataIncludeRoot=$generatedDataIncludeRoot")

	if (-not (Test-Path -LiteralPath $script:ReceiptDirectory)) { [void] (New-Item -ItemType Directory -Path $script:ReceiptDirectory -Force) }
	$script:State = Read-CompileOracleState $script:StatePath
	# Only Local mode persists and compares this identity, and hashing every changed file is not free.
	$localInputsIdentity = if ($selectedMode -ceq 'Local') { Get-LocalInputsIdentity $context $script:Root } else { $null }
	$runGeneration = [bool]$RunDataPacker
	$breachedSlots = [Collections.Generic.List[string]]::new()

	if ($selectedMode -ceq 'Shared') {
		# A worktree that ran Local holds its independent primary-Shared identity in the 'shared' slot.
		# Becoming Shared-eligible inherits that identity instead of issuing a fresh receipt over it, so a
		# drifted or breached Shared identity still clears only through -ReissueSharedReceipt.
		if (-not (Test-EntryMatches $script:State.receipts.selected 'Shared' $gameDataDirectory $baselineCommit) -and
			(Test-EntryMatches $script:State.receipts.shared 'Shared' $gameDataDirectory $baselineCommit)) {
			$script:State.receipts.selected = $script:State.receipts.shared
			$script:Summary.Add("Independent Shared receipt migrated into the selected slot: path=$($script:State.receipts.selected.receiptPath) breached=$($script:State.receipts.selected.breached)")
		}
		Resolve-SharedReceipt 'selected' $gameDataDirectory $baselineCommit
		$script:State.receipts.shared = $null
		$script:State.selectedMode = 'Shared'
		$script:State.generationCompleted = $false
		$script:State.localInputsIdentity = $null
		Save-CompileOracleState
	}
	else {
		$localEntry = $script:State.receipts.selected
		$localCurrent = Test-EntryMatches $localEntry 'Local' $gameDataDirectory $baselineCommit
		if ($localCurrent -and $localEntry.breached) {
			if (-not $runGeneration) {
				Stop-CompileInvoke 'data-oracle.breached' "The persisted Local receipt for '$gameDataDirectory' is marked breached by an earlier post-build verification. Only a newly authorized -RunDataPacker generation clears it."
			}
			$localCurrent = $false
		}
		if ($localCurrent -and $script:State.localInputsIdentity -cne $localInputsIdentity) {
			if (-not $runGeneration) {
				Stop-CompileInvoke 'local-data.stale' "The persisted Local receipt was issued for a different changed-path input state; Local data is missing or stale and Local generation authorization is required."
			}
			$localCurrent = $false
		}
		if ($runGeneration -and $localCurrent -and $script:State.generationCompleted) {
			# A repeated generation call after a host timeout must not regenerate: the single
			# authorized generation already completed for exactly these inputs.
			$runGeneration = $false
			$script:Summary.Add('RunDataPacker downgraded to false: the authorized Local generation already completed for these inputs.')
		}
		if (-not $localCurrent -and -not $runGeneration) {
			Stop-CompileInvoke 'local-data.missing' "Local data is missing or stale; Local generation authorization is required. Re-run with -RunDataPacker once an approved plan or acceptance criterion authorizes generation."
		}
		if ($localCurrent) {
			$verification = Test-OracleReceiptEntry $localEntry
			if (-not $verification.Passed) {
				# Reaching here with generation still authorized means the receipt this run is about to
				# replace is the one failing, so blocking would make the documented retry of the identical
				# command unable to run; the regeneration below reissues it.
				if (-not $runGeneration) {
					Stop-CompileInvoke 'local-data.verify-failed' "The persisted Local receipt no longer matches '$gameDataDirectory': $($verification.Message)"
				}
				Write-CompileDiagnostic "The persisted Local receipt no longer matches '$gameDataDirectory': $($verification.Message). Proceeding: -RunDataPacker authorizes the regeneration that replaces it."
			}
		}
		# A Local build always carries an independent Shared identity for primary data. A worktree that
		# ran Shared holds that identity in the 'selected' slot, which now has to carry the Local one, so
		# it moves into the independent slot instead of a fresh receipt being issued over it: drift or a
		# breach still clears only through -ReissueSharedReceipt. Its receipt file moves with it, because
		# the authorized generation below rewrites 'selected.json' for the Local receipt.
		if (-not (Test-EntryMatches $script:State.receipts.shared 'Shared' $sharedDataDirectory $baselineCommit) -and
			(Test-EntryMatches $script:State.receipts.selected 'Shared' $sharedDataDirectory $baselineCommit)) {
			$migratedShared = $script:State.receipts.selected
			$migratedPath = Join-Path $script:ReceiptDirectory 'shared.json'
			# A receipt file already gone keeps its recorded path, so the verification below fails closed.
			if (-not $migratedShared.receiptPath.Equals($migratedPath, [StringComparison]::OrdinalIgnoreCase) -and
				(Test-Path -LiteralPath $migratedShared.receiptPath -PathType Leaf)) {
				Move-Item -LiteralPath $migratedShared.receiptPath -Destination $migratedPath -Force
				$migratedShared.receiptPath = $migratedPath
			}
			$script:State.receipts.shared = $migratedShared
			$script:State.receipts.selected = $null
			$script:Summary.Add("Persisted Shared receipt migrated into the independent slot: path=$($migratedShared.receiptPath) breached=$($migratedShared.breached)")
		}
		Resolve-SharedReceipt 'shared' $sharedDataDirectory $baselineCommit
		$script:State.selectedMode = 'Local'
		Save-CompileOracleState
	}

	$dataProperties = @(
		"/p:DataBuildMode=$selectedMode",
		"/p:RunDataPacker=$(if ($runGeneration) { 'true' } else { 'false' })",
		"/p:GameDataDirectory=$gameDataDirectory",
		"/p:GeneratedDataIncludeRoot=$generatedDataIncludeRoot"
	)
	$script:Summary.Add("RunDataPacker=$(if ($runGeneration) { 'true' } else { 'false' })")

	if (-not $runGeneration) {
		$buildExitCode = Invoke-WorktreeCliBuild $dataProperties
		foreach ($slot in @('selected', 'shared')) {
			$entry = $script:State.receipts[$slot]
			if ($null -eq $entry) { continue }
			$verification = Test-OracleReceiptEntry $entry
			if (-not $verification.Passed) {
				Write-CompileDiagnostic "post-build oracle verification failed for '$slot': $($verification.Message)"
				$breachedSlots.Add($slot)
			}
		}
		Complete-CompileBuild $buildExitCode $breachedSlots.ToArray()
	}

	# Authorized Local generation: materialize, issue the pre-generation receipt, then run the one
	# RunDataPacker=true build under the Gaea guard.
	if (-not (Test-JsonProperty $context 'devEnvDir') -or [string]::IsNullOrWhiteSpace($context.devEnvDir)) {
		Stop-CompileInvoke 'dev-env-dir.unresolved' 'The generation build requires DevEnvDir, which the compile context did not report.'
	}
	$dataPacker = Join-Path $script:Root 'DataPacker\Platforms\VisualStudio2026\Output\DataPacker.exe'
	if (-not (Test-Path -LiteralPath $dataPacker -PathType Leaf)) {
		Stop-CompileInvoke 'datapacker.missing' "The worktree DataPacker is missing: '$dataPacker'. Build -Target DataPacker first."
	}
	# DataPacker writes progress and mutex-wait notices to stdout, which is reserved for the single
	# JSON envelope, so its output is captured and forwarded as a diagnostic instead.
	$materialize = Start-CompileChild $dataPacker @(
		'--materialize-data',
		(Join-Path $script:Root 'Engine\Data'),
		(Join-Path $script:Root 'Projects\BrokenEngineSandbox\Data'),
		$gameDataDirectory) $true
	if (-not [string]::IsNullOrWhiteSpace($materialize.Stdout)) { Write-CompileDiagnostic $materialize.Stdout.Trim() }
	if ($materialize.ExitCode -ne 0) {
		Stop-CompileInvoke 'data-materialize.failed' "DataPacker --materialize-data exited $($materialize.ExitCode) for '$gameDataDirectory'."
	}
	$script:State.receipts.selected = New-OracleReceiptEntry $gameDataDirectory 'Local' $baselineCommit (Join-Path $script:ReceiptDirectory 'selected.json')
	$script:State.generationCompleted = $false
	$script:State.localInputsIdentity = $localInputsIdentity
	Save-CompileOracleState

	$generationProperties = @($dataProperties) + "/p:DevEnvDir=$($context.devEnvDir)"
	$hadGaeaGuard = Test-Path "Env:$($script:GaeaGuardVariable)"
	$previousGaeaGuard = [Environment]::GetEnvironmentVariable($script:GaeaGuardVariable)
	try {
		if (-not $AllowGaeaExport) {
			[Environment]::SetEnvironmentVariable($script:GaeaGuardVariable, '1')
			$script:Summary.Add("Gaea guard applied: $($script:GaeaGuardVariable)=1 for the generation build.")
		}
		else { $script:Summary.Add('Gaea guard omitted under explicit -AllowGaeaExport authorization.') }
		$buildExitCode = Invoke-WorktreeCliBuild $generationProperties
	}
	finally {
		if (-not $AllowGaeaExport) {
			if ($hadGaeaGuard) { [Environment]::SetEnvironmentVariable($script:GaeaGuardVariable, $previousGaeaGuard) }
			else { [Environment]::SetEnvironmentVariable($script:GaeaGuardVariable, $null) }
		}
	}

	if ($buildExitCode -eq 0) {
		# Generation rewrote Local Data, so the post-generation identity replaces the pre-generation one.
		# The explicit -RunDataPacker authorizes the resulting content delta, but the delta itself must
		# stay visible: the pre-generation aggregate and whether it moved are kept beside the new entry.
		$preGenerationAggregate = $script:State.receipts.selected.aggregateDigest
		$newEntry = New-OracleReceiptEntry $gameDataDirectory 'Local' $baselineCommit (Join-Path $script:ReceiptDirectory 'selected.json')
		$newEntry.preGenerationAggregateDigest = $preGenerationAggregate
		$newEntry.generationDeltaChanged = $newEntry.aggregateDigest -cne $preGenerationAggregate
		$script:Summary.Add("Generation content delta: pre=$preGenerationAggregate post=$($newEntry.aggregateDigest) changed=$($newEntry.generationDeltaChanged) (authorized by -RunDataPacker)")
		$script:State.receipts.selected = $newEntry
		$script:State.generationCompleted = $true
		Save-CompileOracleState
	}
	foreach ($slot in @('selected', 'shared')) {
		$entry = $script:State.receipts[$slot]
		if ($null -eq $entry) { continue }
		$verification = Test-OracleReceiptEntry $entry
		if (-not $verification.Passed) {
			Write-CompileDiagnostic "post-build oracle verification failed for '$slot': $($verification.Message)"
			$breachedSlots.Add($slot)
		}
	}
	Complete-CompileBuild $buildExitCode $breachedSlots.ToArray()
}
catch {
	foreach ($line in $script:Summary) { Write-CompileDiagnostic $line }
	$code = if ($null -ne $script:BlockedCode) { $script:BlockedCode } else { 'internal.error' }
	if ($script:BuildStreamed) {
		# The build envelope already owns stdout, so this failure is stderr only and reuses the
		# post-build exit semantics: the build's own code when it failed, otherwise the reserved 4.
		Write-CompileDiagnostic "post-build failure $code - $($_.Exception.Message)"
		if ($script:BuildExitCode -ne 0) { exit $script:BuildExitCode }
		exit 4
	}
	if ($null -ne $script:BlockedCode) { Complete-CompileInvoke 2 'blocked' $code $_.Exception.Message }
	Complete-CompileInvoke 1 'error' $code $_.Exception.Message
}
