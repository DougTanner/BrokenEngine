[CmdletBinding()]
param([string] $Plan,[switch] $ResumeRetained,[switch] $UserAuthorizedTakeover,[switch] $AllowAgentCliMismatch)
$ErrorActionPreference='Stop'; Set-StrictMode -Version Latest
$result=[ordered]@{schemaVersion='broken-engine-next-plan-claim-result/v11';status='error';code='internal.error';message='Claim did not run.';nextAction='stop-report-to-user';claim=$null}
function Complete-Claim([int]$ExitCode,[string]$Status,[string]$Code,[string]$Message,[string]$NextAction){$result.status=$Status;$result.code=$Code;$result.message=$Message;if($null -ne $result.claim -and $Status -cne 'pass'){$result.message+=" This session's claim on '$($result.claim.plan)' remains held."};$result.nextAction=$NextAction;[Console]::Out.Write(($result|ConvertTo-Json -Depth 100 -Compress));exit $ExitCode}
# NUL-delimited porcelain v1 emits raw paths, so a path containing a space or a quotable character is never C-quoted;
# a rename or copy record is followed by one extra field holding the original path, and both sides matter because both
# are worktree paths a fast-forward could touch.
function Get-DirtyPath([string]$Porcelain){$paths=[Collections.Generic.List[string]]::new();$fields=@($Porcelain -split "`0");$index=0;while($index -lt $fields.Count){$record=$fields[$index];$index++;if($record.Length -lt 4){continue};$paths.Add($record.Substring(3));$state=$record.Substring(0,2);if($state.Contains('R') -or $state.Contains('C')){if($index -lt $fields.Count){$paths.Add($fields[$index]);$index++}}};return $paths}
function Format-DirtyPath([string[]]$Paths){$text=@($Paths|Select-Object -First 10) -join ', ';if($Paths.Count -gt 10){$text+=" (+$($Paths.Count-10) more)"};return $text}
function Test-CanonicalPlanPath([string]$Path){if([string]::IsNullOrEmpty($Path) -or $Path.IndexOf("`0",[StringComparison]::Ordinal) -ge 0 -or $Path.IndexOf("`r",[StringComparison]::Ordinal) -ge 0 -or $Path.IndexOf("`n",[StringComparison]::Ordinal) -ge 0){return $false};$relative=$null;foreach($plansRoot in @('Documents/Plans/','Projects/Coretide/Documents/Plans/')){if($Path.StartsWith($plansRoot,[StringComparison]::Ordinal)){$relative=$Path.Substring($plansRoot.Length);break}};if($null -eq $relative){return $false};$segments=@($relative.Split('/'));if($segments.Count -ne 2){return $false};$area=$segments[0];$name=$segments[1];if([string]::IsNullOrWhiteSpace($area) -or $area -ceq '.' -or $area -ceq '..'){return $false};if(-not $name.EndsWith('.md',[StringComparison]::Ordinal) -or $name.Length -le 3 -or $name -ceq 'AGENTS.md' -or [string]::IsNullOrWhiteSpace($name.Substring(0,$name.Length-3))){return $false};return $true}
function Get-PlanSectionMetadata([string]$WorktreeRoot,[string]$PlanPath){
	$fullPath=Join-Path $WorktreeRoot $PlanPath
	if(-not [IO.File]::Exists($fullPath)){return [ordered]@{lineCount=0;sections=[object[]]@()}}
	$lines=[IO.File]::ReadAllLines($fullPath,[Text.Encoding]::UTF8)
	$sections=[Collections.Generic.List[object]]::new()
	if($lines.Length -gt 0){
		$searchLines=(Get-AgentMarkdownSearchText -Body ($lines -join "`n") -KeepInlineCode).Split("`n")
		for($i=0;$i -lt $lines.Length;$i++){
			if($searchLines[$i] -match '^##\s+'){
				if($sections.Count -gt 0){$sections[$sections.Count-1].endLine=$i}
				$sections.Add([ordered]@{heading=$lines[$i].TrimEnd();startLine=$i+1;endLine=$lines.Length})
			}
		}
	}
	return [ordered]@{lineCount=$lines.Length;sections=[object[]]@($sections)}
}
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
			if($state -ceq 'excluded' -and $row.PSObject.Properties.Name -ccontains 'agentCliMismatch' -and -not [string]::IsNullOrWhiteSpace([string]$row.agentCliMismatch) -and $row.PSObject.Properties.Name -ccontains 'diagnostic' -and [string]$row.diagnostic -ceq "locked to agent CLI '$([string]$row.agentCliMismatch)'"){
				# claim-next already handled any CLI-lock refusal via agentCliLock before calling this fallback, so a
				# CLI-excluded row here was actually stopped by a prerequisite, a live claim, or a post-claim race.
				if($row.PSObject.Properties.Name -ccontains 'blockedBy' -and $null -ne $row.blockedBy){
					$blockers=[Collections.Generic.List[string]]::new()
					foreach($path in @($row.blockedBy)){$blockers.Add([string]$path)}
					if($blockers.Count -gt 0){$blockers.Sort([StringComparer]::Ordinal);return "The requested Plan '$RequestedPlan' is blocked by prerequisite Plans: $(Format-DirtyPath $blockers.ToArray())."}
				}
				if($row.PSObject.Properties.Name -ccontains 'claim' -and $null -ne $row.claim){
					$result.holder=[ordered]@{session=[string]$row.claim.session;worktree=[string]$row.claim.worktree;expiresAt=$row.claim.expiresAt}
					$script:targetedAgentCliMismatch=[string]$row.agentCliMismatch
					return "The requested Plan '$RequestedPlan' is claimed by session $($result.holder.session) in worktree '$($result.holder.worktree)' until $((ConvertTo-Json -InputObject $result.holder.expiresAt -Compress).Trim('"'))."
				}
				return $fallback
			}
			if($state -ceq 'excluded' -and $row.PSObject.Properties.Name -ccontains 'diagnostic' -and -not [string]::IsNullOrWhiteSpace([string]$row.diagnostic)){return "The requested Plan '$RequestedPlan' is excluded from selection: $([string]$row.diagnostic)."}
			if($state -ceq 'claimed'){
				# ConvertFrom-Json turned the timestamp into a UTC DateTime; ConvertTo-Json renders the same instant as ISO 8601
				# text, though not necessarily WorktreeCli's exact digits.
				$result.holder=[ordered]@{session=[string]$row.claim.session;worktree=[string]$row.claim.worktree;expiresAt=$row.claim.expiresAt}
				return "The requested Plan '$RequestedPlan' is claimed by session $($result.holder.session) in worktree '$($result.holder.worktree)' until $((ConvertTo-Json -InputObject $result.holder.expiresAt -Compress).Trim('"'))."
			}
			if($state -ceq 'eligible'){return "A later scheduler listing currently reports the requested Plan '$RequestedPlan' as eligible; retry the targeted claim."}
			return $fallback
		}
		if($matches.Count -gt 1){return $fallback}
		$diagnostics=@($listing.diagnostics)
		$matchingDiagnostics=@($diagnostics|Where-Object{$_.PSObject.Properties.Name -ccontains 'plan' -and [string]$_.plan -ceq $RequestedPlan})
		if($matchingDiagnostics.Count -eq 1 -and $matchingDiagnostics[0].PSObject.Properties.Name -ccontains 'message' -and -not [string]::IsNullOrWhiteSpace([string]$matchingDiagnostics[0].message)){return "The requested Plan '$RequestedPlan' is excluded from selection: $([string]$matchingDiagnostics[0].message)."}
		if($matchingDiagnostics.Count -gt 0){return $fallback}
		# A Coretide Plan moves only with the session Coretide clone's fast-forward, whose paths lack the Projects/Coretide/ prefix.
		$repository=$Context.Worktree;$prefix='';$sync=if($result.Contains('sync')){$result.sync}else{$null}
		if($RequestedPlan.StartsWith('Projects/Coretide/',[StringComparison]::Ordinal)){$repository=$coretide;$prefix='Projects/Coretide/';$sync=$coretideSync}
		if($null -ne $sync){
			# Each NUL-delimited rename record is three fields: status with similarity score, source path, destination path.
			$renames=Invoke-NextPlanProcess 'git.exe' @('-C',$repository,'diff','--name-status','-z','-M','--diff-filter=R',$sync.from,$sync.to,'--','Documents/Plans') $repository
			if($renames.ExitCode -ne 0){return $fallback}
			$fields=@($renames.Stdout -split "`0")
			for($index=0;$index+2 -lt $fields.Count;$index+=3){
				if($prefix+$fields[$index+1] -ceq $RequestedPlan){
					$result.moved=[ordered]@{from=$RequestedPlan;to=$prefix+$fields[$index+2]}
					return "The requested Plan '$RequestedPlan' was renamed to '$($result.moved.to)' by the fast-forward $($sync.from)..$($sync.to); no Plan was claimed. Report this to the user, who decides whether to claim the Plan at its new path by rerunning this command with -Plan '$($result.moved.to)'."
				}
			}
		}
		return "The requested Plan '$RequestedPlan' is absent from the session tree."
	} catch {return $fallback}
}
try {
 $sharedScripts=Join-Path $PSScriptRoot '..\..\..\scripts';if(-not (Test-Path -LiteralPath (Join-Path $sharedScripts 'AgentScriptCommon.psm1'))){$sharedScripts=Join-Path $PSScriptRoot '..\..\..\..\.agents\scripts'}
 Import-Module (Join-Path $sharedScripts 'AgentScriptCommon.psm1') -Force -DisableNameChecking
 Import-Module (Join-Path $PSScriptRoot 'NextPlanWorkflowCommon.psm1') -Force -DisableNameChecking
 $targeted=-not [string]::IsNullOrWhiteSpace($Plan)
 if($ResumeRetained -and -not $targeted){Complete-Claim 2 'blocked' 'claim.usage-error' '-ResumeRetained is valid only with -Plan, because only a named Plan can carry work retained by an earlier deferral; no Plan was claimed and the worktree was not touched.' 'stop-report-to-user'}
 if($UserAuthorizedTakeover -and -not $targeted){Complete-Claim 2 'blocked' 'claim.usage-error' '-UserAuthorizedTakeover is valid only with -Plan, because only a named Plan has a holder to release; no claim was released or made and the worktree was not touched.' 'stop-report-to-user'}
 $normalizedPlan=if($targeted){$Plan.Replace('\','/')}else{''}
 if($AllowAgentCliMismatch -and (-not $targeted -or -not (Test-CanonicalPlanPath $normalizedPlan))){Complete-Claim 2 'blocked' 'claim.usage-error' '-AllowAgentCliMismatch is valid only with a canonical -Plan path (Documents/Plans/<Area>/<Name>.md or Projects/Coretide/Documents/Plans/<Area>/<Name>.md), because overriding a CLI lock requires an exact confirmed Plan; no Plan was claimed and the worktree was not touched.' 'stop-report-to-user'}
 $pattern=$null
 if ($Plan) {
  $Plan=$Plan.Replace('\','/')
  Assert-NextPlanGitPath $Plan
  if ($Plan.StartsWith('Documents/Plans/',[StringComparison]::Ordinal) -or $Plan.StartsWith('Projects/Coretide/Documents/Plans/',[StringComparison]::Ordinal)) {
   # WorktreeCli owns canonical path validation; preserve its exact-path contract.
  } else {
   $pattern=$Plan
   $Plan=$null
  }
 }
 $context=Get-NextPlanContext
 # A bare or exact request reports an authoritative different-Plan mismatch before the gates below. A held claim is
 # reused without git/tree gates or scheduler mutation only when the session is at the primary tip (still reading
 # claim.lineCount and claim.sections from the working-copy Plan file, or 0/[] when locally absent); off it, the request
 # falls through so the fast-forward below reports the sync object the /next-plan final claim refresh reads. A partial
 # pattern needs tree-backed resolution below. Remaining claim-status outcomes, unreadable output included, fall through
 # to the ordinary flow, which can synchronize before plan claim-next re-reports the held claim or returns the scheduler's stop.
 $heldClaim=$null
 if($null -eq $pattern){try{$claimStatus=Get-NextPlanClaimStatus $context $Plan;if($claimStatus.ExitCode -eq 2 -and [string]$claimStatus.Status.code -ceq 'claim-plan-mismatch'){$result.conflict=[ordered]@{requestedPlan=[string]$claimStatus.Status.requestedPlan;heldPlan=[string]$claimStatus.Status.heldPlan};Complete-Claim 2 'blocked' 'claim.plan-mismatch' 'This session already holds a different Plan claim.' 'stop-report-to-user'};if($claimStatus.ExitCode -eq 0 -and [string]$claimStatus.Status.code -ceq 'claimed'){$heldClaim=$claimStatus.Status}}catch{$heldClaim=$null}}
 if($null -ne $heldClaim){$result.claim=[ordered]@{claimed=$true;plan=[string]$heldClaim.plan;state='existing'}}
 if($null -ne $heldClaim -and $context.SessionHead -ceq $context.PrimaryTip){$planMeta=Get-PlanSectionMetadata $context.Worktree $result.claim.plan;$result.claim.lineCount=$planMeta.lineCount;$result.claim.sections=$planMeta.sections;Complete-Claim 0 'pass' 'reused' 'Existing Plan claim remains live for this session.' 'prepare'}
 $status=Invoke-NextPlanProcess 'git.exe' @('-C',$context.Worktree,'status','--porcelain=v1','-z','--untracked-files=all') $context.Worktree
 if($status.ExitCode -ne 0){throw (New-NextPlanStateBlocker "git status could not read the session worktree. $($status.Stderr.Trim())")}
 $dirty=@(Get-DirtyPath $status.Stdout)
 # The session Coretide clone is fetched first, so its upstream is the primary Coretide HEAD the checks below compare
 # against; its dirty paths join the engine's under their session prefix.
 $coretide=Get-AgentCoretideRepository $context.Worktree
 $coretideDirty=@()
 if($coretide){
  $fetch=Invoke-NextPlanProcess 'git.exe' @('-C',$coretide,'fetch') $coretide
  if($fetch.ExitCode -ne 0){throw "git fetch failed in the session Coretide clone. $($fetch.Stderr.Trim())"}
  $coretideStatus=Invoke-NextPlanProcess 'git.exe' @('-C',$coretide,'status','--porcelain=v1','-z','--untracked-files=all') $coretide
  if($coretideStatus.ExitCode -ne 0){throw (New-NextPlanStateBlocker "git status could not read the session Coretide clone. $($coretideStatus.Stderr.Trim())")}
  $coretideDirty=@(Get-DirtyPath $coretideStatus.Stdout|ForEach-Object{"Projects/Coretide/$_"})
  $dirty+=$coretideDirty
 }
 if($dirty.Count -ne 0){
  # Uncommitted scheduler input must never reach selection or healing. plan claim-next reads only committed trees
  # (session HEAD and the primary tip) and with --plan claims only that Plan, and healing reads the primary tip, so two
  # runs that pass --plan are exempt: a run whose early lookup reported the held claim, whatever is dirty, and an exact
  # -Plan request whose every dirty path is under either Plans root and none is the requested Plan. The overlap
  # check below protects retained work.
  $schedulerPaths=@($dirty|Where-Object{$_.StartsWith('Documents/Plans/',[StringComparison]::Ordinal) -or $_.StartsWith('Projects/Coretide/Documents/Plans/',[StringComparison]::Ordinal)})
  $exempt=($null -ne $heldClaim) -or ($targeted -and $null -eq $pattern -and $schedulerPaths.Count -eq $dirty.Count -and $dirty -cnotcontains $Plan)
  if($schedulerPaths.Count -ne 0 -and -not $exempt){Complete-Claim 2 'blocked' 'claim.worktree-dirty' "A plan claim cannot run while scheduler input is uncommitted; $($schedulerPaths.Count) path(s) under Documents/Plans/ or Projects/Coretide/Documents/Plans/ are modified or untracked: $(Format-DirtyPath $schedulerPaths). No Plan was claimed and the worktree was not touched. Report this to the user, who decides whether to set those paths aside (git stash push -u -m '<tag>' -- <those paths>, rerun this command, then git stash apply --index <that stash entry>, which keeps a staged version) or to land that work first." 'stop-report-to-user'}
  if(-not $ResumeRetained -and -not $exempt){$route=if($targeted){' To resume uncommitted work retained by an earlier deferral of this Plan, rerun this command with -ResumeRetained.'}else{''};Complete-Claim 2 'blocked' 'claim.worktree-dirty' "Session worktree must be clean before a plan claim; $($dirty.Count) path(s) are modified or untracked: $(Format-DirtyPath $dirty). No Plan was claimed and the worktree was not touched.$route" $(if($targeted){'resume-with-flag'}else{'stop-report-to-user'})}
  if($context.SessionHead -cne $context.PrimaryTip){
   # The claim below fast-forwards the session, which would overwrite a retained path the primary movement also changed.
   # NUL-delimited output keeps both sides of the comparison in the same raw path form.
   $incoming=Invoke-NextPlanProcess 'git.exe' @('-C',$context.Worktree,'diff','--name-only','-z',$context.SessionHead,$context.PrimaryTip) $context.Worktree
   if($incoming.ExitCode -ne 0){throw (New-NextPlanStateBlocker "git diff --name-only could not compare the session head with the primary tip. $($incoming.Stderr.Trim())")}
   $incomingPaths=@(($incoming.Stdout -split "`0")|Where-Object{-not [string]::IsNullOrEmpty($_)})
   $overlap=@($dirty|Where-Object{$incomingPaths -ccontains $_})
   if($overlap.Count -ne 0){Complete-Claim 2 'blocked' 'claim.worktree-dirty' "The claim cannot proceed because fast-forwarding the session from $($context.SessionHead) to the primary tip $($context.PrimaryTip) would touch $($overlap.Count) retained path(s): $(Format-DirtyPath $overlap). No new Plan was claimed and the worktree was not touched. Report this to the user, who decides how to resolve it, for example by landing that work first." 'stop-report-to-user'}
  }
  if($coretideDirty.Count -ne 0){
   # The claim below also fast-forwards the session Coretide clone to its upstream.
   $coretideIncoming=Invoke-NextPlanProcess 'git.exe' @('-C',$coretide,'diff','--name-only','-z','HEAD','@{upstream}') $coretide
   if($coretideIncoming.ExitCode -ne 0){throw (New-NextPlanStateBlocker "git diff --name-only could not compare the session Coretide clone with its upstream. $($coretideIncoming.Stderr.Trim())")}
   $coretideIncomingPaths=@(($coretideIncoming.Stdout -split "`0")|Where-Object{-not [string]::IsNullOrEmpty($_)}|ForEach-Object{"Projects/Coretide/$_"})
   $coretideOverlap=@($coretideDirty|Where-Object{$coretideIncomingPaths -ccontains $_})
   if($coretideOverlap.Count -ne 0){Complete-Claim 2 'blocked' 'claim.worktree-dirty' "The claim cannot proceed because fast-forwarding the session Coretide clone to its upstream would touch $($coretideOverlap.Count) retained path(s): $(Format-DirtyPath $coretideOverlap). No new Plan was claimed and the worktree was not touched. Report this to the user, who decides how to resolve it, for example by landing that work first." 'stop-report-to-user'}
  }
  $result.retained=[ordered]@{count=$dirty.Count;truncated=($dirty.Count -gt 10);paths=@($dirty|Select-Object -First 10)}
 }
 if($coretide){
  # WorktreeCli's claim path also requires the clone HEAD to be an ancestor of the primary Coretide HEAD, so a clone
  # holding any commit its upstream lacks stops here, before either fast-forward moves anything.
  $coretideRevList=Invoke-NextPlanProcess 'git.exe' @('-C',$coretide,'rev-list','@{upstream}..HEAD') $coretide
  if($coretideRevList.ExitCode -ne 0){throw "git rev-list @{upstream}..HEAD failed in the session Coretide clone. $($coretideRevList.Stderr.Trim())"}
  $coretideHeld=@(($coretideRevList.Stdout -split "`n")|ForEach-Object{$_.Trim()}|Where-Object{$_.Length -gt 0})
  if($coretideHeld.Count -ne 0){Complete-Claim 2 'blocked' 'claim.session-diverged' "The session Coretide clone holds $($coretideHeld.Count) commit(s) its upstream does not ($($coretideHeld -join ', ')), so it cannot be fast-forwarded; no new Plan was claimed and neither the session branch nor the clone was moved. Report this to the user, who decides how to resolve it; these are ordinary unlanded Coretide commits to land." 'stop-report-to-user'}
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
   Complete-Claim 2 'blocked' 'claim.session-diverged' "Session HEAD $($context.SessionHead) holds $($held.Count) commit(s) the primary tip $($context.PrimaryTip) does not ($($held -join ', ')), so the session cannot be fast-forwarded; no new Plan was claimed and the session branch was not moved. Report this to the user, who decides how to resolve it. If the primary branch was rewritten under this session, for example by a history squash, those commits are the session's own work stranded off the rewritten history, and the user-authorized recovery is pwsh -NoProfile -File .agents/scripts/Repair-SessionForkPoint.ps1 run from the session worktree root, which replays this session onto the primary tip, followed by a rerun of this command; that repair changes nothing and reports fork-point.not-recoverable when the pre-rewrite fork point is gone, and reports fork-point.repair-refused with the cause in its own message when the worktree is not clean or the replay fails. Otherwise these are ordinary unlanded commits to land." 'stop-report-to-user'
  }
  $merge=Invoke-NextPlanProcess 'git.exe' @('-C',$context.Worktree,'merge','--ff-only',$context.PrimaryTip) $context.Worktree
  if($merge.ExitCode -ne 0){$mergeDetail=(($merge.Stdout,$merge.Stderr) -join "`n").Trim();Complete-Claim 1 'error' 'claim.session-sync-failed' "Fast-forwarding the session branch from $($context.SessionHead) to the primary tip $($context.PrimaryTip) failed; no new Plan was claimed, and the worktree may be partially updated, so inspect it before retrying. git reported: $mergeDetail" 'stop-report-to-user'}
  $result.sync=[ordered]@{fastForwarded=$true;from=$context.SessionHead;to=$context.PrimaryTip}
 }
 $coretideSync=$null
 if($coretide){
  $coretideRange=Invoke-NextPlanProcess 'git.exe' @('-C',$coretide,'rev-parse','HEAD','@{upstream}') $coretide
  if($coretideRange.ExitCode -ne 0){throw "git rev-parse HEAD @{upstream} failed in the session Coretide clone. $($coretideRange.Stderr.Trim())"}
  $coretideFrom,$coretideTo=@(($coretideRange.Stdout -split "`n")|ForEach-Object{$_.Trim()}|Where-Object{$_.Length -gt 0})
  if($coretideFrom -cne $coretideTo){$coretideSync=[ordered]@{from=$coretideFrom;to=$coretideTo}}
  $coretideMerge=Invoke-NextPlanProcess 'git.exe' @('-C',$coretide,'merge','--ff-only','@{upstream}') $coretide
  if($coretideMerge.ExitCode -ne 0){$coretideMergeDetail=(($coretideMerge.Stdout,$coretideMerge.Stderr) -join "`n").Trim();Complete-Claim 1 'error' 'claim.session-sync-failed' "Fast-forwarding the session Coretide clone to its upstream failed; no new Plan was claimed, and the clone may be partially updated, so inspect it before retrying. git reported: $coretideMergeDetail" 'stop-report-to-user'}
 }
 # plan validate can heal this session's claim and still fail, so from here on only plan claim-next reports it.
 $result.claim=$null
 $validate=Invoke-NextPlanProcess $context.WorktreeCli @('plan','validate','--repo',$context.CommonDirectory,'--worktree',$context.Worktree) $context.Worktree
 $validation=ConvertFrom-NextPlanProcessJson $validate 'plan validate'; $projection=[ordered]@{}; foreach($name in @('status','code','message','diagnostics','healedClaims')){if($validation.PSObject.Properties.Name -ccontains $name){$projection[$name]=$validation.$name}}; $result.validation=$projection
 if($validate.ExitCode -eq 2 -and [string]$projection['code'] -ceq 'busy'){Complete-Claim 2 'blocked' 'scheduler.busy' 'Another session held the plan scheduler for the full wait; no new Plan was claimed. Tell the user and retry later.' 'retry-later'}
 if($validate.ExitCode -eq 1 -and [string]$projection['code'] -ceq 'guard-unavailable'){Complete-Claim 1 'error' 'scheduler.guard-unavailable' 'The plan scheduler lock storage is unusable; no new Plan was claimed. Tell the user - this is not a Plan metadata problem.' 'stop-report-to-user'}
 if($validate.ExitCode -ne 0){$exit=if($validate.ExitCode -eq 2){2}else{1};Complete-Claim $exit $(if($exit -eq 2){'blocked'}else{'error'}) 'plan.validation-failed' 'Plan validation failed.' 'stop-report-to-user'}
 # WorktreeCli reads every .md under Documents/Plans/ as a plan document, and one without valid metadata can fail every
 # plan complete|reject, so a layout violation or an invalid-metadata diagnostic stops the run before any claim.
 $violations=@(Get-AgentPlansLayoutViolation -RepositoryRoot $context.Worktree);$result.integrity=[ordered]@{totalCount=$violations.Count;items=@($violations|Select-Object -First 16);truncated=($violations.Count -gt 16)}
 $invalidMetadata=@(@($projection['diagnostics'])|Where-Object{$null -ne $_ -and $_.PSObject.Properties.Name -ccontains 'code' -and [string]$_.code -ceq 'invalid-metadata'})
 if($violations.Count -ne 0 -or $invalidMetadata.Count -ne 0){$firstViolation=if($violations.Count -ne 0){" (first: $($violations[0].rule) '$($violations[0].path)')"}else{''};$firstInvalid=if($invalidMetadata.Count -ne 0 -and $invalidMetadata[0].PSObject.Properties.Name -ccontains 'plan'){" (first: '$([string]$invalidMetadata[0].plan)')"}else{''};Complete-Claim 2 'blocked' 'claim.plans-integrity' "Documents/Plans/ holds $($violations.Count) layout violation(s)$firstViolation and $($invalidMetadata.Count) invalid-metadata diagnostic(s)$firstInvalid; no Plan was claimed. Report each listed integrity item and diagnostic to the user." 'stop-report-to-user'}
 if($null -ne $pattern){
  $listResponse=Invoke-NextPlanProcess $context.WorktreeCli @('plan','list','--repo',$context.CommonDirectory,'--worktree',$context.Worktree) $context.Worktree
  $listing=ConvertFrom-NextPlanProcessJson $listResponse 'plan list'
  if($listResponse.ExitCode -ne 0){$downstreamCode=if($listing.PSObject.Properties.Name -ccontains 'code'){[string]$listing.code}else{'unknown'};$exit=if($listResponse.ExitCode -eq 2){2}else{1};Complete-Claim $exit $(if($exit -eq 2){'blocked'}else{'error'}) $downstreamCode "WorktreeCli plan list failed during partial-name resolution: $downstreamCode." 'stop-report-to-user'}
  $hiddenPlans=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
  if($listing.PSObject.Properties.Name -ccontains 'plans' -and $null -ne $listing.plans){
   foreach($row in @($listing.plans)){
    if($row.PSObject.Properties.Name -ccontains 'path' -and $row.PSObject.Properties.Name -ccontains 'agentCliMismatch' -and -not [string]::IsNullOrWhiteSpace([string]$row.agentCliMismatch)){
     [void]$hiddenPlans.Add([string]$row.path)
    }
   }
  }
  $candidates=[Collections.Generic.List[string]]::new()
  foreach($executablePlan in @($validation.plans)){
   if($executablePlan.PSObject.Properties.Name -ccontains 'path'){
    $candidate=[string]$executablePlan.path
    if($hiddenPlans.Contains($candidate)){continue}
    # Match below whichever Plans root prefixes the candidate, so a pattern naming a root cannot match every Plan.
    $relative=$candidate;foreach($plansRoot in @('Documents/Plans/','Projects/Coretide/Documents/Plans/')){if($candidate.StartsWith($plansRoot,[StringComparison]::Ordinal)){$relative=$candidate.Substring($plansRoot.Length)}}
    if($relative.Contains($pattern,[StringComparison]::Ordinal)){$candidates.Add($candidate)}
   }
  }
  $candidates.Sort([StringComparer]::Ordinal)
  if($candidates.Count -eq 0){Complete-Claim 2 'blocked' 'plan-name-not-found' "Pattern '$pattern' matched no validated executable Plan." 'stop-report-to-user'}
  if($candidates.Count -gt 1){$result.candidates=@($candidates);Complete-Claim 2 'blocked' 'plan-name-ambiguous' "Pattern '$pattern' matched multiple validated executable Plans; use a canonical Documents/Plans path." 'stop-report-to-user'}
  $Plan=$candidates[0]
 }
 # A bare run holding a claim targets that Plan, so a claim healed away since the lookup ends in none or a fresh claim on
 # that same Plan, never in a claim on another Plan over this Plan's retained work.
 $claimArguments=@('plan','claim-next','--repo',$context.CommonDirectory,'--primary-worktree',$context.Primary,'--worktree',$context.Worktree,'--branch',$context.SessionBranch,'--owner',$context.Owner,'--session',$context.Session);if($Plan){$claimArguments+=@('--plan',$Plan)}elseif($null -ne $heldClaim){$claimArguments+=@('--plan',[string]$heldClaim.plan)};if($AllowAgentCliMismatch){$claimArguments+='--allow-agent-cli-mismatch'}
 # claim-next reports claim-plan-mismatch before none, so reaching the none branch proves this session holds no other
 # claim; a user-authorized takeover releases the named holder's claim there at most once, then claims through this path.
 $takeover=$UserAuthorizedTakeover.IsPresent
 while($true){
  $script:targetedAgentCliMismatch=$null
  $response=Invoke-NextPlanProcess $context.WorktreeCli $claimArguments $context.Worktree;$claim=ConvertFrom-NextPlanProcessJson $response 'plan claim-next'
  if($response.ExitCode -eq 2 -and [string]$claim.code -ceq 'claim-plan-mismatch'){$result.conflict=[ordered]@{requestedPlan=[string]$claim.requestedPlan;heldPlan=[string]$claim.heldPlan};Complete-Claim 2 'blocked' 'claim.plan-mismatch' 'This session already holds a different Plan claim.' 'stop-report-to-user'}
  if($response.ExitCode -ne 0){$detail=(($response.Stdout,$response.Stderr) -join "`n").Trim();$exit=if($response.ExitCode -eq 2){2}else{1};Complete-Claim $exit $(if($exit -eq 2){'blocked'}else{'error'}) 'claim.rejected' "WorktreeCli rejected the plan claim (exit $($response.ExitCode)). WorktreeCli reported: $detail" 'stop-report-to-user'}
  $code=[string]$claim.code
  if($code -cne 'none'){break}
  if($claim.PSObject.Properties.Name -ccontains 'agentCliLock' -and -not [string]::IsNullOrWhiteSpace([string]$claim.agentCliLock)){$result.agentCliLock=[string]$claim.agentCliLock;$lockedPlan=if($Plan){$Plan}elseif($null -ne $heldClaim){[string]$heldClaim.plan}else{''};Complete-Claim 0 'pass' 'none-available' "The requested Plan '$lockedPlan' is locked to agent CLI '$($result.agentCliLock)'." 'checkpoint-followup-gate'}
  $message=if($targeted){Get-TargetedNoneMessage $context $Plan}elseif($null -ne $heldClaim){Get-TargetedNoneMessage $context ([string]$heldClaim.plan)}else{'No eligible Plan is available.'}
  if($result.Contains('moved')){Complete-Claim 2 'blocked' 'claim.plan-moved' $message 'stop-report-to-user'}
  if(-not $takeover -or -not $result.Contains('holder') -or [string]$result.holder.session -ceq $context.Session){Complete-Claim 0 'pass' 'none-available' $message 'checkpoint-followup-gate'}
  if(-not $AllowAgentCliMismatch -and -not [string]::IsNullOrWhiteSpace($script:targetedAgentCliMismatch)){$result.agentCliLock=$script:targetedAgentCliMismatch;Complete-Claim 0 'pass' 'none-available' "The requested Plan '$Plan' is claimed by session $($result.holder.session) in worktree '$($result.holder.worktree)' and is also locked to agent CLI '$($result.agentCliLock)'; the existing claim was left intact until both takeover and CLI override are authorized." 'checkpoint-followup-gate'}
  $takeover=$false
  # Every scheduler claim is created by this script with owner and session both set to the session ID, and plan unclaim
  # finds a claim by owner and session alone, using --worktree only to resolve the repository.
  $unclaim=Invoke-NextPlanProcess $context.WorktreeCli @('plan','unclaim','--repo',$context.CommonDirectory,'--worktree',$context.Worktree,'--owner',$result.holder.session,'--session',$result.holder.session) $context.Worktree
  $releaseCode=try{[string](ConvertFrom-NextPlanProcessJson $unclaim 'plan unclaim').code}catch{''}
  if($unclaim.ExitCode -ne 0 -or $releaseCode -notin @('released','already-absent')){$exit=if($unclaim.ExitCode -eq 2){2}else{1};Complete-Claim $exit $(if($exit -eq 2){'blocked'}else{'error'}) 'claim.takeover-failed' "WorktreeCli did not release the claim session $($result.holder.session) holds on '$Plan' (exit $($unclaim.ExitCode), code '$releaseCode'); no Plan was claimed." 'stop-report-to-user'}
 }
 if($code -cne 'claimed' -and $code -cne 'existing'){throw "plan claim-next returned an unknown result code '$code'."}
 $result.claim=[ordered]@{claimed=$true;plan=[string]$claim.plan;state=$code}
 $planMeta=Get-PlanSectionMetadata $context.Worktree $result.claim.plan
 $result.claim.lineCount=$planMeta.lineCount;$result.claim.sections=$planMeta.sections
 if($code -ceq 'existing'){Complete-Claim 0 'pass' 'reused' 'Existing Plan claim remains live for this session.' 'prepare'}
 Complete-Claim 0 'pass' 'ok' 'Plan claimed for this session.' 'prepare'
} catch {if(Get-Command Test-NextPlanStateBlocker -ErrorAction SilentlyContinue){if(Test-NextPlanStateBlocker $_){Complete-Claim 2 'blocked' 'claim.context-conflict' $_.Exception.Message 'stop-report-to-user'}};Complete-Claim 1 'error' 'claim.failed' $_.Exception.Message 'stop-report-to-user'}
