<#
.SYNOPSIS
	Run a Broken Engine review skill on Codex (Sol) headless and capture its findings.

	Primary Claude Code reviewer route: it runs a delegated reviewer/auditor role on Codex/Sol.
	Driven by the /codex-review skill, which on failure falls back to the Opus reviewer subagent,
	then general-purpose on Opus. Codex never invokes this — its reviewer role is already Sol.

.NOTES
	Auth/billing: Codex uses ChatGPT sign-in by default -> ChatGPT subscription quota, NOT metered
	OpenAI API credits (verify with `codex login status`). The wrapper refuses an inherited
	OPENAI_API_KEY so the child cannot bill the API instead.
	Reviews run on the standard service tier. $serviceTier below switches them to Codex's fast tier
	(~1.5x speed for ~2.5x subscription credit burn); if the model does not advertise that tier
	Codex warns and runs standard.

	One-call contract: a launch call (-Worktree/-PromptFile/-OutFile) starts Codex detached, then
	waits up to 540 seconds for it and returns a single-line JSON receipt (schema
	broken-engine-codex-review/v3) naming the run identifier and its status. A completed receipt is
	followed by the line `--- findings ---` and then the out-file's bytes verbatim, so the findings
	need no separate read. If the budget expires with the run still alive the receipt reports
	running, and one further -Wait <run identifier> call resumes the same bounded wait. Every call
	stays well below the caller's command timeout, and because the run is detached a killed call
	never kills the review.

	Shape check and retry: a Codex exit of 0 whose result does not end in the verdict line every
	prompt mandates (PASS, CHANGES-REQUIRED: <n>, or BLOCKED: <reason>) is malformed. The detached
	run then feeds the same unchanged prompt to Codex a second time, publishing the first attempt's
	bytes to the <out> sibling ending in -attempt1. Only the final attempt's bytes reach <out>. A
	receipt reports retried, and status malformed when the final attempt still lacks that line;
	a malformed receipt carries no separator and no findings, so drafting notes never reach the
	caller. -NoRetry makes the run perform exactly one attempt, which is how a caller spends its own
	single re-dispatch.

	Exit codes: 0 for every wait whatever its status;
	126 if an inherited OPENAI_API_KEY is refused, 127 if the codex CLI is not found, any other
	non-zero only for a script error. The caller reports non-zero, and a failed or malformed wait
	status, as CODEX-UNAVAILABLE and stops there; nothing is substituted automatically, and only
	explicit user authorization routes the assignment to the Opus reviewer. None of these retry.
#>
[CmdletBinding(DefaultParameterSetName = 'Launch')]
param(
	[Parameter(Mandatory, ParameterSetName = 'Launch')]
	[Parameter(Mandatory, ParameterSetName = 'InternalRun')]
	[string] $Worktree,                                   # session worktree checkout to review in (codex -C)

	[Parameter(Mandatory, ParameterSetName = 'Launch')]
	[Parameter(Mandatory, ParameterSetName = 'InternalRun')]
	[string] $PromptFile,                                 # file holding the assembled review prompt (fed on stdin)

	[Parameter(Mandatory, ParameterSetName = 'Launch')]
	[Parameter(Mandatory, ParameterSetName = 'InternalRun')]
	[string] $OutFile,                                    # Codex writes its final message here, complete or not at all

	[Parameter(ParameterSetName = 'Launch')]
	[Parameter(ParameterSetName = 'InternalRun')]
	[switch] $NoRetry,                                    # spend no in-process retry: exactly one Codex attempt

	[Parameter(Mandatory, ParameterSetName = 'Wait')]
	[string] $Wait,                                       # run identifier from a running receipt

	[Parameter(Mandatory, ParameterSetName = 'InternalRun')]
	[string] $InternalRunId                               # detached-run mode: launched by this script, never by a caller
)

$ErrorActionPreference = 'Stop'

# Run records outlive the launching shell, so they live outside the repository.
$recordDirectory = Join-Path ([System.IO.Path]::GetTempPath()) 'broken-engine-codex-review'
$temporaryDirectory = [System.IO.Path]::GetTempPath()

# One wait stays 60 seconds under the caller's 600000 ms command cap; a longer review resumes with -Wait.
$waitBudgetSeconds = 540
$waitSleepSeconds = 5

# 'default' or 'fast'. Fast is disabled for now; set 'fast' to re-enable it (see .NOTES for the cost).
$serviceTier = 'default'

function Write-Receipt([hashtable] $Fields)
{
	Write-Output ([ordered]@{
		schemaVersion = 'broken-engine-codex-review/v3'
		status = $Fields.status
		runId = $Fields.runId
		outFile = $Fields.outFile
		exitCode = $Fields.exitCode
		# An unset field means no second attempt is known of, which is what every non-done receipt reports.
		retried = [bool] $Fields.retried
		reason = $Fields.reason
	} | ConvertTo-Json -Depth 4 -Compress)
}

function Get-RunRecordPath([string] $RunId)
{
	return (Join-Path $recordDirectory "$RunId.run.json")
}

function Get-DoneRecordPath([string] $RunId)
{
	return (Join-Path $recordDirectory "$RunId.done.json")
}

# The one output contract shared by every routed review: the mandated final verdict line. Nothing
# else about the result is judged here, so caller judgment stays the backstop for what this accepts.
function Test-CodexVerdictLine([string] $Text)
{
	$lines = @($Text -split "`r?`n" | ForEach-Object { $_.Trim() })
	$lastIndex = $lines.Count - 1
	while (($lastIndex -ge 0) -and ($lines[$lastIndex] -eq ''))
	{
		$lastIndex--
	}

	if ($lastIndex -lt 0)
	{
		return $false
	}

	# Models sometimes bold the verdict; the match stays case-sensitive because the tokens are
	# uppercase contract vocabulary while leaked drafting notes are ordinary prose.
	$candidate = $lines[$lastIndex] -replace '^\*\*', '' -replace '\*\*$', ''
	return ($candidate -cmatch '^(?:PASS|CHANGES-REQUIRED:[ \t]*\d+|BLOCKED:[ \t]*\S.*)$')
}

# Malformed evidence lands beside the result the caller asked for: Temp/foo-out.md -> Temp/foo-out-attempt1.md.
function Get-FirstAttemptOutFilePath([string] $OutFile)
{
	$stem = [System.IO.Path]::GetFileNameWithoutExtension($OutFile)
	$extension = [System.IO.Path]::GetExtension($OutFile)
	return (Join-Path ([System.IO.Path]::GetDirectoryName($OutFile)) "$stem-attempt1$extension")
}

# A same-directory rename is atomic, so a poller never reads a half-written result file.
function Publish-OutputFile([string] $StagingPath, [string] $DestinationPath, [string] $PendingPath)
{
	Copy-Item -LiteralPath $StagingPath -Destination $PendingPath -Force
	Move-Item -LiteralPath $PendingPath -Destination $DestinationPath -Force
}

# Both attempts are this same call on the same unchanged prompt file; only the staging path differs.
# Codex's own console chatter is discarded, because anything this function writes to the pipeline
# would be captured by the caller's assignment alongside the exit code it actually returns.
function Invoke-CodexAttempt([string] $StagingPath)
{
	Get-Content -LiteralPath $PromptFile -Raw | & $codex.Source `
		-a never `
		exec `
		--sandbox read-only `
		-C $Worktree `
		-m gpt-5.6-sol `
		-c 'model_reasoning_effort="medium"' `
		-c "service_tier=`"$serviceTier`"" `
		--ephemeral `
		-o $StagingPath `
		- | Out-Null
	return [int] $LASTEXITCODE
}

# Shared by both entry paths: the launch waits on the run it just started, -Wait resumes that same wait.
function Wait-Run([string] $RunId)
{
	$runPath = Get-RunRecordPath $RunId
	$donePath = Get-DoneRecordPath $RunId
	$deadline = (Get-Date).AddSeconds($waitBudgetSeconds)

	while ($true)
	{
		# Completion keys on the done record, never on the out-file, which appears just before it.
		if (Test-Path -LiteralPath $donePath -PathType Leaf)
		{
			$done = Get-Content -LiteralPath $donePath -Raw | ConvertFrom-Json
			$retried = [bool] $done.retried
			# Records survive the wait so a repeated wait on a terminal run keeps answering the same way.
			if ($done.exitCode -eq 0)
			{
				if ($done.malformed)
				{
					# Drafting notes never enter the caller's context, so the paths are the whole answer.
					Write-Receipt @{ status = 'malformed'; runId = $RunId; outFile = $done.outFile; exitCode = 0; retried = $retried; reason = $done.reason }
				}
				else
				{
					Write-Receipt @{ status = 'completed'; runId = $RunId; outFile = $done.outFile; exitCode = 0; retried = $retried; reason = $null }
					# The findings ride this call's own stdout, so a successful review needs no separate read.
					Write-Output '--- findings ---'
					# Console.Out keeps the out-file's bytes verbatim; the pipeline would append a trailing newline.
					[Console]::Out.Write([string](Get-Content -LiteralPath $done.outFile -Raw -ErrorAction SilentlyContinue))
				}
			}
			else
			{
				Write-Receipt @{ status = 'failed'; runId = $RunId; outFile = $done.outFile; exitCode = $done.exitCode; retried = $retried; reason = $done.reason }
			}
			return
		}

		if (-not (Test-Path -LiteralPath $runPath -PathType Leaf))
		{
			Write-Receipt @{ status = 'failed'; runId = $RunId; outFile = $null; exitCode = $null; reason = 'unavailable: no run record for that identifier (never launched, or its records were swept)' }
			return
		}

		$run = Get-Content -LiteralPath $runPath -Raw | ConvertFrom-Json
		$alive = $false
		try
		{
			$process = Get-Process -Id $run.processId -ErrorAction SilentlyContinue
			# Start time distinguishes the launched run from an unrelated process that reused its id.
			$alive = ($null -ne $process) -and ($process.StartTime.ToUniversalTime().Ticks -eq $run.processStartTicks)
		}
		catch
		{
			$alive = $false
		}

		# A dead run without a done record can never finish, so answering now beats burning the budget.
		if (-not $alive)
		{
			Write-Receipt @{ status = 'failed'; runId = $RunId; outFile = $run.outFile; exitCode = $null; reason = 'detached run ended without publishing a result; exit code unavailable' }
			return
		}

		if ((Get-Date) -ge $deadline)
		{
			# The run outlives this call, so its identifier is what the caller resumes with.
			Write-Receipt @{ status = 'running'; runId = $RunId; outFile = $run.outFile; exitCode = $null; reason = $null }
			return
		}

		# The last sleep is clamped to what is left, so the call never returns past the budget.
		$remainingSeconds = ($deadline - (Get-Date)).TotalSeconds
		Start-Sleep -Milliseconds ([int]([Math]::Min($waitSleepSeconds, $remainingSeconds) * 1000))
	}
}

if ($PSCmdlet.ParameterSetName -ceq 'Wait')
{
	Wait-Run $Wait
	exit 0
}

# Only the modes that start Codex refuse the key; a wait stays a status query in any environment.
if ($null -ne [Environment]::GetEnvironmentVariable('OPENAI_API_KEY', 'Process'))
{
	Write-Error 'refusing to launch Codex with inherited OPENAI_API_KEY' -ErrorAction Continue
	exit 126
}

$codex = Get-Command codex -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $codex)
{
	Write-Error 'codex CLI not found on PATH' -ErrorAction Continue
	exit 127
}

if ($PSCmdlet.ParameterSetName -ceq 'Launch')
{
	# Every host shell call starts in a fresh working directory, so the detached run only ever sees absolute paths.
	$absoluteWorktree = [System.IO.Path]::GetFullPath($Worktree)
	$absolutePrompt = [System.IO.Path]::GetFullPath($PromptFile)
	$absoluteOutput = [System.IO.Path]::GetFullPath($OutFile)
	$runId = [guid]::NewGuid().ToString('N')
	$detachedPrompt = Join-Path $temporaryDirectory "broken-engine-codex-review-$runId.prompt.md"

	New-Item -ItemType Directory -Path $recordDirectory -Force | Out-Null
	# Records are kept for repeat waits, so a launch is what bounds the directory.
	Get-ChildItem -LiteralPath $recordDirectory -Filter '*.json' -File -ErrorAction SilentlyContinue |
		Where-Object { $_.LastWriteTimeUtc -lt (Get-Date).ToUniversalTime().AddDays(-7) } |
		Remove-Item -Force -ErrorAction SilentlyContinue

	# The publishing rename needs the destination directory, and failing here beats failing after Codex ran.
	$outputParent = [System.IO.Path]::GetDirectoryName($absoluteOutput)
	New-Item -ItemType Directory -Path $outputParent -Force | Out-Null

	# The prompt is copied because the detached run outlives the caller that wrote it.
	Copy-Item -LiteralPath $absolutePrompt -Destination $detachedPrompt

	$powershell = (Get-Process -Id $PID).Path
	# One pre-quoted command line: an argument array would be re-escaped and would split these paths.
	$noRetryArgument = if ($NoRetry) { ' -NoRetry' } else { '' }
	$arguments = "-NoProfile -NonInteractive -File `"$PSCommandPath`" -InternalRunId `"$runId`" -Worktree `"$absoluteWorktree`" -PromptFile `"$detachedPrompt`" -OutFile `"$absoluteOutput`"$noRetryArgument"
	$detached = Start-Process -FilePath $powershell -ArgumentList $arguments -WindowStyle Hidden -PassThru

	[ordered]@{
		runId = $runId
		processId = $detached.Id
		processStartTicks = $detached.StartTime.ToUniversalTime().Ticks
		outFile = $absoluteOutput
	} | ConvertTo-Json -Depth 4 -Compress | Set-Content -LiteralPath (Get-RunRecordPath $runId) -Encoding utf8NoBOM
	$detached.Dispose()

	Wait-Run $runId
	exit 0
}

# Detached-run mode: the caller is already gone, so the result reaches it only through the records below.
$stagingOutput = Join-Path $temporaryDirectory "broken-engine-codex-review-$InternalRunId.output.md"
$retryStagingOutput = Join-Path $temporaryDirectory "broken-engine-codex-review-$InternalRunId.output-retry.md"
$pendingOutput = Join-Path ([System.IO.Path]::GetDirectoryName($OutFile)) "$InternalRunId.partial.md"
$exitCode = 1
$reason = $null
$retried = $false
$malformed = $false
$firstAttemptOutFile = $null

try
{
	$exitCode = Invoke-CodexAttempt $stagingOutput
	$finalStaging = $stagingOutput
	if ($exitCode -eq 0)
	{
		$malformed = -not (Test-CodexVerdictLine ([string](Get-Content -LiteralPath $stagingOutput -Raw -ErrorAction SilentlyContinue)))
		if ($malformed -and (-not $NoRetry))
		{
			# The first attempt's bytes are the only evidence of what went wrong, so they are kept
			# beside the result before the second attempt takes over the caller's out-file. An
			# attempt whose file cannot be read or copied is malformed with no evidence to keep,
			# and losing that evidence must never cost the retry the malformed result has earned.
			if (Test-Path -LiteralPath $stagingOutput -PathType Leaf)
			{
				try
				{
					$firstAttemptOutFile = Get-FirstAttemptOutFilePath $OutFile
					Publish-OutputFile $stagingOutput $firstAttemptOutFile $pendingOutput
				}
				catch
				{
					$firstAttemptOutFile = $null
				}
			}

			$retried = $true
			# $PromptFile is untouched between the attempts, which is what makes the retry identical.
			$exitCode = Invoke-CodexAttempt $retryStagingOutput
			$finalStaging = $retryStagingOutput
			$malformed = ($exitCode -eq 0) -and (-not (Test-CodexVerdictLine ([string](Get-Content -LiteralPath $retryStagingOutput -Raw -ErrorAction SilentlyContinue))))
		}
	}

	if ($exitCode -eq 0)
	{
		# A final attempt that wrote no file is already malformed, and has no bytes to publish.
		if (Test-Path -LiteralPath $finalStaging -PathType Leaf)
		{
			try
			{
				Publish-OutputFile $finalStaging $OutFile $pendingOutput
			}
			catch
			{
				# Bytes that cannot reach the out-file leave it carrying no verdict line either way,
				# so an unpublishable result is the same malformed answer as one never written.
				$malformed = $true
			}
		}

		if ($malformed)
		{
			# The evidence path is named only when a first-attempt file was actually retained.
			$reason = if ($null -ne $firstAttemptOutFile) { "malformed: no final verdict line after one identical retry; the first attempt's result is at $firstAttemptOutFile" } else { 'malformed: the result carries no final verdict line' }
		}
	}
	else
	{
		$reason = "codex exited $exitCode"
	}
}
catch
{
	$exitCode = 1
	$malformed = $false
	$reason = $_.Exception.Message
}
finally
{
	Remove-Item -LiteralPath $PromptFile, $stagingOutput, $retryStagingOutput, $pendingOutput -Force -ErrorAction SilentlyContinue
	# A poll reads the done record the moment it exists, so it appears whole through a same-directory rename.
	$pendingDoneRecord = Join-Path $recordDirectory "$InternalRunId.done.partial.json"
	[ordered]@{
		exitCode = $exitCode
		reason = $reason
		outFile = $OutFile
		retried = $retried
		malformed = $malformed
		firstAttemptOutFile = $firstAttemptOutFile
	} | ConvertTo-Json -Depth 4 -Compress | Set-Content -LiteralPath $pendingDoneRecord -Encoding utf8NoBOM
	Move-Item -LiteralPath $pendingDoneRecord -Destination (Get-DoneRecordPath $InternalRunId) -Force
}

exit $exitCode
