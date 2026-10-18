[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)]
	[string] $RepositoryRoot,
	[int] $WaitSeconds = 500,
	[switch] $SharedDataOnly
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'AgentScriptCommon.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'WorktreeCliSessionExclusion.psm1') -Force

if (-not [System.IO.Path]::IsPathRooted($RepositoryRoot)) { throw 'RepositoryRoot must be absolute.' }
$root = Get-AgentCanonicalPath $RepositoryRoot
$topLevel = Get-AgentCanonicalPath (@(Invoke-AgentGit @('-C', $root, 'rev-parse', '--show-toplevel'))[0].Trim())
if (-not $topLevel.Equals($root, [StringComparison]::OrdinalIgnoreCase)) { throw "RepositoryRoot is not the repository root: '$root'." }
$gitDirectory = Get-Item -LiteralPath (Join-Path $root '.git') -Force -ErrorAction Stop
if (-not $gitDirectory.PSIsContainer -or ($gitDirectory.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "RepositoryRoot is not the primary checkout: '$root'." }
$commonDir = Get-AgentCanonicalPath (@(Invoke-AgentGit @('-C', $root, 'rev-parse', '--path-format=absolute', '--git-common-dir'))[0].Trim())
if (-not $commonDir.Equals((Get-AgentCanonicalPath $gitDirectory.FullName), [StringComparison]::OrdinalIgnoreCase)) { throw "RepositoryRoot is not the primary checkout: '$root'." }

$worktreeCliOutput = Join-Path $root 'Tools\WorktreeCli\Platforms\VisualStudio2026\Output'
$agentHarnessOutput = Join-Path $root 'Tools\AgentHarness\Platforms\VisualStudio2026\Output'
$thirdPartyOutput = Join-Path $root 'ThirdParty\Prebuilts\Platforms\VisualStudio2026\Output'
foreach ($output in @($worktreeCliOutput, $agentHarnessOutput, $thirdPartyOutput)) {
	$outputItem = Get-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
	if ($null -ne $outputItem -and (-not $outputItem.PSIsContainer -or ($outputItem.Attributes -band [IO.FileAttributes]::ReparsePoint))) { throw "Primary shared Output must be an ordinary directory: '$output'." }
}
$worktreeCli = Join-Path $worktreeCliOutput 'WorktreeCli.exe'
$agentHarness = Join-Path $agentHarnessOutput 'AgentHarness.exe'
$stampPath = Join-Path $worktreeCliOutput 'AgentToolsSourceStamp.txt'
$capabilityScript = Join-Path $PSScriptRoot 'Test-AgentToolsCapabilities.ps1'
function Get-AgentToolsSourceStamp {
	# The stamp records built-source provenance only; a rev-parse failure must never block the
	# build, so on failure the stamp is simply left unwritten.
	try { return (@(Invoke-AgentGit @('-C', $root, 'rev-parse', 'HEAD:Tools/WorktreeCli', 'HEAD:Tools/AgentHarness', 'HEAD:Tools/ToolCommon', 'HEAD:Common/Sha256Hasher.h')) -join "`n") }
	catch { return $null }
}

# A transient operation claim excludes AgentTools promotion from swapping the shared WorktreeCli and
# AgentHarness executables while this bootstrap consumes them, on both the dirty-skip and build
# paths. Registered before the dirty check because that path already runs the canonical executables;
# released in the finally covering both paths.
$claimOwner = [guid]::NewGuid().ToString()
Register-WorktreeCliSession -RepositoryRoot $root -Owner $claimOwner -Label 'agenttools bootstrap' -Worktree $root -WaitSeconds $WaitSeconds | Out-Null
try {
	# Never compile uncertified working-tree bytes into the shared binaries: if the primary has
	# uncommitted changes under Tools/ or ThirdParty/, or to Common/Sha256Hasher.h, which the
	# AgentTools compile by path, skip all builds and start on the existing binaries.
	$dirty = @(Invoke-AgentGit @('-C', $root, 'status', '--porcelain', '--untracked-files=all', '--', 'Tools', 'ThirdParty', 'Common/Sha256Hasher.h'))
	if ($dirty.Count -ne 0) {
		Write-Warning "Primary has uncommitted changes under Tools/ or ThirdParty/, or to Common/Sha256Hasher.h, so shared AgentTools and ThirdParty binaries were NOT rebuilt: $($dirty -join '; ')."
		if (-not (Test-Path -LiteralPath $worktreeCli -PathType Leaf) -or -not (Test-Path -LiteralPath $agentHarness -PathType Leaf)) {
			throw "Primary AgentTools executables are missing and uncommitted changes under Tools/ or ThirdParty/, or to Common/Sha256Hasher.h, block a rebuild. Commit or stash those changes, then retry."
		}
		& $capabilityScript -WorktreeCliExecutable $worktreeCli -AgentHarnessExecutable $agentHarness | Out-Null
		Write-Host ''
		Write-Host "Skipped AgentTools bootstrap builds (dirty primary); using existing binaries at '$worktreeCli' and '$agentHarness'."
		return
	}

	$msBuild = 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe'
	if (-not (Test-Path -LiteralPath $msBuild -PathType Leaf)) {
		$vsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
		if (-not (Test-Path -LiteralPath $vsWhere -PathType Leaf)) { throw "Visual Studio locator is missing: '$vsWhere'." }
		$installPaths = @(& $vsWhere -latest -version '[18.0,19.0)' -products * -requires Microsoft.Component.MSBuild -property installationPath)
		if ($LASTEXITCODE -ne 0 -or $installPaths.Count -ne 1 -or [string]::IsNullOrWhiteSpace($installPaths[0])) { throw 'Unable to locate a Visual Studio installation containing MSBuild.' }
		$msBuild = Join-Path $installPaths[0].Trim() 'MSBuild\Current\Bin\MSBuild.exe'
		if (-not (Test-Path -LiteralPath $msBuild -PathType Leaf)) { throw "MSBuild is missing: '$msBuild'." }
	}

	$worktreeCliSolution = Join-Path $root 'Tools\WorktreeCli\Platforms\VisualStudio2026\WorktreeCli.sln'
	$agentHarnessSolution = Join-Path $root 'Tools\AgentHarness\Platforms\VisualStudio2026\AgentHarness.sln'
	$thirdPartySolution = Join-Path $root 'ThirdParty\Prebuilts\Platforms\VisualStudio2026\ThirdParty.sln'
	foreach ($solution in @($worktreeCliSolution, $agentHarnessSolution, $thirdPartySolution)) {
		if (-not (Test-Path -LiteralPath $solution -PathType Leaf)) { throw "Bootstrap solution is missing: '$solution'." }
	}
	# DataPacker is intentionally kept out of the hard-fail existence check above: its prebuild is
	# best-effort, so a missing solution warns and skips rather than failing session start.
	$dataPackerSolution = Join-Path $root 'DataPacker\Platforms\VisualStudio2026\DataPacker.sln'
	$dataPackerOutput = Join-Path $root 'DataPacker\Platforms\VisualStudio2026\Output'
	$dataPackerExe = Join-Path $dataPackerOutput 'DataPacker.exe'
	$dataPackerStampPath = Join-Path $dataPackerOutput 'DataPackerPrebuildStamp.txt'

	$commonBuildArguments = @('/p:Platform=x64', '/p:EnableClangTidyCodeAnalysis=false', '/p:RunCodeAnalysis=false', '/verbosity:minimal')
	function Invoke-BootstrapBuild([string] $Solution, [string] $Configuration, [string] $Executable) {
		# Blank line so each build's MSBuild output reads as its own block in the session start transcript.
		Write-Host ''
		# A link in place fails while another session runs $Executable, so the original is renamed aside
		# (running processes keep it) and a copy takes its name. The copy keeps the original
		# LastWriteTimeUtc, so unchanged sources still skip the link.
		$aside = $null
		if ($Executable) {
			# Best-effort: an aside still running in another session cannot be deleted yet.
			$leaf = Split-Path -Leaf $Executable
			try { Get-ChildItem -LiteralPath (Split-Path -Parent $Executable) -Filter "$leaf.bootstrap-*" -File -Force | ForEach-Object { try { [IO.File]::Delete($_.FullName) } catch { } } } catch { }
			if (Test-Path -LiteralPath $Executable -PathType Leaf) {
				$aside = "$Executable.bootstrap-$([DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ'))-$PID"
				[IO.File]::Copy($Executable, "$aside.staging")
				[IO.File]::Move($Executable, $aside)
			}
		}
		try {
			if ($null -ne $aside) { [IO.File]::Move("$aside.staging", $Executable) }
			$exitCode = Invoke-WorktreeCliTrackedProcess -Executable $msBuild -ArgumentList (@($Solution, "/p:Configuration=$Configuration") + $commonBuildArguments) -WorkingDirectory $root
		}
		finally {
			if ($null -ne $aside -and -not (Test-Path -LiteralPath $Executable)) { [IO.File]::Move($aside, $Executable) }
		}
		if ($exitCode -ne 0) { throw "AgentTools bootstrap build failed for '$Solution' with exit code $exitCode. If another live worktree session is holding these executables, wrap up active worktree sessions and retry." }
	}

	# PC-global mutex scoped like the session ledger mutex (Global\ + SID + repo hash) so concurrent
	# always-rebuild bootstraps serialize. The distinct AgentToolsBootstrap suffix keeps ledger
	# transitions unblocked.
	$bootstrapMutexName = "$((Get-WorktreeCliRepositoryIdentity $root).MutexName)_AgentToolsBootstrap"
	$mutex = [Threading.Mutex]::new($false, $bootstrapMutexName)
	$held = $false
	try {
		$deadline = [DateTime]::UtcNow.AddSeconds($WaitSeconds)
		$milliseconds = [Math]::Max(0, [Math]::Min([int]::MaxValue, [Math]::Ceiling(($deadline - [DateTime]::UtcNow).TotalMilliseconds)))
		try { $held = $mutex.WaitOne([int]$milliseconds) } catch [Threading.AbandonedMutexException] { $held = $true }
		if (-not $held) { throw "Timed out after $WaitSeconds seconds waiting for the AgentTools bootstrap mutex. If another live worktree session is holding these executables, wrap up active worktree sessions and retry." }

		# -SharedDataOnly refreshes Shared data mid-session, so it skips the shared AgentTools and ThirdParty
		# builds, which only a wrapper session start or explicit primary maintenance runs in place.
		if (-not $SharedDataOnly) {
			Invoke-BootstrapBuild $worktreeCliSolution 'Release' $worktreeCli
			Invoke-BootstrapBuild $agentHarnessSolution 'Release' $agentHarness
			& $capabilityScript -WorktreeCliExecutable $worktreeCli -AgentHarnessExecutable $agentHarness | Out-Null
			$builtStamp = Get-AgentToolsSourceStamp
			if ($null -ne $builtStamp) { [IO.File]::WriteAllText($stampPath, $builtStamp + "`n") }
		}
		# Snapshot DataPacker's build inputs before the ThirdParty Release lib it links is (re)built just
		# below (skipped under -SharedDataOnly, where the stamp's ThirdParty tree must match instead), so the
		# post-build re-check covers a landing or user VS build that advances any consumed input during the
		# prebuild. Best-effort like the prebuild: a snapshot failure disables it.
		$dataPackerPreTrees = $null
		$dataPackerPrebuiltClean = $false
		try {
			$dataPackerPreTrees = @(Invoke-AgentGit @('-C', $root, 'rev-parse', 'HEAD:DataPacker', 'HEAD:Common', 'HEAD:ThirdParty'))
			$dataPackerPreDirty = @(Invoke-AgentGit @('-C', $root, 'status', '--porcelain', '--untracked-files=all', '--', 'DataPacker', 'Common', 'ThirdParty'))
		}
		catch { Write-Warning "Could not snapshot DataPacker prebuild inputs, so the prebuild was skipped: $($_.Exception.Message)"; $dataPackerPreTrees = $null }
		if ($SharedDataOnly -and $null -ne $dataPackerPreTrees) {
			# The prebuild links the existing ThirdParty Release lib, built from the ThirdParty tree on the
			# existing stamp's third line, so a new stamp must not record a different tree.
			$stampThirdPartyTree = $null
			try { $stampThirdPartyTree = @([IO.File]::ReadAllLines($dataPackerStampPath))[2] } catch { }
			if ($stampThirdPartyTree -cne $dataPackerPreTrees[2]) {
				Write-Warning "Primary ThirdParty changed since the last DataPacker prebuild, or its stamp '$dataPackerStampPath' is missing or unreadable, so the DataPacker prebuild and the primary data refresh were skipped. A wrapper session start is needed to rebuild ThirdParty and refresh the data."
				$dataPackerPreTrees = $null
			}
		}
		if (-not $SharedDataOnly) { foreach ($configuration in @('Debug', 'Profile', 'Release')) { Invoke-BootstrapBuild $thirdPartySolution $configuration } }
		foreach ($configuration in @('Debug', 'Profile', 'Release')) {
			$library = Join-Path $thirdPartyOutput "ThirdParty.$configuration.lib"
			if (-not (Test-Path -LiteralPath $library -PathType Leaf) -or (Get-Item -LiteralPath $library).Length -eq 0) { throw "Required primary ThirdParty library is missing or empty after bootstrap: '$library'." }
		}
		# Best-effort DataPacker Release prebuild so a new session worktree seeds its Output\DataPacker.exe
		# by verified copy (Build-WorktreeDataPacker.ps1) instead of compiling from scratch. It links the
		# primary ThirdParty Release lib, built just above, or under -SharedDataOnly matched to the existing
		# stamp's ThirdParty tree, and runs inside this mutex so the exe and its stamp stay
		# atomic against peer bootstraps. Unlike the hard-fail AgentTools builds it only warns on failure:
		# the worktree-local fallback reproduces any real failure. The mutex excludes peer bootstraps but
		# not a landing or user VS build, so the stamp is written only when the before and after snapshots
		# (the three DataPacker/Common/ThirdParty tree hashes plus a clean DataPacker/Common/ThirdParty
		# state) are identical. The pre-snapshot is captured above, before the ThirdParty lib build, so it
		# covers every input DataPacker consumes; the pre-mutex Tools/, ThirdParty/, and Common/Sha256Hasher.h
		# dirty early-return (lines 50-59) plus this scoped gate keep the normal path clean.
		try {
			if ($null -ne $dataPackerPreTrees -and $dataPackerPreDirty.Count -eq 0) {
				Invoke-BootstrapBuild $dataPackerSolution 'Release' $dataPackerExe
				if (-not (Test-Path -LiteralPath $dataPackerExe -PathType Leaf)) { throw "DataPacker executable is missing after the prebuild: '$dataPackerExe'." }
				$dataPackerBytes = [IO.File]::ReadAllBytes($dataPackerExe)
				$dataPackerHash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($dataPackerBytes)).ToLowerInvariant()
				$dataPackerPostTrees = @(Invoke-AgentGit @('-C', $root, 'rev-parse', 'HEAD:DataPacker', 'HEAD:Common', 'HEAD:ThirdParty'))
				$dataPackerPostDirty = @(Invoke-AgentGit @('-C', $root, 'status', '--porcelain', '--untracked-files=all', '--', 'DataPacker', 'Common', 'ThirdParty'))
				if (($dataPackerPreTrees -join "`n") -cne ($dataPackerPostTrees -join "`n") -or $dataPackerPostDirty.Count -ne 0) {
					Write-Warning "Primary DataPacker/Common/ThirdParty changed during the DataPacker prebuild, so the stamp was not written; new worktrees will build DataPacker locally."
				}
				else {
					$dataPackerStamp = ($dataPackerPreTrees + $dataPackerHash + $dataPackerBytes.Length) -join "`n"
					[IO.File]::WriteAllText($dataPackerStampPath, $dataPackerStamp + "`n")
					$dataPackerPrebuiltClean = $true
				}
			}
			elseif ($null -ne $dataPackerPreTrees) {
				Write-Warning "Primary has uncommitted changes under DataPacker/, Common/, or ThirdParty/, so the DataPacker Release prebuild was skipped: $($dataPackerPreDirty -join '; ')."
			}
		}
		catch { Write-Warning "DataPacker Release prebuild failed, so new worktrees will build DataPacker locally: $($_.Exception.Message)" }
		# Refresh the primary's generated data that Shared-mode worktrees consume. Gated on the prebuild's
		# clean-snapshot stamp so the data is only ever produced by an exe just built and hash-verified from
		# committed sources. Deliberately outside the best-effort prebuild try above: a run failure must
		# abort session start rather than be swallowed by that warn-only catch. This can block for a long
		# time - on DataPacker's own PC-global "BrokenEngineDataPacker" mutex, on a rare full island bake, or
		# on the bounded wait below for an open client or server - while this bootstrap mutex is held, timing
		# out peer session starts; that cost is accepted. A peer whose own $WaitSeconds bootstrap-mutex
		# deadline expires during that wait fails with the existing mutex-timeout error, which is the
		# expected outcome there rather than a defect.
		$refreshedDataDirectories = [Collections.Generic.List[string]]::new()
		$gameProjects = @()
		if ($dataPackerPrebuiltClean) {
			# Only game projects whose client project is committed at HEAD are refreshed, so a project still
			# uncommitted in the primary can never fail session start.
			$discoveredProjects = @(Get-AgentGameProjects $root)
			$committedProjectFiles = @(Invoke-AgentGit (@('-C', $root, 'ls-tree', '--name-only', 'HEAD', '--') + @($discoveredProjects | ForEach-Object { "Projects/$_/Platforms/VisualStudio2026/$_.vcxproj" })))
			# A game's first export runs against an empty per-game DataPacker cache, a full Engine/Data
			# re-export that would hold this mutex for hours, so a game with no primary Output\Data yet is
			# skipped, which also leaves it out of the process wait below.
			$gameProjects = @(foreach ($committedProject in @($discoveredProjects | Where-Object { "Projects/$_/Platforms/VisualStudio2026/$_.vcxproj" -cin $committedProjectFiles })) {
				$committedDataDirectory = Join-Path $root "Projects\$committedProject\Platforms\VisualStudio2026\Output\Data"
				if (Test-Path -LiteralPath $committedDataDirectory -PathType Container) { $committedProject }
				else { Write-Warning "Skipped the primary data refresh for '$committedProject' because '$committedDataDirectory' does not exist yet. Run '$dataPackerExe' once by hand with no arguments, which packs every game; later session starts refresh it normally." }
			})
		}
		if ($gameProjects.Count -ne 0) {
			# A pack locked by a running game auto-cancels the export instead of prompting, which would hard-fail
			# session start, so wait every game out and skip the run only if one outlives the wait budget.
			$gameProcessNames = @($gameProjects | ForEach-Object { "$_*" })
			$gameProcesses = @(Get-Process -Name $gameProcessNames -ErrorAction SilentlyContinue)
			if ($gameProcesses.Count -ne 0) {
				Write-Host "Pausing the primary data refresh for up to $WaitSeconds seconds until the game exits: $(($gameProcesses | ForEach-Object { "$($_.ProcessName) ($($_.Id))" } | Sort-Object -Unique) -join '; ') is running."
				$gameStopwatch = [Diagnostics.Stopwatch]::StartNew()
				$gameDeadlineMilliseconds = $WaitSeconds * 1000
				while ($gameProcesses.Count -ne 0 -and $gameStopwatch.ElapsedMilliseconds -lt $gameDeadlineMilliseconds) {
					# Sleeping a full interval past the deadline would exceed the bounded wait.
					$remainingMilliseconds = $gameDeadlineMilliseconds - [int]$gameStopwatch.ElapsedMilliseconds
					Start-Sleep -Milliseconds ([Math]::Min(5000, $remainingMilliseconds))
					# A full re-snapshot, so a game started mid-wait also blocks the refresh.
					$gameProcesses = @(Get-Process -Name $gameProcessNames -ErrorAction SilentlyContinue)
				}
				if ($gameProcesses.Count -eq 0) { Write-Host "The game closed after $([int]$gameStopwatch.Elapsed.TotalSeconds) seconds, so the primary data refresh is proceeding." }
			}
			if ($gameProcesses.Count -ne 0) {
				Write-Warning "The $WaitSeconds second wait for the game to exit was exhausted, so the primary DataPacker run was skipped and parent data was left as-is: $(($gameProcesses | ForEach-Object { "$($_.ProcessName) ($($_.Id))" } | Sort-Object -Unique) -join '; ') is still running."
			}
			else {
				foreach ($gameProject in $gameProjects) {
					$gameDataDirectory = Join-Path $root "Projects\$gameProject\Platforms\VisualStudio2026\Output\Data"
					# The data stamp holds the primary commit this export ran from; a worktree game build uses this
					# Shared data only when its data inputs equal that commit's. The old stamp goes before the export,
					# and a failed delete aborts here, so data exported from inputs the stamp does not describe is never
					# vouched for. It sits beside Data, not inside it, to keep the Data inventory.
					$dataStampPath = Get-AgentSharedDataStampPath $root $gameProject
					if (Test-Path -LiteralPath $dataStampPath -PathType Leaf) { [IO.File]::Delete($dataStampPath) }
					# Best-effort like the DataPacker prebuild snapshots: a failure leaves the data unstamped.
					$stampCommit = $null
					try { $stampCommit = ([string](Invoke-AgentGit @('-C', $root, 'rev-parse', 'HEAD'))).Trim() }
					catch { Write-Warning "Could not read the primary HEAD commit, so the Shared data stamp will not be written: $($_.Exception.Message)"; $stampCommit = $null }
					$hadNoninteractive = Test-Path Env:BT_DATAPACKER_NONINTERACTIVE
					$previousNoninteractive = [Environment]::GetEnvironmentVariable('BT_DATAPACKER_NONINTERACTIVE', 'Process')
					try {
						# The child inherits this process's environment block, so the restore below keeps the flag
						# out of the agent CLI this bootstrap's caller launches afterwards.
						$env:BT_DATAPACKER_NONINTERACTIVE = '1'
						$dataPackerExitCode = Invoke-WorktreeCliTrackedProcess -Executable $dataPackerExe -ArgumentList @((Join-Path $root 'Engine\Data'), (Join-Path $root "Projects\$gameProject\Data"), $gameDataDirectory) -WorkingDirectory $root
					}
					finally {
						if ($hadNoninteractive) { $env:BT_DATAPACKER_NONINTERACTIVE = $previousNoninteractive }
						else { Remove-Item Env:BT_DATAPACKER_NONINTERACTIVE -ErrorAction SilentlyContinue }
					}
					if ($dataPackerExitCode -ne 0) { throw "The primary DataPacker run failed with exit code $dataPackerExitCode, so '$gameDataDirectory' may be incomplete. Fix the offending asset in the primary checkout outside an agent session (or run DataPacker there directly), then retry." }
					$refreshedDataDirectories.Add($gameDataDirectory)
					# The mutex excludes peer bootstraps but not a landing or user edit during the export, so the
					# stamp is written only when the data inputs show no committed or uncommitted difference from the
					# commit captured before it.
					if ($null -ne $stampCommit) {
						try {
							[string[]] $dataInputChanges = @(Get-AgentDataInputChanges $root $stampCommit $gameProject)
							if ($dataInputChanges.Count -eq 0) { [IO.File]::WriteAllText($dataStampPath, $stampCommit + "`n") }
							else {
								Write-Warning ("Primary data inputs differ from commit $stampCommit after the data refresh, so the Shared data stamp was not written and worktree game builds resolve Local until a clean refresh: $($dataInputChanges -join '; '). " +
									'When the export itself rewrote tracked data inputs (island BC textures, scene .PreExport markers), committing them in the primary restores Shared at the next refresh.')
							}
						}
						catch { Write-Warning "Could not compare the primary data inputs after the data refresh, so the Shared data stamp was not written and worktree game builds resolve Local until a clean refresh: $($_.Exception.Message)" }
					}
				}
			}
		}
		Write-Host ''
		if (-not $SharedDataOnly) { Write-Host "Built primary AgentTools and ThirdParty at '$worktreeCliOutput', '$agentHarnessOutput', and '$thirdPartyOutput'." }
		foreach ($refreshedDataDirectory in $refreshedDataDirectories) { Write-Host "Refreshed primary data at '$refreshedDataDirectory'." }
	}
	finally { if ($held) { $mutex.ReleaseMutex() }; $mutex.Dispose() }
}
finally {
	Unregister-WorktreeCliSession -RepositoryRoot $root -Owner $claimOwner
}
