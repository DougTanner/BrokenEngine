# Shared caller for the TypeSafe Jev decision model: reads one request or an array of requests from a JSON
# file, posts each to the System One endpoint in parallel, and returns every answer in one result document
# on stdout or at -OutputPath. This is the only tracked script that touches the network, so every Jev use
# goes through it: a caller writes `{state, questions}` objects, never an HTTP call. The key comes from the
# TYPESAFE_API_KEY environment variable and is never printed; without it the result is `blocked`, so a
# consumer falls through to the behaviour it has without Jev instead of failing.
[CmdletBinding()]
param(
	[Parameter(Mandatory)][string] $RequestPath,
	[string] $OutputPath,
	[string] $Model = 'jev-latest',
	[int] $ThrottleLimit = 8,
	[int] $TimeoutSeconds = 120
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$script:Endpoint = 'https://api.typesafe.ai/v1/systemone'
$script:MaximumAttempts = 4
$script:Utf8 = [Text.UTF8Encoding]::new($false)

$result = [ordered]@{
	schemaVersion = 'broken-engine-jev/v1'
	status = 'error'
	code = 'internal.error'
	message = 'Jev request did not run.'
	model = $Model
	requestCount = 0
	failedCount = 0
	inputTokens = 0
	responses = @()
}

function Complete-Jev([int] $ExitCode, [string] $Status, [string] $Code, [string] $Message) {
	$result.status = $Status
	$result.code = $Code
	$result.message = $Message
	$json = $result | ConvertTo-Json -Depth 64 -Compress
	if ([string]::IsNullOrWhiteSpace($OutputPath)) {
		$stream = [Console]::OpenStandardOutput()
		$bytes = $script:Utf8.GetBytes($json)
		$stream.Write($bytes, 0, $bytes.Length)
		$stream.Flush()
	}
	else {
		[IO.File]::WriteAllText($OutputPath, $json, $script:Utf8)
		"$Status $Code $Message ($($result.requestCount) requests, $($result.failedCount) failed, $($result.inputTokens) input tokens) -> $OutputPath"
	}
	exit $ExitCode
}

$apiKey = $env:TYPESAFE_API_KEY
if ([string]::IsNullOrWhiteSpace($apiKey)) { Complete-Jev 2 'blocked' 'jev.key-missing' 'TYPESAFE_API_KEY is not set; Jev did not run.' }
if (-not (Test-Path -LiteralPath $RequestPath -PathType Leaf)) { Complete-Jev 1 'error' 'request.missing' "Request file not found: '$RequestPath'." }

try { $parsed = Get-Content -LiteralPath $RequestPath -Raw | ConvertFrom-Json -Depth 64 -AsHashtable }
catch { Complete-Jev 1 'error' 'request.invalid-json' "Request file is not valid JSON: $($_.Exception.Message)" }
$requests = @(if ($parsed -is [array]) { $parsed } else { @($parsed) })
if ($requests.Count -eq 0) { Complete-Jev 1 'error' 'request.empty' 'Request file holds no requests.' }
for ($i = 0; $i -lt $requests.Count; $i++) {
	$request = $requests[$i]
	if (-not ($request -is [hashtable]) -or -not $request.ContainsKey('state') -or -not $request.ContainsKey('questions')) { Complete-Jev 1 'error' 'request.invalid' "Request $i needs 'state' and 'questions'." }
	if (-not $request.ContainsKey('model')) { $request['model'] = $Model }
}
$result.requestCount = $requests.Count

$indexed = @(for ($i = 0; $i -lt $requests.Count; $i++) { @{ index = $i; body = ($requests[$i] | ConvertTo-Json -Depth 64 -Compress) } })
$responses = $indexed | ForEach-Object -ThrottleLimit $ThrottleLimit -Parallel {
	$item = $_
	$headers = @{ Authorization = "Bearer $using:apiKey" }
	# A custom object, not a hashtable: Sort-Object below orders by a property, and a hashtable key is not one.
	$response = [pscustomobject]@{ index = $item.index; answers = $null; usage = $null; error = $null }
	for ($attempt = 1; $attempt -le $using:script:MaximumAttempts; $attempt++) {
		try {
			$raw = Invoke-RestMethod -Method Post -Uri $using:script:Endpoint -Headers $headers -ContentType 'application/json' -Body $item.body -TimeoutSec $using:TimeoutSeconds
			$response.answers = $raw.answers
			$response.usage = $raw.usage
			$response.error = $null
			break
		}
		catch {
			$status = 0
			if ($null -ne $_.Exception.PSObject.Properties['Response'] -and $null -ne $_.Exception.Response) { $status = [int]$_.Exception.Response.StatusCode }
			$response.error = "HTTP $status $($_.Exception.Message)"
			# Only rate limiting and overload are worth a retry; the API documents both as transient.
			if ($status -ne 429 -and $status -ne 529) { break }
			Start-Sleep -Milliseconds (500 * [Math]::Pow(2, $attempt))
		}
	}
	$response
}

$ordered = @($responses | Sort-Object -Property index)
for ($i = 0; $i -lt $ordered.Count; $i++) { if ($ordered[$i].index -ne $i) { Complete-Jev 1 'error' 'internal.order' "Response order broke at position $i." } }
$result.responses = $ordered
$result.failedCount = @($ordered | Where-Object { $null -ne $_.error }).Count
$result.inputTokens = [int64](($ordered | Where-Object { $null -ne $_.usage } | ForEach-Object { [int64]$_.usage.input_tokens } | Measure-Object -Sum).Sum)
if ($result.failedCount -eq $result.requestCount) { Complete-Jev 2 'blocked' 'jev.unavailable' "Every request failed; first error: $($ordered[0].error)" }
if ($result.failedCount -gt 0) { Complete-Jev 1 'error' 'jev.partial' "$($result.failedCount) of $($result.requestCount) requests failed; each failed response carries its error." }
Complete-Jev 0 'ok' 'jev.answered' 'Every request answered.'
