# PreToolUse hook for the Claude Code Bash and PowerShell tools, registered in
# .claude/settings.json, and for the Antigravity CLI run_command tool, registered
# in .agents/hooks.json. Reads the hook JSON from stdin and blocks the shell
# forms the root AGENTS.md rules forbid:
#   - directory change: cd, chdir, pushd, popd, Set-Location, sl,
#     Push-Location, or Pop-Location as a command word;
#   - -ExecutionPolicy Bypass, including the -ep and -ex... abbreviations;
#   - a .ps1 or .psm1 path rooted under the input cwd but outside <cwd>/Temp/;
#   - git stash bare, pop, save, clear, push without both -u and -m, or apply
#     without a hexadecimal SHA;
#   - sed -i/--in-place or perl -i on a file outside Temp/;
#   - a deletion whose targets are computed rather than written literally:
#     rm, rmdir, Remove-Item and its aliases, git rm, or git clean with a
#     wildcard, a [...] pair (allowed in a -LiteralPath value or a quoted Bash
#     rm argument), -Include/-Exclude/-Filter, a variable or substitution,
#     Bash brace expansion, a PowerShell (...) or splat argument, or pipeline
#     input; git clean without an exact path; find -delete or -exec rm; xargs rm.
# Each command is tokenized per shell, so quoted text, here-strings, and heredoc
# bodies never count as commands; the script-path and deletion checks still
# read quoted arguments. A command inside a quoted string (bash -c '...',
# pwsh -Command "...") or inside a script the command runs is not inspected:
# the hook backs up the prose rules rather than replacing them.
# Exit codes:
#   0 - allowed, including every non-shell tool, input that cannot be parsed,
#       and any unexpected script error
#   2 - blocked; one stderr line names the rule and the correct form
# Antigravity CLI input (a toolCall property) always exits 0; a block is
# {"decision":"deny","reason":...} on stdout, the reason naming the rule.

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$script:Utf8 = [Text.UTF8Encoding]::new($false)
$script:DirectoryCommands = @('cd', 'chdir', 'pushd', 'popd', 'set-location', 'sl', 'push-location', 'pop-location')
$script:DeletionCommands = @('rm', 'rmdir', 'remove-item', 'ri', 'del', 'erase', 'rd')
$script:RootedPattern = '^(?:[A-Za-z]:[\\/]|[\\/]|~(?:[\\/]|$))'
$script:ScriptPathPattern = [regex]::new('(?i)(?:^|(?<=[\s''"&=(,;|]))((?:[a-z]:[\\/]|~[\\/]|[\\/])[^\s''"<>|;&()]*\.psm?1)(?=$|[\s''",;)|&])')
$script:Messages = @{
	DirectoryChange = 'do not change the working directory (cd, Set-Location, Push-Location, or their aliases); run every command from the worktree root and address files by path.'
	ExecutionPolicy = 'never use -ExecutionPolicy Bypass; run a repository script as pwsh -NoProfile -NonInteractive -File <repo-relative script path> [arguments].'
	AbsoluteScriptPath = 'never run a repository script by absolute path; use pwsh -NoProfile -NonInteractive -File <repo-relative script path> [arguments] from the worktree root.'
	Stash = 'the git stash stack is shared by every worktree; use only git stash push -u -m <unique-tag>, git stash list, git stash apply <sha>, and git stash drop.'
	InPlaceEdit = 'do not edit files in place with sed or perl outside Temp/; change tracked files with the host Edit tool.'
	ComputedDeletion = 'delete only by exact literal paths, never by wildcard, pattern filter, pipeline, find, xargs, variable, or substitution; name each path literally (in PowerShell, -LiteralPath ''<path>''); a directory may be deleted recursively.'
}

function Write-Stderr([string] $Message) {
	$stderr = [Console]::OpenStandardError()
	$bytes = $script:Utf8.GetBytes($Message)
	$stderr.Write($bytes, 0, $bytes.Length)
	$stderr.Flush()
}

function Get-WordsFrom($Words, [int] $Start) {
	return , $Words.GetRange($Start, $Words.Count - $Start)
}

function Test-AbsoluteScriptPath([string] $Text, [string] $CwdPath) {
	$candidates = [System.Collections.Generic.List[string]]::new()
	if ($Text -match '(?i)\.psm?1$') { $candidates.Add($Text) }
	foreach ($match in $script:ScriptPathPattern.Matches($Text)) { $candidates.Add($match.Groups[1].Value) }
	foreach ($match in [regex]::Matches($Text, "(?i)'([^']+\.psm?1)'")) { $candidates.Add($match.Groups[1].Value) }
	foreach ($candidate in $candidates) {
		if ($candidate -notmatch $script:RootedPattern) { continue }
		$path = ConvertTo-ComparablePath $candidate $CwdPath
		if ((Test-UnderDirectory $path $CwdPath) -and -not (Test-UnderDirectory $path "$CwdPath/Temp")) { return $true }
	}
	return $false
}

function Test-ExecutionPolicyBypass($Words) {
	for ($j = 0; $j -lt $Words.Count; $j++) {
		if ($Words[$j].Value -notmatch '^-([A-Za-z]+)(?::(.*))?$') { continue }
		$parameter = $Matches[1].ToLowerInvariant()
		$setting = $Matches[2]
		if ($parameter -ne 'ep' -and ($parameter.Length -lt 2 -or -not 'executionpolicy'.StartsWith($parameter))) { continue }
		if ($null -eq $setting -and $j + 1 -lt $Words.Count) { $setting = $Words[$j + 1].Value }
		if ($setting -eq 'bypass') { return $true }
	}
	return $false
}

function Get-GitSubcommandIndex($Arguments) {
	for ($j = 0; $j -lt $Arguments.Count; $j++) {
		$value = $Arguments[$j].Value
		if (@('-C', '-c', '--git-dir', '--work-tree', '--namespace', '--config-env') -ccontains $value) { $j++ }
		elseif (-not $value.StartsWith('-')) { return $j }
	}
	return -1
}

function Test-UnsafeStash($Arguments) {
	if ($Arguments.Count -eq 0) { return $true }
	$subcommand = 'push'
	$options = @($Arguments | ForEach-Object Value)
	if (-not $options[0].StartsWith('-')) {
		$subcommand = $options[0].ToLowerInvariant()
		$options = @($options | Select-Object -Skip 1)
	}
	if (@('pop', 'save', 'clear') -contains $subcommand) { return $true }
	if ($subcommand -eq 'push') {
		$untracked = ($options -ccontains '-u') -or ($options -contains '--include-untracked')
		$message = @($options | Where-Object { $_ -cmatch '^-m' -or $_ -match '^--message(=|$)' }).Count -gt 0
		return -not ($untracked -and $message)
	}
	if ($subcommand -eq 'apply') { return @($options | Where-Object { $_ -match '^[0-9a-f]{7,40}$' }).Count -eq 0 }
	return $false
}

function Test-InPlaceEdit([string] $Name, $Arguments, [string] $CwdPath) {
	$inPlace = $false
	$hasScript = $false
	$files = [System.Collections.Generic.List[string]]::new()
	for ($j = 0; $j -lt $Arguments.Count; $j++) {
		$value = $Arguments[$j].Value
		if ($Name -eq 'sed') {
			if ($value -cmatch '^-[Enrsuz]*i' -or $value -match '^--in-place(=|$)') { $inPlace = $true; continue }
			if (@('-e', '-f', '--expression', '--file') -ccontains $value) { $hasScript = $true; $j++; continue }
			if ($value -match '^--(expression|file)=') { $hasScript = $true; continue }
		}
		else {
			if ($value -cmatch '^-[aclnpsuwTtUWX0-9]*i') { $inPlace = $true; continue }
			if ($value -cmatch '^-[A-Za-z]*[eE]$') { $hasScript = $true; $j++; continue }
		}
		if ($value.StartsWith('-') -and $value.Length -gt 1) { continue }
		$files.Add($value)
	}
	if (-not $inPlace) { return $false }
	if (-not $hasScript -and $files.Count -gt 0) { $files.RemoveAt(0) }
	foreach ($file in $files) {
		if (-not (Test-UnderDirectory (ConvertTo-ComparablePath $file $CwdPath) "$CwdPath/Temp")) { return $true }
	}
	return $false
}

function Test-GitCleanPath($Arguments) {
	for ($j = 0; $j -lt $Arguments.Count; $j++) {
		$value = $Arguments[$j].Value
		if ($value -eq '--') { return $j + 1 -lt $Arguments.Count }
		if ($value -ceq '-e' -or $value -eq '--exclude') { $j++ }
		elseif (-not $value.StartsWith('-')) { return $true }
	}
	return $false
}

function Test-ComputedTarget($Arguments, [bool] $IsBash, [bool] $IsGit) {
	$literalNext = $false
	foreach ($word in $Arguments) {
		$isLiteral = $literalNext
		$literalNext = $false
		if (-not $IsBash -and -not $IsGit -and $word.Raw -match '^-([A-Za-z]+)(:?)') {
			$parameter = $Matches[1].ToLowerInvariant()
			if ($parameter.Length -ge 2 -and ('include'.StartsWith($parameter) -or 'exclude'.StartsWith($parameter) -or 'filter'.StartsWith($parameter))) { return $true }
			$isLiteralPath = $parameter -eq 'lp' -or $parameter -eq 'pspath' -or 'literalpath'.StartsWith($parameter)
			if ($Matches[2]) { $isLiteral = $isLiteralPath } else { $literalNext = $isLiteralPath }
		}
		elseif ($isLiteral -and $word.Raw.EndsWith(',')) { $literalNext = $true }
		if ($word.Value.IndexOfAny([char[]]'*?') -ge 0) { return $true }
		# Bash rm reads [...] inside quotes literally; PowerShell -Path and Git pathspecs expand it even quoted.
		$bracketText = if ($IsBash -and -not $IsGit) { $word.View } elseif ($isLiteral) { '' } else { $word.Value }
		if ($bracketText -match '\[[^\]]*\]') { return $true }
		if ($word.HasVariable -or $word.HasSubstitution) { return $true }
		if ($IsBash -and $word.View -match '\{[^{}]*(,|\.\.)[^{}]*\}') { return $true }
		if (-not $IsBash -and $word.IsSplat) { return $true }
	}
	return $false
}

function Test-ComputedDeletion($Element, $Arguments, [bool] $IsBash) {
	$name = $Element.Name
	if ($name -eq 'find') {
		for ($j = 0; $j -lt $Arguments.Count; $j++) {
			$value = $Arguments[$j].Value
			if ($value -ceq '-delete') { return $true }
			if (@('-exec', '-execdir', '-ok', '-okdir') -ccontains $value -and $j + 1 -lt $Arguments.Count -and
				$script:DeletionCommands -contains $Arguments[$j + 1].Value) { return $true }
		}
		return $false
	}
	if ($name -eq 'xargs') {
		for ($j = 0; $j -lt $Arguments.Count; $j++) {
			$value = $Arguments[$j].Value
			if ($script:DeletionCommands -contains $value) { return $true }
			if ($value -eq 'git' -and $j + 1 -lt $Arguments.Count -and @('rm', 'clean') -contains $Arguments[$j + 1].Value) { return $true }
		}
		return $false
	}
	$isGit = $name -eq 'git'
	if ($isGit) {
		$index = Get-GitSubcommandIndex $Arguments
		if ($index -lt 0 -or @('rm', 'clean') -notcontains $Arguments[$index].Value) { return $false }
		$subcommand = $Arguments[$index].Value
		$Arguments = Get-WordsFrom $Arguments ($index + 1)
		if ($subcommand -eq 'clean' -and -not (Test-GitCleanPath $Arguments)) { return $true }
	}
	elseif ($script:DeletionCommands -notcontains $name) { return $false }
	if ($Element.PrecededBy -eq '|' -or $Element.FollowedBy -eq '(') { return $true }
	return Test-ComputedTarget $Arguments $IsBash $isGit
}

function Get-ElementViolation($Element, [bool] $IsBash, [string] $CwdPath) {
	foreach ($word in $Element.Words) {
		if (Test-AbsoluteScriptPath $word.Value $CwdPath) { return $script:Messages.AbsoluteScriptPath }
	}
	if ($Element.CommandIndex -lt 0) { return $null }
	$name = $Element.Name
	$arguments = Get-WordsFrom $Element.Words ($Element.CommandIndex + 1)
	if ($script:DirectoryCommands -contains $name) { return $script:Messages.DirectoryChange }
	if (Test-ExecutionPolicyBypass $arguments) { return $script:Messages.ExecutionPolicy }
	if ($name -eq 'git') {
		$index = Get-GitSubcommandIndex $arguments
		if ($index -ge 0 -and $arguments[$index].Value -eq 'stash' -and (Test-UnsafeStash (Get-WordsFrom $arguments ($index + 1)))) {
			return $script:Messages.Stash
		}
	}
	if (($name -eq 'sed' -or $name -eq 'perl') -and (Test-InPlaceEdit $name $arguments $CwdPath)) { return $script:Messages.InPlaceEdit }
	if (Test-ComputedDeletion $Element $arguments $IsBash) { return $script:Messages.ComputedDeletion }
	return $null
}

function Get-ShellRuleViolation([string] $Command, [bool] $IsBash, [string] $Cwd) {
	$tokenLists = (Get-ShellTokenLists $Command $IsBash).TokenLists
	$cwdPath = ConvertTo-ComparablePath $Cwd $Cwd
	for ($j = 0; $j -lt $tokenLists.Count; $j++) {
		$elements = Get-ShellElements $tokenLists[$j] $IsBash
		foreach ($element in $elements) {
			$message = Get-ElementViolation $element $IsBash $cwdPath
			if ($message) { return $message }
		}
	}
	return $null
}

try {
	Import-Module (Join-Path $PSScriptRoot 'AgentShellLexer.psm1')
	$reader = [IO.StreamReader]::new([Console]::OpenStandardInput(), $script:Utf8)
	$hook = $reader.ReadToEnd() | ConvertFrom-Json
	# A property test, not a read: strict mode throws on a missing property, and
	# the fail-open catch would then allow every Claude Code command.
	if ($hook.PSObject.Properties['toolCall']) {
		if ($hook.toolCall.name -cne 'run_command') { exit 0 }
		$command = $hook.toolCall.args.CommandLine
		if ($command -isnot [string]) { exit 0 }
		# Antigravity CLI runs run_command in PowerShell 7.
		$message = Get-ShellRuleViolation $command $false ([string]$hook.cwd)
		if ($message) {
			$stdout = [Console]::OpenStandardOutput()
			$bytes = $script:Utf8.GetBytes((ConvertTo-Json -Compress ([ordered]@{ decision = 'deny'; reason = "Invoke-PreToolUseHook: $message" })))
			$stdout.Write($bytes, 0, $bytes.Length)
			$stdout.Flush()
		}
		exit 0
	}
	if (@('Bash', 'PowerShell') -cnotcontains $hook.tool_name) { exit 0 }
	$command = $hook.tool_input.command
	if ($command -isnot [string]) { exit 0 }
	$message = Get-ShellRuleViolation $command ($hook.tool_name -eq 'Bash') ([string]$hook.cwd)
	if ($message) {
		Write-Stderr "Invoke-PreToolUseHook: $message`n"
		exit 2
	}
}
catch {
	# Fail open: a hook bug must never block shell use; the prose rules still apply.
}
exit 0
