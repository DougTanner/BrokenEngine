[CmdletBinding()]
param([ValidateRange(1,500)][int]$Top=5)
$ErrorActionPreference='Stop';Set-StrictMode -Version Latest
$result=[ordered]@{schemaVersion='broken-engine-next-plan-list-error/v1';status='error';code='internal.error';message='Listing did not run.'}
function Complete-Listing([int]$ExitCode,[string]$Status,[string]$Code,[string]$Message){$result.status=$Status;$result.code=$Code;$result.message=$Message;[Console]::Out.Write(($result|ConvertTo-Json -Depth 20 -Compress));exit $ExitCode}
try {
	$sharedScripts=Join-Path $PSScriptRoot '..\..\..\scripts';if(-not (Test-Path -LiteralPath (Join-Path $sharedScripts 'AgentScriptCommon.psm1'))){$sharedScripts=Join-Path $PSScriptRoot '..\..\..\..\.agents\scripts'}
	Import-Module (Join-Path $sharedScripts 'AgentScriptCommon.psm1') -Force -DisableNameChecking
	Import-Module (Join-Path $PSScriptRoot 'NextPlanWorkflowCommon.psm1') -Force -DisableNameChecking
	$context=Get-NextPlanContext
	# plan list compares the session Coretide clone's HEAD with the primary Coretide HEAD inside the clone, so the clone
	# is fetched first; a primary commit it has not fetched would otherwise read as unrelated rather than behind.
	$coretide=Get-AgentCoretideRepository $context.Worktree
	if($coretide){$fetch=Invoke-NextPlanProcess 'git.exe' @('-C',$coretide,'fetch') $coretide;if($fetch.ExitCode -ne 0){throw "git fetch failed in the session Coretide clone. $($fetch.Stderr.Trim())"}}
	$response=Invoke-NextPlanProcess $context.WorktreeCli @('plan','list','--repo',$context.CommonDirectory,'--worktree',$context.Worktree) $context.Worktree
	# WorktreeCli owns the listing contract; preserve its failure code. A successful listing is folded
	# into a bounded projection — state counts plus the first -Top rows in selection order, each row
	# carrying only what a selection decision reads — so a large Plan tree never floods the caller.
	$listing=ConvertFrom-NextPlanProcessJson $response 'plan list'
	if($response.ExitCode -ne 0){$downstreamCode=if($listing.PSObject.Properties.Name -ccontains 'code'){[string]$listing.code}else{'unknown'};Complete-Listing $(if($response.ExitCode -eq 2){2}else{1}) $(if($response.ExitCode -eq 2){'blocked'}else{'error'}) $downstreamCode "WorktreeCli plan list failed: $downstreamCode."}
	$rows=@(if($listing.PSObject.Properties.Name -ccontains 'plans' -and $null -ne $listing.plans){$listing.plans}else{@()})
	$stateCounts=[ordered]@{}
	foreach($row in $rows){$state=[string]$row.state;if(-not $stateCounts.Contains($state)){$stateCounts[$state]=0};$stateCounts[$state]++}
	$visibleRows=@($rows|Where-Object{-not ($_.PSObject.Properties.Name -ccontains 'agentCliMismatch' -and -not [string]::IsNullOrWhiteSpace([string]$_.agentCliMismatch))})
	$plans=@(foreach($row in @($visibleRows|Select-Object -First $Top)){$plan=[ordered]@{path=[string]$row.path;state=[string]$row.state};if($row.PSObject.Properties.Name -ccontains 'agentCli' -and -not [string]::IsNullOrWhiteSpace([string]$row.agentCli)){$plan.agentCli=[string]$row.agentCli};if($row.PSObject.Properties.Name -ccontains 'blockedBy'){$plan.blockedBy=@($row.blockedBy)};if($row.PSObject.Properties.Name -ccontains 'diagnostic'){$plan.diagnostic=[string]$row.diagnostic};if($row.PSObject.Properties.Name -ccontains 'claim'){$plan.claim=$row.claim};$plan})
	$projection=[ordered]@{
		schemaVersion='broken-engine-next-plan-list/v2'
		status='pass'
		code=$(if($listing.PSObject.Properties.Name -ccontains 'code'){[string]$listing.code}else{'ok'})
		message="First $($plans.Count) of $($visibleRows.Count) Plans in selection order."
		stateCounts=$stateCounts
		plans=$plans
		truncated=($visibleRows.Count -gt $Top)
		diagnostics=@(if($listing.PSObject.Properties.Name -ccontains 'diagnostics' -and $null -ne $listing.diagnostics){$listing.diagnostics}else{@()})
		notices=@(if($listing.PSObject.Properties.Name -ccontains 'notices' -and $null -ne $listing.notices){$listing.notices}else{@()})
	}
	[Console]::Out.Write(($projection|ConvertTo-Json -Depth 20 -Compress))
	exit 0
} catch {if(Get-Command Test-NextPlanStateBlocker -ErrorAction SilentlyContinue){if(Test-NextPlanStateBlocker $_){Complete-Listing 2 'blocked' 'list.context-conflict' $_.Exception.Message}};Complete-Listing 1 'error' 'list.failed' $_.Exception.Message}
