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

	Two-call contract: a launch call (-Worktree/-PromptFile/-OutFile) starts Codex detached and
	returns a single-line JSON receipt naming the run identifier immediately; -Poll <run identifier>
	reports that run as running, completed, or failed, also immediately. No call ever waits for
	Codex, so a slow review can never be killed by the caller's command timeout.

	Exit codes: 0 for a successful launch and for every successful poll whatever its status;
	126 if an inherited OPENAI_API_KEY is refused, 127 if the codex CLI is not found, any other
	non-zero only for a script error (the driver treats non-zero, and a failed poll status, as
	CODEX-UNAVAILABLE and the skill falls back to the Opus reviewer subagent, then general-purpose
	on Opus).
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

	[Parameter(Mandatory, ParameterSetName = 'Poll')]
	[string] $Poll,                                       # run identifier from a launch receipt

	[Parameter(Mandatory, ParameterSetName = 'InternalRun')]
	[string] $InternalRunId                               # detached-run mode: launched by this script, never by a caller
)

$ErrorActionPreference = 'Stop'

# Run records outlive the launching shell, so they live outside the repository.
$recordDirectory = Join-Path ([System.IO.Path]::GetTempPath()) 'broken-engine-codex-review'
$temporaryDirectory = [System.IO.Path]::GetTempPath()

function Write-Receipt([hashtable] $Fields)
{
	Write-Output ([ordered]@{
		schemaVersion = 'broken-engine-codex-review/v1'
		status = $Fields.status
		runId = $Fields.runId
		outFile = $Fields.outFile
		exitCode = $Fields.exitCode
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

if ($PSCmdlet.ParameterSetName -ceq 'Poll')
{
	$runPath = Get-RunRecordPath $Poll
	$donePath = Get-DoneRecordPath $Poll
	if (Test-Path -LiteralPath $donePath -PathType Leaf)
	{
		$done = Get-Content -LiteralPath $donePath -Raw | ConvertFrom-Json
		# Records survive the poll so a repeated poll of a terminal run keeps answering the same way.
		if ($done.exitCode -eq 0)
		{
			Write-Receipt @{ status = 'completed'; runId = $Poll; outFile = $done.outFile; exitCode = 0; reason = $null }
		}
		else
		{
			Write-Receipt @{ status = 'failed'; runId = $Poll; outFile = $done.outFile; exitCode = $done.exitCode; reason = $done.reason }
		}
		exit 0
	}

	if (-not (Test-Path -LiteralPath $runPath -PathType Leaf))
	{
		Write-Receipt @{ status = 'failed'; runId = $Poll; outFile = $null; exitCode = $null; reason = 'unavailable: no run record for that identifier (never launched, or its records were swept)' }
		exit 0
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

	if ($alive)
	{
		Write-Receipt @{ status = 'running'; runId = $Poll; outFile = $run.outFile; exitCode = $null; reason = $null }
	}
	else
	{
		Write-Receipt @{ status = 'failed'; runId = $Poll; outFile = $run.outFile; exitCode = $null; reason = 'detached run ended without publishing a result; exit code unavailable' }
	}
	exit 0
}

# Only the modes that start Codex refuse the key; a poll stays a status query in any environment.
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
	# Records are kept for repeat polls, so a launch is what bounds the directory.
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
	$arguments = "-NoProfile -NonInteractive -File `"$PSCommandPath`" -InternalRunId `"$runId`" -Worktree `"$absoluteWorktree`" -PromptFile `"$detachedPrompt`" -OutFile `"$absoluteOutput`""
	$detached = Start-Process -FilePath $powershell -ArgumentList $arguments -WindowStyle Hidden -PassThru

	[ordered]@{
		runId = $runId
		processId = $detached.Id
		processStartTicks = $detached.StartTime.ToUniversalTime().Ticks
		outFile = $absoluteOutput
	} | ConvertTo-Json -Depth 4 -Compress | Set-Content -LiteralPath (Get-RunRecordPath $runId) -Encoding utf8NoBOM
	$detached.Dispose()

	Write-Receipt @{ status = 'launched'; runId = $runId; outFile = $absoluteOutput; exitCode = $null; reason = $null }
	exit 0
}

# Detached-run mode: the caller is already gone, so the result reaches it only through the records below.
$stagingOutput = Join-Path $temporaryDirectory "broken-engine-codex-review-$InternalRunId.output.md"
$pendingOutput = Join-Path ([System.IO.Path]::GetDirectoryName($OutFile)) "$InternalRunId.partial.md"
$exitCode = 1
$reason = $null

try
{
	Get-Content -LiteralPath $PromptFile -Raw | & $codex.Source `
		-a never `
		exec `
		--sandbox read-only `
		-C $Worktree `
		-m gpt-5.6-sol `
		-c 'model_reasoning_effort="medium"' `
		--ephemeral `
		-o $stagingOutput `
		-
	$exitCode = $LASTEXITCODE
	if ($exitCode -eq 0)
	{
		# A same-directory rename is atomic, so a poller never reads a half-written result file.
		Copy-Item -LiteralPath $stagingOutput -Destination $pendingOutput -Force
		Move-Item -LiteralPath $pendingOutput -Destination $OutFile -Force
	}
	else
	{
		$reason = "codex exited $exitCode"
	}
}
catch
{
	$exitCode = 1
	$reason = $_.Exception.Message
}
finally
{
	Remove-Item -LiteralPath $PromptFile, $stagingOutput, $pendingOutput -Force -ErrorAction SilentlyContinue
	# A poll reads the done record the moment it exists, so it appears whole through a same-directory rename.
	$pendingDoneRecord = Join-Path $recordDirectory "$InternalRunId.done.partial.json"
	[ordered]@{
		exitCode = $exitCode
		reason = $reason
		outFile = $OutFile
	} | ConvertTo-Json -Depth 4 -Compress | Set-Content -LiteralPath $pendingDoneRecord -Encoding utf8NoBOM
	Move-Item -LiteralPath $pendingDoneRecord -Destination (Get-DoneRecordPath $InternalRunId) -Force
}

exit $exitCode
