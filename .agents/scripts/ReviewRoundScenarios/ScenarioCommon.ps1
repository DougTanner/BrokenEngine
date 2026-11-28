# Shared helpers for the review-round scenario cases. Each case file takes -ScratchRoot, an existing directory that
# holds everything the case creates, dot-sources this file, and runs from the worktree root, which each script under
# test is resolved from unless the case runs a copy in its scratch repository. A FAIL line is the red signal;
# Invoke-StaticChecks.ps1's review-round-scenarios row runs every case file.
$ErrorActionPreference = 'Stop'
$script:Wt = (Get-Location).Path
$script:Steps = Join-Path $script:Wt '.agents/scripts/Get-ChangeWorkflowSteps.ps1'
$script:InventoryCli = Join-Path $script:Wt '.agents/scripts/Get-SessionChangeInventory.ps1'
# Wildcard-escaped: the child Import-Module, which has no -LiteralPath, would match a bracketed path as a pattern.
$script:InventoryModule = [WildcardPattern]::Escape((Join-Path $script:Wt '.agents/scripts/SessionChangeInventory.psm1'))
$script:RoundsModule = [WildcardPattern]::Escape((Join-Path $script:Wt '.agents/scripts/ReviewRounds.psm1'))
if (-not (Test-Path -LiteralPath $ScratchRoot -PathType Container)) { throw "-ScratchRoot is not an existing directory: $ScratchRoot" }
$script:ScratchRoot = [IO.Path]::GetFullPath($ScratchRoot)
# The scripts under test write their temporary files under [IO.Path]::GetTempPath(), which reads TMP first on Windows.
$env:TMP = $script:ScratchRoot
$env:TEMP = $script:ScratchRoot

function New-ScratchRepo([string] $Path) {
	if (Test-Path -LiteralPath $Path) { throw "scratch path exists: $Path" }
	New-Item -ItemType Directory -Force $Path | Out-Null
	git init -q -b main $Path
	$top = (git -C $Path rev-parse --show-toplevel).Trim()
	if ([IO.Path]::GetFullPath($top) -ne [IO.Path]::GetFullPath($Path)) { throw "scratch repo $Path is not its own toplevel ($top)" }
	git -C $Path config user.name scratch
	git -C $Path config user.email scratch@example.invalid
	git -C $Path config core.autocrlf false
}

function Write-Text([string] $Path, [string] $Text) { [IO.File]::WriteAllText($Path, $Text, [Text.UTF8Encoding]::new($false)) }

# Text as a single-quoted literal for a child pwsh -Command string, so a path holding a quote stays one token.
function ConvertTo-Literal([string] $Text) { return "'$([Management.Automation.Language.CodeGeneration]::EscapeSingleQuotedStringContent($Text))'" }

function Invoke-Steps([string[]] $Arguments) {
	$out = & pwsh -NoProfile -NonInteractive -File $script:Steps @Arguments 2>&1
	$code = $LASTEXITCODE
	return [pscustomobject] @{ Code = $code; Text = (($out | ForEach-Object { "$_" }) -join "`n") }
}

function Show([string] $Title, [object] $Run) {
	"### $Title (exit $($Run.Code))"
	$Run.Text
	''
}

function Assert([bool] $Condition, [string] $Message) {
	if ($Condition) { "PASS: $Message" } else { "FAIL: $Message" }
}

function Get-RoundHead([string] $Text) {
	$m = [regex]::Match($Text, '; Head ([0-9a-f]{40})')
	if ($m.Success) { return $m.Groups[1].Value }
	return $null
}

function Get-RefTip([string] $Repo) {
	$v = git -C $Repo rev-parse --verify --quiet refs/worktree/review-rounds 2>$null
	if ($LASTEXITCODE -ne 0) { return $null }
	return "$v".Trim()
}

# A scratch repository named Name under -ScratchRoot holding Files (text) and Binary (bytes) in its baseline commit.
function New-Case([string] $Name, [hashtable] $Files, [hashtable] $Binary = @{}) {
	$repo = Join-Path $script:ScratchRoot $Name
	New-ScratchRepo $repo
	Write-Text "$repo/.gitignore" "Temp/`nProjects/Coretide/`n"
	foreach ($path in $Files.Keys) {
		$full = Join-Path $repo $path
		New-Item -ItemType Directory -Force (Split-Path $full) | Out-Null
		Write-Text $full $Files[$path]
	}
	foreach ($path in $Binary.Keys) {
		$full = Join-Path $repo $path
		New-Item -ItemType Directory -Force (Split-Path $full) | Out-Null
		[IO.File]::WriteAllBytes($full, [byte[]] $Binary[$path])
	}
	git -C $repo add -A
	git -C $repo commit -q -m baseline
	return $repo
}

function Start-Round([string] $Repo, [string] $Baseline, [int] $Tier, [string[]] $Extra = @()) {
	$r = Invoke-Steps (@('-RepositoryRoot', $Repo, '-Baseline', $Baseline, '-Tier', "$Tier", '-StartRound') + $Extra)
	return [pscustomobject] @{ Code = $r.Code; Text = $r.Text; Sha = Get-RoundHead $r.Text; Template = [regex]::Match($r.Text, "-Results '([^']*)'").Groups[1].Value }
}

function Close-Round([string] $Repo, [string] $Sha, [string] $Results) {
	return Invoke-Steps @('-RepositoryRoot', $Repo, '-CloseRound', '-Head', $Sha, '-Results', $Results)
}

function Get-ReviewRows([string] $Text) { return @($Text -split "`n" | Where-Object { $_ -match '; Head ' }) }

function Get-Owed([string] $Text) { return @(Get-ReviewRows $Text | ForEach-Object { [regex]::Match($_, ' (/[a-z-]+)').Groups[1].Value }) -join ',' }

# The -Landing inventory of the session branch against Base.
function Invoke-Landing([string] $Repo, [string] $Base, [string[]] $Extra = @()) {
	$json = Join-Path $script:ScratchRoot 'landing.json'
	# A run that writes no result must not be read as the previous call's result.
	if (Test-Path -LiteralPath $json) { Remove-Item -LiteralPath $json }
	& pwsh -NoProfile -NonInteractive -File $script:InventoryCli -RepositoryRoot $Repo -Baseline $Base -Head session -Landing -OutputPath $json @Extra 2>&1 | Out-Null
	$result = Get-Content -Raw -LiteralPath $json | ConvertFrom-Json
	"landing: status=$($result.status) code=$($result.code) message=$($result.message)" | Out-Host
	return $result
}
