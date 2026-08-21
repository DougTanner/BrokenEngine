# Deterministic fixtures for New-CodexReviewPrompt.ps1 against throwaway Git repositories under the
# operating system temp directory. Every repository, scope file, and prompt path is created and
# removed by this harness, and no case points the script at a real checkout. Scope files and prompt
# paths live outside the fixture repository, because an unnamed untracked path inside it is itself a
# blocked result. Exit 0 means every case passed.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$script:Failures = [Collections.Generic.List[string]]::new()
$script:Roots = [Collections.Generic.List[string]]::new()
$script:Utf8 = [Text.UTF8Encoding]::new($false)
$script:Script = Join-Path $PSScriptRoot 'New-CodexReviewPrompt.ps1'
$script:Template = Join-Path $PSScriptRoot '..\references\prompt-template.md'
$script:PwshPath = (Get-Process -Id $PID).Path
$script:Scratch = Join-Path ([IO.Path]::GetTempPath()) ('codex-prompt-scratch-' + [Guid]::NewGuid().ToString('n').Substring(0, 8))

function Assert-True([bool] $Condition, [string] $Name) {
	if (-not $Condition) { $script:Failures.Add($Name); Write-Host "FAIL $Name" } else { Write-Host "pass $Name" }
}

function Assert-Equal($Expected, $Actual, [string] $Name) {
	Assert-True ("$Expected" -ceq "$Actual") "$Name (expected '$Expected', was '$Actual')"
}

function Get-FragmentText([string] $Name) {
	$text = [IO.File]::ReadAllText($script:Template)
	$pattern = "(?s)<!-- fragment: $([Regex]::Escape($Name)) -->\r?\n(.*?)\r?\n<!-- end-fragment: $([Regex]::Escape($Name)) -->"
	$match = [Regex]::Match($text, $pattern)
	if (-not $match.Success) { throw "The template has no '$Name' fragment." }
	return $match.Groups[1].Value.Replace("`r`n", "`n")
}

function Get-PromptSectionBody([string] $Prompt, [string] $Heading, [string] $NextBoundary) {
	# Extraction between the prompt format's own fixed boundaries, so extra or overriding text placed
	# next to a fragment lands inside the returned block and fails the exact comparison. An empty
	# boundary means the block runs to the end of the prompt.
	$startMarker = "# $Heading`n`n"
	$start = $Prompt.IndexOf($startMarker, [StringComparison]::Ordinal)
	if ($start -lt 0) { return $null }
	$body = $Prompt.Substring($start + $startMarker.Length)
	if ([string]::IsNullOrEmpty($NextBoundary)) { return $body }
	$end = $body.IndexOf($NextBoundary, [StringComparison]::Ordinal)
	if ($end -lt 0) { return $null }
	return $body.Substring(0, $end)
}

function New-FixtureRoot([string] $Name) {
	$root = Join-Path ([IO.Path]::GetTempPath()) ("codex-prompt-fixture-$Name-" + [Guid]::NewGuid().ToString('n').Substring(0, 8))
	[void] (New-Item -ItemType Directory -Path $root -Force)
	$script:Roots.Add($root)
	Invoke-FixtureGit $root @('init', '--quiet', '--initial-branch=main')
	Invoke-FixtureGit $root @('config', 'user.name', 'Fixture')
	Invoke-FixtureGit $root @('config', 'user.email', 'fixture@example.invalid')
	Invoke-FixtureGit $root @('config', 'core.autocrlf', 'false')
	Invoke-FixtureGit $root @('config', 'commit.gpgsign', 'false')
	return $root
}

function Get-FixtureGitText([string] $Root, [string[]] $Arguments) {
	$output = @(& git -C $Root --no-pager @Arguments 2>&1)
	if ($LASTEXITCODE -ne 0) { throw "git $($Arguments -join ' ') failed in '$Root': $($output -join '; ')" }
	return ($output -join "`n")
}

function Invoke-FixtureGit([string] $Root, [string[]] $Arguments) {
	[void] (Get-FixtureGitText $Root $Arguments)
}

function Set-FixtureText([string] $Root, [string] $RelativePath, [string] $Text) {
	$full = Join-Path $Root ($RelativePath -replace '/', [IO.Path]::DirectorySeparatorChar)
	[void] (New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($full)) -Force)
	[IO.File]::WriteAllBytes($full, $script:Utf8.GetBytes($Text))
}

function Set-FixtureBytes([string] $Root, [string] $RelativePath, [byte[]] $Bytes) {
	$full = Join-Path $Root ($RelativePath -replace '/', [IO.Path]::DirectorySeparatorChar)
	[void] (New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($full)) -Force)
	[IO.File]::WriteAllBytes($full, $Bytes)
}

function Add-FixtureSkill([string] $Root, [string[]] $Name) {
	# -AssignedSkill resolves against the repository under review, so every fixture skill file lives in
	# the fixture repository and is committed with the baseline.
	foreach ($skill in $Name) { Set-FixtureText $Root ".agents/skills/$skill/SKILL.md" "# $skill`n" }
}

function Get-TargetsPath([string] $PromptPath) {
	return ([IO.Path]::GetFullPath($PromptPath) + '.targets.json')
}

function New-ScratchFile([string] $Name, [string] $Text) {
	$path = Join-Path $script:Scratch ("$Name-" + [Guid]::NewGuid().ToString('n').Substring(0, 8) + '.md')
	[IO.File]::WriteAllBytes($path, $script:Utf8.GetBytes($Text))
	return $path
}

function New-ScratchPath([string] $Name) {
	return Join-Path $script:Scratch ("$Name-" + [Guid]::NewGuid().ToString('n').Substring(0, 8) + '.prompt.md')
}

function Invoke-PromptScript([string[]] $Arguments) {
	$start = [Diagnostics.ProcessStartInfo]::new()
	$start.FileName = $script:PwshPath
	$start.WorkingDirectory = [IO.Path]::GetTempPath()
	$start.UseShellExecute = $false
	$start.CreateNoWindow = $true
	$start.RedirectStandardOutput = $true
	$start.RedirectStandardError = $true
	$start.StandardOutputEncoding = $script:Utf8
	$start.StandardErrorEncoding = $script:Utf8
	foreach ($argument in @('-NoProfile', '-File', $script:Script) + $Arguments) { [void] $start.ArgumentList.Add($argument) }
	$process = [Diagnostics.Process]::new()
	$process.StartInfo = $start
	if (-not $process.Start()) { throw 'Could not start pwsh for the prompt script.' }
	$stdoutTask = $process.StandardOutput.ReadToEndAsync()
	$stderrTask = $process.StandardError.ReadToEndAsync()
	$process.WaitForExit()
	$run = [ordered]@{
		ExitCode = $process.ExitCode
		Stdout = $stdoutTask.GetAwaiter().GetResult()
		Stderr = $stderrTask.GetAwaiter().GetResult()
		Json = $null
	}
	$process.Dispose()
	if (-not [string]::IsNullOrWhiteSpace($run.Stdout)) { try { $run.Json = $run.Stdout | ConvertFrom-Json -Depth 32 } catch { } }
	return [pscustomobject] $run
}

# --- Repository A: the assembled prompt ---------------------------------------------------------

$script:BinaryMarker = 'BINARY-PAYLOAD-MARKER'
$script:ScopeText = "Files and regions: Engine/Source/Keep.cpp lines 1-5.`nFocus: the changed bytes only.`nResiduals: none.`n"

function New-VerifyScopeText([string] $Baseline, [string] $Head) {
	return ($script:ScopeText + "Baseline: $Baseline`nHead: $Head`n")
}

function New-RepositoryA() {
	$root = New-FixtureRoot 'prompt'
	Add-FixtureSkill $root @('repo-code-review', 'scope-review', 'plan-audit')
	Set-FixtureText $root 'Engine/Source/Keep.cpp' "1`n2`n3`n4`n5`n"
	Set-FixtureText $root 'Engine/Source/Old.cpp' "int Old() { return 1; }`n"
	Invoke-FixtureGit $root @('add', '--all')
	Invoke-FixtureGit $root @('commit', '--quiet', '-m', 'baseline')
	$baseline = (Get-FixtureGitText $root @('rev-parse', 'HEAD')).Trim()
	Set-FixtureText $root 'Engine/Source/Keep.cpp' "1`n2`nthree`n4`n5`n"
	Invoke-FixtureGit $root @('mv', 'Engine/Source/Old.cpp', 'Engine/Source/New.cpp')
	Set-FixtureText $root 'Notes.md' "untracked note line one`nuntracked note line two`n"
	Set-FixtureBytes $root 'Tools/Blob.bin' ($script:Utf8.GetBytes($script:BinaryMarker) + [byte[]] @(0, 1, 2, 0, 255))
	# A binary payload carrying a textual extension: only a content probe keeps it out of the prompt.
	Set-FixtureBytes $root 'Docs/Capture.md' ($script:Utf8.GetBytes($script:BinaryMarker) + [byte[]] @(0, 7, 0, 9))
	return [pscustomobject] @{ Root = $root; Baseline = $baseline }
}

function Test-AssembledPrompt($Fixture) {
	$scopeFile = New-ScratchFile 'scope' $script:ScopeText
	$promptPath = New-ScratchPath 'assembled'
	$run = Invoke-PromptScript @(
		'-RepositoryRoot', $Fixture.Root, '-Baseline', $Fixture.Baseline, '-AssignedSkill', 'repo-code-review',
		'-ScopeFile', $scopeFile, '-PromptPath', $promptPath,
		'-UntrackedPath', 'Notes.md,Tools/Blob.bin,Docs/Capture.md')
	Assert-Equal 0 $run.ExitCode 'assembled exit code'
	Assert-True ([string]::IsNullOrEmpty($run.Stderr)) 'assembled writes nothing to stderr'
	Assert-True (-not $run.Stdout.Contains('diff --git')) 'assembled keeps the diff off stdout'
	Assert-True ($script:Utf8.GetByteCount($run.Stdout) -lt 4096) 'assembled stdout stays under 4 KB'
	if ($null -eq $run.Json) { Assert-True $false 'assembled emitted JSON'; return }
	Assert-Equal 'broken-engine-codex-review-prompt/v1' $run.Json.schemaVersion 'assembled schemaVersion'
	Assert-Equal 'pass' $run.Json.status 'assembled status'
	Assert-Equal 'ok' $run.Json.code 'assembled code'
	Assert-True (-not [string]::IsNullOrWhiteSpace($run.Json.message)) 'assembled message is non-empty'
	Assert-Equal ([IO.Path]::GetFullPath($promptPath)) $run.Json.promptPath 'assembled promptPath'
	$targetsPath = Get-TargetsPath $promptPath
	Assert-Equal $targetsPath $run.Json.targetsPath 'assembled targetsPath names the prompt sibling'
	Assert-True (Test-Path -LiteralPath $targetsPath) 'assembled wrote the targets file'
	Assert-Equal 5 $run.Json.fileCount 'assembled fileCount'
	Assert-Equal 2 $run.Json.binaryExcluded 'assembled binaryExcluded'
	Assert-Equal 4 $run.Json.sectionsWritten 'assembled sectionsWritten'
	Assert-True (Test-Path -LiteralPath $promptPath) 'assembled wrote the prompt file'
	if (-not (Test-Path -LiteralPath $promptPath)) { return }
	$bytes = [IO.File]::ReadAllBytes($promptPath)
	Assert-Equal $bytes.Length $run.Json.promptBytes 'assembled promptBytes matches the file length'
	$prompt = $script:Utf8.GetString($bytes)

	if (Test-Path -LiteralPath $targetsPath) {
		$targetsText = $script:Utf8.GetString([IO.File]::ReadAllBytes($targetsPath))
		Assert-True ($prompt.Contains("Targets file: $targetsPath`n`n$targetsText")) 'assembled embeds the targets path and bytes in the evidence'
		$targets = $targetsText | ConvertFrom-Json -Depth 32
		Assert-Equal 'broken-engine-code-quality-targets/v1' $targets.schemaVersion 'assembled targets schemaVersion'
		Assert-Equal 'Engine/Source/Keep.cpp,Engine/Source/New.cpp,Engine/Source/Old.cpp' ((@($targets.paths)) -join ',') 'assembled targets hold the union of both rename sides and the modified path'
	}

	$headings = @('# (a) Role', '# (b) Scope', '# (c) Evidence', '# (d) Output contract')
	$previous = -1
	foreach ($heading in $headings) {
		$index = $prompt.IndexOf($heading, [StringComparison]::Ordinal)
		Assert-True ($index -gt $previous) "assembled section order places '$heading' after the previous section"
		$previous = $index
	}
	$delimiters = @($prompt -split "`n" | Where-Object { $_ -ceq '---' })
	Assert-Equal 3 $delimiters.Count 'assembled separates the four sections with three --- delimiter lines'

	$role = (Get-FragmentText 'role-instruction').Replace('{{ASSIGNED_SKILL}}', 'repo-code-review')
	$sectionA = Get-PromptSectionBody $prompt '(a) Role' "`n---`n`n# (b) Scope`n`n"
	Assert-True ($null -ne $sectionA) 'assembled has a role section between its boundaries'
	if ($null -ne $sectionA) {
		$guardrailMarker = "## Guardrails`n`n"
		$guardrailIndex = $sectionA.IndexOf($guardrailMarker, [StringComparison]::Ordinal)
		Assert-True ($guardrailIndex -ge 0) 'assembled places the guardrail block inside the role section'
		if ($guardrailIndex -ge 0) {
			Assert-True ($sectionA.Substring(0, $guardrailIndex) -ceq ($role + "`n`n")) 'assembled copies the role instruction with the assigned skill substituted'
			Assert-True ($sectionA.Substring($guardrailIndex + $guardrailMarker.Length) -ceq ((Get-FragmentText 'guardrails') + "`n")) 'assembled copies the guardrail block byte-identically'
		}
	}
	Assert-True ((Get-PromptSectionBody $prompt '(d) Output contract' '') -ceq ((Get-FragmentText 'output-contract') + "`n")) 'assembled copies the output contract byte-identically'
	Assert-True ((Get-PromptSectionBody $prompt '(b) Scope' "`n---`n`n# (c) Evidence`n`n") -ceq $script:ScopeText) 'assembled copies the scope text byte-identically'
	Assert-True (-not $prompt.Contains('Risk tier:')) 'assembled omits the risk tier line when none is given'

	$expectedDiff = Get-FixtureGitText $Fixture.Root @('diff', '-M', '--no-color', '--no-ext-diff', $Fixture.Baseline, '--', 'Engine/Source/Keep.cpp', 'Engine/Source/New.cpp', 'Engine/Source/Old.cpp')
	$start = $prompt.IndexOf("## Diff`n`n", [StringComparison]::Ordinal)
	Assert-True ($start -ge 0) 'assembled has a diff block'
	if ($start -ge 0) {
		$body = $prompt.Substring($start + "## Diff`n`n".Length)
		$end = $body.IndexOf("`n---`n", [StringComparison]::Ordinal)
		if ($end -ge 0) { $body = $body.Substring(0, $end) }
		$actualDiff = $body.Substring(0, $body.IndexOf("## Untracked file:", [StringComparison]::Ordinal)).TrimEnd("`n")
		Assert-True ($actualDiff -ceq $expectedDiff.TrimEnd("`n")) 'assembled diff equals git diff for the inventoried paths'
		Assert-True ($actualDiff.Contains('rename from Engine/Source/Old.cpp')) 'assembled diff carries both sides of a rename'
	}

	Assert-True ($prompt.Contains("## Untracked file: Notes.md")) 'assembled inlines the untracked text file'
	Assert-True ($prompt.Contains('untracked note line two')) 'assembled inlines the untracked text contents'
	Assert-True (-not $prompt.Contains('## Untracked file: Tools/Blob.bin')) 'assembled never inlines an untracked binary file'
	Assert-True (-not $prompt.Contains($script:BinaryMarker)) 'assembled keeps binary bytes out of the prompt, including a binary file named .md'
	Assert-True ($prompt.Contains('A Tools/Blob.bin — untracked, binary: path and status only')) 'assembled lists the untracked binary by path and status'
	Assert-True ($prompt.Contains('A Docs/Capture.md — untracked, binary: path and status only')) 'assembled classifies a binary .md by content, not extension'
}

function Test-RiskTierLine($Fixture) {
	$scopeFile = New-ScratchFile 'scope' $script:ScopeText
	$promptPath = New-ScratchPath 'risktier'
	$run = Invoke-PromptScript @(
		'-RepositoryRoot', $Fixture.Root, '-Baseline', $Fixture.Baseline, '-AssignedSkill', 'scope-review',
		'-ScopeFile', $scopeFile, '-PromptPath', $promptPath, '-RiskTier', '3',
		'-UntrackedPath', 'Notes.md,Tools/Blob.bin,Docs/Capture.md')
	Assert-Equal 0 $run.ExitCode 'risk tier exit code'
	if (-not (Test-Path -LiteralPath $promptPath)) { Assert-True $false 'risk tier wrote the prompt file'; return }
	$prompt = $script:Utf8.GetString([IO.File]::ReadAllBytes($promptPath))
	Assert-True ((Get-PromptSectionBody $prompt '(b) Scope' "`n---`n`n# (c) Evidence`n`n") -ceq ("Risk tier: 3`n`n" + $script:ScopeText)) 'risk tier writes one line above the verbatim scope text'
}

function Test-PromptPathExists($Fixture) {
	$scopeFile = New-ScratchFile 'scope' $script:ScopeText
	$promptPath = New-ScratchPath 'existing'
	$existing = "already here`n"
	[IO.File]::WriteAllBytes($promptPath, $script:Utf8.GetBytes($existing))
	$run = Invoke-PromptScript @(
		'-RepositoryRoot', $Fixture.Root, '-Baseline', $Fixture.Baseline, '-AssignedSkill', 'repo-code-review',
		'-ScopeFile', $scopeFile, '-PromptPath', $promptPath,
		'-UntrackedPath', 'Notes.md,Tools/Blob.bin,Docs/Capture.md')
	Assert-Equal 2 $run.ExitCode 'existing prompt path exit code'
	if ($null -ne $run.Json) {
		Assert-Equal 'blocked' $run.Json.status 'existing prompt path status'
		Assert-Equal 'prompt.path-exists' $run.Json.code 'existing prompt path code'
		Assert-True ($null -eq $run.Json.promptPath) 'existing prompt path reports no written prompt'
	}
	else { Assert-True $false 'existing prompt path emitted JSON' }
	Assert-True ($script:Utf8.GetString([IO.File]::ReadAllBytes($promptPath)) -ceq $existing) 'existing prompt path leaves the file byte-unchanged'
}

function Test-ScopeFileMissing($Fixture) {
	$promptPath = New-ScratchPath 'noscope'
	$run = Invoke-PromptScript @(
		'-RepositoryRoot', $Fixture.Root, '-Baseline', $Fixture.Baseline, '-AssignedSkill', 'repo-code-review',
		'-ScopeFile', (Join-Path $script:Scratch 'no-such-scope.md'), '-PromptPath', $promptPath)
	Assert-Equal 1 $run.ExitCode 'missing scope file exit code'
	if ($null -ne $run.Json) {
		Assert-Equal 'error' $run.Json.status 'missing scope file status'
		Assert-Equal 'prompt.scope-file-missing' $run.Json.code 'missing scope file code'
	}
	else { Assert-True $false 'missing scope file emitted JSON' }
	Assert-True (-not (Test-Path -LiteralPath $promptPath)) 'missing scope file creates no prompt file'
}

function Test-UnlistedUntracked($Fixture) {
	$scopeFile = New-ScratchFile 'scope' $script:ScopeText
	$promptPath = New-ScratchPath 'unlisted'
	$run = Invoke-PromptScript @(
		'-RepositoryRoot', $Fixture.Root, '-Baseline', $Fixture.Baseline, '-AssignedSkill', 'repo-code-review',
		'-ScopeFile', $scopeFile, '-PromptPath', $promptPath, '-UntrackedPath', 'Notes.md')
	Assert-Equal 2 $run.ExitCode 'unlisted untracked exit code'
	if ($null -ne $run.Json) {
		Assert-Equal 'blocked' $run.Json.status 'unlisted untracked status'
		Assert-Equal 'prompt.inventory-truncated' $run.Json.code 'unlisted untracked code'
	}
	else { Assert-True $false 'unlisted untracked emitted JSON' }
	Assert-True (-not (Test-Path -LiteralPath $promptPath)) 'unlisted untracked creates no prompt file'
}

function Test-TargetsSiblingExists($Fixture) {
	$scopeFile = New-ScratchFile 'scope' $script:ScopeText
	$promptPath = New-ScratchPath 'targetssibling'
	$targetsPath = Get-TargetsPath $promptPath
	$existing = "{`"paths`":[]}`n"
	[IO.File]::WriteAllBytes($targetsPath, $script:Utf8.GetBytes($existing))
	$run = Invoke-PromptScript @(
		'-RepositoryRoot', $Fixture.Root, '-Baseline', $Fixture.Baseline, '-AssignedSkill', 'repo-code-review',
		'-ScopeFile', $scopeFile, '-PromptPath', $promptPath,
		'-UntrackedPath', 'Notes.md,Tools/Blob.bin,Docs/Capture.md')
	Assert-Equal 2 $run.ExitCode 'existing targets sibling exit code'
	if ($null -ne $run.Json) {
		Assert-Equal 'blocked' $run.Json.status 'existing targets sibling status'
		Assert-Equal 'prompt.path-exists' $run.Json.code 'existing targets sibling code'
		Assert-True ($null -eq $run.Json.targetsPath) 'existing targets sibling reports no written targets file'
	}
	else { Assert-True $false 'existing targets sibling emitted JSON' }
	Assert-True ($script:Utf8.GetString([IO.File]::ReadAllBytes($targetsPath)) -ceq $existing) 'existing targets sibling leaves the file byte-unchanged'
	Assert-True (-not (Test-Path -LiteralPath $promptPath)) 'existing targets sibling creates no prompt file'
}

function Test-AssignedSkillUnknown($Fixture) {
	$scopeFile = New-ScratchFile 'scope' $script:ScopeText
	$promptPath = New-ScratchPath 'unknownskill'
	$run = Invoke-PromptScript @(
		'-RepositoryRoot', $Fixture.Root, '-Baseline', $Fixture.Baseline, '-AssignedSkill', 'acceptance-table-review',
		'-ScopeFile', $scopeFile, '-PromptPath', $promptPath,
		'-UntrackedPath', 'Notes.md,Tools/Blob.bin,Docs/Capture.md')
	Assert-Equal 2 $run.ExitCode 'unknown assigned skill exit code'
	if ($null -ne $run.Json) {
		Assert-Equal 'blocked' $run.Json.status 'unknown assigned skill status'
		Assert-Equal 'prompt.assigned-skill-unknown' $run.Json.code 'unknown assigned skill code'
	}
	else { Assert-True $false 'unknown assigned skill emitted JSON' }
	Assert-True (-not (Test-Path -LiteralPath $promptPath)) 'unknown assigned skill creates no prompt file'

	$adHocPath = New-ScratchPath 'adhocrole'
	$adHoc = Invoke-PromptScript @(
		'-RepositoryRoot', $Fixture.Root, '-Baseline', $Fixture.Baseline, '-AssignedSkill', 'acceptance-table-review',
		'-ScopeFile', $scopeFile, '-PromptPath', $adHocPath,
		'-UntrackedPath', 'Notes.md,Tools/Blob.bin,Docs/Capture.md', '-AdHocRole')
	Assert-Equal 0 $adHoc.ExitCode 'ad-hoc role exit code'
	Assert-True (Test-Path -LiteralPath $adHocPath) 'ad-hoc role writes the prompt for a skill-less reviewer role'
}

function Test-MixedCaseAssignedSkill($Fixture) {
	# The skill file resolves case-insensitively on Windows, so a mixed-case name passes validation and
	# must still reach the special-skill contract.
	$scopeFile = New-ScratchFile 'scope' $script:ScopeText
	$promptPath = New-ScratchPath 'mixedcase'
	$run = Invoke-PromptScript @(
		'-RepositoryRoot', $Fixture.Root, '-Baseline', $Fixture.Baseline, '-AssignedSkill', 'Repo-Code-Review',
		'-ScopeFile', $scopeFile, '-PromptPath', $promptPath,
		'-UntrackedPath', 'Notes.md,Tools/Blob.bin,Docs/Capture.md')
	Assert-Equal 0 $run.ExitCode 'mixed-case repo-code-review exit code'
	$targetsPath = Get-TargetsPath $promptPath
	if ($null -ne $run.Json) { Assert-Equal $targetsPath $run.Json.targetsPath 'mixed-case repo-code-review reports the targets sibling' }
	else { Assert-True $false 'mixed-case repo-code-review emitted JSON' }
	Assert-True (Test-Path -LiteralPath $targetsPath) 'mixed-case repo-code-review writes the targets file'
}

function Test-PlanAuditExecutionCard($Fixture) {
	# The card is the one input only the manager can author, so a scope without it has to block at
	# assembly rather than cost a whole review round.
	$missingPath = New-ScratchPath 'nocard'
	$missing = Invoke-PromptScript @(
		'-RepositoryRoot', $Fixture.Root, '-Baseline', $Fixture.Baseline, '-AssignedSkill', 'plan-audit',
		'-ScopeFile', (New-ScratchFile 'scope' $script:ScopeText), '-PromptPath', $missingPath,
		'-UntrackedPath', 'Notes.md,Tools/Blob.bin,Docs/Capture.md')
	Assert-Equal 2 $missing.ExitCode 'plan-audit without an execution card exit code'
	if ($null -ne $missing.Json) {
		Assert-Equal 'blocked' $missing.Json.status 'plan-audit without an execution card status'
		Assert-Equal 'prompt.execution-card-required' $missing.Json.code 'plan-audit without an execution card code'
	}
	else { Assert-True $false 'plan-audit without an execution card emitted JSON' }
	Assert-True (-not (Test-Path -LiteralPath $missingPath)) 'plan-audit without an execution card creates no prompt file'

	$cardText = $script:ScopeText + "Draft execution card`n- Tier 3: determinism surface.`n- Acceptance: the fixture suite exits 0.`n"
	$cardPath = New-ScratchPath 'card'
	$card = Invoke-PromptScript @(
		'-RepositoryRoot', $Fixture.Root, '-Baseline', $Fixture.Baseline, '-AssignedSkill', 'plan-audit',
		'-ScopeFile', (New-ScratchFile 'scope' $cardText), '-PromptPath', $cardPath,
		'-UntrackedPath', 'Notes.md,Tools/Blob.bin,Docs/Capture.md')
	Assert-Equal 0 $card.ExitCode 'plan-audit with an execution card exit code'
	Assert-True (Test-Path -LiteralPath $cardPath) 'plan-audit with an execution card writes the prompt'
	if (-not (Test-Path -LiteralPath $cardPath)) { return }
	$prompt = $script:Utf8.GetString([IO.File]::ReadAllBytes($cardPath))
	Assert-True ((Get-PromptSectionBody $prompt '(b) Scope' "`n---`n`n# (c) Evidence`n`n") -ceq $cardText) 'plan-audit with an execution card still copies the scope text byte-identically'
}

function Test-VerifyChangesTypedArtifacts() {
	# A landing diff that touches both an executable Plan and a skill file. The Plan side is deliberately
	# present in every case: the reviewer runs the Plan lint itself, so a touched Plan must never make the
	# gate demand a validate receipt.
	$root = New-FixtureRoot 'typedartifacts'
	Add-FixtureSkill $root @('verify-changes')
	Set-FixtureText $root 'Documents/Plans/Area/Thing.md' "baseline plan`n"
	Invoke-FixtureGit $root @('add', '--all')
	Invoke-FixtureGit $root @('commit', '--quiet', '-m', 'baseline')
	$baseline = (Get-FixtureGitText $root @('rev-parse', 'HEAD')).Trim()
	Set-FixtureText $root 'Documents/Plans/Area/Thing.md' "landed plan`n"
	Set-FixtureText $root '.agents/skills/verify-changes/SKILL.md' "# verify-changes`nlanded`n"
	Invoke-FixtureGit $root @('add', '--all')
	Invoke-FixtureGit $root @('commit', '--quiet', '-m', 'landed')
	$head = (Get-FixtureGitText $root @('rev-parse', 'HEAD')).Trim()

	# One compact build envelope line, the shape `WorktreeCli build` emits, whose own target names the
	# game project — present in every case below to hold that a game build requires no extra marker.
	$gameBuildLine = "Client build: {`"schemaVersion`":`"broken-engine-build-result/v1`",`"status`":`"success`",`"target`":{`"requested`":`"C:\\repo\\Projects\\BrokenEngineSandbox\\Platforms\\VisualStudio2026\\BrokenEngineSandbox.vcxproj`",`"normalized`":`"c:/repo/projects/brokenenginesandbox/platforms/visualstudio2026/brokenenginesandbox.vcxproj`"}}`n"
	$missingText = (New-VerifyScopeText $baseline $head) + $gameBuildLine
	$missingPath = New-ScratchPath 'noartifacts'
	$missing = Invoke-PromptScript @(
		'-RepositoryRoot', $root, '-Baseline', $baseline, '-AssignedSkill', 'verify-changes',
		'-ScopeFile', (New-ScratchFile 'scope' $missingText), '-PromptPath', $missingPath, '-Head', $head)
	Assert-Equal 2 $missing.ExitCode 'verify-changes without typed artifacts exit code'
	if ($null -ne $missing.Json) {
		Assert-Equal 'blocked' $missing.Json.status 'verify-changes without typed artifacts status'
		Assert-Equal 'prompt.typed-artifacts-required' $missing.Json.code 'verify-changes without typed artifacts code'
		Assert-True ($missing.Json.message.Contains('Validation: PASS')) 'verify-changes names the missing /validate-skill marker'
		Assert-True (-not $missing.Json.message.Contains('"operation":"validate"')) 'verify-changes never demands a plan validate receipt'
	}
	else { Assert-True $false 'verify-changes without typed artifacts emitted JSON' }
	Assert-True (-not (Test-Path -LiteralPath $missingPath)) 'verify-changes without typed artifacts creates no prompt file'

	# The same game build envelope with the /validate-skill artifact present and no plan validate receipt,
	# so a touched Plan assembles on the identity values alone and the game build demands nothing further.
	$artifactText = $missingText + "Skill validation: Validation: PASS`n"
	$presentPath = New-ScratchPath 'artifacts'
	$present = Invoke-PromptScript @(
		'-RepositoryRoot', $root, '-Baseline', $baseline, '-AssignedSkill', 'verify-changes',
		'-ScopeFile', (New-ScratchFile 'scope' $artifactText), '-PromptPath', $presentPath, '-Head', $head)
	Assert-Equal 0 $present.ExitCode 'verify-changes with typed artifacts exit code'
	if ($null -ne $present.Json) { Assert-Equal 'pass' $present.Json.status 'verify-changes with typed artifacts status' }
	else { Assert-True $false 'verify-changes with typed artifacts emitted JSON' }
	Assert-True (Test-Path -LiteralPath $presentPath) 'verify-changes with typed artifacts writes the prompt'
	if (Test-Path -LiteralPath $presentPath) {
		$prompt = $script:Utf8.GetString([IO.File]::ReadAllBytes($presentPath))
		Assert-True ((Get-PromptSectionBody $prompt '(b) Scope' "`n---`n`n# (c) Evidence`n`n") -ceq $artifactText) 'verify-changes with typed artifacts still copies the scope text byte-identically'
	}

	# The same complete artifacts bound to a revision that is not the reviewed one.
	$mismatchText = $script:ScopeText + "Baseline: $('0' * 40)`nHead: $('1' * 40)`nSkill validation: Validation: PASS`n"
	$mismatchPath = New-ScratchPath 'artifactidentity'
	$mismatch = Invoke-PromptScript @(
		'-RepositoryRoot', $root, '-Baseline', $baseline, '-AssignedSkill', 'verify-changes',
		'-ScopeFile', (New-ScratchFile 'scope' $mismatchText), '-PromptPath', $mismatchPath, '-Head', $head)
	Assert-Equal 2 $mismatch.ExitCode 'verify-changes with mismatched identity exit code'
	if ($null -ne $mismatch.Json) {
		Assert-Equal 'blocked' $mismatch.Json.status 'verify-changes with mismatched identity status'
		Assert-Equal 'prompt.typed-artifacts-required' $mismatch.Json.code 'verify-changes with mismatched identity code'
		Assert-True ($mismatch.Json.message.Contains('the baseline SHA')) 'verify-changes names the unmatched baseline identity'
		Assert-True ($mismatch.Json.message.Contains('the head SHA')) 'verify-changes names the unmatched head identity'
	}
	else { Assert-True $false 'verify-changes with mismatched identity emitted JSON' }
	Assert-True (-not (Test-Path -LiteralPath $mismatchPath)) 'verify-changes with mismatched identity creates no prompt file'
}

# --- Repository B: a committed head over a dirty working tree ------------------------------------

function Test-HeadHonoured() {
	$root = New-FixtureRoot 'head'
	Add-FixtureSkill $root @('repo-code-review')
	Set-FixtureText $root 'Engine/Source/Head.cpp' "baseline content`n"
	Invoke-FixtureGit $root @('add', '--all')
	Invoke-FixtureGit $root @('commit', '--quiet', '-m', 'baseline')
	$baseline = (Get-FixtureGitText $root @('rev-parse', 'HEAD')).Trim()
	Set-FixtureText $root 'Engine/Source/Head.cpp' "committed head content`n"
	Invoke-FixtureGit $root @('add', '--all')
	Invoke-FixtureGit $root @('commit', '--quiet', '-m', 'head')
	$head = (Get-FixtureGitText $root @('rev-parse', 'HEAD')).Trim()
	Set-FixtureText $root 'Engine/Source/Head.cpp' "dirty working tree content`n"

	$scopeFile = New-ScratchFile 'scope' $script:ScopeText
	$promptPath = New-ScratchPath 'head'
	$run = Invoke-PromptScript @(
		'-RepositoryRoot', $root, '-Baseline', $baseline, '-AssignedSkill', 'repo-code-review',
		'-ScopeFile', $scopeFile, '-PromptPath', $promptPath, '-Head', $head)
	Assert-Equal 0 $run.ExitCode 'head-side exit code'
	if (-not (Test-Path -LiteralPath $promptPath)) { Assert-True $false 'head-side wrote the prompt file'; return }
	$prompt = $script:Utf8.GetString([IO.File]::ReadAllBytes($promptPath))
	Assert-True ($prompt.Contains("Head: $head")) 'head-side names the resolved head SHA'
	Assert-True ($prompt.Contains('committed head content')) 'head-side diffs the committed head'
	Assert-True (-not $prompt.Contains('dirty working tree content')) 'head-side ignores dirty working-tree bytes'

	$targetsPath = Get-TargetsPath $promptPath
	Assert-True (Test-Path -LiteralPath $targetsPath) 'head-side wrote the targets file'
	if (Test-Path -LiteralPath $targetsPath) {
		$targets = ($script:Utf8.GetString([IO.File]::ReadAllBytes($targetsPath))) | ConvertFrom-Json -Depth 32
		Assert-Equal 'Engine/Source/Head.cpp' ((@($targets.paths)) -join ',') 'head-side targets name the one changed C++ path'
	}

	$conflictPrompt = New-ScratchPath 'conflict'
	$conflict = Invoke-PromptScript @(
		'-RepositoryRoot', $root, '-Baseline', $baseline, '-AssignedSkill', 'repo-code-review',
		'-ScopeFile', $scopeFile, '-PromptPath', $conflictPrompt, '-Head', $head, '-UntrackedPath', 'Notes.md')
	Assert-Equal 2 $conflict.ExitCode 'head with untracked exit code'
	if ($null -ne $conflict.Json) {
		Assert-Equal 'blocked' $conflict.Json.status 'head with untracked status'
		Assert-Equal 'prompt.head-untracked-conflict' $conflict.Json.code 'head with untracked code'
	}
	else { Assert-True $false 'head with untracked emitted JSON' }
	Assert-True (-not (Test-Path -LiteralPath $conflictPrompt)) 'head with untracked creates no prompt file'
}

# --- Repositories C and D: no eligible C++ target, and an untracked-addition rename ---------------

function Test-ZeroTargets() {
	$root = New-FixtureRoot 'zerotargets'
	Add-FixtureSkill $root @('repo-code-review')
	Set-FixtureText $root 'Documents/Notes.txt' "one`n"
	Invoke-FixtureGit $root @('add', '--all')
	Invoke-FixtureGit $root @('commit', '--quiet', '-m', 'baseline')
	$baseline = (Get-FixtureGitText $root @('rev-parse', 'HEAD')).Trim()
	Set-FixtureText $root 'Documents/Notes.txt' "two`n"

	$scopeFile = New-ScratchFile 'scope' $script:ScopeText
	$promptPath = New-ScratchPath 'zerotargets'
	$run = Invoke-PromptScript @(
		'-RepositoryRoot', $root, '-Baseline', $baseline, '-AssignedSkill', 'repo-code-review',
		'-ScopeFile', $scopeFile, '-PromptPath', $promptPath)
	Assert-Equal 0 $run.ExitCode 'zero-target exit code'
	$targetsPath = Get-TargetsPath $promptPath
	Assert-True (Test-Path -LiteralPath $targetsPath) 'zero-target targets file is written, not treated as an error'
	if (-not (Test-Path -LiteralPath $targetsPath)) { return }
	$targets = ($script:Utf8.GetString([IO.File]::ReadAllBytes($targetsPath))) | ConvertFrom-Json -Depth 32
	Assert-Equal 'broken-engine-code-quality-targets/v1' $targets.schemaVersion 'zero-target targets schemaVersion'
	Assert-Equal 0 (@($targets.paths).Count) 'zero-target targets hold no path'
}

function Test-UntrackedRenameTargets() {
	$root = New-FixtureRoot 'untrackedrename'
	Add-FixtureSkill $root @('repo-code-review')
	Set-FixtureText $root 'Engine/Source/Gone.cpp' "shared content`n"
	Invoke-FixtureGit $root @('add', '--all')
	Invoke-FixtureGit $root @('commit', '--quiet', '-m', 'baseline')
	$baseline = (Get-FixtureGitText $root @('rev-parse', 'HEAD')).Trim()
	# An untracked addition holding a removed blob's bytes is a rename Git never reported. The paths
	# union carries both sides, and the analyzer derives the pairing from them.
	Invoke-FixtureGit $root @('rm', '--quiet', 'Engine/Source/Gone.cpp')
	Set-FixtureText $root 'Engine/Source/Copy.cpp' "shared content`n"

	$scopeFile = New-ScratchFile 'scope' $script:ScopeText
	$promptPath = New-ScratchPath 'untrackedrename'
	$run = Invoke-PromptScript @(
		'-RepositoryRoot', $root, '-Baseline', $baseline, '-AssignedSkill', 'repo-code-review',
		'-ScopeFile', $scopeFile, '-PromptPath', $promptPath, '-UntrackedPath', 'Engine/Source/Copy.cpp')
	Assert-Equal 0 $run.ExitCode 'untracked-rename exit code'
	$targetsPath = Get-TargetsPath $promptPath
	Assert-True (Test-Path -LiteralPath $targetsPath) 'untracked-rename wrote the targets file'
	if (-not (Test-Path -LiteralPath $targetsPath)) { return }
	$targets = ($script:Utf8.GetString([IO.File]::ReadAllBytes($targetsPath))) | ConvertFrom-Json -Depth 32
	Assert-Equal 'Engine/Source/Copy.cpp,Engine/Source/Gone.cpp' ((@($targets.paths)) -join ',') 'untracked-rename targets carry both sides'
}

# --- Repository E: the /verify-changes head and clean-tree contract ------------------------------

function Test-VerifyChangesHead() {
	$root = New-FixtureRoot 'verify'
	Add-FixtureSkill $root @('verify-changes')
	Set-FixtureText $root 'Engine/Source/Verify.cpp' "baseline`n"
	Set-FixtureText $root 'Engine/Source/Other.cpp' "other baseline`n"
	Invoke-FixtureGit $root @('add', '--all')
	Invoke-FixtureGit $root @('commit', '--quiet', '-m', 'baseline')
	$baseline = (Get-FixtureGitText $root @('rev-parse', 'HEAD')).Trim()
	Set-FixtureText $root 'Engine/Source/Verify.cpp' "landed`n"
	Invoke-FixtureGit $root @('add', '--all')
	Invoke-FixtureGit $root @('commit', '--quiet', '-m', 'landed')
	$head = (Get-FixtureGitText $root @('rev-parse', 'HEAD')).Trim()
	$scopeFile = New-ScratchFile 'scope' (New-VerifyScopeText $baseline $head)

	$noHeadPath = New-ScratchPath 'nohead'
	$noHead = Invoke-PromptScript @(
		'-RepositoryRoot', $root, '-Baseline', $baseline, '-AssignedSkill', 'verify-changes',
		'-ScopeFile', $scopeFile, '-PromptPath', $noHeadPath)
	Assert-Equal 2 $noHead.ExitCode 'verify-changes without head exit code'
	if ($null -ne $noHead.Json) {
		Assert-Equal 'blocked' $noHead.Json.status 'verify-changes without head status'
		Assert-Equal 'prompt.head-required' $noHead.Json.code 'verify-changes without head code'
	}
	else { Assert-True $false 'verify-changes without head emitted JSON' }
	Assert-True (-not (Test-Path -LiteralPath $noHeadPath)) 'verify-changes without head creates no prompt file'

	$mixedCasePath = New-ScratchPath 'mixedcasenohead'
	$mixedCase = Invoke-PromptScript @(
		'-RepositoryRoot', $root, '-Baseline', $baseline, '-AssignedSkill', 'Verify-Changes',
		'-ScopeFile', $scopeFile, '-PromptPath', $mixedCasePath)
	Assert-Equal 2 $mixedCase.ExitCode 'mixed-case verify-changes without head exit code'
	if ($null -ne $mixedCase.Json) {
		Assert-Equal 'blocked' $mixedCase.Json.status 'mixed-case verify-changes without head status'
		Assert-Equal 'prompt.head-required' $mixedCase.Json.code 'mixed-case verify-changes without head code'
	}
	else { Assert-True $false 'mixed-case verify-changes without head emitted JSON' }
	Assert-True (-not (Test-Path -LiteralPath $mixedCasePath)) 'mixed-case verify-changes without head creates no prompt file'

	$cleanPath = New-ScratchPath 'verifyclean'
	$clean = Invoke-PromptScript @(
		'-RepositoryRoot', $root, '-Baseline', $baseline, '-AssignedSkill', 'verify-changes',
		'-ScopeFile', $scopeFile, '-PromptPath', $cleanPath, '-Head', $head)
	Assert-Equal 0 $clean.ExitCode 'verify-changes clean reviewed path exit code'
	Assert-True (Test-Path -LiteralPath $cleanPath) 'verify-changes clean reviewed path writes the prompt'

	Set-FixtureText $root 'Engine/Source/Other.cpp' "other dirty`n"
	$unrelatedPath = New-ScratchPath 'verifyunrelated'
	$unrelated = Invoke-PromptScript @(
		'-RepositoryRoot', $root, '-Baseline', $baseline, '-AssignedSkill', 'verify-changes',
		'-ScopeFile', $scopeFile, '-PromptPath', $unrelatedPath, '-Head', $head)
	Assert-Equal 0 $unrelated.ExitCode 'verify-changes ignores dirt outside the reviewed set'
	Assert-True (Test-Path -LiteralPath $unrelatedPath) 'verify-changes with unrelated dirt writes the prompt'

	Set-FixtureText $root 'Engine/Source/Verify.cpp' "dirty`n"
	$dirtyPath = New-ScratchPath 'verifydirty'
	$dirty = Invoke-PromptScript @(
		'-RepositoryRoot', $root, '-Baseline', $baseline, '-AssignedSkill', 'verify-changes',
		'-ScopeFile', $scopeFile, '-PromptPath', $dirtyPath, '-Head', $head)
	Assert-Equal 2 $dirty.ExitCode 'verify-changes dirty reviewed path exit code'
	if ($null -ne $dirty.Json) {
		Assert-Equal 'blocked' $dirty.Json.status 'verify-changes dirty reviewed path status'
		Assert-Equal 'prompt.head-required' $dirty.Json.code 'verify-changes dirty reviewed path code'
		Assert-True ($dirty.Json.message.Contains('Engine/Source/Verify.cpp')) 'verify-changes names the dirty reviewed path'
	}
	else { Assert-True $false 'verify-changes dirty reviewed path emitted JSON' }
	Assert-True (-not (Test-Path -LiteralPath $dirtyPath)) 'verify-changes dirty reviewed path creates no prompt file'
}

function New-FixtureCandidateCommit([string] $Root, [string] $Parent) {
	# A temporary index is what leaves the real index, the working tree, and the branch ref untouched
	# while the candidate tree is written, so the commit exists only as an object the caller names.
	$indexFile = Join-Path $script:Scratch ('candidate-index-' + [Guid]::NewGuid().ToString('n').Substring(0, 8))
	$previous = $env:GIT_INDEX_FILE
	$env:GIT_INDEX_FILE = $indexFile
	try {
		Invoke-FixtureGit $Root @('read-tree', $Parent)
		Invoke-FixtureGit $Root @('add', '-A')
		$tree = (Get-FixtureGitText $Root @('write-tree')).Trim()
		return (Get-FixtureGitText $Root @('commit-tree', $tree, '-p', $Parent, '-m', 'candidate')).Trim()
	}
	finally {
		if ($null -eq $previous) { Remove-Item -LiteralPath 'Env:GIT_INDEX_FILE' -ErrorAction SilentlyContinue }
		else { $env:GIT_INDEX_FILE = $previous }
		Remove-Item -LiteralPath $indexFile -Force -ErrorAction SilentlyContinue
	}
}

function Test-VerifyChangesCandidateHead() {
	# The /finalize-changes primary-commit route reviews a candidate commit written through a temporary
	# index: the branch ref, the real index, and the working tree all stay where they were, so every
	# reviewed path is uncommitted relative to the checked-out HEAD while still matching -Head.
	$root = New-FixtureRoot 'candidate'
	Add-FixtureSkill $root @('verify-changes')
	Set-FixtureText $root 'Engine/Source/Kept.cpp' "baseline kept`n"
	Set-FixtureText $root 'Engine/Source/Removed.cpp' "baseline removed`n"
	Invoke-FixtureGit $root @('add', '--all')
	Invoke-FixtureGit $root @('commit', '--quiet', '-m', 'baseline')
	$baseline = (Get-FixtureGitText $root @('rev-parse', 'HEAD')).Trim()
	Set-FixtureText $root 'Engine/Source/Kept.cpp' "candidate kept`n"
	Set-FixtureText $root 'Engine/Source/Added.cpp' "candidate added`n"
	Remove-Item -LiteralPath (Join-Path $root ('Engine/Source/Removed.cpp' -replace '/', [IO.Path]::DirectorySeparatorChar)) -Force
	$candidate = New-FixtureCandidateCommit $root $baseline
	$branchTip = (Get-FixtureGitText $root @('rev-parse', 'HEAD')).Trim()
	Assert-Equal $baseline $branchTip 'candidate head leaves the branch ref unmoved'
	$scopeFile = New-ScratchFile 'scope' (New-VerifyScopeText $baseline $candidate)

	$matchPath = New-ScratchPath 'candidatematch'
	$match = Invoke-PromptScript @(
		'-RepositoryRoot', $root, '-Baseline', $baseline, '-AssignedSkill', 'verify-changes',
		'-ScopeFile', $scopeFile, '-PromptPath', $matchPath, '-Head', $candidate)
	Assert-Equal 0 $match.ExitCode 'candidate head matching the working tree exit code'
	Assert-True (Test-Path -LiteralPath $matchPath) 'candidate head matching the working tree writes the prompt'
	if (Test-Path -LiteralPath $matchPath) {
		$prompt = $script:Utf8.GetString([IO.File]::ReadAllBytes($matchPath))
		Assert-True ($prompt.Contains('candidate added')) 'candidate head carries the path that is untracked in the real index'
	}

	Set-FixtureText $root 'Engine/Source/Kept.cpp' "edited after the candidate`n"
	$editedPath = New-ScratchPath 'candidateedited'
	$edited = Invoke-PromptScript @(
		'-RepositoryRoot', $root, '-Baseline', $baseline, '-AssignedSkill', 'verify-changes',
		'-ScopeFile', $scopeFile, '-PromptPath', $editedPath, '-Head', $candidate)
	Assert-Equal 2 $edited.ExitCode 'candidate head with an edited reviewed path exit code'
	if ($null -ne $edited.Json) {
		Assert-Equal 'blocked' $edited.Json.status 'candidate head with an edited reviewed path status'
		Assert-Equal 'prompt.head-required' $edited.Json.code 'candidate head with an edited reviewed path code'
		Assert-True ($edited.Json.message.Contains('Engine/Source/Kept.cpp')) 'candidate head names the edited reviewed path'
	}
	else { Assert-True $false 'candidate head with an edited reviewed path emitted JSON' }
	Assert-True (-not (Test-Path -LiteralPath $editedPath)) 'candidate head with an edited reviewed path creates no prompt file'

	# A path the candidate deleted, recreated as an untracked working-tree file: present on one side
	# only, which a naive diff of the two sides would miss.
	Set-FixtureText $root 'Engine/Source/Kept.cpp' "candidate kept`n"
	Set-FixtureText $root 'Engine/Source/Removed.cpp' "recreated after the candidate`n"
	$recreatedPath = New-ScratchPath 'candidaterecreated'
	$recreated = Invoke-PromptScript @(
		'-RepositoryRoot', $root, '-Baseline', $baseline, '-AssignedSkill', 'verify-changes',
		'-ScopeFile', $scopeFile, '-PromptPath', $recreatedPath, '-Head', $candidate)
	Assert-Equal 2 $recreated.ExitCode 'candidate head with a recreated deleted path exit code'
	if ($null -ne $recreated.Json) {
		Assert-Equal 'prompt.head-required' $recreated.Json.code 'candidate head with a recreated deleted path code'
		Assert-True ($recreated.Json.message.Contains('Engine/Source/Removed.cpp')) 'candidate head names the recreated deleted path'
	}
	else { Assert-True $false 'candidate head with a recreated deleted path emitted JSON' }
	Assert-True (-not (Test-Path -LiteralPath $recreatedPath)) 'candidate head with a recreated deleted path creates no prompt file'
}

function Test-VerifyChangesReviewedPathShapes() {
	# Two path shapes the reviewed-tree comparison has to resolve exactly on Windows: a name carrying
	# pathspec metacharacters, and the losing side of a case-only rename, whose old spelling still
	# resolves to the surviving file on a case-insensitive file system.
	$root = New-FixtureRoot 'pathshapes'
	# Without this git never records the case-only rename, and the fixture would prove nothing.
	Invoke-FixtureGit $root @('config', 'core.ignorecase', 'false')
	Add-FixtureSkill $root @('verify-changes')
	Set-FixtureText $root 'Engine/Source/Bracket[a].cpp' "baseline bracket`n"
	Set-FixtureText $root 'Engine/Source/CaseName.cpp' "baseline case`n"
	Invoke-FixtureGit $root @('add', '--all')
	Invoke-FixtureGit $root @('commit', '--quiet', '-m', 'baseline')
	$baseline = (Get-FixtureGitText $root @('rev-parse', 'HEAD')).Trim()
	Set-FixtureText $root 'Engine/Source/Bracket[a].cpp' "candidate bracket`n"
	$oldCase = Join-Path $root ('Engine/Source/CaseName.cpp' -replace '/', [IO.Path]::DirectorySeparatorChar)
	$newCase = Join-Path $root ('Engine/Source/casename.cpp' -replace '/', [IO.Path]::DirectorySeparatorChar)
	# Through a third name, because a direct case-only move is a same-file move on this volume.
	[IO.File]::Move($oldCase, "$oldCase.rename")
	[IO.File]::Move("$oldCase.rename", $newCase)

	$indexFile = Join-Path $script:Scratch ('pathshapes-index-' + [Guid]::NewGuid().ToString('n').Substring(0, 8))
	$previous = $env:GIT_INDEX_FILE
	$env:GIT_INDEX_FILE = $indexFile
	try {
		Invoke-FixtureGit $root @('read-tree', $baseline)
		# The old spelling still stats successfully here, so only an explicit removal takes it out of
		# the candidate tree; the following add picks up the surviving spelling from the directory.
		Invoke-FixtureGit $root @('update-index', '--force-remove', 'Engine/Source/CaseName.cpp')
		Invoke-FixtureGit $root @('add', '-A')
		$tree = (Get-FixtureGitText $root @('write-tree')).Trim()
		$candidate = (Get-FixtureGitText $root @('commit-tree', $tree, '-p', $baseline, '-m', 'candidate')).Trim()
	}
	finally {
		if ($null -eq $previous) { Remove-Item -LiteralPath 'Env:GIT_INDEX_FILE' -ErrorAction SilentlyContinue }
		else { $env:GIT_INDEX_FILE = $previous }
		Remove-Item -LiteralPath $indexFile -Force -ErrorAction SilentlyContinue
	}
	$treeText = Get-FixtureGitText $root @('ls-tree', '-r', '--name-only', $candidate)
	Assert-True ($treeText.Contains('Engine/Source/casename.cpp')) 'candidate tree carries the new spelling'
	Assert-True (-not $treeText.Contains('Engine/Source/CaseName.cpp')) 'candidate tree drops the old spelling'
	$scopeFile = New-ScratchFile 'scope' (New-VerifyScopeText $baseline $candidate)

	$matchPath = New-ScratchPath 'pathshapes'
	$match = Invoke-PromptScript @(
		'-RepositoryRoot', $root, '-Baseline', $baseline, '-AssignedSkill', 'verify-changes',
		'-ScopeFile', $scopeFile, '-PromptPath', $matchPath, '-Head', $candidate)
	Assert-Equal 0 $match.ExitCode 'metacharacter name and case-only rename exit code'
	Assert-True (Test-Path -LiteralPath $matchPath) 'metacharacter name and case-only rename writes the prompt'

	# The same metacharacter name once its bytes really differ: the literal lookup still has to block.
	Set-FixtureText $root 'Engine/Source/Bracket[a].cpp' "edited after the candidate`n"
	$editedPath = New-ScratchPath 'pathshapesedited'
	$edited = Invoke-PromptScript @(
		'-RepositoryRoot', $root, '-Baseline', $baseline, '-AssignedSkill', 'verify-changes',
		'-ScopeFile', $scopeFile, '-PromptPath', $editedPath, '-Head', $candidate)
	Assert-Equal 2 $edited.ExitCode 'edited metacharacter name exit code'
	if ($null -ne $edited.Json) {
		Assert-Equal 'blocked' $edited.Json.status 'edited metacharacter name status'
		Assert-Equal 'prompt.head-required' $edited.Json.code 'edited metacharacter name code'
		Assert-True ($edited.Json.message.Contains('Engine/Source/Bracket[a].cpp')) 'edited metacharacter name is named'
	}
	else { Assert-True $false 'edited metacharacter name emitted JSON' }
	Assert-True (-not (Test-Path -LiteralPath $editedPath)) 'edited metacharacter name creates no prompt file'
}

function Test-VerifyChangesGitlinkDirectory() {
	# A candidate tree recording a gitlink whose commit is this repository's own HEAD, standing against a
	# plain working-tree directory at that path: `git -C <directory> rev-parse HEAD` searches upwards, so
	# the superproject answers for a submodule that is not there and the two sides would compare equal.
	$root = New-FixtureRoot 'gitlink'
	Add-FixtureSkill $root @('verify-changes')
	Set-FixtureText $root 'Engine/Source/Kept.cpp' "baseline kept`n"
	Invoke-FixtureGit $root @('add', '--all')
	Invoke-FixtureGit $root @('commit', '--quiet', '-m', 'baseline')
	$baseline = (Get-FixtureGitText $root @('rev-parse', 'HEAD')).Trim()
	# An ordinary directory where the candidate records the gitlink, holding a file the repository
	# tracks: nothing here is untracked, so the inventory stays complete without naming a path.
	Set-FixtureText $root 'Vendor/Sub/Inner.txt' "inner`n"
	Invoke-FixtureGit $root @('add', '--all')
	Invoke-FixtureGit $root @('commit', '--quiet', '-m', 'inner')
	$branchTip = (Get-FixtureGitText $root @('rev-parse', 'HEAD')).Trim()

	$indexFile = Join-Path $script:Scratch ('gitlink-index-' + [Guid]::NewGuid().ToString('n').Substring(0, 8))
	$previous = $env:GIT_INDEX_FILE
	$env:GIT_INDEX_FILE = $indexFile
	try {
		# From the baseline tree, which carries no path under Vendor/, so the recorded gitlink is the
		# only difference the reviewed diff has to resolve.
		Invoke-FixtureGit $root @('read-tree', $baseline)
		# A recorded gitlink needs no submodule repository and no privileges: the commit is written
		# straight into a temporary index, and it is the branch tip the upward search would return.
		Invoke-FixtureGit $root @('update-index', '--add', '--cacheinfo', "160000,$branchTip,Vendor/Sub")
		$tree = (Get-FixtureGitText $root @('write-tree')).Trim()
		$candidate = (Get-FixtureGitText $root @('commit-tree', $tree, '-p', $baseline, '-m', 'candidate')).Trim()
	}
	finally {
		if ($null -eq $previous) { Remove-Item -LiteralPath 'Env:GIT_INDEX_FILE' -ErrorAction SilentlyContinue }
		else { $env:GIT_INDEX_FILE = $previous }
		Remove-Item -LiteralPath $indexFile -Force -ErrorAction SilentlyContinue
	}
	$treeText = Get-FixtureGitText $root @('ls-tree', $candidate, '--', 'Vendor/Sub')
	Assert-True ($treeText.Contains("160000 commit $branchTip")) 'candidate tree records the gitlink at the repository head'
	$scopeFile = New-ScratchFile 'scope' (New-VerifyScopeText $baseline $candidate)

	$promptPath = New-ScratchPath 'gitlink'
	$run = Invoke-PromptScript @(
		'-RepositoryRoot', $root, '-Baseline', $baseline, '-AssignedSkill', 'verify-changes',
		'-ScopeFile', $scopeFile, '-PromptPath', $promptPath, '-Head', $candidate)
	Assert-Equal 2 $run.ExitCode 'plain directory under a recorded gitlink exit code'
	if ($null -ne $run.Json) {
		Assert-Equal 'blocked' $run.Json.status 'plain directory under a recorded gitlink status'
		Assert-Equal 'prompt.head-required' $run.Json.code 'plain directory under a recorded gitlink code'
		Assert-True ($run.Json.message.Contains('Vendor/Sub')) 'plain directory under a recorded gitlink is named'
	}
	else { Assert-True $false 'plain directory under a recorded gitlink emitted JSON' }
	Assert-True (-not (Test-Path -LiteralPath $promptPath)) 'plain directory under a recorded gitlink creates no prompt file'
}

# --- Repository F: the 4 MB prompt budget --------------------------------------------------------

function Test-DiffTooLarge() {
	$root = New-FixtureRoot 'oversize'
	Add-FixtureSkill $root @('repo-code-review')
	Set-FixtureText $root 'Engine/Source/Small.cpp' "small`n"
	Invoke-FixtureGit $root @('add', '--all')
	Invoke-FixtureGit $root @('commit', '--quiet', '-m', 'baseline')
	$baseline = (Get-FixtureGitText $root @('rev-parse', 'HEAD')).Trim()
	Set-FixtureText $root 'Engine/Source/Small.cpp' "small`nchanged`n"
	$builder = [Text.StringBuilder]::new()
	# Just over the 4 MB prompt budget on its own, so only the budget rule can decide this case.
	for ($index = 0; $index -lt 150000; $index++) { [void] $builder.Append("oversized evidence line $index`n") }
	Set-FixtureText $root 'Huge.txt' $builder.ToString()

	$scopeFile = New-ScratchFile 'scope' $script:ScopeText
	$promptPath = New-ScratchPath 'oversize'
	$run = Invoke-PromptScript @(
		'-RepositoryRoot', $root, '-Baseline', $baseline, '-AssignedSkill', 'repo-code-review',
		'-ScopeFile', $scopeFile, '-PromptPath', $promptPath, '-UntrackedPath', 'Huge.txt')
	Assert-Equal 2 $run.ExitCode 'oversize exit code'
	if ($null -ne $run.Json) {
		Assert-Equal 'blocked' $run.Json.status 'oversize status'
		Assert-Equal 'prompt.diff-too-large' $run.Json.code 'oversize code'
		Assert-True ($run.Json.message.Contains('Huge.txt')) 'oversize message names the largest contributing path'
		Assert-Equal 0 $run.Json.promptBytes 'oversize reports no prompt bytes'
		Assert-True ($null -eq $run.Json.targetsPath) 'oversize reports no targets path'
	}
	else { Assert-True $false 'oversize emitted JSON' }
	Assert-True (-not (Test-Path -LiteralPath $promptPath)) 'oversize leaves no partial prompt file'
	Assert-True (-not (Test-Path -LiteralPath (Get-TargetsPath $promptPath))) 'oversize deletes the targets file it created before the prompt'
	Assert-True (-not $run.Stdout.Contains('oversized evidence line')) 'oversize keeps evidence off stdout'
}

try {
	[void] (New-Item -ItemType Directory -Path $script:Scratch -Force)
	$repositoryA = New-RepositoryA
	Test-AssembledPrompt $repositoryA
	Test-RiskTierLine $repositoryA
	Test-PromptPathExists $repositoryA
	Test-ScopeFileMissing $repositoryA
	Test-UnlistedUntracked $repositoryA
	Test-TargetsSiblingExists $repositoryA
	Test-AssignedSkillUnknown $repositoryA
	Test-MixedCaseAssignedSkill $repositoryA
	Test-PlanAuditExecutionCard $repositoryA
	Test-VerifyChangesTypedArtifacts
	Test-HeadHonoured
	Test-ZeroTargets
	Test-UntrackedRenameTargets
	Test-VerifyChangesHead
	Test-VerifyChangesCandidateHead
	Test-VerifyChangesReviewedPathShapes
	Test-VerifyChangesGitlinkDirectory
	Test-DiffTooLarge
}
finally {
	foreach ($root in $script:Roots) {
		if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue }
	}
	if (Test-Path -LiteralPath $script:Scratch) { Remove-Item -LiteralPath $script:Scratch -Recurse -Force -ErrorAction SilentlyContinue }
}

if ($script:Failures.Count -gt 0) {
	Write-Host "FAILED: $($script:Failures.Count) fixture assertion(s)."
	exit 1
}

Write-Host 'All Codex review prompt fixtures passed.'
exit 0
