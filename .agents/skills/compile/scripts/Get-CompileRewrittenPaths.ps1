# Lists the data-input tracked paths first modified during a /compile build window. Without -Compare,
# writes the set of paths `git status --porcelain=v1 -z --untracked-files=no` reports in the current
# directory's worktree to -SnapshotPath, one per line. With -Compare, prints in ordinal order, one per
# line, each path the current read reports that the snapshot lacks and that Test-AgentDataInputPath
# accepts for some game project, and nothing when none. A build writes tracked files only under those
# data inputs, so every path it rewrites is printed, but so is a concurrent edit to another data input;
# a concurrent edit elsewhere is not. A path modified before the snapshot is never printed, even when a
# build rewrote it again.
# Git runs with GIT_OPTIONAL_LOCKS=0 so the read never takes the index lock a concurrent build or
# Git call needs. Changes nothing but the snapshot file. Exit 0 on success, 1 with stderr on any
# failure.
[CmdletBinding()]
param(
	[Parameter(Mandatory)][string] $SnapshotPath,
	[switch] $Compare
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$sharedScripts = Join-Path $PSScriptRoot '..\..\..\scripts'
if (-not (Test-Path -LiteralPath (Join-Path $sharedScripts 'AgentScriptCommon.psm1'))) {
	$sharedScripts = Join-Path $PSScriptRoot '..\..\..\..\.agents\scripts'
}
Import-Module (Join-Path $sharedScripts 'AgentScriptCommon.psm1') -Force

function Get-ModifiedTrackedPaths([string] $WorkingDirectory) {
	$run = Invoke-AgentProcess 'git' @('status', '--porcelain=v1', '-z', '--untracked-files=no') $WorkingDirectory
	if ($run.ExitCode -ne 0) { throw "git status exited $($run.ExitCode): $($run.Stderr.Trim())" }

	# Each -z entry is `XY <path>`; a rename or copy entry is followed by its source path as its own
	# entry, which is not a path in the worktree.
	$paths = [System.Collections.Generic.List[string]]::new()
	$entries = $run.Stdout.Split([char] 0)
	for ($index = 0; $index -lt $entries.Length; $index++) {
		$entry = $entries[$index]
		if ($entry.Length -eq 0) { continue }
		if ($entry.Length -lt 4) { throw "Unexpected git status entry '$entry'" }
		$paths.Add($entry.Substring(3))
		if ($entry.Substring(0, 2) -cmatch '[RC]') { $index++ }
	}

	$sorted = $paths.ToArray()
	[Array]::Sort($sorted, [StringComparer]::Ordinal)
	return , $sorted
}

try {
	$workingDirectory = (Get-Location).ProviderPath
	$snapshotFullPath = [System.IO.Path]::GetFullPath($SnapshotPath, $workingDirectory)
	$current = Get-ModifiedTrackedPaths $workingDirectory
	$utf8 = [Text.UTF8Encoding]::new($false)
	if (-not $Compare) {
		$text = (($current | ForEach-Object { "$_`n" }) -join '')
		[System.IO.File]::WriteAllText($snapshotFullPath, $text, $utf8)
		exit 0
	}

	$snapshot = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
	foreach ($line in [System.IO.File]::ReadAllText($snapshotFullPath, $utf8).Split("`n")) {
		if ($line.Length -gt 0) { [void] $snapshot.Add($line) }
	}

	$projects = Get-AgentGameProjects $workingDirectory
	foreach ($path in $current) {
		if ($snapshot.Contains($path)) { continue }
		if (@($projects | Where-Object { Test-AgentDataInputPath $path $_ }).Count -gt 0) { [Console]::Out.WriteLine($path) }
	}

	exit 0
}
catch {
	[Console]::Error.WriteLine($_.Exception.Message)
	exit 1
}
