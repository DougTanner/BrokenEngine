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

Export-ModuleMember -Function Get-AgentCanonicalPath, Invoke-AgentGit, Invoke-AgentProcess, Get-AgentNormalizedText, Measure-AgentTokenCount
