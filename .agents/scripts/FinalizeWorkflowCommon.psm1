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

function Test-FinalizeAllWorktreesClear([string] $RepositoryWorktree, [int] $WaitSeconds = 0) {
	$budgetMilliseconds = $WaitSeconds * 1000
	$clock = [Diagnostics.Stopwatch]::StartNew()
	while ($true) {
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
		$remaining = $budgetMilliseconds - $clock.Elapsed.TotalMilliseconds
		if ($problems.Count -eq 0 -or $remaining -le 0) { break }
		# A coarse poll: each inspection spawns several git rev-parse processes per worktree. The last
		# sleep is clamped to what the budget still allows, so the inspection it pays for still runs.
		Start-Sleep -Milliseconds ([int][Math]::Min(5000, [Math]::Ceiling($remaining)))
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

function Get-FinalizeActiveGitOperationMarker([string] $Repository) {
	foreach ($marker in @('MERGE_HEAD', 'rebase-merge', 'rebase-apply', 'CHERRY_PICK_HEAD', 'REVERT_HEAD', 'BISECT_LOG', 'sequencer')) {
		$markerPath = (Invoke-FinalizeGit $Repository @('rev-parse', '--path-format=absolute', '--git-path', $marker)).Trim()
		if (Test-Path -LiteralPath $markerPath) { return $marker }
	}
	return $null
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
			$activeMarker = Get-FinalizeActiveGitOperationMarker $worktree
			if ($null -ne $activeMarker) {
				return New-FinalizeLandingSanityResult $false 'git.operation-active' "Git operation marker '$activeMarker' is active in '$worktree'." $session $primary $null
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

# The approval review receipt gate: null when the 'broken-engine-finalize-approval-review/v1' receipt at Path records
# an attempted SmartGit launch for ApprovedCommit, the full commit being landed; otherwise its blocking Code and Message.
# SmartGit outcomes stay non-blocking; malformed, mismatched, or non-attempted receipts block.
function Test-FinalizeApprovalReviewReceipt([string] $Path, [string] $ApprovedCommit) {
	$rerun = 'Rerun Show-FinalizeApprovalReview.ps1 for the approved commit with the documented stdout redirect so the receipt is overwritten.'
	if ([string]::IsNullOrWhiteSpace($Path)) {
		return [pscustomobject]@{ Code = 'approval-review.missing'; Message = 'A broken-engine-finalize-approval-review/v1 receipt is required for postconfirmation landing.' }
	}
	$reviewItem = Get-Item -LiteralPath $Path -Force -ErrorAction SilentlyContinue
	if ($null -eq $reviewItem -or $reviewItem.PSIsContainer) { return [pscustomobject]@{ Code = 'approval-review.missing'; Message = "Approval review receipt '$Path' is not a readable file." } }
	try { $reviewResult = [IO.File]::ReadAllText($reviewItem.FullName) | ConvertFrom-Json -Depth 64 -ErrorAction Stop }
	catch { return [pscustomobject]@{ Code = 'approval-review.missing'; Message = "Approval review receipt '$Path' is not valid JSON: $($_.Exception.Message)" } }
	if ($null -eq $reviewResult) { return [pscustomobject]@{ Code = 'approval-review.missing'; Message = "Approval review receipt '$Path' carries no result object." } }
	$reviewProperties = @($reviewResult.PSObject.Properties.Name)
	if ($reviewProperties -cnotcontains 'schemaVersion' -or $reviewProperties -cnotcontains 'status' -or $reviewProperties -cnotcontains 'approvedTip' -or [string]$reviewResult.schemaVersion -cne 'broken-engine-finalize-approval-review/v1') {
		return [pscustomobject]@{ Code = 'approval-review.missing'; Message = "Approval review receipt '$Path' must be a 'broken-engine-finalize-approval-review/v1' result carrying status and approvedTip." }
	}
	if ([string]$reviewResult.approvedTip -cne $ApprovedCommit) { return [pscustomobject]@{ Code = 'approval-review.candidate-mismatch'; Message = "Approval review receipt '$Path' records a different reviewed commit than the approved commit. $rerun" } }
	if ([string]$reviewResult.status -cnotin @('opened', 'unavailable', 'failed')) { return [pscustomobject]@{ Code = 'approval-review.not-launched'; Message = "Approval review receipt '$Path' does not record an attempted SmartGit launch for the approved commit. $rerun" } }
	return $null
}

function Start-FinalizeGitProcess([string] $WorkingDirectory, [string[]] $Arguments, [bool] $RedirectInput) {
	$start = [Diagnostics.ProcessStartInfo]::new()
	$start.FileName = 'git.exe'
	$start.WorkingDirectory = $WorkingDirectory
	$start.UseShellExecute = $false
	$start.CreateNoWindow = $true
	$start.RedirectStandardOutput = $true
	$start.RedirectStandardError = $true
	$start.RedirectStandardInput = $RedirectInput
	foreach ($argument in $Arguments) { [void] $start.ArgumentList.Add($argument) }
	$process = [Diagnostics.Process]::new()
	$process.StartInfo = $start
	if (-not $process.Start()) {
		$process.Dispose()
		throw "Could not start 'git.exe'."
	}
	return $process
}

# Patch text reaches git patch-id as the exact bytes git produced: the identity is compared across
# the rebase to decide whether the landing bytes are still the confirmed ones, so no text decoding
# may sit between the two commands. --verbatim keeps whitespace and patch bytes significant, so a
# commit differing from the confirmed one only in whitespace cannot pass as byte-identical; it
# implies --stable, so hunk line numbers, file order, and text diff index headers stay ignored and
# a rebase over another hunk still compares equal.
function Get-FinalizePatchId([string] $Repository, [string] $Base, [string] $Tip, [string[]] $Pathspec = @()) {
	$patch = [IO.MemoryStream]::new()
	$diff = $null
	$identify = $null
	$diffErrorTask = $null
	$identifyOutTask = $null
	$identifyErrorTask = $null
	try {
		$diffArguments = @('-C', $Repository, 'diff-tree', '-p', '--no-color', '--no-ext-diff', '--full-index', '--binary', '--no-renames', '-r', $Base, $Tip)
		if (@($Pathspec).Count -gt 0) { $diffArguments += @('--'); $diffArguments += @($Pathspec) }
		$diff = Start-FinalizeGitProcess $Repository $diffArguments $false
		$diffErrorTask = $diff.StandardError.ReadToEndAsync()
		$diff.StandardOutput.BaseStream.CopyTo($patch)
		$diffError = $diffErrorTask.GetAwaiter().GetResult()
		$diff.WaitForExit()
		if ($diff.ExitCode -ne 0) { throw "git diff-tree failed for the landing patch: $($diffError.Trim())" }
		$identify = Start-FinalizeGitProcess $Repository @('patch-id', '--verbatim') $true
		$identifyOutTask = $identify.StandardOutput.ReadToEndAsync()
		$identifyErrorTask = $identify.StandardError.ReadToEndAsync()
		$patch.Position = 0
		$patch.CopyTo($identify.StandardInput.BaseStream)
		$identify.StandardInput.BaseStream.Flush()
		$identify.StandardInput.Close()
		$identifyOut = $identifyOutTask.GetAwaiter().GetResult()
		$identifyError = $identifyErrorTask.GetAwaiter().GetResult()
		$identify.WaitForExit()
		if ($identify.ExitCode -ne 0) { throw "git patch-id failed for the landing patch: $($identifyError.Trim())" }
		$identityValue = $identifyOut.Trim().Split(' ')[0]
		if ([string]::IsNullOrWhiteSpace($identityValue)) { return '(empty)' }
		return $identityValue
	}
	finally {
		foreach ($entry in @(
			@{ Process = $identify; HasInput = $true; Tasks = @($identifyOutTask, $identifyErrorTask) }
			@{ Process = $diff; HasInput = $false; Tasks = @($diffErrorTask) }
		)) {
			$process = $entry.Process
			if ($null -eq $process) { continue }
			try {
				if (-not $process.HasExited) { $process.Kill($true) }
			}
			catch {}
			if ($entry.HasInput) {
				try { $process.StandardInput.Dispose() } catch {}
			}
			try { $process.StandardOutput.Dispose() } catch {}
			try { $process.StandardError.Dispose() } catch {}
			try { $null = $process.WaitForExit(5000) } catch {}
			foreach ($task in $entry.Tasks) {
				if ($null -ne $task) {
					try { $null = $task.Wait(5000) } catch {}
				}
			}
			try { $process.Dispose() } catch {}
		}
		$patch.Dispose()
	}
}

# Path, mode, and status of every changed file, with the blob object IDs deliberately dropped: an
# upstream commit touching another hunk of the same file changes the pre-image ID without changing
# what this patch does, and content — including binary blobs — is already covered by patch-id.
function Get-FinalizeChangeGuard([string] $Repository, [string] $Base, [string] $Tip) {
	$fields = (Invoke-FinalizeGit $Repository @('diff', '--raw', '--no-renames', '-z', $Base, $Tip)).Split([char]0, [StringSplitOptions]::RemoveEmptyEntries)
	if ($fields.Count % 2 -ne 0) { throw 'git diff --raw produced an incomplete record for the landing patch.' }
	$records = [Collections.Generic.List[string]]::new()
	for ($index = 0; $index -lt $fields.Count; $index += 2) {
		$parts = $fields[$index].Split(' ')
		if ($parts.Count -ne 5) { throw "git diff --raw produced an unrecognized record: '$($fields[$index])'." }
		$records.Add("$($parts[0]) $($parts[1]) $($parts[4])`t$($fields[$index + 1])")
	}
	return $records -join "`n"
}

# A blocked retry must leave the branch exactly as confirmed, never resetting a foreign branch or
# detached checkout and returning Restored = $true only when every restoration postcondition holds.
function Restore-FinalizeBranch([string] $Repository, [string] $Branch, [string] $Commit) {
	try {
		foreach ($marker in @('rebase-merge', 'rebase-apply')) {
			# `git rebase --abort` exits 128 when no rebase is in progress, so only a started one is aborted.
			if (Test-Path -LiteralPath (Invoke-FinalizeGit $Repository @('rev-parse', '--path-format=absolute', '--git-path', $marker)).Trim()) {
				Invoke-FinalizeGit $Repository @('rebase', '--abort') | Out-Null
				break
			}
		}
		$currentBranch = (Invoke-FinalizeGit $Repository @('branch', '--show-current')).Trim()
		if ($currentBranch -cne $Branch) {
			return [pscustomobject]@{ Restored = $false; Message = "Branch '$Branch' is not checked out (found '$currentBranch'); refused to reset." }
		}
		if ((Invoke-FinalizeGit $Repository @('rev-parse', "refs/heads/$Branch")).Trim() -cne $Commit) {
			Invoke-FinalizeGit $Repository @('reset', '--hard', $Commit) | Out-Null
		}
		foreach ($marker in @('rebase-merge', 'rebase-apply')) {
			if (Test-Path -LiteralPath (Invoke-FinalizeGit $Repository @('rev-parse', '--path-format=absolute', '--git-path', $marker)).Trim()) {
				return [pscustomobject]@{ Restored = $false; Message = "Rebase marker '$marker' remained active after restoration." }
			}
		}
		if ((Invoke-FinalizeGit $Repository @('branch', '--show-current')).Trim() -cne $Branch -or
			(Invoke-FinalizeGit $Repository @('rev-parse', 'HEAD')).Trim() -cne $Commit -or
			(Invoke-FinalizeGit $Repository @('rev-parse', "refs/heads/$Branch")).Trim() -cne $Commit -or
			(Invoke-FinalizeGit $Repository @('status', '--porcelain', '-z', '--untracked-files=all')).Length -ne 0) {
			return [pscustomobject]@{ Restored = $false; Message = "The worktree could not be proven restored to commit '$Commit' on branch '$Branch'." }
		}
		return [pscustomobject]@{ Restored = $true; Message = "Branch '$Branch' was restored to commit '$Commit'." }
	}
	catch {
		return [pscustomobject]@{ Restored = $false; Message = $_.Exception.Message }
	}
}

# Only a clean rebase whose patch is provably identical to the confirmed one becomes the new
# candidate, so neither engine nor Coretide landing can advance primary with unconfirmed bytes.
function Invoke-FinalizeVerifiedRebase {
	[CmdletBinding()] param(
		[Parameter(Mandatory)][string] $Repository,
		[Parameter(Mandatory)][string] $Branch,
		[Parameter(Mandatory)][string] $Base,
		[Parameter(Mandatory)][string] $Commit,
		[Parameter(Mandatory)][string] $Target
	)
	$activeMarker = Get-FinalizeActiveGitOperationMarker $Repository
	if ($null -ne $activeMarker) {
		return [pscustomobject]@{ Code = 'git.operation-active'; Message = "Git operation marker '$activeMarker' is active in '$Repository'."; Commit = $null; Tree = $null }
	}
	$currentBranch = (Invoke-FinalizeGit $Repository @('branch', '--show-current')).Trim()
	if ($currentBranch -cne $Branch) {
		return [pscustomobject]@{ Code = 'git.branch-mismatch'; Message = "Checked-out branch '$currentBranch' does not match expected branch '$Branch'."; Commit = $null; Tree = $null }
	}
	$approvedPatchId = Get-FinalizePatchId $Repository $Base $Commit
	$approvedChanges = Get-FinalizeChangeGuard $Repository $Base $Commit
	$failureCode = $null
	$failureDetail = $null
	try {
		$rebase = Invoke-FinalizeNativeText 'git.exe' @('-C', $Repository, 'rebase', '--onto', $Target, $Base, $Branch) $Repository
		if ($rebase.ExitCode -ne 0) {
			$failureCode = 'rebase.conflicted'
			$failureDetail = "Rebasing onto the target commit did not apply cleanly: $($rebase.Stderr.Trim())"
		}
		else {
			$rebasedCommit = (Invoke-FinalizeGit $Repository @('rev-parse', "refs/heads/$Branch")).Trim()
			$rebasedPatchId = Get-FinalizePatchId $Repository $Target $rebasedCommit
			$rebasedChanges = Get-FinalizeChangeGuard $Repository $Target $rebasedCommit
			if ($rebasedPatchId -ceq $approvedPatchId -and $rebasedChanges -ceq $approvedChanges) {
				$rebasedTree = (Invoke-FinalizeGit $Repository @('rev-parse', "$rebasedCommit^{tree}")).Trim()
				return [pscustomobject]@{ Code = 'ok'; Message = 'Rebased commit matches the approved patch identity.'; Commit = $rebasedCommit; Tree = $rebasedTree }
			}
			$failureCode = 'rebase.patch-not-identical'
			$failureDetail = 'Rebasing onto the target commit did not reproduce the approved patch.'
		}
	}
	catch {
		$failureCode = 'rebase.verify-failed'
		$failureDetail = $_.Exception.Message
	}
	$restore = Restore-FinalizeBranch $Repository $Branch $Commit
	return [pscustomobject]@{
		Code = $(if ($restore.Restored) { $failureCode } else { 'rebase.abort-failed' })
		Message = "$failureDetail $($restore.Message)".Trim()
		Commit = $null
		Tree = $null
	}
}

Export-ModuleMember -Function Get-FinalizeRootPreservingFullPath, Get-FinalizeExistingWindowsIdentity, Test-FinalizeExistingIdentityEqual, Invoke-FinalizeNativeText, Invoke-FinalizeGit, Test-FinalizeGitSuccess, Get-FinalizeGitIdentity, Assert-FinalizeGitPath, Get-FinalizeWorktreeRecords, Test-FinalizeWorktreeRegistration, Test-FinalizeAllWorktreesClear, Test-FinalizeLandingSanity, Test-FinalizeLandingLockClaimIdentity, Get-FinalizeLandingLockState, Invoke-FinalizeLandingLockClaim, Test-FinalizeApprovalReviewReceipt, Get-FinalizePatchId, Get-FinalizeChangeGuard, Restore-FinalizeBranch, Invoke-FinalizeVerifiedRebase
