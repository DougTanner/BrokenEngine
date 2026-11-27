# Writer of the Change Workflow review-round objects: the snapshot builder, the close-commit writer, and the
# compare-and-swap advance of refs/worktree/review-rounds, plus the two message forms they carry. The forms, the
# chain, and the reading rule are documented in the Get-ChangeWorkflowSteps.ps1 header; SessionChangeInventory.psm1
# Get-ReviewRoundState reads them back. Imported by Get-ChangeWorkflowSteps.ps1 and Invoke-Sweep.ps1 -ReviewRound.
# A snapshot's tree is built in a temporary index, so nothing here writes a branch, the real index, or the working
# tree.

Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'AgentScriptCommon.psm1')

$script:ReviewRoundRef = 'refs/worktree/review-rounds'
$script:ZeroOid = '0000000000000000000000000000000000000000'
$script:Utf8 = [Text.UTF8Encoding]::new($false)

function Invoke-ReviewRoundGit([string] $Root, [string[]] $Arguments, [hashtable] $Environment = @{}, [bool] $AllowFailure = $false) {
	$start = [Diagnostics.ProcessStartInfo]::new()
	$start.FileName = 'git'
	$start.WorkingDirectory = $Root
	$start.UseShellExecute = $false
	$start.CreateNoWindow = $true
	$start.RedirectStandardOutput = $true
	$start.RedirectStandardError = $true
	$start.StandardOutputEncoding = $script:Utf8
	$start.StandardErrorEncoding = $script:Utf8
	# Keeps git status from refreshing the real index.
	$start.Environment['GIT_OPTIONAL_LOCKS'] = '0'
	foreach ($key in $Environment.Keys) { $start.Environment[$key] = $Environment[$key] }
	foreach ($argument in @('-C', $Root, '--no-pager') + $Arguments) { [void] $start.ArgumentList.Add($argument) }
	$process = [Diagnostics.Process]::new()
	$process.StartInfo = $start
	if (-not $process.Start()) { throw "Could not start git for: $($Arguments -join ' ')" }
	$stdoutTask = $process.StandardOutput.ReadToEndAsync()
	$stderrTask = $process.StandardError.ReadToEndAsync()
	$process.WaitForExit()
	$run = [pscustomobject] @{
		ExitCode = $process.ExitCode
		Stdout = $stdoutTask.GetAwaiter().GetResult()
		Stderr = $stderrTask.GetAwaiter().GetResult()
	}
	$process.Dispose()
	if (-not $AllowFailure -and $run.ExitCode -ne 0) { throw "git $($Arguments -join ' ') failed with exit $($run.ExitCode): $($run.Stderr.Trim())" }
	return $run
}

# The ref's stored value, which rev-parse reports even when its object is missing, or the zero OID when the ref is
# absent: the old value of the compare-and-swap.
function Get-ReviewRoundRefValue([string] $Root) {
	$run = Invoke-ReviewRoundGit $Root @('rev-parse', '--verify', '--quiet', $script:ReviewRoundRef) @{} $true
	if ($run.ExitCode -ne 0) { return $script:ZeroOid }
	return $run.Stdout.Trim()
}

# The tree of HEAD plus every path git status reports, both sides of a rename, staged through a temporary index.
# Status leaves out skip-worktree entries and ignored files, so a sparse-checkout path keeps the blob read-tree gave
# it and never reads as deleted.
function New-ReviewRoundTree([string] $Root) {
	$tokens = (Invoke-ReviewRoundGit $Root @('status', '--porcelain=v1', '-z', '--untracked-files=all')).Stdout -split "`0"
	$paths = [Collections.Generic.List[string]]::new()
	$index = 0
	while ($index -lt $tokens.Count) {
		$record = $tokens[$index++]
		if ([string]::IsNullOrEmpty($record)) { continue }
		if ($record.Length -lt 4) { throw "Malformed git status record '$record'." }
		$paths.Add($record.Substring(3))
		if ($record.Substring(0, 2) -cmatch '[RC]') { $paths.Add($tokens[$index++]) }
	}
	# A path on neither disk nor HEAD (added to the real index, then deleted) has nothing to stage, and git add
	# rejects a pathspec that matches nothing.
	$stage = @($paths | Sort-Object -Unique -CaseSensitive | Where-Object {
		(Test-Path -LiteralPath (Join-Path $Root ($_ -replace '/', [IO.Path]::DirectorySeparatorChar))) -or
		(Invoke-ReviewRoundGit $Root @('cat-file', '-e', "HEAD:$_") @{} $true).ExitCode -eq 0
	} | ForEach-Object { ":(literal)$_" })
	$name = 'BrokenEngineReviewRound-' + [guid]::NewGuid().ToString('N')
	$indexFile = Join-Path ([IO.Path]::GetTempPath()) "$name.index"
	$pathspecFile = Join-Path ([IO.Path]::GetTempPath()) "$name.pathspec"
	try {
		$environment = @{ GIT_INDEX_FILE = $indexFile }
		Invoke-ReviewRoundGit $Root @('read-tree', 'HEAD') $environment | Out-Null
		if ($stage.Count -gt 0) {
			[IO.File]::WriteAllText($pathspecFile, ($stage -join "`0"), $script:Utf8)
			Invoke-ReviewRoundGit $Root @('add', '-A', "--pathspec-from-file=$pathspecFile", '--pathspec-file-nul') $environment | Out-Null
		}
		return (Invoke-ReviewRoundGit $Root @('write-tree') $environment).Stdout.Trim()
	}
	finally { Remove-Item -LiteralPath $indexFile, $pathspecFile -Force -ErrorAction SilentlyContinue }
}

# One commit object on Tree with the single Parent; a close commit is this call with its snapshot's tree and the
# snapshot as parent.
function New-ReviewRoundCommit([string] $Root, [string] $Tree, [string] $Parent, [string] $Message) {
	$messageFile = Join-Path ([IO.Path]::GetTempPath()) ('BrokenEngineReviewRound-' + [guid]::NewGuid().ToString('N') + '.message')
	try {
		[IO.File]::WriteAllText($messageFile, $Message, $script:Utf8)
		return (Invoke-ReviewRoundGit $Root @('commit-tree', $Tree, '-p', $Parent, '-F', $messageFile)).Stdout.Trim()
	}
	finally { Remove-Item -LiteralPath $messageFile -Force -ErrorAction SilentlyContinue }
}

# True when the ref moved from Old (the zero OID: absent) to New. --no-deref writes the ref itself, so a symbolic
# review ref never moves the ref it points to.
function Update-ReviewRoundRef([string] $Root, [string] $New, [string] $Old) {
	return (Invoke-ReviewRoundGit $Root @('update-ref', '--no-deref', $script:ReviewRoundRef, $New, $Old) @{} $true).ExitCode -eq 0
}

# The Coretide clone's snapshot for round Number, chained on the clone's own ref tip when that resolves, else on the
# clone's HEAD, with the clone's session baseline (merge base of HEAD and upstream) and the ref's old value; null
# when RepositoryRoot holds no clone. Its ref is advanced by the caller. One read of the ref serves as both the parent
# and the old value, so a concurrent advance fails the compare-and-swap.
function New-ReviewRoundCoretideSnapshot([string] $RepositoryRoot, [int] $Number) {
	$clone = Get-AgentCoretideRepository $RepositoryRoot
	if ($null -eq $clone) { return $null }
	$old = Get-ReviewRoundRefValue $clone
	$tipResolves = $old -cne $script:ZeroOid -and (Invoke-ReviewRoundGit $clone @('cat-file', '-e', "$old^{commit}") @{} $true).ExitCode -eq 0
	$parent = if ($tipResolves) { $old } else { (Invoke-ReviewRoundGit $clone @('rev-parse', '--verify', 'HEAD^{commit}')).Stdout.Trim() }
	$baseline = (Invoke-ReviewRoundGit $clone @('merge-base', 'HEAD', '@{upstream}')).Stdout.Trim()
	$commit = New-ReviewRoundCommit $clone (New-ReviewRoundTree $clone) $parent "Review round $Number`n"
	return [pscustomobject] @{ Root = $clone; Commit = $commit; Baseline = $baseline; Old = $old }
}

function Format-ReviewRoundSnapshotMessage([int] $Number, [string] $Baseline, [string] $Phase, [string] $Kind, [int] $Tier, [string[]] $Owes, [object] $Coretide) {
	$lines = [Collections.Generic.List[string]]::new()
	foreach ($line in @("Review round $Number", '', "Round: $Number", "Baseline: $Baseline", "Phase: $Phase", "Kind: $Kind")) { $lines.Add($line) }
	if ($Tier -gt 0) { $lines.Add("Tier: $Tier") }
	foreach ($key in $Owes) { $lines.Add("Owes: $key") }
	if ($null -ne $Coretide) {
		$lines.Add("Coretide-Snapshot: $($Coretide.Commit)")
		$lines.Add("Coretide-Baseline: $($Coretide.Baseline)")
	}
	return ($lines -join "`n") + "`n"
}

# Results is an ordered list of key/severity pairs; a sweep close passes none.
function Format-ReviewRoundCloseMessage([int] $Number, [object[]] $Results, [string] $Next) {
	$lines = [Collections.Generic.List[string]]::new()
	foreach ($line in @("Review round $Number closed", '', "Closes: $Number")) { $lines.Add($line) }
	foreach ($result in $Results) { $lines.Add("Result: $($result.Key) $($result.Severity)") }
	$lines.Add("Next: $Next")
	return ($lines -join "`n") + "`n"
}

Export-ModuleMember -Function New-ReviewRoundTree, New-ReviewRoundCommit, Update-ReviewRoundRef, New-ReviewRoundCoretideSnapshot, Format-ReviewRoundSnapshotMessage, Format-ReviewRoundCloseMessage
