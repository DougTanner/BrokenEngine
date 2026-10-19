[CmdletBinding()]
param([switch]$Reject)
$ErrorActionPreference='Stop';Set-StrictMode -Version Latest
$result=[ordered]@{schemaVersion='broken-engine-next-plan-completion-result/v5';status='error';code='internal.error';message='Terminal preparation did not run.';messageLength=33;messageTruncated=$false;workflowTerminal=$false;nextAction='stop-report-to-user';claim=$null;changes=[ordered]@{totalCount=0;path=$null};diagnostics=[ordered]@{totalCount=0;items=@();truncated=$false;selector=$null;requery=$null}}
function Get-BoundedText([AllowNull()]$Value,[int]$Limit) { if($null -eq $Value){return [pscustomobject]@{Text=$null;Length=0;Truncated=$false}};$Value=[string]$Value;$length=$Value.Length;[pscustomobject]@{Text=$(if($length -gt $Limit){$Value.Substring(0,$Limit)}else{$Value});Length=$length;Truncated=($length -gt $Limit)} }
function Set-ResultMessage([string]$Message) {$bounded=Get-BoundedText $Message 512;$result.message=$bounded.Text;$result.messageLength=$bounded.Length;$result.messageTruncated=$bounded.Truncated}
function Add-Diagnostic([string]$Source,[string]$Code,[AllowNull()][string]$Path,[string]$Message) {$codeText=Get-BoundedText $Code 128;$pathText=Get-BoundedText $Path 1024;$messageText=Get-BoundedText $Message 512;$result.diagnostics.items=@([ordered]@{source=$Source;code=$codeText.Text;codeLength=$codeText.Length;codeTruncated=$codeText.Truncated;path=$pathText.Text;pathLength=$pathText.Length;pathTruncated=$pathText.Truncated;message=$messageText.Text;messageLength=$messageText.Length;messageTruncated=$messageText.Truncated});$result.diagnostics.totalCount=1}
function Complete-Workflow([int]$ExitCode,[string]$Status,[string]$Code,[string]$Message,[string]$NextAction){$result.status=$Status;$result.code=(Get-BoundedText $Code 128).Text;Set-ResultMessage $Message;$result.nextAction=$NextAction;[Console]::Out.Write(($result|ConvertTo-Json -Depth 10 -Compress));exit $ExitCode}
# The full list goes to a file so no changed path is ever cut; the result carries only its count and path.
function Set-ChangedPaths($ChangedPaths,[string]$Worktree) {$paths=@($ChangedPaths|ForEach-Object{[string]$_});[Array]::Sort($paths,[StringComparer]::Ordinal);$listPath='Temp/next-plan-completion-changes.txt';[void][IO.Directory]::CreateDirectory((Join-Path $Worktree 'Temp'));[IO.File]::WriteAllText((Join-Path $Worktree $listPath),(@($paths|ForEach-Object{"$_`n"}) -join ''),[Text.UTF8Encoding]::new($false));$result.changes=[ordered]@{totalCount=$paths.Count;path=$listPath}}
try {
 $sharedScripts=Join-Path $PSScriptRoot '..\..\..\scripts';if(-not (Test-Path -LiteralPath (Join-Path $sharedScripts 'AgentScriptCommon.psm1'))){$sharedScripts=Join-Path $PSScriptRoot '..\..\..\..\.agents\scripts'}
 Import-Module (Join-Path $sharedScripts 'AgentScriptCommon.psm1') -Force -DisableNameChecking
 Import-Module (Join-Path $PSScriptRoot 'NextPlanWorkflowCommon.psm1') -Force -DisableNameChecking
 $context=Get-NextPlanContext
 $status=Get-NextPlanClaimStatus $context
 $claimArguments=$status.Arguments;$claimStatus=$status.Status
 if($status.ExitCode -ne 0){$exit=if($status.ExitCode -eq 2){2}else{1};Complete-Workflow $exit $(if($exit -eq 2){'blocked'}else{'error'}) 'completion.claim-status-failed' 'WorktreeCli could not report the Plan claim.' 'stop-report-to-user'}
 if([string]$claimStatus.code -ceq 'none'){$result.claim=[ordered]@{state='absent';disposition='none'};Complete-Workflow 0 'pass' 'no-claim' 'No Plan claim is present.' 'checkpoint-followup-gate'}
 $result.claim=[ordered]@{state='claimed';disposition='none'}
 # The held Plan's support folder is checked before the terminal command's first change, so a refusal leaves everything in place.
 $heldPlan=[string]$claimStatus.plan;$supportFolder=$heldPlan.Substring(0,$heldPlan.Length-'.md'.Length)
 $folderViolations=@(Get-AgentPlansLayoutViolation -RepositoryRoot $context.Worktree -Folder $supportFolder)
 # Remove-PlanSupportFolder.ps1 deletes only the whole folder, so an absent folder whose every violation is support-uncommitted is an earlier run's deletion.
 $earlierDeletion=$folderViolations.Count -ne 0 -and -not (Test-Path -LiteralPath (Join-Path $context.Worktree $supportFolder)) -and @($folderViolations|Where-Object{$_.rule -cne 'support-uncommitted'}).Count -eq 0
 if($earlierDeletion){Add-Diagnostic 'Get-AgentPlansLayoutViolation' ([string]$folderViolations[0].rule) ([string]$folderViolations[0].path) "$($folderViolations.Count) committed file(s) of the support folder '$supportFolder' are deleted in the worktree."
  Complete-Workflow 2 'blocked' 'completion.already-prepared' "An earlier run already prepared the claim exit and deleted the support folder '$supportFolder'; nothing was changed. To run it again, restore the session's changed Plan files and that folder with Git, then rerun." 'stop-report-to-user'}
 if($folderViolations.Count -ne 0){Add-Diagnostic 'Get-AgentPlansLayoutViolation' ([string]$folderViolations[0].rule) ([string]$folderViolations[0].path) "$($folderViolations.Count) layout violation(s) in the support folder '$supportFolder'.";Complete-Workflow 2 'blocked' 'completion.support-folder-invalid' "The held Plan's support folder breaks the Documents/Plans/AGENTS.md layout rules; nothing was changed." 'stop-report-to-user'}
 # Another live Plan naming the held Plan's file name after its metadata line would cite a deleted Plan once this exit runs, so it refuses before any change.
 $heldName=[IO.Path]::GetFileName($heldPlan);$citation=[regex]::new('(?<![A-Za-z0-9])'+[regex]::Escape($heldName));$citingCount=0;$firstCiting=$null
 $listing=Invoke-AgentProcess 'git' @('-C',$context.Worktree,'ls-files','-z','--','Documents/Plans') $context.Worktree
 if($listing.ExitCode -ne 0){throw "git ls-files failed for 'Documents/Plans': $($listing.Stderr.Trim())"}
 foreach($path in ($listing.Stdout -split "`0")){
  if(-not $path.EndsWith('.md',[StringComparison]::Ordinal) -or [IO.Path]::GetFileName($path) -ceq 'AGENTS.md' -or $path -ceq $heldPlan){continue}
  # A Plan deleted in the worktree is still listed until the deletion is staged.
  $file=Join-Path $context.Worktree $path;if(-not (Test-Path -LiteralPath $file -PathType Leaf)){continue}
  $lines=[IO.File]::ReadAllLines($file)
  for($index=1;$index -lt $lines.Count;$index++){if($citation.IsMatch($lines[$index])){$citingCount++;if($null -eq $firstCiting){$firstCiting=[pscustomobject]@{Path=$path;Line=$index+1}}}}
 }
 if($citingCount -ne 0){Add-Diagnostic 'Complete-NextPlan' 'completion.plan-cited' $firstCiting.Path "Line $($firstCiting.Line) names '$heldName'.";Complete-Workflow 2 'blocked' 'completion.plan-cited' "$citingCount line(s) in other live Plans name '$heldName'; nothing was changed. Rewrite each line by the citation rule in .agents/skills/next-plan/references/preparation-brief.md '## Citation rule', then rerun." 'prepare'}
 $operation=if($Reject){'reject'}else{'complete'}
 $expectedDisposition=if($Reject){'rejected'}else{'completed'}
 $arguments=@('plan',$operation)+$claimArguments
 if($Reject){$arguments+='--user-authorized-rejection'}
 $response=Invoke-NextPlanProcess $context.WorktreeCli $arguments $context.Worktree;$prepared=ConvertFrom-NextPlanProcessJson $response $operation
 if($response.ExitCode -ne 0){$exit=if($response.ExitCode -eq 2){2}else{1};$downstreamCode=if($prepared.PSObject.Properties.Name -ccontains 'code'){[string]$prepared.code}else{'unknown'};$downstreamPath=if($prepared.PSObject.Properties.Name -ccontains 'plan'){[string]$prepared.plan}else{$null};$downstreamMessage=if($prepared.PSObject.Properties.Name -ccontains 'message'){[string]$prepared.message}else{'WorktreeCli did not return a diagnostic message.'};Add-Diagnostic 'WorktreeCli' $downstreamCode $downstreamPath $downstreamMessage;$result.diagnostics.requery='Complete-NextPlan';Complete-Workflow $exit $(if($exit -eq 2){'blocked'}else{'error'}) 'completion.prepare-failed' 'WorktreeCli did not prepare Plan terminal state.' 'stop-report-to-user'}
 if([string]$prepared.code -cne $expectedDisposition){Complete-Workflow 2 'blocked' 'completion.disposition-mismatch' 'Prepared Plan terminal disposition does not match the requested operation.' 'stop-report-to-user'}
 $result.claim=[ordered]@{state='claimed';disposition=$expectedDisposition}
 $shell=[Environment]::ProcessPath;if([string]::IsNullOrEmpty($shell)){$shell='pwsh'}
 $removal=Invoke-NextPlanProcess $shell @('-NoProfile','-File',(Join-Path $sharedScripts 'Remove-PlanSupportFolder.ps1'),'-Plan',$heldPlan,'-RepositoryRoot',$context.Worktree) $context.Worktree;$removed=ConvertFrom-NextPlanProcessJson $removal 'Remove-PlanSupportFolder'
 if($removal.ExitCode -ne 0){Add-Diagnostic 'Remove-PlanSupportFolder' ([string]$removed.code) ([string]$removed.folder) ([string]$removed.message);Complete-Workflow 1 'error' 'completion.support-folder-delete-failed' "The Plan terminal state was prepared but its support folder '$supportFolder' was not deleted; restore the session's changed Plan files and that folder with Git, then rerun." 'stop-report-to-user'}
 Set-ChangedPaths (@($prepared.changedPaths)+@($removed.paths)) $context.Worktree
 Complete-Workflow 0 'pass' 'ok' 'Plan terminal state prepared; finalization is the mandatory next action.' 'finalize-changes'
} catch {if(Get-Command Test-NextPlanStateBlocker -ErrorAction SilentlyContinue){if(Test-NextPlanStateBlocker $_){Complete-Workflow 2 'blocked' 'completion.context-conflict' $_.Exception.Message 'stop-report-to-user'}};Complete-Workflow 1 'error' 'completion.failed' $_.Exception.Message 'stop-report-to-user'}
