# Runs checks 2-5 in order; each check's log goes to Temp/loop-scenario/check<n>-out.txt. The scratch repositories
# (repo, repo2, repo3, coretide.git, coretide3.git and their -seed clones) must not exist yet.
$here = $PSScriptRoot
foreach ($n in 2, 3, 4, 5) {
	pwsh -NoProfile -NonInteractive -File (Join-Path $here "check$n.ps1") *> (Join-Path $here "check$n-out.txt")
	"check$n exit $LASTEXITCODE"
	Get-Content (Join-Path $here "check$n-out.txt") | Where-Object { $_ -match '^(PASS|FAIL|## )|Exception|Get-ChangeWorkflowSteps:|landing:' }
}
