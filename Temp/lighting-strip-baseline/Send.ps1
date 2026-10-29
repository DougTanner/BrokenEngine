param(
	[Parameter(Mandatory)][int]$Port,
	[Parameter(Mandatory)][string]$Json,
	[int]$TimeoutMs = 15000,
	[string]$OutFile = ''
)
$Harness = 'C:\Users\dougt\.claude\worktrees\BrokenEngine\0cb8b7b1-9695-4b87-ab78-8bf06eceacad\Tools\AgentHarness\Platforms\VisualStudio2026\Output\AgentHarness.exe'
$Owner = '97ca3cb8-2b56-42a8-acc7-6e0009465041'
$Response = $Json | & $Harness --owner $Owner --port $Port --timeout-ms $TimeoutMs -
$Code = $LASTEXITCODE
if ($OutFile) {
	$Response | Set-Content -LiteralPath $OutFile -Encoding utf8
	"exit=$Code bytes=$(($Response | Out-String).Length) -> $OutFile"
} else {
	"exit=$Code"
	$Response
}
