# Reads the text an `xperf -i <etl> -o <txt> -a dumper` run writes. Each body row is: event name, timestamp in
# microseconds from trace start, "Process Name ( PID)", thread ID, CPU, then provider columns; a manifest event
# carries four etw: columns before its payload. The header lists every event layout, and payload positions are
# resolved from it rather than hard-coded. One event name can appear with several layouts (QueuePacket/win:Start
# does); the first layout wins, so only read fields that every layout of that name places alike.

function Resolve-TraceDumpPath([string] $Path) {
	(Resolve-Path -LiteralPath $Path).ProviderPath
}

function Read-TraceDumpHeader([string] $Path) {
	$layouts = @{}
	$reader = [System.IO.StreamReader]::new((Resolve-TraceDumpPath $Path))
	try {
		while ($null -ne ($line = $reader.ReadLine()) -and $line -ne 'EndHeader') {
			if ($line -eq 'BeginHeader') { continue }
			$columns = @($line.Split(',') | ForEach-Object { $_.Trim() })
			if (-not $layouts.ContainsKey($columns[0])) { $layouts[$columns[0]] = $columns }
		}
	}
	finally {
		$reader.Dispose()
	}
	$layouts
}

# Returns -1 when the header has no such event or field, so a caller decides whether the field is required.
function Get-TraceFieldIndex($Header, [string] $EventName, [string] $FieldName) {
	$columns = $Header[$EventName]
	if ($null -eq $columns) { return -1 }
	[Array]::IndexOf([string[]] $columns, $FieldName)
}

# Streams body rows inside [From, To]; the dumper writes rows in timestamp order, so reading stops past To. The
# finally block also runs when a downstream Select-Object -First stops the pipeline, which releases the file.
function Get-TraceDumpRows([string] $Path, [long] $From = 0, [long] $To = [long]::MaxValue) {
	$reader = [System.IO.StreamReader]::new((Resolve-TraceDumpPath $Path))
	try {
		$inBody = $false
		while ($null -ne ($line = $reader.ReadLine())) {
			if (-not $inBody) { $inBody = $line -eq 'EndHeader'; continue }
			$columns = $line.Split(',')
			if ($columns.Count -lt 5) { continue }
			$time = 0L
			if (-not [long]::TryParse($columns[1].Trim(), [ref] $time)) { continue }
			if ($time -lt $From) { continue }
			if ($time -gt $To) { break }
			$processId = if ($columns[2] -match '\(\s*(\d+)\)\s*$') { [int] $Matches[1] } else { -1 }
			[pscustomobject]@{
				Name = $columns[0].Trim()
				Time = $time
				ProcessId = $processId
				ThreadId = $columns[3].Trim()
				Columns = $columns
			}
		}
	}
	finally {
		$reader.Dispose()
	}
}

# Trace time zero as a QueryPerformanceCounter value, from a VSyncDPC row's FrameQPCTime; xperf's default clock
# type is PerfCounter, so trace timestamps and FrameQPCTime share one clock.
function Get-TraceOriginQpc($Row, [int] $FrameQpcIndex, [long] $QpcFrequency) {
	$frameQpc = [long] $Row.Columns[$FrameQpcIndex].Trim()
	$frameQpc - [long] ([decimal] $Row.Time * $QpcFrequency / 1000000)
}

Export-ModuleMember -Function Read-TraceDumpHeader, Get-TraceFieldIndex, Get-TraceDumpRows, Get-TraceOriginQpc
