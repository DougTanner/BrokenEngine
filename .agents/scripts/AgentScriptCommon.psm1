Set-StrictMode -Version Latest

# Exports are Agent-prefixed so they can never shadow a host script's local Invoke-Git/Get-CanonicalPath
# when this module is imported by a script invoked in-process from that host (same convention as
# FinalizeWorkflowCommon's Finalize-prefixed exports).

function Get-AgentCanonicalPath([string] $Path) {
	$full = [System.IO.Path]::GetFullPath($Path)
	$root = [System.IO.Path]::GetPathRoot($full)
	if ($full.Length -gt $root.Length) { return $full.TrimEnd('\', '/') }
	return $full
}

function Invoke-AgentGit([string[]] $Arguments) {
	$output = @(& git @Arguments 2>&1)
	if ($LASTEXITCODE -ne 0) { throw "git $($Arguments -join ' ') failed: $($output -join '; ')" }
	return $output
}

# Runs a child process with captured UTF-8 stdout/stderr and GIT_OPTIONAL_LOCKS=0; the other scripts keep
# their own ProcessStartInfo runners until they are next changed.
function Invoke-AgentProcess([string] $FileName, [string[]] $Arguments, [string] $WorkingDirectory) {
	$utf8 = [Text.UTF8Encoding]::new($false)
	$start = [Diagnostics.ProcessStartInfo]::new()
	$start.FileName = $FileName
	$start.WorkingDirectory = $WorkingDirectory
	$start.UseShellExecute = $false
	$start.CreateNoWindow = $true
	$start.RedirectStandardOutput = $true
	$start.RedirectStandardError = $true
	$start.StandardOutputEncoding = $utf8
	$start.StandardErrorEncoding = $utf8
	$start.Environment['GIT_OPTIONAL_LOCKS'] = '0'
	foreach ($argument in $Arguments) { [void] $start.ArgumentList.Add($argument) }
	$process = [Diagnostics.Process]::new()
	$process.StartInfo = $start
	if (-not $process.Start()) { throw "Could not start $FileName with: $($Arguments -join ' ')" }
	$stdoutTask = $process.StandardOutput.ReadToEndAsync()
	$stderrTask = $process.StandardError.ReadToEndAsync()
	$process.WaitForExit()
	$run = [pscustomobject] @{
		ExitCode = $process.ExitCode
		Stdout = $stdoutTask.GetAwaiter().GetResult()
		Stderr = $stderrTask.GetAwaiter().GetResult()
	}
	$process.Dispose()
	return $run
}

function Get-AgentNormalizedText([byte[]] $Bytes) {
	$offset = 0
	if ($Bytes.Length -ge 3 -and $Bytes[0] -eq 0xEF -and $Bytes[1] -eq 0xBB -and $Bytes[2] -eq 0xBF)
	{
		$offset = 3
	}

	$strictUtf8 = [Text.UTF8Encoding]::new($false, $true)
	$text = $strictUtf8.GetString($Bytes, $offset, $Bytes.Length - $offset)
	return $text.Replace("`r`n", "`n").Replace("`r", "`n")
}

function Measure-AgentTokenCount([string] $Text) {
	$byteCount = [Text.UTF8Encoding]::new($false).GetByteCount($Text)
	return [int64](($byteCount + 3) -shr 2)
}

function Test-AgentBlockOpener([string[]] $Lines, [int] $Number) {
	# A column-0 `{` opens a function or class body unless the nearest non-blank line above it is a namespace.
	if ($Lines[$Number - 1] -cnotmatch '^\{\s*$') { return $false }
	for ($above = $Number - 1; $above -ge 1; $above--) {
		if ([string]::IsNullOrWhiteSpace($Lines[$above - 1])) { continue }
		return $Lines[$above - 1] -cnotmatch '^\s*namespace\b'
	}
	return $true
}

function Get-AgentMarkdownSearchText
{
	param([string] $Body)

	$characters = $Body.ToCharArray()
	$inFence = $false
	$fenceCharacter = [char] 0
	$fenceLength = 0
	$offset = 0
	while ($offset -lt $Body.Length)
	{
		$newline = $Body.IndexOf("`n", $offset)
		$lineEnd = if ($newline -lt 0) { $Body.Length } else { $newline }
		$line = $Body.Substring($offset, $lineEnd - $offset)
		$maskLine = $inFence
		if ($inFence)
		{
			$closingPattern = '^ {{0,3}}{0}{{{1},}}[ \t]*$' -f [regex]::Escape([string] $fenceCharacter), $fenceLength
			if ($line -match $closingPattern)
			{
				$inFence = $false
			}
		}
		else
		{
			$fenceMatch = [regex]::Match($line, '^ {0,3}(?<fence>`{3,}|~{3,})')
			if ($fenceMatch.Success)
			{
				$fence = $fenceMatch.Groups['fence'].Value
				$fenceCharacter = $fence[0]
				$fenceLength = $fence.Length
				$inFence = $true
				$maskLine = $true
			}
		}
		if ($maskLine)
		{
			for ($i = $offset; $i -lt $lineEnd; $i++)
			{
				$characters[$i] = ' '
			}
		}
		$offset = if ($newline -lt 0) { $Body.Length } else { $newline + 1 }
	}

	for ($i = 0; $i -lt $characters.Length; $i++)
	{
		if ($characters[$i] -ne '`')
		{
			continue
		}
		$openingStart = $i
		while ($i -lt $characters.Length -and $characters[$i] -eq '`')
		{
			$i++
		}
		$openingLength = $i - $openingStart
		$closingStart = -1
		for ($candidate = $i; $candidate -lt $characters.Length; $candidate++)
		{
			if ($characters[$candidate] -ne '`')
			{
				continue
			}
			$runStart = $candidate
			while ($candidate -lt $characters.Length -and $characters[$candidate] -eq '`')
			{
				$candidate++
			}
			if ($candidate - $runStart -eq $openingLength)
			{
				$closingStart = $runStart
				break
			}
			$candidate--
		}
		if ($closingStart -lt 0)
		{
			$i--
			continue
		}
		$closingEnd = $closingStart + $openingLength
		for ($masked = $openingStart; $masked -lt $closingEnd; $masked++)
		{
			if ($characters[$masked] -ne "`n")
			{
				$characters[$masked] = ' '
			}
		}
		$i = $closingEnd - 1
	}
	return [string]::new($characters)
}

# Null when the slug is not gpt-<version>-<family>, such as gpt-5.5 or codex-auto-review.
function Get-AgentModelFamily([string] $Slug) {
	if ($Slug -cmatch '^gpt-(\d+(?:\.\d+)*)-([a-z][a-z0-9-]*)$') {
		return [pscustomobject] @{ Version = $Matches[1]; Family = $Matches[2] }
	}

	return $null
}

# Part by part with a missing part read as 0, so 6 equals 6.0, 6 < 6.1, and 5.10 > 5.9.
function Compare-AgentModelVersion([string] $Left, [string] $Right) {
	$leftParts = @($Left.Split('.') | ForEach-Object { [long] $_ })
	$rightParts = @($Right.Split('.') | ForEach-Object { [long] $_ })
	for ($index = 0; $index -lt [Math]::Max($leftParts.Count, $rightParts.Count); $index++) {
		$leftPart = if ($index -lt $leftParts.Count) { $leftParts[$index] } else { 0 }
		$rightPart = if ($index -lt $rightParts.Count) { $rightParts[$index] } else { 0 }
		if ($leftPart -ne $rightPart) {
			return [Math]::Sign($leftPart - $rightPart)
		}
	}

	return 0
}

Export-ModuleMember -Function Get-AgentCanonicalPath, Invoke-AgentGit, Invoke-AgentProcess, Get-AgentNormalizedText, Measure-AgentTokenCount, Test-AgentBlockOpener, Get-AgentMarkdownSearchText, Get-AgentModelFamily, Compare-AgentModelVersion
