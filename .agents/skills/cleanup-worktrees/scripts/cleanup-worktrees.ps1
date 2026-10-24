[CmdletBinding()]
param(
	[switch] $Preview
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-CanonicalPath {
	param([Parameter(Mandatory = $true)][string] $Path)
	return [IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
}

function Invoke-Git {
	param([Parameter(Mandatory = $true)][string[]] $Arguments)

	$output = @(& git @Arguments 2>&1 | ForEach-Object { $_.ToString() })
	return [pscustomobject] @{
		ExitCode = $LASTEXITCODE
		Output = $output
	}
}

function Get-WorktreeRecords {
	param([Parameter(Mandatory = $true)][string] $RepositoryRoot)

	$result = Invoke-Git -Arguments @('-C', $RepositoryRoot, 'worktree', 'list', '--porcelain')
	if ($result.ExitCode -ne 0) {
		throw "git worktree list failed: $($result.Output -join '; ')"
	}

	$records = @()
	$record = $null
	foreach ($line in $result.Output) {
		if ($line -like 'worktree *') {
			if ($null -ne $record) { $records += [pscustomobject] $record }
			$record = @{
				Path = $line.Substring(9)
				Branch = ''
				IsPrunable = $false
				PrunableReason = ''
			}
		}
		elseif ($line -like 'branch *') { $record.Branch = $line.Substring(7) -replace '^refs/heads/', '' }
		elseif ($line -like 'prunable*') {
			$record.IsPrunable = $true
			$record.PrunableReason = $line.Substring(8).Trim()
		}
	}
	if ($null -ne $record) { $records += [pscustomobject] $record }
	return $records
}

function Get-PrimaryState {
	param([Parameter(Mandatory = $true)][string] $RepositoryRoot)

	$branchResult = Invoke-Git -Arguments @('-C', $RepositoryRoot, 'branch', '--show-current')
	$headResult = Invoke-Git -Arguments @('-C', $RepositoryRoot, 'rev-parse', 'HEAD')
	return [pscustomobject] @{
		Branch = if ($branchResult.ExitCode -eq 0 -and $branchResult.Output.Count -eq 1) { $branchResult.Output[0].Trim() } else { '' }
		Head = if ($headResult.ExitCode -eq 0 -and $headResult.Output.Count -eq 1) { $headResult.Output[0].Trim() } else { '' }
	}
}

function Add-Retained {
	param(
		[Parameter(Mandatory = $true)][AllowEmptyCollection()][Collections.Generic.List[object]] $List,
		[Parameter(Mandatory = $true)][string] $Path,
		[Parameter(Mandatory = $true)][string] $Reason,
		[bool] $Expected = $false
	)

	$List.Add([pscustomobject] @{ Path = $Path; Reason = $Reason; Expected = $Expected })
}

function Remove-AgedFiles {
	param(
		[Parameter(Mandatory = $true)][string] $Root,
		[Parameter(Mandatory = $true)][DateTime] $Cutoff,
		[string] $RelativePathPattern = '',
		[Parameter(Mandatory = $true)][string] $Label,
		[Parameter(Mandatory = $true)][AllowEmptyCollection()][Collections.Generic.List[string]] $Residuals,
		[switch] $Preview
	)

	$files = @(Get-ChildItem -LiteralPath $Root -Recurse -File -Force |
		Where-Object { -not ($_.Attributes -band [IO.FileAttributes]::ReparsePoint) -and $_.LastWriteTime -le $Cutoff -and
			(-not $RelativePathPattern -or [IO.Path]::GetRelativePath($Root, $_.FullName) -cmatch $RelativePathPattern) })
	foreach ($file in $files) {
		if ($Preview) { $file; continue }
		try {
			Remove-Item -LiteralPath $file.FullName -Force -Confirm:$false -ErrorAction Stop
			$file
		}
		catch { $Residuals.Add("$Label file deletion failed for $($file.FullName)") }
	}
	if (-not $Preview) {
		# Descending ordinal order visits children before parents, so nested
		# just-emptied folders unwind bottom-up. Directory age uses CreationTime:
		# deleting contents refreshes the folder's LastWriteTime, which would
		# otherwise keep every just-emptied folder alive past this run.
		$directories = @(Get-ChildItem -LiteralPath $Root -Recurse -Directory -Force |
			Where-Object { -not ($_.Attributes -band [IO.FileAttributes]::ReparsePoint) -and $_.CreationTime -le $Cutoff } |
			ForEach-Object { $_.FullName })
		[Array]::Sort($directories, [StringComparer]::OrdinalIgnoreCase)
		[Array]::Reverse($directories)
		foreach ($directory in $directories) {
			if (@(Get-ChildItem -LiteralPath $directory -Force).Count -ne 0) { continue }
			try {
				Remove-Item -LiteralPath $directory -Force -Confirm:$false -ErrorAction Stop
				[IO.DirectoryInfo]::new($directory)
			}
			catch { $Residuals.Add("$Label directory deletion failed for $directory") }
		}
	}
}

function Write-Report {
	param(
		[Parameter(Mandatory = $true)][string] $Status,
		[Parameter(Mandatory = $true)][string] $RepositoryRoot,
		[Parameter(Mandatory = $true)][string] $PrimaryBranch,
		[Parameter(Mandatory = $true)][string] $PrimaryHead,
		[Parameter(Mandatory = $true)][AllowEmptyCollection()][Collections.Generic.List[object]] $Removed,
		[Parameter(Mandatory = $true)][AllowEmptyCollection()][Collections.Generic.List[object]] $Retained,
		[Parameter(Mandatory = $true)][AllowEmptyCollection()][Collections.Generic.List[string]] $TempRemoved,
		[Parameter(Mandatory = $true)][AllowEmptyCollection()][Collections.Generic.List[object]] $CodexLogsRemoved,
		[Parameter(Mandatory = $true)][AllowEmptyCollection()][object[]] $SnapshotRefs,
		[Parameter(Mandatory = $true)][AllowEmptyCollection()][string[]] $Residuals,
		[switch] $Preview
	)

	Write-Output "Cleanup: $Status"
	Write-Output 'Primary:'
	Write-Output "- Checkout: $RepositoryRoot"
	Write-Output "- Branch: $PrimaryBranch"
	Write-Output "- HEAD: $PrimaryHead"
	Write-Output 'Removed:'
	if ($Removed.Count -eq 0) { Write-Output '- none' }
	else {
		foreach ($item in $Removed) { Write-Output "- $($item.Path) | branch $($item.Branch): $($item.BranchStatus)" }
	}
	Write-Output 'Retained:'
	if ($Retained.Count -eq 0) { Write-Output '- none' }
	else {
		foreach ($item in $Retained) { Write-Output "- $($item.Path) | $($item.Reason)" }
	}
	Write-Output 'Temp removed:'
	if ($TempRemoved.Count -eq 0) { Write-Output '- none' }
	else {
		foreach ($item in $TempRemoved) { Write-Output "- $item" }
	}
	Write-Output 'Codex logs removed:'
	$codexLines = [Collections.Generic.List[string]]::new()
	foreach ($group in @($CodexLogsRemoved | Where-Object { $_ -is [IO.FileInfo] } | Group-Object -Property DirectoryName)) {
		$bytes = [long] ($group.Group | Measure-Object -Property Length -Sum).Sum
		$codexLines.Add("$($group.Name) | $($group.Count) files, $bytes bytes$(if ($Preview) { ' | eligible (preview)' })")
	}
	foreach ($item in @($CodexLogsRemoved | Where-Object { $_ -is [IO.DirectoryInfo] })) { $codexLines.Add("$($item.FullName) | emptied folder removed") }
	$codexLines.Sort([StringComparer]::OrdinalIgnoreCase)
	if ($codexLines.Count -eq 0) { Write-Output '- none' }
	else {
		foreach ($item in $codexLines) { Write-Output "- $item" }
	}
	Write-Output 'Snapshot refs:'
	if ($SnapshotRefs.Count -eq 0) { Write-Output '- none' }
	else {
		foreach ($item in $SnapshotRefs) { Write-Output "- $($item.Ref) | $($item.Object) | $($item.Date)" }
	}
	if ($Residuals.Count -eq 0) { Write-Output 'Residuals: none' }
	else { Write-Output "Residuals: $($Residuals -join '; ')" }
}

$removed = [Collections.Generic.List[object]]::new()
$retained = [Collections.Generic.List[object]]::new()
$tempRemoved = [Collections.Generic.List[string]]::new()
$codexLogsRemoved = [Collections.Generic.List[object]]::new()
$residuals = [Collections.Generic.List[string]]::new()
$snapshotRefs = @()
$repositoryRoot = ''
$primaryBranch = '<unproven>'
$primaryHead = '<unproven>'

try {
	$rootResult = Invoke-Git -Arguments @('rev-parse', '--show-toplevel')
	if ($rootResult.ExitCode -ne 0 -or $rootResult.Output.Count -ne 1) { throw 'cleanup-worktrees must run inside a Git repository' }
	$repositoryRoot = Get-CanonicalPath -Path $rootResult.Output[0].Trim()

	$gitDirResult = Invoke-Git -Arguments @('-C', $repositoryRoot, 'rev-parse', '--path-format=absolute', '--git-dir')
	$commonDirResult = Invoke-Git -Arguments @('-C', $repositoryRoot, 'rev-parse', '--path-format=absolute', '--git-common-dir')
	if ($gitDirResult.ExitCode -ne 0 -or $commonDirResult.ExitCode -ne 0) { throw 'unable to resolve Git directories' }
	$gitDir = Get-CanonicalPath -Path $gitDirResult.Output[0].Trim()
	$commonDir = Get-CanonicalPath -Path $commonDirResult.Output[0].Trim()
	if (-not $gitDir.Equals($commonDir, [StringComparison]::OrdinalIgnoreCase)) { throw 'cleanup-worktrees must run from the primary checkout' }

	$initialPrimary = Get-PrimaryState -RepositoryRoot $repositoryRoot
	$primaryBranch = $initialPrimary.Branch
	$primaryHead = $initialPrimary.Head
	if ([string]::IsNullOrWhiteSpace($primaryBranch) -or [string]::IsNullOrWhiteSpace($primaryHead)) { throw 'primary checkout must have an attached branch and readable HEAD' }

	$snapshotResult = Invoke-Git -Arguments @('-C', $repositoryRoot, 'for-each-ref', 'refs/codex/snapshots', '--format=%(refname)%09%(objectname)%09%(creatordate:iso-strict)')
	if ($snapshotResult.ExitCode -ne 0) { $residuals.Add('unable to enumerate Codex snapshot refs') }
	else {
		foreach ($line in $snapshotResult.Output) {
			$parts = $line -split "`t", 3
			if ($parts.Count -eq 3) { $snapshotRefs += [pscustomobject] @{ Ref = $parts[0]; Object = $parts[1]; Date = $parts[2] } }
		}
	}

	$cutoff = [DateTime]::Now.AddHours(-48)
	$eligible = @()
	foreach ($record in @(Get-WorktreeRecords -RepositoryRoot $repositoryRoot)) {
		$path = Get-CanonicalPath -Path $record.Path
		if ($path.Equals($repositoryRoot, [StringComparison]::OrdinalIgnoreCase)) { continue }

		if ($record.IsPrunable) { Add-Retained -List $retained -Path $path -Reason "prunable: $($record.PrunableReason)"; continue }
		if (-not (Test-Path -LiteralPath $path)) { Add-Retained -List $retained -Path $path -Reason 'path is missing'; continue }
		$created = (Get-Item -LiteralPath $path -Force).CreationTime
		if ($created -gt $cutoff) { Add-Retained -List $retained -Path $path -Reason 'younger than 48 hours' -Expected $true; continue }
		if ($Preview) { Add-Retained -List $retained -Path $path -Reason 'eligible (preview)' -Expected $true }
		else { $eligible += $path }
	}

	foreach ($candidate in $eligible) {
		$currentRecords = @(Get-WorktreeRecords -RepositoryRoot $repositoryRoot | Where-Object {
			(Get-CanonicalPath -Path $_.Path).Equals($candidate, [StringComparison]::OrdinalIgnoreCase)
		})
		if ($currentRecords.Count -ne 1) { Add-Retained -List $retained -Path $candidate -Reason 'registration changed before removal'; continue }
		$current = $currentRecords[0]
		if (-not (Test-Path -LiteralPath $candidate)) { Add-Retained -List $retained -Path $candidate -Reason 'path is missing before removal'; continue }
		if ((Get-Item -LiteralPath $candidate -Force).CreationTime -gt $cutoff) { Add-Retained -List $retained -Path $candidate -Reason 'younger than 48 hours before removal'; continue }

		$removeResult = Invoke-Git -Arguments @('-c', 'core.longpaths=true', '-C', $repositoryRoot, 'worktree', 'remove', '--force', $candidate)
		if ($removeResult.ExitCode -ne 0) {
			Add-Retained -List $retained -Path $candidate -Reason "git worktree remove failed: $($removeResult.Output -join '; ')"
			continue
		}
		if ([string]::IsNullOrWhiteSpace($current.Branch)) {
			$removed.Add([pscustomobject] @{ Path = $candidate; Branch = '<none>'; BranchStatus = 'no branch to delete' })
			continue
		}
		# Only wrapper-created session branches are disposable; any other branch is the user's.
		if (-not ($current.Branch.StartsWith('claude/', [StringComparison]::Ordinal) -or $current.Branch.StartsWith('codex/', [StringComparison]::Ordinal) -or $current.Branch.StartsWith('opencode/', [StringComparison]::Ordinal))) {
			$removed.Add([pscustomobject] @{ Path = $candidate; Branch = $current.Branch; BranchStatus = 'left (not a session branch)' })
			continue
		}
		$removedItem = [pscustomobject] @{ Path = $candidate; Branch = $current.Branch; BranchStatus = 'retained' }
		$removed.Add($removedItem)
		$branchResult = Invoke-Git -Arguments @('-C', $repositoryRoot, 'branch', '-D', '--', $current.Branch)
		if ($branchResult.ExitCode -ne 0) {
			$residuals.Add("branch deletion failed for $($current.Branch)")
			continue
		}
		$removedItem.BranchStatus = 'deleted'
	}

	$tempRoot = Get-CanonicalPath -Path (Join-Path $repositoryRoot 'Temp')
	if (Test-Path -LiteralPath $tempRoot -PathType Container) {
		Remove-AgedFiles -Root $tempRoot -Cutoff $cutoff -Label 'temp' -Residuals $residuals -Preview:$Preview |
			ForEach-Object { $tempRemoved.Add($(if ($Preview) { "$($_.FullName) | eligible (preview)" } else { $_.FullName })) }
		$tempRemoved.Sort([StringComparer]::OrdinalIgnoreCase)
	}

	$codexHome = Get-CanonicalPath -Path (Join-Path ([Environment]::GetFolderPath([Environment+SpecialFolder]::UserProfile)) '.codex')
	$codexRoot = Join-Path $codexHome 'sessions'
	if (Test-Path -LiteralPath $codexRoot -PathType Container) {
		if (((Get-Item -LiteralPath $codexHome -Force).Attributes -bor (Get-Item -LiteralPath $codexRoot -Force).Attributes) -band [IO.FileAttributes]::ReparsePoint) {
			$residuals.Add('Codex sessions root is a reparse point; Codex logs skipped')
		}
		else {
			Remove-AgedFiles -Root $codexRoot -Cutoff ([DateTime]::Now.AddDays(-30)) -RelativePathPattern '^\d{4}\\\d{2}\\\d{2}\\rollout-\d{4}-\d{2}-\d{2}T\d{2}-\d{2}-\d{2}-[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\.jsonl(\.zst)?$' -Label 'Codex log' -Residuals $residuals -Preview:$Preview |
				ForEach-Object { $codexLogsRemoved.Add($_) }
		}
	}

	$unexpectedRetained = @($retained | Where-Object { -not $_.Expected })
	if ($Preview) { $cleanupStatus = 'PREVIEW' }
	elseif ($residuals.Count -ne 0 -or $unexpectedRetained.Count -ne 0) { $cleanupStatus = 'PARTIAL' }
	else { $cleanupStatus = 'COMPLETED' }
	if ($unexpectedRetained.Count -ne 0) { $residuals.Add("$($unexpectedRetained.Count) worktree(s) retained for safety; see Retained") }
	Write-Report -Status $cleanupStatus -RepositoryRoot $repositoryRoot -PrimaryBranch $primaryBranch -PrimaryHead $primaryHead -Removed $removed -Retained $retained -TempRemoved $tempRemoved -CodexLogsRemoved $codexLogsRemoved -SnapshotRefs $snapshotRefs -Residuals $residuals.ToArray() -Preview:$Preview
	if ($cleanupStatus -eq 'PARTIAL') { exit 2 }
}
catch {
	$residuals.Add($_.Exception.Message)
	Write-Report -Status 'BLOCKED' -RepositoryRoot $(if ($repositoryRoot) { $repositoryRoot } else { '<unproven>' }) -PrimaryBranch $primaryBranch -PrimaryHead $primaryHead -Removed $removed -Retained $retained -TempRemoved $tempRemoved -CodexLogsRemoved $codexLogsRemoved -SnapshotRefs $snapshotRefs -Residuals $residuals.ToArray() -Preview:$Preview
	exit 1
}
