# Deletes one Plan's support folder (Documents/Plans/AGENTS.md `## Plan files`); this script is the only route that
# deletes a support folder. The folder is derived from -Plan alone, every check runs before the single literal-path
# delete, and the index is never touched.
[CmdletBinding()]
param(
	[Parameter(Mandatory)][string] $Plan,
	[string] $RepositoryRoot
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'AgentScriptCommon.psm1') -Force -DisableNameChecking

$result = [ordered]@{
	schemaVersion = 'broken-engine-plan-support-folder/v1'
	status = 'error'
	code = 'internal.error'
	message = 'Support folder deletion did not run.'
	plan = $Plan
	folder = $null
	paths = @()
	violations = @()
}

function Complete-SupportFolder([int] $ExitCode, [string] $Status, [string] $Code, [string] $Message) {
	$result.status = $Status
	$result.code = $Code
	$result.message = $Message
	[Console]::Out.Write(($result | ConvertTo-Json -Depth 8 -Compress))
	exit $ExitCode
}

try {
	if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
		$topLevel = @(& git rev-parse --show-toplevel 2>$null)
		if ($LASTEXITCODE -ne 0 -or $topLevel.Count -eq 0 -or [string]::IsNullOrWhiteSpace([string] $topLevel[0])) {
			Complete-SupportFolder 1 'error' 'git.no-repository' 'The current directory is not inside a Git repository and no -RepositoryRoot was supplied.'
		}
		$RepositoryRoot = ([string] $topLevel[0]).Trim()
	}
	$root = [IO.Path]::GetFullPath($RepositoryRoot)

	# The New-PlanFile.ps1 name and area grammar, which admits no empty, '.', '..', ':', or wildcard segment.
	$segments = $Plan.Split('/')
	if ($segments.Count -ne 4 -or $segments[0] -cne 'Documents' -or $segments[1] -cne 'Plans' -or $segments[2] -cnotmatch '^[A-Za-z0-9]+\z' -or
		$segments[3] -cnotmatch '^[A-Z][A-Za-z0-9]*\.md\z' -or $segments[3] -ceq 'AGENTS.md') {
		Complete-SupportFolder 2 'blocked' 'input.plan-invalid' "-Plan must be exactly Documents/Plans/<Area>/<Name>.md in the New-PlanFile.ps1 grammar: '$Plan'."
	}
	$area = $segments[2]
	$name = $segments[3].Substring(0, $segments[3].Length - '.md'.Length)
	$result.folder = "Documents/Plans/$area/$name"
	$plansFull = [IO.Path]::Combine($root, 'Documents', 'Plans')
	$areaFull = [IO.Path]::Combine($plansFull, $area)
	$folderFull = [IO.Path]::Combine($areaFull, $name)

	if (-not (Test-Path -LiteralPath $folderFull)) { Complete-SupportFolder 0 'pass' 'absent' "No support folder exists for '$Plan'." }
	$item = Get-Item -LiteralPath $folderFull -Force
	# The file system resolves names case-insensitively, so a wrongly cased -Plan would otherwise reach another folder.
	$areaListed = @([IO.Directory]::GetFileSystemEntries($plansFull) | ForEach-Object { [IO.Path]::GetFileName($_) }) -ccontains $area
	$folderListed = @([IO.Directory]::GetFileSystemEntries($areaFull) | ForEach-Object { [IO.Path]::GetFileName($_) }) -ccontains $name
	if (-not $item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 -or -not $areaListed -or -not $folderListed) {
		Complete-SupportFolder 2 'blocked' 'folder.unsafe' "The support folder for '$Plan' is not an ordinary directory named exactly '$($result.folder)'; nothing was deleted."
	}

	$violations = @(Get-AgentPlansLayoutViolation -RepositoryRoot $root -Folder $result.folder)
	if ($violations.Count -ne 0) {
		$result.violations = $violations
		Complete-SupportFolder 2 'blocked' 'folder.violation' "The support folder '$($result.folder)' holds $($violations.Count) layout violation(s), first $($violations[0].rule) '$($violations[0].path)'; nothing was deleted."
	}

	# With no uncommitted entry, the folder's index listing equals its files on disk.
	$listing = Invoke-AgentProcess 'git' @('-C', $root, 'ls-files', '-z', '--', (':(literal)' + $result.folder + '/')) $root
	if ($listing.ExitCode -ne 0) { throw "git ls-files failed for '$($result.folder)': $($listing.Stderr.Trim())" }
	$result.paths = @(($listing.Stdout -split "`0") | Where-Object { $_.Length -ne 0 })

	Remove-Item -LiteralPath $folderFull -Recurse -Force -Confirm:$false -ErrorAction Stop
	Complete-SupportFolder 0 'pass' 'ok' "Deleted the support folder '$($result.folder)'."
}
catch {
	Complete-SupportFolder 1 'error' 'internal.error' $_.Exception.Message
}
