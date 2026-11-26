# Antigravity CLI Workaround Cleanup Tracker

Tracks the repository workarounds introduced for Antigravity CLI (`agy`)
limitations against their upstream `google-antigravity/antigravity-cli` issues.
When a newer `agy` release closes an issue, run the item's verification check
before promoting its cleanup to a Plan under `Documents/Plans/ChangeWorkflow/`
or executing it as a scoped change.

## Summary

| # | Upstream issue | Workaround area |
| --- | --- | --- |
| 1 | [#1853](https://github.com/google-antigravity/antigravity-cli/issues/1853) | Windows Job Object kills detached child processes |
| 2 | [#1854](https://github.com/google-antigravity/antigravity-cli/issues/1854) | `edit_file` prepends UTF-8 BOM to `.ps1` files |
| 3 | [#1855](https://github.com/google-antigravity/antigravity-cli/issues/1855) | Silent truncation of injected `SKILL.md` files |
| 4 | [#1856](https://github.com/google-antigravity/antigravity-cli/issues/1856) | Role-pinned subagent 429 ignores `Model="inherit"` |
| 5 | [#1857](https://github.com/google-antigravity/antigravity-cli/issues/1857) | Built-in `<skills>` prompt overrides delegation |
| 6 | [#1858](https://github.com/google-antigravity/antigravity-cli/issues/1858) | Native loading of subdirectory `AGENTS.md` files |
| 7 | [#1859](https://github.com/google-antigravity/antigravity-cli/issues/1859) | Hook protocol parity and `.antigravity/` config |
| 8 | [#1860](https://github.com/google-antigravity/antigravity-cli/issues/1860) | Honor `user-invocable: false` skill frontmatter |
| 9 | [#1496](https://github.com/google-antigravity/antigravity-cli/issues/1496) | Exact model ID and effort for subagents |
| 10 | [#1704](https://github.com/google-antigravity/antigravity-cli/issues/1704) | Trust subfolders inside a trusted parent folder |
| 11 | [#1121](https://github.com/google-antigravity/antigravity-cli/issues/1121) | PowerShell mandatory-parameter stdin hang |
| 12 | [#1106](https://github.com/google-antigravity/antigravity-cli/issues/1106) | Missing `Grep` tool and `rg` on Windows |

## 1. Windows Job Object kills detached child processes on `run_command` exit

- **Upstream issue(s):**
  [#1853](https://github.com/google-antigravity/antigravity-cli/issues/1853)
  (related:
  [#1187](https://github.com/google-antigravity/antigravity-cli/issues/1187),
  [#1718](https://github.com/google-antigravity/antigravity-cli/issues/1718))

- **Current workaround:**
  - `.agents/references/codex-bridge.md` (step 3): Antigravity CLI workaround
    passing `-WaitBudgetSeconds 0` with `WaitMsBeforeAsync` and awaiting async
    wakeup so the `run_command` shell stays open instead of detaching with a
    `running` receipt. (`-WaitBudgetSeconds` itself is a shared option in
    `.agents/scripts/Invoke-Codex.ps1`, where `Wait-Run` only selects a timed
    deadline vs `$null`.)
  - `.agents/skills/finalize-changes/scripts/Show-FinalizeApprovalReview.ps1`,
    `.agents/skills/finalize-changes/SKILL.md`, and
    `.agents/skills/finalize-changes/references/scripts.md`: `-HandoffOnly`
    warm-handoff check (`Get-SmartGitReceivers`) and desktop command fallback
    when cold-started SmartGit cannot survive shell exit.
  - `.agents/skills/agent-harness/references/launch.md`,
    `.agents/skills/agent-harness/references/server-monitor.md`, and
    `.agents/skills/agent-harness/references/worker.md`: desktop PowerShell
    fallback instructions when the host cannot keep harness, game, or
    ServerMonitor processes alive across `run_command` calls.

- **How to verify the upstream fix:**
  In a newer `agy` release on Windows, verify each detached launch path with its
  own post-shell check:
  1. Spawn a detached `pwsh` sleep process via `Start-Process -PassThru` in one
     `run_command` call and exit immediately, then confirm `Get-Process -Id
     <PID>` still finds it alive in a second `run_command` call.
  2. Launch both `server` and `client` roles via `Invoke-HarnessLaunch.ps1` and
     confirm each survives shell exit with a later owned ping and launched
     process identity check.
  3. Cold-start SmartGit via `Show-FinalizeApprovalReview.ps1` without
     `-HandoffOnly` and confirm the SmartGit window stays open on the review
     candidate after shell exit.
  4. Start `ServerMonitor.exe` in one `run_command` call and confirm it survives
     shell exit so the split `Capture-ServerMonitor.ps1` and stop calls succeed
     against its retained PID and start time.

- **Cleanup once verified:**
  - Remove the Antigravity-only `-WaitBudgetSeconds 0` async-wakeup branch from
    `.agents/references/codex-bridge.md`; keep `-WaitBudgetSeconds` as a shared
    option in `.agents/scripts/Invoke-Codex.ps1` unless no host uses it.
  - Remove the Antigravity desktop fallback and `-HandoffOnly` workaround from
    `Show-FinalizeApprovalReview.ps1` and `finalize-changes` docs (keep any
    guard still required by other restricted hosts).
  - Restore direct `Invoke-HarnessLaunch.ps1` and ServerMonitor launch paths in
    `.agents/skills/agent-harness/references/launch.md`, `server-monitor.md`,
    and `worker.md` where the desktop fallback existed only for `agy`.

## 2. `edit_file` silently prepends UTF-8 BOM to BOM-less `.ps1` files

- **Upstream issue(s):**
  [#1854](https://github.com/google-antigravity/antigravity-cli/issues/1854)
  (sibling:
  [#337](https://github.com/google-antigravity/antigravity-cli/issues/337))

- **Current workaround:**
  No permanent code workaround; caught retroactively by `file-hygiene` (`added
  BOM`) in `.agents/scripts/Invoke-StaticChecks.ps1` and repaired via manual BOM
  stripping and check reruns (tracked in
  `Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md` `edit_file` BOM
  rows).

- **How to verify the upstream fix:**
  Create a BOM-less UTF-8 `.ps1` file in a scratch path, edit it in place using
  `edit_file`, and inspect its first three bytes to confirm they remain the
  original ASCII/UTF-8 content rather than a UTF-8 BOM. Repeat on a `.ps1` file
  that already starts with a BOM to confirm existing BOMs are also preserved.

- **Cleanup once verified:**
  Keep the `added BOM` check in `.agents/scripts/Invoke-StaticChecks.ps1` as
  general repository hygiene, and stop anticipating manual BOM stripping after
  `edit_file` edits to `.ps1` scripts.

## 3. Silent truncation of injected `SKILL.md` files

- **Upstream issue(s):**
  [#1855](https://github.com/google-antigravity/antigravity-cli/issues/1855)

- **Current workaround:**
  - `.agents/skills/external-skill-creator/scripts/Validate-Skill.ps1`:
    `BODY002` UTF-8 byte ceiling check enforcing a conservative cap below the
    observed silent cutoff.
  - `.agents/skills/external-skill-creator/references/frontmatter-schema.md`:
    documents the `BODY002` byte ceiling and notes that the exact host budget
    and counting convention are unverified.

- **How to verify the upstream fix:**
  Invoke a test skill fixture whose `SKILL.md` body is confirmed to truncate on
  the affected `agy` release (not merely above `BODY002`, which sits below the
  observed host cutoff) in a newer `agy` release, plus a second fixture at a
  defined upper bound exceeding the largest repository `SKILL.md`. Confirm both
  arrive with trailing sections intact in the injected prompt, and confirm from
  upstream release notes or issue resolution whether `agy` removed the silent
  truncation limit altogether (passing finite fixtures alone only proves the
  tested size bound, not limit removal) or documented an exact budget with an
  explicit load-time truncation warning when exceeded.

- **Cleanup once verified:**
  - Only if upstream confirms the silent truncation limit was removed (and the
    fixture check passes), decide whether to drop `BODY002` from
    `Validate-Skill.ps1` and `frontmatter-schema.md` or retain a progressive-
    disclosure ceiling intentionally owned by the repository.
  - If `agy` documents a specific limit (or only raises it without confirming
    removal), update `BODY002` in `Validate-Skill.ps1` to stay within the
    documented contract or tested bound and update `frontmatter-schema.md`
    accordingly.

## 4. Role-pinned subagent 429 ignores `Model="inherit"`

- **Upstream issue(s):**
  [#1856](https://github.com/google-antigravity/antigravity-cli/issues/1856)
  (related:
  [#1195](https://github.com/google-antigravity/antigravity-cli/issues/1195),
  [#1846](https://github.com/google-antigravity/antigravity-cli/issues/1846))

- **Current workaround:**
  - `.agents/agents/builder.md` and `.agents/agents/locator.md`: bumped their
    frontmatter `model:` tier alias away from the rate-limited tier.
  - Operational fallback to `TypeName="self"` whenever a role-pinned subagent
    hits HTTP 429 `RESOURCE_EXHAUSTED` and `Model="inherit"` fails to override
    the frontmatter pin (recorded in
    `Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md`).

- **How to verify the upstream fix:**
  Configure a custom subagent in `.agents/agents/` with a pinned `model:` that
  differs from the parent session's active model, dispatch it via
  `invoke_subagent` with explicit `Model="inherit"`, and confirm from the
  subagent's session metadata that it ran on the parent session's model rather
  than the frontmatter pin. Also verify when the role's pinned tier is returning
  HTTP 429 `RESOURCE_EXHAUSTED` while the parent tier is available that
  re-dispatching the role with `Model="inherit"` succeeds on the parent tier
  without falling back to `TypeName="self"`.

- **Cleanup once verified:**
  - Stop falling back to `TypeName="self"` on role-pinned 429 errors; recover by
    passing `Model="inherit"` while keeping the role's prompt and tool policy.
  - Re-evaluate the `model:` setting in `.agents/agents/builder.md` and
    `.agents/agents/locator.md` (alongside item 9 if exact model selection is
    also available).

## 5. Built-in `<skills>` system prompt overrides delegated skills

- **Upstream issue(s):**
  [#1857](https://github.com/google-antigravity/antigravity-cli/issues/1857)

- **Current workaround:**
  - `AGENTS.md` (`## Environment`): explicit `IMPORTANT:` bullet instructing the
    main session to ignore Antigravity CLI's `<skills>` rule for delegated
    skills (subagent and Codex bridge) and instead follow
    `.agents/references/subagent-reporting.md` `## Task brief`.

- **How to verify the upstream fix:**
  In a newer `agy` release, inspect the injected `<skills>` system prompt block
  (or verify the new opt-out config/frontmatter setting) and run a multi-step
  delegated workflow without the `AGENTS.md` override to confirm the main
  session passes skill paths to native subagents and Codex bridge workers
  without reading full delegated `SKILL.md` files into main context.

- **Cleanup once verified:**
  Remove the Antigravity `<skills>` override bullet from `AGENTS.md`
  (`## Environment`), and enable any upstream configuration or frontmatter
  setting if the fix is opt-in.

## 6. Native automatic loading of ancestor/subdirectory `AGENTS.md` files

- **Upstream issue(s):**
  [#1858](https://github.com/google-antigravity/antigravity-cli/issues/1858)
  (related:
  [#808](https://github.com/google-antigravity/antigravity-cli/issues/808),
  [#1596](https://github.com/google-antigravity/antigravity-cli/issues/1596))

- **Current workaround:**
  - `.agents/hooks.json` (`PostInvocation` entry) invokes
    `.agents/scripts/Invoke-SubdirectoryAgentsHook.ps1`.
  - `.agents/scripts/Invoke-SubdirectoryAgentsHook.ps1`
    (`Invoke-AgyPostInvocation`, `Get-SafeAgentsCandidate`,
    `Get-DirectReadShellPlan`, `Get-ShellAccessedTargets`) together with
    `.agents/scripts/AgentShellLexer.psm1` scans the `agy` transcript on every
    turn to inject unseen ancestor `AGENTS.md` files.
  - Note: `.agents/scripts/Invoke-SubdirectoryAgentsHook.ps1` is also wired for
    Codex CLI `PostToolUse` in `.codex/config.toml`
    (`Invoke-CodexPostToolUse`).

- **How to verify the upstream fix:**
  Disable the active Antigravity subdirectory-`AGENTS.md` hook entry
  (`PostInvocation`, or `PostToolUse` if migrated under item 7) locally in a
  newer `agy` release, read or edit a file inside a nested directory with
  ancestor `AGENTS.md` files, and confirm `agy` injects each unseen ancestor
  `AGENTS.md` automatically at most once per session while skipping paths
  outside the workspace, `Temp/`, and reparse points, and without
  over-injecting on metadata-only shell commands.

- **Cleanup once verified:**
  - Remove the active Antigravity subdirectory-`AGENTS.md` hook entry
    (`PostInvocation` in `.agents/hooks.json`, or the migrated `PostToolUse`
    entry in `.agents/hooks.json` or `.antigravity/hooks.json` if item 7
    landed first).
  - Remove `Invoke-AgyPostInvocation` and Antigravity-only transcript/marker
    helpers (or Antigravity-only `PostToolUse` handling if migrated under item
    7) from `.agents/scripts/Invoke-SubdirectoryAgentsHook.ps1`.
  - Once Codex CLI also gains native subdirectory `AGENTS.md` loading, remove
    its `PostToolUse` hook registration from `.codex/config.toml` before
    deleting `.agents/scripts/Invoke-SubdirectoryAgentsHook.ps1`.
  - Keep `.agents/scripts/AgentShellLexer.psm1` while
    `.agents/scripts/Invoke-PreToolUseHook.ps1` (or any other script) still
    imports it; delete it only when no remaining script imports it.

## 7. Hook protocol parity and `.antigravity/` config directory support

- **Upstream issue(s):**
  [#1859](https://github.com/google-antigravity/antigravity-cli/issues/1859)
  (related:
  [#808](https://github.com/google-antigravity/antigravity-cli/issues/808),
  [#1358](https://github.com/google-antigravity/antigravity-cli/issues/1358))

- **Current workaround:**
  - `.agents/hooks.json` passes `-InjectStepsJson` on all `SessionStart` hooks
    to `.agents/scripts/Write-SessionWorkflowContext.ps1`.
  - `.agents/scripts/Invoke-PreToolUseHook.ps1` carries a dedicated `toolCall`
    input branch and stdout JSON `decision: deny` exit-0 path alongside the
    `tool_name` / exit-2 stderr path.
  - `.agents/hooks.json` registers `PostInvocation` and
    `.agents/scripts/Invoke-SubdirectoryAgentsHook.ps1` branches on
    `PostInvocation` vs `PostToolUse`.
  - Antigravity-specific `hooks.json` and `agents/*.md` remain under `.agents/`
    instead of `.antigravity/` alongside `.antigravity/agy-worktree.ps1`.

- **How to verify the upstream fix:**
  Verify each supported capability in a newer `agy` release:
  1. `SessionStart` injects plain-text stdout from
     `Write-SessionWorkflowContext.ps1` without `-InjectStepsJson`.
  2. `PreToolUse` blocks a disallowed `run_command` when the hook writes the
     denial reason to stderr and exits with code 2 (and check whether standard
     `tool_name`/`tool_input` fields are provided).
  3. `PostToolUse` (if still needed pending item 6) fires with Antigravity's
     actual file and shell tool events and injects unseen ancestor `AGENTS.md`
     files end-to-end through the `hookSpecificOutput.additionalContext`
     envelope.
  4. Moving `hooks.json` and `agents/*.md` into `.antigravity/` is discovered
     automatically by `agy`.

- **Cleanup once verified:**
  - Remove `-InjectStepsJson` from `hooks.json` and from
    `.agents/scripts/Write-SessionWorkflowContext.ps1`.
  - Collapse the stdout-JSON deny path in
    `.agents/scripts/Invoke-PreToolUseHook.ps1` to the shared stderr/exit-2 deny
    protocol while preserving command validation for Antigravity's tool name
    (`run_command`) and command payload (`toolCall.args.CommandLine`, or
    `tool_input` if aliased) — do not collapse into the `@('Bash',
    'PowerShell')` branch unless `agy` matches that tool name and
    `tool_input.command` shape.
  - If `PostToolUse` protocol parity lands before item 6 (native subdirectory
    `AGENTS.md` loading), migrate the `PostInvocation` entry in `hooks.json` to
    `PostToolUse` and replace `Invoke-AgyPostInvocation` in
    `.agents/scripts/Invoke-SubdirectoryAgentsHook.ps1` with a `PostToolUse`
    `hookSpecificOutput.additionalContext` handler that accepts Antigravity's
    actual tool names and input payloads (do not route Antigravity events
    unchanged into `Invoke-CodexPostToolUse`'s `@('Bash', 'apply_patch')`
    filter), verified end-to-end on Antigravity file and shell actions; once
    item 6 lands, remove the Antigravity hook entry and handler per item 6.
  - Relocate `.agents/hooks.json` to `.antigravity/hooks.json` and
    `.agents/agents/*.md` to `.antigravity/agents/*.md`, updating path
    references in `.agents/references/change-workflow.md`,
    `.agents/scripts/Invoke-StaticChecks.ps1`, and related scripts/docs.

## 8. Honor `user-invocable: false` skill frontmatter

- **Upstream issue(s):**
  [#1860](https://github.com/google-antigravity/antigravity-cli/issues/1860)
  (related:
  [#1817](https://github.com/google-antigravity/antigravity-cli/issues/1817))

- **Current workaround:**
  - Relocated internal workflow helper `codex-bridge` out of `.agents/skills/`
    into `.agents/references/codex-bridge.md` and
    `.agents/scripts/Invoke-Codex.ps1` so it does not pollute Antigravity's `/`
    slash-command list.
  - Documented the Antigravity `user-invocable: false` gap in
    `.agents/skills/external-skill-creator/references/client-compatibility.md`.

- **How to verify the upstream fix:**
  Add `user-invocable: false` to a test skill under `.agents/skills/`, launch a
  newer `agy` release, and confirm the skill is absent from the `/`
  slash-command list while remaining present in the model's skill catalog.

- **Cleanup once verified:**
  - Update the Antigravity CLI entry in
    `.agents/skills/external-skill-creator/references/client-compatibility.md`
    to record verified `user-invocable: false` support.
  - Keep `.agents/references/codex-bridge.md` in place unless there is a new
    reason to make it a model-catalog skill; future model-only skills can stay
    in `.agents/skills/` with `user-invocable: false` instead of being moved
    out.

## 9. Exact model ID and reasoning-effort selection for subagents

- **Upstream issue(s):**
  [#1496](https://github.com/google-antigravity/antigravity-cli/issues/1496)
  (related:
  [#1237](https://github.com/google-antigravity/antigravity-cli/issues/1237),
  [#1150](https://github.com/google-antigravity/antigravity-cli/issues/1150),
  [#1846](https://github.com/google-antigravity/antigravity-cli/issues/1846))

- **Current workaround:**
  - `.agents/agents/*.md` (`planner`, `reviewer`, `implementer`, `researcher`,
    `locator`, `builder`, `runner`, `mechanic`) use coarse tier aliases and omit
    reasoning-effort settings, whereas `.claude/agents/*.md` and
    `.codex/agents/*.toml` pin exact models and explicit reasoning effort per
    role.

- **How to verify the upstream fix:**
  Set an exact model ID and an explicit reasoning-effort field in a test role
  definition under `.agents/agents/` (or `.antigravity/agents/` after item 7),
  invoke that role via `invoke_subagent` in a newer `agy` release, and verify
  from the subagent session metadata that both the requested model and effort
  level were applied.

- **Cleanup once verified:**
  Update the eight role files in `.agents/agents/*.md` (or
  `.antigravity/agents/*.md`) to pin explicit per-role model IDs and reasoning
  effort in parity with `.claude/agents/*.md` and `.codex/agents/*.toml`.

## 10. Trust subfolders inside a trusted parent folder (worktrees)

- **Upstream issue(s):**
  [#1704](https://github.com/google-antigravity/antigravity-cli/issues/1704)

- **Current workaround:**
  - `.agents/scripts/Start-AgentWorktreeSession.ps1` injects
    `--settings '{"trustedWorkspaces":["<worktree>"]}'` on every `agy` launch
    because trusting the worktree parent directory does not inherit to fresh
    per-session UUID worktree subdirectories.

- **How to verify the upstream fix:**
  Configure the worktree parent folder as a recursive/prefix trusted workspace
  in `agy` user settings, launch `agy` inside a newly created worktree without
  passing `--settings`, and confirm `agy` starts with no workspace-trust prompt
  and without writing per-UUID entries to persistent settings.

- **Cleanup once verified:**
  Remove the `if ($Client -ceq 'agy')` `--settings` `trustedWorkspaces`
  injection from `.agents/scripts/Start-AgentWorktreeSession.ps1` and document
  the one-time parent-folder trust setup in `Documents/FreshMachineSetup.md` if
  needed.

## 11. PowerShell mandatory-parameter stdin hang in `run_command`

- **Upstream issue(s):**
  [#1121](https://github.com/google-antigravity/antigravity-cli/issues/1121)
  (related:
  [#1754](https://github.com/google-antigravity/antigravity-cli/issues/1754))

- **Current workaround:**
  - `AGENTS.md` (`## Directives`, bundled scripts bullet): mandates
    `-NonInteractive` on bundled-script `pwsh` invocations so omitted mandatory
    parameters fail fast instead of blocking stdin.
  - `.agents/hooks.json`: passes `-NonInteractive` on the `PostInvocation` hook
    `pwsh` command.
  - `.agents/scripts/AgentTranscriptCommon.psm1`
    (`Resolve-AgentAgyParentSession`): defaults `$SessionId` from
    `$env:ANTIGRAVITY_CONVERSATION_ID` and validates with an explicit `throw`
    rather than `[Parameter(Mandatory)]`.

- **How to verify the upstream fix:**
  Run a scratch `.ps1` script declaring `[Parameter(Mandatory)] [string] $Name`
  through `run_command` as `pwsh -NoProfile -File <script>` without
  `-NonInteractive` and without `-Name` in a newer `agy` release, and confirm
  the command exits promptly with a non-zero error instead of hanging on stdin.

- **Cleanup once verified:**
  Re-evaluate whether `-NonInteractive` in `AGENTS.md` and `.agents/hooks.json`
  (`PostInvocation`) is still desirable as cross-host defense (for Claude Code
  and Codex shells) or can be relaxed, and lift the informal avoidance of
  `[Parameter(Mandatory)]` in new Antigravity-only helpers while preserving the
  `$env:ANTIGRAVITY_CONVERSATION_ID` default on `$SessionId` in
  `.agents/scripts/AgentTranscriptCommon.psm1`
  (`Resolve-AgentAgyParentSession`).

## 12. Missing `Grep` tool and `rg` on Windows

- **Upstream issue(s):**
  [#1106](https://github.com/google-antigravity/antigravity-cli/issues/1106)
  (related:
  [#1525](https://github.com/google-antigravity/antigravity-cli/issues/1525))

- **Current workaround:**
  - `AGENTS.md` (`## Environment`): Antigravity-specific `Select-String
    -LiteralPath '<path>' -Pattern '<regex>'` quoting rule noting that
    Antigravity has no `Grep` tool or `rg` on Windows.
  - `.agents/agents/locator.md` (and other role files in `.agents/agents/*.md`):
    tool allowlists omit any dedicated grep/search tool, forcing all code search
    through `run_command`.

- **How to verify the upstream fix:**
  In a newer `agy` release on Windows, check separately whether (1) a native
  grep/code-search tool is available and works cleanly in both the main session
  and delegated `locator` subagents, and/or (2) `rg` is bundled on `PATH` for
  `run_command`.

- **Cleanup once verified:**
  - Remove or narrow the Antigravity-only `Select-String` fallback clause in
    `AGENTS.md` (`## Environment`) according to which capability landed (native
    search tool, bundled `rg`, or both).
  - Only if `agy` provides a native grep/search tool, add it to the `tools:`
    allowlist in `.agents/agents/locator.md` and the other role files under
    `.agents/agents/*.md` (or `.antigravity/agents/*.md` after item 7).
