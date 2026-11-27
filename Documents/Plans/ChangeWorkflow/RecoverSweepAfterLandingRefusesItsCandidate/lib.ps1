# Shared helpers for the scratch review-round scenarios. Dot-source from the worktree root.
$ErrorActionPreference = 'Stop'
$script:Wt = (Get-Location).Path
$script:Steps = '.agents/scripts/Get-ChangeWorkflowSteps.ps1'

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
