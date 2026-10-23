# Resolves the deterministic build context /compile needs before any build command runs:
# repository identities, the game project (the -Project name, else the only game project
# discovered), the changed-path set, the data mode and its directories, and optionally DevEnvDir.
# Read-only: it launches no writing command and emits nothing but stdout JSON and stderr
# diagnostics.
#
# dataBuildMode is decided here from the primary's Shared data stamp alone, and no agent judgment
# remains: Shared when this worktree's data inputs equal those of the stamp's commit, otherwise
# Local, including a missing or unresolvable stamp.
[CmdletBinding()]
param(
	[string] $RepositoryRoot,
	[string] $Baseline,
	[string] $PrimaryCheckout,
	[string] $Project,
	[ValidateSet('ModelViewer')][string] $Target,
	[switch] $IncludeDevEnvDir
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$sharedScripts = Join-Path $PSScriptRoot '..\..\..\scripts'
if (-not (Test-Path -LiteralPath (Join-Path $sharedScripts 'AgentScriptCommon.psm1'))) {
	$sharedScripts = Join-Path $PSScriptRoot '..\..\..\..\.agents\scripts'
}
Import-Module (Join-Path $sharedScripts 'AgentScriptCommon.psm1') -Force
Import-Module (Join-Path $sharedScripts 'FinalizeWorkflowCommon.psm1') -Force

$script:MaximumReportedPaths = 512
$script:MaximumOutputBytes = 65536
$script:MaximumReportedDataInputChanges = 20

$script:BlockedCode = $null
$script:Result = [ordered]@{
	schemaVersion = 'broken-engine-compile-context/v2'
	status = 'error'
	code = 'internal.error'
	message = 'Compile context resolution did not run.'
	repositoryRoot = $null
	primaryCheckout = $null
	baseline = $null
	project = $null
	projects = @()
	changedPaths = @()
	changedPathCount = 0
	changedPathsTruncated = $false
	dataBuildMode = $null
	dataBuildModeReason = $null
	sharedDataStamp = $null
	dataInputChanges = @()
	dataInputChangeCount = 0
	gameDataDirectory = $null
	generatedDataIncludeRoot = $null
	recommendedTargets = @()
}
if ($IncludeDevEnvDir) { $script:Result.devEnvDir = $null }

function Complete-CompileContext([int] $ExitCode, [string] $Status, [string] $Code, [string] $Message) {
	$script:Result.status = $Status
	$script:Result.code = $Code
	$script:Result.message = $Message
	$json = $script:Result | ConvertTo-Json -Depth 8 -Compress
	if ([Text.Encoding]::UTF8.GetByteCount($json) -gt $script:MaximumOutputBytes) {
		# Never elide reported paths silently: drop the lists, flag them, and block.
		$script:Result.changedPaths = @()
		$script:Result.changedPathsTruncated = $true
		$script:Result.recommendedTargets = @()
		$script:Result.status = 'blocked'
		$script:Result.code = 'output.capacity-exceeded'
		$script:Result.message = "Compile context exceeds the $($script:MaximumOutputBytes)-byte output cap; the changed-path evidence cannot be reported completely."
		$json = $script:Result | ConvertTo-Json -Depth 8 -Compress
		$ExitCode = 2
	}
	[Console]::Out.Write($json)
	[Console]::Error.WriteLine("compile-context: $($script:Result.status) $($script:Result.code) - $($script:Result.message)")
	exit $ExitCode
}

function Stop-CompileContext([string] $Code, [string] $Message) {
	$script:BlockedCode = $Code
	throw $Message
}

function Invoke-ContextGit([string[]] $Arguments) {
	# $PSScriptRoot is a known-good working directory; every repository selection is an explicit -C argument.
	return Invoke-FinalizeNativeText 'git.exe' $Arguments $PSScriptRoot
}

function Get-ContextGitText([string[]] $Arguments, [string] $Code, [string] $Message) {
	$response = Invoke-ContextGit $Arguments
	if ($response.ExitCode -ne 0) { Stop-CompileContext $Code "$Message git exit $($response.ExitCode): $($response.Stderr.Trim())" }
	return $response.Stdout
}

function Get-ContextCanonicalDirectory([string] $Path, [string] $Code, [string] $Label) {
	if ([string]::IsNullOrWhiteSpace($Path)) { Stop-CompileContext $Code "$Label is empty." }
	$canonical = $null
	try { $canonical = Get-AgentCanonicalPath $Path.Trim() }
	catch { Stop-CompileContext $Code "$Label is not a usable path: '$Path'." }
	if (-not [IO.Path]::IsPathFullyQualified($canonical)) { Stop-CompileContext $Code "$Label must be an absolute path: '$canonical'." }
	$item = Get-Item -LiteralPath $canonical -Force -ErrorAction SilentlyContinue
	if ($null -eq $item -or -not $item.PSIsContainer) { Stop-CompileContext $Code "$Label must be an existing directory: '$canonical'." }
	return $canonical
}

function Get-ContextNormalizedPaths([string] $Text) {
	$paths = [Collections.Generic.List[string]]::new()
	foreach ($line in ($Text -split "`n")) {
		$value = $line.TrimEnd("`r").Trim()
		if ($value.Length -eq 0) { continue }
		$paths.Add($value.Replace('\', '/'))
	}
	return $paths
}

function Get-ContextRecommendedTargets([string[]] $Paths, [string] $Project) {
	# A changed path under one of the shared roots recommends both executables; any other path
	# contributes nothing, so this stays advisory and the worker still fixes the final list.
	if ([string]::IsNullOrEmpty($Project)) { return @() }
	$sharedPrefixes = @('Common/', 'Engine/', "Projects/$Project/Source/")
	$viewerChanged = @($Paths | Where-Object {
		$_.StartsWith('ModelViewer/', [StringComparison]::OrdinalIgnoreCase) -or
		$_.StartsWith('Common/', [StringComparison]::OrdinalIgnoreCase) -or
		$_.StartsWith('Engine/', [StringComparison]::OrdinalIgnoreCase)
	}).Count -gt 0
	$viewerTargets = @(if ($viewerChanged -or $Target -eq 'ModelViewer') {
		@([pscustomobject][ordered]@{ project = 'ModelViewer'; configuration = 'Debug'; reason = 'viewer' })
	})
	foreach ($path in $Paths) {
		foreach ($prefix in $sharedPrefixes) {
			if ($path.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
				return @(
					[pscustomobject][ordered]@{ project = $Project; configuration = 'Debug'; reason = 'shared-code' }
					[pscustomobject][ordered]@{ project = "$($Project)Server"; configuration = 'Debug'; reason = 'shared-code' }
				) + $viewerTargets
			}
		}
	}
	if ($viewerTargets.Count -gt 0) { return $viewerTargets }
	return @([pscustomobject][ordered]@{ project = $Project; configuration = 'Debug'; reason = 'default' })
}

function Get-ContextDevEnvDirectory {
	$candidate = Join-Path 'C:\Program Files\Microsoft Visual Studio\18\Community' 'Common7\IDE'
	if (-not (Test-ContextIdeDirectory $candidate)) {
		$vsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
		if (-not (Test-Path -LiteralPath $vsWhere -PathType Leaf)) { Stop-CompileContext 'dev-env-dir.unresolved' "Visual Studio 2026 is not at the fixed install path and vswhere is missing at '$vsWhere'." }
		$response = Invoke-FinalizeNativeText $vsWhere @('-latest', '-version', '[18.0,19.0)', '-products', '*', '-requires', 'Microsoft.Component.MSBuild', '-property', 'installationPath') $PSScriptRoot
		$install = $response.Stdout.Trim()
		if ($response.ExitCode -ne 0 -or [string]::IsNullOrWhiteSpace($install)) { Stop-CompileContext 'dev-env-dir.unresolved' "vswhere did not report a Visual Studio 2026 installation: $($response.Stderr.Trim())" }
		$candidate = Join-Path ($install -split "`n")[0].Trim() 'Common7\IDE'
		if (-not (Test-ContextIdeDirectory $candidate)) { Stop-CompileContext 'dev-env-dir.unresolved' "Visual Studio IDE executable missing under '$candidate'." }
	}
	return (Get-AgentCanonicalPath $candidate) + '\'
}

function Test-ContextIdeDirectory([string] $Directory) {
	return (Test-Path -LiteralPath (Join-Path $Directory 'devenv.com') -PathType Leaf) -or (Test-Path -LiteralPath (Join-Path $Directory 'devenv.exe') -PathType Leaf)
}

try {
	# Identity precedence per input: explicit parameter, then derived default.
	$rootInput = if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) { $null } else { $RepositoryRoot.Trim() }
	if ($null -eq $rootInput) {
		$rootInput = (Get-ContextGitText @('-C', $PSScriptRoot, 'rev-parse', '--show-toplevel') 'repository-root.unresolved' 'Could not derive the repository root from this script location:').Trim()
	}
	$root = Get-ContextCanonicalDirectory $rootInput 'repository-root.unresolved' 'RepositoryRoot'
	$topLevel = Get-ContextCanonicalDirectory (Get-ContextGitText @('-C', $root, 'rev-parse', '--show-toplevel') 'repository-root.unresolved' "'$root' is not inside a Git working tree:").Trim() 'repository-root.unresolved' 'Repository top level'
	if (-not $topLevel.Equals($root, [StringComparison]::OrdinalIgnoreCase)) {
		Stop-CompileContext 'repository-root.mismatch' "RepositoryRoot '$root' is not its own Git top level '$topLevel'."
	}
	$script:Result.repositoryRoot = $root

	# With several game projects and none named, the project stays null and the caller picks from projects.
	$projects = @(Get-AgentGameProjects $root)
	$script:Result.projects = $projects
	if ($Target -eq 'ModelViewer') {
		if (-not [string]::IsNullOrWhiteSpace($Project) -and $Project.Trim() -cne 'BrokenEngineSandbox') {
			Stop-CompileContext 'project.invalid-data-owner' 'ModelViewer uses BrokenEngineSandbox data.'
		}
		$Project = 'BrokenEngineSandbox'
	}
	if (-not [string]::IsNullOrWhiteSpace($Project)) {
		$projectInput = $Project.Trim()
		if ($projectInput -cnotin $projects) {
			Stop-CompileContext 'project.not-found' "Project '$projectInput' is not a game project under '$root'; discovered: $(if ($projects.Count -eq 0) { 'none' } else { $projects -join ', ' })."
		}
		$script:Result.project = $projectInput
	}
	elseif ($projects.Count -eq 1) { $script:Result.project = $projects[0] }

	$primaryInput = if ([string]::IsNullOrWhiteSpace($PrimaryCheckout)) { $null } else { $PrimaryCheckout.Trim() }
	if ($null -eq $primaryInput) {
		$commonDirectory = Get-ContextCanonicalDirectory (Get-ContextGitText @('-C', $root, 'rev-parse', '--path-format=absolute', '--git-common-dir') 'primary-checkout.unresolved' 'Could not resolve the Git common directory:').Trim() 'primary-checkout.unresolved' 'Git common directory'
		$primaryInput = [IO.Path]::GetDirectoryName($commonDirectory)
	}
	$primary = Get-ContextCanonicalDirectory $primaryInput 'primary-checkout.unresolved' 'PrimaryCheckout'
	$primaryGit = Get-Item -LiteralPath (Join-Path $primary '.git') -Force -ErrorAction SilentlyContinue
	if ($null -eq $primaryGit -or -not $primaryGit.PSIsContainer -or ($primaryGit.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
		Stop-CompileContext 'primary-checkout.invalid' "PrimaryCheckout must contain an ordinary '.git' directory: '$primary'."
	}
	$script:Result.primaryCheckout = $primary

	# An explicitly supplied baseline is authoritative and is never advanced. Without one, the session
	# context owns the answer, and may advance it to the session's real divergence point after a
	# mid-session rebase. Baseline resolution never fails closed: any module, session, or resolution
	# failure falls back to the HEAD default.
	$baselineInput = if ([string]::IsNullOrWhiteSpace($Baseline)) { $null } else { $Baseline.Trim() }
	if ($null -eq $baselineInput) {
		try {
			Import-Module (Join-Path $sharedScripts 'AgentWorktreeSession.psm1') -Force -DisableNameChecking
			$sessionContext = Get-AgentWorktreeSessionContext -Worktree $root
			if (-not [string]::IsNullOrWhiteSpace($sessionContext.SessionId) -and -not [string]::IsNullOrWhiteSpace($sessionContext.Baseline)) {
				$sessionResponse = Invoke-ContextGit @('-C', $root, 'rev-parse', '--verify', '--quiet', '--end-of-options', "$($sessionContext.Baseline)^{commit}")
				$sessionBaseline = $sessionResponse.Stdout.Trim()
				if ($sessionResponse.ExitCode -eq 0 -and $sessionBaseline -cmatch '^[0-9a-f]{40}$') { $baselineInput = $sessionBaseline }
			}
		}
		catch {
			[Console]::Error.WriteLine("compile-context: session baseline resolution failed; falling back to HEAD: $($_.Exception.Message)")
		}
		if ($null -eq $baselineInput) { $baselineInput = 'HEAD' }
	}
	$baselineResponse = Invoke-ContextGit @('-C', $root, 'rev-parse', '--verify', '--quiet', '--end-of-options', "$baselineInput^{commit}")
	$resolvedBaseline = $baselineResponse.Stdout.Trim()
	if ($baselineResponse.ExitCode -ne 0 -or $resolvedBaseline -cnotmatch '^[0-9a-f]{40}$') {
		Stop-CompileContext 'baseline.unresolved' "Baseline '$baselineInput' does not resolve to a commit in '$root'."
	}
	$script:Result.baseline = $resolvedBaseline

	# One baseline diff covers committed, staged, and unstaged tracked changes; ls-files adds untracked files.
	$diffText = Get-ContextGitText @('-c', 'core.quotepath=false', '-C', $root, 'diff', '--name-only', $resolvedBaseline, '--') 'changed-paths.unavailable' 'Could not list changed paths:'
	$untrackedText = Get-ContextGitText @('-c', 'core.quotepath=false', '-C', $root, 'ls-files', '--others', '--exclude-standard') 'changed-paths.unavailable' 'Could not list untracked files:'
	$changed = [Collections.Generic.SortedSet[string]]::new([StringComparer]::Ordinal)
	foreach ($path in (Get-ContextNormalizedPaths $diffText)) { [void] $changed.Add($path) }
	foreach ($path in (Get-ContextNormalizedPaths $untrackedText)) { [void] $changed.Add($path) }

	$script:Result.changedPathCount = $changed.Count
	$script:Result.changedPathsTruncated = $changed.Count -gt $script:MaximumReportedPaths
	$script:Result.changedPaths = @(@($changed) | Select-Object -First $script:MaximumReportedPaths)

	if ($null -ne $script:Result.project) {
		# The stamp vouches for the Shared data only as exported from its own commit, so Shared needs this
		# worktree's data inputs to equal that commit's; every other case resolves Local.
		$stamp = $null
		try { $stamp = [IO.File]::ReadAllText((Get-AgentSharedDataStampPath $primary $script:Result.project)).Trim() }
		catch [IO.FileNotFoundException], [IO.DirectoryNotFoundException] { }
		$script:Result.sharedDataStamp = $stamp
		if ($null -eq $stamp) { $reason = 'stamp-missing' }
		else {
			$stampResponse = Invoke-ContextGit @('-C', $root, 'rev-parse', '--verify', '--quiet', '--end-of-options', "$stamp^{commit}")
			$stampCommit = $stampResponse.Stdout.Trim()
			if ($stampResponse.ExitCode -ne 0 -or $stampCommit -cnotmatch '^[0-9a-f]{40}$') { $reason = 'stamp-unresolvable' }
			else {
				[string[]] $dataInputChanges = @(Get-AgentDataInputChanges $root $stampCommit $script:Result.project)
				$script:Result.dataInputChangeCount = $dataInputChanges.Count
				$script:Result.dataInputChanges = @($dataInputChanges | Select-Object -First $script:MaximumReportedDataInputChanges)
				$reason = if ($dataInputChanges.Count -eq 0) { 'stamp-match' } else { 'inputs-differ' }
			}
		}
		$script:Result.dataBuildModeReason = $reason
		$script:Result.dataBuildMode = if ($reason -ceq 'stamp-match') { 'Shared' } else { 'Local' }
		$dataOwner = if ($script:Result.dataBuildMode -ceq 'Local') { $root } else { $primary }
		$gameDataDirectory = Get-AgentCanonicalPath (Join-Path $dataOwner "Projects\$($script:Result.project)\Platforms\VisualStudio2026\Output\Data")
		$script:Result.gameDataDirectory = $gameDataDirectory
		$script:Result.generatedDataIncludeRoot = Get-AgentCanonicalPath ([IO.Path]::GetDirectoryName($gameDataDirectory))
	}

	if ($IncludeDevEnvDir) { $script:Result.devEnvDir = Get-ContextDevEnvDirectory }

	if ($script:Result.changedPathsTruncated) {
		Complete-CompileContext 2 'blocked' 'changed-paths.capacity-exceeded' "The changed-path evidence exceeds the $($script:MaximumReportedPaths)-entry report cap and cannot be reported completely."
	}

	$script:Result.recommendedTargets = @(Get-ContextRecommendedTargets @($changed) $script:Result.project)
	$modeText = if ($null -eq $script:Result.dataBuildMode) { 'unresolved without a game project' } else { "$($script:Result.dataBuildMode) ($($script:Result.dataBuildModeReason))" }
	Complete-CompileContext 0 'pass' 'ok' "Compile context resolved; data mode $modeText."
}
catch {
	if ($null -ne $script:BlockedCode) { Complete-CompileContext 2 'blocked' $script:BlockedCode $_.Exception.Message }
	Complete-CompileContext 1 'error' 'internal.error' $_.Exception.Message
}
