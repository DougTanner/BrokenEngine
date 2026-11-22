<#
.SYNOPSIS
	Run one routed review task on Codex headless in a worktree from a non-Codex host and capture its result.

	-Skill and optional -Mode select one of the $routes entries, which fixes the $presets model/effort
	pair and the sandbox mode (read-only or workspace-write). Unrouted skills or modes fail before
	spawning Codex. -CheckModels checks the presets' models instead of launching; -ListRoutes prints
	each routed <skill>[:<mode>] and sandbox without launching.

.NOTES
	Auth/billing: Codex uses ChatGPT sign-in by default -> ChatGPT subscription quota, NOT metered
	OpenAI API credits (verify with `codex login status`). The wrapper refuses an inherited
	OPENAI_API_KEY so the child cannot bill the API instead.
	Runs use the standard service tier. $serviceTier below switches them to Codex's fast tier
	(~1.5x speed for ~2.5x subscription credit burn); if the model does not advertise that tier
	Codex warns and runs standard.

	One-call contract: a launch call (-Skill [-Mode]/-Worktree/-PromptFile/-OutFile) starts Codex detached,
	then waits up to 540 seconds for it and returns a single-line JSON receipt naming the run identifier
	and its status. A completed receipt is followed by the line `--- findings ---` and then the
	out-file's bytes verbatim, so the result needs no separate read. If the budget expires with the
	run still alive the receipt reports running, and one further -Wait <run identifier> call resumes
	the same bounded wait after the launching call exits. Both calls must be in a host context where
	the worker survives the launching call and the run records are visible.

	Receipt fields: schemaVersion (broken-engine-codex-run/v2); status (completed, malformed,
	failed, or running); runId; skill (routed skill name, null when unavailable or incompatible);
	mode (optional sub-mode, null when omitted, unavailable, or incompatible); noRetry (true when
	launched with -NoRetry); outFile (absolute out-file path, null when unavailable or
	incompatible); exitCode (Codex's exit code, null while running, unavailable, or incompatible);
	retried (true when a second attempt ran); reason (null, or the malformed or failed cause).

	Shape check and retry: a Codex exit of 0 whose result does not end in the verdict line every
	prompt mandates (PASS, CHANGES-REQUIRED: <n>, or BLOCKED: <reason>) is malformed. A read-only
	detached run then feeds the same unchanged prompt to Codex a second time, publishing the first
	attempt's bytes to the <out> sibling ending in -attempt1. Only the final attempt's bytes reach
	<out>. A receipt reports retried, and status malformed when the final attempt still lacks that
	line; a malformed receipt carries no separator and no result, so drafting notes never reach the
	caller. -NoRetry makes the run perform exactly one attempt. A workspace-write run always
	performs exactly one attempt, because a second attempt would start from the first attempt's
	edits.

	Exit codes (launch and -Wait calls; -ListRoutes and -CheckModels below): 0 for every wait whatever
	its status; 126 if an inherited OPENAI_API_KEY is refused, 127 if the codex CLI is not found, 1 for
	an unrouted -Skill/-Mode or any other script error. None of these retry. When the codex CLI itself
	exits non-zero, the failed receipt's reason carries its last non-empty stderr line capped at 200
	characters, with the full stderr retained as <runId>.stderr.txt beside the run records; when the
	CLI printed nothing it stays `codex exited <n>`. How the caller reports a non-zero exit or a failed
	or malformed status is `.agents/references/codex-bridge.md`, `## Rules`.

	-ListRoutes prints one `<skill>[:<mode>] <sandbox>` line per $routes entry sorted ordinally and
	exits 0 in any environment.

	-CheckModels checks each $presets model against the live Codex model catalog
	(`codex debug models`) and prints one compact JSON receipt: schemaVersion
	(broken-engine-codex-models/v1); status (pass, blocked, or error); checks, one row per slug with
	slug, newest, and reason (null when it passes); message (null, or the error cause). A slug is
	blocked when the catalog lacks it, or when a list-visibility slug of the same
	gpt-<version>-<family> carries a numerically higher version; hidden entries never block. Exit 0
	pass, 2 blocked, 1 error.
#>
[CmdletBinding(DefaultParameterSetName = 'Launch')]
param(
	[Parameter(Mandatory, ParameterSetName = 'Launch')]
	[Parameter(Mandatory, ParameterSetName = 'InternalRun')]
	[string] $Skill,                                      # routed review skill name from $routes

	[Parameter(ParameterSetName = 'Launch')]
	[Parameter(ParameterSetName = 'InternalRun')]
	[string] $Mode = '',                                  # optional sub-mode for multi-mode skills in $routes

	[Parameter(Mandatory, ParameterSetName = 'Launch')]
	[Parameter(Mandatory, ParameterSetName = 'InternalRun')]
	[string] $Worktree,                                   # session worktree checkout Codex runs in (codex -C)

	[Parameter(Mandatory, ParameterSetName = 'Launch')]
	[Parameter(Mandatory, ParameterSetName = 'InternalRun')]
	[string] $PromptFile,                                 # file holding the caller-written prompt (fed on stdin)

	[Parameter(Mandatory, ParameterSetName = 'Launch')]
	[Parameter(Mandatory, ParameterSetName = 'InternalRun')]
	[string] $OutFile,                                    # Codex writes its final message here, complete or not at all

	[Parameter(ParameterSetName = 'Launch')]
	[Parameter(ParameterSetName = 'InternalRun')]
	[switch] $NoRetry,                                    # spend no in-process retry: exactly one Codex attempt

	[Parameter(Mandatory, ParameterSetName = 'InternalRun')]
	[ValidateSet('sol', 'astra')]
	[string] $Agent,                                      # $presets entry resolved from $routes; internal only

	[Parameter(Mandatory, ParameterSetName = 'InternalRun')]
	[ValidateSet('read-only', 'workspace-write')]
	[string] $Sandbox,                                    # codex --sandbox resolved from $routes; internal only

	[Parameter(Mandatory, ParameterSetName = 'Wait')]
	[string] $Wait,                                       # run identifier from a running receipt

	[Parameter(Mandatory, ParameterSetName = 'CheckModels')]
	[switch] $CheckModels,                                # check the $presets models; never launches

	[Parameter(Mandatory, ParameterSetName = 'ListRoutes')]
	[switch] $ListRoutes,                                 # print routed skill[:mode] keys and sandboxes; never launches

	[Parameter(Mandatory, ParameterSetName = 'InternalRun')]
	[string] $InternalRunId                               # detached-run mode: launched by this script, never by a caller
)

$ErrorActionPreference = 'Stop'

$runSchemaVersion = 'broken-engine-codex-run/v2'

# One wait stays 60 seconds under the caller's 600000 ms command cap; a longer run resumes with -Wait.
$waitBudgetSeconds = 540
$waitSleepSeconds = 5

# 'default' or 'fast'. Fast is disabled for now; set 'fast' to re-enable it (see .NOTES for the cost).
$serviceTier = 'default'

# The only presets: $routes maps each routed skill/mode to one, and -CheckModels checks their models.
$presets = @{
	sol = @{ Model = 'gpt-6.2-sol'; Effort = 'xhigh' }
	astra = @{ Model = 'gpt-6.2-astra'; Effort = 'high' }
}

# Exact case-sensitive (Skill[:Mode]) -> (Agent, Sandbox) routing table for non-Codex review dispatch.
$routes = [Collections.Hashtable]::new([StringComparer]::Ordinal)
# Astra (gpt-6.2-astra, high)
$routes['repo-code-review'] = @{ Agent = 'astra'; Sandbox = 'read-only' }
$routes['glsl-review'] = @{ Agent = 'astra'; Sandbox = 'read-only' }
$routes['adversarial-review'] = @{ Agent = 'astra'; Sandbox = 'read-only' }
$routes['plan-audit'] = @{ Agent = 'astra'; Sandbox = 'read-only' }
$routes['plan-simplicity-review'] = @{ Agent = 'astra'; Sandbox = 'read-only' }
$routes['over-engineering-review:finder'] = @{ Agent = 'astra'; Sandbox = 'workspace-write' }
$routes['external-architecture-review'] = @{ Agent = 'astra'; Sandbox = 'read-only' }
# Sol (gpt-6.2-sol, xhigh)
$routes['code-style-review'] = @{ Agent = 'sol'; Sandbox = 'workspace-write' }
$routes['comment-review'] = @{ Agent = 'sol'; Sandbox = 'workspace-write' }
$routes['coherence-review:coherence'] = @{ Agent = 'sol'; Sandbox = 'read-only' }
$routes['coherence-review:combined'] = @{ Agent = 'sol'; Sandbox = 'workspace-write' }
$routes['progressive-disclosure-review'] = @{ Agent = 'sol'; Sandbox = 'read-only' }
$routes['external-skill-creator:validate'] = @{ Agent = 'sol'; Sandbox = 'read-only' }
$routes['verify-acceptance'] = @{ Agent = 'sol'; Sandbox = 'read-only' }
$routes['next-plan-review'] = @{ Agent = 'sol'; Sandbox = 'read-only' }
$routes['next-plan-checkpoint-review'] = @{ Agent = 'sol'; Sandbox = 'read-only' }

if ($PSCmdlet.ParameterSetName -ceq 'ListRoutes')
{
	[string[]] $keys = @($routes.Keys)
	[Array]::Sort($keys, [StringComparer]::Ordinal)
	foreach ($key in $keys)
	{
		Write-Output "$key $($routes[$key].Sandbox)"
	}
	exit 0
}

# Run records outlive the launching shell, so they live outside the repository.
$recordDirectory = Join-Path ([System.IO.Path]::GetTempPath()) 'broken-engine-codex-run'
$temporaryDirectory = [System.IO.Path]::GetTempPath()

function Write-Receipt([hashtable] $Fields)
{
	Write-Output ([ordered]@{
		schemaVersion = $runSchemaVersion
		status = $Fields.status
		runId = $Fields.runId
		skill = $Fields.skill
		mode = $Fields.mode
		noRetry = [bool] $Fields.noRetry
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

# The one output contract shared by every run: the mandated final verdict line. Nothing
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
# would be captured by the caller's assignment alongside the exit code it actually returns; its
# stderr instead goes to the run's retained stderr file so a failure reason can quote it.
# --disable hooks prevents .codex/config.toml SessionStart hooks from injecting main-session
# workflow context into headless runs if hook trust is persisted.
function Invoke-CodexAttempt([string] $StagingPath)
{
	Get-Content -LiteralPath $PromptFile -Raw | & $codex.Source `
		-a never `
		exec `
		--sandbox $Sandbox `
		-C $Worktree `
		-m $presets[$Agent].Model `
		-c "model_reasoning_effort=`"$($presets[$Agent].Effort)`"" `
		-c "service_tier=`"$serviceTier`"" `
		--disable hooks `
		--ephemeral `
		-o $StagingPath `
		- 2> $stderrOutput | Out-Null
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
			if ($done.schemaVersion -cne $runSchemaVersion)
			{
				Write-Receipt @{ status = 'failed'; runId = $RunId; skill = $null; mode = $null; noRetry = $false; outFile = $null; exitCode = $null; reason = "incompatible run record schema version; expected $runSchemaVersion" }
				return
			}
			$retried = [bool] $done.retried
			# Records survive the wait so a repeated wait on a terminal run keeps answering the same way.
			if ($done.exitCode -eq 0)
			{
				if ($done.malformed)
				{
					# Drafting notes never enter the caller's context, so the paths are the whole answer.
					Write-Receipt @{ status = 'malformed'; runId = $RunId; skill = $done.skill; mode = $done.mode; noRetry = [bool] $done.noRetry; outFile = $done.outFile; exitCode = 0; retried = $retried; reason = $done.reason }
				}
				else
				{
					Write-Receipt @{ status = 'completed'; runId = $RunId; skill = $done.skill; mode = $done.mode; noRetry = [bool] $done.noRetry; outFile = $done.outFile; exitCode = 0; retried = $retried; reason = $null }
					# The result rides this call's own stdout, so a successful run needs no separate read.
					Write-Output '--- findings ---'
					# Console.Out keeps the out-file's bytes verbatim; the pipeline would append a trailing newline.
					[Console]::Out.Write([string](Get-Content -LiteralPath $done.outFile -Raw -ErrorAction SilentlyContinue))
				}
			}
			else
			{
				Write-Receipt @{ status = 'failed'; runId = $RunId; skill = $done.skill; mode = $done.mode; noRetry = [bool] $done.noRetry; outFile = $done.outFile; exitCode = $done.exitCode; retried = $retried; reason = $done.reason }
			}
			return
		}

		if (-not (Test-Path -LiteralPath $runPath -PathType Leaf))
		{
			Write-Receipt @{ status = 'failed'; runId = $RunId; skill = $null; mode = $null; noRetry = $false; outFile = $null; exitCode = $null; reason = 'unavailable: no run record for that identifier (never launched, or its records were swept)' }
			return
		}

		$run = Get-Content -LiteralPath $runPath -Raw | ConvertFrom-Json
		if ($run.schemaVersion -cne $runSchemaVersion)
		{
			Write-Receipt @{ status = 'failed'; runId = $RunId; skill = $null; mode = $null; noRetry = $false; outFile = $null; exitCode = $null; reason = "incompatible run record schema version; expected $runSchemaVersion" }
			return
		}
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

		if (-not $alive)
		{
			# The worker may have published its result and exited since the first check.
			if (Test-Path -LiteralPath $donePath -PathType Leaf)
			{
				continue
			}

			Write-Receipt @{ status = 'failed'; runId = $RunId; skill = $run.skill; mode = $run.mode; noRetry = [bool] $run.noRetry; outFile = $run.outFile; exitCode = $null; reason = 'detached run ended without publishing a result; exit code unavailable' }
			return
		}

		if ((Get-Date) -ge $deadline)
		{
			# The run outlives this call, so its identifier is what the caller resumes with.
			Write-Receipt @{ status = 'running'; runId = $RunId; skill = $run.skill; mode = $run.mode; noRetry = [bool] $run.noRetry; outFile = $run.outFile; exitCode = $null; reason = $null }
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

if ($PSCmdlet.ParameterSetName -ceq 'CheckModels')
{
	Set-StrictMode -Version Latest
	Import-Module (Join-Path $PSScriptRoot 'AgentScriptCommon.psm1') -Force

	$result = [ordered]@{
		schemaVersion = 'broken-engine-codex-models/v1'
		status = 'error'
		checks = @()
		message = $null
	}

	$exitCode = 1
	try
	{
		$codex = Get-Command codex -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
		if (-not $codex)
		{
			throw 'codex CLI not found on PATH'
		}

		$run = Invoke-AgentProcess $codex.Source @('debug', 'models') $PSScriptRoot
		if ($run.ExitCode -ne 0)
		{
			throw "codex debug models exited $($run.ExitCode)"
		}

		$catalog = @(($run.Stdout | ConvertFrom-Json).models)
		$blocked = $false
		foreach ($slug in @($presets.Values | ForEach-Object { $_.Model } | Sort-Object -Unique))
		{
			$reason = $null
			if (-not @($catalog | Where-Object { $_.slug -ceq $slug }))
			{
				$reason = 'absent from the catalog'
			}

			$newest = $slug
			$pinned = Get-AgentModelFamily $slug
			if ($null -ne $pinned)
			{
				$newestVersion = $pinned.Version
				foreach ($entry in @($catalog | Where-Object { $_.visibility -ceq 'list' }))
				{
					$candidate = Get-AgentModelFamily $entry.slug
					if (($null -ne $candidate) -and ($candidate.Family -ceq $pinned.Family) -and ((Compare-AgentModelVersion $candidate.Version $newestVersion) -gt 0))
					{
						$newest = $entry.slug
						$newestVersion = $candidate.Version
					}
				}

				if (($null -eq $reason) -and ($newest -cne $slug))
				{
					$reason = 'a newer listed version exists'
				}
			}

			$blocked = $blocked -or ($null -ne $reason)
			$result.checks += [ordered]@{ slug = $slug; newest = $newest; reason = $reason }
		}

		$result.status = if ($blocked) { 'blocked' } else { 'pass' }
		$exitCode = if ($blocked) { 2 } else { 0 }
	}
	catch
	{
		$result.status = 'error'
		$result.message = $_.Exception.Message
		$exitCode = 1
	}

	[Console]::Out.Write(($result | ConvertTo-Json -Depth 8 -Compress))
	exit $exitCode
}

if ($PSCmdlet.ParameterSetName -ceq 'Launch')
{
	$routeKey = if ([string]::IsNullOrWhiteSpace($Mode)) { $Skill } else { "${Skill}:${Mode}" }
	if ($Skill.Contains(':') -or ($PSBoundParameters.ContainsKey('Mode') -and [string]::IsNullOrWhiteSpace($Mode)) -or (-not $routes.ContainsKey($routeKey)))
	{
		Write-Error "unrouted skill or mode '$routeKey'; run -ListRoutes on $PSCommandPath" -ErrorAction Continue
		exit 1
	}

	$route = $routes[$routeKey]
	$Agent = $route.Agent
	$Sandbox = $route.Sandbox
}

# Only the modes that start Codex refuse the key; a wait, route listing, or model check runs in any environment.
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

$modeValue = if ([string]::IsNullOrWhiteSpace($Mode)) { $null } else { $Mode }

if ($PSCmdlet.ParameterSetName -ceq 'Launch')
{
	# Every host shell call starts in a fresh working directory, so the detached run only ever sees absolute paths.
	$absoluteWorktree = [System.IO.Path]::GetFullPath($Worktree)
	$absolutePrompt = [System.IO.Path]::GetFullPath($PromptFile)
	$absoluteOutput = [System.IO.Path]::GetFullPath($OutFile)
	$runId = [guid]::NewGuid().ToString('N')
	$detachedPrompt = Join-Path $temporaryDirectory "broken-engine-codex-run-$runId.prompt.md"

	New-Item -ItemType Directory -Path $recordDirectory -Force | Out-Null
	# Records are kept for repeat waits, so a launch is what bounds the directory.
	Get-ChildItem -LiteralPath $recordDirectory -File -ErrorAction SilentlyContinue |
		Where-Object { $_.LastWriteTimeUtc -lt (Get-Date).ToUniversalTime().AddDays(-7) } |
		Remove-Item -Force -ErrorAction SilentlyContinue

	# The publishing rename needs the destination directory, and failing here beats failing after Codex ran.
	$outputParent = [System.IO.Path]::GetDirectoryName($absoluteOutput)
	New-Item -ItemType Directory -Path $outputParent -Force | Out-Null

	# The prompt is copied because the detached run outlives the caller that wrote it.
	Copy-Item -LiteralPath $absolutePrompt -Destination $detachedPrompt

	$start = [System.Diagnostics.ProcessStartInfo]::new((Get-Process -Id $PID).Path)
	# Shell execution keeps the worker from holding the caller's capture pipes open.
	$start.UseShellExecute = $true
	$start.WindowStyle = [System.Diagnostics.ProcessWindowStyle]::Hidden
	$start.WorkingDirectory = $ExecutionContext.SessionState.Path.CurrentFileSystemLocation.ProviderPath
	foreach ($argument in @('-NoProfile', '-NonInteractive', '-File', $PSCommandPath,
		'-InternalRunId', $runId, '-Skill', $Skill, '-Worktree', $absoluteWorktree, '-PromptFile', $detachedPrompt,
		'-OutFile', $absoluteOutput, '-Agent', $Agent, '-Sandbox', $Sandbox))
	{
		$start.ArgumentList.Add($argument)
	}
	if ($null -ne $modeValue)
	{
		$start.ArgumentList.Add('-Mode')
		$start.ArgumentList.Add($modeValue)
	}
	if ($NoRetry)
	{
		$start.ArgumentList.Add('-NoRetry')
	}
	$detached = [System.Diagnostics.Process]::Start($start)
	try
	{
		# The launch handle identifies this worker even if it has already exited.
		[ordered]@{
			schemaVersion = $runSchemaVersion
			runId = $runId
			processId = $detached.Id
			processStartTicks = $detached.StartTime.ToUniversalTime().Ticks
			outFile = $absoluteOutput
			skill = $Skill
			mode = $modeValue
			noRetry = [bool] $NoRetry
		} | ConvertTo-Json -Depth 4 -Compress | Set-Content -LiteralPath (Get-RunRecordPath $runId) -Encoding utf8NoBOM
	}
	finally
	{
		$detached.Dispose()
	}

	Wait-Run $runId
	exit 0
}

# Detached-run mode: the caller is already gone, so the result reaches it only through the records below.
$stagingOutput = Join-Path $temporaryDirectory "broken-engine-codex-run-$InternalRunId.output.md"
$retryStagingOutput = Join-Path $temporaryDirectory "broken-engine-codex-run-$InternalRunId.output-retry.md"
$pendingOutput = Join-Path ([System.IO.Path]::GetDirectoryName($OutFile)) "$InternalRunId.partial.md"
# Retained beside the run records: both attempts truncate and rewrite it, so it always holds the
# stderr of the attempt whose exit code is reported.
$stderrOutput = Join-Path $recordDirectory "$InternalRunId.stderr.txt"
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
		# A workspace-write retry would start from the first attempt's edits, so only read-only retries.
		if ($malformed -and (-not $NoRetry) -and ($Sandbox -ceq 'read-only'))
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
		# The CLI's own error text is the only diagnosis a caller gets, but the receipt is read into
		# a parent session's context, so only the last non-empty line survives, capped in length.
		$reason = "codex exited $exitCode"
		$tail = @(Get-Content -LiteralPath $stderrOutput -ErrorAction SilentlyContinue) -match '\S' | Select-Object -Last 1
		if ($null -ne $tail)
		{
			$tail = $tail.Trim()
			if ($tail.Length -gt 200)
			{
				$tail = $tail.Substring(0, 200)
			}

			$reason = "codex exited ${exitCode}: $tail"
		}
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
		schemaVersion = $runSchemaVersion
		skill = $Skill
		mode = $modeValue
		noRetry = [bool] $NoRetry
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
