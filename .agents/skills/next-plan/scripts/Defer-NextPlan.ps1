[CmdletBinding()]
param()
$ErrorActionPreference='Stop';Set-StrictMode -Version Latest
$result=[ordered]@{schemaVersion='broken-engine-next-plan-deferral-result/v1';status='error';code='internal.error';message='Deferral did not run.';nextAction='stop-report-to-user';claim=$null}
function Complete-Deferral([int]$ExitCode,[string]$Status,[string]$Code,[string]$Message,[string]$NextAction){$result.status=$Status;$result.code=$Code;$result.message=$Message;$result.nextAction=$NextAction;[Console]::Out.Write(($result|ConvertTo-Json -Depth 20 -Compress));exit $ExitCode}
# NUL-delimited porcelain v1 emits raw paths, so a path containing a space or a quotable character is never C-quoted;
# a rename or copy record is followed by one extra field holding the original path, and both sides are worktree paths
# the resume claim must weigh.
function Get-DirtyPath([string]$Porcelain){$paths=[Collections.Generic.List[string]]::new();$fields=@($Porcelain -split "`0");$index=0;while($index -lt $fields.Count){$record=$fields[$index];$index++;if($record.Length -lt 4){continue};$paths.Add($record.Substring(3));$state=$record.Substring(0,2);if($state.Contains('R') -or $state.Contains('C')){if($index -lt $fields.Count){$paths.Add($fields[$index]);$index++}}};return $paths}
try {
	Import-Module (Join-Path $PSScriptRoot 'NextPlanWorkflowCommon.psm1') -Force -DisableNameChecking
	$context=Get-NextPlanContext
	$claimArguments=@('--repo',$context.CommonDirectory,'--worktree',$context.Worktree,'--owner',$context.Owner,'--session',$context.Session)
	$status=Invoke-NextPlanProcess $context.WorktreeCli (@('plan','claim-status')+$claimArguments) $context.Worktree
	$claimStatus=ConvertFrom-NextPlanProcessJson $status 'plan claim-status'
	if($status.ExitCode -ne 0){Complete-Deferral $(if($status.ExitCode -eq 2){2}else{1}) $(if($status.ExitCode -eq 2){'blocked'}else{'error'}) 'defer.claim-status-failed' 'WorktreeCli could not report the Plan claim.' 'stop-report-to-user'}
	if([string]$claimStatus.code -ceq 'none'){Complete-Deferral 0 'pass' 'no-claim' 'No Plan claim is present.' 'stop-report-to-user'}
	# Read the retained work before releasing, so a failure to read it stops with the claim still held.
	$tree=Invoke-NextPlanProcess 'git.exe' @('-C',$context.Worktree,'status','--porcelain=v1','-z','--untracked-files=all') $context.Worktree
	if($tree.ExitCode -ne 0){throw (New-NextPlanStateBlocker "git status could not read the session worktree, so retained work could not be reported; the Plan claim is unchanged. $($tree.Stderr.Trim())")}
	$retained=@(Get-DirtyPath $tree.Stdout)
	$response=Invoke-NextPlanProcess $context.WorktreeCli (@('plan','unclaim')+$claimArguments) $context.Worktree
	$release=ConvertFrom-NextPlanProcessJson $response 'plan unclaim'
	if($response.ExitCode -ne 0 -or [string]$release.code -notin @('released','already-absent')){Complete-Deferral $(if($response.ExitCode -eq 2){2}else{1}) $(if($response.ExitCode -eq 2){'blocked'}else{'error'}) 'defer.unclaim-failed' 'WorktreeCli did not release the Plan claim.' 'stop-report-to-user'}
	$result.claim=[ordered]@{released=$true};$result.retained=[ordered]@{count=$retained.Count;truncated=($retained.Count -gt 10);paths=@($retained|Select-Object -First 10)}
	# The release check above admits only 'released' and 'already-absent', and both are a stop the user is told about.
	Complete-Deferral 0 'pass' ([string]$release.code) 'Plan claim deferred.' 'stop-report-to-user'
} catch {if(Get-Command Test-NextPlanStateBlocker -ErrorAction SilentlyContinue){if(Test-NextPlanStateBlocker $_){Complete-Deferral 2 'blocked' 'defer.context-conflict' $_.Exception.Message 'stop-report-to-user'}};Complete-Deferral 1 'error' 'defer.failed' $_.Exception.Message 'stop-report-to-user'}
