# Codex CLI Workaround Audit

Audits every remaining Codex CLI host workaround in this repository in two
phases:

1. **Verify** on the currently installed Codex CLI whether each underlying bug
   or limitation still reproduces using its minimal probe below.
2. **Resolve or track**:
   - If the issue is **fixed**, remove or simplify the workaround through the
     Change Workflow (or promote a larger cleanup to a Plan under
     `Documents/Plans/ChangeWorkflow/`).
   - If the issue is **still present**, search `openai/codex` for an existing
     issue (upvote or add reproduction details) or file a new report, then
     track the remaining workaround and its upstream issue URL in
     `Documents/Investigations/ChangeWorkflow/CodexCliWorkaroundCleanup.md`
     (mirroring
     `Documents/Investigations/ChangeWorkflow/AntigravityWorkaroundCleanup.md`).

## Summary

| Item | Workaround area | Primary owning files | Status |
| --- | --- | --- | --- |
| 1 | No native subdirectory `AGENTS.md` loading | `.codex/config.toml`, `.agents/scripts/Invoke-SubdirectoryAgentsHook.ps1`, `.agents/references/subagent-reporting.md` | Unverified |
| 2 | `SessionStart` / `PostToolUse` spill threshold | `.codex/config.toml`, `.agents/scripts/Write-SessionWorkflowContext.ps1` | Unverified |
| 3 | Project `[[skills.config]]` parsed but not applied | `.agents/skills/external-skill-creator/references/client-compatibility.md`, `frontmatter-schema.md` | Unverified |
| 4 | Headless `codex exec` hook, role, and API-key limits | `.agents/scripts/Invoke-Codex.ps1`, `.agents/skills/sweep/scripts/Invoke-Sweep.ps1`, `.agents/references/codex-bridge.md` | Unverified |
| 5 | Windows sandbox and ACL restrictions | `README.md`, `.agents/references/codex-bridge.md`, `.agents/scripts/AgentScriptCommon.psm1`, `.agents/skills/sweep/scripts/Invoke-Sweep.ps1` | Unverified |
| 6 | Missing session ID, fresh reattach, skipped checkpoint | `.agents/skills/eod/SKILL.md`, `.agents/skills/next-plan/references/follow-up-provenance.md`, `run-checkpoint.md`, `Find-AgentSessionTranscript.ps1` | Unverified |
| 7 | Per-role `service_tier` override in `.codex/agents/*.toml` | `.codex/agents/reviewer.toml`, `Documents/Features/Agents/CodexHostedReviewFastServiceTier.md` | Unverified |

## 1. No native subdirectory `AGENTS.md` loading

### Current workaround

- `.codex/config.toml` registers a `[[hooks.PostToolUse]]` entry
  (`matcher = "^(Bash|apply_patch)$"`) invoking
  `.agents/scripts/Invoke-SubdirectoryAgentsHook.ps1`.
- `.agents/scripts/Invoke-SubdirectoryAgentsHook.ps1`
  (`Invoke-CodexPostToolUse`, `Get-SafeAgentsCandidate`,
  `Get-ShellAccessedTargets`, `Add-AccessedTarget`, `Format-EmissionText`)
  parses `Bash` commands and `apply_patch` headers, then streams the session
  transcript to deduplicate unseen in-worktree ancestor `AGENTS.md` files.
- `.agents/references/subagent-reporting.md` (`## Task brief`) instructs
  callers to derive nested `AGENTS.md` chains through
  `.agents/skills/update-claude-docs/scripts/Get-AffectedAgentsDocs.ps1`
  (explicitly noting hookless Codex runs).
- Cross-client sharing: `.agents/scripts/Invoke-SubdirectoryAgentsHook.ps1` is
  shared with Antigravity CLI (`.agents/hooks.json` `PostInvocation`,
  `Invoke-AgyPostInvocation`), and `.agents/scripts/AgentShellLexer.psm1` is
  shared with `.agents/scripts/Invoke-PreToolUseHook.ps1`.

### How to check if the issue still exists

Temporarily disable the `[[hooks.PostToolUse]]` entry in `.codex/config.toml`
in a scratch Codex session, then read and edit a file inside a nested directory
that has ancestor `AGENTS.md` files (plus one path under `Temp/` or through a
reparse point). Inspect the session transcript to confirm whether Codex
automatically injects each unseen in-worktree ancestor `AGENTS.md` at most once
per session without injecting `Temp/` or reparse-point targets. Repeat the read
in a headless `codex exec` run to check whether hookless runs also load
ancestor `AGENTS.md` files natively.

### If fixed (cleanup)

- Remove the `[[hooks.PostToolUse]]` block from `.codex/config.toml`.
- Remove `Invoke-CodexPostToolUse`, the `PostToolUse` dispatch branch, and
  Codex-only helpers (`$script:ApplyPatchHeaderRegex`, Codex transcript
  scanning) from `.agents/scripts/Invoke-SubdirectoryAgentsHook.ps1`.
- Delete `.agents/scripts/Invoke-SubdirectoryAgentsHook.ps1` only when
  Antigravity CLI also no longer needs it (tracked separately in
  `Documents/Investigations/ChangeWorkflow/AntigravityWorkaroundCleanup.md`,
  item 6).
- Keep `.agents/scripts/AgentShellLexer.psm1` while
  `.agents/scripts/Invoke-PreToolUseHook.ps1` (or any other script) still
  imports it; delete it only when no remaining script imports it.
- Remove the hookless-Codex fallback note from
  `.agents/references/subagent-reporting.md` (`## Task brief`) only if headless
  `codex exec` also loads subdirectory `AGENTS.md` files natively.

### If still present (report and track)

Search `openai/codex` for an existing issue requesting native automatic loading
of ancestor subdirectory `AGENTS.md` files on file access (upvote/comment or
file a new issue), then record this workaround and its issue URL in
`Documents/Investigations/ChangeWorkflow/CodexCliWorkaroundCleanup.md`.

## 2. `SessionStart` / `PostToolUse` spill threshold

### Current workaround

- `.codex/config.toml` sets the `additionalContextLimit` override on all four
  `[[hooks.SessionStart.hooks]]` entries (running
  `.agents/scripts/Write-SessionWorkflowContext.ps1`) and on the
  `[[hooks.PostToolUse.hooks]]` entry (running
  `.agents/scripts/Invoke-SubdirectoryAgentsHook.ps1`) so Codex does not spill
  hook context past its default threshold while
  `Write-SessionWorkflowContext.ps1` already bounds each part via
  `$script:MaximumPartCharacters`.

### How to check if the issue still exists

Temporarily remove `additionalContextLimit` from every
`[[hooks.SessionStart.hooks]]` entry and from `[[hooks.PostToolUse.hooks]]` in
`.codex/config.toml`. Start a fresh Codex session so all startup context parts
run (including parts whose output exceeds Codex's default spill threshold),
then trigger a `PostToolUse` injection whose emitted ancestor `AGENTS.md`
chain also exceeds the default spill threshold. Inspect the session transcript
to verify that every `SessionStart` part and the `PostToolUse` payload arrive
completely inline without spilling to a file preview or truncating.

### If fixed (cleanup)

Remove `additionalContextLimit` and its explanatory comment from every hook
entry in `.codex/config.toml`.

### If still present (report and track)

Search `openai/codex` for an existing issue on the default
`additionalContextLimit` spill threshold for `SessionStart` / `PostToolUse`
hooks (upvote/comment or file a new issue), then record this workaround and its
issue URL in
`Documents/Investigations/ChangeWorkflow/CodexCliWorkaroundCleanup.md`.

## 3. Project `.codex/config.toml` ignores `[[skills.config]]` enablement

### Current workaround

- `.agents/skills/external-skill-creator/references/client-compatibility.md`
  (`## Codex`) records that project `.codex/config.toml` parses
  `[[skills.config]]` without applying it to skill enablement (unlike user
  `$CODEX_HOME/config.toml` or CLI `-c skills.config=...`), forcing per-skill
  `agents/openai.yaml` sidecar files (`policy.allow_implicit_invocation:
  false`) to control Codex invocation policy.
- `.agents/skills/external-skill-creator/references/frontmatter-schema.md`
  (`## Package members` and `## Codex companion file`) defines and validates the
  `agents/openai.yaml` companion contract.

### How to check if the issue still exists

Add a temporary `[[skills.config]]` entry in project `.codex/config.toml`
disabling a test skill without editing `$CODEX_HOME/config.toml` or passing
`-c skills.config=...`, start a fresh Codex session, and confirm both whether
the skill is disabled and whether explicit invocation still works (unlike
`policy.allow_implicit_invocation: false`, disabling the skill altogether does
not preserve explicit invocation). Separately check whether Codex honors
`disable-model-invocation: true` in `SKILL.md` frontmatter natively by
disabling implicit invocation while keeping explicit invocation working.

### If fixed (cleanup)

- Update
  `.agents/skills/external-skill-creator/references/client-compatibility.md`
  (`## Codex`) to record verified project-level `[[skills.config]]` (or native
  frontmatter) enforcement.
- Only if the verified mechanism preserves explicit invocation while disabling
  implicit invocation, decide whether to drop `policy` entries from
  `agents/openai.yaml` sidecars — retaining `agents/openai.yaml` for skills
  that carry `interface` metadata (currently
  `.agents/skills/code-quality-metrics/agents/openai.yaml` and
  `.agents/skills/external-diagnose-bug/agents/openai.yaml`) and keeping
  `interface` and `dependencies` support in
  `.agents/skills/external-skill-creator/references/frontmatter-schema.md` and
  `Validate-Skill.ps1` unless those sidecar roles are also retired.

### If still present (report and track)

Search `openai/codex` for an existing issue where project `.codex/config.toml`
parses `[[skills.config]]` without enforcing it (upvote/comment or file a new
issue), then record this workaround and its issue URL in
`Documents/Investigations/ChangeWorkflow/CodexCliWorkaroundCleanup.md`.

## 4. Headless `codex exec` hook, role, and API-key limitations

### Current workaround

- `--disable hooks`: `.agents/scripts/Invoke-Codex.ps1` and
  `.agents/skills/sweep/scripts/Invoke-Sweep.ps1` (`Invoke-SweepCodex` and the
  propagation launch) pass `--disable hooks` to `codex exec` so
  `.codex/config.toml` `SessionStart` hooks do not inject main-session workflow
  instructions into headless subagent runs when hook trust is persisted.
- Role-prompt carriage: `.agents/references/codex-bridge.md` (`## Purpose`)
  notes that `codex exec` loads no role definitions from `.codex/agents/*.toml`,
  so the prompt's `Role` line carries the role assignment manually.
- `OPENAI_API_KEY` guard: `.agents/scripts/Invoke-Codex.ps1` refuses to launch
  when `OPENAI_API_KEY` is present in the process environment, and
  `.agents/skills/sweep/scripts/Invoke-Sweep.ps1` clears `Env:OPENAI_API_KEY`
  before calling `codex exec`, preventing headless runs from billing metered
  API credits instead of the ChatGPT subscription. `README.md`
  (`### Codex CLI`) documents the same restriction.

### How to check if the issue still exists

Test each behavior independently with the installed `codex exec`:

1. Run `codex exec` in a hook-trusted worktree without `--disable hooks` and
   check whether `SessionStart` hooks still inject into headless runs (or
   whether an upstream hook-scope setting separates interactive and headless
   sessions).
2. Check `codex exec --help` and test whether `codex exec` can load a named
   role from `.codex/agents/<role>.toml` directly (applying its model,
   reasoning effort, and `developer_instructions`).
3. With a valid ChatGPT login (`codex login status`) and a dummy
   `OPENAI_API_KEY` set in the environment, test whether `codex exec` still
   attempts API-key auth or provides a flag/config to force subscription auth.

### If fixed (cleanup)

- Remove `--disable hooks` from `.agents/scripts/Invoke-Codex.ps1` and
  `.agents/skills/sweep/scripts/Invoke-Sweep.ps1` if `codex exec` no longer
  runs main-session `SessionStart` hooks (or adopt the upstream hook-scope
  config).
- Update `.agents/scripts/Invoke-Codex.ps1` and
  `.agents/references/codex-bridge.md` to pass the native role selector if
  `codex exec` loads `.codex/agents/*.toml` directly.
- Replace the `OPENAI_API_KEY` refusal/removal in
  `.agents/scripts/Invoke-Codex.ps1`,
  `.agents/skills/sweep/scripts/Invoke-Sweep.ps1`, and `README.md` with the
  upstream subscription-auth selector if supported, or retain the guard as
  defense-in-depth if `OPENAI_API_KEY` still takes precedence.

### If still present (report and track)

Search `openai/codex` for each still-reproducing limitation (headless
`SessionStart` hook isolation, native `.codex/agents/*.toml` role loading in
`codex exec`, and subscription-auth selection when `OPENAI_API_KEY` is set),
upvote/comment or file new issues, and record each URL in
`Documents/Investigations/ChangeWorkflow/CodexCliWorkaroundCleanup.md`.

## 5. Windows sandbox and ACL restrictions

### Current workaround

- MSIX/Store `pwsh` failure: `README.md` requires installing PowerShell 7 with
  `--installer-type wix` (MSI) instead of the default MSIX/Store package
  because `WindowsApps` ACLs cannot be granted to Codex sandbox users, causing
  sandboxed `pwsh` commands to fail with access denied.
- `read-only` sandbox restrictions:
  - `.agents/references/codex-bridge.md` (`## Rules`) states that `read-only`
    runs cannot write `<worktree>/Temp/` or `$env:TEMP`.
  - `Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md` (lines 33,
    51, and 57) records `read-only` sandbox failures where Git rejects
    primary-checkout ownership checks and where
    `external-skill-creator:validate` cannot create disposable test fixtures.
  - `.agents/scripts/AgentScriptCommon.psm1`,
    `.agents/scripts/Invoke-StaticChecks.ps1`,
    `.agents/scripts/SessionChangeInventory.psm1`,
    `.agents/scripts/Find-SessionCandidates.ps1`,
    `.agents/scripts/Get-SessionChangeInventory.ps1`, and
    `.agents/skills/compile/scripts/Get-CompileRewrittenPaths.ps1` set
    `GIT_OPTIONAL_LOCKS=0` so read-only Git queries do not fail refreshing the
    index.
- `workspace-write` sandbox `.agents/` block:
  `.agents/skills/sweep/scripts/Invoke-Sweep.ps1` rejects sweep targets under
  `.agents/` (`Codex workspace-write cannot edit .agents/`), and
  `.agents/skills/sweep/SKILL.md` (`## Inputs` and `## Rules`) declines
  `.agents/` edits and routes them to the deferred-fixes record.

### How to check if the issue still exists

Test each Windows sandbox limitation independently on the installed Codex CLI:

1. On a machine with MSIX/Store `pwsh`, run a trivial
   `codex exec --sandbox read-only` PowerShell command and check whether it
   still fails with access denied.
2. In a worktree, run `codex exec --sandbox read-only` to test (a) creating a
   disposable file under both `<worktree>/Temp/` and `$env:TEMP`, (b) running
   `git -C <primary checkout> status` or `Get-NextPlanContext` against the
   primary checkout without a Git ownership failure, and (c) running `git
   status` without `GIT_OPTIONAL_LOCKS=0`.
3. In a scratch worktree, run `codex exec --sandbox workspace-write` and
   attempt a trivial edit to a file under `.agents/` to confirm whether
   `.agents/` remains write-protected.

### If fixed (cleanup)

- If MSIX `pwsh` works inside the Windows sandbox, relax the
  `--installer-type wix` requirement in `README.md`.
- Update `.agents/references/codex-bridge.md` (`## Rules`) for whichever of
  `<worktree>/Temp/` and `$env:TEMP` is verified writable in `read-only` runs
  (allowing disposable fixture checks under `external-skill-creator:validate`
  without escalating sandboxes if `$env:TEMP` writes succeed, and lifting the
  `<worktree>/Temp/` overflow restriction only if worktree `Temp/` writes also
  succeed), and note primary-checkout Git query support if that probe passes.
  Keep `GIT_OPTIONAL_LOCKS=0` in `.agents/scripts/AgentScriptCommon.psm1` and
  sibling scripts regardless, as it also avoids index-lock contention with
  concurrent builds and workers.
- If `workspace-write` can edit `.agents/` (or provides a config key to allow
  it), remove the `.agents/` target guard from
  `.agents/skills/sweep/scripts/Invoke-Sweep.ps1` and the `.agents/` deferral
  rule from `.agents/skills/sweep/SKILL.md`.

### If still present (report and track)

Search `openai/codex` for each still-reproducing Windows sandbox issue (MSIX
`WindowsApps` ACL denial, `read-only` `<worktree>/Temp/` and `$env:TEMP`
writes, Git worktree primary-checkout ownership, and `workspace-write`
blocking `.agents/`), upvote/comment or file new issues, and record each URL
in `Documents/Investigations/ChangeWorkflow/CodexCliWorkaroundCleanup.md`.

## 6. Missing session ID, fresh-conversation reattach, and skipped checkpoint

### Current workaround

- Fresh-conversation reattach: `.agents/skills/eod/SKILL.md` (`## Steps`
  step 6 and `## Rules`) documents that
  `.\.codex\codex-worktree.ps1 -ReattachWorktree '<path>'` (via
  `.agents/scripts/Start-AgentWorktreeSession.ps1`) starts a new conversation
  instead of resuming the prior one, requiring `Temp/eod-resume.md` to carry
  session state across reattach.
- Unrecorded conversation session ID:
  `.agents/skills/next-plan/references/follow-up-provenance.md`
  (`## Conversation session ID`) records no Codex conversation session ID in
  follow-up provenance (unlike Claude's `CLAUDE_CODE_SESSION_ID` or `agy`'s
  `ANTIGRAVITY_CONVERSATION_ID`), even though Codex hooks already receive
  `transcript_path` (used by
  `.agents/scripts/Invoke-SubdirectoryAgentsHook.ps1`). As a result,
  `.agents/skills/next-plan-review/scripts/Find-AgentSessionTranscript.ps1`
  falls back to `bounded-commit-window` discovery across `~/.codex/sessions`
  and `~/.codex/archived_sessions` matched against eligible retained
  worktrees.
- Skipped live checkpoint review:
  `.agents/skills/next-plan/references/run-checkpoint.md` (`## Dispatch` and
  `## Measurement states`) skips `/next-plan-checkpoint-review` for Codex main
  sessions (`none (codex)`), leaving friction, isolation, and
  context-efficiency review to post-landing `/next-plan-review`.

### How to check if the issue still exists

Verify each capability on the installed Codex CLI:

1. Inside a Codex session, inspect shell and hook environment variables (and
   Codex documentation) for an exposed conversation or thread session ID.
2. Check whether `codex` provides a non-interactive or worktree-scoped resume
   flag that `.agents/scripts/Start-AgentWorktreeSession.ps1` can pass on
   `-ReattachWorktree` to resume the worktree's prior conversation.
3. Confirm whether the active session's JSONL rollout file under
   `~/.codex/sessions` is flushed and readable mid-session by a delegated
   `reviewer` subagent using that session ID, enabling live
   `/next-plan-checkpoint-review` projection.

### If fixed (cleanup)

- If Codex exposes a conversation session ID, document and read it in
  `.agents/skills/next-plan/references/follow-up-provenance.md`
  (`## Conversation session ID`) and pass it to
  `Find-AgentSessionTranscript.ps1 -SessionId` (keeping `bounded-commit-window`
  only for legacy commits without a recorded ID).
- If Codex supports resuming a worktree's conversation on startup, wire it into
  `.agents/scripts/Start-AgentWorktreeSession.ps1` (`-Client codex` with
  `-ReattachWorktree`) and update the Conversation column in
  `.agents/skills/eod/SKILL.md` (step 6).
- If live mid-session transcript resolution is reliable for Codex, add a
  Codex transcript resolver to
  `.agents/skills/next-plan-checkpoint-review/references/worker.md` (step 1)
  and enable `/next-plan-checkpoint-review` for Codex main sessions in
  `.agents/skills/next-plan/references/run-checkpoint.md` and
  `.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1`.

### If still present (report and track)

Search `openai/codex` for issues requesting an exposed conversation/thread
session ID environment variable, worktree-scoped conversation resume on
startup, and reliable live mid-session rollout transcript access (upvote,
comment, or file new issues for each gap that still reproduces), then record
each URL in
`Documents/Investigations/ChangeWorkflow/CodexCliWorkaroundCleanup.md`.

## 7. Per-role `service_tier` override in `.codex/agents/*.toml`

### Current workaround

- `.codex/agents/reviewer.toml` (and sibling files under `.codex/agents/`)
  omits `service_tier`, so Codex-hosted reviews inherit whatever `service_tier`
  the user's `~/.codex/config.toml` sets.
- `Documents/Features/Agents/CodexHostedReviewFastServiceTier.md` tracks the
  deferred fast-tier reviewer option. Its session findings confirmed that Codex
  CLI parses `service_tier` in per-agent role TOMLs, but left unproven whether
  a per-role `service_tier` actually overrides a user-level
  `~/.codex/config.toml` `service_tier = "default"` setting in an observed run.

### How to check if the issue still exists

With `service_tier = "default"` configured in user `~/.codex/config.toml`,
temporarily add `service_tier = "fast"` to `.codex/agents/reviewer.toml`,
dispatch one trivial `reviewer` subagent from a Codex-hosted session, and
inspect the session transcript or API request metadata (not only the absence of
warnings) to confirm whether the request actually ran on the fast/priority tier
instead of falling back to the user-level default.

### If fixed (cleanup)

Update `Documents/Features/Agents/CodexHostedReviewFastServiceTier.md` to
record that per-role `service_tier` override is verified (refreshing its stale
role filenames to `.codex/agents/reviewer.toml` when that feature is
revisited). Keep `.codex/agents/reviewer.toml` on the default tier unless the
user explicitly opts into fast-tier credit burn.

### If still present (report and track)

Search `openai/codex` for an existing issue where per-agent `service_tier` in
`.codex/agents/*.toml` fails to override top-level `~/.codex/config.toml`
`service_tier` (upvote/comment or file a new issue), then record this
workaround and its issue URL in
`Documents/Investigations/ChangeWorkflow/CodexCliWorkaroundCleanup.md`.
