# Claude Code Workaround Audit

Audits every remaining repository workaround introduced for Claude Code CLI
bugs or limitations. Work through this document in two phases:

1. **Verify on current Claude Code CLI:** run each item's probe on the current
   installed Claude Code CLI to confirm whether the underlying bug or
   limitation still reproduces.
2. **Clean up or report and track:**
   - If the issue is fixed, remove or simplify the workaround through the
     Change Workflow (accounting for Codex and Antigravity CLI wherever a
     mechanism is shared).
   - If the issue still reproduces, search `anthropics/claude-code` for an
     existing issue (comment or upvote if found) or file a new report, then
     record the remaining workaround and its issue URL in
     `Documents/Investigations/ChangeWorkflow/ClaudeCodeWorkaroundCleanup.md`
     (mirroring
     `Documents/Investigations/ChangeWorkflow/AntigravityWorkaroundCleanup.md`).

## Summary

| Item | Workaround area | Primary owning files | Status |
| --- | --- | --- | --- |
| 1 | Bypass-permissions shell file-edit prompt | `AGENTS.md` | Unverified |
| 2 | Subagent `Write` refusal on report-like `.md` names | `AGENTS.md`, `.agents/references/subagent-handoff.md` | Unverified |
| 3 | `SessionStart` hook stdout cap four-part split | `.agents/scripts/Write-SessionWorkflowContext.ps1`, `.claude/settings.json`, `.codex/config.toml`, `.agents/hooks.json` | Unverified |
| 4 | `disallowed-tools` leaking to the invoking turn | `.agents/skills/external-skill-creator/references/frontmatter-schema.md`, `.agents/skills/external-skill-creator/references/client-compatibility.md`, `.agents/skills/external-skill-creator/scripts/Validate-Skill.ps1` | Unverified |
| 5 | `Glob`/`Grep` symlink skip, `/` glob prefix, `.claude/skills` link | `AGENTS.md`, `.agents/scripts/Initialize-AgentWorktreeSession.ps1` | Unverified |
| 6 | Blocking shell command timeout ceiling | `.agents/scripts/Invoke-Codex.ps1`, `.agents/references/codex-bridge.md`, `.agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1`, `.agents/skills/agent-harness/references/worker.md`, `.agents/skills/code-quality-metrics/references/worker.md`, `.agents/skills/sweep/SKILL.md` | Unverified |
| 7 | Duplicate handoff delivery via `TaskOutput` | `.agents/references/subagent-reporting.md` | Unverified |

## 1. Bypass-permissions prompt injecting shell file-edit preference

### Current workaround

- `AGENTS.md` (`## Environment`): explicitly directs Claude Code sessions to
  ignore the bypass-permissions host instruction that prefers `Bash`, `sed`,
  heredocs, or scripts for file edits, and instead use `Edit` for tracked
  files and `Write` for new files so UTF-8 BOM, CRLF, and trailing newlines
  remain intact.

### How to check if the issue still exists

Start a current Claude Code CLI session in bypass-permissions mode, inspect
the injected system prompt for any directive preferring shell tools (`Bash`,
`sed`, heredocs, or scripts) over `Edit`/`Write`, and run a small scratch
file modification without the `AGENTS.md` override to confirm whether the
host still steers edits through shell commands.

### If fixed (cleanup)

Remove the bypass-permissions `Bash`/`sed`/heredoc override bullet from
`AGENTS.md` (`## Environment`). Keep the repository's general encoding and
line-ending checks in `.agents/scripts/Invoke-StaticChecks.ps1`.

### If still present (report and track)

Search `anthropics/claude-code` for an existing issue on bypass-permissions
mode injecting shell file-edit instructions; upvote or comment on an existing
issue, or file a new report explaining the BOM/CRLF/newline corruption risk.
Add the workaround and issue URL to
`Documents/Investigations/ChangeWorkflow/ClaudeCodeWorkaroundCleanup.md`.

## 2. Subagent `Write` refusal on report-like `.md` filenames

### Current workaround

- `AGENTS.md` (`## Environment`): exempts every `Temp/` file cited under a
  handoff's `Evidence` field from Claude Code's host note against writing
  report files.
- `.agents/references/subagent-handoff.md` (`## Handoffs`): instructs briefs
  and workers naming a `Temp/` overflow file to prefix filenames that would
  otherwise start with `analysis`, `report`, `findings`, or `summary` in any
  case (for example `temp-analysis.md`) because the Claude Code host refuses
  a subagent `Write` to those prefixes.

### How to check if the issue still exists

In a current Claude Code CLI session, dispatch a subagent to call `Write` on
scratch `.md` paths under `Temp/` starting with `analysis`, `report`,
`findings`, and `summary` (both lowercase and capitalized) without a prefix,
and confirm whether each write succeeds without a host refusal or injected
anti-report warning.

### If fixed (cleanup)

Remove the report-file host-note exemption bullet from `AGENTS.md`
(`## Environment`), along with both the report-file exemption pointer and
the filename-prefix workaround sentence in
`.agents/references/subagent-handoff.md` (`## Handoffs`). Retain the shared
`Temp/` overflow handoff mechanism itself.

### If still present (report and track)

Search `anthropics/claude-code` for an existing issue on subagent `Write`
refusing `.md` filenames starting with `analysis`, `report`, `findings`, or
`summary`; upvote or comment if found, or file a new report. Record the
workaround and issue URL in
`Documents/Investigations/ChangeWorkflow/ClaudeCodeWorkaroundCleanup.md`.

## 3. Single `SessionStart` hook output cap forcing four-part split

### Current workaround

- `.agents/scripts/Write-SessionWorkflowContext.ps1` (`$Part`,
  `$script:MaximumPartCharacters`): splits
  `.agents/references/change-workflow.md` on four
  `<!-- session-context-part: <title> -->` marker lines and appends
  `.agents/references/risk-tiers.md` to the last part so each hook stdout
  stays below Claude Code's per-hook character cap (above which Claude Code
  replaces hook output with a saved-file preview).
- `.claude/settings.json` (`hooks.SessionStart`), `.codex/config.toml`
  (`[[hooks.SessionStart.hooks]]`, using `additionalContextLimit`), and
  `.agents/hooks.json` (`change-workflow.SessionStart`, using
  `-InjectStepsJson`) all register the four `-Part` hook invocations.

### How to check if the issue still exists

In a current Claude Code CLI session, configure a temporary single
`SessionStart` hook that prints the full concatenated
`.agents/references/change-workflow.md` plus
`.agents/references/risk-tiers.md` to stdout, then inspect the injected
session context to verify whether the complete text arrives inline without
truncation or saved-file preview replacement.

### If fixed (cleanup)

Verify first that Codex (with `additionalContextLimit`) and Antigravity CLI
(with `-InjectStepsJson`) also ingest the combined single output without
truncation. Then remove the four `<!-- session-context-part: ... -->` markers
from `.agents/references/change-workflow.md`, simplify
`.agents/scripts/Write-SessionWorkflowContext.ps1` to emit the full combined
context in one call (dropping `$Part` and `$script:MaximumPartCharacters`
while keeping `-InjectStepsJson` until Antigravity CLI no longer needs it),
and collapse the four `SessionStart` entries to one in `.claude/settings.json`,
`.codex/config.toml`, and `.agents/hooks.json`.

### If still present (report and track)

Search `anthropics/claude-code` for an existing issue on the `SessionStart`
hook output character cap and saved-file preview replacement; upvote or
comment if found, or file a new report. Record the workaround and issue URL
in `Documents/Investigations/ChangeWorkflow/ClaudeCodeWorkaroundCleanup.md`.

## 4. `disallowed-tools` stripping tools from the invoking turn

### Current workaround

- `.agents/skills/external-skill-creator/references/frontmatter-schema.md`
  and `.agents/skills/external-skill-creator/references/client-compatibility.md`:
  ban `disallowed-tools` in repository skills because Claude Code removes its
  listed tools from the invoking context for the rest of the turn rather than
  scoping the restriction to the skill alone.
- `.agents/skills/external-skill-creator/scripts/Validate-Skill.ps1`
  (`$allowedKeys`): excludes `disallowed-tools` from the accepted frontmatter
  allowlist, so skills state tool bounds in body prose and rely on host
  permission configuration.

### How to check if the issue still exists

Create a scratch skill declaring `disallowed-tools` for one tool (such as
`Edit`) in a current Claude Code CLI session. First run a control check to
confirm that tool is available before skill invocation, then invoke the
skill inline and confirm `disallowed-tools` blocks that tool during skill
execution (so an ignored field is not mistaken for a fix), and finally
attempt to use that tool later in the same turn outside the skill's work to
verify whether the restriction still persists for the remainder of the turn.

### If fixed (cleanup)

Re-evaluate whether `disallowed-tools` is useful as a portable frontmatter
field across Claude Code, Codex, and Antigravity CLI. If adopted, add
`disallowed-tools` to `$allowedKeys` and `$listKeys` in
`.agents/skills/external-skill-creator/scripts/Validate-Skill.ps1` and update
`frontmatter-schema.md` and `client-compatibility.md`. If kept excluded due
to cross-client non-portability, update the rationale in those two references
to drop the Claude Code turn-leak claim.

### If still present (report and track)

Search `anthropics/claude-code` for an existing issue on `SKILL.md`
`disallowed-tools` stripping tools from the invoking turn after skill
execution; upvote or comment if found, or file a new report. Record the
workaround and issue URL in
`Documents/Investigations/ChangeWorkflow/ClaudeCodeWorkaroundCleanup.md`.

## 5. `Glob`/`Grep` symlink skipping, `/` glob prefix, and `.claude/skills` link

### Current workaround

- `AGENTS.md` (`## Environment`): documents that `Glob` and `Grep` directory
  searches skip symlinked `ThirdParty` submodule contents and `.claude/skills`
  (directing callers to search explicit paths, `.agents/skills`, or `rg -L`),
  and notes that a `Grep` `glob` containing `/` matches nothing without a
  leading `**/`.
- `.agents/scripts/Initialize-AgentWorktreeSession.ps1`
  (`Assert-AgentWorktreeSkillsLink`): validates and repairs the `.claude/skills`
  directory symlink to `../.agents/skills` in fresh Windows worktrees so
  Claude Code discovers skills from the canonical `.agents/skills` tree.

### How to check if the issue still exists

In a current Claude Code CLI session inside a session worktree, run three
probes:

1. Run `Glob` and `Grep` against `ThirdParty` and `.claude/skills` to verify
   whether symlinked directories are now traversed.
2. Run `Grep` with a `glob` containing `/` without a leading `**/` (for
   example `plan-*/SKILL.md`) to verify whether nested paths match.
3. Temporarily test whether Claude Code discovers skills directly under
   `.agents/skills` without relying on `.claude/skills`.

### If fixed (cleanup)

Remove whichever `AGENTS.md` (`## Environment`) caveat is resolved while
keeping the `git grep` note for uninitialized `ThirdParty` submodules (which
is a Git behavior). If Claude Code natively discovers `.agents/skills`
without `.claude/skills`, remove the tracked `.claude/skills` symlink,
remove `Assert-AgentWorktreeSkillsLink` and its call from
`.agents/scripts/Initialize-AgentWorktreeSession.ps1`, remove the
`.claude/skills` search note from `AGENTS.md`, and update the
`.claude/skills` setup, recovery, and discovery references in `README.md`
(`## Git`), `Documents/FreshMachineSetup.md` (Steps 1 and 6), and
`.agents/skills/external-skill-creator/references/client-compatibility.md`
(`## Claude Code`).

### If still present (report and track)

Search `anthropics/claude-code` for existing issues covering `Glob`/`Grep`
symlink traversal, `Grep` `glob` matching with `/` without a leading `**/`,
and native `.agents/skills` discovery; upvote or comment on existing issues
or file new reports for any gaps. Record the remaining workarounds and issue
URLs in
`Documents/Investigations/ChangeWorkflow/ClaudeCodeWorkaroundCleanup.md`.

## 6. Blocking shell command timeout ceiling

### Current workaround

- `.agents/scripts/Invoke-Codex.ps1` (`$WaitBudgetSeconds`, `-Wait`) and
  `.agents/references/codex-bridge.md` (steps 3-4): cap each synchronous wait
  below Claude Code's shell command timeout ceiling and return a `running`
  receipt so a follow-up `-Wait` call resumes without hitting the host cap.
- `.agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1`
  (`$WaitBudgetSeconds`, `$StaleHeartbeatSeconds`) and
  `.agents/skills/agent-harness/references/worker.md` (step 5): bound one
  claim wait below Claude Code's tool timeout ceiling and handle
  `claim.wait-timeout` re-invocations or host-backgrounded commands.
- `.agents/skills/code-quality-metrics/references/worker.md` (`### Compare`):
  mandates an explicit elevated shell timeout for full-corpus Compare runs
  instead of the default short call cap.
- `.agents/skills/sweep/SKILL.md` (`### Per batch`, steps 3-4): launches
  `Invoke-Sweep.ps1` detached via `Start-Process` and polls `<pid>` on Claude
  Code because long sweep batches exceed the host's command runtime limit.

### How to check if the issue still exists

In a current Claude Code CLI session, inspect the `Bash` and `PowerShell`
tool schemas for their default and maximum timeout limits, and run a bounded
sleep probe exceeding the prior ceiling (and check any native async-wait
option) to confirm whether long-running commands still hit the cap or get
forced to background.

### If fixed (cleanup)

Simplify each site without breaking Codex or Antigravity CLI contracts:
remove Claude-specific timeout-ceiling guidance from
`.agents/references/codex-bridge.md`,
`.agents/skills/agent-harness/references/worker.md`, and
`.agents/skills/code-quality-metrics/references/worker.md`; collapse the
detached `Start-Process` plus PID-polling branch in
`.agents/skills/sweep/SKILL.md` if Claude Code supports long async tasks
directly; and keep `$WaitBudgetSeconds` in `Invoke-Codex.ps1` and
`Invoke-HarnessClaim.ps1` only where still needed by other hosts or bounded
liveness checks.

### If still present (report and track)

Search `anthropics/claude-code` for an existing issue on raising or
configuring the shell command timeout ceiling for long-running tasks; upvote
or comment if found, or file a new report. Record the workaround and issue
URL in
`Documents/Investigations/ChangeWorkflow/ClaudeCodeWorkaroundCleanup.md`.

## 7. Duplicate handoff delivery via `TaskOutput`

### Current workaround

- `.agents/references/subagent-reporting.md` (`## Whether a worker is still
  running, and interruption`): instructs a Claude Code main session to wait
  for a background worker by ending the turn or doing independent work so the
  host's task notification delivers the handoff once, and forbids calling
  `TaskOutput` because it duplicates the same handoff text already delivered
  by the notification (against the single-consumption rule in
  `.agents/references/subagent-handoff.md` `## Handoffs`).

### How to check if the issue still exists

In a current Claude Code CLI session, dispatch a short background subagent,
let it complete, and check whether `TaskOutput` still exists and whether
calling it alongside or after the task notification injects a second copy of
the subagent's handoff into main context.

### If fixed (cleanup)

Remove the Claude-specific `TaskOutput` prohibition clause from
`.agents/references/subagent-reporting.md` (`## Whether a worker is still
running, and interruption`) while keeping the cross-host single-consumption
rule in `.agents/references/subagent-handoff.md` (`## Handoffs`).

### If still present (report and track)

Search `anthropics/claude-code` for an existing issue on `TaskOutput`
duplicating background subagent completion notifications; upvote or comment if
found, or file a new report. Record the workaround and issue URL in
`Documents/Investigations/ChangeWorkflow/ClaudeCodeWorkaroundCleanup.md`.
