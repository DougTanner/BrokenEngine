# The single documented /compile build entry point. Every invocation is independently valid in a
# fresh shell: identities, data mode, directories, and tool paths are re-derived per call, and no
# state persists between calls.
#
# Stdout contract: each WorktreeCli build writes one newline-terminated
# `broken-engine-build-result/v1` object, and those objects are the only thing on stdout - one per
# build run, two for ClientServer with Client first. This script never captures, rewrites, or
# impersonates them; a linked worktree's Local game build first runs a DataPacker Release build
# whose envelope goes to stderr, or to stdout in place of the game builds' when it fails. When no
# build runs, stdout carries exactly one
# `broken-engine-compile-invoke-result/v1` object instead; the schema name, not the exit code, is
# what distinguishes the two.
# Exit codes: once a build ran, the first nonzero WorktreeCli build code, else 1 after a post-build
# failure, else 0; 2 for a structured pre-invocation block, 1 for an internal error.
[CmdletBinding()]
param(
	[Parameter(Mandatory)][ValidateSet('ThirdParty', 'DataPacker', 'Client', 'Server', 'ClientServer', 'ModelViewer', 'ServerMonitor', 'WorktreeCli', 'AgentHarness')][string] $Target,
	[ValidateSet('Debug', 'Profile', 'Release')][string] $Configuration,
	[string[]] $Files,
	[switch] $AllowGaeaExport,
	[switch] $ForbidExpensiveExport,
	[switch] $Prefast,
	[switch] $ClangTidy,
	[string] $Project,
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
foreach ($parameterName in @('Target', 'Configuration')) {
	$boundValue = [string](Get-Variable -Name $parameterName -ValueOnly)
	if ([string]::IsNullOrWhiteSpace($boundValue)) { continue }
	$declared = @($PSCmdlet.MyInvocation.MyCommand.Parameters[$parameterName].Attributes |
		Where-Object { $_ -is [Management.Automation.ValidateSetAttribute] } |
		ForEach-Object { $_.ValidValues })
	Set-Variable -Name $parameterName -Value @($declared | Where-Object { $_ -eq $boundValue })[0]
}

$script:GameTargets = @('Client', 'Server', 'ClientServer', 'ModelViewer')
# ClientServer builds a game's Client then Server on one resolved context, so both carry one data mode.
$script:BuildTargets = if ($Target -ceq 'ClientServer') { @('Client', 'Server') } else { @($Target) }
$script:ReleaseOnlyTargets = @('DataPacker', 'WorktreeCli', 'AgentHarness', 'ServerMonitor')
$script:AgentToolsTargets = @('WorktreeCli', 'AgentHarness')
$script:GaeaGuardVariable = 'BT_DATAPACKER_FORBID_GAEA_EXPORT'
$script:GaeaAllowVariable = 'BT_DATAPACKER_ALLOW_GAEA_EXPORT'
$script:ExpensiveExportGuardVariable = 'BT_DATAPACKER_FORBID_EXPENSIVE_EXPORT'
# The Client and Server entries, the selective targets, and the data output suffix derive from the
# resolved game project.
$script:TargetFiles = @{
	ThirdParty = 'ThirdParty\Prebuilts\Platforms\VisualStudio2026\ThirdParty.sln'
	DataPacker = 'DataPacker\Platforms\VisualStudio2026\DataPacker.sln'
	WorktreeCli = 'Tools\WorktreeCli\Platforms\VisualStudio2026\WorktreeCli.sln'
	AgentHarness = 'Tools\AgentHarness\Platforms\VisualStudio2026\AgentHarness.sln'
	ServerMonitor = 'Tools\ServerMonitor\Platforms\VisualStudio2026\ServerMonitor.sln'
	ModelViewer = 'Projects\ModelViewer\Platforms\VisualStudio2026\ModelViewer.sln'
}
# --files requires a .vcxproj target, so a selective compile builds the project, not the solution.
$script:SelectiveTargetFiles = @{ ModelViewer = 'Projects\ModelViewer\Platforms\VisualStudio2026\ModelViewer.vcxproj' }
$script:DataOutputSuffix = $null

$script:BlockedCode = $null
# Set the moment WorktreeCli's inherited stdout can carry its envelope: after that, this script
# itself may write no stdout object, so every later failure is stderr plus an exit code.
$script:BuildStreamed = $false
# The first nonzero build exit code, kept when a later build of the same call succeeds.
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

# --- Build invocation --------------------------------------------------------------------------

function Get-AnalysisArguments([bool] $Requested) {
	if ($Requested -and $Prefast) {
		# EnableMicrosoftCodeAnalysis=true only guards against an upstream default change.
		return @('/p:EnableClangTidyCodeAnalysis=false', '/p:EnableMicrosoftCodeAnalysis=true', '/p:RunCodeAnalysis=true', '/verbosity:minimal')
	}
	if ($Requested -and $ClangTidy) {
		# EnableMicrosoftCodeAnalysis=false keeps the rule-set path off under RunCodeAnalysis=true. Normal verbosity
		# names the ClangTidy target and its invocation per project, the only execution trace a --quiet clean run leaves.
		return @('/p:EnableClangTidyCodeAnalysis=true', '/p:EnableMicrosoftCodeAnalysis=false', '/p:RunCodeAnalysis=true', '/verbosity:normal')
	}
	return @('/p:EnableClangTidyCodeAnalysis=false', '/p:RunCodeAnalysis=false', '/verbosity:minimal')
}

# -LocalDataPacker builds plain DataPacker Release instead of the requested target and returns its
# captured envelope, so the requested builds' envelopes stay the only ones on stdout.
function Invoke-WorktreeCliBuild([string] $TargetPath, [string[]] $DataProperties, [switch] $LocalDataPacker) {
	$arguments = [Collections.Generic.List[string]]::new()
	$arguments.Add('build')
	if ($script:SelectedFiles.Count -gt 0 -and -not $LocalDataPacker) {
		$arguments.Add('--files')
		foreach ($file in $script:SelectedFiles) { $arguments.Add($file) }
		$arguments.Add('--')
	}
	if ($LocalDataPacker) {
		$arguments.Add((Join-Path $script:Root $script:TargetFiles.DataPacker))
		$arguments.Add('/p:Configuration=Release')
	}
	else {
		$arguments.Add($TargetPath)
		$arguments.Add("/p:Configuration=$($script:ResolvedConfiguration)")
	}
	$arguments.Add('/p:Platform=x64')
	if ($Target -cin $script:AgentToolsTargets) {
		# The default Output is the shared immutable primary Output the running driver holds.
		$arguments.Add("/p:OutDir=$($script:Root)\Temp\AgentToolsCandidate\")
	}
	if ($Target -ceq 'ThirdParty' -and $script:IsLinkedWorktree) {
		# A linked worktree's ThirdParty Output is a link to the primary's shared library, so its build
		# writes a session candidate instead.
		$arguments.Add("/p:OutDir=$($script:ThirdPartyCandidateDirectory)\")
	}
	foreach ($property in $DataProperties) { $arguments.Add($property) }
	foreach ($argument in (Get-AnalysisArguments (-not $LocalDataPacker))) { $arguments.Add($argument) }
	if (($Prefast -or $ClangTidy) -and $script:SelectedFiles.Count -eq 0 -and -not $LocalDataPacker) {
		# RunNativeCodeAnalysis and ClangTidy are incremental, so an up-to-date full build would report green without analyzing.
		# The --files path reuses this argument list for its MSBuild evaluation query, so it stays incremental.
		$arguments.Add('/t:Rebuild')
	}
	$script:Summary.Add("WorktreeCli build arguments: $($arguments -join ' ')")
	# A private PDB server endpoint keeps this build's cl/link off the shared per-user mspdbsrv, which runs
	# inside the kill-on-close job of whichever build started it and dies with that job, failing concurrent builds.
	[Environment]::SetEnvironmentVariable('_MSPDBSRV_ENDPOINT_', [guid]::NewGuid().ToString('N'))
	if ($LocalDataPacker) { return Start-CompileChild $script:WorktreeCliPath $arguments.ToArray() $true }
	# Stdout is not redirected: WorktreeCli's result envelope is the caller's stdout, byte-verbatim.
	$exitCode = (Start-CompileChild $script:WorktreeCliPath $arguments.ToArray() $false).ExitCode
	if ($script:BuildExitCode -eq 0) { $script:BuildExitCode = $exitCode }
	return $exitCode
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
	if ($Target -ceq 'ModelViewer' -and $script:ResolvedConfiguration -ceq 'Profile') {
		Stop-CompileInvoke 'parameter.configuration-invalid' 'ModelViewer supports Debug or Release only.'
	}
	if ($Target -ceq 'ModelViewer' -and -not [string]::IsNullOrWhiteSpace($Project) -and $Project -cne 'BrokenEngineSandbox') {
		Stop-CompileInvoke 'parameter.project-invalid' 'ModelViewer uses BrokenEngineSandbox data; -Project cannot select another data owner.'
	}
	if ($Prefast -and ($Target -cnotin @('Client', 'Server') -or $script:ResolvedConfiguration -cne 'Release')) {
		Stop-CompileInvoke 'parameter.prefast-invalid' 'PREfast verification applies to Client or Server Release builds only.'
	}
	# Profile is blocked because the global property would override the client Profile's clang-tidy opt-out.
	if ($ClangTidy -and ($Prefast -or $script:ResolvedConfiguration -ceq 'Profile' -or $Target -cnotin @('Client', 'Server', 'DataPacker'))) {
		Stop-CompileInvoke 'parameter.clangtidy-invalid' 'Clang-Tidy verification applies to Client or Server Debug or Release builds and DataPacker builds only, and never with -Prefast.'
	}
	if (-not $isGameTarget) {
		foreach ($pair in @(
			@{ Set = $null -ne $Files -and $Files.Count -gt 0; Name = '-Files' },
			@{ Set = [bool]$AllowGaeaExport; Name = '-AllowGaeaExport' },
			@{ Set = [bool]$ForbidExpensiveExport; Name = '-ForbidExpensiveExport' },
			@{ Set = -not [string]::IsNullOrWhiteSpace($Project); Name = '-Project' })) {
			if ($pair.Set) { Stop-CompileInvoke 'parameter.game-only' "$($pair.Name) applies to Client, Server, ClientServer or ModelViewer builds only." }
		}
	}
	if ($null -ne $Files) {
		if ($Target -ceq 'ClientServer') { Stop-CompileInvoke 'parameter.files-invalid' '-Files selects one project; ClientServer builds the full Client and Server pair only.' }
		# Elements split on ',' so the single-argument -File form carries a whole list.
		$script:SelectedFiles = @($Files | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_.Length -gt 0 })
		if ($script:SelectedFiles.Count -eq 0) { Stop-CompileInvoke 'parameter.files-invalid' '-Files was supplied without any source path.' }
	}

	$contextArguments = [Collections.Generic.List[string]]::new()
	if (-not [string]::IsNullOrWhiteSpace($RepositoryRoot)) { $contextArguments.AddRange([string[]]@('-RepositoryRoot', $RepositoryRoot)) }
	if (-not [string]::IsNullOrWhiteSpace($PrimaryCheckout)) { $contextArguments.AddRange([string[]]@('-PrimaryCheckout', $PrimaryCheckout)) }
	if (-not [string]::IsNullOrWhiteSpace($Baseline)) { $contextArguments.AddRange([string[]]@('-Baseline', $Baseline)) }
	if (-not [string]::IsNullOrWhiteSpace($Project)) { $contextArguments.AddRange([string[]]@('-Project', $Project)) }
	$contextArguments.AddRange([string[]]@('-Target', $Target))
	$contextResponse = Invoke-CompileScript 'Resolve-CompileContext.ps1' $contextArguments.ToArray()
	$context = ConvertFrom-ChildJson $contextResponse.Stdout 'Resolve-CompileContext.ps1'
	if ($contextResponse.ExitCode -ne 0) {
		if ($contextResponse.ExitCode -eq 2 -and $context.PSObject.Properties['code'] -and [string]$context.code -ceq 'shared-data.refresh-timeout') {
			Stop-CompileInvoke 'shared-data.refresh-timeout' ([string]$context.message)
		}
		Stop-CompileInvoke 'context.unresolved' "Compile context resolution failed with $($context.code): $($context.message)"
	}
	$script:Root = [string]$context.repositoryRoot
	$primary = [string]$context.primaryCheckout

	$script:IsLinkedWorktree = [bool]$context.isLinkedWorktree
	$script:ThirdPartyCandidateDirectory = "$($script:Root)\Temp\ThirdPartyCandidate"
	# Resolve-CompileContext.ps1 emits the ThirdParty row only when this linked worktree's consumers
	# link the session candidate.
	if ($Target -cin @('Client', 'Server', 'ClientServer', 'ModelViewer', 'DataPacker') -and @($context.recommendedTargets | Where-Object { $_.project -ceq 'ThirdParty' }).Count -gt 0) {
		# An environment variable, not a /p: switch, so the -LocalDataPacker build, which passes no
		# data properties, inherits and links the same candidate.
		[Environment]::SetEnvironmentVariable('ThirdPartyLibraryDirectory', $script:ThirdPartyCandidateDirectory)
		$script:Summary.Add("ThirdPartyLibraryDirectory=$($script:ThirdPartyCandidateDirectory) (thirdparty-candidate row).")
	}

	# The data mode resolves from the read-only context alone, before provisioning or any other side
	# effect can run.
	if ($isGameTarget) {
		# One run builds one game; with several game projects the caller names one per run.
		if ($null -eq $context.project) {
			$discovered = @($context.projects | ForEach-Object { [string]$_ })
			Stop-CompileInvoke 'project.unresolved' "No game project resolved for $Target; discovered: $(if ($discovered.Count -eq 0) { 'none' } else { $discovered -join ', ' }). Pass -Project <Name> to build one game per run."
		}
		$gameProject = [string]$context.project
		$projectDirectory = "Projects\$gameProject\Platforms\VisualStudio2026"
		$script:TargetFiles.Client = "$projectDirectory\$gameProject.sln"
		$script:TargetFiles.Server = "$projectDirectory\$($gameProject)Server.sln"
		$script:SelectiveTargetFiles.Client = "$projectDirectory\$gameProject.vcxproj"
		$script:SelectiveTargetFiles.Server = "$projectDirectory\$($gameProject)Server.vcxproj"
		$script:DataOutputSuffix = "$projectDirectory\Output\Data"
		$dataBuildMode = [string]$context.dataBuildMode
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

	$targetPaths = @{}
	foreach ($buildTarget in $script:BuildTargets) {
		$relativeTarget = if ($script:SelectedFiles.Count -gt 0) { $script:SelectiveTargetFiles[$buildTarget] } else { $script:TargetFiles[$buildTarget] }
		$targetPaths[$buildTarget] = Get-AgentCanonicalPath (Join-Path $script:Root $relativeTarget)
		if (-not (Test-Path -LiteralPath $targetPaths[$buildTarget] -PathType Leaf)) {
			Stop-CompileInvoke 'target.missing' "Build target does not exist: '$($targetPaths[$buildTarget])'."
		}
	}
	$resolvedFiles = [Collections.Generic.List[string]]::new()
	foreach ($file in $script:SelectedFiles) {
		$candidate = if ([IO.Path]::IsPathFullyQualified($file)) { $file } else { Join-Path $script:Root $file }
		$resolvedFiles.Add((Get-AgentCanonicalPath $candidate))
	}
	$script:SelectedFiles = $resolvedFiles.ToArray()

	# An agent host keeps the VK_SDK_PATH it inherited at start, while wrapper bootstrap rebuilds the
	# shared ThirdParty prebuilt in its own wrapper session's environment, which is current only when
	# that terminal started after the last Vulkan SDK change, so the two can compile against different
	# Vulkan headers. The expected value takes the user level over the machine level, as Windows does
	# when it builds a new process environment.
	if ($Target -cnotin $script:AgentToolsTargets) {
		$expectedSdk = [Environment]::GetEnvironmentVariable('VK_SDK_PATH', 'User')
		if ([string]::IsNullOrEmpty($expectedSdk)) { $expectedSdk = [Environment]::GetEnvironmentVariable('VK_SDK_PATH', 'Machine') }
		if (-not [string]::IsNullOrEmpty($expectedSdk)) {
			$processSdk = [string][Environment]::GetEnvironmentVariable('VK_SDK_PATH')
			if (-not [string]::Equals($processSdk.TrimEnd('\', '/'), $expectedSdk.TrimEnd('\', '/'), [StringComparison]::OrdinalIgnoreCase)) {
				Stop-CompileInvoke 'vulkan-sdk.stale-environment' "This process has VK_SDK_PATH '$processSdk', but the current user/machine environment sets '$expectedSdk': the agent host started before a Vulkan SDK change, so this build would compile against different Vulkan headers than the shared ThirdParty prebuilt. Restart the agent host from a fresh environment (a new wrapper session started from a new Windows Terminal tab, or, if this block persists, after fully restarting the terminal application, for example VS Code), then rerun /compile."
			}
		}
	}

	if (-not $isGameTarget) {
		$script:Summary.Add("Target $Target ($($script:ResolvedConfiguration)) at $($targetPaths[$Target]); no runtime data mode applies.")
		Complete-CompileBuild (Invoke-WorktreeCliBuild $targetPaths[$Target] @())
	}

	# Game target: the data mode is already resolved, so this runs the mode's directories.
	$gameDataDirectory = [string]$context.gameDataDirectory
	$generatedDataIncludeRoot = [string]$context.generatedDataIncludeRoot
	$modeReason = [string]$context.dataBuildModeReason
	if ($modeReason -ceq 'inputs-differ') {
		$modeReason += ": $($context.dataInputChangeCount) data input(s) differ from Shared data stamp $($context.sharedDataStamp), reported: $(@($context.dataInputChanges | ForEach-Object { [string]$_ }) -join ', ')"
	}
	$script:Summary.Add("DataBuildMode=$dataBuildMode ($modeReason) GameDataDirectory=$gameDataDirectory GeneratedDataIncludeRoot=$generatedDataIncludeRoot")

	$runGeneration = $dataBuildMode -ceq 'Local'

	$dataProperties = @(
		"/p:DataBuildMode=$dataBuildMode",
		"/p:RunDataPacker=$(if ($runGeneration) { 'true' } else { 'false' })",
		"/p:GameDataDirectory=$gameDataDirectory",
		"/p:GeneratedDataIncludeRoot=$generatedDataIncludeRoot"
	)
	$script:Summary.Add("RunDataPacker=$(if ($runGeneration) { 'true' } else { 'false' })")
	if ($Target -ceq 'ModelViewer') {
		# Visual Studio imports the same resolved data properties as this build; its project generates
		# the default-root header from those properties before compiling the viewer host.
		Write-AgentModelViewerResolvedData $script:Root $dataBuildMode $(if ($runGeneration) { 'true' } else { 'false' }) $gameDataDirectory $generatedDataIncludeRoot
	}

	if (-not $runGeneration) {
		# The harness claim reads its data directory from this record, and the claim and launch compare the
		# Shared data directory against its listing. Taking the stamp check and listing under the bootstrap
		# mutex excludes any in-flight refresh, so an absent stamp or one rewritten with a different commit
		# since mode selection blocks before enumeration. The listing is taken before MSBuild runs, so a
		# refresh during the build differs at the next comparison, and a failed build keeps the previous record.
		[string[]] $sharedDataListing = @()
		if ($dataBuildMode -ceq 'Shared' -and $script:SelectedFiles.Count -eq 0 -and $Target -cne 'ModelViewer') {
			try {
				$sharedDataListing = [string[]]@(Invoke-AgentBootstrapLock -PrimaryCheckout $primary -Action {
					$stamp = $null
					try { $stamp = [IO.File]::ReadAllText((Get-AgentSharedDataStampPath $primary $gameProject)).Trim() }
					catch [IO.FileNotFoundException], [IO.DirectoryNotFoundException] { }
					if ($stamp -cne [string]$context.sharedDataStamp) {
						Stop-CompileInvoke 'shared-data.refresh-overlap' "The primary Shared data stamp was left unwritten or rewritten with a different commit since data mode selection, so the Shared data listing may not match the published data. Rerun /compile once the primary SharedDataStamp.txt exists again; a rerun while it is missing resolves Local and runs a full Local generation build."
					}
					return [string[]]@(Get-AgentDataListing $gameDataDirectory)
				})
			}
			catch [TimeoutException] {
				Stop-CompileInvoke 'shared-data.refresh-timeout' 'Timed out waiting for the AgentTools bootstrap mutex before listing Shared data.'
			}
		}
		foreach ($buildTarget in $script:BuildTargets) {
			$buildExitCode = Invoke-WorktreeCliBuild $targetPaths[$buildTarget] $dataProperties
			if ($buildExitCode -eq 0 -and $script:SelectedFiles.Count -eq 0 -and $Target -cne 'ModelViewer') {
				Write-AgentSharedDataRecord $script:Root $gameProject $buildTarget $script:ResolvedConfiguration $gameDataDirectory $sharedDataListing
			}
		}
		Complete-CompileBuild $script:BuildExitCode
	}

	# Local generation: materialize once, then run each build target's RunDataPacker=true build under
	# the Gaea guard unless -AllowGaeaExport is passed, and under the expensive-export guard when
	# -ForbidExpensiveExport is passed.
	# Session worktrees are created with the two large Engine/Data art subtrees skipped by sparse
	# checkout, and this is the one build that reads them as DataPacker inputs, so restore the full
	# working tree first. Disabling sparse checkout is idempotent and keeps uncommitted edits.
	# The sparse state itself is the predicate: files created under a skipped tree recreate its
	# directory without restoring the tracked bytes, so presence would report a false restore.
	$sparseState = Start-CompileChild 'git' @('-C', $script:Root, 'config', '--get', 'core.sparseCheckout') $true
	if ($sparseState.Stdout.Trim() -ieq 'true') {
		$restore = Invoke-AgentProcess 'git' @('-C', $script:Root, 'sparse-checkout', 'disable') $PSScriptRoot
		if (-not [string]::IsNullOrWhiteSpace($restore.Stdout)) { Write-CompileDiagnostic $restore.Stdout.Trim() }
		if (-not [string]::IsNullOrWhiteSpace($restore.Stderr)) { Write-CompileDiagnostic $restore.Stderr.Trim() }
		if ($restore.ExitCode -ne 0) {
			# git prints its fatal reason first and advice after it; one bounded line keeps the envelope
			# message a one-line handoff row.
			$reason = $restore.Stderr -split '\r?\n' | Where-Object { $_.Trim() } | Select-Object -First 1
			$suffix = if ($reason) { $reason = $reason.Trim(); ": $($reason.Substring(0, [Math]::Min(300, $reason.Length)))" } else { '.' }
			Stop-CompileInvoke 'data.sparse-restore-failed' "git sparse-checkout disable exited $($restore.ExitCode) in '$($script:Root)', so the generation build may still be missing sparse-skipped Engine\Data inputs$suffix"
		}
	}
	if ($script:IsLinkedWorktree) {
		# Build DataPacker Release through WorktreeCli under datapacker.lock before exporting.
		# A failure reports this build's envelope in place of the requested builds' envelopes.
		$localDataPackerBuild = Invoke-WorktreeCliBuild $null @() -LocalDataPacker
		if ($localDataPackerBuild.ExitCode -ne 0) {
			[Console]::Out.Write($localDataPackerBuild.Stdout)
			Complete-CompileBuild $localDataPackerBuild.ExitCode
		}
		$script:Summary.Add("DataPacker Release build result: $($localDataPackerBuild.Stdout.Trim())")
	}
	$dataPacker = Join-Path $script:Root 'DataPacker\Platforms\VisualStudio2026\Output\DataPacker.exe'
	if (-not (Test-Path -LiteralPath $dataPacker -PathType Leaf)) {
		Stop-CompileInvoke 'datapacker.missing' "The worktree DataPacker is missing: '$dataPacker'. Build -Target DataPacker first."
	}
	$isPrimaryLocal = (Get-AgentCanonicalPath $script:Root).Equals((Get-AgentCanonicalPath $primary), [StringComparison]::OrdinalIgnoreCase)
	$runLocalGeneration = {
		# In the primary checkout the Local data directory is the Shared one, so this export rewrites the
		# Shared data, which the stamp may only vouch for as exported from its own commit. A failed delete
		# throws here, before any data is written.
		if ($isPrimaryLocal) {
			$primaryStampPath = Get-AgentSharedDataStampPath $primary $gameProject
			if (Test-Path -LiteralPath $primaryStampPath -PathType Leaf) { [IO.File]::Delete($primaryStampPath) }
		}
		# DataPacker writes progress and mutex-wait notices to stdout, which is reserved for the JSON
		# envelopes, so its output is captured and forwarded as a diagnostic instead.
		$materialize = Start-CompileChild $dataPacker @(
			'--materialize-data',
			(Join-Path $script:Root 'Engine\Data'),
			(Join-Path $script:Root "Projects\$gameProject\Data"),
			$gameDataDirectory) $true
		if (-not [string]::IsNullOrWhiteSpace($materialize.Stdout)) { Write-CompileDiagnostic $materialize.Stdout.Trim() }
		if ($materialize.ExitCode -ne 0) {
			# DataPacker prints its failure reason last, and only to stdout; one bounded line keeps the
			# envelope message a one-line handoff row.
			$reason = $materialize.Stdout -split '\r?\n' | Where-Object { $_.Trim() } | Select-Object -Last 1
			$suffix = if ($reason) { $reason = $reason.Trim(); ": $($reason.Substring(0, [Math]::Min(300, $reason.Length)))" } else { '.' }
			Stop-CompileInvoke 'data-materialize.failed' "DataPacker --materialize-data exited $($materialize.ExitCode) for '$gameDataDirectory'$suffix"
		}

		$hadGaeaGuard = Test-Path "Env:$($script:GaeaGuardVariable)"
		$previousGaeaGuard = [Environment]::GetEnvironmentVariable($script:GaeaGuardVariable)
		$hadGaeaAllow = Test-Path "Env:$($script:GaeaAllowVariable)"
		$previousGaeaAllow = [Environment]::GetEnvironmentVariable($script:GaeaAllowVariable)
		if ($ForbidExpensiveExport) {
			$hadExpensiveExportGuard = Test-Path "Env:$($script:ExpensiveExportGuardVariable)"
			$previousExpensiveExportGuard = [Environment]::GetEnvironmentVariable($script:ExpensiveExportGuardVariable)
		}
		try {
			if (-not $AllowGaeaExport) {
				[Environment]::SetEnvironmentVariable($script:GaeaGuardVariable, '1')
				$script:Summary.Add("Gaea guard applied: $($script:GaeaGuardVariable)=1 for the generation build.")
			}
			else {
				[Environment]::SetEnvironmentVariable($script:GaeaAllowVariable, '1')
				$script:Summary.Add('Gaea authoring enabled under explicit -AllowGaeaExport authorization; caller forbid guards remain effective.')
			}
			if ($ForbidExpensiveExport) {
				[Environment]::SetEnvironmentVariable($script:ExpensiveExportGuardVariable, '1')
				$script:Summary.Add("Expensive-export guard applied: $($script:ExpensiveExportGuardVariable)=1 for the generation build.")
			}
			foreach ($buildTarget in $script:BuildTargets) {
				$buildExitCode = Invoke-WorktreeCliBuild $targetPaths[$buildTarget] $dataProperties
				# Local output needs no listing: the comparison skips data directories under the worktree root.
				if ($buildExitCode -eq 0 -and $script:SelectedFiles.Count -eq 0 -and $Target -cne 'ModelViewer') {
					Write-AgentSharedDataRecord $script:Root $gameProject $buildTarget $script:ResolvedConfiguration $gameDataDirectory @()
				}
			}
		}
		finally {
			if (-not $AllowGaeaExport) {
				if ($hadGaeaGuard) { [Environment]::SetEnvironmentVariable($script:GaeaGuardVariable, $previousGaeaGuard) }
				else { [Environment]::SetEnvironmentVariable($script:GaeaGuardVariable, $null) }
			}
			if ($AllowGaeaExport) {
				if ($hadGaeaAllow) { [Environment]::SetEnvironmentVariable($script:GaeaAllowVariable, $previousGaeaAllow) }
				else { [Environment]::SetEnvironmentVariable($script:GaeaAllowVariable, $null) }
			}
			if ($ForbidExpensiveExport) {
				if ($hadExpensiveExportGuard) { [Environment]::SetEnvironmentVariable($script:ExpensiveExportGuardVariable, $previousExpensiveExportGuard) }
				else { [Environment]::SetEnvironmentVariable($script:ExpensiveExportGuardVariable, $null) }
			}
		}
	}
	if ($isPrimaryLocal) {
		try {
			Invoke-AgentBootstrapLock -PrimaryCheckout $primary -Action $runLocalGeneration
		}
		catch [TimeoutException] {
			Stop-CompileInvoke 'shared-data.refresh-timeout' 'Timed out waiting for the AgentTools bootstrap mutex before running primary Local generation.'
		}
	}
	else {
		& $runLocalGeneration
	}
	if ($script:BuildExitCode -ne 0 -and -not $AllowGaeaExport) {
		$script:Summary.Add("A failure reporting 'Gaea.Swarm export blocked by $($script:GaeaGuardVariable)=1' means this worktree's island inputs differ from the shared Gaea bake. When this worktree is behind primary main, rebase onto it. A change to this worktree's own island inputs exports only through -AllowGaeaExport, which only an approved plan or acceptance criterion requiring that island re-bake authorizes.")
	}

	Complete-CompileBuild $script:BuildExitCode
}
catch {
	foreach ($line in $script:Summary) { Write-CompileDiagnostic $line }
	$code = if ($null -ne $script:BlockedCode) { $script:BlockedCode } else { 'internal.error' }
	if ($script:BuildStreamed) {
		# The build envelope already owns stdout, so this failure is stderr
		# only: the first nonzero build code, otherwise the internal-error code.
		Write-CompileDiagnostic "post-build failure $code - $($_.Exception.Message)"
		if ($script:BuildExitCode -ne 0) { exit $script:BuildExitCode }
		exit 1
	}
	if ($null -ne $script:BlockedCode) { Complete-CompileInvoke 2 'blocked' $code $_.Exception.Message }
	Complete-CompileInvoke 1 'error' $code $_.Exception.Message
}
