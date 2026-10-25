[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)]
	[ValidateSet('DataPacker', 'ModelViewer')]
	[string] $Target,
	[Parameter(Mandatory = $true)]
	[string] $Worktree,
	[Parameter(Mandatory = $true)]
	[string] $WorktreeCliExecutable,
	[Parameter(Mandatory = $true)]
	[string] $PrimaryCheckout,
	[Parameter(Mandatory = $true)]
	[string] $Baseline
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'AgentScriptCommon.psm1') -Force

# DataPacker and ModelViewer are not symlinked, so each worktree owns its own Output. Rather than
# compile the Target from scratch at session start, seed the worktree Output by copying the primary
# bootstrap prebuild (Bootstrap-AgentTools.ps1) when a stamp proves the primary exe was built from the
# exact trees this clean worktree holds. ModelViewer also needs Shared data, because its prebuild
# compiles the primary's generated data headers and embeds the primary data path. The copy runs
# outside the bootstrap mutex, so the copied exe is re-hashed against the stamp to close torn-read
# races against a concurrent primary rebuild or user VS build. Any failure removes the files this
# run copied, warns with the reason, and falls through to the Target's local build below.
# The tree lists match Bootstrap-AgentTools.ps1's, in stamp order. ThirdParty is included because
# directly tracked files under it are on the include path; the tree-hash check only catches
# committed drift, so uncommitted edits need the clean gate.
$trees = if ($Target -ceq 'DataPacker') { @('DataPacker', 'Common', 'ThirdParty') } else { @('Tools/ModelViewer', 'Common', 'Engine', 'ThirdParty', 'DataPacker', 'Projects/BrokenEngineSandbox/Data') }
$targetDirectory = if ($Target -ceq 'ModelViewer') { 'Tools\ModelViewer' } else { 'DataPacker' }
$outputDirectory = Join-Path $Worktree "$targetDirectory\Platforms\VisualStudio2026\Output"
$worktreeExe = Join-Path $outputDirectory "$Target.exe"
$worktreePdb = Join-Path $outputDirectory "$Target.pdb"
$primaryOutput = Join-Path $PrimaryCheckout "$targetDirectory\Platforms\VisualStudio2026\Output"
$stampPath = Join-Path $primaryOutput "$($Target)PrebuildStamp.txt"
$compileScripts = Join-Path $PSScriptRoot '..\skills\compile\scripts'
$copiedFiles = [Collections.Generic.List[string]]::new()
$seeded = $false
try {
	if (-not (Test-Path -LiteralPath $stampPath -PathType Leaf)) { throw "No $Target prebuild stamp at '$stampPath'." }
	$stampLines = @([IO.File]::ReadAllText($stampPath).Replace("`r", '').Split("`n") | Where-Object { $_.Length -ne 0 })
	if ($stampLines.Count -ne $trees.Count + 2) { throw "$Target prebuild stamp is malformed: '$stampPath'." }
	$stampTreeHashes = $stampLines[0..($trees.Count - 1)]
	$stampExeHash = $stampLines[$trees.Count]
	$stampExeLength = [long] $stampLines[$trees.Count + 1]

	$treeNames = ($trees | ForEach-Object { "$_/" }) -join ', '
	$dirty = @(Invoke-AgentGit (@('-C', $Worktree, 'status', '--porcelain', '--untracked-files=all', '--') + $trees))
	if ($dirty.Count -ne 0) { throw "Worktree has uncommitted changes under $($treeNames): $($dirty -join '; ')." }
	$worktreeTreeHashes = @(Invoke-AgentGit (@('-C', $Worktree, 'rev-parse') + @($trees | ForEach-Object { "HEAD:$_" })))
	if (($worktreeTreeHashes -join "`n") -cne ($stampTreeHashes -join "`n")) { throw "Worktree $treeNames trees differ from the primary $Target prebuild stamp." }

	if ($Target -ceq 'ModelViewer') {
		# A child process, because Resolve-CompileContext.ps1 ends by calling exit, which in-process would
		# end this script's caller too.
		$contextScript = Join-Path $compileScripts 'Resolve-CompileContext.ps1'
		$contextRun = Invoke-AgentProcess 'pwsh' @('-NoProfile', '-File', $contextScript, '-Target', 'ModelViewer', '-RepositoryRoot', $Worktree, '-PrimaryCheckout', $PrimaryCheckout, '-Baseline', $Baseline) $PSScriptRoot
		if ($contextRun.ExitCode -ne 0) { throw "Resolve-CompileContext.ps1 exited $($contextRun.ExitCode): $($contextRun.Stderr.Trim())" }
		if ([string]::IsNullOrWhiteSpace($contextRun.Stdout)) { throw 'Resolve-CompileContext.ps1 produced no JSON result.' }
		try { $context = $contextRun.Stdout | ConvertFrom-Json }
		catch { throw 'Resolve-CompileContext.ps1 produced an unparsable JSON result.' }
		if ([string]$context.dataBuildMode -cne 'Shared') { throw "ModelViewer data resolves to $($context.dataBuildMode) ($($context.dataBuildModeReason)), not the Shared data the primary prebuild compiled." }
		$gameDataDirectory = [string]$context.gameDataDirectory
		$generatedDataIncludeRoot = [string]$context.generatedDataIncludeRoot
	}

	$primaryExe = Join-Path $primaryOutput "$Target.exe"
	$primaryPdb = Join-Path $primaryOutput "$Target.pdb"
	if (-not (Test-Path -LiteralPath $primaryExe -PathType Leaf)) { throw "Primary $Target executable is missing: '$primaryExe'." }
	New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
	# Each file is recorded before its copy starts, so the failure path also removes a partial copy and
	# keeps any worktree file this run never overwrote.
	$copiedFiles.Add($worktreeExe)
	Copy-Item -LiteralPath $primaryExe -Destination $worktreeExe -Force
	if (Test-Path -LiteralPath $primaryPdb -PathType Leaf) {
		$copiedFiles.Add($worktreePdb)
		Copy-Item -LiteralPath $primaryPdb -Destination $worktreePdb -Force
	}

	$copiedBytes = [IO.File]::ReadAllBytes($worktreeExe)
	$copiedHash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($copiedBytes)).ToLowerInvariant()
	if ($copiedBytes.Length -ne $stampExeLength -or $copiedHash -cne $stampExeHash) { throw "Copied $Target executable does not match the primary prebuild stamp." }
	if ($Target -ceq 'ModelViewer') { Write-AgentModelViewerResolvedData $Worktree 'Shared' 'false' $gameDataDirectory $generatedDataIncludeRoot }
	$seeded = $true
	Write-Host "Seeded from primary prebuilt $Target at '$primaryOutput' for '$Worktree'."
}
catch {
	foreach ($copiedFile in $copiedFiles) { Remove-Item -LiteralPath $copiedFile -Force -ErrorAction SilentlyContinue }
	Write-Warning "Could not seed worktree $Target from the primary prebuild, building locally: $($_.Exception.Message)"
}
if ($seeded) { return }

if ($Target -ceq 'ModelViewer') {
	# The local build resolves the data mode itself and rewrites Build\ResolvedData.props.
	& pwsh -NoProfile -File (Join-Path $compileScripts 'Invoke-CompileBuild.ps1') -Target ModelViewer -Configuration Release -RepositoryRoot $Worktree -PrimaryCheckout $PrimaryCheckout -Baseline $Baseline
	if ($LASTEXITCODE -ne 0) { throw "ModelViewer build failed in worktree '$Worktree' (exit $LASTEXITCODE)." }
	return
}

# WorktreeCli serializes the target inside this private worktree, so there is no cross-session
# contention here. A nonzero exit hard-fails session start (new session or reattach); the message
# names the local build, not the shared-executable wrap-up-sessions guidance, which does not apply
# to a private compile.
$solution = Join-Path $Worktree 'DataPacker\Platforms\VisualStudio2026\DataPacker.sln'
& $WorktreeCliExecutable build $solution '/p:Configuration=Release' '/p:Platform=x64' '/p:EnableClangTidyCodeAnalysis=false' '/p:RunCodeAnalysis=false' '/verbosity:minimal'
if ($LASTEXITCODE -ne 0) { throw "Worktree DataPacker Release build failed with exit code $LASTEXITCODE for '$Worktree'. Fix the local DataPacker sources so they compile, then retry." }
