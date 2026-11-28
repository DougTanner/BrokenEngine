# Deterministic session change inventory module shared by the CLI wrapper, workflow scripts, and
# read-only review workers: one read-only in-process answer to "what changed between the session
# baseline and the head side, and what artifact type is each change". Writes nothing to stdout,
# stderr, or disk, and performs no JSON serialization (GIT_OPTIONAL_LOCKS=0 keeps Git from
# refreshing the index). A session Coretide clone at Projects/Coretide is inventoried by the same
# pipeline in that repository, from the merge base of its HEAD and upstream, and its rows are
# merged under that prefix; a commit-valued head whose message carries a Coretide-Snapshot line
# (a review-round snapshot) sets the Coretide head to that commit instead of the clone's HEAD,
# and its Coretide-Baseline line replaces that merge base.
# Get-ReviewRoundState and Test-ReviewRoundCoverage read the review-round chain the
# Get-ChangeWorkflowSteps.ps1 header documents.

Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'AgentScriptCommon.psm1')
Import-Module (Join-Path $PSScriptRoot 'SourceAffinity.psm1')

$script:MaximumSymbolLength = 96
$script:MaximumMessageLength = 256

# GLSL source extensions, matching the list in .agents/skills/glsl-review/SKILL.md.
$script:ShaderExtensions = @('.vert', '.frag', '.comp', '.geom', '.tesc', '.tese', '.glsl', '.mesh', '.task', '.rgen', '.rmiss', '.rchit', '.rahit', '.rint', '.rcall')
# C++ reachability follows PCH -> project ShaderLayouts.h -> engine ShaderLayoutsBase.h -> nested layout headers; ShaderLayoutsBase.h's branch-specific bridge setup precedes those includes, which are outside its BT_ENGINE branch.
$script:DualLanguageHeaders = @('shaderlayouts.h', 'shaderlayoutsbase.h', 'shadergloballayout.h', 'shadermainlayout.h')
$script:CppClasses = @('cpp', 'dual-language-header')
# The classes whose addition, deletion, rename, or type change can move a project item.
$script:SourceMembershipClasses = @('cpp', 'dual-language-header', 'glsl')
# The changed-file classes that make a landing owe its Review and resolve correctness /coherence-review row: every
# class but the C++ and GLSL ones.
$script:CoherenceReviewClasses = @('doc', 'plan', 'script', 'skill', 'vcxproj', 'binary', 'other')
# One BLOCKED acceptance row per triggered check in this set; this order is the emitted row order.
$script:AcceptanceSkeletonChecks = [ordered]@{
	coherenceReview = '/coherence-review'
	codeStyleReview = '/code-style-review'
	commentReview = '/comment-review'
	updateVcxproj = '/update-vcxproj'
	validateSkill = '/external-skill-creator validate mode'
	updateClaudeDocs = '/update-claude-docs'
	progressiveDisclosureReview = '/progressive-disclosure-review'
}
$script:ClassNames = @('dual-language-header', 'glsl', 'cpp', 'skill', 'plan', 'script', 'vcxproj', 'doc', 'binary', 'other')
$script:ManifestModes = @('100644', '100755')
$script:ManifestRoots = @('Common', 'DataPacker', 'Engine', 'Projects', 'Tools')
# Ordinal planning-root prefixes accepted by -SavePlanPaths; a rooted, UNC, or drive-qualified path
# cannot match any of these after separator normalization and removing at most one leading './'.
$script:SavePlanRoots = @(
	'Documents/Plans/',
	'Documents/Features/',
	'Documents/Investigations/',
	'Projects/Coretide/Documents/Plans/',
	'Projects/Coretide/Documents/Features/',
	'Projects/Coretide/Documents/Investigations/'
)
$script:ZeroOid = '0000000000000000000000000000000000000000'
$script:CoretidePrefix = 'Projects/Coretide/'
$script:Utf8 = [Text.UTF8Encoding]::new($false)
$script:InventoryModulePath = Join-Path $PSScriptRoot 'SessionChangeInventory.psm1'
# The per-run state Get-SessionChangeInventory sets; the review-round gate saves and restores it around its own runs.
$script:Root = $null
$script:BaselineSha = ''
$script:HeadSha = ''
$script:HeadPaths = $null
$script:Coretide = $null
$script:EngineSide = $null
$script:BlobHashes = @{}
$script:ReviewRoundRef = 'refs/worktree/review-rounds'
# The review keys in the Get-ChangeWorkflowSteps.ps1 header: the group that decides the phase, and the inventory
# trigger that owes each key. /adversarial-review is owed at Tier 3 with any trigger, /verify-acceptance by the rules.
$script:ReviewRoundKeys = [ordered]@{
	'/repo-code-review' = @{ Group = 'code'; Trigger = 'repoCodeReview' }
	'/glsl-review' = @{ Group = 'code'; Trigger = 'glslReview' }
	'/comment-review' = @{ Group = 'words'; Trigger = 'commentReview' }
	'/progressive-disclosure-review' = @{ Group = 'words'; Trigger = 'progressiveDisclosureReview' }
	'/coherence-review' = @{ Group = 'neutral'; Trigger = 'coherenceReview' }
	'/external-skill-creator' = @{ Group = 'neutral'; Trigger = 'validateSkill' }
	'/adversarial-review' = @{ Group = 'neutral'; Trigger = $null }
	'/verify-acceptance' = @{ Group = 'acceptance'; Trigger = $null }
}
# The checks a Tier-1 combined /coherence-review row carries; a combined snapshot's Owes lines name the ones it carried.
$script:CombinedCarriedKeys = @('/external-skill-creator', '/progressive-disclosure-review', '/verify-acceptance')
$script:ReviewSeverityRank = @{ none = 0; Low = 1; Medium = 2; High = 3; Critical = 4 }
# The routing triggers a proven comment-text-only modification never fires on a -Since run.
$script:CodeOnlyTriggers = @('repoCodeReview', 'overEngineeringReview', 'glslReview', 'updateAffectedCode', 'updateClaudeDocs')

function Set-InventoryOutcome([pscustomobject] $Result, [string] $Status, [string] $Code, [string] $Message) {
	$text = if ($null -eq $Message) { '' } else { $Message }
	$Result.status = $Status
	$Result.code = $Code
	$Result.message = if ($text.Length -gt $script:MaximumMessageLength) { $text.Substring(0, $script:MaximumMessageLength) } else { $text }
	return $Result
}

function Invoke-InventoryGit([string[]] $Arguments, [bool] $AllowFailure = $false) {
	$start = [Diagnostics.ProcessStartInfo]::new()
	$start.FileName = 'git'
	$start.WorkingDirectory = $script:Root
	$start.UseShellExecute = $false
	$start.CreateNoWindow = $true
	$start.RedirectStandardOutput = $true
	$start.RedirectStandardError = $true
	$start.StandardOutputEncoding = $script:Utf8
	$start.StandardErrorEncoding = $script:Utf8
	$start.Environment['GIT_OPTIONAL_LOCKS'] = '0'
	foreach ($argument in @('-C', $script:Root, '--no-pager') + $Arguments) { [void] $start.ArgumentList.Add($argument) }
	$process = [Diagnostics.Process]::new()
	$process.StartInfo = $start
	if (-not $process.Start()) { throw "Could not start git for: $($Arguments -join ' ')" }
	$stdoutTask = $process.StandardOutput.ReadToEndAsync()
	$stderrTask = $process.StandardError.ReadToEndAsync()
	$process.WaitForExit()
	$run = [pscustomobject] @{
		ExitCode = $process.ExitCode
		Stdout = $stdoutTask.GetAwaiter().GetResult()
		Stderr = $stderrTask.GetAwaiter().GetResult()
	}
	$process.Dispose()
	if (-not $AllowFailure -and $run.ExitCode -ne 0) { throw "git $($Arguments -join ' ') failed with exit $($run.ExitCode): $($run.Stderr.Trim())" }
	return $run
}

# Points every Git read and side variable at the Coretide clone, so the engine pipeline functions answer for it with
# unprefixed paths, until Exit-CoretideSide puts the engine values back. Calls never nest.
function Enter-CoretideSide() {
	$script:EngineSide = @{ Root = $script:Root; BaselineSha = $script:BaselineSha; HeadSha = $script:HeadSha; HeadPaths = $script:HeadPaths }
	$script:Root = $script:Coretide.Root
	$script:BaselineSha = $script:Coretide.BaselineSha
	$script:HeadSha = $script:Coretide.HeadSha
	$script:HeadPaths = $script:Coretide.HeadPaths
}

function Exit-CoretideSide() {
	$script:Coretide.HeadPaths = $script:HeadPaths
	$script:Root = $script:EngineSide.Root
	$script:BaselineSha = $script:EngineSide.BaselineSha
	$script:HeadSha = $script:EngineSide.HeadSha
	$script:HeadPaths = $script:EngineSide.HeadPaths
}

function Test-CoretidePath([string] $Path) {
	return $null -ne $script:Coretide -and $Path.StartsWith($script:CoretidePrefix, [StringComparison]::Ordinal)
}

function Add-CoretidePrefix([object] $Entry) {
	$Entry.Path = $script:CoretidePrefix + $Entry.Path
	if ($null -ne $Entry.OldPath) { $Entry.OldPath = $script:CoretidePrefix + $Entry.OldPath }
	return $Entry
}

function Get-InventoryHex([byte[]] $Hash) {
	return [Convert]::ToHexString($Hash).ToLowerInvariant()
}

function Get-InventoryBlobSha256([string] $Oid) {
	if ($script:BlobHashes.ContainsKey($Oid)) { return $script:BlobHashes[$Oid] }
	$start = [Diagnostics.ProcessStartInfo]::new()
	$start.FileName = 'git'
	$start.WorkingDirectory = $script:Root
	$start.UseShellExecute = $false
	$start.CreateNoWindow = $true
	$start.RedirectStandardOutput = $true
	$start.RedirectStandardError = $true
	$start.Environment['GIT_OPTIONAL_LOCKS'] = '0'
	$start.Environment['GIT_NO_LAZY_FETCH'] = '1'
	$start.StandardErrorEncoding = $script:Utf8
	foreach ($argument in @('-C', $script:Root, '--no-pager', 'cat-file', 'blob', $Oid)) { [void] $start.ArgumentList.Add($argument) }
	$process = [Diagnostics.Process]::new()
	$process.StartInfo = $start
	if (-not $process.Start()) { throw "Could not start git cat-file for blob $Oid." }
	$stderrTask = $process.StandardError.ReadToEndAsync()
	$hash = [Security.Cryptography.SHA256]::HashData($process.StandardOutput.BaseStream)
	$process.WaitForExit()
	$stderr = $stderrTask.GetAwaiter().GetResult()
	$exitCode = $process.ExitCode
	$process.Dispose()
	if ($exitCode -ne 0) {
		$prefix = "git cat-file blob $Oid failed with exit $exitCode"
		$diagnostic = $stderr.Trim()
		$remaining = $script:MaximumMessageLength - $prefix.Length - 2
		if ($remaining -le 0) { throw $prefix.Substring(0, $script:MaximumMessageLength) }
		if ($diagnostic.Length -gt $remaining) { $diagnostic = $diagnostic.Substring(0, $remaining) }
		if ([string]::IsNullOrEmpty($diagnostic)) { throw $prefix }
		throw "$prefix`: $diagnostic"
	}
	$script:BlobHashes[$Oid] = Get-InventoryHex $hash
	return $script:BlobHashes[$Oid]
}

function Get-InventoryModuleSha256() {
	$stream = [IO.File]::Open($script:InventoryModulePath, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
	try { return Get-InventoryHex ([Security.Cryptography.SHA256]::HashData($stream)) }
	finally { $stream.Dispose() }
}

function Get-InventoryFileSha256([string] $RelativePath) {
	$full = Join-Path $script:Root ($RelativePath -replace '/', [IO.Path]::DirectorySeparatorChar)
	$stream = [IO.File]::Open($full, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
	try { return Get-InventoryHex ([Security.Cryptography.SHA256]::HashData($stream)) }
	finally { $stream.Dispose() }
}

function Get-InventoryWorkingMode([string] $RelativePath) {
	$full = Join-Path $script:Root ($RelativePath -replace '/', [IO.Path]::DirectorySeparatorChar)
	$item = Get-Item -LiteralPath $full -Force -ErrorAction Stop
	if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { return '120000' }
	return '100644'
}

function Get-InventoryIdentity([string] $Mode, [string] $Oid, [string] $RelativePath, [bool] $WorkingTreeSide = $false) {
	if ($Mode -ceq '000000') { return $null }
	if ($Mode -ceq '160000') {
		# A gitlink's identity is the SHA-256 over the ASCII bytes of its 40-character commit hex.
		$commit = $Oid
		if ($commit -ceq $script:ZeroOid) {
			$submodule = Invoke-InventoryGit @('-C', $RelativePath, 'rev-parse', 'HEAD') $true
			$commit = if ($submodule.ExitCode -eq 0) { $submodule.Stdout.Trim() } else { $null }
		}
		$sha256 = if ($null -eq $commit) { $null } else { Get-InventoryHex ([Security.Cryptography.SHA256]::HashData([Text.Encoding]::ASCII.GetBytes($commit))) }
		return [pscustomobject][ordered]@{ mode = $Mode; sha256 = $sha256 }
	}
	# On the working-tree side rename detection can print a destination OID that diffcore computed for the
	# working-tree content but never wrote to the object database, so only the file bytes are a readable
	# identity there. $script:ManifestModes is exactly the ordinary-file modes; any other mode (a symlink,
	# for one) is not the file's own bytes and keeps blob hashing. A working-tree row with a nonzero mode and
	# no file is an index entry Git does not compare with the disk, such as a path sparse checkout omits: a
	# removed file Git does compare reports mode 000000 and returned above, so this OID names the index's blob.
	$fromFile = ($WorkingTreeSide -and $script:ManifestModes -ccontains $Mode -and (Test-InventoryHeadPath $RelativePath)) -or ($Oid -ceq $script:ZeroOid)
	$sha256 = if ($fromFile) { Get-InventoryFileSha256 $RelativePath } else { Get-InventoryBlobSha256 $Oid }
	return [pscustomobject][ordered]@{ mode = $Mode; sha256 = $sha256 }
}

function Get-PathClass([string] $Path, [string] $Mode, [bool] $IsBinary) {
	# A gitlink is never a source artifact even when its path resembles one.
	if ($Mode -ceq '160000') { return 'other' }
	$leaf = $Path.Substring($Path.LastIndexOf('/') + 1)
	$extension = if ($leaf.Contains('.')) { $leaf.Substring($leaf.LastIndexOf('.')).ToLowerInvariant() } else { '' }
	$components = $Path -split '/'
	# The Coretide Plans tree follows the engine Plans rules below its Projects/Coretide/ prefix.
	$planComponents = $components
	if ($Path.StartsWith('Projects/Coretide/', [StringComparison]::Ordinal)) { $planComponents = @($components | Select-Object -Skip 2) }
	# A text file in a Plan's support folder (Documents/Plans/<Area>/<Name>/) is Plan content whatever its extension.
	if (-not $IsBinary -and $planComponents.Count -gt 4 -and $planComponents[0] -ceq 'Documents' -and $planComponents[1] -ceq 'Plans') { return 'plan' }
	if ($script:DualLanguageHeaders -ccontains $leaf.ToLowerInvariant()) { return 'dual-language-header' }
	$underShaders = $false
	for ($i = 0; $i -lt $components.Count - 2; $i++) {
		if ($components[$i] -ceq 'Data' -and $components[$i + 1] -ceq 'Shaders') { $underShaders = $true; break }
	}
	# The directory clause covers headers and shader sources only: other files under
	# Data/Shaders (AGENTS.md, tables) fall through to the later rules.
	if ($script:ShaderExtensions -ccontains $extension) { return 'glsl' }
	if ($underShaders -and $extension -ceq '.h') { return 'glsl' }
	if ($extension -cin @('.cpp', '.h', '.inl')) { return 'cpp' }
	if ($leaf -ceq 'SKILL.md' -and $components.Count -eq 4 -and $components[0] -ceq '.agents' -and $components[1] -ceq 'skills') { return 'skill' }
	if ($extension -ceq '.md' -and $planComponents.Count -gt 2 -and $planComponents[0] -ceq 'Documents' -and $planComponents[1] -ceq 'Plans' -and $leaf -cne 'AGENTS.md') { return 'plan' }
	if ($extension -cin @('.ps1', '.psm1', '.py', '.sh')) { return 'script' }
	if ($extension -ceq '.vcxproj' -or $leaf.ToLowerInvariant().EndsWith('.vcxproj.filters')) { return 'vcxproj' }
	if ($extension -cin @('.md', '.txt')) { return 'doc' }
	if ($IsBinary) { return 'binary' }
	return 'other'
}

function Get-InventoryEntry([hashtable] $Row, [hashtable] $BinaryPaths, [bool] $WorkingTreeHead) {
	# One raw-diff row becomes one entry: both sides' identities, and both sides' classes whenever the
	# row has a baseline side, so a rename crossing a class boundary and a type change between an
	# ordinary file and a gitlink both stay visible to routing and to target eligibility.
	$path = $Row.Path
	$oldPath = $Row.OldPath
	$isBinary = $BinaryPaths.ContainsKey($path) -or ($null -ne $oldPath -and $BinaryPaths.ContainsKey($oldPath))
	$baselinePath = if ($null -ne $oldPath) { $oldPath } else { $path }
	$currentMode = if ($Row.DestinationMode -cne '000000') { $Row.DestinationMode } else { $Row.SourceMode }
	$baseline = Get-InventoryIdentity $Row.SourceMode $Row.SourceOid $baselinePath
	$current = Get-InventoryIdentity $Row.DestinationMode $Row.DestinationOid $path $WorkingTreeHead
	$class = Get-PathClass $path $currentMode $isBinary
	$oldClass = if ($Row.SourceMode -ceq '000000') { $null } else { Get-PathClass $baselinePath $Row.SourceMode $isBinary }
	return [pscustomobject] @{
		Status = $Row.Status
		Path = $path
		OldPath = $oldPath
		Baseline = $baseline
		Current = $current
		Class = $class
		OldClass = $oldClass
		Untracked = $Row.Untracked
	}
}

function Test-ReviewExemptEntry([object] $Entry) {
	# A deleted Plan file owes no review: /next-plan completion deletes the claimed Plan.
	# A modified refactor-candidate or workflow incident log owes none either: their rows are added after review.
	if ($Entry.Status -ceq 'D' -and $Entry.Class -ceq 'plan') { return $true }
	return $Entry.Status -ceq 'M' -and $Entry.Path -cin @('Documents/Investigations/Engine/RefactorCandidates.md', 'Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md')
}

function Test-CoherenceReviewEntry([object] $Entry) {
	if (Test-ReviewExemptEntry $Entry) { return $false }
	return $script:CoherenceReviewClasses -ccontains $Entry.Class -or ($null -ne $Entry.OldClass -and $script:CoherenceReviewClasses -ccontains $Entry.OldClass)
}

function Test-InstructionDocPath([string] $Path) {
	$leaf = $Path.Substring($Path.LastIndexOf('/') + 1)
	if ($leaf -ceq 'AGENTS.md') { return $true }
	if (-not $leaf.ToLowerInvariant().EndsWith('.md')) { return $false }
	return $Path.StartsWith('.agents/skills/') -or $Path.StartsWith('.agents/references/') -or $Path.StartsWith('Documents/Architecture/')
}

function Test-InventoryHeadPath([string] $Path) {
	# A commit-valued head is answered from its tree, so a file the working tree happens to hold but
	# that commit does not is never treated as present.
	if ([string]::IsNullOrEmpty($script:HeadSha)) { return Test-Path -LiteralPath (Join-Path $script:Root ($Path -replace '/', [IO.Path]::DirectorySeparatorChar)) -PathType Leaf }
	if ($null -eq $script:HeadPaths) {
		$script:HeadPaths = [Collections.Generic.HashSet[string]]::new([string[]] @())
		foreach ($line in ((Invoke-InventoryGit @('ls-tree', '-r', '--name-only', '-z', $script:HeadSha)).Stdout -split "`0")) {
			if (-not [string]::IsNullOrEmpty($line)) { [void] $script:HeadPaths.Add($line) }
		}
	}
	return $script:HeadPaths.Contains($Path)
}

function Get-SkillPackageManifestPath([string] $Path) {
	# Every file under `.agents/skills/<package>/` belongs to that package; a path elsewhere has none.
	if ([string]::IsNullOrEmpty($Path)) { return $null }
	if ($Path -cmatch '^\.agents/skills/([^/]+)/') { return ".agents/skills/$($Matches[1])/SKILL.md" }
	return $null
}

function Get-InventorySideText([bool] $HeadSide, [string] $Path) {
	# A commit-valued side is read from the object database; the working-tree side is the file itself.
	# An unreadable side returns $null, which suppresses the guard candidate rather than guessing.
	# A Coretide path is read unprefixed in the Coretide clone at its own baseline or head.
	$coretide = Test-CoretidePath $Path
	if ($coretide) {
		Enter-CoretideSide
		$Path = $Path.Substring($script:CoretidePrefix.Length)
	}
	try {
		$revision = if ($HeadSide) { $script:HeadSha } else { $script:BaselineSha }
		if ([string]::IsNullOrEmpty($revision)) {
			$full = Join-Path $script:Root ($Path -replace '/', [IO.Path]::DirectorySeparatorChar)
			if (-not (Test-Path -LiteralPath $full -PathType Leaf)) { return $null }
			return [IO.File]::ReadAllText($full)
		}
		$run = Invoke-InventoryGit @('show', "$($revision):$Path") $true
		if ($run.ExitCode -ne 0) { return $null }
		return $run.Stdout
	}
	finally { if ($coretide) { Exit-CoretideSide } }
}

function Get-InventoryGuardSide([string] $Text) {
	# Only a proven whole-file client or server guard is one-sided; every other outcome collapses to
	# one value, so a candidate is emitted only where /update-vcxproj's own resolver also sees a move.
	if ($null -eq $Text) { return $null }
	$affinity = (Resolve-FileAffinity $Text).affinity
	if ($affinity -ceq 'client' -or $affinity -ceq 'server') { return $affinity }
	return 'not-one-sided'
}

function Get-RoutingTrigger([object[]] $Entries) {
	$classes = [Collections.Generic.HashSet[string]]::new([string[]] @())
	$membershipClasses = [Collections.Generic.HashSet[string]]::new([string[]] @())
	$coherenceReview = $false
	# Instruction docs span the doc and skill classes, so this one trigger is decided from the paths.
	$instructionDoc = $false
	# Any changed file inside a skill package that has a head-side SKILL.md can invalidate that package,
	# which the `skill` class (SKILL.md alone) does not cover, so this trigger is decided from the paths
	# too — both sides, because a rename out of a package can break the package it left.
	$validateSkill = $false
	# The paths /update-vcxproj must re-resolve, with why: a membership move the class set already
	# counts, or a modification that flipped the file's whole-file client/server guard.
	$candidates = [Collections.Generic.List[object]]::new()
	$candidatePaths = [Collections.Generic.HashSet[string]]::new([string[]] @(), [StringComparer]::Ordinal)
	$guardCandidate = $false
	foreach ($entry in $Entries) {
		[void] $classes.Add($entry.Class)
		if ($null -ne $entry.OldClass) { [void] $classes.Add($entry.OldClass) }
		if (-not $coherenceReview -and (Test-CoherenceReviewEntry $entry)) { $coherenceReview = $true }
		if ($entry.Status -cin @('A', 'D', 'R', 'T')) {
			[void] $membershipClasses.Add($entry.Class)
			if ($null -ne $entry.OldClass) { [void] $membershipClasses.Add($entry.OldClass) }
			if ($script:SourceMembershipClasses -ccontains $entry.Class -or ($null -ne $entry.OldClass -and $script:SourceMembershipClasses -ccontains $entry.OldClass)) {
				foreach ($side in @($entry.Path, $entry.OldPath)) {
					if ($null -ne $side -and $candidatePaths.Add($side)) { $candidates.Add([pscustomobject][ordered]@{ path = $side; reason = 'membership' }) }
				}
			}
		}
		elseif ($entry.Status -ceq 'M' -and $script:CppClasses -ccontains $entry.Class) {
			$baselineSide = Get-InventoryGuardSide (Get-InventorySideText $false $entry.Path)
			$currentSide = Get-InventoryGuardSide (Get-InventorySideText $true $entry.Path)
			if ($null -ne $baselineSide -and $null -ne $currentSide -and $baselineSide -cne $currentSide) {
				$guardCandidate = $true
				if ($candidatePaths.Add($entry.Path)) { $candidates.Add([pscustomobject][ordered]@{ path = $entry.Path; reason = 'guard' }) }
			}
		}
		if (Test-InstructionDocPath $entry.Path) { $instructionDoc = $true }
		if ($null -ne $entry.OldPath -and (Test-InstructionDocPath $entry.OldPath)) { $instructionDoc = $true }
		if (-not $validateSkill) {
			foreach ($side in @($entry.Path, $entry.OldPath)) {
				$manifest = Get-SkillPackageManifestPath $side
				if ($null -ne $manifest -and (Test-InventoryHeadPath $manifest)) { $validateSkill = $true; break }
			}
		}
	}
	$cpp = $classes.Contains('cpp') -or $classes.Contains('dual-language-header')
	$glsl = $classes.Contains('glsl') -or $classes.Contains('dual-language-header')
	$sourceMembership = @($script:SourceMembershipClasses | Where-Object { $membershipClasses.Contains($_) }).Count -gt 0
	return [pscustomobject][ordered]@{
		coherenceReview = $coherenceReview
		repoCodeReview = $cpp
		overEngineeringReview = $classes.Contains('cpp')
		glslReview = $glsl
		codeStyleReview = $cpp
		commentReview = $cpp -or $glsl
		updateAffectedCode = $cpp -or $glsl
		updateVcxproj = $sourceMembership -or $guardCandidate
		updateClaudeDocs = $cpp -or $glsl
		validateSkill = $validateSkill
		progressiveDisclosureReview = $instructionDoc
		planTouched = $classes.Contains('plan')
		vcxprojCandidates = [object[]] $candidates.ToArray()
	}
}

function Get-InventoryCount([object[]] $Entries, [int] $UnlistedUntracked) {
	$counts = [ordered]@{ total = $Entries.Count }
	foreach ($name in $script:ClassNames) { $counts[$name] = @($Entries | Where-Object { $_.Class -ceq $name }).Count }
	$counts['unlistedUntracked'] = $UnlistedUntracked
	return [pscustomobject] $counts
}

function Get-InventoryDiffArgument([string] $BaselineSha, [string] $HeadSha) {
	if ([string]::IsNullOrEmpty($HeadSha)) { return @($BaselineSha) }
	return @($BaselineSha, $HeadSha)
}

function Get-InventoryRawRow([string] $BaselineSha, [string] $HeadSha) {
	$arguments = @('diff', '--raw', '--abbrev=40', '-M', '-z', '--no-color', '--no-ext-diff') + (Get-InventoryDiffArgument $BaselineSha $HeadSha) + @('--')
	$tokens = (Invoke-InventoryGit $arguments).Stdout -split "`0"
	$rows = [Collections.Generic.List[hashtable]]::new()
	$index = 0
	while ($index -lt $tokens.Count) {
		$token = $tokens[$index]
		if ([string]::IsNullOrEmpty($token)) { $index++; continue }
		$fields = $token.TrimStart(':') -split ' '
		$status = $fields[4].Substring(0, 1)
		$row = @{
			SourceMode = $fields[0]
			DestinationMode = $fields[1]
			SourceOid = $fields[2]
			DestinationOid = $fields[3]
			Status = $status
			OldPath = $null
			Untracked = $false
		}
		if ($status -ceq 'R' -or $status -ceq 'C') {
			$row.OldPath = $tokens[$index + 1]
			$row.Path = $tokens[$index + 2]
			$index += 3
		}
		else {
			$row.Path = $tokens[$index + 1]
			$index += 2
		}
		$rows.Add($row)
	}
	return $rows
}

function Get-InventoryBinaryPath([string] $BaselineSha, [string] $HeadSha) {
	# Git's own numstat decides binary, so .gitattributes settings are honoured.
	$arguments = @('diff', '--numstat', '-M', '-z', '--no-color', '--no-ext-diff') + (Get-InventoryDiffArgument $BaselineSha $HeadSha) + @('--')
	$tokens = (Invoke-InventoryGit $arguments).Stdout -split "`0"
	$binary = @{}
	$index = 0
	while ($index -lt $tokens.Count) {
		$token = $tokens[$index]
		if ([string]::IsNullOrEmpty($token)) { $index++; continue }
		$fields = $token -split "`t"
		$isBinary = $fields[0] -ceq '-' -and $fields[1] -ceq '-'
		if ($fields.Count -ge 3 -and -not [string]::IsNullOrEmpty($fields[2])) {
			if ($isBinary) { $binary[$fields[2]] = $true }
			$index++
			continue
		}
		if ($isBinary) {
			$binary[$tokens[$index + 1]] = $true
			$binary[$tokens[$index + 2]] = $true
		}
		$index += 3
	}
	return $binary
}

function Test-InventoryUntrackedBinary([string] $RelativePath) {
	$nullDevice = if ($IsWindows) { 'NUL' } else { '/dev/null' }
	$run = Invoke-InventoryGit @('diff', '--no-index', '--numstat', '--no-color', '--no-ext-diff', '--', $nullDevice, $RelativePath) $true
	foreach ($line in ($run.Stdout -split "`n")) {
		if ($line.StartsWith("-`t-`t")) { return $true }
	}
	return $false
}

function ConvertTo-NormalizedPathFilter([string[]] $Values, [bool] $SplitCommas = $false) {
	$normalizedList = [Collections.Generic.List[string]]::new()
	if ($null -eq $Values) { return , $normalizedList }
	foreach ($item in $Values) {
		if ([string]::IsNullOrEmpty($item)) { continue }
		$parts = if ($SplitCommas) { @($item -split ',') } else { @($item) }
		foreach ($part in $parts) {
			$normalized = $part.Replace('\', '/')
			if ($SplitCommas) { $normalized = $normalized.Trim() }
			while ($normalized.StartsWith('./')) { $normalized = $normalized.Substring(2) }
			$normalized = $normalized.TrimEnd('/')
			if ($normalized.Length -gt 0) { $normalizedList.Add($normalized) }
		}
	}
	return , $normalizedList
}

function Test-InventoryPathInScope([string] $Path, [string[]] $Prefixes) {
	# A prefix names a whole path segment, so Engine/Source/Frame never matches Engine/Source/FrameX.
	foreach ($prefix in $Prefixes) {
		if ([string]::Equals($Path, $prefix, [StringComparison]::Ordinal) -or $Path.StartsWith($prefix + '/', [StringComparison]::Ordinal)) { return $true }
	}
	return $false
}

function Get-InventoryUntrackedEntry([bool] $ListAll, [bool] $HasUntrackedFilter, [Collections.Generic.List[string]] $UntrackedFilter, [string] $Prefix, [ref] $UnlistedCount) {
	$tracked = @((Invoke-InventoryGit @('ls-files', '--others', '--exclude-standard', '-z')).Stdout -split "`0" | Where-Object { -not [string]::IsNullOrEmpty($_) })
	$entries = [Collections.Generic.List[object]]::new()
	$unlisted = 0
	foreach ($path in $tracked) {
		$admit = if ($HasUntrackedFilter) { $null -ne $UntrackedFilter -and $UntrackedFilter.Count -gt 0 -and (Test-InventoryPathInScope ($Prefix + $path) $UntrackedFilter) } else { $ListAll }
		if (-not $admit) { $unlisted++; continue }
		$mode = Get-InventoryWorkingMode $path
		# The binary rule is ordered after every extension rule, so the per-file Git probe only
		# runs for a path no earlier rule already claimed.
		$class = Get-PathClass $path $mode $false
		if ($class -ceq 'other' -and (Test-InventoryUntrackedBinary $path)) { $class = 'binary' }
		$entries.Add([pscustomobject] @{
			Status = 'A'
			Path = $path
			OldPath = $null
			Baseline = $null
			Current = [pscustomobject][ordered]@{ mode = $mode; sha256 = Get-InventoryFileSha256 $path }
			Class = $class
			OldClass = $null
			Untracked = $true
		})
	}
	$UnlistedCount.Value = $unlisted
	return $entries
}

function Get-RegionTable([string] $BaselineSha, [string] $HeadSha, [object[]] $UntrackedEntries) {
	$arguments = @('-c', 'core.quotepath=false', 'diff', '-U0', '-M', '--no-color', '--no-ext-diff', '--no-textconv') + (Get-InventoryDiffArgument $BaselineSha $HeadSha) + @('--')
	$lines = @((Invoke-InventoryGit $arguments).Stdout -split "`n")
	foreach ($entry in $UntrackedEntries) {
		$nullDevice = if ($IsWindows) { 'NUL' } else { '/dev/null' }
		$run = Invoke-InventoryGit @('-c', 'core.quotepath=false', 'diff', '--no-index', '-U0', '--no-color', '--no-ext-diff', '--no-textconv', '--', $nullDevice, $entry.Path) $true
		$lines += @($run.Stdout -split "`n")
	}
	$regions = [Collections.Generic.List[object]]::new()
	$oldPath = $null
	$newPath = $null
	foreach ($line in $lines) {
		if ($line.StartsWith('--- ')) {
			$value = $line.Substring(4).TrimEnd("`r").TrimEnd("`t")
			$oldPath = if ($value -ceq '/dev/null' -or $value -ceq 'NUL') { $null } else { $value.Substring(2) }
			continue
		}
		if ($line.StartsWith('+++ ')) {
			$value = $line.Substring(4).TrimEnd("`r").TrimEnd("`t")
			$newPath = if ($value -ceq '/dev/null' -or $value -ceq 'NUL') { $null } else { $value.Substring(2) }
			continue
		}
		if (-not $line.StartsWith('@@ ')) { continue }
		if ($line -cnotmatch '^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@ ?(.*)$') { continue }
		$oldStart = [int] $Matches[1]
		$oldCount = if ([string]::IsNullOrEmpty($Matches[2])) { 1 } else { [int] $Matches[2] }
		$newStart = [int] $Matches[3]
		$newCount = if ([string]::IsNullOrEmpty($Matches[4])) { 1 } else { [int] $Matches[4] }
		$symbol = $Matches[5].TrimEnd("`r").Trim()
		if ($symbol.Length -gt $script:MaximumSymbolLength) { $symbol = $symbol.Substring(0, $script:MaximumSymbolLength) }
		$kind = if ($oldCount -eq 0) { 'added' } elseif ($newCount -eq 0) { 'deleted' } else { 'modified' }
		# A non-null symbol is Git's preceding-context hint from the pre-change side (nearest preceding
		# unindented line, usually the enclosing declaration), never the region's own text, so it is emitted
		# only for the $script:SourceMembershipClasses classes; that set picks which regions carry one.
		# The fixed ordinary-file mode and $false are safe: Get-PathClass uses the mode only to divert a
		# gitlink, whose hunk header carries no context text, and the flag only to relabel 'other' as
		# 'binary' and to send a Plan support-folder file past the support-folder 'plan' rule to the
		# extension rules, while a binary path emits no @@ header at all.
		$regionPath = if ($null -ne $newPath) { $newPath } else { $oldPath }
		$carriesSymbol = $script:SourceMembershipClasses -ccontains (Get-PathClass $regionPath '100644' $false)
		$regions.Add([pscustomobject][ordered]@{
			path = $regionPath
			kind = $kind
			startLine = if ($newCount -eq 0) { $null } else { $newStart }
			endLine = if ($newCount -eq 0) { $null } else { $newStart + $newCount - 1 }
			oldStartLine = if ($oldCount -eq 0) { $null } else { $oldStart }
			oldEndLine = if ($oldCount -eq 0) { $null } else { $oldStart + $oldCount - 1 }
			addedLines = $newCount
			deletedLines = $oldCount
			symbol = if ($carriesSymbol -and -not [string]::IsNullOrEmpty($symbol)) { $symbol } else { $null }
		})
	}
	# Wrap so a zero- or one-element table stays a collection instead of unrolling (see
	# Find-SessionCandidates.ps1 Get-NewSideLine for the same pattern).
	return , $regions
}

function Get-SavePlanAuthorizedPaths([string] $RawPaths, [ref] $ErrorMessage) {
	if ([string]::IsNullOrWhiteSpace($RawPaths)) {
		$ErrorMessage.Value = '-SavePlanPaths must contain at least one path under a planning root.'
		return $null
	}
	$authorized = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
	foreach ($segment in ($RawPaths -split ',')) {
		$trimmed = $segment.Trim()
		if ($trimmed.Length -eq 0) {
			$ErrorMessage.Value = '-SavePlanPaths must not contain an empty comma segment.'
			return $null
		}
		$normalized = $trimmed.Replace('\', '/')
		if ($normalized.StartsWith('./', [StringComparison]::Ordinal)) { $normalized = $normalized.Substring(2) }
		$parts = $normalized.Split('/')
		foreach ($part in $parts) {
			if ($part.Length -eq 0 -or $part -ceq '.' -or $part -ceq '..') {
				$ErrorMessage.Value = "Invalid -SavePlanPaths entry '$trimmed': empty, '.', or '..' path segments are not allowed."
				return $null
			}
		}
		$inPlanningRoot = $false
		foreach ($root in $script:SavePlanRoots) {
			if ($normalized.StartsWith($root, [StringComparison]::Ordinal)) {
				$inPlanningRoot = $true
				break
			}
		}
		if (-not $inPlanningRoot) {
			$ErrorMessage.Value = "Invalid -SavePlanPaths entry '$trimmed': path must be under a planning root."
			return $null
		}
		if ($parts[-1].Equals('AGENTS.md', [StringComparison]::OrdinalIgnoreCase)) {
			$ErrorMessage.Value = "Invalid -SavePlanPaths entry '$trimmed': AGENTS.md is not allowed."
			return $null
		}
		[void] $authorized.Add($normalized)
	}
	# Wrap so a single-element HashSet is not unrolled into a string by the pipeline.
	return , $authorized
}

function Test-SavePlanOnlyReviewedRows([Collections.Generic.List[object]] $Reviewed, [Collections.Generic.HashSet[string]] $AuthorizedPaths) {
	$paths = [Collections.Generic.List[string]]::new($Reviewed.Count)
	foreach ($row in $Reviewed) {
		if ($row.status -cne 'A' -or $script:ManifestModes -cnotcontains $row.currentMode) { return $false }
		$paths.Add([string] $row.path)
	}
	return $AuthorizedPaths.SetEquals($paths)
}

function Get-LandingState([string] $BaselineSha, [string] $HeadSha, [ref] $Truncation, [ref] $Refusal, [Collections.Generic.HashSet[string]] $SavePlanAuthorizedPaths = $null) {
	# The reviewed diff is the session's own three-dot range, resolved to its merge base once so the
	# script's own entry pipeline answers it: `git diff A...B` is defined as the diff from
	# merge-base(A, B) to B, so the two-dot pair below compares the same two trees.
	$mergeBase = (Invoke-InventoryGit @('merge-base', $BaselineSha, $HeadSha)).Stdout.Trim()
	$reviewedBinaryPaths = Get-InventoryBinaryPath $mergeBase $HeadSha
	$reviewed = [Collections.Generic.List[object]]::new()
	$reviewedEntries = [Collections.Generic.List[object]]::new()
	foreach ($row in (Get-InventoryRawRow $mergeBase $HeadSha)) {
		# -Landing requires a commit-valued head, so the head side is never the working tree.
		$reviewedEntries.Add((Get-InventoryEntry $row $reviewedBinaryPaths $false))
		$reviewed.Add([pscustomobject][ordered]@{
			status = $row.Status
			path = $row.Path
			oldPath = $row.OldPath
			baselineMode = $row.SourceMode
			currentMode = $row.DestinationMode
		})
	}
	# The Coretide side's baseline is already the merge base of its HEAD and upstream, so its two-dot range is its
	# reviewed three-dot range.
	if ($null -ne $script:Coretide) {
		Enter-CoretideSide
		try {
			$coretideBinaryPaths = Get-InventoryBinaryPath $script:BaselineSha $script:HeadSha
			foreach ($row in (Get-InventoryRawRow $script:BaselineSha $script:HeadSha)) {
				$reviewedEntries.Add((Add-CoretidePrefix (Get-InventoryEntry $row $coretideBinaryPaths $false)))
				$reviewed.Add([pscustomobject][ordered]@{
					status = $row.Status
					path = $script:CoretidePrefix + $row.Path
					oldPath = if ($null -ne $row.OldPath) { $script:CoretidePrefix + $row.OldPath } else { $null }
					baselineMode = $row.SourceMode
					currentMode = $row.DestinationMode
				})
			}
		}
		finally { Exit-CoretideSide }
	}
	# Routing reads a modified file's baseline side from $script:BaselineSha, whose reviewed-diff
	# value is the merge base; restore the module-scoped value afterwards so later helpers still see
	# the caller's baseline.
	$topLevelBaselineSha = $script:BaselineSha
	$script:BaselineSha = $mergeBase
	$reviewedTriggers = Get-RoutingTrigger ([object[]] $reviewedEntries.ToArray())
	$script:BaselineSha = $topLevelBaselineSha
	$porcelain = @((Invoke-InventoryGit @('status', '--porcelain=v2', '--untracked-files=all', '--ignored=matching')).Stdout -split "`n" | ForEach-Object { $_.TrimEnd("`r") } | Where-Object { -not [string]::IsNullOrEmpty($_) })
	# Uninitialized submodules are normal provisioning state, reported verbatim and never drift;
	# a repository whose gitlinks have no .gitmodules entry simply reports none.
	$submoduleRun = Invoke-InventoryGit @('submodule', 'status') $true
	$submodules = @()
	if ($submoduleRun.ExitCode -eq 0) { $submodules = @($submoduleRun.Stdout -split "`n" | ForEach-Object { $_.TrimEnd("`r") } | Where-Object { -not [string]::IsNullOrEmpty($_) }) }
	$planTouched = $false
	foreach ($row in $reviewed) {
		if ((Get-PathClass $row.path $row.currentMode $false) -ceq 'plan') { $planTouched = $true; break }
		if ($null -ne $row.oldPath -and (Get-PathClass $row.oldPath $row.baselineMode $false) -ceq 'plan') { $planTouched = $true; break }
	}
	if ($null -ne $SavePlanAuthorizedPaths -and (Test-SavePlanOnlyReviewedRows $reviewed $SavePlanAuthorizedPaths)) {
		$reviewedTriggers.coherenceReview = $false
	}
	# The clone's HEAD is the commit Invoke-FinalizeCoretide.ps1 -Prepare made before this run.
	$coretideCandidate = if ($null -ne $script:Coretide) { $script:Coretide.HeadSha } else { '' }
	$Refusal.Value = Get-ReviewRoundCoverageFailure $script:Root $BaselineSha $HeadSha $coretideCandidate $SavePlanAuthorizedPaths
	# The rows this landing owes, each left BLOCKED for the finalizer to fill with evidence. The
	# Executable Plan row is decided from the reviewed paths rather than from planTouched, whose
	# `plan` class deliberately excludes the Plans tree's own AGENTS.md.
	$skeleton = [Collections.Generic.List[object]]::new()
	foreach ($name in $script:AcceptanceSkeletonChecks.Keys) {
		if ($reviewedTriggers.$name) { $skeleton.Add([pscustomobject][ordered]@{ check = $script:AcceptanceSkeletonChecks[$name]; status = 'BLOCKED' }) }
	}
	foreach ($row in $reviewed) {
		$planPath = $false
		foreach ($side in @($row.path, $row.oldPath)) {
			if ($null -ne $side -and ($side.StartsWith('Documents/Plans/') -or $side.StartsWith('Projects/Coretide/Documents/Plans/'))) { $planPath = $true }
		}
		if ($planPath) {
			$skeleton.Add([pscustomobject][ordered]@{ check = 'Executable Plan check'; status = 'BLOCKED' })
			break
		}
	}
	$emittedReviewed = [object[]] $reviewed.ToArray()
	$emittedPorcelain = [string[]] $porcelain
	$emittedSubmodules = [string[]] $submodules
	$landingTruncation = [pscustomobject][ordered]@{
		reviewed = [pscustomobject][ordered]@{ full = $reviewed.Count; emitted = $emittedReviewed.Count }
		porcelain = [pscustomobject][ordered]@{ full = $porcelain.Count; emitted = $emittedPorcelain.Count }
		submodules = [pscustomobject][ordered]@{ full = $submodules.Count; emitted = $emittedSubmodules.Count }
	}
	$Truncation.Value = $landingTruncation
	return [pscustomobject][ordered]@{
		reviewed = $emittedReviewed
		porcelain = $emittedPorcelain
		submodules = $emittedSubmodules
		planTouched = $planTouched
		acceptanceSkeleton = [object[]] $skeleton.ToArray()
	}
}

function Save-InventoryState() {
	return @{ Root = $script:Root; BaselineSha = $script:BaselineSha; HeadSha = $script:HeadSha; HeadPaths = $script:HeadPaths; Coretide = $script:Coretide; EngineSide = $script:EngineSide; BlobHashes = $script:BlobHashes }
}

function Restore-InventoryState([hashtable] $State) {
	$script:Root = $State.Root
	$script:BaselineSha = $State.BaselineSha
	$script:HeadSha = $State.HeadSha
	$script:HeadPaths = $State.HeadPaths
	$script:Coretide = $State.Coretide
	$script:EngineSide = $State.EngineSide
	$script:BlobHashes = $State.BlobHashes
}

# A Git run in an explicit repository, so the review-round readers never depend on the per-run state.
function Invoke-ReviewChainGit([string] $Repository, [string[]] $Arguments, [bool] $AllowFailure = $true) {
	$run = Invoke-AgentProcess 'git' (@('-C', $Repository, '--no-pager') + $Arguments) $Repository
	if (-not $AllowFailure -and $run.ExitCode -ne 0) { throw "git $($Arguments -join ' ') failed with exit $($run.ExitCode): $($run.Stderr.Trim())" }
	return $run
}

# The fields of a review-round snapshot or close message in the Get-ChangeWorkflowSteps.ps1 header forms, or null
# when the subject is neither or any line breaks the form.
function ConvertFrom-ReviewRoundMessage([string] $Message) {
	$lines = [Collections.Generic.List[string]]::new([string[]] ($Message -split "`n"))
	while ($lines.Count -gt 0 -and $lines[$lines.Count - 1] -ceq '') { $lines.RemoveAt($lines.Count - 1) }
	if ($lines.Count -lt 3 -or $lines[1] -cne '') { return $null }
	$subject = [regex]::Match($lines[0], '^Review round ([1-9][0-9]{0,8})( closed)?$')
	if (-not $subject.Success) { return $null }
	$number = [int] $subject.Groups[1].Value
	$values = @{}
	for ($i = 2; $i -lt $lines.Count; $i++) {
		$line = [regex]::Match($lines[$i], '^([A-Za-z-]+): (\S(?:.*\S)?)$')
		if (-not $line.Success) { return $null }
		$name = $line.Groups[1].Value
		if (-not $values.ContainsKey($name)) { $values[$name] = [Collections.Generic.List[string]]::new() }
		$values[$name].Add($line.Groups[2].Value)
	}
	$single = { param([string] $Name) if ($values.ContainsKey($Name) -and $values[$Name].Count -eq 1) { $values[$Name][0] } else { $null } }
	if ($subject.Groups[2].Success) {
		foreach ($name in $values.Keys) { if ($name -cnotin @('Closes', 'Result', 'Next')) { return $null } }
		$next = & $single 'Next'
		if ((& $single 'Closes') -cne "$number" -or $next -cnotin @('repeat', 'check', 'done')) { return $null }
		$results = [Collections.Generic.List[object]]::new()
		$highest = 'none'
		if ($values.ContainsKey('Result')) {
			foreach ($value in $values['Result']) {
				$result = [regex]::Match($value, '^(\S+) (none|Low|Medium|High|Critical)$')
				if (-not $result.Success -or -not $script:ReviewRoundKeys.Contains($result.Groups[1].Value)) { return $null }
				if (@($results | Where-Object { $_.Key -ceq $result.Groups[1].Value }).Count -gt 0) { return $null }
				$results.Add([pscustomobject] @{ Key = $result.Groups[1].Value; Severity = $result.Groups[2].Value })
				if ($script:ReviewSeverityRank[$result.Groups[2].Value] -gt $script:ReviewSeverityRank[$highest]) { $highest = $result.Groups[2].Value }
			}
		}
		return [pscustomobject] @{ IsClose = $true; Number = $number; Results = [object[]] $results.ToArray(); Next = $next; Highest = $highest }
	}
	foreach ($name in $values.Keys) { if ($name -cnotin @('Round', 'Baseline', 'Phase', 'Kind', 'Tier', 'Owes', 'Coretide-Snapshot', 'Coretide-Baseline')) { return $null } }
	$baseline = & $single 'Baseline'
	$phase = & $single 'Phase'
	$kind = & $single 'Kind'
	$tier = & $single 'Tier'
	if ((& $single 'Round') -cne "$number" -or $baseline -cnotmatch '^[0-9a-f]{40}$' -or $phase -cnotin @('code', 'words', 'combined', 'acceptance', 'sweep') -or $kind -cnotin @('ordinary', 'check')) { return $null }
	if ($values.ContainsKey('Tier') -eq ($phase -ceq 'sweep') -or ($phase -cne 'sweep' -and $tier -cnotin @('1', '2', '3'))) { return $null }
	$owes = [string[]] @()
	if ($values.ContainsKey('Owes')) { $owes = [string[]] $values['Owes'].ToArray() }
	foreach ($key in $owes) { if (-not $script:ReviewRoundKeys.Contains($key)) { return $null } }
	if (@($owes | Sort-Object -Unique -CaseSensitive).Count -ne $owes.Count -or (($phase -ceq 'sweep') -ne ($owes.Count -eq 0))) { return $null }
	$coretideSnapshot = & $single 'Coretide-Snapshot'
	$coretideBaseline = & $single 'Coretide-Baseline'
	if ($values.ContainsKey('Coretide-Snapshot') -ne $values.ContainsKey('Coretide-Baseline')) { return $null }
	if ($values.ContainsKey('Coretide-Snapshot') -and ($coretideSnapshot -cnotmatch '^[0-9a-f]{40}$' -or $coretideBaseline -cnotmatch '^[0-9a-f]{40}$')) { return $null }
	return [pscustomobject] @{ IsClose = $false; Number = $number; Baseline = $baseline; Phase = $phase; Kind = $kind; Tier = $(if ($null -ne $tier) { [int] $tier } else { 0 }); Owes = $owes; CoretideSnapshot = $coretideSnapshot; CoretideBaseline = $coretideBaseline }
}

# The engine chain by the reading rule, rounds oldest first: walking first parents from the ref tip, only the tip
# round may be open, every close sits on the snapshot of the round it names with that snapshot's tree, a result for
# each owed key and otherwise only /adversarial-review, and the Next its results and the snapshot's Kind give, a Tier
# line except on a sweep round, a sweep round is ordinary and closed with no result, numbers descend by one to round
# 1, and every Coretide-Snapshot resolves in the clone. Anything else, a symbolic ref included, reads as no chain,
# never repaired. Tip is the ref's value as read here (the zero OID when absent), the old value of the compare-and-swap
# of a writer that planned from this read.
function Read-ReviewRoundChain([string] $Root) {
	$tip = Invoke-ReviewChainGit $Root @('rev-parse', '--verify', '--quiet', $script:ReviewRoundRef)
	$tipSha = if ($tip.ExitCode -eq 0) { $tip.Stdout.Trim() } else { $script:ZeroOid }
	$unreadable = [pscustomobject] @{ Readable = $false; Rounds = [object[]] @(); Tip = $tipSha }
	if ($tip.ExitCode -ne 0) { return $unreadable }
	if ((Invoke-ReviewChainGit $Root @('symbolic-ref', '--quiet', $script:ReviewRoundRef)).ExitCode -eq 0) { return $unreadable }
	$rounds = [Collections.Generic.List[object]]::new()
	$close = $null
	$sha = $tipSha
	while ($true) {
		$commit = Invoke-ReviewChainGit $Root @('cat-file', 'commit', $sha)
		if ($commit.ExitCode -ne 0) { return $unreadable }
		$split = $commit.Stdout.IndexOf("`n`n")
		if ($split -lt 0) { return $unreadable }
		$header = $commit.Stdout.Substring(0, $split)
		$parent = [regex]::Match($header, '(?m)^parent ([0-9a-f]{40})$')
		$tree = [regex]::Match($header, '(?m)^tree ([0-9a-f]{40})$').Groups[1].Value
		$fields = ConvertFrom-ReviewRoundMessage $commit.Stdout.Substring($split + 2)
		if ($null -eq $fields) { return $unreadable }
		$expected = if ($null -ne $close) { $close.Number } elseif ($rounds.Count -gt 0) { $rounds[$rounds.Count - 1].Number - 1 } else { $fields.Number }
		if ($fields.Number -ne $expected) { return $unreadable }
		if ($fields.IsClose) {
			if ($null -ne $close) { return $unreadable }
			$close = [pscustomobject] @{ Commit = $sha; Tree = $tree; Number = $fields.Number; Results = $fields.Results; Next = $fields.Next; Highest = $fields.Highest }
		}
		else {
			if ($null -eq $close -and $rounds.Count -gt 0) { return $unreadable }
			if ($fields.Phase -ceq 'sweep' -and ($null -eq $close -or $fields.Kind -cne 'ordinary' -or $close.Results.Count -gt 0)) { return $unreadable }
			if ($null -ne $close) {
				# Only the close -CloseRound would write for this snapshot reads.
				$resultKeys = [string[]] @($close.Results | ForEach-Object { $_.Key })
				if ($close.Tree -cne $tree) { return $unreadable }
				if (@($fields.Owes | Where-Object { $resultKeys -cnotcontains $_ }).Count -gt 0) { return $unreadable }
				if (@($resultKeys | Where-Object { $fields.Owes -cnotcontains $_ -and $_ -cne '/adversarial-review' }).Count -gt 0) { return $unreadable }
				$next = if ($close.Highest -cin @('High', 'Critical')) { 'repeat' } elseif ($fields.Kind -ceq 'ordinary' -and $close.Highest -cin @('Medium', 'Low')) { 'check' } else { 'done' }
				if ($close.Next -cne $next) { return $unreadable }
			}
			$rounds.Add([pscustomobject] @{
				Number = $fields.Number; Snapshot = $sha; Baseline = $fields.Baseline; Phase = $fields.Phase; Kind = $fields.Kind; Tier = $fields.Tier; Owes = $fields.Owes
				Carried = [string[]] @(if ($fields.Phase -ceq 'combined') { $fields.Owes | Where-Object { $script:CombinedCarriedKeys -ccontains $_ } })
				CoretideSnapshot = $fields.CoretideSnapshot; CoretideBaseline = $fields.CoretideBaseline; Close = $close
			})
			$close = $null
			if ($fields.Number -eq 1) { break }
		}
		if (-not $parent.Success) { return $unreadable }
		$sha = $parent.Groups[1].Value
	}
	$clone = $null
	foreach ($round in $rounds) {
		if ($null -eq $round.CoretideSnapshot) { continue }
		if ($null -eq $clone) { $clone = Get-AgentCoretideRepository $Root }
		if ($null -eq $clone) { return $unreadable }
		foreach ($revision in @($round.CoretideSnapshot, $round.CoretideBaseline)) {
			if ((Invoke-ReviewChainGit $clone @('cat-file', '-e', "$revision^{commit}")).ExitCode -ne 0) { return $unreadable }
		}
	}
	$rounds.Reverse()
	return [pscustomobject] @{ Readable = $true; Rounds = [object[]] $rounds.ToArray(); Tip = $tipSha }
}

# The closed rounds, counted back from the last, whose highest result is Critical or High; 0 when the last is open.
function Get-ReviewRoundConsecutiveHigh([object[]] $Rounds) {
	$count = 0
	for ($i = $Rounds.Count - 1; $i -ge 0; $i--) {
		if ($null -eq $Rounds[$i].Close -or $Rounds[$i].Close.Highest -cnotin @('High', 'Critical')) { break }
		$count++
	}
	return $count
}

function Get-ReviewRoundInventory([string] $Root, [string] $Baseline, [string] $Since, [string] $Head) {
	$parameters = @{ RepositoryRoot = $Root; Baseline = $Baseline }
	if (-not [string]::IsNullOrEmpty($Since)) { $parameters.Since = $Since }
	if (-not [string]::IsNullOrEmpty($Head)) { $parameters.Head = $Head } else { $parameters.IncludeUntracked = $true }
	$document = Get-SessionChangeInventory @parameters
	if ($null -eq $document -or $document.status -cne 'pass') {
		$reason = if ($null -ne $document) { "$($document.code): $($document.message)" } else { 'no result returned' }
		$exception = [InvalidOperationException]::new("the session change inventory did not pass: $reason")
		$exception.Data['ReviewRoundInventory'] = $true
		throw $exception
	}
	return $document
}

# The next round after Rounds (oldest first, every one closed) by the Get-ChangeWorkflowSteps.ps1 header rules, from
# each key's diff D(S_k) to Head (the working tree when empty).
function Get-ReviewRoundPlan([string] $Root, [object[]] $Rounds, [string] $Head, [string] $Baseline, [int] $Tier, [bool] $Landing) {
	$last = if ($Rounds.Count -gt 0) { $Rounds[$Rounds.Count - 1] } else { $null }
	$since = @{}
	$high = @{}
	foreach ($key in $script:ReviewRoundKeys.Keys) { $since[$key] = ''; $high[$key] = $false }
	foreach ($round in $Rounds) {
		if ($round.Phase -ceq 'sweep') { foreach ($key in $script:ReviewRoundKeys.Keys) { $since[$key] = $round.Snapshot; $high[$key] = $false }; continue }
		foreach ($result in $round.Close.Results) {
			$high[$result.Key] = $result.Severity -cin @('High', 'Critical')
			if (-not $high[$result.Key]) { $since[$result.Key] = $round.Snapshot }
		}
	}
	$bSince = if ($null -ne $last) { $last.Snapshot } else { '' }
	$docs = @{}
	foreach ($point in @(@($since.Values) + @($bSince) | Sort-Object -Unique -CaseSensitive)) { $docs[$point] = Get-ReviewRoundInventory $Root $Baseline $point $Head }
	$owed = [ordered]@{}
	$reviewTriggers = @($script:ReviewRoundKeys.Values | Where-Object { $null -ne $_.Trigger } | ForEach-Object { $_.Trigger })
	$anyFires = { param($Triggers) @($reviewTriggers | Where-Object { $Triggers.$_ }).Count -gt 0 }
	foreach ($key in $script:ReviewRoundKeys.Keys) {
		$t = $docs[$since[$key]].triggers
		$trigger = $script:ReviewRoundKeys[$key].Trigger
		$adversarial = $key -ceq '/adversarial-review'
		$fires = if ($adversarial) { $Tier -eq 3 -and (& $anyFires $t) } elseif ($null -ne $trigger) { [bool] $t.$trigger } else { $false }
		if (($fires -or $high[$key]) -and ($adversarial -or $null -ne $trigger) -and -not $docs.ContainsKey('')) { $docs[''] = Get-ReviewRoundInventory $Root $Baseline '' $Head }
		$owed[$key] = ($fires -or $high[$key]) -and $(if ($adversarial) { & $anyFires $docs[''].triggers } else { $null -eq $trigger -or [bool] $docs[''].triggers.$trigger })
	}
	$db = $docs[$bSince]
	$keysIn = { param([string] $Group) [string[]] @($owed.Keys | Where-Object { $owed[$_] -and $script:ReviewRoundKeys[$_].Group -ceq $Group }) }
	$code = @(& $keysIn 'code')
	$words = @(& $keysIn 'words')
	$neutral = @(& $keysIn 'neutral')
	$plan = [pscustomobject] @{
		Done = $false; Phase = ''; Kind = $(if ($null -ne $last -and $last.Close.Next -ceq 'check') { 'check' } else { 'ordinary' })
		Keys = [string[]] @(); Carried = [string[]] @(); OverEngineering = [bool] $db.triggers.overEngineeringReview; DB = $db; DW = $docs[$since['/comment-review']]
	}
	# T1: no words key prints while a code key is owed; a withheld key keeps its S_k.
	if ($code.Count -gt 0) { $plan.Phase = 'code'; $plan.Keys = $code + $neutral }
	elseif ($words.Count -gt 0) { $plan.Phase = 'words'; $plan.Keys = $words + $neutral }
	elseif ($neutral.Count -gt 0) { $plan.Phase = 'code'; $plan.Keys = $neutral }
	elseif (-not $Landing -and ($owed['/verify-acceptance'] -or $null -eq $last -or $since['/verify-acceptance'] -cne $last.Snapshot)) {
		$plan.Phase = 'acceptance'
		$plan.Keys = [string[]] @('/verify-acceptance')
		return $plan
	}
	else {
		$plan.Done = $true
		return $plan
	}
	if ($Tier -eq 1 -and $plan.Keys -ccontains '/coherence-review' -and @($plan.Keys | Where-Object { $_ -cin @('/repo-code-review', '/glsl-review', '/comment-review') }).Count -eq 0) {
		$plan.Phase = 'combined'
		$plan.Carried = [string[]] @(@('/external-skill-creator', '/progressive-disclosure-review') | Where-Object { $plan.Keys -ccontains $_ }) + @(if (-not $Landing) { '/verify-acceptance' })
		$plan.Keys = [string[]] @(@('/coherence-review') + @($plan.Keys | Where-Object { $_ -ceq '/adversarial-review' }))
	}
	return $plan
}

# The engine review-round chain and, unless -ChainOnly, the round it owes next from the diffs to -Head (the working
# tree when omitted): an open last round is owed again as recorded. -Baseline defaults to the last round's. -Chain
# takes the state an earlier -ChainOnly call returned instead of reading the ref again, so a writer plans from the
# same read whose Tip it compares and swaps. Failure names an inventory that did not pass; Plan.Done means the loop is
# done.
function Get-ReviewRoundState {
	[CmdletBinding()]
	param(
		[Parameter(Mandatory)][string] $RepositoryRoot,
		[string] $Baseline,
		[string] $Head,
		[ValidateRange(0, 3)][int] $Tier = 0,
		[switch] $Landing,
		[switch] $ChainOnly,
		[object] $Chain
	)

	$ErrorActionPreference = 'Stop'
	$root = Get-AgentCanonicalPath $RepositoryRoot
	if ($null -eq $Chain) { $Chain = Read-ReviewRoundChain $root }
	$rounds = [object[]] $Chain.Rounds
	$last = if ($rounds.Count -gt 0) { $rounds[$rounds.Count - 1] } else { $null }
	$state = [pscustomobject] @{
		Readable = $Chain.Readable
		Tip = $Chain.Tip
		Rounds = $rounds
		Last = $last
		Open = $null -ne $last -and $null -eq $last.Close
		Number = $(if ($null -ne $last) { $last.Number + 1 } else { 1 })
		ConsecutiveHigh = Get-ReviewRoundConsecutiveHigh $rounds
		Plan = $null
		Failure = $null
	}
	if ($ChainOnly) { return $state }
	if ([string]::IsNullOrEmpty($Baseline)) {
		if ($null -eq $last) { throw '-Baseline is required when no review round exists.' }
		$Baseline = $last.Baseline
	}
	try {
		if ($state.Open) {
			$plan = Get-ReviewRoundPlan $root ([object[]] @($rounds | Select-Object -First ($rounds.Count - 1))) $last.Snapshot $Baseline $Tier $Landing.IsPresent
			$plan.Done = $false
			$plan.Phase = $last.Phase
			$plan.Kind = $last.Kind
			$plan.Carried = $last.Carried
			$plan.Keys = [string[]] @($last.Owes | Where-Object { $last.Carried -cnotcontains $_ })
			$state.Plan = $plan
		}
		else { $state.Plan = Get-ReviewRoundPlan $root $rounds $Head $Baseline $Tier $Landing.IsPresent }
	}
	catch {
		if (-not $_.Exception.Data.Contains('ReviewRoundInventory')) { throw }
		$state.Failure = $_.Exception.Message
	}
	return $state
}

# The landing gate: null when no review is owed for the landing candidate (the engine candidate, or HEAD with an empty
# engine diff when only a Coretide candidate is supplied, with the clone's HEAD as the Coretide side), the question the
# step script asks of the working tree; else the refusal's code and message. A candidate the rounds cannot be evaluated
# at is refused as inventory.round-evaluation-failed, which no further round cures. Saves and restores the per-run state.
function Get-ReviewRoundCoverageFailure([string] $Root, [string] $EngineBase, [string] $EngineCandidate, [string] $CoretideCandidate, [Collections.Generic.HashSet[string]] $SavePlanAuthorizedPaths) {
	$saved = Save-InventoryState
	try {
		$engineSide = -not [string]::IsNullOrEmpty($EngineCandidate)
		$coretideSide = -not [string]::IsNullOrEmpty($CoretideCandidate)
		$script:Root = $Root
		$script:BlobHashes = @{}
		$script:HeadPaths = $null
		$script:EngineSide = $null
		$script:Coretide = $null
		$script:BaselineSha = ''
		$script:HeadSha = ''
		$engineHead = (Invoke-InventoryGit @('rev-parse', '--verify', "$(if ($engineSide) { $EngineCandidate } else { 'HEAD' })^{commit}")).Stdout.Trim()
		$engineMergeBase = if ($engineSide) { (Invoke-InventoryGit @('merge-base', $EngineBase, $engineHead)).Stdout.Trim() } else { '' }
		if ($coretideSide) {
			$clone = Get-AgentCoretideRepository $Root
			if ($null -eq $clone) { throw "'$Root' holds no Coretide clone for the Coretide candidate." }
			$coretideHead = (Invoke-ReviewChainGit $clone @('rev-parse', '--verify', "$CoretideCandidate^{commit}") $false).Stdout.Trim()
			if ((Invoke-ReviewChainGit $clone @('rev-parse', '--verify', 'HEAD^{commit}') $false).Stdout.Trim() -cne $coretideHead) { throw "The Coretide candidate $coretideHead is not the clone's HEAD." }
			$coretideBase = (Invoke-ReviewChainGit $clone @('merge-base', $coretideHead, '@{upstream}') $false).Stdout.Trim()
			$script:Coretide = @{ Root = $clone; BaselineSha = $coretideBase; HeadSha = $coretideHead; HeadPaths = $null }
		}
		if ($null -ne $SavePlanAuthorizedPaths) {
			$reviewed = [Collections.Generic.List[object]]::new()
			if ($engineSide) {
				$script:HeadSha = $engineHead
				$script:BaselineSha = $engineMergeBase
				foreach ($row in (Get-InventoryRawRow $script:BaselineSha $script:HeadSha)) { $reviewed.Add([pscustomobject] @{ status = $row.Status; path = $row.Path; currentMode = $row.DestinationMode }) }
			}
			if ($coretideSide) {
				Enter-CoretideSide
				try { foreach ($row in (Get-InventoryRawRow $script:BaselineSha $script:HeadSha)) { $reviewed.Add([pscustomobject] @{ status = $row.Status; path = $script:CoretidePrefix + $row.Path; currentMode = $row.DestinationMode }) } }
				finally { Exit-CoretideSide }
			}
			# Only the supplied sides' authorized paths, so a save spanning both repositories passes each landing script.
			$sidePaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
			foreach ($path in $SavePlanAuthorizedPaths) {
				$coretidePath = $path.StartsWith($script:CoretidePrefix, [StringComparison]::Ordinal)
				if (($coretidePath -and $coretideSide) -or (-not $coretidePath -and $engineSide)) { [void] $sidePaths.Add($path) }
			}
			if (Test-SavePlanOnlyReviewedRows $reviewed $sidePaths) { return $null }
		}
		$chain = Get-ReviewRoundState -RepositoryRoot $Root -ChainOnly
		if ($chain.Open) {
			return [pscustomobject] @{ Code = 'inventory.review-round-unfinished'; Message = "Review round $($chain.Last.Number) is open; close it with Get-ChangeWorkflowSteps.ps1 -CloseRound and finish the rounds it owes." }
		}
		$baseline = if ($engineSide) { $engineMergeBase } else { $engineHead }
		# The tier the last non-sweep round recorded, so /adversarial-review is owed as the step script owes it.
		$tier = 0
		foreach ($round in $chain.Rounds) { if ($round.Tier -gt 0) { $tier = $round.Tier } }
		$at = Get-ReviewRoundState -RepositoryRoot $Root -Chain $chain -Baseline $baseline -Head $engineHead -Tier $tier -Landing
		if ($null -ne $at.Failure) {
			return [pscustomobject] @{ Code = 'inventory.round-evaluation-failed'; Message = "The review rounds could not be evaluated at the landing candidate: $($at.Failure)" }
		}
		if ($at.Plan.Done) { return $null }
		$owed = "a $($at.Plan.Phase) round owing $((@($at.Plan.Keys) + @($at.Plan.Carried)) -join ', ')"
		if ($chain.Rounds.Count -eq 0) {
			return [pscustomobject] @{ Code = 'inventory.review-round-missing'; Message = "No readable review round covers this change and $owed is owed; run the review rounds Get-ChangeWorkflowSteps.ps1 prints before landing." }
		}
		return [pscustomobject] @{ Code = 'inventory.review-round-unfinished'; Message = "At the landing candidate $owed is owed after review round $($chain.Last.Number); rerun Get-ChangeWorkflowSteps.ps1." }
	}
	finally { Restore-InventoryState $saved }
}

# Test-ReviewRoundCoverage -RepositoryRoot <engine worktree> [-EngineBase <commit> -EngineCandidate <commit>]
# [-CoretideCandidate <commit>] [-SavePlanPaths '<comma-separated paths>']: nothing when no review is owed for the
# landing candidate, else an object with the refusal's Code and its Message.
function Test-ReviewRoundCoverage {
	[CmdletBinding()]
	param(
		[Parameter(Mandatory)][string] $RepositoryRoot,
		[string] $EngineBase,
		[string] $EngineCandidate,
		[string] $CoretideCandidate,
		[string] $SavePlanPaths
	)

	$ErrorActionPreference = 'Stop'
	$authorized = $null
	if (-not [string]::IsNullOrWhiteSpace($SavePlanPaths)) {
		$saveError = $null
		$authorized = Get-SavePlanAuthorizedPaths $SavePlanPaths ([ref] $saveError)
		if ($null -eq $authorized) { return [pscustomobject] @{ Code = 'inventory.save-plan-paths-invalid'; Message = $saveError } }
	}
	return Get-ReviewRoundCoverageFailure (Get-AgentCanonicalPath $RepositoryRoot) $EngineBase $EngineCandidate $CoretideCandidate $authorized
}

# The Baseline, Coretide-Snapshot, and Coretide-Baseline lines of a commit's message, each null when absent.
function Get-InventoryRoundField([string] $Sha) {
	$message = (Invoke-InventoryGit @('show', '-s', '--format=%B', $Sha)).Stdout
	$field = { param([string] $Name) $m = [regex]::Match($message, "(?m)^$([regex]::Escape($Name)): ([0-9a-f]{40})\r?$"); if ($m.Success) { $m.Groups[1].Value } else { $null } }
	return [pscustomobject] @{ Baseline = & $field 'Baseline'; CoretideSnapshot = & $field 'Coretide-Snapshot'; CoretideBaseline = & $field 'Coretide-Baseline' }
}

# One side's entries from DiffBase to the head, untracked entries included. On a -Since run (Since holds the round
# snapshot's own Baseline, or null, and the side's session baseline), an untracked path the since commit's tree holds
# is compared with that entry by git hash-object (equal drops both rows, different leaves one M entry), and only rows
# whose path or old path is a session path are kept; RoundPaths receives the since commit's own session paths.
function Get-InventorySideEntry([string] $DiffBase, [string] $HeadSha, [bool] $WorkingTreeHead, [object[]] $Untracked, [object] $Since, [Collections.Generic.HashSet[string]] $RoundPaths) {
	$binaryPaths = Get-InventoryBinaryPath $DiffBase $HeadSha
	$untrackedByPath = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::Ordinal)
	if ($null -ne $Since) { foreach ($entry in $Untracked) { $untrackedByPath[$entry.Path] = $entry } }
	$reconciled = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
	$entries = [Collections.Generic.List[object]]::new()
	foreach ($row in (Get-InventoryRawRow $DiffBase $HeadSha)) {
		if ($row.Status -ceq 'D' -and $untrackedByPath.ContainsKey($row.Path)) {
			[void] $reconciled.Add($row.Path)
			if ((Invoke-InventoryGit @('hash-object', '--', $row.Path)).Stdout.Trim() -ceq $row.SourceOid) { continue }
			$current = $untrackedByPath[$row.Path]
			$entries.Add([pscustomobject] @{
				Status = 'M'
				Path = $row.Path
				OldPath = $null
				Baseline = Get-InventoryIdentity $row.SourceMode $row.SourceOid $row.Path
				Current = $current.Current
				Class = $current.Class
				OldClass = Get-PathClass $row.Path $row.SourceMode $false
				Untracked = $true
			})
			continue
		}
		$entries.Add((Get-InventoryEntry $row $binaryPaths $WorkingTreeHead))
	}
	foreach ($entry in $Untracked) { if (-not $reconciled.Contains($entry.Path)) { $entries.Add($entry) } }
	if ($null -eq $Since) { return , $entries }
	if ($null -ne $Since.Baseline) {
		foreach ($row in (Get-InventoryRawRow $Since.Baseline $DiffBase)) { foreach ($side in @($row.Path, $row.OldPath)) { if ($null -ne $side) { [void] $RoundPaths.Add($side) } } }
	}
	$sessionPaths = [Collections.Generic.HashSet[string]]::new($RoundPaths, [StringComparer]::Ordinal)
	$sessionRows = Get-InventoryRawRow $Since.SessionBaseline $HeadSha
	foreach ($row in $sessionRows) { foreach ($side in @($row.Path, $row.OldPath)) { if ($null -ne $side) { [void] $sessionPaths.Add($side) } } }
	foreach ($entry in $Untracked) { [void] $sessionPaths.Add($entry.Path) }
	$kept = [Collections.Generic.List[object]]::new()
	$keptPaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
	foreach ($entry in $entries) {
		if ($sessionPaths.Contains($entry.Path) -or ($null -ne $entry.OldPath -and $sessionPaths.Contains($entry.OldPath))) {
			$kept.Add($entry)
			foreach ($side in @($entry.Path, $entry.OldPath)) { if ($null -ne $side) { [void] $keptPaths.Add($side) } }
		}
	}
	if ($null -eq $Since.Baseline) { return , $kept }
	# A session path that was not one at the since commit is new to review even when its bytes equal that commit's, so
	# it keeps its session row.
	$sessionBinaryPaths = $null
	foreach ($row in $sessionRows) {
		$new = @(@($row.Path, $row.OldPath) | Where-Object { $null -ne $_ -and -not $RoundPaths.Contains($_) -and -not $keptPaths.Contains($_) })
		if ($new.Count -eq 0) { continue }
		if ($null -eq $sessionBinaryPaths) { $sessionBinaryPaths = Get-InventoryBinaryPath $Since.SessionBaseline $HeadSha }
		$kept.Add((Get-InventoryEntry $row $sessionBinaryPaths $WorkingTreeHead))
		foreach ($side in @($row.Path, $row.OldPath)) { if ($null -ne $side) { [void] $keptPaths.Add($side) } }
	}
	return , $kept
}

# True when a modified C++ or GLSL path's change is proven to touch comment text alone: both sides tokenize into the
# same number of tokens, every non-comment token is byte-identical, and each comment pair holds as many line feeds.
function Test-InventoryCommentOnlyChange([string] $Path) {
	$before = Get-InventorySideText $false $Path
	$after = Get-InventorySideText $true $Path
	if ($null -eq $before -or $null -eq $after) { return $false }
	# The compiler splices a backslash, trailing whitespace, and the line break after it before it finds tokens, so a
	# splice anywhere on either side can move a token boundary away from where this tokenizer puts it.
	$splice = '\\[ \t\v\f]*\r?\n'
	if ($before -cmatch $splice -or $after -cmatch $splice) { return $false }
	$pattern = Get-AgentCppTokenPattern
	$beforeTokens = [regex]::Matches($before, $pattern)
	$afterTokens = [regex]::Matches($after, $pattern)
	if ($beforeTokens.Count -ne $afterTokens.Count) { return $false }
	for ($i = 0; $i -lt $beforeTokens.Count; $i++) {
		$left = $beforeTokens[$i]
		$right = $afterTokens[$i]
		if ($left.Groups['comment'].Success -ne $right.Groups['comment'].Success) { return $false }
		if ($left.Groups['comment'].Success) {
			if (($left.Value.Split("`n").Count) -ne ($right.Value.Split("`n").Count)) { return $false }
		}
		elseif ($left.Value -cne $right.Value) { return $false }
	}
	return $true
}

function Test-ManifestRootPath([string] $Path) {
	$slash = $Path.IndexOf('/')
	if ($slash -le 0) { return $false }
	return $script:ManifestRoots -ccontains $Path.Substring(0, $slash)
}

function Get-ManifestTreeEntries([string] $CommitSha) {
	# `-z` keeps the path unquoted and record-delimited; split only the fixed header/tab boundary
	# so tabs and other path characters are never treated as delimiters.
	$run = Invoke-InventoryGit @('ls-tree', '-r', '-z', '--full-tree', $CommitSha, '--')
	$entries = [Collections.Generic.List[object]]::new()
	foreach ($record in (($run.Stdout -split "`0") | Where-Object { -not [string]::IsNullOrEmpty($_) })) {
		$separator = $record.IndexOf("`t")
		if ($separator -le 0) { throw 'git ls-tree returned a malformed record without a header separator.' }
		$header = $record.Substring(0, $separator)
		$path = $record.Substring($separator + 1)
		$fields = $header -split ' ', 3
		if ($fields.Count -ne 3) { throw 'git ls-tree returned a malformed record header.' }
		$mode = $fields[0]
		if ($fields[1] -cne 'blob' -or $script:ManifestModes -cnotcontains $mode) { continue }
		if (-not (Test-ManifestRootPath $path)) { continue }
		$class = Get-PathClass $path $mode $false
		if ($script:CppClasses -cnotcontains $class) { continue }
		$entries.Add([pscustomobject][ordered]@{
			path = $path
			class = $class
			mode = $mode
			sha256 = Get-InventoryBlobSha256 $fields[2]
		})
	}
	$entries.Sort([Comparison[object]] {
		param($left, $right)
		return [string]::CompareOrdinal($left.path, $right.path)
	})
	return , $entries.ToArray()
}

function Get-SessionManifest([string] $CommitSha, [string] $TreeSha, [object[]] $Entries) {
	return [pscustomobject][ordered]@{
		schemaVersion = 'broken-engine-cpp-manifest/v1'
		commitSha = $CommitSha
		treeSha = $TreeSha
		classifierSha256 = Get-InventoryModuleSha256
		entries = [object[]] $Entries
	}
}

function Get-SessionChangeInventory {
	[CmdletBinding()]
	param(
		[Parameter(Mandatory)][string] $RepositoryRoot,
		[Parameter(Mandatory)][string] $Baseline,
		[string] $Head,
		[switch] $IncludeUntracked,
		[string[]] $UntrackedPath,
		[string[]] $PathPrefix,
		[switch] $Regions,
		[switch] $CoherenceOnly,
		[switch] $SinceLastRound,
		[switch] $Landing,
		[switch] $EmitManifest,
		[string] $SavePlanPaths,
		# Module-only: entries, counts, and triggers describe the diff from this commit (a review-round snapshot) to
		# the head, kept to session paths, with the comment-text-only rule (Get-ChangeWorkflowSteps.ps1 header).
		[string] $Since,
		[ref] $Envelope
	)

	$ErrorActionPreference = 'Stop'
	$script:Root = $null
	$script:BaselineSha = ''
	$script:HeadSha = ''
	$script:HeadPaths = $null
	# The session Coretide clone's Root, BaselineSha, HeadSha, and HeadPaths, or null; Enter-CoretideSide swaps them in.
	$script:Coretide = $null
	$script:EngineSide = $null
	$script:BlobHashes = @{}

	$result = [pscustomobject][ordered]@{
		schemaVersion = 'broken-engine-session-change-inventory/v2'
		status = 'error'
		code = 'internal.error'
		message = 'Session change inventory did not run.'
		baselineSha = $null
		headSha = $null
		coretide = $null
		entries = @()
		counts = $null
		triggers = $null
		regions = $null
		landing = $null
		truncated = $false
		truncation = $null
	}
	if ($null -ne $Envelope) { $Envelope.Value = $result }

	try {
		$manifestConflict = $EmitManifest -and ($Regions -or $Landing -or $PSBoundParameters.ContainsKey('Head') -or $IncludeUntracked -or $PSBoundParameters.ContainsKey('UntrackedPath'))
		# Those two modes never read the filtered entries or regions, so they would silently ignore the filter.
		$prefixConflict = $PSBoundParameters.ContainsKey('PathPrefix') -and ($Landing -or $EmitManifest)
		$coherenceConflict = $CoherenceOnly -and ($Landing -or $EmitManifest)
		$savePlanConflict = $PSBoundParameters.ContainsKey('SavePlanPaths') -and -not $Landing
		$sinceConflict = $PSBoundParameters.ContainsKey('Since') -and ($Regions -or $Landing -or $EmitManifest)
		$sinceLastRoundConflict = $SinceLastRound -and ($Landing -or $EmitManifest -or $PSBoundParameters.ContainsKey('Since'))
		if (($Regions -and $Landing) -or $manifestConflict -or $prefixConflict -or $coherenceConflict -or $savePlanConflict -or $sinceConflict -or $sinceLastRoundConflict) {
			$message = if ($manifestConflict) { 'The -EmitManifest mode is exclusive with -Regions, -Landing, -Head, -IncludeUntracked, and -UntrackedPath.' } elseif ($prefixConflict) { 'The -PathPrefix filter is exclusive with -Landing and -EmitManifest.' } elseif ($coherenceConflict) { 'The -CoherenceOnly filter is exclusive with -Landing and -EmitManifest.' } elseif ($savePlanConflict) { 'The -SavePlanPaths option is valid only with -Landing.' } elseif ($sinceConflict) { 'The -Since option is exclusive with -Regions, -Landing, and -EmitManifest.' } elseif ($sinceLastRoundConflict) { 'The -SinceLastRound filter is exclusive with -Landing, -EmitManifest, and -Since.' } else { 'Supply at most one of -Regions and -Landing.' }
			return Set-InventoryOutcome $result 'blocked' 'inventory.mode-conflict' $message
		}
		$script:Root = Get-AgentCanonicalPath $RepositoryRoot
		if (-not (Test-Path -LiteralPath $script:Root -PathType Container)) {
			return Set-InventoryOutcome $result 'blocked' 'inventory.repository-root-invalid' "-RepositoryRoot must be an existing directory: '$RepositoryRoot'."
		}
		$topLevel = Invoke-InventoryGit @('rev-parse', '--show-toplevel') $true
		if ($topLevel.ExitCode -ne 0) {
			return Set-InventoryOutcome $result 'blocked' 'inventory.repository-root-invalid' "-RepositoryRoot is not inside a Git repository: '$RepositoryRoot'."
		}
		# Every diff below is relative to the repository root, so a subdirectory root would silently
		# report an inventory that omits everything outside it.
		$topLevelPath = Get-AgentCanonicalPath ($topLevel.Stdout.Trim())
		if (-not [string]::Equals($topLevelPath, $script:Root, [StringComparison]::OrdinalIgnoreCase)) {
			return Set-InventoryOutcome $result 'blocked' 'inventory.root-not-toplevel' "-RepositoryRoot must be the repository toplevel '$topLevelPath', not '$RepositoryRoot'."
		}
		if ($Landing -and [string]::IsNullOrWhiteSpace($Head)) {
			return Set-InventoryOutcome $result 'blocked' 'inventory.landing-requires-head' '-Landing compares the reviewed diff and requires a commit-valued -Head.'
		}
		$savePlanAuthorizedPaths = $null
		if ($PSBoundParameters.ContainsKey('SavePlanPaths')) {
			$savePlanError = $null
			$savePlanAuthorizedPaths = Get-SavePlanAuthorizedPaths $SavePlanPaths ([ref] $savePlanError)
			if ($null -eq $savePlanAuthorizedPaths) {
				return Set-InventoryOutcome $result 'blocked' 'inventory.save-plan-paths-invalid' $savePlanError
			}
		}
		if ($Baseline -cnotmatch '^[0-9a-fA-F]{40}$') {
			return Set-InventoryOutcome $result 'blocked' 'inventory.baseline-unresolved' "-Baseline must be a full 40-character Git SHA: '$Baseline'."
		}
		$baselineRun = Invoke-InventoryGit @('rev-parse', '--verify', '--quiet', "$Baseline^{commit}") $true
		if ($baselineRun.ExitCode -ne 0) {
			return Set-InventoryOutcome $result 'blocked' 'inventory.baseline-unresolved' "The baseline does not resolve to a commit in this repository: '$Baseline'."
		}
		$baselineSha = $baselineRun.Stdout.Trim()
		$script:BaselineSha = $baselineSha
		$result.baselineSha = $baselineSha
		$headSha = ''
		if (-not [string]::IsNullOrWhiteSpace($Head)) {
			$headRun = Invoke-InventoryGit @('rev-parse', '--verify', '--quiet', "$Head^{commit}") $true
			if ($headRun.ExitCode -ne 0) {
				return Set-InventoryOutcome $result 'blocked' 'inventory.head-unresolved' "The head revision does not resolve to a commit in this repository: '$Head'."
			}
			$headSha = $headRun.Stdout.Trim()
			$script:HeadSha = $headSha
			$result.headSha = $headSha
		}
		$sinceSha = ''
		$sinceFields = $null
		if ($PSBoundParameters.ContainsKey('Since')) {
			$sinceRun = Invoke-InventoryGit @('rev-parse', '--verify', '--quiet', "$Since^{commit}") $true
			if ($sinceRun.ExitCode -ne 0) {
				return Set-InventoryOutcome $result 'blocked' 'inventory.since-unresolved' "The -Since revision does not resolve to a commit in this repository: '$Since'."
			}
			$sinceSha = $sinceRun.Stdout.Trim()
			$sinceFields = Get-InventoryRoundField $sinceSha
		}
		if ($EmitManifest) {
			$treeRun = Invoke-InventoryGit @('rev-parse', '--verify', "$baselineSha^{tree}")
			$treeSha = $treeRun.Stdout.Trim()
			if ($treeSha -notmatch '^[0-9a-fA-F]{40}$') { throw "The baseline tree did not resolve to a full Git SHA: '$treeSha'." }
			$manifestEntries = Get-ManifestTreeEntries $baselineSha
			return Get-SessionManifest $baselineSha $treeSha ([object[]] $manifestEntries)
		}

		$sincePaths = $null
		if ($SinceLastRound) {
			$roundState = Get-ReviewRoundState -RepositoryRoot $script:Root -ChainOnly
			$lastClosed = $null
			foreach ($round in $roundState.Rounds) {
				if ($null -ne $round.Close) { $lastClosed = $round }
			}
			if ($null -ne $lastClosed) {
				$sinceParams = @{
					RepositoryRoot = $script:Root
					Baseline = $baselineSha
					Since = $lastClosed.Snapshot
				}
				if (-not [string]::IsNullOrEmpty($headSha)) { $sinceParams.Head = $headSha }
				if ($IncludeUntracked) { $sinceParams.IncludeUntracked = $true }
				if ($PSBoundParameters.ContainsKey('UntrackedPath')) { $sinceParams.UntrackedPath = $UntrackedPath }
				$saved = Save-InventoryState
				try { $sinceDoc = Get-SessionChangeInventory @sinceParams }
				finally { Restore-InventoryState $saved }
				if ($sinceDoc.status -cne 'pass') {
					return Set-InventoryOutcome $result $sinceDoc.status $sinceDoc.code $sinceDoc.message
				}
				$sincePaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
				foreach ($entry in $sinceDoc.entries) {
					[void] $sincePaths.Add($entry.path)
					if (-not [string]::IsNullOrEmpty($entry.oldPath)) { [void] $sincePaths.Add($entry.oldPath) }
				}
			}
		}

		$prefixes = ConvertTo-NormalizedPathFilter $PathPrefix $true
		$hasUntrackedFilter = $PSBoundParameters.ContainsKey('UntrackedPath')
		$untrackedFilter = [Collections.Generic.List[string]]::new()
		if ($hasUntrackedFilter) { $untrackedFilter = ConvertTo-NormalizedPathFilter $UntrackedPath $false }

		$workingTreeHead = [string]::IsNullOrEmpty($headSha)
		# On a -Since run every diff, and routing's baseline-side text, starts at the since commit.
		$diffBase = if ($sinceSha) { $sinceSha } else { $baselineSha }
		$script:BaselineSha = $diffBase
		$unlistedUntracked = 0
		$untrackedEntries = @()
		if ($workingTreeHead) {
			# Untracked paths exist only against the working tree; a commit-valued head has none.
			$unlistedReference = 0
			$untrackedEntries = @(Get-InventoryUntrackedEntry $IncludeUntracked.IsPresent $hasUntrackedFilter $untrackedFilter '' ([ref] $unlistedReference))
			$unlistedUntracked = $unlistedReference
		}
		$sinceSide = if ($sinceSha) { [pscustomobject] @{ Baseline = $sinceFields.Baseline; SessionBaseline = $baselineSha } } else { $null }
		# The since commit's own session paths, both repositories, where the comment-text-only rule may apply.
		$roundPaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
		$entries = Get-InventorySideEntry $diffBase $headSha $workingTreeHead ([object[]] $untrackedEntries) $sinceSide $roundPaths
		$headFields = if ($workingTreeHead) { $null } else { Get-InventoryRoundField $headSha }
		$coretideRegions = [Collections.Generic.List[object]]::new()
		$coretideRoot = Get-AgentCoretideRepository $script:Root
		if ($null -ne $coretideRoot) {
			$script:Coretide = @{ Root = $coretideRoot; BaselineSha = ''; HeadSha = ''; HeadPaths = $null }
			Enter-CoretideSide
			try {
				# A review-round head's Coretide-Baseline is the merge base its snapshot froze, so a later rebase of the
				# clone onto upstream never reads as a session change at that snapshot.
				$coretideSessionBaseline = if ($null -ne $headFields -and $null -ne $headFields.CoretideBaseline) { $headFields.CoretideBaseline } else { (Invoke-InventoryGit @('merge-base', 'HEAD', '@{upstream}')).Stdout.Trim() }
				# With a commit-valued engine head the clone's HEAD, or the Coretide snapshot a review-round head names,
				# stands for its side and its working tree is left out.
				if (-not $workingTreeHead) {
					$coretideHead = if ($null -ne $headFields.CoretideSnapshot) { "$($headFields.CoretideSnapshot)^{commit}" } else { 'HEAD^{commit}' }
					$coretideHeadRun = Invoke-InventoryGit @('rev-parse', '--verify', '--quiet', $coretideHead) $true
					if ($coretideHeadRun.ExitCode -ne 0) {
						return Set-InventoryOutcome $result 'blocked' 'inventory.head-unresolved' "The Coretide head does not resolve to a commit in the clone: '$coretideHead'."
					}
					$script:HeadSha = $coretideHeadRun.Stdout.Trim()
				}
				$script:BaselineSha = if ($sinceSha -and $null -ne $sinceFields.CoretideSnapshot) { $sinceFields.CoretideSnapshot } else { $coretideSessionBaseline }
				$script:Coretide.BaselineSha = $script:BaselineSha
				$script:Coretide.HeadSha = $script:HeadSha
				$coretideUntracked = @()
				if ($workingTreeHead) {
					$unlistedReference = 0
					$coretideUntracked = @(Get-InventoryUntrackedEntry $IncludeUntracked.IsPresent $hasUntrackedFilter $untrackedFilter $script:CoretidePrefix ([ref] $unlistedReference))
					$unlistedUntracked += $unlistedReference
				}
				if ($Regions) {
					foreach ($region in (Get-RegionTable $script:BaselineSha $script:HeadSha ([object[]] $coretideUntracked))) {
						$region.path = $script:CoretidePrefix + $region.path
						$coretideRegions.Add($region)
					}
				}
				$coretideSince = if ($sinceSha) { [pscustomobject] @{ Baseline = $sinceFields.CoretideBaseline; SessionBaseline = $coretideSessionBaseline } } else { $null }
				$coretideRoundPaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
				foreach ($entry in (Get-InventorySideEntry $script:BaselineSha $script:HeadSha $workingTreeHead ([object[]] $coretideUntracked) $coretideSince $coretideRoundPaths)) {
					$entries.Add((Add-CoretidePrefix $entry))
				}
				foreach ($path in $coretideRoundPaths) { [void] $roundPaths.Add($script:CoretidePrefix + $path) }
			}
			finally { Exit-CoretideSide }
			$result.coretide = [pscustomobject][ordered]@{
				baselineSha = $coretideSessionBaseline
				headSha = if ($workingTreeHead) { $null } else { $script:Coretide.HeadSha }
			}
		}
		$sorted = [Collections.Generic.List[object]]::new($entries)
		$sorted.Sort([Comparison[object]] {
			param($left, $right)
			return [string]::CompareOrdinal($left.Path, $right.Path)
		})

		# Counts and triggers always describe the complete inventory, never the scoped emission.
		$result.counts = Get-InventoryCount ([object[]] $sorted.ToArray()) $unlistedUntracked
		$triggers = Get-RoutingTrigger ([object[]] $sorted.ToArray())
		if ($sinceSha) {
			# A modification of one of the since commit's own session paths proven to touch comment text alone
			# fires none of the code-only triggers; every other entry keeps every trigger.
			$codeEntries = [Collections.Generic.List[object]]::new()
			foreach ($entry in $sorted) {
				if ($entry.Status -ceq 'M' -and $script:SourceMembershipClasses -ccontains $entry.Class -and $roundPaths.Contains($entry.Path) -and (Test-InventoryCommentOnlyChange $entry.Path)) { continue }
				$codeEntries.Add($entry)
			}
			if ($codeEntries.Count -ne $sorted.Count) {
				$codeTriggers = Get-RoutingTrigger ([object[]] $codeEntries.ToArray())
				foreach ($name in $script:CodeOnlyTriggers) { $triggers.$name = $codeTriggers.$name }
			}
		}
		$result.triggers = $triggers
		$scoped = $sorted
		if ($prefixes.Count -gt 0) {
			$scoped = [Collections.Generic.List[object]]::new()
			foreach ($entry in $sorted) { if (Test-InventoryPathInScope $entry.Path $prefixes) { $scoped.Add($entry) } }
		}
		if ($null -ne $sincePaths) {
			$sinceScoped = [Collections.Generic.List[object]]::new()
			foreach ($entry in $scoped) { if ($sincePaths.Contains($entry.Path)) { $sinceScoped.Add($entry) } }
			$scoped = $sinceScoped
		}
		if ($CoherenceOnly) {
			$coherenceScoped = [Collections.Generic.List[object]]::new()
			foreach ($entry in $scoped) { if (Test-CoherenceReviewEntry $entry) { $coherenceScoped.Add($entry) } }
			$scoped = $coherenceScoped
		}
		$emittedEntries = [Collections.Generic.List[object]]::new()
		foreach ($entry in $scoped) {
			$emittedEntries.Add([pscustomobject][ordered]@{
				status = $entry.Status
				path = $entry.Path
				oldPath = $entry.OldPath
				baseline = $entry.Baseline
				current = $entry.Current
				class = $entry.Class
			})
		}
		$regionList = [Collections.Generic.List[object]]::new()
		if ($Regions) {
			$regionList = Get-RegionTable $baselineSha $headSha ([object[]] $untrackedEntries)
			$regionList.AddRange($coretideRegions)
		}
		if ($prefixes.Count -gt 0) {
			$allRegions = $regionList
			$regionList = [Collections.Generic.List[object]]::new()
			foreach ($region in $allRegions) { if (Test-InventoryPathInScope $region.path $prefixes) { $regionList.Add($region) } }
		}
		if ($CoherenceOnly -or $null -ne $sincePaths) {
			$keptPaths = [Collections.Generic.HashSet[string]]::new([string[]] @(), [StringComparer]::Ordinal)
			foreach ($entry in $scoped) { [void] $keptPaths.Add($entry.Path) }
			$allRegions = $regionList
			$regionList = [Collections.Generic.List[object]]::new()
			foreach ($region in $allRegions) { if ($keptPaths.Contains($region.path)) { $regionList.Add($region) } }
		}
		$landingTruncation = $null
		$landingRefusal = $null
		if ($Landing) {
			$landingReference = $null
			$result.landing = Get-LandingState $baselineSha $headSha ([ref] $landingReference) ([ref] $landingRefusal) $savePlanAuthorizedPaths
			$landingTruncation = $landingReference
		}

		$result.entries = [object[]] $emittedEntries.ToArray()
		if ($Regions) { $result.regions = [object[]] $regionList.ToArray() }
		$result.truncated = $false
		$result.truncation = [pscustomobject][ordered]@{
			entries = [pscustomobject][ordered]@{ full = $scoped.Count; emitted = $emittedEntries.Count }
			regions = [pscustomobject][ordered]@{ full = $regionList.Count; emitted = $regionList.Count }
			landing = $landingTruncation
			outputBytes = 0
		}
		if ($null -ne $landingRefusal) { return Set-InventoryOutcome $result 'blocked' $landingRefusal.Code $landingRefusal.Message }
		return Set-InventoryOutcome $result 'pass' 'ok' "Inventoried $($sorted.Count) change(s) against baseline $baselineSha."
	}
	catch {
		return Set-InventoryOutcome $result 'error' 'internal.error' $_.Exception.Message
	}
}

Export-ModuleMember -Function Get-SessionChangeInventory, Get-ReviewRoundState, Test-ReviewRoundCoverage
