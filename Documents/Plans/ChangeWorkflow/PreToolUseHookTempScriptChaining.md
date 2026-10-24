<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-24T21:19:17.875Z","dependsOn":[]} -->
# Fix: Invoke-PreToolUseHook.ps1 — chained-script rule blocks ad hoc Temp/ scripts

## Context
Observed symptom: during a `/next-plan` run, main ran one PowerShell tool call
that looped over several files and, inside a `foreach (...) { ... }` body,
called `& pwsh -NoProfile -File <untracked ad hoc script under Temp/> -Path $f`
once per file. The PreToolUse hook blocked the call with its `ChainedScript`
message ("run one repository script per shell call with nothing chained before
or after it; ..."). Main then reimplemented the script's FBX material scan
inline in the tool call to get the same result.

The script was not a repository script: it was an untracked file the session
wrote under `Temp/`. The root `AGENTS.md` `## Directives` "Bundled scripts"
rule, which this hook backs up, governs repository scripts only. The hook
already treats `Temp/` as a session scratch area for its rooted-path rule
(header comment line 7; `Test-AbsoluteScriptPath`,
`.agents/scripts/Invoke-PreToolUseHook.ps1:501-512`, exempts paths under
`<cwd>/Temp`), but its chained-script rule (header comment lines 8-13) does not:
`Get-ScriptCall` (`.agents/scripts/Invoke-PreToolUseHook.ps1:644-666`) returns a
script call for every non-rooted `-File` `.ps1` path, a `& '<.ps1>'` `-Command`
argument, and every `Import-Module ./<path>`, with no `Temp/` check, so
`Test-ChainedScriptCall` (`:678-707`) blocks any other element in the same call.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 5dcbdc70-8ebe-4339-a698-b4d8b614d50e
- Worktree/branch UUID: f524889d-98ac-40da-8484-00ea2c8f9455
- Session branch: claude/f524889d-98ac-40da-8484-00ea2c8f9455
- Worktree: .claude\worktrees\BrokenEngine\f524889d-98ac-40da-8484-00ea2c8f9455
- Landing ref: claude/f524889d-98ac-40da-8484-00ea2c8f9455
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/PreToolUseHookTempScriptChaining.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above. OpenCode transcript review remains unsupported regardless of worktree
  retention.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode — the landing ref —
supplying the recorded client and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

Author's recommendation: make `Get-ScriptCall` return no script call when the
`.ps1` or module path resolves under `<cwd>/Temp/`, reusing the comparable-path
helpers `Test-AbsoluteScriptPath` already uses, so the chained-script rule and
the rooted-path rule agree on what a repository script is. Rationale: the prose
rule the hook backs up covers repository scripts only, and the hook already
draws that line at `Temp/` for its other script rule.

## Critical files
- `.agents/scripts/Invoke-PreToolUseHook.ps1`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to
  `.agents/scripts/Invoke-PreToolUseHook.ps1`: `Get-ScriptCall` and the header
  comment's chained-script bullet (lines 8-13)

## Out of scope
- The landed change the session produced
- The root `AGENTS.md` "Bundled scripts" rule and every other hook rule
  (directory change, execution policy, rooted script path, git stash, in-place
  edit, computed deletion)
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: one hook script's classification);
escalate if the fix reaches build/bootstrap coordination. The hook keeps failing
open on parse errors and keeps blocking every chained repository script call.
Never embed transcript paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation: a
  PowerShell tool call that runs `& pwsh -NoProfile -File Temp/<name>.ps1 ...`
  inside a `foreach` body is allowed
- A repository script call chained with another command, such as
  `pwsh -NoProfile -File .agents/scripts/Invoke-PlanSchedulerStateCheck.ps1; git status`,
  is still blocked with the `ChainedScript` message
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
