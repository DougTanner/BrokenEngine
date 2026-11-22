<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-22T04:07:32.684Z","dependsOn":[]} -->
# Relocate Antigravity wrapper to `.antigravity/`, centralize PowerShell wrapper defaults, and restore Codex history entry

## Context

Commit `e2ace3e0` added Antigravity CLI (`agy`) as a session client and placed
its session wrapper at `.agents/agy-worktree.ps1`, its hook configuration at
`.agents/hooks.json`, and its eight role definitions under `.agents/agents/`.
Claude Code and Codex CLI place their session wrappers in client-named root
directories (`.claude/claude-worktree.sh` and `.codex/codex-worktree.ps1`), and
OpenCode places its wrapper at `.opencode/opencode-worktree.ps1`.

The user requested:
`.\.agents\agy-worktree.ps1 and any Antigravity-specific files should be in
/.antigravity/ matching the claude and codex pattersn, also put
.\.codex\codex-worktree.ps1 back in the Windows Terminal Powershell 7 history
push (but the 'oldest')`.
After presentation of options during planning, the user selected:

1. **Keep `agy.exe`-hardcoded paths in `.agents/`**: move only
   `.agents/agy-worktree.ps1` to `.antigravity/agy-worktree.ps1` and leave
   `.agents/hooks.json` and `.agents/agents/*.md` in `.agents/`. Binary
   inspection of `agy.exe` (`1.5.14`) confirmed it hardcodes `.agents/hooks.json`
   and `.agents/agents` (not `.antigravity/...`) and runs hooks with working
   directory `<workspace>\.agents`
   (`Documents/Plans/ChangeWorkflow/LoadSubdirectoryAgentsMdInCodexAndAgy.md:36-38`).
   Shared `.agents/skills/`, `.agents/references/`, and `.agents/scripts/` also
   remain under `.agents/`.
2. **Centralize PowerShell wrapper launch defaults in
   `Start-AgentWorktreeSession.ps1`**: collapse `.antigravity/agy-worktree.ps1`,
   `.codex/codex-worktree.ps1`, and `.opencode/opencode-worktree.ps1` into thin
   shims and move optional `-RepositoryRoot` Git-root resolution and unbound
   `ClientArguments`/`ClientExecutable` defaults for `agy`, `codex`, and
   `opencode` into `.agents/scripts/Start-AgentWorktreeSession.ps1`.

Baseline facts at `1f957dd75523edd85dca50b95150569a718e5f44`:

- `.agents/agy-worktree.ps1:7-20`, `.codex/codex-worktree.ps1:7-20`, and
  `.opencode/opencode-worktree.ps1:10-27` duplicate `git rev-parse
  --show-toplevel`, `GetFullPath`, `ReattachWorktree` splatting, and launcher
  invocation while hardcoding client launch flags (`agy` in `e2ace3e0`; `codex`
  in `c921bc1b` and `9c746fa0`; `opencode` `pwsh` wrapper clearing
  `PSModulePath` in `2028ccd5`).
- `.agents/scripts/Start-AgentWorktreeSession.ps1` already validates `-Client`
  (`:3`, `:13`), decodes/clears `claude`'s `BROKEN_ENGINE_CLIENT_ARGUMENTS`
  (`:18-25`, `:107`), treats omitted/empty/whitespace `-ReattachWorktree`
  identically (`:39`), calls `Get-AgentWorktreePrimaryCheckout`
  (`.agents/scripts/AgentWorktreeSession.psm1:43-54`, which canonicalizes
  `$RepositoryRoot` and returns canonical `CommonDirectory`), imports
  `WorktreeCliSessionExclusion.psm1` (`:28`, exporting
  `Get-WorktreeCliRepositoryIdentity` whose `.Repository` canonicalizes
  `--git-common-dir` identically), injects `agy`'s `--settings`
  `trustedWorkspaces` (`:109`), and resolves default `$ClientExecutable`
  (`:110-112`), but requires `-RepositoryRoot` as `[Parameter(Mandatory)]` (`:4`)
  and has no default `ClientArguments` or `opencode` `'pwsh'` executable.
- All three baseline wrappers guard against launching when dot-sourced
  (`.agents/agy-worktree.ps1:23`, `.codex/codex-worktree.ps1:23`,
  `.opencode/opencode-worktree.ps1:7`), and `agy-worktree.ps1:6` and
  `codex-worktree.ps1:6` define `Invoke-AgyWorktree` and `Invoke-CodexWorktree`
  (the latter documented at `README.md:145-152` and dot-sourced by the host's
  PowerShell 7 `$PROFILE`).
- Five tracked files reference `.agents/agy-worktree.ps1`:
  `.agents/references/change-workflow.md:160`, `.agents/skills/eod/SKILL.md:71`,
  `Documents/FreshMachineSetup.md:113`,
  `Documents/Plans/ChangeWorkflow/RemoveOpenCodeSupport.md:148`, and
  `README.md:327`.
- The host-local PowerShell 7 profile resolved by `$PROFILE` (outside the Git
  repository; intentionally omitted from `README.md:152-154`) appends startup
  commands to `PSReadLine`'s `ConsoleHost_history.txt` via
  `Add-Content -LiteralPath $historyPath -Encoding utf8NoBOM -Value @(...)`
  ending in `'.\.agents\agy-worktree.ps1'` and currently omitting
  `'.\.codex\codex-worktree.ps1'`.

## Design

### 1. Centralize defaults and repository-identity validation in `Start-AgentWorktreeSession.ps1`

Make five edits to `.agents/scripts/Start-AgentWorktreeSession.ps1` while
leaving lines 18-25 (`claude` transport decode), lines 39-107 (reattach,
worktree creation, `extensions.worktreeConfig`, sparse checkout,
`Initialize-AgentWorktreeSession.ps1`, and `claude` transport clear), and line
109 (`agy` `--settings` injection) intact:

1. **Optional `-RepositoryRoot`** (`:4`): remove `[Parameter(Mandatory)]` so
   line 4 becomes `[string] $RepositoryRoot,`. Keep
   `.claude/claude-worktree.sh:42-43` passing `-RepositoryRoot` unchanged.
2. **Default `ClientArguments` when unbound** (immediately after the `claude`
   `BROKEN_ENGINE_CLIENT_ARGUMENTS` `if` block at `:19-25`):
   ```powershell
   elseif (-not $PSBoundParameters.ContainsKey('ClientArguments')) {
   	$ClientArguments = @(switch -CaseSensitive ($Client) {
   		'agy' { '--dangerously-skip-permissions' }
   		'codex' { '--no-daemon', '--dangerously-bypass-approvals-and-sandbox', '--dangerously-bypass-hook-trust' }
   		# PSModulePath is cleared because inherited PowerShell 7 module paths break OpenCode's Windows
   		# PowerShell archive helper, which breaks its Grep and Skill tools.
   		'opencode' { '-NoProfile', '-Command', '$env:PSModulePath = $null; & opencode.cmd; exit $LASTEXITCODE' }
   	})
   }
   ```
   Binding `-ClientArguments` (even `@()`) skips this branch; wrapping `switch`
   in `@(...)` preserves `[string[]]` without single-element unrolling; `claude`
   remains `@()` unless `BROKEN_ENGINE_CLIENT_ARGUMENTS` or `-ClientArguments`
   is supplied; `agy` still appends `--settings` at `:109`.
3. **Scalar Git-root fallback** (at the start of `try {` before `:34`):
   ```powershell
   	if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
   		$resolvedRoot = git rev-parse --show-toplevel 2>$null
   		if (($LASTEXITCODE -ne 0) -or [string]::IsNullOrWhiteSpace($resolvedRoot)) {
   			throw "$Client-worktree must be run inside a Git repository."
   		}
   		$RepositoryRoot = $resolvedRoot.Trim()
   	}
   ```
   Author recommendation: reuse the existing wrappers' scalar lookup and let
   `Get-AgentWorktreePrimaryCheckout` (`AgentWorktreeSession.psm1:44`) handle
   canonicalization so no second normalization path is introduced.
4. **Launcher/target repository-identity check** (immediately after `:34-35`,
   before reattach at `:39` or any write at `:60-81`):
   ```powershell
   	$launcherCommon = (Get-WorktreeCliRepositoryIdentity $PSScriptRoot).Repository
   	if (-not $launcherCommon.Equals($primary.CommonDirectory, [StringComparison]::OrdinalIgnoreCase)) {
   		throw "RepositoryRoot '$root' does not belong to this launcher's repository."
   	}
   ```
   Author recommendation: because the thin shims resolve
   `Start-AgentWorktreeSession.ps1` via `$PSScriptRoot` instead of
   `$repositoryRoot`, reusing the already-imported
   `Get-WorktreeCliRepositoryIdentity` (`WorktreeCliSessionExclusion.psm1:8-23`)
   blocks a profile-loaded helper from writing branches or worktrees into a
   foreign ordinary Git repository while still allowing a linked worktree's
   launcher to target its own primary checkout.
5. **Default `ClientExecutable` for `opencode`** (at `:110-112`):
   ```powershell
   	if (-not $PrepareOnly -and [string]::IsNullOrWhiteSpace($ClientExecutable)) {
   		$executableName = if ($Client -ceq 'opencode') { 'pwsh' } else { $Client }
   		$ClientExecutable = (Get-Command $executableName -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
   	}
   ```

### 2. Relocate `agy-worktree.ps1` and collapse the three PowerShell wrappers

Author recommendation for shim shape: forward `-ReattachWorktree
$ReattachWorktree` (and `-PrepareOnly:$PrepareOnly` for OpenCode) directly
without blank-path checks or hashtables (`Start-AgentWorktreeSession.ps1:39`
already ignores blank paths); preserve the dot-source import guard on all three
wrappers and preserve `Invoke-AgyWorktree` alongside `Invoke-CodexWorktree`;
keep the launcher call and `exit $LASTEXITCODE` once in each script's direct
entry point, and have `Invoke-AgyWorktree` / `Invoke-CodexWorktree` invoke that
script via `& $MyInvocation.MyCommand.ScriptBlock.File` and throw on nonzero
`$LASTEXITCODE` so a dot-sourced failure stays catchable in an interactive shell
while direct invocation exits with `$LASTEXITCODE` and no duplicate stderr error.

1. Create the `.antigravity/` directory, then move `.agents/agy-worktree.ps1`
   to `.antigravity/agy-worktree.ps1` with
   `git mv .agents/agy-worktree.ps1 .antigravity/agy-worktree.ps1` (leaving no
   backward-compatibility alias at `.agents/agy-worktree.ps1` per root
   `AGENTS.md`), with body:
   ```powershell
   [CmdletBinding()]
   param(
   	[string] $ReattachWorktree
   )

   function Invoke-AgyWorktree([string] $ReattachWorktree) {
   	& $MyInvocation.MyCommand.ScriptBlock.File -ReattachWorktree $ReattachWorktree
   	if ($LASTEXITCODE -ne 0) { throw "agy-worktree exited with code $LASTEXITCODE." }
   }

   if ($MyInvocation.InvocationName -eq '.') { return }

   & (Join-Path $PSScriptRoot '..\.agents\scripts\Start-AgentWorktreeSession.ps1') -Client agy -ReattachWorktree $ReattachWorktree
   exit $LASTEXITCODE
   ```
2. Collapse `.codex/codex-worktree.ps1` to the parallel thin shim:
   ```powershell
   [CmdletBinding()]
   param(
   	[string] $ReattachWorktree
   )

   function Invoke-CodexWorktree([string] $ReattachWorktree) {
   	& $MyInvocation.MyCommand.ScriptBlock.File -ReattachWorktree $ReattachWorktree
   	if ($LASTEXITCODE -ne 0) { throw "codex-worktree exited with code $LASTEXITCODE." }
   }

   if ($MyInvocation.InvocationName -eq '.') { return }

   & (Join-Path $PSScriptRoot '..\.agents\scripts\Start-AgentWorktreeSession.ps1') -Client codex -ReattachWorktree $ReattachWorktree
   exit $LASTEXITCODE
   ```
3. Collapse `.opencode/opencode-worktree.ps1` to the thin shim preserving its
   import guard and `-PrepareOnly`:
   ```powershell
   [CmdletBinding()]
   param(
   	[string] $ReattachWorktree,
   	[switch] $PrepareOnly
   )

   if ($MyInvocation.InvocationName -eq '.') { return }

   & (Join-Path $PSScriptRoot '..\.agents\scripts\Start-AgentWorktreeSession.ps1') -Client opencode -ReattachWorktree $ReattachWorktree -PrepareOnly:$PrepareOnly
   exit $LASTEXITCODE
   ```

### 3. Update tracked references and synchronize `RemoveOpenCodeSupport.md`

- `.agents/references/change-workflow.md:159-164`: replace
  `.agents/agy-worktree.ps1` with `.antigravity/agy-worktree.ps1` and rewrap
  only that paragraph at ~80 columns (Part 2 stays well under the 9,500-char
  limit in `.agents/scripts/Write-SessionWorkflowContext.ps1:25`).
- `.agents/skills/eod/SKILL.md:71`: replace
  `.\.agents\agy-worktree.ps1 -ReattachWorktree '<path>'` with
  `.\.antigravity\agy-worktree.ps1 -ReattachWorktree '<path>'`.
- `Documents/FreshMachineSetup.md:112-113`: replace
  `` `.\.agents\agy-worktree.ps1` `` with
  `` `.\.antigravity\agy-worktree.ps1` ``.
- `README.md:324-328`: replace `.\.agents\agy-worktree.ps1` with
  `.\.antigravity\agy-worktree.ps1`.
- `Documents/Plans/ChangeWorkflow/RemoveOpenCodeSupport.md` (preserving
  byte-zero `broken-engine-plan/v1` metadata and historical `## Context`):
  - `:55-58` (`## Design`) and `:106-107` (`## In scope`): include dropping the
    `opencode` default `ClientArguments` and `ClientExecutable` branches from
    `Start-AgentWorktreeSession.ps1`.
  - `:147-149` (`## Notes`): replace `` `.agents/agy-worktree.ps1` `` with
    `` `.antigravity/agy-worktree.ps1` ``.

### 4. Update the host-local PowerShell 7 profile

Prerequisite: this step runs on the host where PowerShell 7's `$PROFILE`
(`pwsh -NoProfile -Command '$PROFILE'`) resolves to the user's existing
`Microsoft.PowerShell_profile.ps1` containing the
`Add-Content ... ConsoleHost_history.txt` history-push array (not inside an
isolated sandbox where `$PROFILE` is absent).

Edit that `Microsoft.PowerShell_profile.ps1` file (preserving UTF-8 without BOM
and LF line endings, touching only the `Add-Content ... -Value @(...)`
history-push array):

- Prepend `'.\.codex\codex-worktree.ps1'` as the first (oldest) element.
- Replace `'.\.agents\agy-worktree.ps1'` with
  `'.\.antigravity\agy-worktree.ps1'` as the last (newest) element.

### Risk tier and invariants

Change Workflow tier: **Tier 3 — trust boundary**
(`.agents/references/risk-tiers.md:12-18`). Direct calls to
`.agents/scripts/Start-AgentWorktreeSession.ps1 -Client agy` or `-Client codex`
with omitted `-ClientArguments` gain permission-bypass (and for `codex`,
hook-trust bypass) defaults instead of `@()`, altering what that direct entry
point trusts; launcher/target `--git-common-dir` validation also enforces a
repository boundary before writes. No C++, GLSL, determinism/CRC, wire,
serialization, save/replay, threading, or build/bootstrap lock surfaces are
touched.

Invariants preserved:

- Explicit `-ClientArguments` (including `@()`) overrides defaults; `claude`'s
  `BROKEN_ENGINE_CLIENT_ARGUMENTS` decode/clear ordering and `agy`'s
  `--settings` `trustedWorkspaces` injection remain intact.
- Foreign ordinary Git checkouts are rejected before any reattach, branch,
  worktree, or `extensions.worktreeConfig` write.
- Dot-sourcing any of the three shims never invokes the launcher; dot-sourced
  `Invoke-AgyWorktree`/`Invoke-CodexWorktree` failures throw catchably without
  exiting the calling shell; direct script calls propagate `$LASTEXITCODE` with
  no duplicate stderr error.
- `Write-SessionWorkflowContext.ps1` Part 2 stays under 9,500 characters and
  `RemoveOpenCodeSupport.md` keeps valid byte-zero metadata.

## Critical files

- `.agents/scripts/Start-AgentWorktreeSession.ps1`
- `.agents/agy-worktree.ps1` (deleted via `git mv`)
- `.antigravity/agy-worktree.ps1` (added via `git mv`)
- `.codex/codex-worktree.ps1`
- `.opencode/opencode-worktree.ps1`
- `.agents/references/change-workflow.md`
- `.agents/skills/eod/SKILL.md`
- `Documents/FreshMachineSetup.md`
- `Documents/Plans/ChangeWorkflow/RemoveOpenCodeSupport.md`
- `README.md`
- Host-local PowerShell 7 profile (`$PROFILE`, outside the Git repository)

## In scope

- `.agents/scripts/Start-AgentWorktreeSession.ps1`: `:4` (make
  `$RepositoryRoot` optional), `:19-26` (add `elseif` default `ClientArguments`
  for `agy`, `codex`, and `opencode` when unbound), `:33-36` (add scalar
  `git rev-parse --show-toplevel` fallback and
  `(Get-WorktreeCliRepositoryIdentity $PSScriptRoot).Repository` vs
  `$primary.CommonDirectory` check), and `:110-112` (default `ClientExecutable`
  to `'pwsh'` for `opencode` when not `-PrepareOnly`).
- Relocate `.agents/agy-worktree.ps1` to `.antigravity/agy-worktree.ps1`
  (removing `.agents/agy-worktree.ps1`) and replace its body with the thin shim
  keeping `Invoke-AgyWorktree`, dot-source guard, and catchable failure.
- `.codex/codex-worktree.ps1`: replace body with the parallel thin shim keeping
  `Invoke-CodexWorktree`, dot-source guard, and catchable failure.
- `.opencode/opencode-worktree.ps1`: replace body with the thin shim keeping its
  dot-source guard and `-PrepareOnly` forwarding.
- `.agents/references/change-workflow.md:159-164`: update wrapper path and
  rewrap only that paragraph.
- `.agents/skills/eod/SKILL.md:71`: update reattach command path.
- `Documents/FreshMachineSetup.md:112-113`: update wrapper path.
- `Documents/Plans/ChangeWorkflow/RemoveOpenCodeSupport.md:55-58,106-107,147-149`:
  add the `opencode` default branches to `## Design` and `## In scope` and
  update the wrapper path in `## Notes`.
- `README.md:324-328`: update wrapper path.
- Host-local PowerShell 7 `$PROFILE` (`Microsoft.PowerShell_profile.ps1`): edit
  only the `Add-Content ... -Value @(...)` history-push array to prepend
  `'.\.codex\codex-worktree.ps1'` as oldest and replace
  `'.\.agents\agy-worktree.ps1'` with `'.\.antigravity\agy-worktree.ps1'` as
  newest.

## Out of scope

- Moving or symlinking `.agents/hooks.json` or `.agents/agents/*.md`.
- Modifying `.claude/claude-worktree.sh`, `Initialize-AgentWorktreeSession.ps1`,
  `Bootstrap-AgentTools.ps1`, `WorktreeCliSessionExclusion.psm1`,
  `AgentWorktreeSession.psm1`, or `Start-AgentWorktreeSession.ps1:39-109`
  worktree creation, bootstrap, `claude` transport, or `agy` `--settings` logic.
- Changing the `agy` client identifier, `agy/<uuid>` branch prefix, or
  `~/.agy/worktrees/` worktree directory path.
- Deleting OpenCode support (owned by `RemoveOpenCodeSupport.md`), modifying
  root `AGENTS.md`, modifying any C++ or GLSL files, or adding personal profile
  history snippets to `README.md`.
- Modifying `$codexWorktreeScript` dot-sourcing or `function prompt` in the
  host-local PowerShell 7 `$PROFILE`.

## Acceptance criteria

1. **Relocation, syntax, and parameter metadata**:
   `.antigravity/agy-worktree.ps1` exists, `.agents/agy-worktree.ps1` does not
   exist, all four PowerShell scripts parse with zero errors under
   `[System.Management.Automation.Language.Parser]::ParseFile(...)`, and
   `-RepositoryRoot` on `.agents/scripts/Start-AgentWorktreeSession.ps1` has
   `Mandatory = $false`.
2. **Shim forwarding, dot-source guards, and catchable-vs-exit behavior**: using
   a temporary scratch harness under `Temp/shim-verify/` (deleted by exact
   literal path afterward) with a stub `.agents/scripts/Start-AgentWorktreeSession.ps1`
   printing `CLIENT=$Client|REATTACH=$ReattachWorktree|PREPARE=$PrepareOnly` and
   exiting `$env:STUB_EXIT`:
   - Dot-sourcing each shim under `Set-StrictMode -Version Latest` emits zero
     stdout and exports `Invoke-AgyWorktree` and `Invoke-CodexWorktree`.
   - Direct and dot-sourced calls forward omitted and spaced `-ReattachWorktree`
     values and OpenCode's `-PrepareOnly` (`False` and `True`) accurately.
   - With `STUB_EXIT=7`, direct invocation of each shim exits `7` with empty
     stderr, while dot-sourced `Invoke-AgyWorktree` and `Invoke-CodexWorktree`
     throw catchably so a trailing statement after `try { ... } catch { ... }`
     executes.
3. **Edited launcher runtime checks** (resolving the worktree's edited script
   paths via `(Resolve-Path '<repo-relative path>').Path` and using child
   `pwsh -NoProfile -WorkingDirectory <dir>` calls):
   - From `<PrimaryRoot>`, running the edited `Start-AgentWorktreeSession.ps1`
     with `-Client agy -ReattachWorktree <worktree>` (omitting `-RepositoryRoot`
     and `-ClientArguments`, without `-PrepareOnly`) and `-ClientExecutable`
     pointing to Git's `usr\bin\echo.exe` exits `0` with `Reattached worktree`,
     `--dangerously-skip-permissions`, and `--settings` naming `<worktree>` in
     `trustedWorkspaces`; re-running with `-ClientArguments @('--custom-flag')`
     replaces `--dangerously-skip-permissions` while still appending
     `--settings`.
   - From a foreign ordinary Git repository (e.g., a temporary `git init` repo
     under `Temp/foreign-repo-verify/`, deleted by exact literal path
     afterward), running the edited `.codex/codex-worktree.ps1` once exits `1`
     with `does not belong to this launcher's repository` on stderr and leaves
     zero new branches, worktrees, or config changes in that repository.
   - From `C:\Windows`, running each edited shim directly exits `1` with stderr
     matching `^(agy|codex|opencode)-worktree must be run inside a Git
     repository\.`, and dot-sourced `Invoke-CodexWorktree` and
     `Invoke-AgyWorktree` throw catchably so a trailing statement executes.
   - Code reading of `Start-AgentWorktreeSession.ps1` confirms `claude`
     `BROKEN_ENGINE_CLIENT_ARGUMENTS` decode/clear ordering, `codex` and
     `opencode` default `ClientArguments`, and `opencode` `'pwsh'` default
     `ClientExecutable` match `## Design`.
4. **Reference sync and workflow checks**:
   - `git grep -n '\.agents[/\\]agy-worktree\.ps1' -- ':!Documents/Plans/ChangeWorkflow/RelocateAntigravityWrapperAndCentralizeLauncherDefaults.md'`
     exits `1` with empty output;
     `git grep -n '\.antigravity[/\\]agy-worktree\.ps1' -- ':!Documents/Plans/ChangeWorkflow/RelocateAntigravityWrapperAndCentralizeLauncherDefaults.md'`
     matches the five tracked target files (or the four non-Plan target files
     if `RemoveOpenCodeSupport.md` has already landed); when
     `RemoveOpenCodeSupport.md` is present, its `## Design` and `## In scope`
     both include the `opencode` default branches.
   - `pwsh -NoProfile -File .agents/scripts/Write-SessionWorkflowContext.ps1 -Part 2`
     exits `0`, emits `.antigravity/agy-worktree.ps1`, and omits
     `.agents/agy-worktree.ps1`.
   - `pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1 -RepositoryRoot '<worktree root>' -Baseline <baseline SHA> -IncludeUntracked`
     exits `0` and `pwsh -NoProfile -File .agents/scripts/Invoke-PlanSchedulerStateCheck.ps1`
     exits `0` with empty `diagnostics`.
5. **Host PowerShell 7 profile**: run on the host where PowerShell 7's
   `$PROFILE` resolves to the user's existing `Microsoft.PowerShell_profile.ps1`
   containing the `Add-Content ... ConsoleHost_history.txt` history-push array
   (not inside an isolated sandbox where `$PROFILE` is absent). `$PROFILE`
   parses with zero errors, keeps UTF-8 without BOM and LF line endings, lists
   `'.\.codex\codex-worktree.ps1'` first (oldest) and
   `'.\.antigravity\agy-worktree.ps1'` last (newest) in its
   `Add-Content ... -Value @(...)` array, and has zero
   `.agents\agy-worktree.ps1` references.

## Notes

- `dependsOn` is intentionally `[]`: although this Plan is authored against the
  current tree where `.opencode/opencode-worktree.ps1` and
  `RemoveOpenCodeSupport.md` are present, `Tools/WorktreeCli/PlanScheduler.cpp`
  can skip a claimed Plan and select `RemoveOpenCodeSupport.md` independently,
  so `RemoveOpenCodeSupport.md` may land first. If `RemoveOpenCodeSupport.md`
  has already landed by execution time, the executing session omits the
  `.opencode/opencode-worktree.ps1` shim, the `opencode` branches in
  `Start-AgentWorktreeSession.ps1`, and the `RemoveOpenCodeSupport.md` edits.
- No build or `/agent-harness` verification is required (no C++ or GLSL files
  are modified).
