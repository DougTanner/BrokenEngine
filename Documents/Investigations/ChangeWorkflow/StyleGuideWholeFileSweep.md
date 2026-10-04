# One-pass style guide sweep of existing C++

Runbook for bringing every first-party C++ file up to the style guide rules
the Change Workflow checks, in one pass: per unit (a header and its `.cpp`, or a lone
file), one Codex agent finds every violation and a second applies the fixes. It
owns the sweep mechanics and lives here, not in `Documents/Plans`, so the
scheduler never claims it. Stages `Common/`, `DataPacker/` and `Tools/` landed
at `9a12791d`; the two remaining stages are the executable Plans
`Documents/Plans/Engine/StyleGuideSweepEngine.md` and
`Documents/Plans/Game/StyleGuideSweepProjects.md`, which run this document. It
replaces the earlier per-rule and per-area style sweep Plans and their residual
Plans.

## Relation to the Change Workflow

The user's request to run a stage — directly, or through the claim of its stage
Plan — is the approval, and this runbook
takes the place of the Change Workflow steps from Approve and classify through
Verify the acceptance table for the sweep only: no `/prepare-change`,
`/plan-alternatives`, `/plan-audit`, `/plan-simplicity-review`, execution card,
`/repo-code-review`, `/comment-review` dispatch, `/coherence-review`, or
`/verify-acceptance`. What stays is listed under each phase below. The Verify
and land step applies unchanged: every stage lands through `/finalize-changes`
with one explicit user confirmation of the finalizer's summary.

## Scope

- In: tracked `*.h` and `*.cpp` under `Common/`, `DataPacker/`, `Tools/`,
  `Engine/` and `Projects/` (about 570 files, 114,000 lines at `ff516591`).
- Out: `ThirdParty/`, and shaders except the reference updates a C++ rename
  propagates.
- Rules: the `/code-style-review` mandate — the scanner's `style-rule-<n>`
  kinds plus the hand-read list and permitted forms in
  `.agents/skills/code-style-review/references/worker.md` steps 7 and 10 — and
  rule 64 as `/comment-review` applies it. `Documents/C++StyleGuide.txt` is
  the authority for each; the user rulings not in the guide are carried in the
  FIND prompt (`## Appendix — prompts and coordinator`).

## Fix bound

Wider than `/code-style-review`'s meaning-preserving bound (its worker steps
11-12): a fix may change a type, container, signature, overload choice,
error-handling path, or `kb*` toggle, provided observable behavior is
unchanged. Earlier sweeps stopped at the narrow bound and turned every such
site into a residual Plan; this sweep fixes them in place. The landed stages
accepted, for example, removal of trivial accessors and forwarders (rule 49),
pointer-and-count parameters replaced by `std::span`, and long-form renames.

A finding is out of bound, and left unfixed, when its fix would change any of:
sim output or the per-tick CRC; serialized, save, replay, wire, or `.pack`
bytes; threading; or what a trust-boundary check accepts. Out-of-bound findings
are collected in the ledger (Phase 3) for the user to decide on; the sweep
creates no follow-up Plans on its own.

## Units and batches

- Unit: one header together with its same-name `.cpp`, or a lone file. A unit
  is the file scope of one FIND-and-FIX pipeline.
- Batch: one directory's units. `Engine/Source/<subsystem>/` directories
  holding more than 20 C++ files (`Agent`, `Frame`, `Graphics`, `Network`,
  `Ui`) and `Projects/BrokenEngineSandbox/Source/` `Agent/`, `Frame/`,
  `Network/` and `Ui/` are batches of their own; the rest of each area is one
  batch.
- Stage: one top-level area, landed on its own, in the order `Common/`,
  `DataPacker/`, `Tools/`, `Engine/`, `Projects/`, so a rename lands before the
  areas that consume it are swept.

Scratch files live under `Temp/StyleSweep/` (ignored by Git, per worktree): the
prompts and coordinator from the appendix, `units/<unit>.findings.md` and
`units/<unit>.fix.md` per unit, `<batch>.crossfile.md` and
`<batch>.propagate.md` per batch, and `Status.txt`, `Progress.md`, `Ledger.md`
and `Failures.txt`. `Progress.md` records each finished unit and batch, and the
coordinator skips finished work, so a session that is compacted or restarted
resumes where it stopped.

## Setup

1. One fresh `implementer` writes the four appendix blocks verbatim to
   `Temp/StyleSweep/prompts/Find.md`, `Temp/StyleSweep/prompts/Fix.md`,
   `Temp/StyleSweep/prompts/Propagate.md` and `Temp/StyleSweep/Run-Sweep.ps1`
   — each block is the fenced block under its `###` path heading in
   `## Appendix — prompts and coordinator`, read through its closing fence,
   since the prompt blocks contain `##` lines of their own — and applies only
   the prompt corrections the stage's Plan names, which the brief cites as that
   Plan's path and section. It returns the four paths written. Main never reads
   the appendix.
2. Open SmartGit on the worktree (`smartgit.exe --open <worktree root>`). The
   user browses the uncommitted changes there, so the session makes no commit
   before landing.
3. `pwsh -NoProfile -File Temp/StyleSweep/Run-Sweep.ps1 -List` prints every
   batch name (`Engine_Source_Agent`, ..., `Engine`,
   `Projects_BrokenEngineSandbox_Source_Agent`, ..., `Projects`) with its unit
   count.

The coordinator calls the `codex` CLI directly and unsets `OPENAI_API_KEY`
first so the runs bill to the ChatGPT subscription. It does not route through
`/claude-to-codex`, which launches one blocking run per session call, treats a
result not ending in `PASS`, `CHANGES-REQUIRED: <n>` or `BLOCKED: <reason>` as
malformed, never re-dispatches a `workspace-write` run, and refuses an inherited
`OPENAI_API_KEY` rather than unsetting it; the coordinator is a detached script
running parallel unit pipelines whose prompts end in `FIND-DONE`, `FIX-DONE` and
`PROPAGATE-DONE`, and it retries a failed unit run once in either sandbox.

## Phase 1 — per-unit pipeline

Batches run one after another. For each batch, main launches the coordinator
detached, because a batch outlives the 2-hour background-task limit:
`Start-Process pwsh -WindowStyle Hidden -WorkingDirectory <worktree root> -ArgumentList '-NoProfile','-File','Temp/StyleSweep/Run-Sweep.ps1','-Batch','<batch>'`,
then waits with an until-loop on `Temp/StyleSweep/Status.txt` leaving
`RUNNING`. The coordinator runs eight unit pipelines at a time
(`-Throttle`); each pipeline is two Codex runs in order, retried once on
failure, and a unit that still fails is listed in `Failures.txt`. Agents hand
off through files; main reads only `Status.txt` and the propagate summary.

1. FIND — `gpt-6-luna`, reasoning effort `xhigh`, `--sandbox read-only`, prompt
   `Find.md`. Runs the scanner's whole-file mode on the unit's files,
   adjudicates each row, hand-reads every line for the rest of the rule set in
   `## Scope`, and classifies each finding `local` (edits only the unit,
   including a rename whose references stay inside it), `cross-file` (a rename
   or signature change with references outside the unit), or `out-of-bound`
   (`## Fix bound`). Its final message becomes `units/<unit>.findings.md`.
2. FIX — `gpt-6.1-sol`, reasoning effort `high`, `--sandbox workspace-write`,
   prompt `Fix.md`; skipped when FIND reports no findings. Confirms and applies
   each `local` finding, editing only the unit's files, declines the rest with a
   reason, and carries `cross-file` and `out-of-bound` findings unchanged. There
   is no spot-check; the stage cleanup pass (Phase 3) takes its place.

Codex `workspace-write` cannot edit `.agents/`; a finding whose fix needs such
an edit is declined and goes to the deferred-fixes file at landing. A Codex
git warning that it cannot read the user's global ignore file is harmless.
Sol propagation writing its own helper scripts and file backups under
`Temp/StyleSweep/` is expected.

## Phase 2 — per-batch close

1. Propagate — once every unit of the batch has finished, the coordinator
   collects the `## Cross-file` sections into `<batch>.crossfile.md` and runs
   one `gpt-6.1-sol` `high` `workspace-write` Codex run with `Propagate.md`,
   which applies them and updates every reference across the repository
   (C++, shaders, and AGENTS.md or docs naming the identifier), never
   `ThirdParty/`. The units' `## Out-of-bound` and `## Declined` sections and
   declined propagations go to `Ledger.md`; `Status.txt` reads `BATCH-DONE` with totals, or
   `INCOMPLETE` (units missing) or `PROPAGATE-FAILED`, which main resolves by
   re-running the batch.
2. Build — `builder` runs `/compile` for the targets the propagate summary's
   `## Build targets` names, and at least Client and Server Debug; a failure
   goes to `/resolve-findings`.
   Running this runbook authorizes Local generation (`-RunDataPacker`, Gaea
   export still forbidden) for any build whose changes trigger Local data mode,
   such as `DataPacker/**` or `Common/DataFile.h`
   (`.agents/skills/compile/references/runtime-data-mode.md` `## Mode selection`).
   `Tools/` changes to WorktreeCli or AgentHarness follow `/compile`'s
   AgentTools policy.

No commit is made between batches; `/finalize-changes` squashes at landing.

## Phase 3 — per-stage close and landing

1. Cleanup pass — one fresh `implementer` reads the stage's unit diffs and
   propagated lines against the stage baseline and corrects any edit that
   breaks a ruling in the FIND prompt, recording each item with `path:line` in
   `Temp/StyleSweep/Cleanup-<stage>.md`. Items found in earlier stages:
   integers changed to `size_t` instead of `int64_t` with `std::ssize()` (a
   `std::span` extent or other API consumer keeps `size_t`); `d`-prefixed
   doubles; lowercase lambda or `std::function` variables (an immediately
   invoked initializer lambda's variable holds its result and is exempt); a
   same-check or related-field guard split into several `if` statements;
   Win32 file APIs swapped for `std::filesystem`; `NULL` changed to `nullptr`
   inside an SDK SAL annotation; an index-and-count enum turned `enum class`;
   a split one-line braced list; a comment added to an anonymous namespace's
   closing brace or removed from a named one's.
2. Ordering review — one fresh `reviewer`, findings only, over every
   container or ordering change (for example `std::map` to
   `std::unordered_map`) in code whose output order matters; such a change is
   behavior-preserving only when every ordered traversal sorts. A finding goes
   to `/resolve-findings`.
3. `pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1 -RepositoryRoot <worktree root> -Baseline <stage baseline SHA>`.
4. `implementer` runs `/update-claude-docs` for the stage's renamed or
   re-signatured identifiers.
5. `builder` runs `/compile` for Client and Server Debug and Release, Profile
   when the stage edited code conditional on `BT_PROFILE`, DataPacker Release
   when a changed file compiles into it, and WorktreeCli and AgentHarness for
   the `Tools/` stage.
6. `/agent-harness` replay determinism check for every stage except `Tools/`.
7. Main appends the ledger (`Temp/StyleSweep/Ledger.md`: each unit's
   out-of-bound and FIX-declined findings and each batch's declined
   propagations, with `path:line`, rule, and why each was left) to `Documents/Investigations/ChangeWorkflow/StyleGuideSweepDeferredFixes.md`
   as a section for the stage, reports the stage (units swept, fixes applied,
   deferred fixes), and lands it through `/finalize-changes` after the user
   confirms the finalizer's summary.

## Cost

`Engine/` is 208 units (about 70,000 lines) in six batches and `Projects/` 74
units (about 21,000 lines) in five. Each unit costs one Luna `xhigh` FIND run
and, when it has findings, one Sol `high` FIX run; each batch adds one Sol
propagation and a build. Each stage adds one cleanup `implementer`, one
ordering `reviewer`, the Phase 3 builds and checks, and one landing.

## Appendix — prompts and coordinator

The coordinator substitutes `{{ROOT}}`, `{{FILES}}`, `{{PATHARGS}}`,
`{{UNIT}}`, `{{FINDINGS}}`, `{{BATCH}}` and `{{CROSSFILE}}`.

### `Temp/StyleSweep/prompts/Find.md`

```text
You are the FIND step of one unit pipeline in the style guide whole-file sweep.
Repository root (session worktree): {{ROOT}}

Read `Documents/Investigations/ChangeWorkflow/StyleGuideWholeFileSweep.md` sections `## Scope` and `## Fix bound`. As its `## Phase 1 — per-unit pipeline` describes, this FIND step only finds and records; a separate FIX agent applies the fixes afterwards from your output alone, and there is no spot-check. You are findings-only: never edit any file and never run a Git command that changes state.

User rulings for this sweep (never report these): `using namespace std::chrono_literals;` stays as is; the DirectXMath-style `XM_PIDIV*` constants in `Common/ExternalHeaders.h` keep their `XM_` names; a named namespace's closing comment (`} // namespace <name>`) is required, never a rule 64 finding; an anonymous namespace closes with a bare `}` (rule 65), so a comment on its closing brace is a rule 65 `local` finding whose fix deletes the comment. Rule 51's lambda/initializer-list exception applies only to a braced body or literal that spans several lines; a braced initializer list written on one line (e.g. `FormatVector(pcBuffer, {f4.x, f4.y, f4.z, f4.w})`) stays inline on the call line — never split it. A variable of any Vulkan `Vk*` type, including Vulkan enums such as `VkResult` and `VkFormat` and integer typedefs such as `VkDeviceSize` and `VkBool32`, takes the `vk` prefix (`VkResult vkResult`, `VkFormat vkFormat`), never `e`; after another prefix it is written `Vk` (`VkBuffer& rVkBuffer`, `inline constexpr VkFormat kVkFormatDepth`); class members follow rule 54 (`m...Vk<TypeName>`, any Vk* type); struct members use the plain `vk` prefix (`TextureHeader::vkFormat`). Rule 55: an enum whose values are used as indices or integer ids and which ends in a `k...Count` enumerator (e.g. `common::Threads`) is an index-and-count enum and stays a plain `enum`; never convert it to `enum class`. Rule 17 (user ruling, overrides any older text): `std::vector` sizes and indices use `int64_t` with `std::ssize()` (`for (int64_t i = 0; i < std::ssize(vec); ++i)`); never change an integer type to `size_t`. A `size_t` vector size/index is a rule 17 `local` finding converting it to `int64_t`/`std::ssize()`, unless the value's consumer requires `size_t` (an API parameter or serialized field type). The scanner's `style-rule-17` rows still follow the old size_t wording — reject them. Rule 3: `double` takes the `f` prefix exactly like `float` (`double fSeconds`); a `d`-prefixed double (`dHPrevious`, `dDiagonal`) is a rule 3 finding renaming it to `f…` — check every `double` declaration, and when a rename for another rule touches a double, apply the `f` prefix in the same rename. Rule 62 (user ruling): conditions that together validate one object's related fields with the same exit (e.g. a texture's width, height and mip count against limits) are one check and stay in one `if`. Never replace a Win32 file API call (`DeleteFileW`, `MoveFileExW`, etc.) with `std::filesystem`: their error and attribute semantics differ, so it is `out-of-bound`. A declaration that must match a system or SDK declaration (e.g. `operator new` replacements with `_Success_(return != NULL)` matching `vcruntime_new.h`) keeps its annotation text verbatim — never change `NULL` to `nullptr` inside a SAL annotation. Rule 7: a variable holding a lambda or a `std::function` is named like a function, starting uppercase (`auto Sample = [&](int64_t iFrame) ...`, called `Sample(i)`); with a rule 3 prefix the name after the prefix is uppercase (`mGetNextTrack`, `rWriteBody`); a lowercase one is a rule 7 finding renaming it and its calls.

Unit files (the whole scope of this step):
{{FILES}}

Steps:
1. Run the scanner's whole-file mode on exactly the unit files, from the worktree root, exactly as documented:
   pwsh -NoProfile -Command "& '.agents/scripts/Find-SessionCandidates.ps1' -RepositoryRoot '{{ROOT}}' -Path {{PATHARGS}}"
   Adjudicate every row it returns against `Documents/C++StyleGuide.txt` (a scanner row is a candidate, not a verdict). If the scanner cannot run, say so in the output and continue by hand.
2. Hand-read every line of every unit file for the rest of the rule set in `## Scope`: the hand-read list and permitted forms in `.agents/skills/code-style-review/references/worker.md` steps 7 and 10, and rule 64 as `.agents/skills/comment-review` applies it. A permitted form is never a finding.
3. Classify each finding: `local` (the fix edits only unit files, including a rename whose every reference is inside the unit — confirm by searching the repository, shaders included), `cross-file` (a rename or signature change with references outside the unit), or `out-of-bound` (per `## Fix bound`).

Your final message is written verbatim to the findings file the FIX agent reads, so it must be self-contained. Format:

# Findings: {{UNIT}}
Scanner: <ran, N rows | could not run: reason>

## Findings
One row per finding:
- F<n> | `path:line` | Rule <N> | class | violating text (verbatim, short) | exact fix (replacement text or precise edit; for renames old -> new and every in-unit reference line)

## Rejected scanner rows
- `path:line` | Rule <N> | why it is not a violation (one line each)

Write `none` under a heading that has no rows. No other prose. End with one line: `FIND-DONE findings=<n>`.
```

### `Temp/StyleSweep/prompts/Fix.md`

```text
You are the FIX step of one unit pipeline in the style guide whole-file sweep.
Repository root (session worktree): {{ROOT}}

Read `Documents/Investigations/ChangeWorkflow/StyleGuideWholeFileSweep.md` section `## Fix bound`. A FIND agent already recorded this unit's findings in `{{FINDINGS}}`; read that file.

Unit files (the only files you may edit):
{{FILES}}

Rules:
- Other agents are editing other units in this same worktree at the same time. Edit only the unit files above. Never run a Git command that changes state (no add, commit, checkout, restore, stash, reset). Do not build.
- For each `local` finding: confirm it against the code and `Documents/C++StyleGuide.txt` (permitted forms in `.agents/skills/code-style-review/references/worker.md` steps 7 and 10 are not violations), then apply it if it is a real violation and the fix is behavior-preserving within `## Fix bound`; otherwise decline it with a one-line reason. For a rename, search the repository (C++ and shaders) first; if any reference lies outside the unit, do not apply it — reclassify it `cross-file`.
- Do not apply `cross-file` or `out-of-bound` findings; carry them to your output unchanged (with any reclassification noted).
- Preserve each file's encoding, BOM, line endings (CRLF), tabs, and trailing newline exactly; change only the bytes the fix needs.
- After editing, read `git diff HEAD -- <unit files>` and confirm every hunk belongs to an applied finding and preserves behavior.

Your final message is written verbatim to a file the coordinator parses. Format:

# Fix: {{UNIT}}

## Applied
- F<n> `path:line` short description   (or `none`)

## Declined
- F<n> | `path:line` | Rule <N> | reason   (or `none`)

## Cross-file
- F<n> | `path:line` | Rule <N> | violating text | exact fix, with old -> new names for renames   (or `none`)

## Out-of-bound
- F<n> | `path:line` | Rule <N> | why it is out of bound   (or `none`)

End with one line: `FIX-DONE applied=<n> declined=<n> crossfile=<n> oob=<n>`.
```

### `Temp/StyleSweep/prompts/Propagate.md`

```text
You are the PROPAGATE step (Phase 2 step 1) of the style guide whole-file sweep for batch `{{BATCH}}`.
Repository root (session worktree): {{ROOT}}

Read `Documents/Investigations/ChangeWorkflow/StyleGuideWholeFileSweep.md` sections `## Fix bound` and `## Phase 2 — per-batch close`. The batch's per-unit FIX agents left these `cross-file` findings unapplied, collected in `{{CROSSFILE}}`; read that file.

For each finding: confirm it against the code and `Documents/C++StyleGuide.txt`; apply it if it is a real violation and its fix stays within `## Fix bound`, otherwise decline it with a one-line reason. Then follow `.agents/skills/update-affected-code/SKILL.md` over the applied changes: search the whole repository (C++, shaders, and AGENTS.md / docs that name the identifier) for every renamed or changed symbol and update every reference. Never edit `ThirdParty/` (AGENTS.md: do not modify); decline a finding whose fix needs a `ThirdParty/` edit. Preserve encoding, BOM, CRLF, tabs, and trailing newline. Never run a Git command that changes state and do not build.

Your final message is written verbatim to a file the coordinator reads. Format:

# Propagate: {{BATCH}}

## Applied
- F-id `path:line` old -> new, files updated   (or `none`)

## Declined
- F-id | `path:line` | Rule <N> | reason   (or `none`)

## Build targets
One line naming the projects whose sources changed this batch beyond Client and Server (DataPacker, WorktreeCli, AgentHarness), or `Client, Server`.

End with one line: `PROPAGATE-DONE applied=<n> declined=<n>`.
```

### `Temp/StyleSweep/Run-Sweep.ps1`

```powershell
# Scratch coordinator for the style guide whole-file sweep: per unit, Codex Luna (xhigh, read-only) finds,
# then Codex Sol (high, workspace-write) fixes; per batch, Sol propagates cross-file findings.
# -List prints batches and unit counts. -Batch <name> runs one batch (resumable). -Unit <stem> runs one unit only.
param(
	[string] $Batch,
	[string] $Unit,
	[switch] $List,
	[int] $Throttle = 8
)

$ErrorActionPreference = 'Stop'
Remove-Item Env:OPENAI_API_KEY -ErrorAction SilentlyContinue
$root = (git rev-parse --show-toplevel).Trim() -replace '/', '\'
$sweep = Join-Path $root 'Temp\StyleSweep'
$status = Join-Path $sweep 'Status.txt'

$ownBatchDirs = @(
	'Engine/Source/Agent', 'Engine/Source/Frame', 'Engine/Source/Graphics', 'Engine/Source/Network', 'Engine/Source/Ui',
	'Projects/BrokenEngineSandbox/Source/Agent', 'Projects/BrokenEngineSandbox/Source/Frame',
	'Projects/BrokenEngineSandbox/Source/Network', 'Projects/BrokenEngineSandbox/Source/Ui')

$files = git -C $root ls-files -- 'Common/*.h' 'Common/*.cpp' 'DataPacker/*.h' 'DataPacker/*.cpp' 'Tools/*.h' 'Tools/*.cpp' 'Engine/*.h' 'Engine/*.cpp' 'Projects/*.h' 'Projects/*.cpp'
$units = $files | Group-Object { $_ -replace '\.(h|cpp)$', '' } | ForEach-Object {
	$stem = $_.Name
	$dir = $ownBatchDirs | Where-Object { $stem.StartsWith("$_/") } | Select-Object -First 1
	if (-not $dir) { $dir = $stem.Split('/')[0] }
	[pscustomobject] @{ Stem = $stem; Id = ($stem -replace '[/\\]', '_'); Files = @($_.Group); Batch = ($dir -replace '/', '_') }
}

if ($List)
{
	$units | Group-Object Batch | ForEach-Object { '{0} {1}' -f $_.Name, $_.Count }
	return
}

$batchUnits = @($units | Where-Object { $_.Batch -ceq $Batch -and (-not $Unit -or $_.Stem -ceq $Unit) })
if ($batchUnits.Count -eq 0) { throw "no units for batch '$Batch' unit '$Unit'" }
New-Item -ItemType Directory -Force -Path (Join-Path $sweep 'units') | Out-Null
"RUNNING $Batch $(Get-Date -Format s) units=$($batchUnits.Count)" | Set-Content $status

$batchUnits | ForEach-Object -ThrottleLimit $Throttle -Parallel {
	$u = $_
	$root = $using:root
	$sweep = $using:sweep
	$codex = (Get-Command codex).Source
	$findOut = Join-Path $sweep "units\$($u.Id).findings.md"
	$fixOut = Join-Path $sweep "units\$($u.Id).fix.md"
	$fileList = ($u.Files | ForEach-Object { "- $_" }) -join "`n"
	$pathArgs = ($u.Files | ForEach-Object { "'$_'" }) -join ','

	function Invoke-Codex([string] $Template, [string] $Out, [string] $Sandbox, [string] $Model, [string] $Effort, [hashtable] $Vars)
	{
		$prompt = Get-Content -Raw (Join-Path $sweep "prompts\$Template")
		foreach ($k in $Vars.Keys) { $prompt = $prompt.Replace("{{$k}}", $Vars[$k]) }
		for ($attempt = 1; $attempt -le 2; $attempt++)
		{
			$prompt | & $codex -a never exec --sandbox $Sandbox -C $root -m $Model -c "model_reasoning_effort=`"$Effort`"" --ephemeral -o $Out - 2> "$Out.stderr.txt" | Out-Null
			if ($LASTEXITCODE -eq 0 -and (Test-Path $Out)) { return $true }
		}
		return $false
	}

	$vars = @{ ROOT = $root; FILES = $fileList; PATHARGS = $pathArgs; UNIT = $u.Stem; FINDINGS = "Temp/StyleSweep/units/$($u.Id).findings.md" }
	if (-not ((Test-Path $findOut) -and (Select-String -Quiet -SimpleMatch 'FIND-DONE' $findOut)))
	{
		if (-not (Invoke-Codex 'Find.md' $findOut 'read-only' 'gpt-6-luna' 'xhigh' $vars)) { Add-Content (Join-Path $sweep 'Failures.txt') "$($u.Stem) FIND failed"; return }
	}
	if ((Test-Path $fixOut) -and (Select-String -Quiet -SimpleMatch 'FIX-DONE' $fixOut)) { return }
	if (Select-String -Quiet 'FIND-DONE findings=0\b' $findOut)
	{
		"# Fix: $($u.Stem)`n`n## Applied`nnone`n`n## Declined`nnone`n`n## Cross-file`nnone`n`n## Out-of-bound`nnone`n`nFIX-DONE applied=0 declined=0 crossfile=0 oob=0" | Set-Content $fixOut
	}
	elseif (-not (Invoke-Codex 'Fix.md' $fixOut 'workspace-write' 'gpt-6.1-sol' 'high' $vars)) { Add-Content (Join-Path $sweep 'Failures.txt') "$($u.Stem) FIX failed"; return }
	Add-Content (Join-Path $sweep 'Progress.md') "unit $($u.Stem) $((Select-String 'FIX-DONE.*' $fixOut).Matches[0].Value)"
}

if ($Unit) { "UNIT-DONE $Batch $Unit $(Get-Date -Format s)" | Set-Content $status; return }

# Collect the batch's cross-file, out-of-bound and declined sections.
function Get-Section([string] $Path, [string] $Heading)
{
	$text = Get-Content -Raw $Path
	$m = [regex]::Match($text, "(?ms)^## $Heading\s*\r?\n(.*?)(?=^## |^FIX-DONE)")
	$body = $m.Groups[1].Value.Trim()
	if ($body -and $body -notmatch '^(-\s*)?none$') { return $body }
}

$crossFile = Join-Path $sweep "$Batch.crossfile.md"
$cross = @(); $ledger = @(); $missing = @()
foreach ($u in $batchUnits)
{
	$fixOut = Join-Path $sweep "units\$($u.Id).fix.md"
	if (-not ((Test-Path $fixOut) -and (Select-String -Quiet -SimpleMatch 'FIX-DONE' $fixOut))) { $missing += $u.Stem; continue }
	$c = Get-Section $fixOut 'Cross-file'
	if ($c) { $cross += "### $($u.Stem)`n$c`n" }
	$o = Get-Section $fixOut 'Out-of-bound'
	if ($o) { $ledger += "### $($u.Stem) (out-of-bound)`n$o`n" }
	$d = Get-Section $fixOut 'Declined'
	if ($d) { $ledger += "### $($u.Stem) (declined)`n$d`n" }
}
if ($missing.Count -gt 0)
{
	"INCOMPLETE $Batch $(Get-Date -Format s) missing=$($missing -join ',')" | Set-Content $status
	return
}
# The cross-file file is written once per complete batch, so its absence means the ledger is not yet appended.
if (-not (Test-Path $crossFile)) { foreach ($l in $ledger) { Add-Content (Join-Path $sweep 'Ledger.md') $l } }

$propOut = Join-Path $sweep "$Batch.propagate.md"
if ($cross.Count -gt 0 -and -not ((Test-Path $propOut) -and (Select-String -Quiet -SimpleMatch 'PROPAGATE-DONE' $propOut)))
{
	"# Cross-file findings: $Batch`n`n$($cross -join "`n")" | Set-Content $crossFile
	Remove-Item Env:OPENAI_API_KEY -ErrorAction SilentlyContinue
	$prompt = (Get-Content -Raw (Join-Path $sweep 'prompts\Propagate.md')).Replace('{{ROOT}}', $root).Replace('{{BATCH}}', $Batch).Replace('{{CROSSFILE}}', "Temp/StyleSweep/$Batch.crossfile.md")
	$prompt | & (Get-Command codex).Source -a never exec --sandbox workspace-write -C $root -m gpt-6.1-sol -c 'model_reasoning_effort="high"' --ephemeral -o $propOut - 2> "$propOut.stderr.txt" | Out-Null
	if ($LASTEXITCODE -ne 0 -or -not (Test-Path $propOut) -or -not (Select-String -Quiet -SimpleMatch 'PROPAGATE-DONE' $propOut)) { "PROPAGATE-FAILED $Batch $(Get-Date -Format s)" | Set-Content $status; return }
	$declined = Get-Section $propOut 'Declined'
	if ($declined) { Add-Content (Join-Path $sweep 'Ledger.md') "### $Batch propagation (declined)`n$declined`n" }
}
elseif ($cross.Count -eq 0 -and -not (Test-Path $crossFile)) { 'none' | Set-Content $crossFile }

$totals = @{ applied = 0; declined = 0; crossfile = 0; oob = 0 }
foreach ($u in $batchUnits)
{
	$line = (Select-String 'FIX-DONE.*' (Join-Path $sweep "units\$($u.Id).fix.md")).Matches[0].Value
	foreach ($k in @($totals.Keys)) { if ($line -match "$k=(\d+)") { $totals[$k] += [int] $Matches[1] } }
}
$summary = "units=$($batchUnits.Count) applied=$($totals.applied) declined=$($totals.declined) crossfile=$($totals.crossfile) oob=$($totals.oob)"
Add-Content (Join-Path $sweep 'Progress.md') "batch $Batch phase1+propagate $summary"
"BATCH-DONE $Batch $(Get-Date -Format s) $summary" | Set-Content $status
```
