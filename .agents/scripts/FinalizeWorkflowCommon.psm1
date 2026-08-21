Set-StrictMode -Version Latest

$script:FinalizeUtf8 = [Text.UTF8Encoding]::new($false, $true)

function Get-FinalizeRootPreservingFullPath([string] $Path) {
	$full = [IO.Path]::GetFullPath($Path)
	$root = [IO.Path]::GetPathRoot($full)
	if ($full.Length -gt $root.Length) {
		return $full.TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
	}
	return $full
}

if (-not ('BrokenEngine.FinalizeWorkflowPathIdentity' -as [type])) {
	Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.Win32.SafeHandles;
namespace BrokenEngine {
public static class FinalizeWorkflowPathIdentity {
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
 static extern SafeFileHandle CreateFileW(string name,uint access,uint share,IntPtr security,uint creation,uint flags,IntPtr template);
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
 static extern uint GetFinalPathNameByHandleW(SafeFileHandle file,StringBuilder path,uint size,uint flags);
 public static string Resolve(string path) {
  using(var handle=CreateFileW(path,0,7,IntPtr.Zero,3,0x02000000,IntPtr.Zero)) {
   if(handle.IsInvalid) throw new Win32Exception(Marshal.GetLastWin32Error());
   var buffer=new StringBuilder(32768); uint length=GetFinalPathNameByHandleW(handle,buffer,(uint)buffer.Capacity,0);
   if(length==0 || length>=buffer.Capacity) throw new Win32Exception(Marshal.GetLastWin32Error());
   string result=buffer.ToString();
   if(result.StartsWith(@"\\?\UNC\",StringComparison.OrdinalIgnoreCase)) return @"\\"+result.Substring(8);
   if(result.StartsWith(@"\\?\",StringComparison.OrdinalIgnoreCase)) return result.Substring(4);
   return result;
  }
 }
}}
'@
}

function Get-FinalizeExistingWindowsIdentity([string] $Path, [string] $Label = 'Path') {
	$full = Get-FinalizeRootPreservingFullPath $Path
	if (-not (Test-Path -LiteralPath $full)) { throw "$Label does not exist: '$full'." }
	try {
		return Get-FinalizeRootPreservingFullPath ([BrokenEngine.FinalizeWorkflowPathIdentity]::Resolve($full))
	}
	catch {
		throw "Unable to resolve $Label identity '$full': $($_.Exception.Message)"
	}
}

function Test-FinalizeExistingIdentityEqual([string] $Candidate, [string] $Expected) {
	try {
		if (-not (Test-Path -LiteralPath (Get-FinalizeRootPreservingFullPath $Candidate))) { return $false }
		return (Get-FinalizeExistingWindowsIdentity $Candidate 'Registered worktree').Equals($Expected, [StringComparison]::OrdinalIgnoreCase)
	}
	catch {
		return $false
	}
}

function Invoke-FinalizeNativeText([string] $Executable, [string[]] $Arguments, [string] $WorkingDirectory) {
	$start = [Diagnostics.ProcessStartInfo]::new()
	$start.FileName = $Executable
	$start.WorkingDirectory = $WorkingDirectory
	$start.UseShellExecute = $false
	$start.CreateNoWindow = $true
	$start.RedirectStandardOutput = $true
	$start.RedirectStandardError = $true
	$start.StandardOutputEncoding = $script:FinalizeUtf8
	$start.StandardErrorEncoding = $script:FinalizeUtf8
	foreach ($argument in $Arguments) { [void] $start.ArgumentList.Add($argument) }
	$process = [Diagnostics.Process]::new()
	$process.StartInfo = $start
	if (-not $process.Start()) { throw "Could not start '$Executable'." }
	$stdoutTask = $process.StandardOutput.ReadToEndAsync()
	$stderrTask = $process.StandardError.ReadToEndAsync()
	$process.WaitForExit()
	$result = [pscustomobject]@{
		ExitCode = $process.ExitCode
		Stdout = $stdoutTask.GetAwaiter().GetResult()
		Stderr = $stderrTask.GetAwaiter().GetResult()
	}
	$process.Dispose()
	return $result
}

function Invoke-FinalizeGit([string] $Worktree, [string[]] $Arguments) {
	$response = Invoke-FinalizeNativeText 'git.exe' (@('-C', $Worktree) + $Arguments) $Worktree
	if ($response.ExitCode -ne 0) { throw "git $($Arguments -join ' ') failed: $($response.Stderr.Trim())." }
	return $response.Stdout
}

function Test-FinalizeGitSuccess([string] $Worktree, [string[]] $Arguments) {
	return (Invoke-FinalizeNativeText 'git.exe' (@('-C', $Worktree) + $Arguments) $Worktree).ExitCode -eq 0
}

function Get-FinalizeGitIdentity([string] $Worktree, [string] $Label = 'Worktree') {
	$identity = Get-FinalizeExistingWindowsIdentity $Worktree $Label
	$topLevel = Get-FinalizeExistingWindowsIdentity ((Invoke-FinalizeGit $identity @('rev-parse', '--show-toplevel')).Trim()) "$Label Git top-level"
	if (-not $identity.Equals($topLevel, [StringComparison]::OrdinalIgnoreCase)) {
		throw "$Label is not a Git top-level: '$identity'."
	}
	$common = Get-FinalizeExistingWindowsIdentity ((Invoke-FinalizeGit $identity @('rev-parse', '--path-format=absolute', '--git-common-dir')).Trim()) "$Label Git common directory"
	$branch = (Invoke-FinalizeGit $identity @('branch', '--show-current')).Trim()
	$head = (Invoke-FinalizeGit $identity @('rev-parse', 'HEAD')).Trim()
	if ([string]::IsNullOrWhiteSpace($branch) -or $head -cnotmatch '^[0-9a-f]{40}$') {
		throw "$Label has an unattached branch or malformed HEAD."
	}
	return [pscustomobject]@{ Worktree = $identity; CommonDirectory = $common; Branch = $branch; Head = $head }
}

function Assert-FinalizeGitPath([string] $Path) {
	if ([string]::IsNullOrEmpty($Path) -or $Path.IndexOf("`0", [StringComparison]::Ordinal) -ge 0 -or
		$Path.IndexOf("`r", [StringComparison]::Ordinal) -ge 0 -or $Path.IndexOf("`n", [StringComparison]::Ordinal) -ge 0 -or
		$Path.Contains('\', [StringComparison]::Ordinal) -or $Path.StartsWith('/', [StringComparison]::Ordinal) -or
		$Path -match '^[A-Za-z]:' -or [IO.Path]::IsPathRooted($Path)) {
		throw "Git path is not canonical repository-relative text: '$Path'."
	}
	foreach ($component in $Path.Split('/')) {
		if ([string]::IsNullOrEmpty($component) -or $component -ceq '.' -or $component -ceq '..') {
			throw "Git path contains an empty or traversing component: '$Path'."
		}
	}
}

function Get-FinalizeWorktreeRecords([string] $Worktree) {
	$records = [Collections.Generic.List[object]]::new()
	$current = $null
	$output = Invoke-FinalizeGit $Worktree @('worktree', 'list', '--porcelain', '-z')
	foreach ($field in $output.Split([char]0, [StringSplitOptions]::RemoveEmptyEntries)) {
		if ($field.StartsWith('worktree ', [StringComparison]::Ordinal)) {
			if ($null -ne $current) { $records.Add([pscustomobject] $current) }
			$current = [ordered]@{ Path = $field.Substring(9); Head = $null; Branch = $null; Prunable = $false; Bare = $false }
		}
		elseif ($null -ne $current -and $field.StartsWith('HEAD ', [StringComparison]::Ordinal)) { $current.Head = $field.Substring(5) }
		elseif ($null -ne $current -and $field.StartsWith('branch refs/heads/', [StringComparison]::Ordinal)) { $current.Branch = $field.Substring(18) }
		elseif ($null -ne $current -and $field.StartsWith('prunable', [StringComparison]::Ordinal)) { $current.Prunable = $true }
		elseif ($null -ne $current -and $field -ceq 'bare') { $current.Bare = $true }
	}
	if ($null -ne $current) { $records.Add([pscustomobject] $current) }
	return $records.ToArray()
}

function Test-FinalizeWorktreeRegistration([string] $RepositoryWorktree, [string] $ExpectedWorktree, [string] $ExpectedBranch, [string] $ExpectedHead) {
	$expectedIdentity = Get-FinalizeExistingWindowsIdentity $ExpectedWorktree 'Expected worktree'
	$records = @(Get-FinalizeWorktreeRecords $RepositoryWorktree | Where-Object {
		-not $_.Prunable -and -not $_.Bare -and (Test-FinalizeExistingIdentityEqual $_.Path $expectedIdentity)
	})
	if ($records.Count -ne 1) { return [pscustomobject]@{ Registered = $false; Message = 'Worktree is not uniquely registered by canonical identity.' } }
	$record = $records[0]
	if ($record.Head -cne $ExpectedHead -or $record.Branch -cne $ExpectedBranch) {
		return [pscustomobject]@{ Registered = $false; Message = 'Registered worktree branch or HEAD differs from the expected identity.' }
	}
	return [pscustomobject]@{ Registered = $true; Message = 'Worktree registration matches canonical identity.' }
}

function Test-FinalizeAllWorktreesClear([string] $RepositoryWorktree) {
	$problems = [Collections.Generic.List[string]]::new()
	$inspected = [Collections.Generic.List[string]]::new()
	foreach ($record in @(Get-FinalizeWorktreeRecords $RepositoryWorktree)) {
		if ($record.Prunable -or $record.Bare) { continue }
		try {
			$worktree = Get-FinalizeExistingWindowsIdentity $record.Path 'Registered worktree'
			$inspected.Add($worktree)
			foreach ($marker in @('MERGE_HEAD', 'rebase-merge', 'rebase-apply', 'CHERRY_PICK_HEAD', 'REVERT_HEAD', 'BISECT_LOG', 'sequencer')) {
				$markerPath = (Invoke-FinalizeGit $worktree @('rev-parse', '--path-format=absolute', '--git-path', $marker)).Trim()
				if (Test-Path -LiteralPath $markerPath) { $problems.Add("${worktree}: active Git marker $marker") }
			}
		}
		catch {
			$problems.Add("$($record.Path): unreadable ($($_.Exception.Message))")
		}
	}
	return [pscustomobject]@{ Clear = $problems.Count -eq 0; Inspected = $inspected.ToArray(); Problems = $problems.ToArray() }
}

function New-FinalizeLandingSanityResult([bool] $Ok, [string] $Code, [string] $Message, $Session, $Primary, [string] $WorktreeCliExecutable) {
	return [pscustomobject]@{
		Ok = $Ok
		Code = $Code
		Message = $Message
		SessionWorktree = $(if ($null -ne $Session) { $Session.Worktree } else { $null })
		PrimaryWorktree = $(if ($null -ne $Primary) { $Primary.Worktree } else { $null })
		GitCommonDirectory = $(if ($null -ne $Session) { $Session.CommonDirectory } else { $null })
		SessionTip = $(if ($null -ne $Session) { $Session.Head } else { $null })
		PrimaryTip = $(if ($null -ne $Primary) { $Primary.Head } else { $null })
		WorktreeCliExecutable = $WorktreeCliExecutable
	}
}

# Structural sanity shared by approval preparation and landing: two Git top-levels of one
# repository on the expected branches and tips, no Git operation in progress, clean trees,
# and the canonical WorktreeCli the session reaches through its primary Output link.
function Test-FinalizeLandingSanity {
	[CmdletBinding()] param(
		[Parameter(Mandatory)][string] $SessionWorktree,
		[Parameter(Mandatory)][string] $PrimaryWorktree,
		[Parameter(Mandatory)][string] $SessionBranch,
		[Parameter(Mandatory)][string] $PrimaryBranch,
		[string] $ExpectedSessionTip,
		[string] $ExpectedPrimaryTip
	)
	$session = $null
	$primary = $null
	try {
		$session = Get-FinalizeGitIdentity $SessionWorktree 'Session worktree'
		$primary = Get-FinalizeGitIdentity $PrimaryWorktree 'Primary worktree'
	}
	catch { return New-FinalizeLandingSanityResult $false 'identity.unreadable' $_.Exception.Message $session $primary $null }
	if (-not $session.CommonDirectory.Equals($primary.CommonDirectory, [StringComparison]::OrdinalIgnoreCase)) {
		return New-FinalizeLandingSanityResult $false 'identity.repository-mismatch' 'Session and primary worktrees do not share one Git common directory.' $session $primary $null
	}
	if ($session.Worktree.Equals($primary.Worktree, [StringComparison]::OrdinalIgnoreCase)) {
		return New-FinalizeLandingSanityResult $false 'identity.session-is-primary' 'Session landing requires distinct session and primary worktrees.' $session $primary $null
	}
	if ($session.Branch -cne $SessionBranch -or $primary.Branch -cne $PrimaryBranch) {
		return New-FinalizeLandingSanityResult $false 'git.branch-mismatch' 'A checked-out branch differs from its supplied identity.' $session $primary $null
	}
	if (-not [string]::IsNullOrWhiteSpace($ExpectedSessionTip) -and $session.Head -cne $ExpectedSessionTip) {
		return New-FinalizeLandingSanityResult $false 'git.session-tip-changed' 'Session tip differs from the recorded expectation.' $session $primary $null
	}
	if (-not [string]::IsNullOrWhiteSpace($ExpectedPrimaryTip) -and $primary.Head -cne $ExpectedPrimaryTip) {
		return New-FinalizeLandingSanityResult $false 'git.primary-tip-changed' 'Primary tip differs from the recorded expectation.' $session $primary $null
	}
	$executable = $null
	try {
		foreach ($worktree in @($session.Worktree, $primary.Worktree)) {
			foreach ($marker in @('MERGE_HEAD', 'rebase-merge', 'rebase-apply', 'CHERRY_PICK_HEAD', 'REVERT_HEAD', 'BISECT_LOG', 'sequencer')) {
				$markerPath = (Invoke-FinalizeGit $worktree @('rev-parse', '--path-format=absolute', '--git-path', $marker)).Trim()
				if (Test-Path -LiteralPath $markerPath) {
					return New-FinalizeLandingSanityResult $false 'git.operation-active' "Git operation marker '$marker' is active in '$worktree'." $session $primary $null
				}
			}
		}
		# A dirty primary reading that does not persist has been observed while primary was clean,
		# most likely a background index refresh from an external Git client, so it is re-read a
		# bounded number of times instead of blocking landing. A primary still dirty on the last
		# attempt is genuinely dirty and blocks exactly as before.
		$primaryDirty = $true
		for ($attempt = 1; $attempt -le 4; $attempt++) {
			if ((Invoke-FinalizeGit $primary.Worktree @('status', '--porcelain', '-z', '--untracked-files=all')).Length -eq 0) {
				$primaryDirty = $false
				break
			}
			if ($attempt -lt 4) {
				# Callers parse stdout as JSON, so this diagnostic goes to stderr like every other one.
				[Console]::Error.WriteLine("FinalizeLandingSanity: primary read dirty; re-reading (attempt $attempt of 4).")
				Start-Sleep -Seconds 1
			}
		}
		if ($primaryDirty) {
			return New-FinalizeLandingSanityResult $false 'git.primary-dirty' 'Primary worktree is not clean.' $session $primary $null
		}
		if ((Invoke-FinalizeGit $session.Worktree @('status', '--porcelain', '-z', '--untracked-files=all')).Length -ne 0) {
			return New-FinalizeLandingSanityResult $false 'git.session-dirty' 'Session worktree has remaining staged, unstaged, or untracked status.' $session $primary $null
		}
		$relativeOutput = 'Tools\WorktreeCli\Platforms\VisualStudio2026\Output'
		$primaryOutput = Get-Item -LiteralPath (Join-Path $primary.Worktree $relativeOutput) -Force -ErrorAction Stop
		if (-not $primaryOutput.PSIsContainer -or ($primaryOutput.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
			return New-FinalizeLandingSanityResult $false 'worktreecli.primary-output-invalid' 'Primary WorktreeCli Output must be an ordinary directory.' $session $primary $null
		}
		$executable = Get-Item -LiteralPath (Join-Path $primaryOutput.FullName 'WorktreeCli.exe') -Force -ErrorAction Stop
		if ($executable.PSIsContainer -or ($executable.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 -or $executable.Length -eq 0) {
			return New-FinalizeLandingSanityResult $false 'worktreecli.executable-invalid' 'Canonical WorktreeCli.exe must be a nonempty ordinary file.' $session $primary $null
		}
		$sessionOutput = Get-Item -LiteralPath (Join-Path $session.Worktree $relativeOutput) -Force -ErrorAction Stop
		if (-not $sessionOutput.PSIsContainer -or ($sessionOutput.Attributes -band [IO.FileAttributes]::ReparsePoint) -eq 0) {
			return New-FinalizeLandingSanityResult $false 'worktreecli.session-output-not-link' 'Session WorktreeCli Output must be a directory link.' $session $primary $null
		}
		if (-not (Test-FinalizeExistingIdentityEqual $sessionOutput.FullName (Get-FinalizeExistingWindowsIdentity $primaryOutput.FullName 'Primary WorktreeCli Output'))) {
			return New-FinalizeLandingSanityResult $false 'worktreecli.session-output-wrong-target' 'Session WorktreeCli Output does not target primary WorktreeCli Output.' $session $primary $null
		}
	}
	catch { return New-FinalizeLandingSanityResult $false 'state.unreadable' $_.Exception.Message $session $primary $null }
	return New-FinalizeLandingSanityResult $true 'ok' 'Landing sanity checks passed.' $session $primary (Get-FinalizeRootPreservingFullPath $executable.FullName)
}

function New-FinalizeLandingLockClaimResult([bool] $Claimed, [string] $Code, [string] $Message, [string] $Disposition, [bool] $RequiresUserAuthority, [int] $RetryAfterMilliseconds, [string] $Owner, $Lock, [int] $Attempts) {
	return [pscustomobject]@{
		Claimed = $Claimed
		Code = $Code
		Message = $Message
		Disposition = $Disposition
		RequiresUserAuthority = $RequiresUserAuthority
		RetryAfterMilliseconds = $RetryAfterMilliseconds
		Owner = $Owner
		Lock = $Lock
		Attempts = $Attempts
	}
}

function ConvertFrom-FinalizeLandingLockJson($Response, [string] $Operation) {
	if ([string]::IsNullOrWhiteSpace($Response.Stdout)) {
		throw "$Operation returned no JSON. stderr: $($Response.Stderr.Trim())"
	}
	try {
		$convertArguments = if ((Get-Command ConvertFrom-Json).Parameters.ContainsKey('DateKind')) { @{ DateKind = 'String' } } else { @{} }
		return $Response.Stdout.Trim() | ConvertFrom-Json -Depth 32 -ErrorAction Stop @convertArguments
	}
	catch {
		throw "$Operation returned invalid JSON: $($Response.Stdout.Trim())"
	}
}

function Test-FinalizeLandingLockClaimIdentity($Status, [string] $Owner, [string] $Session, [string] $Worktree) {
	if ($null -eq $Status) { return $false }
	$properties = @($Status.PSObject.Properties.Name)
	if (-not ($properties -ccontains 'owner') -or $Status.owner -isnot [string] -or $Status.owner -cne $Owner -or
		-not ($properties -ccontains 'session') -or $Status.session -isnot [string] -or $Status.session -cne $Session -or
		-not ($properties -ccontains 'worktree') -or $Status.worktree -isnot [string]) {
		return $false
	}
	return Test-FinalizeExistingIdentityEqual $Status.worktree $Worktree
}

function Get-FinalizeLandingLockState([string] $WorktreeCliExecutable, [string] $GitCommonDirectory, [string] $WorkingDirectory) {
	$response = Invoke-FinalizeNativeText $WorktreeCliExecutable @('lock', 'status', '--repo', $GitCommonDirectory) $WorkingDirectory
	$status = ConvertFrom-FinalizeLandingLockJson $response 'landing lock status'
	$properties = @($status.PSObject.Properties.Name)
	if ($response.ExitCode -eq 2 -and $properties -ccontains 'held' -and $status.held -is [bool] -and -not $status.held) {
		return [pscustomobject]@{ Kind = 'absent'; Status = $status; Response = $response; ExpiresAt = $null }
	}
	if ($response.ExitCode -ne 0 -or -not ($properties -ccontains 'held') -or $status.held -isnot [bool] -or -not $status.held) {
		return [pscustomobject]@{ Kind = 'terminal'; Status = $status; Response = $response; ExpiresAt = $null }
	}
	if (-not ($properties -ccontains 'leaseState') -or $status.leaseState -isnot [string]) {
		return [pscustomobject]@{ Kind = 'authority-required'; Status = $status; Response = $response; ExpiresAt = $null }
	}
	if ($status.leaseState -ceq 'unverifiable') {
		return [pscustomobject]@{ Kind = 'authority-required'; Status = $status; Response = $response; ExpiresAt = $null }
	}
	if ($status.leaseState -cnotin @('live', 'expired') -or -not ($properties -ccontains 'owner') -or $status.owner -isnot [string] -or [string]::IsNullOrWhiteSpace($status.owner) -or
		-not ($properties -ccontains 'expiresAt') -or $status.expiresAt -isnot [string]) {
		return [pscustomobject]@{ Kind = 'authority-required'; Status = $status; Response = $response; ExpiresAt = $null }
	}
	$expiresAt = [DateTimeOffset]::MinValue
	if (-not [DateTimeOffset]::TryParse($status.expiresAt, [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::AssumeUniversal, [ref] $expiresAt)) {
		return [pscustomobject]@{ Kind = 'authority-required'; Status = $status; Response = $response; ExpiresAt = $null }
	}
	return [pscustomobject]@{ Kind = [string]$status.leaseState; Status = $status; Response = $response; ExpiresAt = $expiresAt.ToUniversalTime() }
}

# WorktreeCli owns the bounded wait and the guarded expired-lease recovery, so this helper
# makes one blocking claim and interprets its single outcome. It is shared by reconciliation
# and post-confirmation landing so neither path can steal a live foreign lease or reinterpret
# malformed metadata as a retryable conflict: unverifiable state still requires user authority.
function Invoke-FinalizeLandingLockClaim {
	[CmdletBinding()] param(
		[Parameter(Mandatory)][string] $WorktreeCliExecutable,
		[Parameter(Mandatory)][string] $GitCommonDirectory,
		[Parameter(Mandatory)][string] $Owner,
		[Parameter(Mandatory)][string] $Session,
		[Parameter(Mandatory)][string] $Worktree,
		[ValidateRange(60, 86400)][int] $LeaseSeconds = 3600,
		[ValidateRange(1, 3600)][int] $WaitSeconds = 300,
		[ValidateRange(50, 5000)][int] $PollMilliseconds = 500
	)
	$item = Get-Item -LiteralPath $WorktreeCliExecutable -Force -ErrorAction Stop
	if ($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -or $item.Length -eq 0) {
		throw "WorktreeCli executable must be a nonempty ordinary file: '$WorktreeCliExecutable'."
	}
	if ([string]::IsNullOrWhiteSpace($GitCommonDirectory) -or [string]::IsNullOrWhiteSpace($Owner) -or [string]::IsNullOrWhiteSpace($Session) -or [string]::IsNullOrWhiteSpace($Worktree)) {
		throw 'Landing lock claim requires nonblank repository, owner, session, and worktree identities.'
	}
	$worktreeIdentity = Get-FinalizeExistingWindowsIdentity $Worktree 'Landing worktree'
	$claim = Invoke-FinalizeNativeText $item.FullName @('lock', 'claim', '--repo', $GitCommonDirectory, '--owner', $Owner, '--session', $Session, '--worktree', $worktreeIdentity, '--lease-seconds', [string]$LeaseSeconds, '--wait-seconds', [string]$WaitSeconds, '--poll-milliseconds', [string]$PollMilliseconds) $worktreeIdentity
	if ($claim.ExitCode -eq 0) {
		$status = ConvertFrom-FinalizeLandingLockJson $claim 'landing lock claim'
		$properties = @($status.PSObject.Properties.Name)
		if (($properties -ccontains 'held') -and $status.held -is [bool] -and $status.held -and
			($properties -ccontains 'leaseState') -and $status.leaseState -ceq 'live' -and
			(Test-FinalizeLandingLockClaimIdentity $status $Owner $Session $worktreeIdentity)) {
			return New-FinalizeLandingLockClaimResult $true 'ok' 'Landing lock is live and owned by this transaction.' 'terminal' $false 0 $Owner $status 1
		}
		return New-FinalizeLandingLockClaimResult $false 'landing-lock.claim-invalid' 'Landing lock claim returned an invalid ownership record.' 'terminal' $false 0 $Owner $status 1
	}
	if ($claim.ExitCode -ne 2) {
		return New-FinalizeLandingLockClaimResult $false 'landing-lock.claim-failed' "Landing lock claim failed: $($claim.Stdout.Trim())$($claim.Stderr.Trim())" 'terminal' $false 0 $Owner $null 1
	}

	$state = Get-FinalizeLandingLockState $item.FullName $GitCommonDirectory $worktreeIdentity
	if ($state.Kind -ceq 'terminal') {
		return New-FinalizeLandingLockClaimResult $false 'landing-lock.status-failed' "Landing lock status is not a recognized absence or lease state: $($state.Response.Stdout.Trim())$($state.Response.Stderr.Trim())" 'terminal' $false 0 $Owner $state.Status 1
	}
	if ($state.Kind -ceq 'authority-required') {
		return New-FinalizeLandingLockClaimResult $false 'landing-lock.unverifiable' 'Landing lock metadata cannot be verified; external repair authority is required.' 'authority-required' $true 0 $Owner $state.Status 1
	}
	if (($state.Kind -ceq 'live') -and (Test-FinalizeLandingLockClaimIdentity $state.Status $Owner $Session $worktreeIdentity)) {
		return New-FinalizeLandingLockClaimResult $true 'ok' 'Landing lock was already live and owned by this transaction.' 'terminal' $false 0 $Owner $state.Status 1
	}
	return New-FinalizeLandingLockClaimResult $false 'landing-lock.retryable-wait' 'A foreign landing lease remains live; retry this claim after its reported expiry.' 'retryable-wait' $false $PollMilliseconds $Owner $state.Status 1
}

function Assert-FinalizeHistoryObject($Value, [string] $Name) {
	if ($null -eq $Value -or $Value -is [string] -or $Value -is [ValueType] -or $Value -is [Array]) {
		throw "$Name must be a JSON object."
	}
}

function Assert-FinalizeHistoryProperties($Value, [string[]] $Expected, [string] $Name) {
	Assert-FinalizeHistoryObject $Value $Name
	$actual = @($Value.PSObject.Properties.Name)
	$expectedSet = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
	foreach ($property in $Expected) { [void]$expectedSet.Add($property) }
	if ($actual.Count -ne $Expected.Count -or $expectedSet.Count -ne $Expected.Count) {
		throw "$Name does not have the exact v1 property set."
	}
	foreach ($property in $actual) {
		if (-not $expectedSet.Contains([string]$property)) { throw "$Name does not have the exact v1 property set." }
	}
}

function Assert-FinalizeHistoryString($Value, [string] $Name) {
	if ($Value -isnot [string] -or [string]::IsNullOrWhiteSpace($Value)) {
		throw "$Name must be a JSON string."
	}
}

function Assert-FinalizeHistoryBoolean($Value, [string] $Name) {
	if ($Value -isnot [bool]) { throw "$Name must be a JSON boolean." }
}

function Test-FinalizeHistoryNumber($Value) {
	return $null -ne $Value -and $Value -isnot [bool] -and $Value -isnot [string] -and
		($Value -is [byte] -or $Value -is [sbyte] -or $Value -is [int16] -or $Value -is [uint16] -or
			$Value -is [int32] -or $Value -is [uint32] -or $Value -is [int64] -or $Value -is [uint64] -or
			$Value -is [single] -or $Value -is [double] -or $Value -is [decimal])
}

function Assert-FinalizeHistoryInteger($Value, [string] $Name, [long] $Minimum = 0) {
	if (-not (Test-FinalizeHistoryNumber $Value) -or $Value -is [single] -or $Value -is [double] -or $Value -is [decimal]) { throw "$Name must be a JSON integer." }
	$number = [double]$Value
	if ([double]::IsNaN($number) -or [double]::IsInfinity($number) -or [Math]::Truncate($number) -ne $number -or
		$number -lt [double]$Minimum) {
		throw "$Name must be a JSON integer greater than or equal to $Minimum."
	}
	return [int64]$Value
}

function Assert-FinalizeHistoryMetric($Value, [string] $Name) {
	if (-not (Test-FinalizeHistoryNumber $Value)) { throw "$Name must be a JSON number." }
	$number = [double]$Value
	if ([double]::IsNaN($number) -or [double]::IsInfinity($number) -or $number -lt 0.0 -or $number -gt 1.0) {
		throw "$Name must be a finite JSON number in [0,1]."
	}
}

function Assert-FinalizeHistoryDigest($Value, [string] $Name) {
	Assert-FinalizeHistoryString $Value $Name
	if ($Value -cnotmatch '^[0-9a-f]{64}$') { throw "$Name must be a lowercase SHA-256 digest." }
}

function Assert-FinalizeHistoryCommit($Value, [string] $Name) {
	Assert-FinalizeHistoryString $Value $Name
	if ($Value -cnotmatch '^[0-9a-f]{40}$') { throw "$Name must be a lowercase 40-character commit identity." }
}

function Assert-FinalizeHistoryDate($Value, [string] $Name) {
	Assert-FinalizeHistoryString $Value $Name
	if ($Value -notmatch '^\d{4}-\d{2}-\d{2}$') { throw "$Name must be a UTC calendar date in YYYY-MM-DD form." }
	$parsed = [datetime]::MinValue
	if (-not [datetime]::TryParseExact($Value, 'yyyy-MM-dd', [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::None, [ref]$parsed)) {
		throw "$Name must be a valid UTC calendar date."
	}
}

function Assert-FinalizeHistoryRelativePath($Value, [string] $Name, [string] $RequiredLeaf = $null) {
	Assert-FinalizeHistoryString $Value $Name
	if ($Value.IndexOf([char]0) -ge 0 -or $Value.IndexOf([char]13) -ge 0 -or $Value.IndexOf([char]10) -ge 0 -or
		$Value.Contains('\', [StringComparison]::Ordinal) -or
		$Value.StartsWith('/', [StringComparison]::Ordinal) -or $Value.Contains(':', [StringComparison]::Ordinal) -or
		[IO.Path]::IsPathRooted($Value)) { throw "$Name must be canonical repository-relative POSIX text." }
	$parts = $Value.Split('/')
	foreach ($part in $parts) {
		if ([string]::IsNullOrEmpty($part) -or $part -ceq '.' -or $part -ceq '..') { throw "$Name must not contain empty or traversing path components." }
	}
	if (-not [string]::IsNullOrEmpty($RequiredLeaf) -and [IO.Path]::GetFileName($Value) -cne $RequiredLeaf) { throw "$Name must name '$RequiredLeaf'." }
}

function Get-FinalizeHistoryJsonSha256($Value) {
	$json = $Value | ConvertTo-Json -Compress -Depth 64
	return [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.UTF8Encoding]::new($false).GetBytes($json))).ToLowerInvariant()
}

function Assert-FinalizeHistoryPrefix($Prefix, [string] $Name) {
	Assert-FinalizeHistoryProperties $Prefix @('bytes', 'lines', 'sha256') $Name
	if ((Assert-FinalizeHistoryInteger $Prefix.bytes "$Name.bytes") -ne 133323 -or
		(Assert-FinalizeHistoryInteger $Prefix.lines "$Name.lines") -ne 648) { throw "$Name does not match the immutable history prefix." }
	Assert-FinalizeHistoryDigest $Prefix.sha256 "$Name.sha256"
	if ($Prefix.sha256 -cne '5a39debf4be41abebd8496b9f25ee4023d109813788e95b30da8f74474fe75ed') { throw "$Name does not match the immutable history prefix." }
}

function Assert-FinalizeHistoryRow($Row, [string] $Name) {
	Assert-FinalizeHistoryProperties $Row @('index', 'date', 'captureMode', 'verbosity', 'structuralErosion', 'supported', 'parsed') $Name
	Assert-FinalizeHistoryInteger $Row.index "$Name.index" | Out-Null
	Assert-FinalizeHistoryDate $Row.date "$Name.date"
	Assert-FinalizeHistoryString $Row.captureMode "$Name.captureMode"
	if ($Row.captureMode -cnotin @('catch-up', 'cpp-change', 'carry-forward')) { throw "$Name.captureMode is invalid." }
	Assert-FinalizeHistoryMetric $Row.verbosity "$Name.verbosity"
	Assert-FinalizeHistoryMetric $Row.structuralErosion "$Name.structuralErosion"
	$supported = Assert-FinalizeHistoryInteger $Row.supported "$Name.supported"
	$parsed = Assert-FinalizeHistoryInteger $Row.parsed "$Name.parsed"
	if ($parsed -gt $supported) { throw "$Name.parsed must not exceed $Name.supported." }
}

function Assert-FinalizeHistoryPatch($Patch, [string] $BaseCommit, [string] $TipCommit, [string] $Name) {
	Assert-FinalizeHistoryProperties $Patch @('baseCommit', 'tipCommit', 'changes', 'metricSupportedChanges', 'cppChanged') $Name
	Assert-FinalizeHistoryCommit $Patch.baseCommit "$Name.baseCommit"
	Assert-FinalizeHistoryCommit $Patch.tipCommit "$Name.tipCommit"
	if ($Patch.baseCommit -cne $BaseCommit -or $Patch.tipCommit -cne $TipCommit) { throw "$Name commit identities do not match the requested source." }
	if ($Patch.changes -isnot [Array]) { throw "$Name.changes must be a JSON array." }
	$metricCount = 0
	foreach ($changeIndex in 0..([Math]::Max(0, $Patch.changes.Count - 1))) {
		if ($Patch.changes.Count -eq 0) { break }
		$change = $Patch.changes[$changeIndex]
		$changeName = "$Name.changes[$changeIndex]"
		Assert-FinalizeHistoryProperties $change @('status', 'path', 'oldPath', 'metricSupported') $changeName
		Assert-FinalizeHistoryString $change.status "$changeName.status"
		if ($change.status -cnotmatch '^[ACDMRTUXB]$') { throw "$changeName.status is invalid." }
		Assert-FinalizeHistoryRelativePath $change.path "$changeName.path"
		if ($null -ne $change.oldPath) {
			Assert-FinalizeHistoryRelativePath $change.oldPath "$changeName.oldPath"
			if ($change.status -notin @('R', 'C')) { throw "$changeName.oldPath is only valid for rename or copy records." }
		}
		elseif ($change.status -in @('R', 'C')) { throw "$changeName.oldPath is required for rename or copy records." }
		Assert-FinalizeHistoryBoolean $change.metricSupported "$changeName.metricSupported"
		if ($change.metricSupported) { $metricCount++ }
	}
	$declaredMetricCount = Assert-FinalizeHistoryInteger $Patch.metricSupportedChanges "$Name.metricSupportedChanges"
	Assert-FinalizeHistoryBoolean $Patch.cppChanged "$Name.cppChanged"
	if ($declaredMetricCount -ne $metricCount -or [bool]$Patch.cppChanged -ne ($metricCount -gt 0)) { throw "$Name metric-supported change counts are inconsistent." }
}

function Assert-FinalizeHistoryManifest($Manifest, [string] $Name) {
	Assert-FinalizeHistoryProperties $Manifest @('gitlinkCommit', 'resolvedHead', 'clean', 'entries') $Name
	Assert-FinalizeHistoryCommit $Manifest.gitlinkCommit "$Name.gitlinkCommit"
	Assert-FinalizeHistoryCommit $Manifest.resolvedHead "$Name.resolvedHead"
	Assert-FinalizeHistoryBoolean $Manifest.clean "$Name.clean"
	if (-not $Manifest.clean -or $Manifest.entries -isnot [Array] -or $Manifest.entries.Count -eq 0) { throw "$Name must be clean and contain file entries." }
	$previousPath = $null
	foreach ($entryIndex in 0..($Manifest.entries.Count - 1)) {
		$entry = $Manifest.entries[$entryIndex]
		$entryName = "$Name.entries[$entryIndex]"
		Assert-FinalizeHistoryProperties $entry @('relativePath', 'gitMode', 'type', 'length', 'rawSha256') $entryName
		Assert-FinalizeHistoryRelativePath $entry.relativePath "$entryName.relativePath"
		if ($null -ne $previousPath -and [StringComparer]::Ordinal.Compare($previousPath, [string]$entry.relativePath) -ge 0) { throw "$Name.entries must be unique and ordinally sorted." }
		$previousPath = [string]$entry.relativePath
		Assert-FinalizeHistoryString $entry.gitMode "$entryName.gitMode"
		if ($entry.gitMode -notin @('100644', '100755')) { throw "$entryName.gitMode is invalid." }
		Assert-FinalizeHistoryString $entry.type "$entryName.type"
		if ($entry.type -cne 'file') { throw "$entryName.type is invalid." }
		Assert-FinalizeHistoryInteger $entry.length "$entryName.length" | Out-Null
		Assert-FinalizeHistoryDigest $entry.rawSha256 "$entryName.rawSha256"
	}
}

function Assert-FinalizeHistoryCapture($Capture, [string] $Mode, [string] $Name) {
	if ($Mode -ceq 'carry-forward') {
		if ($null -ne $Capture) { throw "$Name must be null for carry-forward." }
		return
	}
	if ($null -eq $Capture) { throw "$Name is required for an active capture." }
	Assert-FinalizeHistoryProperties $Capture @('digest', 'bootstrapIdentityDigest', 'scbContentDigest', 'manifest', 'manifestDigest') $Name
	foreach ($field in @('digest', 'bootstrapIdentityDigest', 'scbContentDigest', 'manifestDigest')) { Assert-FinalizeHistoryDigest $Capture.$field "$Name.$field" }
	Assert-FinalizeHistoryManifest $Capture.manifest "$Name.manifest"
	if ((Get-FinalizeHistoryJsonSha256 $Capture.manifest) -cne $Capture.manifestDigest) { throw "$Name.manifestDigest does not match its manifest." }
}

function Assert-FinalizeHistoryCoverage($Coverage, [string] $Name) {
	if ($null -eq $Coverage) { return }
	Assert-FinalizeHistoryProperties $Coverage @('corpusCounts', 'targetCounts') $Name
	foreach ($field in @('corpusCounts', 'targetCounts')) {
		$counts = $Coverage.$field
		$countName = "$Name.$field"
		Assert-FinalizeHistoryProperties $counts @('supported', 'parsed', 'omitted') $countName
		$supported = Assert-FinalizeHistoryInteger $counts.supported "$countName.supported"
		$parsed = Assert-FinalizeHistoryInteger $counts.parsed "$countName.parsed"
		Assert-FinalizeHistoryInteger $counts.omitted "$countName.omitted" | Out-Null
		if ($parsed -gt $supported) { throw "$countName.parsed must not exceed $countName.supported." }
	}
}

function Assert-FinalizeHistoryContractReceipt($Receipt, [string] $BaseCommit, [string] $TipCommit) {
	Assert-FinalizeHistoryProperties $Receipt @('schemaVersion', 'mode', 'source', 'prefix', 'series', 'patch', 'decision', 'generator', 'capture', 'snapshot') 'History Contract receipt'
	Assert-FinalizeHistoryString $Receipt.schemaVersion 'History Contract receipt.schemaVersion'
	Assert-FinalizeHistoryString $Receipt.mode 'History Contract receipt.mode'
	if ($Receipt.schemaVersion -cne 'broken-engine-code-quality-history-contract/v1' -or $Receipt.mode -cne 'Contract') { throw 'History Contract receipt is not a broken-engine-code-quality-history-contract/v1 result.' }
	Assert-FinalizeHistoryProperties $Receipt.source @('baseCommit', 'tipCommit') 'History Contract source'
	Assert-FinalizeHistoryCommit $Receipt.source.baseCommit 'History Contract source.baseCommit'; Assert-FinalizeHistoryCommit $Receipt.source.tipCommit 'History Contract source.tipCommit'
	if ($Receipt.source.baseCommit -cne $BaseCommit -or $Receipt.source.tipCommit -cne $TipCommit) { throw 'History Contract source identities do not match the requested commits.' }
	Assert-FinalizeHistoryPrefix $Receipt.prefix 'History Contract prefix'
	Assert-FinalizeHistoryProperties $Receipt.series @('rows', 'liveRows', 'lastIndex', 'lastDate', 'historyBytesSha256') 'History Contract series'
	$rows = Assert-FinalizeHistoryInteger $Receipt.series.rows 'History Contract series.rows' 1
	$liveRows = Assert-FinalizeHistoryInteger $Receipt.series.liveRows 'History Contract series.liveRows'
	$lastIndex = Assert-FinalizeHistoryInteger $Receipt.series.lastIndex 'History Contract series.lastIndex'
	if ($liveRows -gt $rows -or $lastIndex -ne $rows - 1) { throw 'History Contract series counts are inconsistent.' }
	Assert-FinalizeHistoryDate $Receipt.series.lastDate 'History Contract series.lastDate'; Assert-FinalizeHistoryDigest $Receipt.series.historyBytesSha256 'History Contract series.historyBytesSha256'
	Assert-FinalizeHistoryPatch $Receipt.patch $BaseCommit $TipCommit 'History Contract patch'
	Assert-FinalizeHistoryProperties $Receipt.decision @('captureMode', 'reason', 'forceSnapshot') 'History Contract decision'
	Assert-FinalizeHistoryString $Receipt.decision.captureMode 'History Contract decision.captureMode'
	if ($Receipt.decision.captureMode -notin @('catch-up', 'cpp-change', 'carry-forward')) { throw 'History Contract decision.captureMode is invalid.' }
	Assert-FinalizeHistoryString $Receipt.decision.reason 'History Contract decision.reason'
	$reasons = @{ 'catch-up' = 'no-live-suffix'; 'cpp-change' = 'metric-supported-cpp-change'; 'carry-forward' = 'no-metric-supported-cpp-change' }
	if ($Receipt.decision.reason -cne $reasons[[string]$Receipt.decision.captureMode]) { throw 'History Contract decision.reason is inconsistent with captureMode.' }
	Assert-FinalizeHistoryBoolean $Receipt.decision.forceSnapshot 'History Contract decision.forceSnapshot'
	if ([bool]$Receipt.decision.forceSnapshot -ne ([string]$Receipt.decision.captureMode -ne 'carry-forward')) { throw 'History Contract decision.forceSnapshot is inconsistent with captureMode.' }
	Assert-FinalizeHistoryProperties $Receipt.generator @('relativePath', 'sha256') 'History Contract generator'
	Assert-FinalizeHistoryRelativePath $Receipt.generator.relativePath 'History Contract generator.relativePath' 'Invoke-CodeQualityMetricsHistory.ps1'; Assert-FinalizeHistoryDigest $Receipt.generator.sha256 'History Contract generator.sha256'
	Assert-FinalizeHistoryCapture $Receipt.capture ([string]$Receipt.decision.captureMode) 'History Contract capture'
	if ([string]$Receipt.decision.captureMode -eq 'carry-forward') {
		if ($null -ne $Receipt.snapshot) { throw 'History Contract snapshot must be null for carry-forward.' }
	}
	else {
		Assert-FinalizeHistoryProperties $Receipt.snapshot @('target', 'scope', 'coverageRequired') 'History Contract snapshot'
		Assert-FinalizeHistoryString $Receipt.snapshot.target 'History Contract snapshot.target'; Assert-FinalizeHistoryString $Receipt.snapshot.scope 'History Contract snapshot.scope'; Assert-FinalizeHistoryBoolean $Receipt.snapshot.coverageRequired 'History Contract snapshot.coverageRequired'
		if ($Receipt.snapshot.target -cne 'Engine/Source' -or $Receipt.snapshot.scope -cne 'Recursive' -or -not $Receipt.snapshot.coverageRequired) { throw 'History Contract snapshot does not describe the required coverage.' }
	}
	return $Receipt
}

function Assert-FinalizeHistoryArtifact($Artifact, [string] $Name, [string] $Leaf) {
	Assert-FinalizeHistoryProperties $Artifact @('path', 'bytes', 'sha256') $Name
	Assert-FinalizeHistoryRelativePath $Artifact.path "$Name.path" $Leaf
	if (-not ([string]$Artifact.path).StartsWith('Temp/', [StringComparison]::Ordinal)) { throw "$Name.path must be beneath repository Temp." }
	Assert-FinalizeHistoryInteger $Artifact.bytes "$Name.bytes" | Out-Null; Assert-FinalizeHistoryDigest $Artifact.sha256 "$Name.sha256"
}

function Assert-FinalizeHistoryUpdateReceipt($Receipt, [string] $BaseCommit, [string] $TipCommit) {
	Assert-FinalizeHistoryProperties $Receipt @('schemaVersion', 'mode', 'date', 'captureMode', 'source', 'prefix', 'patch', 'generator', 'capture', 'series', 'outputs') 'History Generate receipt'
	Assert-FinalizeHistoryString $Receipt.schemaVersion 'History Generate receipt.schemaVersion'; Assert-FinalizeHistoryString $Receipt.mode 'History Generate receipt.mode'
	if ($Receipt.schemaVersion -cne 'broken-engine-code-quality-history-update/v1' -or $Receipt.mode -cne 'Generate') { throw 'History Generate receipt is not a broken-engine-code-quality-history-update/v1 result.' }
	Assert-FinalizeHistoryDate $Receipt.date 'History Generate receipt.date'; Assert-FinalizeHistoryString $Receipt.captureMode 'History Generate receipt.captureMode'
	if ($Receipt.captureMode -notin @('catch-up', 'cpp-change', 'carry-forward')) { throw 'History Generate receipt.captureMode is invalid.' }
	Assert-FinalizeHistoryProperties $Receipt.source @('baseCommit', 'tipCommit') 'History Generate source'
	Assert-FinalizeHistoryCommit $Receipt.source.baseCommit 'History Generate source.baseCommit'; Assert-FinalizeHistoryCommit $Receipt.source.tipCommit 'History Generate source.tipCommit'
	if ($Receipt.source.baseCommit -cne $BaseCommit -or $Receipt.source.tipCommit -cne $TipCommit) { throw 'History Generate source identities do not match the requested commits.' }
	Assert-FinalizeHistoryPrefix $Receipt.prefix 'History Generate prefix'
	Assert-FinalizeHistoryPatch $Receipt.patch $BaseCommit $TipCommit 'History Generate patch'
	Assert-FinalizeHistoryProperties $Receipt.generator @('relativePath', 'sha256') 'History Generate generator'
	Assert-FinalizeHistoryRelativePath $Receipt.generator.relativePath 'History Generate generator.relativePath' 'Invoke-CodeQualityMetricsHistory.ps1'; Assert-FinalizeHistoryDigest $Receipt.generator.sha256 'History Generate generator.sha256'
	Assert-FinalizeHistoryCapture $Receipt.capture ([string]$Receipt.captureMode) 'History Generate capture'
	Assert-FinalizeHistoryProperties $Receipt.series @('index', 'digest', 'historyBytesSha256', 'row', 'coverage') 'History Generate series'
	$index = Assert-FinalizeHistoryInteger $Receipt.series.index 'History Generate series.index'
	Assert-FinalizeHistoryDigest $Receipt.series.digest 'History Generate series.digest'; Assert-FinalizeHistoryDigest $Receipt.series.historyBytesSha256 'History Generate series.historyBytesSha256'
	Assert-FinalizeHistoryRow $Receipt.series.row 'History Generate series.row'
	if ($Receipt.series.row.index -ne $index -or $Receipt.series.row.date -cne $Receipt.date -or $Receipt.series.row.captureMode -cne $Receipt.captureMode) { throw 'History Generate series row does not match the receipt date, index, or capture mode.' }
	if ($Receipt.captureMode -eq 'carry-forward' -and $null -ne $Receipt.series.coverage) { throw 'History Generate coverage must be null for carry-forward.' }
	if ($Receipt.captureMode -ne 'carry-forward' -and $null -eq $Receipt.series.coverage) { throw 'History Generate coverage is required for an active capture.' }
	Assert-FinalizeHistoryCoverage $Receipt.series.coverage 'History Generate series.coverage'
	Assert-FinalizeHistoryProperties $Receipt.outputs @('jsonl', 'svg') 'History Generate outputs'
	Assert-FinalizeHistoryArtifact $Receipt.outputs.jsonl 'History Generate outputs.jsonl' 'CodeQualityMetricsHistory.jsonl'; Assert-FinalizeHistoryArtifact $Receipt.outputs.svg 'History Generate outputs.svg' 'CodeQualityMetricsHistory.svg'
	return $Receipt
}

# Scratch-fixture helpers shared by the finalize-changes suites. Assert-SafeScratchRoot gates every
# recursive scratch delete: a root only passes when it is the expected GUID leaf directly under the
# expected fixture parent.
function Assert-SafeScratchRoot([string] $Parent, [string] $Root, [string] $ExpectedLeaf) {
	$parentPath = [IO.Path]::GetFullPath($Parent).TrimEnd('\', '/')
	$rootPath = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
	if ((Split-Path -Parent $rootPath) -cne $parentPath -or (Split-Path -Leaf $rootPath) -cne $ExpectedLeaf -or $ExpectedLeaf -cnotmatch '^[0-9a-f]{32}$') {
		throw "Fixture scratch root failed containment validation: '$rootPath'."
	}
	return $rootPath
}

function Invoke-ScratchGit([string] $Root, [string[]] $Arguments) {
	$output = @(& git -C $Root -c user.name=fixture -c user.email=fixture@example.com @Arguments 2>&1)
	if ($LASTEXITCODE -ne 0) { throw "git $($Arguments -join ' ') failed: $($output -join '; ')" }
	return $output
}

Export-ModuleMember -Function Assert-SafeScratchRoot, Invoke-ScratchGit, Get-FinalizeRootPreservingFullPath, Get-FinalizeExistingWindowsIdentity, Test-FinalizeExistingIdentityEqual, Invoke-FinalizeNativeText, Invoke-FinalizeGit, Test-FinalizeGitSuccess, Get-FinalizeGitIdentity, Assert-FinalizeGitPath, Get-FinalizeWorktreeRecords, Test-FinalizeWorktreeRegistration, Test-FinalizeAllWorktreesClear, Test-FinalizeLandingSanity, Test-FinalizeLandingLockClaimIdentity, Get-FinalizeLandingLockState, Invoke-FinalizeLandingLockClaim, Assert-FinalizeHistoryRow, Assert-FinalizeHistoryContractReceipt, Assert-FinalizeHistoryUpdateReceipt
