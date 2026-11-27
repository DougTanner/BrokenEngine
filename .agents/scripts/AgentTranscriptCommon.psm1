Set-StrictMode -Version Latest

# Native Antigravity CLI (agy) and shared transcript helpers.
# Public exports are Agent-prefixed so callers can import this module alongside
# local script helpers without command shadowing.

$script:AgyUuidPattern = '^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$'
$script:AgySystemMessageWrapperPattern = '\AThe following is a <SYSTEM_MESSAGE> not actually sent by the user\. It is provided by the system as important information to pay attention to\.\r?\n\r?\n<SYSTEM_MESSAGE>\r?\n'
$script:AgyInvokeSubagentPattern = '(?s)Created the following subagent:\s*\{[^}]*?"conversationId"\s*:\s*"([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})"'
$script:AgyBackgroundTaskPattern = '\A(?:Created At:[^\r\n]*\r?\n)?Tool is running as a background task with task id:\s*([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}/task-\d+)\b'

function New-MalformedTranscriptLineException([int] $LineNumber, [string] $Message, [Exception] $Inner = $null) {
	$ex = if ($null -ne $Inner) {
		[FormatException]::new($Message, $Inner)
	}
	else {
		[FormatException]::new($Message)
	}
	$ex.Data['IsMalformedTranscriptLine'] = $true
	$ex.Data['LineNumber'] = $LineNumber
	return $ex
}

function ConvertTo-SeparatorPattern([string] $PathPrefix) {
	if ([string]::IsNullOrWhiteSpace($PathPrefix)) { return '' }
	$trimmed = $PathPrefix.Trim().Replace('\', '/').TrimEnd('/')
	if ([string]::IsNullOrEmpty($trimmed)) { return '' }
	$segments = $trimmed -split '/'
	return ($segments | ForEach-Object { [regex]::Escape($_) }) -join '[\\/]'
}

function Get-AgentTranscriptField($Object, [string] $Name) {
	if ($Object -isnot [Management.Automation.PSCustomObject]) { return $null }
	foreach ($property in $Object.PSObject.Properties) {
		if ($property.Name -ceq $Name) {
			if ($null -eq $property.Value) { return $null }
			# The unary comma keeps an array-valued field from unrolling into the pipeline.
			return , $property.Value
		}
	}
	return $null
}

function Protect-AgentTranscriptPath([string] $Text) {
	if ([string]::IsNullOrEmpty($Text)) { return '' }
	$userProfile = $env:USERPROFILE
	if ([string]::IsNullOrWhiteSpace($userProfile)) {
		$userProfile = [Environment]::GetFolderPath([Environment+SpecialFolder]::UserProfile)
	}
	$profilePattern = ConvertTo-SeparatorPattern $userProfile
	$appDataBrainPattern = ''
	if (-not [string]::IsNullOrWhiteSpace($env:ANTIGRAVITY_APP_DATA_DIR)) {
		$appDataBrainPattern = ConvertTo-SeparatorPattern (Join-Path $env:ANTIGRAVITY_APP_DATA_DIR 'brain')
	}

	$ignoreCase = [Text.RegularExpressions.RegexOptions]::IgnoreCase
	$redacted = $Text
	if (-not [string]::IsNullOrEmpty($appDataBrainPattern)) {
		$redacted = [regex]::Replace($redacted, "(?:file:///)?$appDataBrainPattern(?:[\\/][^\s''`"<>|]*)?", '<transcript-path>', $ignoreCase)
	}
	$leadingPrefix = if (-not [string]::IsNullOrEmpty($profilePattern)) {
		"(?:file:///)?(?:$profilePattern[\\/]|[A-Za-z]:[\\/][^\s''`"<>|;,]*?[\\/]|[\\/])?"
	}
	else {
		"(?:file:///)?(?:[A-Za-z]:[\\/][^\s''`"<>|;,]*?[\\/]|[\\/])?"
	}
	$redacted = [regex]::Replace($redacted, "${leadingPrefix}\.gemini[\\/]antigravity-cli[\\/]brain(?:[\\/][^\s''`"<>|]*)?", '<transcript-path>', $ignoreCase)
	$redacted = [regex]::Replace($redacted, "${leadingPrefix}\.claude[\\/]projects(?:[\\/][^\s''`"<>|]*)?", '<transcript-path>', $ignoreCase)

	if (-not [string]::IsNullOrEmpty($profilePattern)) {
		$redacted = [regex]::Replace($redacted, "(?:file:///)?$profilePattern(?=$|[\\/\s''`"<>|,;)\]])", '<user-profile>', $ignoreCase)
	}
	return $redacted
}

function ConvertTo-AgentLexicalCanonicalPath([string] $Path) {
	if ([string]::IsNullOrWhiteSpace($Path)) { return $null }
	try {
		$fullPath = [IO.Path]::GetFullPath($Path).Replace([IO.Path]::AltDirectorySeparatorChar, [IO.Path]::DirectorySeparatorChar)
		$root = [IO.Path]::GetPathRoot($fullPath)
		if ([string]::IsNullOrWhiteSpace($root)) { return $null }
		if ($fullPath.Length -gt $root.Length) {
			$fullPath = $fullPath.TrimEnd([char[]]@('\', '/'))
		}
		return $fullPath
	}
	catch { return $null }
}

function Test-AgentPathWithin([string] $Candidate, [string] $Root) {
	$candidatePath = ConvertTo-AgentLexicalCanonicalPath $Candidate
	$rootPath = ConvertTo-AgentLexicalCanonicalPath $Root
	if ($null -eq $candidatePath -or $null -eq $rootPath) { return $false }
	$prefix = if ($rootPath.EndsWith([IO.Path]::DirectorySeparatorChar)) { $rootPath } else { "$rootPath$([IO.Path]::DirectorySeparatorChar)" }
	return $candidatePath.Equals($rootPath, [StringComparison]::OrdinalIgnoreCase) -or
		$candidatePath.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)
}

function Get-AgentPathSafety([string] $Path) {
	$fullPath = ConvertTo-AgentLexicalCanonicalPath $Path
	if ($null -eq $fullPath) { return [pscustomobject]@{ Status = 'error'; Item = $null; Path = $null } }
	$root = [IO.Path]::GetPathRoot($fullPath)
	$segments = $fullPath.Substring($root.Length).Split([char[]]@('\', '/'), [StringSplitOptions]::RemoveEmptyEntries)
	$current = $root
	$item = $null
	foreach ($segment in @('') + $segments) {
		if ($segment.Length -ne 0) { $current = Join-Path $current $segment }
		try { $item = Get-Item -LiteralPath $current -Force -ErrorAction Stop }
		catch {
			if ($_.Exception -is [Management.Automation.ItemNotFoundException]) {
				return [pscustomobject]@{ Status = 'missing'; Item = $null; Path = $fullPath }
			}
			return [pscustomobject]@{ Status = 'error'; Item = $null; Path = $fullPath }
		}
		if (([IO.FileAttributes] $item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
			return [pscustomobject]@{ Status = 'unsafe'; Item = $null; Path = $fullPath }
		}
	}
	return [pscustomobject]@{ Status = 'safe'; Item = $item; Path = $fullPath }
}

function Get-AgentAgyBrainRoot([string] $OverrideRoot) {
	$candidate = if (-not [string]::IsNullOrWhiteSpace($OverrideRoot)) {
		$OverrideRoot
	}
	elseif (-not [string]::IsNullOrWhiteSpace($env:ANTIGRAVITY_APP_DATA_DIR)) {
		Join-Path $env:ANTIGRAVITY_APP_DATA_DIR 'brain'
	}
	else {
		# The inherited profile first: a Codex sandbox reports its own account's special folder.
		$userProfile = $env:USERPROFILE
		if ([string]::IsNullOrWhiteSpace($userProfile)) { $userProfile = [Environment]::GetFolderPath([Environment+SpecialFolder]::UserProfile) }
		Join-Path $userProfile '.gemini\antigravity-cli\brain'
	}
	$canonical = ConvertTo-AgentLexicalCanonicalPath $candidate
	if ($null -ne $canonical) { return $canonical }
	return $candidate
}

function Resolve-AgentAgyLogPair([string] $FullPath, [string] $ShortPath, [string] $SessionId) {
	$fullSafety = Get-AgentPathSafety $FullPath
	if ($fullSafety.Status -ceq 'safe') {
		$status = if ($fullSafety.Item.PSIsContainer) { 'error' } else { 'safe' }
		return [pscustomobject]@{ Status = $status; Path = $fullSafety.Path; SessionId = $SessionId }
	}
	if ($fullSafety.Status -cne 'missing') {
		return [pscustomobject]@{ Status = $fullSafety.Status; Path = $fullSafety.Path; SessionId = $SessionId }
	}
	$shortSafety = Get-AgentPathSafety $ShortPath
	if ($shortSafety.Status -ceq 'safe') {
		$status = if ($shortSafety.Item.PSIsContainer) { 'error' } else { 'safe' }
		return [pscustomobject]@{ Status = $status; Path = $shortSafety.Path; SessionId = $SessionId }
	}
	return [pscustomobject]@{ Status = $shortSafety.Status; Path = $shortSafety.Path; SessionId = $SessionId }
}

function Resolve-AgentAgyTranscriptPath {
	[CmdletBinding()]
	param(
		[string] $SessionId,
		[string] $TranscriptPath,
		[string] $BrainRoot
	)

	$hasSessionId = -not [string]::IsNullOrWhiteSpace($SessionId)
	$hasTranscriptPath = -not [string]::IsNullOrWhiteSpace($TranscriptPath)
	if ($hasSessionId -eq $hasTranscriptPath) {
		throw 'Pass either -SessionId or -TranscriptPath.'
	}

	if ($hasSessionId) {
		if ($SessionId -cnotmatch $script:AgyUuidPattern) {
			throw 'SessionId must be an exact lowercase UUID.'
		}
		$root = Get-AgentAgyBrainRoot $BrainRoot
		$logsDir = Join-Path (Join-Path (Join-Path $root $SessionId) '.system_generated') 'logs'
		return Resolve-AgentAgyLogPair (Join-Path $logsDir 'transcript_full.jsonl') (Join-Path $logsDir 'transcript.jsonl') $SessionId
	}

	$canonical = ConvertTo-AgentLexicalCanonicalPath $TranscriptPath
	if ($null -eq $canonical) {
		return [pscustomobject]@{ Status = 'error'; Path = $null; SessionId = $null }
	}
	$fileName = [IO.Path]::GetFileName($canonical)
	if ($fileName -ieq 'transcript_full.jsonl' -or $fileName -ieq 'transcript.jsonl') {
		$logsDir = [IO.Path]::GetDirectoryName($canonical)
		if (-not [string]::IsNullOrEmpty($logsDir) -and [IO.Path]::GetFileName($logsDir) -ieq 'logs') {
			$sysGenDir = [IO.Path]::GetDirectoryName($logsDir)
			if (-not [string]::IsNullOrEmpty($sysGenDir) -and [IO.Path]::GetFileName($sysGenDir) -ieq '.system_generated') {
				$convDir = [IO.Path]::GetDirectoryName($sysGenDir)
				if (-not [string]::IsNullOrEmpty($convDir)) {
					$convName = [IO.Path]::GetFileName($convDir)
					$resolvedSessionId = if ($convName -cmatch $script:AgyUuidPattern) { $convName } else { $null }
					return Resolve-AgentAgyLogPair (Join-Path $logsDir 'transcript_full.jsonl') (Join-Path $logsDir 'transcript.jsonl') $resolvedSessionId
				}
			}
		}
	}

	return [pscustomobject]@{ Status = 'passthrough'; Path = $TranscriptPath; SessionId = $null }
}

function Open-AgentTranscriptReadStream([string] $Path) {
	return [IO.FileStream]::new($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
}

function Add-AgyReadError([Collections.Generic.List[object]] $ReadErrors, [Collections.Generic.HashSet[string]] $Seen, [string] $Locator, [string] $Code, [string] $Message) {
	$key = "$Code`0$Locator"
	if ($Seen.Add($key)) {
		$ReadErrors.Add([pscustomobject]@{ locator = $Locator; code = $Code; message = $Message })
	}
}

function Resolve-AgentAgyParentSession {
	[CmdletBinding()]
	param(
		[string] $SessionId = $env:ANTIGRAVITY_CONVERSATION_ID,
		[string] $BrainRoot
	)

	if ([string]::IsNullOrWhiteSpace($SessionId) -or $SessionId -cnotmatch $script:AgyUuidPattern) {
		throw 'SessionId must be an exact lowercase UUID.'
	}

	$root = Get-AgentAgyBrainRoot $BrainRoot
	$readErrors = [Collections.Generic.List[object]]::new()
	$seenErrors = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)

	$convDirs = @()
	$rootSafety = Get-AgentPathSafety $root
	if ($rootSafety.Status -ceq 'safe' -and $rootSafety.Item.PSIsContainer) {
		try {
			$entries = @(Get-ChildItem -LiteralPath $rootSafety.Path -Force -ErrorAction Stop)
			$convDirs = @($entries | Where-Object {
				$_.PSIsContainer -and
				(([IO.FileAttributes] $_.Attributes -band [IO.FileAttributes]::ReparsePoint) -eq 0) -and
				$_.Name -cmatch $script:AgyUuidPattern
			} | Sort-Object Name)
		}
		catch {
			Add-AgyReadError $readErrors $seenErrors 'brain' 'transcript.read-failed' 'Transcript store contents could not be safely read.'
		}
	}
	elseif ($rootSafety.Status -cne 'missing') {
		$code = if ($rootSafety.Status -ceq 'unsafe') { 'transcript.unsafe-path' } else { 'transcript.read-failed' }
		Add-AgyReadError $readErrors $seenErrors 'brain' $code 'Transcript store path could not be safely read.'
	}

	$visited = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
	[void] $visited.Add($SessionId)
	$currentId = $SessionId
	$parentSessionId = $null
	$cycleDetected = $false

	while ($true) {
		$foundParent = $null
		foreach ($convDir in $convDirs) {
			if ($convDir.Name -ceq $currentId) { continue }
			$probePath = Join-Path (Join-Path (Join-Path $convDir.FullName '.system_generated') 'subagents') "$currentId.json"
			$safety = Get-AgentPathSafety $probePath
			if ($safety.Status -ceq 'missing') { continue }
			$locator = "brain/$($convDir.Name)/.system_generated/subagents/$currentId.json"
			if ($safety.Status -cne 'safe' -or $safety.Item.PSIsContainer) {
				$code = if ($safety.Status -ceq 'unsafe') { 'transcript.unsafe-path' } else { 'transcript.read-failed' }
				Add-AgyReadError $readErrors $seenErrors $locator $code 'Subagent metadata path could not be safely read.'
				continue
			}
			try {
				$rawText = [IO.File]::ReadAllText($safety.Path)
				$subagentRecord = ConvertFrom-Json -InputObject $rawText -Depth 16 -ErrorAction Stop
				if ($subagentRecord -isnot [Management.Automation.PSCustomObject]) {
					throw 'Subagent metadata is not a JSON object.'
				}
				$recordedId = Get-AgentTranscriptField $subagentRecord 'conversationId'
				if ($null -ne $recordedId -and ($recordedId -isnot [string] -or $recordedId -cne $currentId)) {
					throw 'Subagent metadata conversationId does not match probe target.'
				}
			}
			catch {
				Add-AgyReadError $readErrors $seenErrors $locator 'transcript.read-failed' 'Subagent metadata could not be read.'
				continue
			}
			$foundParent = $convDir.Name
			break
		}
		if ($null -eq $foundParent) { break }
		if ($null -eq $parentSessionId) { $parentSessionId = $foundParent }
		if (-not $visited.Add($foundParent)) {
			$cycleDetected = $true
			break
		}
		$currentId = $foundParent
	}

	$rootSessionId = $null
	$rootTranscriptPath = $null
	$rootResolved = $null
	if (-not $cycleDetected) {
		$rootSessionId = $currentId
		$rootResolved = Resolve-AgentAgyTranscriptPath -SessionId $rootSessionId -BrainRoot $root
		if ($rootResolved.Status -ceq 'safe') {
			$probeStream = $null
			try {
				$probeStream = Open-AgentTranscriptReadStream $rootResolved.Path
				$rootTranscriptPath = $rootResolved.Path
			}
			catch {
				$locator = "brain/$rootSessionId/.system_generated/logs/$([IO.Path]::GetFileName($rootResolved.Path))"
				Add-AgyReadError $readErrors $seenErrors $locator 'transcript.read-failed' 'Root transcript file could not be opened for reading.'
			}
			finally {
				if ($null -ne $probeStream) { $probeStream.Dispose() }
			}
		}
		elseif ($rootResolved.Status -ceq 'unsafe' -or $rootResolved.Status -ceq 'error') {
			$code = if ($rootResolved.Status -ceq 'unsafe') { 'transcript.unsafe-path' } else { 'transcript.read-failed' }
			$locator = if (-not [string]::IsNullOrEmpty($rootResolved.Path)) {
				"brain/$rootSessionId/.system_generated/logs/$([IO.Path]::GetFileName($rootResolved.Path))"
			}
			else {
				"brain/$rootSessionId"
			}
			Add-AgyReadError $readErrors $seenErrors $locator $code 'Root transcript path could not be safely read.'
		}
	}

	$isAvailable = (-not $cycleDetected) -and (-not [string]::IsNullOrEmpty($rootTranscriptPath)) -and ($readErrors.Count -eq 0)
	$status = if ($isAvailable) { 'available' } else { 'unavailable' }
	$reason = $null
	if (-not $isAvailable) {
		$reasons = [Collections.Generic.List[string]]::new()
		if ($cycleDetected) { $reasons.Add('cycle-detected') }
		if ($readErrors.Count -gt 0) { $reasons.Add('read-error') }
		if ((-not $cycleDetected) -and ($null -ne $rootResolved) -and ($rootResolved.Status -ceq 'missing')) {
			$reasons.Add('root-transcript-missing')
		}
		$reason = $reasons -join ','
	}

	return [pscustomobject]@{
		SessionId = $SessionId
		ParentSessionId = $parentSessionId
		RootSessionId = $rootSessionId
		RootTranscriptPath = $rootTranscriptPath
		CycleDetected = $cycleDetected
		ReadErrors = @($readErrors)
		Status = $status
		Reason = $reason
	}
}

function Test-AgentAgyTranscript([string] $Path) {
	$stream = Open-AgentTranscriptReadStream $Path
	$reader = [IO.StreamReader]::new($stream)
	try {
		$lineNumber = 0
		while ($null -ne ($line = $reader.ReadLine())) {
			$lineNumber++
			if ([string]::IsNullOrWhiteSpace($line)) { continue }
			try {
				$record = ConvertFrom-Json -InputObject $line -Depth 100 -NoEnumerate -DateKind String -ErrorAction Stop
			}
			catch {
				throw (New-MalformedTranscriptLineException $lineNumber "Transcript line $lineNumber is not valid JSON." $_.Exception)
			}
			if ($record -isnot [Management.Automation.PSCustomObject]) { return $false }
			$type = Get-AgentTranscriptField $record 'type'
			$stepIndex = Get-AgentTranscriptField $record 'step_index'
			return ($null -ne $stepIndex) -and ($type -is [string]) -and (
				$type -ceq 'USER_INPUT' -or
				$type -ceq 'SYSTEM_MESSAGE' -or
				$type -ceq 'PLANNER_RESPONSE' -or
				$type -ceq 'GENERIC'
			)
		}
		return $false
	}
	finally {
		$reader.Dispose()
		$stream.Dispose()
	}
}

function Read-AgentAgyTranscript([string] $Path) {
	$stream = Open-AgentTranscriptReadStream $Path
	$reader = [IO.StreamReader]::new($stream)
	$pendingToolCalls = [Collections.Generic.Queue[object]]::new()
	$subagentToolUseIds = @{}
	$taskToolUseIds = @{}
	try {
		$lineNumber = 0
		while ($null -ne ($line = $reader.ReadLine())) {
			$lineNumber++
			if ([string]::IsNullOrWhiteSpace($line)) { continue }
			try {
				$raw = ConvertFrom-Json -InputObject $line -Depth 100 -NoEnumerate -DateKind String -ErrorAction Stop
			}
			catch {
				throw (New-MalformedTranscriptLineException $lineNumber "Transcript line $lineNumber is not valid JSON." $_.Exception)
			}
			if ($raw -isnot [Management.Automation.PSCustomObject]) {
				throw (New-MalformedTranscriptLineException $lineNumber "Transcript line $lineNumber is not a JSON object.")
			}

			$type = [string](Get-AgentTranscriptField $raw 'type')
			$source = [string](Get-AgentTranscriptField $raw 'source')
			$status = [string](Get-AgentTranscriptField $raw 'status')
			$stepIndexRaw = Get-AgentTranscriptField $raw 'step_index'
			$stepIndex = if ($null -ne $stepIndexRaw) { [long] $stepIndexRaw } else { $null }
			$createdAtRaw = Get-AgentTranscriptField $raw 'created_at'
			$createdAt = if ($null -ne $createdAtRaw) { [string] $createdAtRaw } else { $null }
			$contentRaw = Get-AgentTranscriptField $raw 'content'
			$content = if ($contentRaw -is [string]) { $contentRaw } else { '' }
			$errorRaw = Get-AgentTranscriptField $raw 'error'
			$errorText = if ($errorRaw -is [string]) { $errorRaw } else { '' }
			$isError = (-not [string]::IsNullOrEmpty($errorText)) -or ($status -ceq 'ERROR')

			$toolCallsList = [Collections.Generic.List[object]]::new()
			$pairedToolCall = $null
			$deliveryToolUseId = $null
			$isIdleNotice = $false

			if ($type -ceq 'PLANNER_RESPONSE') {
				$rawCalls = Get-AgentTranscriptField $raw 'tool_calls'
				if ($rawCalls -is [object[]]) {
					$stepTag = if ($null -ne $stepIndex) { [string] $stepIndex } else { "line$lineNumber" }
					for ($idx = 0; $idx -lt $rawCalls.Count; $idx++) {
						$tc = $rawCalls[$idx]
						$tcObj = [pscustomobject]@{
							ToolUseId = "step-$stepTag-$idx"
							Name = [string](Get-AgentTranscriptField $tc 'name')
							Args = (Get-AgentTranscriptField $tc 'args')
						}
						$toolCallsList.Add($tcObj)
						$pendingToolCalls.Enqueue($tcObj)
					}
				}
			}
			elseif ($type -ceq 'GENERIC') {
				if ($pendingToolCalls.Count -gt 0) {
					$dequeued = $pendingToolCalls.Dequeue()
					$launchedSubagentId = $null
					$launchedTaskId = $null
					if ($dequeued.Name -ceq 'invoke_subagent' -and $content -cmatch $script:AgyInvokeSubagentPattern) {
						$launchedSubagentId = $Matches[1]
						$subagentToolUseIds[$launchedSubagentId] = $dequeued.ToolUseId
					}
					elseif (-not $isError -and $dequeued.Name -ceq 'run_command' -and $content -cmatch $script:AgyBackgroundTaskPattern) {
						$launchedTaskId = $Matches[1]
						if (-not $taskToolUseIds.ContainsKey($launchedTaskId)) {
							$taskToolUseIds[$launchedTaskId] = $dequeued.ToolUseId
						}
					}
					$pairedToolCall = [pscustomobject]@{
						ToolUseId = $dequeued.ToolUseId
						ToolName = $dequeued.Name
						LaunchedSubagentId = $launchedSubagentId
						LaunchedTaskId = $launchedTaskId
					}
				}
			}
			elseif ($type -ceq 'SYSTEM_MESSAGE' -and $source -ceq 'SYSTEM') {
				$deliveryBody = $content
				if ($deliveryBody -cmatch $script:AgySystemMessageWrapperPattern) {
					$deliveryBody = $deliveryBody.Substring($Matches[0].Length)
				}
				if ($deliveryBody -cmatch '(?s)\A\[Message\]\s+(?<header>[^\r\n]*?)\bcontent=(?<body>.*)\z') {
					$header = $Matches['header']
					$body = $Matches['body']
					if ($header -cmatch '(?:^|\s)sender=([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}/task-\d+)(?=\s|$)') {
						if ($taskToolUseIds.ContainsKey($Matches[1])) {
							$deliveryToolUseId = $taskToolUseIds[$Matches[1]]
						}
					}
					elseif ($header -cmatch '(?:^|\s)sender=([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})(?=\s|$)') {
						if ($subagentToolUseIds.ContainsKey($Matches[1])) {
							$deliveryToolUseId = $subagentToolUseIds[$Matches[1]]
						}
					}
					elseif ($header -cmatch '(?:^|\s)sender=system(?=\s|$)' -and $body -cmatch '\ASubagent ([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}) has gone idle') {
						$isIdleNotice = $true
						if ($subagentToolUseIds.ContainsKey($Matches[1])) {
							$deliveryToolUseId = $subagentToolUseIds[$Matches[1]]
						}
					}
				}
			}

			[pscustomobject]@{
				LineNumber = $lineNumber
				Type = $type
				Source = $source
				Status = $status
				StepIndex = $stepIndex
				CreatedAt = $createdAt
				Content = $content
				Error = $errorText
				IsError = $isError
				ToolCalls = @($toolCallsList)
				PairedToolCall = $pairedToolCall
				DeliveryToolUseId = $deliveryToolUseId
				IsIdleNotice = $isIdleNotice
			}
		}
	}
	finally {
		$reader.Dispose()
		$stream.Dispose()
	}
}

Export-ModuleMember -Function Get-AgentTranscriptField, Protect-AgentTranscriptPath, ConvertTo-AgentLexicalCanonicalPath, Test-AgentPathWithin, Get-AgentPathSafety, Get-AgentAgyBrainRoot, Resolve-AgentAgyTranscriptPath, Resolve-AgentAgyParentSession, Test-AgentAgyTranscript, Read-AgentAgyTranscript
