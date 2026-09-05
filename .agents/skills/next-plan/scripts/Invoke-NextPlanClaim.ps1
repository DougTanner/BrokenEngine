[CmdletBinding()]
param([string] $Plan,[switch] $ResumeRetained)
$ErrorActionPreference='Stop'; Set-StrictMode -Version Latest
$result=[ordered]@{schemaVersion='broken-engine-next-plan-claim-result/v5';status='error';code='internal.error';message='Claim did not run.';nextAction='stop-report-to-user';claim=$null}
function Complete-Claim([int]$ExitCode,[string]$Status,[string]$Code,[string]$Message,[string]$NextAction){$result.status=$Status;$result.code=$Code;$result.message=$Message;$result.nextAction=$NextAction;[Console]::Out.Write(($result|ConvertTo-Json -Depth 100 -Compress));exit $ExitCode}
# NUL-delimited porcelain v1 emits raw paths, so a path containing a space or a quotable character is never C-quoted;
# a rename or copy record is followed by one extra field holding the original path, and both sides matter because both
# are worktree paths a fast-forward could touch.
function Get-DirtyPath([string]$Porcelain){$paths=[Collections.Generic.List[string]]::new();$fields=@($Porcelain -split "`0");$index=0;while($index -lt $fields.Count){$record=$fields[$index];$index++;if($record.Length -lt 4){continue};$paths.Add($record.Substring(3));$state=$record.Substring(0,2);if($state.Contains('R') -or $state.Contains('C')){if($index -lt $fields.Count){$paths.Add($fields[$index]);$index++}}};return $paths}
function Format-DirtyPath([string[]]$Paths){$text=@($Paths|Select-Object -First 10) -join ', ';if($Paths.Count -gt 10){$text+=" (+$($Paths.Count-10) more)"};return $text}
function Get-TargetedNoneMessage($Context,[string]$RequestedPlan){
	$fallback="The requested Plan '$RequestedPlan' could not be classified after the claim attempt; retry the targeted claim."
	try {
		$response=Invoke-NextPlanProcess $Context.WorktreeCli @('plan','list','--repo',$Context.CommonDirectory,'--worktree',$Context.Worktree) $Context.Worktree
		$listing=ConvertFrom-NextPlanProcessJson $response 'plan list'
		if($response.ExitCode -ne 0){return $fallback}
		if($listing.PSObject.Properties.Name -cnotcontains 'plans' -or $null -eq $listing.plans -or $listing.PSObject.Properties.Name -cnotcontains 'diagnostics' -or $null -eq $listing.diagnostics){return $fallback}
		$rows=@($listing.plans)
		$matches=@($rows|Where-Object{[string]$_.path -ceq $RequestedPlan})
		if($matches.Count -eq 1){
			$row=$matches[0];$state=[string]$row.state
			if($state -ceq 'blocked' -and $row.PSObject.Properties.Name -ccontains 'blockedBy'){$blockers=[Collections.Generic.List[string]]::new();foreach($path in @($row.blockedBy)){$blockers.Add([string]$path)};if($blockers.Count -eq 0){return $fallback};$blockers.Sort([StringComparer]::Ordinal);return "The requested Plan '$RequestedPlan' is blocked by prerequisite Plans: $(Format-DirtyPath $blockers.ToArray())."}
			if($state -ceq 'excluded' -and $row.PSObject.Properties.Name -ccontains 'diagnostic' -and -not [string]::IsNullOrWhiteSpace([string]$row.diagnostic)){return "The requested Plan '$RequestedPlan' is excluded from selection: $([string]$row.diagnostic)."}
			if($state -ceq 'claimed'){return "The requested Plan '$RequestedPlan' is claimed by another session."}
			if($state -ceq 'eligible'){return "A later scheduler listing currently reports the requested Plan '$RequestedPlan' as eligible; retry the targeted claim."}
			return $fallback
		}
		if($matches.Count -gt 1){return $fallback}
		$diagnostics=@($listing.diagnostics)
		$matchingDiagnostics=@($diagnostics|Where-Object{$_.PSObject.Properties.Name -ccontains 'plan' -and [string]$_.plan -ceq $RequestedPlan})
		if($matchingDiagnostics.Count -eq 1 -and $matchingDiagnostics[0].PSObject.Properties.Name -ccontains 'message' -and -not [string]::IsNullOrWhiteSpace([string]$matchingDiagnostics[0].message)){return "The requested Plan '$RequestedPlan' is excluded from selection: $([string]$matchingDiagnostics[0].message)."}
		if($matchingDiagnostics.Count -gt 0){return $fallback}
		return "The requested Plan '$RequestedPlan' is absent from the session tree."
	} catch {return $fallback}
}
try {
 Import-Module (Join-Path $PSScriptRoot 'NextPlanWorkflowCommon.psm1') -Force -DisableNameChecking
 $targeted=-not [string]::IsNullOrWhiteSpace($Plan)
 if($ResumeRetained -and -not $targeted){Complete-Claim 2 'blocked' 'claim.usage-error' '-ResumeRetained is valid only with -Plan, because only a named Plan can carry work retained by an earlier deferral; no Plan was claimed and the worktree was not touched.' 'stop-report-to-user'}
 $pattern=$null
 if ($Plan) {
  $Plan=$Plan.Replace('\','/')
  Assert-NextPlanGitPath $Plan
  if ($Plan.StartsWith('Documents/Plans/',[StringComparison]::Ordinal)) {
   # WorktreeCli owns canonical path validation; preserve its exact-path contract.
  } else {
   $pattern=$Plan
   $Plan=$null
  }
 }
 $context=Get-NextPlanContext
 $status=Invoke-NextPlanProcess 'git.exe' @('-C',$context.Worktree,'status','--porcelain=v1','-z','--untracked-files=all') $context.Worktree
 if($status.ExitCode -ne 0){throw (New-NextPlanStateBlocker "git status could not read the session worktree. $($status.Stderr.Trim())")}
 $dirty=@(Get-DirtyPath $status.Stdout)
 if($dirty.Count -ne 0){
  if(-not $ResumeRetained){$route=if($targeted){' To resume uncommitted work retained by an earlier deferral of this Plan, rerun this command with -ResumeRetained.'}else{''};Complete-Claim 2 'blocked' 'claim.worktree-dirty' "Session worktree must be clean before a plan claim; $($dirty.Count) path(s) are modified or untracked: $(Format-DirtyPath $dirty). No Plan was claimed and the worktree was not touched.$route" $(if($targeted){'resume-with-flag'}else{'stop-report-to-user'})}
  # Selection and validation read Documents/Plans from this tree, so uncommitted scheduler input is never safe to claim against.
  $schedulerPaths=@($dirty|Where-Object{$_.StartsWith('Documents/Plans/',[StringComparison]::Ordinal)})
  if($schedulerPaths.Count -ne 0){Complete-Claim 2 'blocked' 'claim.worktree-dirty' "-ResumeRetained cannot claim while scheduler input is uncommitted; $($schedulerPaths.Count) path(s) under Documents/Plans/ are modified or untracked: $(Format-DirtyPath $schedulerPaths). No Plan was claimed and the worktree was not touched." 'stop-report-to-user'}
  if($context.SessionHead -cne $context.PrimaryTip){
   # The claim below fast-forwards the session, which would overwrite a retained path the primary movement also changed.
   # NUL-delimited output keeps both sides of the comparison in the same raw path form.
   $incoming=Invoke-NextPlanProcess 'git.exe' @('-C',$context.Worktree,'diff','--name-only','-z',$context.SessionHead,$context.PrimaryTip) $context.Worktree
   if($incoming.ExitCode -ne 0){throw (New-NextPlanStateBlocker "git diff --name-only could not compare the session head with the primary tip. $($incoming.Stderr.Trim())")}
   $incomingPaths=@(($incoming.Stdout -split "`0")|Where-Object{-not [string]::IsNullOrEmpty($_)})
   $overlap=@($dirty|Where-Object{$incomingPaths -ccontains $_})
   if($overlap.Count -ne 0){Complete-Claim 2 'blocked' 'claim.worktree-dirty' "-ResumeRetained cannot claim because fast-forwarding the session from $($context.SessionHead) to the primary tip $($context.PrimaryTip) would touch $($overlap.Count) retained path(s): $(Format-DirtyPath $overlap). No Plan was claimed and the worktree was not touched; commit or land that work first." 'stop-report-to-user'}
  }
  $result.retained=[ordered]@{count=$dirty.Count;truncated=($dirty.Count -gt 10);paths=@($dirty|Select-Object -First 10)}
 }
 # The executable-Plan inventory below is read from the session tree, so a session behind primary hides
 # Plans that already exist at the primary tip; fast-forward first so the inventory matches primary.
 if($context.SessionHead -cne $context.PrimaryTip){
  # WorktreeCli's claim path requires session HEAD to be an ancestor of the primary tip, so a session
  # holding any commit the primary tip lacks stops here instead of reaching a confusing claim rejection.
  $behind=Invoke-NextPlanProcess 'git.exe' @('-C',$context.Worktree,'merge-base','--is-ancestor',$context.SessionHead,$context.PrimaryTip) $context.Worktree
  if($behind.ExitCode -gt 1){throw "git merge-base --is-ancestor failed. $($behind.Stderr.Trim())"}
  if($behind.ExitCode -ne 0){
   $revList=Invoke-NextPlanProcess 'git.exe' @('-C',$context.Worktree,'rev-list',"$($context.PrimaryTip)..$($context.SessionHead)") $context.Worktree
   if($revList.ExitCode -ne 0){throw "git rev-list $($context.PrimaryTip)..$($context.SessionHead) failed. $($revList.Stderr.Trim())"}
   $held=@(($revList.Stdout -split "`n")|ForEach-Object{$_.Trim()}|Where-Object{$_.Length -gt 0})
   Complete-Claim 2 'blocked' 'claim.session-diverged' "Session HEAD $($context.SessionHead) holds $($held.Count) commit(s) the primary tip $($context.PrimaryTip) does not ($($held -join ', ')), so the session cannot be fast-forwarded; no Plan was claimed and the session branch was not moved. Report this to the user, who decides how to resolve it." 'stop-report-to-user'
  }
  $merge=Invoke-NextPlanProcess 'git.exe' @('-C',$context.Worktree,'merge','--ff-only',$context.PrimaryTip) $context.Worktree
  if($merge.ExitCode -ne 0){$mergeDetail=(($merge.Stdout,$merge.Stderr) -join "`n").Trim();Complete-Claim 1 'error' 'claim.session-sync-failed' "Fast-forwarding the session branch from $($context.SessionHead) to the primary tip $($context.PrimaryTip) failed; no Plan was claimed, and the worktree may be partially updated, so inspect it before retrying. git reported: $mergeDetail" 'stop-report-to-user'}
  $result.sync=[ordered]@{fastForwarded=$true;from=$context.SessionHead;to=$context.PrimaryTip}
 }
 $validate=Invoke-NextPlanProcess $context.WorktreeCli @('plan','validate','--repo',$context.CommonDirectory,'--worktree',$context.Worktree) $context.Worktree
 $validation=ConvertFrom-NextPlanProcessJson $validate 'plan validate'; $projection=[ordered]@{}; foreach($name in @('status','code','message','diagnostics','notices','healedClaims')){if($validation.PSObject.Properties.Name -ccontains $name){$projection[$name]=$validation.$name}}; $result.validation=$projection
 if($validate.ExitCode -eq 2 -and [string]$projection['code'] -ceq 'busy'){Complete-Claim 2 'blocked' 'scheduler.busy' 'Another session held the plan scheduler for the full wait; no Plan was claimed. Tell the user and retry later.' 'retry-later'}
 if($validate.ExitCode -eq 1 -and [string]$projection['code'] -ceq 'guard-unavailable'){Complete-Claim 1 'error' 'scheduler.guard-unavailable' 'The plan scheduler lock storage is unusable; no Plan was claimed. Tell the user - this is not a Plan metadata problem.' 'stop-report-to-user'}
 if($validate.ExitCode -ne 0){$exit=if($validate.ExitCode -eq 2){2}else{1};Complete-Claim $exit $(if($exit -eq 2){'blocked'}else{'error'}) 'plan.validation-failed' 'Plan validation failed.' 'stop-report-to-user'}
 if($null -ne $pattern){
  $candidates=[Collections.Generic.List[string]]::new()
  foreach($executablePlan in @($validation.plans)){
   if($executablePlan.PSObject.Properties.Name -ccontains 'path'){
    $candidate=[string]$executablePlan.path
    # Match below Documents/Plans/ so a pattern naming that prefix cannot match every Plan.
    $relative=if($candidate.StartsWith('Documents/Plans/',[StringComparison]::Ordinal)){$candidate.Substring('Documents/Plans/'.Length)}else{$candidate}
    if($relative.Contains($pattern,[StringComparison]::Ordinal)){$candidates.Add($candidate)}
   }
  }
  $candidates.Sort([StringComparer]::Ordinal)
  if($candidates.Count -eq 0){Complete-Claim 2 'blocked' 'plan-name-not-found' "Pattern '$pattern' matched no validated executable Plan." 'stop-report-to-user'}
  if($candidates.Count -gt 1){$result.candidates=@($candidates);Complete-Claim 2 'blocked' 'plan-name-ambiguous' "Pattern '$pattern' matched multiple validated executable Plans; use a canonical Documents/Plans path." 'stop-report-to-user'}
  $Plan=$candidates[0]
 }
 $claimArguments=@('plan','claim-next','--repo',$context.CommonDirectory,'--primary-worktree',$context.Primary,'--worktree',$context.Worktree,'--branch',$context.SessionBranch,'--owner',$context.Owner,'--session',$context.Session);if($Plan){$claimArguments+=@('--plan',$Plan)}
 $response=Invoke-NextPlanProcess $context.WorktreeCli $claimArguments $context.Worktree;$claim=ConvertFrom-NextPlanProcessJson $response 'plan claim-next'
 if($response.ExitCode -ne 0){$exit=if($response.ExitCode -eq 2){2}else{1};Complete-Claim $exit $(if($exit -eq 2){'blocked'}else{'error'}) 'claim.rejected' 'WorktreeCli rejected the plan claim.' 'stop-report-to-user'}
 $code=[string]$claim.code
 if($code -ceq 'none'){$message=if($targeted){Get-TargetedNoneMessage $context $Plan}else{'No eligible Plan is available.'};Complete-Claim 0 'pass' 'none-available' $message 'stop-report-to-user'}
 if($code -cne 'claimed' -and $code -cne 'existing'){throw "plan claim-next returned an unknown result code '$code'."}
 $result.claim=[ordered]@{claimed=$true;plan=[string]$claim.plan;state=$code}
 if($code -ceq 'existing'){Complete-Claim 0 'pass' 'reused' 'Existing Plan claim remains live for this session.' 'prepare'}
 Complete-Claim 0 'pass' 'ok' 'Plan claimed for this session.' 'prepare'
} catch {if(Get-Command Test-NextPlanStateBlocker -ErrorAction SilentlyContinue){if(Test-NextPlanStateBlocker $_){Complete-Claim 2 'blocked' 'claim.context-conflict' $_.Exception.Message 'stop-report-to-user'}};Complete-Claim 1 'error' 'claim.failed' $_.Exception.Message 'stop-report-to-user'}
