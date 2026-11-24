# Coordinator for /sweep (.agents/skills/sweep/SKILL.md). Refuses codex/ session branches in every mode.
# Every Codex run uses the one -Model at reasoning effort high; agents hand off through files under Temp/Sweep/,
# and Status.txt carries the run's state. -Spec must point to a file under Documents/Investigations/ or
# Projects/Coretide/Documents/Investigations/.
# -ListModels prints the list-visible Codex model slugs, marking the highest gpt-<version>-sol (recommended).
# -Spec <path> -List prints the sweep specification's batches and unit counts in run order: the ## Sweep Batches
# entries first in their listed order, then the remaining top-level batches by name.
# -Spec <path> -Model <slug> -Batch <name> runs one batch (resumable); -Unit <stem> runs one unit only.
# -Throttle <n> sets how many units of a -Batch run execute in parallel (default 8).
# -Spec <path> -Model <slug> -Close runs the stage close: Cleanup, Ordering, and the deferred-fixes ledger.
param(
	[string] $Spec,
	[string] $Model,
	[string] $Batch,
	[string] $Unit,
	[switch] $List,
	[switch] $ListModels,
	[switch] $Close,
	[int] $Throttle = 8
)

$ErrorActionPreference = 'Stop'
Remove-Item Env:OPENAI_API_KEY -ErrorAction SilentlyContinue
$rootRaw = git rev-parse --show-toplevel
if ($LASTEXITCODE -ne 0) { throw "Failed to determine git repository root (git rev-parse --show-toplevel exited $LASTEXITCODE)" }
$root = [IO.Path]::GetFullPath(($rootRaw.Trim() -replace '/', '\'))
$branch = git -C $root rev-parse --abbrev-ref HEAD
if ($LASTEXITCODE -ne 0) { throw "Failed to determine current git branch in '$root' (git rev-parse --abbrev-ref HEAD exited $LASTEXITCODE)" }
if ($branch -like 'codex/*') { throw "/sweep cannot run from Codex session branch '$branch'; run from a non-Codex host session (Claude Code or Antigravity CLI)" }
$sweep = Join-Path $root 'Temp\Sweep'
$status = Join-Path $sweep 'Status.txt'
$baselineFile = Join-Path $sweep 'Baseline.txt'
$skillRel = '.agents/skills/sweep'

if ($ListModels)
{
	Import-Module (Join-Path $root '.agents\scripts\AgentScriptCommon.psm1') -Force
	$codexApp = Get-Command codex -CommandType Application | Select-Object -First 1
	$run = Invoke-AgentProcess $codexApp.Source @('debug', 'models') $root
	if ($run.ExitCode -ne 0) { throw "codex debug models exited $($run.ExitCode)" }
	$slugs = @(($run.Stdout | ConvertFrom-Json).models | Where-Object { $_.visibility -ceq 'list' } | ForEach-Object { $_.slug })
	$recommended = $null
	$recommendedVersion = $null
	foreach ($slug in $slugs)
	{
		$family = Get-AgentModelFamily $slug
		if (($null -ne $family) -and ($family.Family -ceq 'sol') -and (($null -eq $recommendedVersion) -or ((Compare-AgentModelVersion $family.Version $recommendedVersion) -gt 0)))
		{
			$recommended = $slug
			$recommendedVersion = $family.Version
		}
	}
	foreach ($slug in $slugs) { if ($slug -ceq $recommended) { "$slug (recommended)" } else { $slug } }
	return
}

if (-not $Spec) { throw '-Spec is required except with -ListModels' }
$specFull = [IO.Path]::GetFullPath($Spec, $root)
$allowedRoots = @(
	([IO.Path]::GetFullPath((Join-Path $root 'Documents\Investigations')) + [IO.Path]::DirectorySeparatorChar)
	([IO.Path]::GetFullPath((Join-Path $root 'Projects\Coretide\Documents\Investigations')) + [IO.Path]::DirectorySeparatorChar)
)
$inAllowedRoot = $false
foreach ($allowedRoot in $allowedRoots)
{
	if (($specFull.Length -gt $allowedRoot.Length) -and $specFull.StartsWith($allowedRoot, [StringComparison]::OrdinalIgnoreCase))
	{
		$inAllowedRoot = $true
		break
	}
}
if (-not $inAllowedRoot) { throw "-Spec must be under Documents/Investigations/ or Projects/Coretide/Documents/Investigations/: '$Spec'" }
if (-not (Test-Path -LiteralPath $specFull -PathType Leaf)) { throw "-Spec file not found: '$Spec'" }
$specRel = [IO.Path]::GetRelativePath($root, $specFull) -replace '\\', '/'
$specText = Get-Content -Raw -LiteralPath $specFull
$sweepSection = [regex]::Match($specText, '(?ms)^## Sweep[ \t]*\r?\n(.*?)(?=^## |\z)')
if (-not $sweepSection.Success) { throw "$specRel has no ## Sweep section" }

# The backticked values of one `- <Name>:` bullet of the specification's ## Sweep section.
function Get-SweepValues([string] $Name)
{
	$bullet = [regex]::Match($sweepSection.Groups[1].Value, "(?ms)^- ${Name}:(.*?)(?=^- |\z)")
	return @([regex]::Matches($bullet.Groups[1].Value, '`([^`]+)`') | ForEach-Object { $_.Groups[1].Value })
}

$typeName = @(Get-SweepValues 'Type')
if ($typeName.Count -ne 1) { throw "$specRel ## Sweep must name exactly one Type" }
$typeRel = "$skillRel/references/types/$($typeName[0]).md"
if (-not (Test-Path -LiteralPath (Join-Path $root $typeRel))) { throw "sweep type '$($typeName[0])' has no file $typeRel" }
$targets = @(Get-SweepValues 'Targets')
if ($targets.Count -eq 0) { throw "$specRel ## Sweep names no Targets" }
$ownBatchDirs = @(Get-SweepValues 'Batches')
$promptDir = Join-Path $root "$skillRel\references\prompts"

$files = @(git -C $root ls-files -- @targets)
$blocked = @($files | Where-Object { $_.StartsWith('.agents/') })
if ($blocked.Count -gt 0) { throw "Codex workspace-write cannot edit .agents/; remove these targets: $($blocked -join ', ')" }
$units = $files | Group-Object { $_ -replace '\.(h|cpp)$', '' } | ForEach-Object {
	$stem = $_.Name
	$dir = $ownBatchDirs | Where-Object { $stem.StartsWith("$_/") } | Select-Object -First 1
	if (-not $dir) { $dir = $stem.Split('/')[0] }
	[pscustomobject] @{ Stem = $stem; Id = ($stem -replace '[/\\]', '_'); Files = @($_.Group); Batch = ($dir -replace '/', '_') }
}

if ($List)
{
	$order = @($ownBatchDirs -replace '/', '_')
	$units | Group-Object Batch | Sort-Object { $i = [array]::IndexOf($order, $_.Name); if ($i -lt 0) { $order.Count } else { $i } }, Name | ForEach-Object { '{0} {1}' -f $_.Name, $_.Count }
	return
}

if (-not $Model) { throw '-Model is required with -Batch and -Close' }

# Fills the template's {{KEY}} placeholders and runs it on Codex, retrying once; true when the run wrote $Out.
function Invoke-SweepCodex([string] $Template, [string] $Out, [string] $Sandbox, [hashtable] $Vars, [string] $Root, [string] $Model)
{
	$prompt = Get-Content -Raw $Template
	foreach ($k in $Vars.Keys) { $prompt = $prompt.Replace("{{$k}}", $Vars[$k]) }
	$codex = (Get-Command codex).Source
	for ($attempt = 1; $attempt -le 2; $attempt++)
	{
		$prompt | & $codex -a never exec --sandbox $Sandbox -C $Root -m $Model -c 'model_reasoning_effort="high"' --disable hooks --ephemeral -o $Out - 2> "$Out.stderr.txt" | Out-Null
		if ($LASTEXITCODE -eq 0 -and (Test-Path $Out)) { return $true }
	}
	return $false
}

# The body of one ## section of a Codex output, or nothing when it is absent or none.
function Get-Section([string] $Path, [string] $Heading)
{
	$text = Get-Content -Raw $Path
	$m = [regex]::Match($text, "(?ms)^## $Heading\s*\r?\n(.*?)(?=^## |^FIX-DONE|^PROPAGATE-DONE)")
	$body = $m.Groups[1].Value.Trim()
	if ($body -and $body -notmatch '^(-\s*)?none$') { return $body }
}

# The final non-empty line (trailing whitespace trimmed) of a Codex output when it matches $Pattern, or $null.
function Get-SweepReceiptLine([string] $Path, [string] $Pattern)
{
	if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
	$last = Get-Content -LiteralPath $Path | ForEach-Object { $_.TrimEnd() } | Where-Object { $_ } | Select-Object -Last 1
	if ($last -and ($last -cmatch $Pattern)) { return $last }
	return $null
}

# The build fields main branches on, from the C++ files changed since the given commit: Client and Server for any
# change, DataPacker when one is under DataPacker/ or Common/ (DataPacker's Pch.h includes Common.h), WorktreeCli and
# AgentHarness when one is under Tools/ or is Common/Sha256Hasher.h (ToolCommon includes it by path) or
# Common/DebugDefines.h (the tools force-include it), and the Profile build when one now mentions BT_PROFILE.
function Get-BuildFields([string] $Since)
{
	$cpp = @(git -C $root diff --name-only $Since -- '*.h' '*.cpp')
	if ($cpp.Count -eq 0) { return 'cpp=no builds=none profile=no' }
	$builds = @('Client', 'Server')
	if ($cpp | Where-Object { $_ -match '^(DataPacker|Common)/' }) { $builds += 'DataPacker' }
	if ($cpp | Where-Object { $_.StartsWith('Tools/') -or $_ -eq 'Common/Sha256Hasher.h' -or $_ -eq 'Common/DebugDefines.h' }) { $builds += 'WorktreeCli', 'AgentHarness' }
	# Greps every C++ file rather than passing the changed paths, which can exceed the command-line length limit.
	$profiled = @(git -C $root grep -l 'BT_PROFILE' -- '*.h' '*.cpp')
	$profileBuild = if ($cpp | Where-Object { $profiled -contains $_ }) { 'yes' } else { 'no' }
	return "cpp=yes builds=$($builds -join ',') profile=$profileBuild"
}

# Reads Temp/Sweep/Baseline.txt (line 1: baseline SHA, line 2: owning spec path) and verifies HEAD and ownership.
function Read-SweepBaseline
{
	$lines = @(Get-Content -LiteralPath $baselineFile -TotalCount 2)
	$baseline = if ($lines.Count -ge 1) { $lines[0].Trim() } else { '' }
	$recordedSpec = if ($lines.Count -ge 2) { $lines[1].Trim() } else { '' }
	if (-not $baseline) { throw 'Temp/Sweep/Baseline.txt is missing its baseline SHA on line 1' }
	if (-not $recordedSpec) { throw 'Temp/Sweep/Baseline.txt is missing its sweep spec path on line 2' }
	$headRaw = git -C $root rev-parse HEAD
	if ($LASTEXITCODE -ne 0) { throw "Failed to resolve HEAD in '$root' (git rev-parse HEAD exited $LASTEXITCODE)" }
	$head = $headRaw.Trim()
	if ($baseline -cne $head)
	{
		throw "Temp/Sweep/Baseline.txt belongs to commit $baseline (HEAD is $head); delete Temp/Sweep before starting a new sweep"
	}
	if (-not [string]::Equals($recordedSpec, $specRel, [StringComparison]::OrdinalIgnoreCase))
	{
		throw "Temp/Sweep/Baseline.txt belongs to '$recordedSpec', not '$specRel'"
	}
	return $baseline
}

if ($Close)
{
	# Every failure writes CLOSE-FAILED <run>, so main's wait on Status.txt always ends.
	New-Item -ItemType Directory -Force -Path $sweep | Out-Null
	$closeRun = 'Baseline'
	try
	{
		if (-not (Test-Path -LiteralPath $baselineFile)) { throw 'no -Batch run has written Temp/Sweep/Baseline.txt' }
		$baseline = Read-SweepBaseline
		"RUNNING close $(Get-Date -Format s)" | Set-Content $status
		$vars = @{ ROOT = $root; TYPE = $typeRel; SPEC = $specRel; BASELINE = $baseline }

		$closeRun = 'Cleanup'
		$cleanupOut = Join-Path $sweep 'Cleanup.md'
		if (-not ((Test-Path $cleanupOut) -and (Select-String -Quiet -SimpleMatch 'CLEANUP-DONE' $cleanupOut)))
		{
			if (-not ((Invoke-SweepCodex (Join-Path $promptDir 'Cleanup.md') $cleanupOut 'workspace-write' $vars $root $Model) -and (Select-String -Quiet -SimpleMatch 'CLEANUP-DONE' $cleanupOut))) { throw 'no CLEANUP-DONE output' }
		}

		$closeRun = 'Ordering'
		$buildFields = Get-BuildFields $baseline
		$ordering = 0
		if ($buildFields.StartsWith('cpp=yes'))
		{
			$orderingOut = Join-Path $sweep 'Ordering.md'
			if (-not ((Test-Path $orderingOut) -and (Select-String -Quiet 'ORDERING-DONE findings=(\d+)' $orderingOut)))
			{
				if (-not ((Invoke-SweepCodex (Join-Path $promptDir 'Ordering.md') $orderingOut 'read-only' $vars $root $Model) -and (Select-String -Quiet 'ORDERING-DONE findings=(\d+)' $orderingOut))) { throw 'no ORDERING-DONE findings=<n> output' }
			}
			$ordering = [int] (Select-String 'ORDERING-DONE findings=(\d+)' $orderingOut).Matches[0].Groups[1].Value
		}

		$closeRun = 'Ledger'
		# Writes the ledger into the type's deferred-fixes record as one ## <Spec file stem> section, replacing
		# that section wholesale so a rerun never duplicates it.
		$specStem = [IO.Path]::GetFileNameWithoutExtension($specRel)
		$typeText = Get-Content -Raw -LiteralPath (Join-Path $root $typeRel)
		$deferred = [regex]::Match($typeText, '(?ms)^## Deferred fixes[ \t]*\r?\n.*?`([^`]+)`').Groups[1].Value.Replace('<Spec file stem>', $specStem)
		if (-not $deferred) { throw "$typeRel ## Deferred fixes names no record" }
		$ledgerPath = Join-Path $sweep 'Ledger.md'
		$ledgerText = if (Test-Path $ledgerPath) { (Get-Content -Raw $ledgerPath) -replace '\r\n', "`n" } else { '' }
		$entries = @([regex]::Matches($ledgerText, '(?ms)^### .*?(?=^### |\z)') | ForEach-Object { '#' + $_.Value.TrimEnd() })
		$outOfBound = @($entries | Where-Object { $_ -match '\A#### [^\n]*\(out-of-bound\)' })
		$declined = @($entries | Where-Object { $_ -notmatch '\A#### [^\n]*\(out-of-bound\)' })
		$section = "## $specStem`n`n### Out of the fix bound`n`n$(if ($outOfBound) { $outOfBound -join "`n`n" } else { 'none' })`n`n### Declined`n`n$(if ($declined) { $declined -join "`n`n" } else { 'none' })`n"
		$recordPath = Join-Path $root $deferred
		$record = if (Test-Path -LiteralPath $recordPath) { (Get-Content -Raw -LiteralPath $recordPath) -replace '\r\n', "`n" } else { "# ${specStem}: deferred fixes`n" }
		$existing = [regex]::Match($record, "(?ms)^## $([regex]::Escape($specStem))[ \t]*\n.*?(?=^## |\z)")
		if ($existing.Success) { $record = $record.Remove($existing.Index, $existing.Length).Insert($existing.Index, "$section`n") }
		else { $record = $record.TrimEnd() + "`n`n$section" }
		[IO.File]::WriteAllText($recordPath, $record.TrimEnd() + "`n", [Text.UTF8Encoding]::new($false))

		$closeRun = 'Report'
		$unitLines = @{}
		if (Test-Path (Join-Path $sweep 'Progress.md'))
		{
			foreach ($m in (Select-String '^unit (\S+) (FIX-DONE.*)$' (Join-Path $sweep 'Progress.md'))) { $unitLines[$m.Matches[0].Groups[1].Value] = $m.Matches[0].Groups[2].Value }
		}
		$applied = 0
		foreach ($line in $unitLines.Values) { if ($line -match 'applied=(\d+)') { $applied += [int] $Matches[1] } }
		$deferredCount = [regex]::Matches($ledgerText, '(?m)^- ').Count
		"CLOSE-DONE units=$($unitLines.Count) applied=$applied deferred=$deferredCount ordering=$ordering $buildFields" | Set-Content $status
	}
	catch
	{
		"CLOSE-FAILED $closeRun $($_.Exception.Message)" | Set-Content $status
	}
	return
}

$batchUnits = @($units | Where-Object { $_.Batch -ceq $Batch -and (-not $Unit -or $_.Stem -ceq $Unit) })
if ($batchUnits.Count -eq 0) { throw "no units for batch '$Batch' unit '$Unit'" }
if (Test-Path -LiteralPath $baselineFile)
{
	[void] (Read-SweepBaseline)
	New-Item -ItemType Directory -Force -Path (Join-Path $sweep 'units') | Out-Null
}
else
{
	$dirty = @(git -C $root status --porcelain --untracked-files=no)
	if ($LASTEXITCODE -ne 0) { throw "Failed to check working tree status in '$root' (git status exited $LASTEXITCODE)" }
	if ($dirty.Count -gt 0) { throw 'Cannot start a new sweep with uncommitted tracked changes; commit, stash, or revert them first' }
	$headRaw = git -C $root rev-parse HEAD
	if ($LASTEXITCODE -ne 0) { throw "Failed to resolve HEAD in '$root' (git rev-parse HEAD exited $LASTEXITCODE)" }
	$headSha = $headRaw.Trim()
	New-Item -ItemType Directory -Force -Path (Join-Path $sweep 'units') | Out-Null
	@($headSha, $specRel) | Set-Content -LiteralPath $baselineFile
}
# The batch-start snapshot: a commit of the tracked working tree before any unit runs, or HEAD when that tree is
# clean. git stash create stores nothing in refs/stash.
$startFile = Join-Path $sweep "$Batch.start.txt"
if (-not (Test-Path -LiteralPath $startFile)) { $start = git -C $root stash create; if (-not $start) { $start = git -C $root rev-parse HEAD }; $start.Trim() | Set-Content $startFile }
"RUNNING $Batch $(Get-Date -Format s) units=$($batchUnits.Count)" | Set-Content $status

$findReceiptPattern = '^FIND-DONE findings=(\d+)$'
$fixReceiptPattern = '^FIX-DONE applied=\d+ declined=\d+ crossfile=\d+ oob=\d+$'
$propagateReceiptPattern = '^PROPAGATE-DONE applied=\d+ declined=\d+$'
$invokeSweepCodex = ${function:Invoke-SweepCodex}.ToString()
$getSweepReceiptLine = ${function:Get-SweepReceiptLine}.ToString()
try
{
	$unitOutcome = $batchUnits | ForEach-Object -ThrottleLimit $Throttle -Parallel {
		$u = $_
		$root = $using:root
		$sweep = $using:sweep
		$promptDir = $using:promptDir
		$model = $using:Model
		$findReceiptPattern = $using:findReceiptPattern
		$fixReceiptPattern = $using:fixReceiptPattern
		${function:Invoke-SweepCodex} = $using:invokeSweepCodex
		${function:Get-SweepReceiptLine} = $using:getSweepReceiptLine
		$findOut = Join-Path $sweep "units\$($u.Id).findings.md"
		$fixOut = Join-Path $sweep "units\$($u.Id).fix.md"
		$fileList = ($u.Files | ForEach-Object { "- $_" }) -join "`n"
		$pathArgs = ($u.Files | ForEach-Object { "'$_'" }) -join ','

		$vars = @{ ROOT = $root; FILES = $fileList; PATHARGS = $pathArgs; UNIT = $u.Stem; FINDINGS = "Temp/Sweep/units/$($u.Id).findings.md"; TYPE = $using:typeRel; SPEC = $using:specRel }
		$findReceipt = Get-SweepReceiptLine $findOut $findReceiptPattern
		if (-not $findReceipt)
		{
			$findOk = Invoke-SweepCodex (Join-Path $promptDir 'Find.md') $findOut 'read-only' $vars $root $model
			if ($findOk) { $findReceipt = Get-SweepReceiptLine $findOut $findReceiptPattern }
			if (-not $findReceipt) { Add-Content (Join-Path $sweep 'Failures.txt') "$($u.Stem) FIND failed"; $false; return }
		}
		$fixReceipt = Get-SweepReceiptLine $fixOut $fixReceiptPattern
		if ($fixReceipt) { $true; return }
		if ($findReceipt -match '=0+$')
		{
			$fixReceipt = 'FIX-DONE applied=0 declined=0 crossfile=0 oob=0'
			"# Fix: $($u.Stem)`n`n## Applied`nnone`n`n## Declined`nnone`n`n## Cross-file`nnone`n`n## Out-of-bound`nnone`n`n$fixReceipt" | Set-Content $fixOut
		}
		else
		{
			$fixOk = Invoke-SweepCodex (Join-Path $promptDir 'Fix.md') $fixOut 'workspace-write' $vars $root $model
			if ($fixOk) { $fixReceipt = Get-SweepReceiptLine $fixOut $fixReceiptPattern }
			if (-not $fixReceipt) { Add-Content (Join-Path $sweep 'Failures.txt') "$($u.Stem) FIX failed"; $false; return }
		}
		Add-Content (Join-Path $sweep 'Progress.md') "unit $($u.Stem) $fixReceipt"
		$true
	}
}
catch
{
	if (-not $Unit) { throw }
	$unitOutcome = $false
}

if ($Unit)
{
	if ($unitOutcome -is [bool] -and $unitOutcome) { "UNIT-DONE $Batch $Unit $(Get-Date -Format s)" | Set-Content $status; return }
	"UNIT-FAILED $Batch $Unit $(Get-Date -Format s)" | Set-Content $status
	exit 1
}

$outcomes = @($unitOutcome)
$workersOk = $outcomes.Count -eq $batchUnits.Count
foreach ($outcome in $outcomes) { if (-not ($outcome -is [bool] -and $outcome)) { $workersOk = $false; break } }

# Collect the batch's cross-file, out-of-bound and declined sections.
$crossFile = Join-Path $sweep "$Batch.crossfile.md"
$cross = @(); $ledger = @(); $missing = @()
foreach ($u in $batchUnits)
{
	$findOut = Join-Path $sweep "units\$($u.Id).findings.md"
	$fixOut = Join-Path $sweep "units\$($u.Id).fix.md"
	if (-not (Get-SweepReceiptLine $findOut $findReceiptPattern) -or -not (Get-SweepReceiptLine $fixOut $fixReceiptPattern)) { $missing += $u.Stem; continue }
	$c = Get-Section $fixOut 'Cross-file'
	if ($c) { $cross += "### $($u.Stem)`n$c`n" }
	$o = Get-Section $fixOut 'Out-of-bound'
	if ($o) { $ledger += "### $($u.Stem) (out-of-bound)`n$o`n" }
	$d = Get-Section $fixOut 'Declined'
	if ($d) { $ledger += "### $($u.Stem) (declined)`n$d`n" }
}
if ($missing.Count -gt 0 -or -not $workersOk)
{
	$missingText = if ($missing.Count -gt 0) { $missing -join ',' } else { 'worker-failed' }
	"INCOMPLETE $Batch $(Get-Date -Format s) missing=$missingText" | Set-Content $status
	return
}
# The cross-file file is written once per complete batch, so its absence means the ledger is not yet appended.
if (-not (Test-Path $crossFile)) { foreach ($l in $ledger) { Add-Content (Join-Path $sweep 'Ledger.md') $l } }

$propOut = Join-Path $sweep "$Batch.propagate.md"
if ($cross.Count -gt 0 -and -not (Get-SweepReceiptLine $propOut $propagateReceiptPattern))
{
	"# Cross-file findings: $Batch`n`n$($cross -join "`n")" | Set-Content $crossFile
	Remove-Item Env:OPENAI_API_KEY -ErrorAction SilentlyContinue
	$prompt = (Get-Content -Raw (Join-Path $promptDir 'Propagate.md')).Replace('{{ROOT}}', $root).Replace('{{BATCH}}', $Batch).Replace('{{CROSSFILE}}', "Temp/Sweep/$Batch.crossfile.md").Replace('{{TYPE}}', $typeRel).Replace('{{SPEC}}', $specRel)
	$prompt | & (Get-Command codex).Source -a never exec --sandbox workspace-write -C $root -m $Model -c 'model_reasoning_effort="high"' --disable hooks --ephemeral -o $propOut - 2> "$propOut.stderr.txt" | Out-Null
	if ($LASTEXITCODE -ne 0 -or -not (Get-SweepReceiptLine $propOut $propagateReceiptPattern)) { "PROPAGATE-FAILED $Batch $(Get-Date -Format s)" | Set-Content $status; return }
	$declined = Get-Section $propOut 'Declined'
	if ($declined) { Add-Content (Join-Path $sweep 'Ledger.md') "### $Batch propagation (declined)`n$declined`n" }
}
elseif ($cross.Count -eq 0 -and -not (Test-Path $crossFile)) { 'none' | Set-Content $crossFile }

$totals = @{ applied = 0; declined = 0; crossfile = 0; oob = 0 }
foreach ($u in $batchUnits)
{
	$line = Get-SweepReceiptLine (Join-Path $sweep "units\$($u.Id).fix.md") $fixReceiptPattern
	foreach ($k in @($totals.Keys)) { if ($line -match "$k=(\d+)") { $totals[$k] += [int] $Matches[1] } }
}
$summary = "units=$($batchUnits.Count) applied=$($totals.applied) declined=$($totals.declined) crossfile=$($totals.crossfile) oob=$($totals.oob)"
Add-Content (Join-Path $sweep 'Progress.md') "batch $Batch phase1+propagate $summary"
"BATCH-DONE $Batch $(Get-Date -Format s) $summary $(Get-BuildFields (Get-Content -LiteralPath $startFile -TotalCount 1).Trim())" | Set-Content $status
