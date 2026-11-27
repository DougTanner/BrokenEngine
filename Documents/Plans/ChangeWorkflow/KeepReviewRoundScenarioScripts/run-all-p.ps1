# run-all.ps1 against another tree: copies lib.ps1 and check2-5 into Temp/loop-scenario/ra-<Tag>/ with every
# .agents/scripts/ path pointed at -Scripts and the landing JSON kept inside that directory, then runs them there so
# their scratch repositories are fresh. Run from the worktree root.
param([Parameter(Mandatory)][string] $Scripts, [Parameter(Mandatory)][string] $Tag)
$Scripts = [IO.Path]::GetFullPath($Scripts) -replace '\\', '/'
$dir = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "ra-$Tag"))
if (Test-Path -LiteralPath $dir) { throw "scratch path exists: $dir" }
New-Item -ItemType Directory -Force $dir | Out-Null
foreach ($name in 'lib.ps1', 'check2.ps1', 'check3.ps1', 'check4.ps1', 'check5.ps1') {
	$text = [IO.File]::ReadAllText((Join-Path $PSScriptRoot $name))
	$text = $text.Replace('./.agents/scripts/', "$Scripts/.agents/scripts/").Replace(' .agents/scripts/', " $Scripts/.agents/scripts/").Replace("'.agents/scripts/", "'$Scripts/.agents/scripts/")
	$text = $text.Replace("'Temp/loop-scenario/landing.json'", "'$($dir -replace '\\', '/')/landing.json'")
	[IO.File]::WriteAllText((Join-Path $dir $name), $text, [Text.UTF8Encoding]::new($false))
}
foreach ($n in 2, 3, 4, 5) {
	pwsh -NoProfile -NonInteractive -File (Join-Path $dir "check$n.ps1") *> (Join-Path $dir "check$n-out.txt")
	"check$n exit $LASTEXITCODE"
	Get-Content (Join-Path $dir "check$n-out.txt") | Where-Object { $_ -match '^(PASS|FAIL|## )|Exception|Get-ChangeWorkflowSteps:|landing:' }
}
