# Assembles the /codex-review dispatch prompt file and returns only a small receipt, so the diff
# evidence never passes through the calling manager's context. The fixed wording comes from
# ../references/prompt-template.md; the changed-file set comes from Get-SessionChangeInventory.ps1;
# the judgment text comes from the manager-authored -ScopeFile and is copied verbatim. The script
# writes a file, so it is manager-side only: the reviewer sandbox only reads the prompt.
# Diagnostics go to stderr; stdout carries only the receipt.
[CmdletBinding()]
param(
	[Parameter(Mandatory)][string] $RepositoryRoot,
	[Parameter(Mandatory)][string] $Baseline,
	[Parameter(Mandatory)][string] $AssignedSkill,
	[Parameter(Mandatory)][string] $ScopeFile,
	[Parameter(Mandatory)][string] $PromptPath,
	[ValidateRange(1, 3)][int] $RiskTier = 0,
	[string[]] $UntrackedPath,
	[string] $Head,
	[switch] $AdHocRole
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$sharedScripts = Join-Path $PSScriptRoot '..\..\..\scripts'
if (-not (Test-Path -LiteralPath (Join-Path $sharedScripts 'AgentScriptCommon.psm1'))) {
	$sharedScripts = Join-Path $PSScriptRoot '..\..\..\..\.agents\scripts'
}
Import-Module (Join-Path $sharedScripts 'AgentScriptCommon.psm1') -Force

$script:MaximumPromptBytes = 4 * 1024 * 1024
$script:MaximumMessageLength = 256
$script:MaximumContributors = 5
$script:BinaryProbeBytes = 8192
$script:CopyBufferBytes = 65536
# One ranking has to mix a tracked diff measured in changed lines with an untracked file measured in
# bytes, so untracked bytes are normalized by this rough bytes-per-line figure.
$script:BytesPerLine = 50

$script:Utf8 = [Text.UTF8Encoding]::new($false)
$script:Inventory = Join-Path $sharedScripts 'Get-SessionChangeInventory.ps1'
$script:Template = Join-Path $PSScriptRoot '..\references\prompt-template.md'
$script:Root = $null
$script:PromptFile = $null
$script:PromptStream = $null
$script:PromptCreated = $false
$script:TargetsFile = $null
$script:TargetsText = $null
$script:TargetsCreated = $false
$script:PromptBytes = 0
$script:SectionCount = 0
$script:DiffRange = @()
$script:DiffPath = @()
$script:HeadSha = $null
$script:Untracked = @()

$result = [ordered]@{
	schemaVersion = 'broken-engine-codex-review-prompt/v1'
	status = 'error'
	code = 'internal.error'
	message = 'Codex review prompt assembly did not run.'
	promptPath = $null
	targetsPath = $null
	promptBytes = 0
	fileCount = 0
	binaryExcluded = 0
	sectionsWritten = 0
}

function Write-PromptStderr([string] $Text) {
	$stream = [Console]::OpenStandardError()
	$bytes = $script:Utf8.GetBytes($Text)
	$stream.Write($bytes, 0, $bytes.Length)
	$stream.Flush()
}

function Complete-CodexReviewPrompt([int] $ExitCode, [string] $Status, [string] $Code, [string] $Message) {
	if ($null -ne $script:PromptStream) { $script:PromptStream.Dispose(); $script:PromptStream = $null }
	# A non-pass run leaves no partial prompt or targets file behind, and both paths are refused before
	# anything is created, so the caller's file is never the one removed here.
	if ($ExitCode -ne 0 -and $script:PromptCreated) {
		Remove-Item -LiteralPath $script:PromptFile -Force -ErrorAction SilentlyContinue
		$script:PromptCreated = $false
	}
	if ($ExitCode -ne 0 -and $script:TargetsCreated) {
		Remove-Item -LiteralPath $script:TargetsFile -Force -ErrorAction SilentlyContinue
		$script:TargetsCreated = $false
	}
	$result.status = $Status
	$result.code = $Code
	$result.message = if ($Message.Length -gt $script:MaximumMessageLength) { $Message.Substring(0, $script:MaximumMessageLength) } else { $Message }
	$result.promptPath = if ($script:PromptCreated) { $script:PromptFile } else { $null }
	$result.targetsPath = if ($script:TargetsCreated) { $script:TargetsFile } else { $null }
	$result.promptBytes = if ($script:PromptCreated) { $script:PromptBytes } else { 0 }
	$result.sectionsWritten = if ($script:PromptCreated) { $script:SectionCount } else { 0 }
	[Console]::Out.Write(($result | ConvertTo-Json -Depth 32 -Compress))
	exit $ExitCode
}

function Invoke-PromptGit([string[]] $Arguments, [bool] $AllowFailure = $false, [string] $StandardInput = '') {
	$start = [Diagnostics.ProcessStartInfo]::new()
	$start.FileName = 'git'
	$start.WorkingDirectory = $script:Root
	$start.UseShellExecute = $false
	$start.CreateNoWindow = $true
	$start.RedirectStandardOutput = $true
	$start.RedirectStandardError = $true
	$start.StandardOutputEncoding = $script:Utf8
	$start.StandardErrorEncoding = $script:Utf8
	if ($StandardInput.Length -gt 0) {
		$start.RedirectStandardInput = $true
		$start.StandardInputEncoding = $script:Utf8
	}
	$start.Environment['GIT_OPTIONAL_LOCKS'] = '0'
	foreach ($argument in @('-C', $script:Root, '--no-pager') + $Arguments) { [void] $start.ArgumentList.Add($argument) }
	$process = [Diagnostics.Process]::new()
	$process.StartInfo = $start
	if (-not $process.Start()) { throw "Could not start git for: $($Arguments -join ' ')" }
	$stdoutTask = $process.StandardOutput.ReadToEndAsync()
	$stderrTask = $process.StandardError.ReadToEndAsync()
	# Both reads are already draining, so the payload cannot deadlock behind a full pipe.
	if ($StandardInput.Length -gt 0) {
		$process.StandardInput.Write($StandardInput)
		$process.StandardInput.Close()
	}
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

function Get-PromptRelativePath([string] $Path) {
	# The same normalization the inventory applies to its -IncludeUntracked values, so a listed path
	# and an inventory entry compare as the same repository-relative path.
	$normalized = $Path.Replace('\', '/').Trim()
	if ([IO.Path]::IsPathRooted($normalized)) {
		$rootPosix = $script:Root.Replace('\', '/').TrimEnd('/') + '/'
		if ($normalized.StartsWith($rootPosix, [StringComparison]::OrdinalIgnoreCase)) { $normalized = $normalized.Substring($rootPosix.Length) }
	}
	if ($normalized.StartsWith('./')) { $normalized = $normalized.Substring(2) }
	return $normalized
}

function Get-PromptFullPath([string] $RelativePath) {
	return Join-Path $script:Root ($RelativePath -replace '/', [IO.Path]::DirectorySeparatorChar)
}

function Get-PromptFragment([string] $Text, [string] $Name) {
	$pattern = "(?s)<!-- fragment: $([Regex]::Escape($Name)) -->\r?\n(.*?)\r?\n<!-- end-fragment: $([Regex]::Escape($Name)) -->"
	$match = [Regex]::Match($Text, $pattern)
	if (-not $match.Success) {
		Complete-CodexReviewPrompt 1 'error' 'prompt.template-invalid' "The prompt template has no '$Name' fragment: '$($script:Template)'."
	}
	return $match.Groups[1].Value.Replace("`r`n", "`n")
}

function Test-PromptBinaryContent([string] $RelativePath) {
	# Content decides, not the extension: a binary payload named .md or .ps1 must never be inlined.
	# Git's own heuristic is a NUL byte in the first probe window.
	$stream = [IO.File]::Open((Get-PromptFullPath $RelativePath), [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
	try {
		$buffer = [byte[]]::new($script:BinaryProbeBytes)
		$read = $stream.Read($buffer, 0, $buffer.Length)
		for ($index = 0; $index -lt $read; $index++) {
			if ($buffer[$index] -eq 0) { return $true }
		}
		return $false
	}
	finally { $stream.Dispose() }
}

function Get-PromptContributor() {
	$contributors = [Collections.Generic.List[object]]::new()
	if ($script:DiffPath.Count -gt 0) {
		$arguments = @('diff', '--numstat', '-M', '--no-color', '--no-ext-diff') + $script:DiffRange + @('--') + @($script:DiffPath | ForEach-Object { ":(literal)$($_)" })
		$run = Invoke-PromptGit $arguments $true
		foreach ($line in ($run.Stdout -split "`n")) {
			if ([string]::IsNullOrWhiteSpace($line)) { continue }
			$fields = $line.TrimEnd("`r") -split "`t"
			if ($fields.Count -lt 3) { continue }
			if ($fields[0] -ceq '-') {
				$contributors.Add([pscustomobject] @{ Weight = 0; Text = "$($fields[2]) (binary)" })
				continue
			}
			$changed = [int] $fields[0] + [int] $fields[1]
			$contributors.Add([pscustomobject] @{ Weight = $changed; Text = "$($fields[2]) ($changed changed line(s))" })
		}
	}
	foreach ($entry in $script:Untracked) {
		if ($entry.Binary) { continue }
		$length = (Get-Item -LiteralPath (Get-PromptFullPath $entry.Path) -Force).Length
		$contributors.Add([pscustomobject] @{ Weight = [int] ($length / $script:BytesPerLine); Text = "$($entry.Path) ($length untracked byte(s))" })
	}
	$ranked = @($contributors | Sort-Object -Property @{ Expression = 'Weight'; Descending = $true }, @{ Expression = 'Text' } | Select-Object -First $script:MaximumContributors)
	if ($ranked.Count -eq 0) { return 'none' }
	return (($ranked | ForEach-Object { $_.Text }) -join '; ')
}

function Complete-PromptOverBudget() {
	$largest = Get-PromptContributor
	Complete-CodexReviewPrompt 2 'blocked' 'prompt.diff-too-large' "The assembled prompt passed the $($script:MaximumPromptBytes)-byte budget; evidence is never truncated, so split the review. Largest contributing paths: $largest"
}

function Add-PromptText([string] $Text) {
	$bytes = $script:Utf8.GetBytes($Text)
	$script:PromptStream.Write($bytes, 0, $bytes.Length)
	$script:PromptBytes += $bytes.Length
	if ($script:PromptBytes -gt $script:MaximumPromptBytes) { Complete-PromptOverBudget }
}

function Add-PromptStream([IO.Stream] $Source, [Diagnostics.Process] $Process) {
	$buffer = [byte[]]::new($script:CopyBufferBytes)
	while ($true) {
		$read = $Source.Read($buffer, 0, $buffer.Length)
		if ($read -le 0) { break }
		$script:PromptStream.Write($buffer, 0, $read)
		$script:PromptBytes += $read
		if ($script:PromptBytes -gt $script:MaximumPromptBytes) {
			# Stop draining the child before reporting, so an unbounded diff cannot keep writing.
			if ($null -ne $Process -and -not $Process.HasExited) { $Process.Kill() }
			Complete-PromptOverBudget
		}
	}
}

function Write-PromptSection([string] $Heading, [string] $Body) {
	if ($script:SectionCount -gt 0) { Add-PromptText "`n---`n`n" }
	Add-PromptText "# $Heading`n`n"
	if (-not [string]::IsNullOrEmpty($Body)) { Add-PromptText $Body }
	$script:SectionCount++
}

function Write-GuardrailBlock([string] $Guardrails) {
	Add-PromptText "## Guardrails`n`n"
	Add-PromptText $Guardrails
	Add-PromptText "`n"
}

function Get-ChangedFileSet([string[]] $Listed) {
	$arguments = @('-NoProfile', '-File', $script:Inventory, '-RepositoryRoot', $script:Root, '-Baseline', $Baseline)
	if (-not [string]::IsNullOrWhiteSpace($Head)) { $arguments += @('-Head', $Head) }
	if ($Listed.Count -gt 0) { $arguments += @('-IncludeUntracked', ($Listed -join ',')) }
	$start = [Diagnostics.ProcessStartInfo]::new()
	$start.FileName = (Get-Process -Id $PID).Path
	$start.WorkingDirectory = $script:Root
	$start.UseShellExecute = $false
	$start.CreateNoWindow = $true
	$start.RedirectStandardOutput = $true
	$start.RedirectStandardError = $true
	$start.StandardOutputEncoding = $script:Utf8
	$start.StandardErrorEncoding = $script:Utf8
	foreach ($argument in $arguments) { [void] $start.ArgumentList.Add($argument) }
	$process = [Diagnostics.Process]::new()
	$process.StartInfo = $start
	if (-not $process.Start()) { throw 'Could not start pwsh for the session change inventory.' }
	$stdoutTask = $process.StandardOutput.ReadToEndAsync()
	$stderrTask = $process.StandardError.ReadToEndAsync()
	$process.WaitForExit()
	$exitCode = $process.ExitCode
	$stdout = $stdoutTask.GetAwaiter().GetResult()
	$stderr = $stderrTask.GetAwaiter().GetResult()
	$process.Dispose()
	$inventory = $null
	if (-not [string]::IsNullOrWhiteSpace($stdout)) { try { $inventory = $stdout | ConvertFrom-Json -Depth 32 } catch { } }
	if ($null -eq $inventory) {
		if (-not [string]::IsNullOrWhiteSpace($stderr)) { Write-PromptStderr $stderr }
		Complete-CodexReviewPrompt 1 'error' 'prompt.inventory-failed' "The session change inventory returned no usable result (exit $exitCode)."
	}
	if ($exitCode -ne 0) {
		Complete-CodexReviewPrompt 2 'blocked' 'prompt.inventory-blocked' "The session change inventory blocked with $($inventory.code): $($inventory.message)"
	}
	# Evidence is complete or the run blocks: a capped entry list, or an untracked path the manager
	# did not name, would let the review report a clean result over bytes it never saw.
	if ($inventory.truncated) {
		Complete-CodexReviewPrompt 2 'blocked' 'prompt.inventory-truncated' 'The session change inventory truncated its entry list; narrow the baseline or the reviewed scope so every changed path is listed.'
	}
	if ($inventory.counts.unlistedUntracked -ne 0) {
		Complete-CodexReviewPrompt 2 'blocked' 'prompt.inventory-truncated' "The worktree holds $($inventory.counts.unlistedUntracked) untracked path(s) that -UntrackedPath does not name; name them or remove them so the evidence is complete."
	}
	return $inventory
}

function Write-PromptTargets([string[]] $Listed) {
	# /repo-code-review requires a supplied targets file and must not rebuild one, so the same
	# inventory that produced the evidence emits it here, next to the prompt.
	$arguments = @('-NoProfile', '-File', $script:Inventory, '-RepositoryRoot', $script:Root, '-Baseline', $Baseline, '-EmitTargets')
	if (-not [string]::IsNullOrWhiteSpace($Head)) { $arguments += @('-Head', $Head) }
	if ($Listed.Count -gt 0) { $arguments += @('-IncludeUntracked', ($Listed -join ',')) }
	$start = [Diagnostics.ProcessStartInfo]::new()
	$start.FileName = (Get-Process -Id $PID).Path
	$start.WorkingDirectory = $script:Root
	$start.UseShellExecute = $false
	$start.CreateNoWindow = $true
	$start.RedirectStandardOutput = $true
	$start.RedirectStandardError = $true
	$start.StandardOutputEncoding = $script:Utf8
	$start.StandardErrorEncoding = $script:Utf8
	foreach ($argument in $arguments) { [void] $start.ArgumentList.Add($argument) }
	$process = [Diagnostics.Process]::new()
	$process.StartInfo = $start
	if (-not $process.Start()) { throw 'Could not start pwsh for the targets file.' }
	$stdoutTask = $process.StandardOutput.ReadToEndAsync()
	$stderrTask = $process.StandardError.ReadToEndAsync()
	$process.WaitForExit()
	$exitCode = $process.ExitCode
	$stdout = $stdoutTask.GetAwaiter().GetResult()
	$stderr = $stderrTask.GetAwaiter().GetResult()
	$process.Dispose()
	if ($exitCode -ne 0) {
		# A targets run reports its outcome on stderr and leaves stdout empty, so the inventory's own
		# code is what names the fix here.
		$envelope = $null
		if (-not [string]::IsNullOrWhiteSpace($stderr)) { try { $envelope = $stderr | ConvertFrom-Json -Depth 32 } catch { } }
		if ($null -eq $envelope) {
			if (-not [string]::IsNullOrWhiteSpace($stderr)) { Write-PromptStderr $stderr }
			Complete-CodexReviewPrompt 1 'error' 'prompt.inventory-failed' "The targets run returned no usable result (exit $exitCode)."
		}
		Complete-CodexReviewPrompt 2 'blocked' 'prompt.inventory-blocked' "The targets run blocked with $($envelope.code): $($envelope.message)"
	}
	# A targets file with no path is a complete answer for a change that touches no C++ target.
	$script:TargetsText = $stdout
	$stream = [IO.File]::Open($script:TargetsFile, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
	$script:TargetsCreated = $true
	try {
		$bytes = $script:Utf8.GetBytes($script:TargetsText)
		$stream.Write($bytes, 0, $bytes.Length)
	}
	finally { $stream.Dispose() }
}

function Get-PromptHeadEntry([string] $RelativePath) {
	# ls-tree takes a pathspec, so a name holding '[', '*', or '?' would be read as a pattern and could
	# miss its own entry or answer for a different one; :(literal) makes the lookup name the path given.
	# An absent path is empty stdout with exit 0, so presence is decided from the listing, not the exit.
	$listed = @((Invoke-PromptGit @('ls-tree', $script:HeadSha, '--', ":(literal)$RelativePath")).Stdout -split "`n" | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
	if ($listed.Count -eq 0) { return $null }
	# '<mode> <type> <oid>' ahead of the tab that separates the path, which may itself hold spaces.
	$fields = ($listed[0].TrimEnd("`r") -split "`t")[0] -split ' '
	if ($fields.Count -lt 3) { return $null }
	# The permission bit is deliberately not part of the type: a Windows working tree cannot express it,
	# so 100644 and 100755 have to compare as the same kind of entry.
	$type = if ($fields[0] -ceq '120000') { 'link' } elseif ($fields[0] -ceq '160000') { 'commit' } elseif ($fields[0] -ceq '040000') { 'tree' } else { 'blob' }
	return [pscustomobject] @{ Type = $type; Oid = $fields[2] }
}

function Get-PromptWorkingEntry([string] $RelativePath) {
	# Windows resolves a path case-insensitively, so the losing side of a case-only rename would find the
	# winning side's file and report a difference that does not exist. Each component is matched against
	# the name the file system actually holds.
	$full = $script:Root
	foreach ($component in ($RelativePath -split '/')) {
		if ($component.Length -eq 0 -or -not [IO.Directory]::Exists($full)) { return $null }
		$matched = $null
		foreach ($candidate in [IO.Directory]::GetFileSystemEntries($full, $component)) {
			if ([IO.Path]::GetFileName($candidate) -ceq $component) { $matched = $candidate; break }
		}
		if ($null -eq $matched) { return $null }
		$full = $matched
	}
	$item = Get-Item -LiteralPath $full -Force
	# A named link target, not the reparse-point attribute: a cloud-storage placeholder carries that
	# attribute while standing in for the plain file it names no target for.
	if (-not [string]::IsNullOrEmpty($item.LinkTarget)) {
		# Git stores a symlink as its target text: forward slashes, no trailing newline. Hashing that
		# text is what keeps a symlink apart from a regular file holding the same characters.
		return [pscustomobject] @{ Type = 'link'; Oid = (Invoke-PromptGit @('hash-object', '--stdin') $false $item.LinkTarget.Replace('\', '/')).Stdout.Trim() }
	}
	if ($item -is [IO.DirectoryInfo]) {
		# A directory can only answer for a gitlink, whose working side is the submodule's checked-out
		# commit; a submodule that cannot report one leaves no oid and stays divergent.
		# rev-parse searches upwards, so a plain directory — a missing or deleted submodule — would
		# answer with this repository's own HEAD and could match the recorded gitlink. An empty prefix
		# is what proves the directory is a repository root rather than a path inside one; anything else
		# is not a gitlink at all, and compares as an absent working side.
		$prefix = Invoke-PromptGit @('-C', $full, 'rev-parse', '--show-prefix') $true
		if ($prefix.ExitCode -ne 0 -or $prefix.Stdout.Trim().Length -ne 0) { return $null }
		$run = Invoke-PromptGit @('-C', $full, 'rev-parse', 'HEAD') $true
		return [pscustomobject] @{ Type = 'commit'; Oid = $(if ($run.ExitCode -eq 0) { $run.Stdout.Trim() } else { $null }) }
	}
	# hash-object runs on the repository-relative path so the same attribute filters `git add` applied
	# when the candidate tree was written apply here, and the oids compare like-for-like.
	return [pscustomobject] @{ Type = 'blob'; Oid = (Invoke-PromptGit @('hash-object', '--', $RelativePath)).Stdout.Trim() }
}

function Test-PromptReviewedTreeClean() {
	# /verify-changes maps its acceptance evidence onto the -Head commit, so the only thing that would
	# put the review on a diff nobody approved is a reviewed path whose working-tree bytes differ from
	# that commit's tree. The index is deliberately not consulted: a landing candidate built into a
	# temporary index leaves the branch ref and the real index behind, and its tree is still the
	# reviewed one.
	if ($script:DiffPath.Count -eq 0) { return }
	$divergent = [Collections.Generic.List[string]]::new()
	$seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
	foreach ($path in $script:DiffPath) {
		if (-not $seen.Add($path)) { continue }
		# Absence on a side is itself a state, so it compares as a null type and a null oid: a path
		# present on exactly one side is a divergence, not a match. The entry kind is compared alongside
		# the oid, because a symlink and a regular file holding the same target text share one oid.
		$head = Get-PromptHeadEntry $path
		$working = Get-PromptWorkingEntry $path
		$headType = $(if ($null -eq $head) { $null } else { $head.Type })
		$workingType = $(if ($null -eq $working) { $null } else { $working.Type })
		$headOid = $(if ($null -eq $head) { $null } else { $head.Oid })
		$workingOid = $(if ($null -eq $working) { $null } else { $working.Oid })
		if ($headType -cne $workingType -or $headOid -cne $workingOid) { $divergent.Add($path) }
	}
	if ($divergent.Count -gt 0) {
		Complete-CodexReviewPrompt 2 'blocked' 'prompt.head-required' "The working tree still differs from -Head on reviewed path(s): $($divergent -join '; ')"
	}
}

function Write-DiffEvidence() {
	if ($script:DiffPath.Count -gt 0) {
		Add-PromptText "## Diff`n`n"
		$arguments = @('diff', '-M', '--no-color', '--no-ext-diff') + $script:DiffRange + @('--') + @($script:DiffPath | ForEach-Object { ":(literal)$($_)" })
		$start = [Diagnostics.ProcessStartInfo]::new()
		$start.FileName = 'git'
		$start.WorkingDirectory = $script:Root
		$start.UseShellExecute = $false
		$start.CreateNoWindow = $true
		$start.RedirectStandardOutput = $true
		$start.RedirectStandardError = $true
		$start.StandardErrorEncoding = $script:Utf8
		$start.Environment['GIT_OPTIONAL_LOCKS'] = '0'
		foreach ($argument in @('-C', $script:Root, '--no-pager') + $arguments) { [void] $start.ArgumentList.Add($argument) }
		$process = [Diagnostics.Process]::new()
		$process.StartInfo = $start
		if (-not $process.Start()) { throw 'Could not start git for the diff evidence.' }
		# The child's diff bytes go straight from its pipe into the prompt file: never into a
		# PowerShell variable, and never onto this script's stdout.
		$stderrTask = $process.StandardError.ReadToEndAsync()
		Add-PromptStream $process.StandardOutput.BaseStream $process
		$process.WaitForExit()
		$exitCode = $process.ExitCode
		$stderr = $stderrTask.GetAwaiter().GetResult()
		$process.Dispose()
		if ($exitCode -ne 0) {
			if (-not [string]::IsNullOrWhiteSpace($stderr)) { Write-PromptStderr $stderr }
			Complete-CodexReviewPrompt 1 'error' 'prompt.diff-failed' "git diff failed with exit $exitCode while streaming the evidence."
		}
		Add-PromptText "`n"
	}
	foreach ($entry in $script:Untracked) {
		if ($entry.Binary) { continue }
		Add-PromptText "## Untracked file: $($entry.Path)`n`n"
		$stream = [IO.File]::Open((Get-PromptFullPath $entry.Path), [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
		try { Add-PromptStream $stream $null }
		finally { $stream.Dispose() }
		Add-PromptText "`n"
	}
}

try {
	$listed = [Collections.Generic.List[string]]::new()
	# `pwsh -File` hands one literal string per token, so the documented comma-separated form is
	# split here rather than by the parameter binder.
	foreach ($path in @(@($UntrackedPath) -split ',')) {
		if ([string]::IsNullOrWhiteSpace($path)) { continue }
		$listed.Add($path.Trim())
	}
	if ($listed.Count -gt 0 -and -not [string]::IsNullOrWhiteSpace($Head)) {
		Complete-CodexReviewPrompt 2 'blocked' 'prompt.head-untracked-conflict' 'A commit-valued -Head has no untracked side, so -UntrackedPath cannot be classified; drop one of the two.'
	}
	$script:Root = Get-AgentCanonicalPath $RepositoryRoot
	if (-not [IO.Path]::IsPathRooted($RepositoryRoot) -or -not (Test-Path -LiteralPath $script:Root -PathType Container)) {
		Complete-CodexReviewPrompt 2 'blocked' 'prompt.repository-root-invalid' "-RepositoryRoot must be an existing absolute directory: '$RepositoryRoot'."
	}
	# An assigned skill that names no skill file leaves the reviewer with only the scope text as its
	# contract, so an unknown name has to be the caller's deliberate choice.
	if (-not $AdHocRole) {
		$skillFile = Join-Path $script:Root (Join-Path '.agents' (Join-Path 'skills' (Join-Path $AssignedSkill 'SKILL.md')))
		if (-not (Test-Path -LiteralPath $skillFile -PathType Leaf)) {
			Complete-CodexReviewPrompt 2 'blocked' 'prompt.assigned-skill-unknown' "-AssignedSkill names no skill file '$skillFile'; pass -AdHocRole for a descriptive reviewer role that has none."
		}
	}
	# Case-insensitive, because the skill-file check above already accepted any casing the file system
	# resolves; a case-sensitive match here would silently drop the special-skill contract.
	if ($AssignedSkill -eq 'verify-changes' -and [string]::IsNullOrWhiteSpace($Head)) {
		Complete-CodexReviewPrompt 2 'blocked' 'prompt.head-required' '/verify-changes reviews the committed landing diff and requires a commit-valued -Head.'
	}
	if (-not (Test-Path -LiteralPath $ScopeFile -PathType Leaf)) {
		Complete-CodexReviewPrompt 1 'error' 'prompt.scope-file-missing' "-ScopeFile must be an existing file holding the manager-authored scope text: '$ScopeFile'."
	}
	$script:PromptFile = [IO.Path]::GetFullPath($PromptPath)
	if (Test-Path -LiteralPath $script:PromptFile) {
		Complete-CodexReviewPrompt 2 'blocked' 'prompt.path-exists' "-PromptPath already exists and is never overwritten: '$($script:PromptFile)'."
	}
	if ($AssignedSkill -eq 'repo-code-review') {
		$script:TargetsFile = $script:PromptFile + '.targets.json'
		if (Test-Path -LiteralPath $script:TargetsFile) {
			Complete-CodexReviewPrompt 2 'blocked' 'prompt.path-exists' "The targets sibling of -PromptPath already exists and is never overwritten: '$($script:TargetsFile)'."
		}
	}
	if (-not (Test-Path -LiteralPath $script:Template -PathType Leaf)) {
		Complete-CodexReviewPrompt 1 'error' 'prompt.template-invalid' "The prompt template is missing: '$($script:Template)'."
	}
	$templateText = [IO.File]::ReadAllText($script:Template)
	$roleInstruction = (Get-PromptFragment $templateText 'role-instruction').Replace('{{ASSIGNED_SKILL}}', $AssignedSkill)
	$guardrails = Get-PromptFragment $templateText 'guardrails'
	$outputContract = Get-PromptFragment $templateText 'output-contract'
	$scopeText = [IO.File]::ReadAllText($ScopeFile)

	# Not $inventory: a script-scope local by that name would overwrite the $script:Inventory script
	# path, which the targets run below still needs.
	$changeSet = Get-ChangedFileSet $listed.ToArray()
	$listedSet = [Collections.Generic.HashSet[string]]::new([string[]] @($listed | ForEach-Object { Get-PromptRelativePath $_ }))
	$diffPath = [Collections.Generic.List[string]]::new()
	$untracked = [Collections.Generic.List[object]]::new()
	$fileLine = [Collections.Generic.List[string]]::new()
	$binaryExcluded = 0
	foreach ($entry in @($changeSet.entries)) {
		if ($null -eq $entry.baseline -and $listedSet.Contains($entry.path)) {
			# The inventory classifies by extension first; the content probe is what keeps a binary
			# payload with a textual name out of the prompt.
			$isBinary = $entry.class -ceq 'binary' -or (Test-PromptBinaryContent $entry.path)
			if ($isBinary) { $binaryExcluded++ }
			$untracked.Add([pscustomobject] @{ Path = $entry.path; Binary = $isBinary })
			$fileLine.Add("$($entry.status) $($entry.path) — untracked$(if ($isBinary) { ', binary: path and status only' } else { ', contents below' })")
			continue
		}
		# A rename contributes both sides to the pathspec so both appear in the evidence.
		$diffPath.Add($entry.path)
		if ($null -ne $entry.oldPath) {
			$diffPath.Add($entry.oldPath)
			$fileLine.Add("$($entry.status) $($entry.path) — was $($entry.oldPath)")
			continue
		}
		$fileLine.Add("$($entry.status) $($entry.path)")
	}
	# A named path the inventory never reported — ignored, already tracked and unchanged, or absent —
	# would leave the prompt silently missing evidence the manager believes it carries.
	$untrackedSet = [Collections.Generic.HashSet[string]]::new([string[]] @($untracked | ForEach-Object { $_.Path }))
	$missing = @($listedSet | Where-Object { -not $untrackedSet.Contains($_) })
	if ($missing.Count -gt 0) {
		Complete-CodexReviewPrompt 2 'blocked' 'prompt.untracked-path-unknown' "The inventory reports no untracked entry for: $(($missing | Sort-Object) -join ', ')"
	}
	$script:DiffPath = $diffPath.ToArray()
	$script:Untracked = $untracked.ToArray()
	$script:HeadSha = $changeSet.headSha
	$script:DiffRange = if ([string]::IsNullOrEmpty($changeSet.headSha)) { @($changeSet.baselineSha) } else { @($changeSet.baselineSha, $changeSet.headSha) }
	if ($AssignedSkill -eq 'verify-changes') { Test-PromptReviewedTreeClean }
	if ($null -ne $script:TargetsFile) { Write-PromptTargets $listed.ToArray() }

	$script:PromptStream = [IO.File]::Open($script:PromptFile, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
	$script:PromptCreated = $true

	Write-PromptSection '(a) Role' ($roleInstruction + "`n`n")
	Write-GuardrailBlock $guardrails

	$scopeBody = ''
	if ($RiskTier -ne 0) { $scopeBody += "Risk tier: $RiskTier`n`n" }
	$scopeBody += $scopeText
	if (-not $scopeBody.EndsWith("`n")) { $scopeBody += "`n" }
	Write-PromptSection '(b) Scope' $scopeBody

	$headText = if ([string]::IsNullOrEmpty($changeSet.headSha)) { 'working tree' } else { $changeSet.headSha }
	$evidence = "Baseline: $($changeSet.baselineSha)`nHead: $headText`nChanged files ($($fileLine.Count)):`n"
	foreach ($line in $fileLine) { $evidence += "- $line`n" }
	$evidence += "`n"
	if ($script:TargetsCreated) {
		$evidence += "Targets file: $($script:TargetsFile)`n`n$($script:TargetsText)`n"
	}
	Write-PromptSection '(c) Evidence' $evidence
	Write-DiffEvidence

	Write-PromptSection '(d) Output contract' ($outputContract + "`n")

	$result.fileCount = $fileLine.Count
	$result.binaryExcluded = $binaryExcluded
	Complete-CodexReviewPrompt 0 'pass' 'ok' "Wrote a $($script:SectionCount)-section review prompt covering $($fileLine.Count) changed file(s)."
}
catch {
	Complete-CodexReviewPrompt 1 'error' 'internal.error' $_.Exception.Message
}
