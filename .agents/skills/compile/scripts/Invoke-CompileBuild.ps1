# The single documented /compile build entry point. Every invocation is independently valid in a
# fresh shell: identities, data mode, directories, and tool paths are re-derived per call, and no
# state persists between calls.
#
# Stdout contract: when WorktreeCli runs, its `broken-engine-build-result/v1` object is the only
# thing on stdout - this script never captures, rewrites, or impersonates it, and its exit code is
# returned unchanged. When the build never runs, stdout carries exactly one
# `broken-engine-compile-invoke-result/v1` object instead; the schema name, not the exit code, is
# what distinguishes the two.
# Exit codes: WorktreeCli's own code once the build ran, 2 for a structured pre-invocation block,
# 1 for an internal error.
[CmdletBinding()]
param(
	[Parameter(Mandatory)][ValidateSet('ThirdParty', 'DataPacker', 'Client', 'Server', 'WorktreeCli', 'AgentHarness')][string] $Target,
	[ValidateSet('Debug', 'Profile', 'Release')][string] $Configuration,
	[string[]] $Files,
	[ValidateSet('Shared', 'Local')][string] $DataBuildMode,
	[switch] $RunDataPacker,
	[switch] $AllowGaeaExport,
	[switch] $ForbidExpensiveExport,
	[switch] $AcceptDeletionOnlyException,
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
$script:GaeaGuardVariable = 'BT_DATAPACKER_FORBID_GAEA_EXPORT'
$script:ExpensiveExportGuardVariable = 'BT_DATAPACKER_FORBID_EXPENSIVE_EXPORT'
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
	if ($Prefast -and $script:SelectedFiles.Count -eq 0) {
		# RunNativeCodeAnalysis is incremental, so an up-to-date full build would report green without analyzing.
		# The --files path reuses this argument list for its MSBuild evaluation query, so it stays incremental.
		$arguments.Add('/t:Rebuild')
	}
	$script:Summary.Add("WorktreeCli build arguments: $($arguments -join ' ')")
	# Stdout is not redirected: WorktreeCli's result envelope is the caller's stdout, byte-verbatim.
	$script:BuildExitCode = (Start-CompileChild $script:WorktreeCliPath $arguments.ToArray() $false).ExitCode
	return $script:BuildExitCode
}

function Complete-CompileBuild([int] $BuildExitCode) {
	foreach ($line in $script:Summary) { Write-CompileDiagnostic $line }
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
			@{ Set = [bool]$ForbidExpensiveExport; Name = '-ForbidExpensiveExport' },
			@{ Set = [bool]$AcceptDeletionOnlyException; Name = '-AcceptDeletionOnlyException' })) {
			if ($pair.Set) { Stop-CompileInvoke 'parameter.game-only' "$($pair.Name) applies to Client or Server builds only." }
		}
	}
	if ($AllowGaeaExport -and -not $RunDataPacker) {
		Stop-CompileInvoke 'parameter.gaea-invalid' '-AllowGaeaExport applies only to the authorized -RunDataPacker generation build.'
	}
	if ($ForbidExpensiveExport -and -not $RunDataPacker) {
		Stop-CompileInvoke 'parameter.expensive-export-invalid' '-ForbidExpensiveExport applies only to the authorized -RunDataPacker generation build.'
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

	if ($Target -ceq 'ThirdParty') {
		# Only the primary checkout owns a real ThirdParty Output; in a linked worktree it is a link to the
		# primary's shared library, so a build here would overwrite it. The primary is identified by its .git
		# being an ordinary directory, as Bootstrap-AgentTools.ps1 and Provision-WorktreeThirdParty.ps1 do.
		$rootGit = Get-Item -LiteralPath (Join-Path $script:Root '.git') -Force -ErrorAction SilentlyContinue
		if ($null -eq $rootGit -or -not $rootGit.PSIsContainer -or ($rootGit.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
			Stop-CompileInvoke 'thirdparty.worktree-not-primary' "ThirdParty builds run in the primary checkout only ('$primary'); this checkout's ThirdParty Output is a link to the primary's shared library. Rebuild ThirdParty as explicit primary maintenance (wrapper bootstrap rebuilds it at session start)."
		}
	}

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
		Complete-CompileBuild (Invoke-WorktreeCliBuild @())
	}

	# Game target: the effective data mode is already resolved and guarded, so this runs the
	# mode's directories.
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
	$script:Summary.Add("DataBuildMode=$selectedMode (path rules reported $reportedMode) GameDataDirectory=$gameDataDirectory GeneratedDataIncludeRoot=$generatedDataIncludeRoot")

	$runGeneration = [bool]$RunDataPacker

	$dataProperties = @(
		"/p:DataBuildMode=$selectedMode",
		"/p:RunDataPacker=$(if ($runGeneration) { 'true' } else { 'false' })",
		"/p:GameDataDirectory=$gameDataDirectory",
		"/p:GeneratedDataIncludeRoot=$generatedDataIncludeRoot"
	)
	$script:Summary.Add("RunDataPacker=$(if ($runGeneration) { 'true' } else { 'false' })")

	if (-not $runGeneration) {
		Complete-CompileBuild (Invoke-WorktreeCliBuild $dataProperties)
	}

	# Authorized Local generation: materialize, then run the one RunDataPacker=true build under the
	# Gaea guard and, when requested, the optional expensive-export guard.
	if (-not (Test-JsonProperty $context 'devEnvDir') -or [string]::IsNullOrWhiteSpace($context.devEnvDir)) {
		Stop-CompileInvoke 'dev-env-dir.unresolved' 'The generation build requires DevEnvDir, which the compile context did not report.'
	}
	# Session worktrees are created with the two large Engine/Data art subtrees skipped by sparse
	# checkout, and this is the one build that reads them as DataPacker inputs, so restore the full
	# working tree first. Disabling sparse checkout is idempotent and keeps uncommitted edits.
	# The sparse state itself is the predicate: files created under a skipped tree recreate its
	# directory without restoring the tracked bytes, so presence would report a false restore.
	$sparseState = Start-CompileChild 'git' @('-C', $script:Root, 'config', '--get', 'core.sparseCheckout') $true
	if ($sparseState.Stdout.Trim() -ieq 'true') {
		$restore = Start-CompileChild 'git' @('-C', $script:Root, 'sparse-checkout', 'disable') $true
		if (-not [string]::IsNullOrWhiteSpace($restore.Stdout)) { Write-CompileDiagnostic $restore.Stdout.Trim() }
		if ($restore.ExitCode -ne 0) {
			Stop-CompileInvoke 'data.sparse-restore-failed' "git sparse-checkout disable exited $($restore.ExitCode) in '$($script:Root)', so the generation build may still be missing sparse-skipped Engine\Data inputs."
		}
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

	$generationProperties = @($dataProperties) + "/p:DevEnvDir=$($context.devEnvDir)"
	$hadGaeaGuard = Test-Path "Env:$($script:GaeaGuardVariable)"
	$previousGaeaGuard = [Environment]::GetEnvironmentVariable($script:GaeaGuardVariable)
	if ($ForbidExpensiveExport) {
		$hadExpensiveExportGuard = Test-Path "Env:$($script:ExpensiveExportGuardVariable)"
		$previousExpensiveExportGuard = [Environment]::GetEnvironmentVariable($script:ExpensiveExportGuardVariable)
	}
	try {
		if (-not $AllowGaeaExport) {
			[Environment]::SetEnvironmentVariable($script:GaeaGuardVariable, '1')
			$script:Summary.Add("Gaea guard applied: $($script:GaeaGuardVariable)=1 for the generation build.")
		}
		else { $script:Summary.Add('Gaea guard omitted under explicit -AllowGaeaExport authorization.') }
		if ($ForbidExpensiveExport) {
			[Environment]::SetEnvironmentVariable($script:ExpensiveExportGuardVariable, '1')
			$script:Summary.Add("Expensive-export guard applied: $($script:ExpensiveExportGuardVariable)=1 for the generation build.")
		}
		$buildExitCode = Invoke-WorktreeCliBuild $generationProperties
	}
	finally {
		if (-not $AllowGaeaExport) {
			if ($hadGaeaGuard) { [Environment]::SetEnvironmentVariable($script:GaeaGuardVariable, $previousGaeaGuard) }
			else { [Environment]::SetEnvironmentVariable($script:GaeaGuardVariable, $null) }
		}
		if ($ForbidExpensiveExport) {
			if ($hadExpensiveExportGuard) { [Environment]::SetEnvironmentVariable($script:ExpensiveExportGuardVariable, $previousExpensiveExportGuard) }
			else { [Environment]::SetEnvironmentVariable($script:ExpensiveExportGuardVariable, $null) }
		}
	}

	Complete-CompileBuild $buildExitCode
}
catch {
	foreach ($line in $script:Summary) { Write-CompileDiagnostic $line }
	$code = if ($null -ne $script:BlockedCode) { $script:BlockedCode } else { 'internal.error' }
	if ($script:BuildStreamed) {
		# The build envelope already owns stdout, so this failure is stderr only: the build's own code
		# when it failed, otherwise the internal-error code.
		Write-CompileDiagnostic "post-build failure $code - $($_.Exception.Message)"
		if ($script:BuildExitCode -ne 0) { exit $script:BuildExitCode }
		exit 1
	}
	if ($null -ne $script:BlockedCode) { Complete-CompileInvoke 2 'blocked' $code $_.Exception.Message }
	Complete-CompileInvoke 1 'error' $code $_.Exception.Message
}
