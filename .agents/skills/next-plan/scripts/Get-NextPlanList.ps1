[CmdletBinding()]
param([ValidateRange(1,500)][int]$Top=5)
$ErrorActionPreference='Stop';Set-StrictMode -Version Latest
$result=[ordered]@{schemaVersion='broken-engine-next-plan-list-error/v1';status='error';code='internal.error';message='Listing did not run.'}
function Complete-Listing([int]$ExitCode,[string]$Status,[string]$Code,[string]$Message){$result.status=$Status;$result.code=$Code;$result.message=$Message;[Console]::Out.Write(($result|ConvertTo-Json -Depth 20 -Compress));exit $ExitCode}
try {
	Import-Module (Join-Path $PSScriptRoot 'NextPlanWorkflowCommon.psm1') -Force -DisableNameChecking
	$context=Get-NextPlanContext
	$response=Invoke-NextPlanProcess $context.WorktreeCli @('plan','list','--repo',$context.CommonDirectory,'--worktree',$context.Worktree) $context.Worktree
	# WorktreeCli owns the listing contract; preserve its failure code. A successful listing is folded
	# into a bounded projection — state counts plus the first -Top rows in selection order, each row
	# carrying only what a selection decision reads — so a large Plan tree never floods the caller.
	$listing=ConvertFrom-NextPlanProcessJson $response 'plan list'
	if($response.ExitCode -ne 0){$downstreamCode=if($listing.PSObject.Properties.Name -ccontains 'code'){[string]$listing.code}else{'unknown'};Complete-Listing $(if($response.ExitCode -eq 2){2}else{1}) $(if($response.ExitCode -eq 2){'blocked'}else{'error'}) $downstreamCode "WorktreeCli plan list failed: $downstreamCode."}
	$rows=@(if($listing.PSObject.Properties.Name -ccontains 'plans' -and $null -ne $listing.plans){$listing.plans}else{@()})
	$stateCounts=[ordered]@{}
	foreach($row in $rows){$state=[string]$row.state;if(-not $stateCounts.Contains($state)){$stateCounts[$state]=0};$stateCounts[$state]++}
	$plans=@(foreach($row in @($rows|Select-Object -First $Top)){$plan=[ordered]@{path=[string]$row.path;state=[string]$row.state};if($row.PSObject.Properties.Name -ccontains 'blockedBy'){$plan.blockedBy=@($row.blockedBy)};if($row.PSObject.Properties.Name -ccontains 'diagnostic'){$plan.diagnostic=[string]$row.diagnostic};$plan})
	$projection=[ordered]@{
		schemaVersion='broken-engine-next-plan-list/v1'
		status='pass'
		code=$(if($listing.PSObject.Properties.Name -ccontains 'code'){[string]$listing.code}else{'ok'})
		message="First $($plans.Count) of $($rows.Count) Plans in selection order."
		stateCounts=$stateCounts
		plans=$plans
		truncated=($rows.Count -gt $Top)
		diagnostics=@(if($listing.PSObject.Properties.Name -ccontains 'diagnostics' -and $null -ne $listing.diagnostics){$listing.diagnostics}else{@()})
		notices=@(if($listing.PSObject.Properties.Name -ccontains 'notices' -and $null -ne $listing.notices){$listing.notices}else{@()})
	}
	[Console]::Out.Write(($projection|ConvertTo-Json -Depth 20 -Compress))
	exit 0
} catch {if(Get-Command Test-NextPlanStateBlocker -ErrorAction SilentlyContinue){if(Test-NextPlanStateBlocker $_){Complete-Listing 2 'blocked' 'list.context-conflict' $_.Exception.Message}};Complete-Listing 1 'error' 'list.failed' $_.Exception.Message}
