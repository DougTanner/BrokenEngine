[CmdletBinding()]
param(
	[Parameter(Mandatory)][string] $Executable
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$utf8 = [Text.UTF8Encoding]::new($false, $true)
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..\..'))
$fixtureRoot = Join-Path $repositoryRoot "Temp/NextPlanWorkflowFixtures/$([guid]::NewGuid().ToString('N'))"
# LOCALAPPDATA is isolated to the scratch claims store; restore it last so cleanup can still reach it.
$originalLocalAppData = [Environment]::GetEnvironmentVariable('LOCALAPPDATA')
$originalBaseline = [Environment]::GetEnvironmentVariable('BROKEN_ENGINE_BASELINE')

function Invoke-Process([string] $FilePath, [string[]] $Arguments, [string] $WorkingDirectory) {
	$start = [Diagnostics.ProcessStartInfo]::new()
	$start.FileName = $FilePath
	$start.WorkingDirectory = $WorkingDirectory
	$start.UseShellExecute = $false
	$start.CreateNoWindow = $true
	$start.RedirectStandardOutput = $true
	$start.RedirectStandardError = $true
	foreach ($argument in $Arguments) { [void]$start.ArgumentList.Add($argument) }
	$process = [Diagnostics.Process]::new()
	$process.StartInfo = $start
	if (-not $process.Start()) { throw "Could not start '$FilePath'." }
	$stdout = $process.StandardOutput.ReadToEndAsync()
	$stderr = $process.StandardError.ReadToEndAsync()
	$process.WaitForExit()
	$result = [pscustomobject]@{ ExitCode = $process.ExitCode; Stdout = $stdout.GetAwaiter().GetResult(); Stderr = $stderr.GetAwaiter().GetResult() }
	$process.Dispose()
	return $result
}

function Invoke-Git([string] $WorkingDirectory, [string[]] $Arguments) {
	$result = Invoke-Process 'git.exe' $Arguments $WorkingDirectory
	if ($result.ExitCode -ne 0) { throw "git $($Arguments -join ' ') failed: $($result.Stdout)$($result.Stderr)" }
	return $result.Stdout.Trim()
}

function Set-Utf8File([string] $Path, [string] $Text) {
	[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Path)) | Out-Null
	[IO.File]::WriteAllText($Path, $Text, $utf8)
}

function Invoke-WorkflowScript([string] $Name, [string[]] $Arguments, [int] $ExpectedExit) {
	$response = Invoke-Process (Join-Path $PSHOME 'pwsh.exe') (@('-NoLogo','-NoProfile','-File',(Join-Path $PSScriptRoot $Name)) + $Arguments) $script:session
	if ($response.ExitCode -ne $ExpectedExit) { throw "$Name exited $($response.ExitCode), expected $ExpectedExit. stdout=$($response.Stdout) stderr=$($response.Stderr)" }
	try { return $response.Stdout | ConvertFrom-Json -Depth 100 -ErrorAction Stop }
	catch { throw "$Name did not return exactly one JSON value: $($response.Stdout)" }
}

function Assert-True([bool] $Condition, [string] $Message) {
	if (-not $Condition) { throw $Message }
}

try {
	$primary = Join-Path $fixtureRoot 'primary'
	$script:session = Join-Path $fixtureRoot 'session'
	[IO.Directory]::CreateDirectory($primary) | Out-Null
	Invoke-Git $primary @('init','--initial-branch=main','.') | Out-Null
	Invoke-Git $primary @('config','user.email','next-plan-fixture@example.invalid') | Out-Null
	Invoke-Git $primary @('config','user.name','Next Plan Fixture') | Out-Null
	Invoke-Git $primary @('config','core.autocrlf','false') | Out-Null
	Set-Utf8File (Join-Path $primary '.gitignore') "Temp/`nTools/WorktreeCli/Platforms/VisualStudio2026/Output/`n"
	$plan = 'Documents/Plans/TestPlan.md'
	Set-Utf8File (Join-Path $primary $plan) "<!-- broken-engine-plan/v1 {`"createdUtc`":`"2024-01-01T00:00:00.000Z`",`"dependsOn`":[]} -->`n# Test plan`n`nImplement the fixture behavior.`n"
	$uniqueFilenamePlan = 'Documents/Plans/Unique/UniqueFilename.md'
	Set-Utf8File (Join-Path $primary $uniqueFilenamePlan) "<!-- broken-engine-plan/v1 {`"createdUtc`":`"2024-01-02T00:00:00.000Z`",`"dependsOn`":[]} -->`n# Unique filename plan`n"
	Set-Utf8File (Join-Path $primary 'Documents/Plans/AGENTS.md') "# Directory guidance`n"
	Invoke-Git $primary @('add','--all') | Out-Null
	Invoke-Git $primary @('commit','-m','fixture baseline') | Out-Null
	# Isolate the machine-local claims store so this fixture never touches the real one.
	$env:LOCALAPPDATA = Join-Path $fixtureRoot 'localappdata'
	[IO.Directory]::CreateDirectory($env:LOCALAPPDATA) | Out-Null
	[Environment]::SetEnvironmentVariable('BROKEN_ENGINE_BASELINE', $null)
	$primaryOutput = Join-Path $primary 'Tools/WorktreeCli/Platforms/VisualStudio2026/Output'
	$sessionOutput = Join-Path $script:session 'Tools/WorktreeCli/Platforms/VisualStudio2026/Output'
	[IO.Directory]::CreateDirectory($primaryOutput) | Out-Null
	Copy-Item -LiteralPath (Get-Item -LiteralPath $Executable -Force).FullName -Destination (Join-Path $primaryOutput 'WorktreeCli.exe')
	# Session identity is derived from the Git branch, so the session branch must be codex/<guid>.
	$sessionBranch = "codex/$([guid]::NewGuid().ToString())"
	Invoke-Git $primary @('worktree','add','-b',$sessionBranch,$script:session,'HEAD') | Out-Null
	# The session module resolves the landing target through this sidecar, which the wrapper scripts write.
	Set-Utf8File (Join-Path $script:session 'Temp/session-sidecar.json') "{`"schemaVersion`":`"broken-engine-session-sidecar/v1`",`"targetBranch`":`"main`"}"
	[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($sessionOutput)) | Out-Null
	New-Item -ItemType Junction -Path $sessionOutput -Target $primaryOutput | Out-Null

	# No claim: both terminal scripts are a clean pass that mutates nothing.
	$noClaimDeferral = Invoke-WorkflowScript 'Defer-NextPlan.ps1' @() 0
	Assert-True ($noClaimDeferral.status -ceq 'pass' -and $noClaimDeferral.code -ceq 'no-claim') 'Deferral without a claim was not a clean no-claim pass.'
	$noClaimCompletion = Invoke-WorkflowScript 'Complete-NextPlan.ps1' @() 0
	Assert-True ($noClaimCompletion.status -ceq 'pass' -and $noClaimCompletion.code -ceq 'no-claim' -and (Test-Path -LiteralPath (Join-Path $script:session $plan))) 'Completion without a claim was not a clean no-claim pass.'

	# Listing reports every executable Plan as eligible and changes no tree or claim state.
	$listing = Invoke-WorkflowScript 'Get-NextPlanList.ps1' @() 0
	$listedPaths = @($listing.plans | ForEach-Object { $_.path })
	Assert-True ($listedPaths -ccontains $plan -and $listedPaths -ccontains $uniqueFilenamePlan) 'Listing did not report every executable Plan.'
	Assert-True (@($listing.plans | Where-Object { $_.state -cne 'eligible' }).Count -eq 0) 'Listing did not report the unclaimed Plans as eligible.'
	Assert-True ($listing.planCount -eq $listedPaths.Count -and -not $listing.truncated -and $listing.stateCounts.eligible -eq $listedPaths.Count) 'Listing projection counts did not match its rows.'
	$boundedListing = Invoke-WorkflowScript 'Get-NextPlanList.ps1' @('-Top','1') 0
	Assert-True (@($boundedListing.plans).Count -eq 1 -and $boundedListing.truncated -and $boundedListing.planCount -eq $listedPaths.Count) 'Listing -Top did not bound the projection.'
	Assert-True ([string]::IsNullOrWhiteSpace((Invoke-Git $script:session @('status','--porcelain=v1','--untracked-files=all')))) 'Listing changed the session worktree.'
	$listingNoClaim = Invoke-WorkflowScript 'Defer-NextPlan.ps1' @() 0
	Assert-True ($listingNoClaim.code -ceq 'no-claim') 'Listing created a Plan claim.'

	# A dirty session worktree is a deterministic claim blocker before any scheduler access.
	Set-Utf8File (Join-Path $script:session 'Dirty.txt') "uncommitted claim blocker`n"
	$dirtyClaim = Invoke-WorkflowScript 'Invoke-NextPlanClaim.ps1' @('-Plan',$plan) 2
	Assert-True ($dirtyClaim.code -ceq 'claim.worktree-dirty' -and $dirtyClaim.message -clike '*Dirty.txt*') 'A dirty session worktree was not a deterministic claim blocker naming its dirty paths.'
	$bareDirtyClaim = Invoke-WorkflowScript 'Invoke-NextPlanClaim.ps1' @() 2
	Assert-True ($bareDirtyClaim.code -ceq 'claim.worktree-dirty') 'A bare invocation did not block on the dirty session worktree.'
	Remove-Item -LiteralPath (Join-Path $script:session 'Dirty.txt') -Force

	# Primary moves ahead so the retained-work resume exercises its fast-forward overlap guard.
	Set-Utf8File (Join-Path $primary 'Source/Shared.txt') "primary moved`n"
	Invoke-Git $primary @('add','--all') | Out-Null
	Invoke-Git $primary @('commit','-m','primary movement') | Out-Null
	$retainedFile = Join-Path $script:session 'Retained.txt'
	Set-Utf8File $retainedFile "retained implementation`n"
	$retainedContent = [Convert]::ToBase64String([IO.File]::ReadAllBytes($retainedFile))
	# A retained path containing a space proves the reported paths are raw, not Git's quoted display form.
	$spacedRetainedFile = Join-Path $script:session 'Retained Work.txt'
	Set-Utf8File $spacedRetainedFile "retained implementation under a spaced path`n"
	# A retained path the incoming fast-forward also changes is never safe to claim onto.
	Set-Utf8File (Join-Path $script:session 'Source/Shared.txt') "session edit of an incoming path`n"
	$overlapResume = Invoke-WorkflowScript 'Invoke-NextPlanClaim.ps1' @('-Plan',$plan,'-ResumeRetained') 2
	Assert-True ($overlapResume.code -ceq 'claim.worktree-dirty' -and $overlapResume.message -clike '*Source/Shared.txt*') 'A fast-forward overlapping retained work was not a resume blocker.'
	Remove-Item -LiteralPath (Join-Path $script:session 'Source/Shared.txt') -Force
	# Uncommitted scheduler input blocks the resume even when the fast-forward would not touch it.
	Set-Utf8File (Join-Path $script:session 'Documents/Plans/Scratch/Dirty.md') "uncommitted scheduler input`n"
	$schedulerResume = Invoke-WorkflowScript 'Invoke-NextPlanClaim.ps1' @('-Plan',$plan,'-ResumeRetained') 2
	Assert-True ($schedulerResume.code -ceq 'claim.worktree-dirty' -and $schedulerResume.message -clike '*Documents/Plans/Scratch/Dirty.md*') 'Uncommitted scheduler input was not a resume blocker.'
	Remove-Item -LiteralPath (Join-Path $script:session 'Documents/Plans/Scratch/Dirty.md') -Force
	# A scheduler path containing a space is quoted in Git's display output, so it must still be recognized as scheduler input.
	Set-Utf8File (Join-Path $script:session 'Documents/Plans/Scratch/Dirty Path.md') "uncommitted scheduler input under a spaced path`n"
	$spacedSchedulerResume = Invoke-WorkflowScript 'Invoke-NextPlanClaim.ps1' @('-Plan',$plan,'-ResumeRetained') 2
	Assert-True ($spacedSchedulerResume.code -ceq 'claim.worktree-dirty' -and $spacedSchedulerResume.message -clike '*Documents/Plans/Scratch/Dirty Path.md*') 'A spaced uncommitted scheduler path was not a resume blocker.'
	Remove-Item -LiteralPath (Join-Path $script:session 'Documents/Plans/Scratch/Dirty Path.md') -Force
	# Non-overlapping retained work claims, fast-forwards, and stays byte-identical across claim and deferral.
	$resumeClaim = Invoke-WorkflowScript 'Invoke-NextPlanClaim.ps1' @('-Plan',$plan,'-ResumeRetained') 0
	Assert-True ($resumeClaim.status -ceq 'pass' -and $resumeClaim.claim.plan -ceq $plan -and $resumeClaim.sync.fastForwarded) 'A resume claim over non-overlapping retained work did not claim and fast-forward.'
	Assert-True ($resumeClaim.retained.count -eq 2 -and (@($resumeClaim.retained.paths) -ccontains 'Retained.txt') -and (@($resumeClaim.retained.paths) -ccontains 'Retained Work.txt')) 'A resume claim did not report its retained paths.'
	Assert-True ([Convert]::ToBase64String([IO.File]::ReadAllBytes($retainedFile)) -ceq $retainedContent) 'A resume claim did not leave the retained file byte-identical.'
	$retainedDeferral = Invoke-WorkflowScript 'Defer-NextPlan.ps1' @() 0
	Assert-True ($retainedDeferral.code -ceq 'released' -and (@($retainedDeferral.retained.paths) -ccontains 'Retained.txt') -and (@($retainedDeferral.retained.paths) -ccontains 'Retained Work.txt')) 'Deferral did not report the retained work.'
	Assert-True ([Convert]::ToBase64String([IO.File]::ReadAllBytes($retainedFile)) -ceq $retainedContent) 'Deferral did not leave the retained file byte-identical.'
	Remove-Item -LiteralPath $retainedFile -Force
	Remove-Item -LiteralPath $spacedRetainedFile -Force

	# Bare selection takes the oldest eligible Plan; deferral releases it.
	$bareClaim = Invoke-WorkflowScript 'Invoke-NextPlanClaim.ps1' @() 0
	Assert-True ($bareClaim.status -ceq 'pass' -and $bareClaim.code -ceq 'ok' -and $bareClaim.claim.plan -ceq $plan) 'Bare invocation did not claim the oldest eligible Plan.'
	$deferral = Invoke-WorkflowScript 'Defer-NextPlan.ps1' @() 0
	Assert-True ($deferral.status -ceq 'pass' -and $deferral.code -ceq 'released' -and $deferral.claim.released) 'Deferral did not release the live claim.'

	# Partial inputs resolve against the validated executable Plans.
	$uniqueFilenameClaim = Invoke-WorkflowScript 'Invoke-NextPlanClaim.ps1' @('-Plan','UniqueFilename.md') 0
	Assert-True ($uniqueFilenameClaim.status -ceq 'pass' -and $uniqueFilenameClaim.claim.plan -ceq $uniqueFilenamePlan) 'A unique filename-only input did not claim its canonical Plan path.'
	Invoke-WorkflowScript 'Defer-NextPlan.ps1' @() 0 | Out-Null
	$interiorPartialClaim = Invoke-WorkflowScript 'Invoke-NextPlanClaim.ps1' @('-Plan','nique/UniqueFile') 0
	Assert-True ($interiorPartialClaim.status -ceq 'pass' -and $interiorPartialClaim.claim.plan -ceq $uniqueFilenamePlan) 'An interior-substring partial input did not claim its canonical Plan path.'
	Invoke-WorkflowScript 'Defer-NextPlan.ps1' @() 0 | Out-Null
	$missingFilename = Invoke-WorkflowScript 'Invoke-NextPlanClaim.ps1' @('-Plan','MissingFilename.md') 2
	Assert-True ($missingFilename.status -ceq 'blocked' -and $missingFilename.code -ceq 'plan-name-not-found') 'A nonexistent filename-only input was not a deterministic blocker.'
	$ambiguousPartial = Invoke-WorkflowScript 'Invoke-NextPlanClaim.ps1' @('-Plan','.md') 2
	Assert-True ($ambiguousPartial.status -ceq 'blocked' -and $ambiguousPartial.code -ceq 'plan-name-ambiguous') 'An ambiguous partial input was not a deterministic blocker.'
	Assert-True ((@($ambiguousPartial.candidates) -join '|') -ceq (@($plan,$uniqueFilenamePlan) -join '|')) 'An ambiguous partial input did not report its sorted candidate list.'

	# Claiming an already-claimed Plan from the same session is idempotent.
	$claim = Invoke-WorkflowScript 'Invoke-NextPlanClaim.ps1' @('-Plan',$plan) 0
	Assert-True ($claim.status -ceq 'pass' -and $claim.code -ceq 'ok' -and $claim.claim.plan -ceq $plan) 'Claim result did not bind the selected plan.'
	$reusedClaim = Invoke-WorkflowScript 'Invoke-NextPlanClaim.ps1' @('-Plan',$plan) 0
	Assert-True ($reusedClaim.status -ceq 'pass' -and $reusedClaim.code -ceq 'reused' -and $reusedClaim.claim.plan -ceq $plan) 'A live claim was not reused idempotently.'

	# A live claim is visible in the listing, which still leaves that claim untouched.
	$claimedListing = Invoke-WorkflowScript 'Get-NextPlanList.ps1' @() 0
	$claimedRow = @($claimedListing.claimed | Where-Object { $_.path -ceq $plan })[0]
	Assert-True ($claimedRow.state -ceq 'claimed') 'Listing did not report the live claim.'
	$listedClaim = Invoke-WorkflowScript 'Invoke-NextPlanClaim.ps1' @('-Plan',$plan) 0
	Assert-True ($listedClaim.code -ceq 'reused' -and $listedClaim.claim.plan -ceq $plan) 'Listing changed the live claim.'

	# Terminal preparation deletes the Plan file, reports its changed paths, and keeps the claim.
	Set-Utf8File (Join-Path $script:session 'Source/Implemented.txt') "implemented`n"
	$completion = Invoke-WorkflowScript 'Complete-NextPlan.ps1' @() 0
	Assert-True ($completion.status -ceq 'pass' -and $completion.code -ceq 'ok' -and $completion.claim.disposition -ceq 'completed') 'Terminal preparation did not report a completed disposition.'
	Assert-True (@($completion.changes.items | ForEach-Object { $_.path }) -contains $plan -and $completion.changes.totalCount -ge 1) 'Terminal preparation did not report the deleted Plan in its changed paths.'
	Assert-True (-not (Test-Path -LiteralPath (Join-Path $script:session $plan))) 'Terminal preparation retained the selected plan file.'
	Assert-True ($completion.nextAction -ceq 'finalize-changes' -and -not $completion.workflowTerminal) 'Terminal preparation did not route to finalization.'
	$retainedClaim = Invoke-WorkflowScript 'Defer-NextPlan.ps1' @() 0
	Assert-True ($retainedClaim.status -ceq 'pass' -and $retainedClaim.code -ceq 'released') 'Terminal preparation did not retain the claim until landing.'

	[pscustomobject]@{
		schemaVersion = 'broken-engine-next-plan-workflow-fixtures/v1'
		status = 'pass'
		cases = @('no-claim deferral','no-claim completion','read-only eligible listing','dirty-tree claim rejection','bare dirty-tree claim rejection','fast-forward overlap resume rejection','scheduler-input resume rejection','spaced scheduler-input resume rejection','retained-work resume claim','retained-work deferral report','bare oldest eligible selection','claim deferral release','unique filename-only claim','interior-substring partial claim','missing filename-only blocker','ambiguous partial blocker','explicit plan claim','idempotent claim reuse','claimed-state listing','terminal preparation changed paths','retained claim after terminal preparation')
	} | ConvertTo-Json -Depth 5
}
finally {
	if (Test-Path -LiteralPath $fixtureRoot) { Remove-Item -LiteralPath $fixtureRoot -Recurse -Force }
	if ($null -eq $originalLocalAppData) { [Environment]::SetEnvironmentVariable('LOCALAPPDATA', $null) }
	else { $env:LOCALAPPDATA = $originalLocalAppData }
	[Environment]::SetEnvironmentVariable('BROKEN_ENGINE_BASELINE', $originalBaseline)
}
