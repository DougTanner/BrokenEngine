<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-08T13:08:30.849Z","dependsOn":[]} -->
# Warn on Claude direct-file destinations outside the session worktree

## Context

At repository baseline `2c881f45c057941cea51a62adbb50a9ac615b355`, `.claude/settings.json:11-30` registers only the three `SessionStart` hooks, and no `.claude/hooks/` directory or other Claude hook exists in the repository (the `.codex/config.toml` `SessionStart` hooks are Codex-only). A Claude `Edit` or `Write` aimed at the primary checkout, a sibling session worktree, or another outside path therefore receives no repository-configured destination feedback. The wrapper starts every Claude session with `--dangerously-skip-permissions` (`.claude/claude-worktree.sh:5`) through `Start-AgentWorktreeSession.ps1` (`.claude/claude-worktree.sh:42-43`), which launches the client with the resolved session worktree as its working directory (`.agents/scripts/Start-AgentWorktreeSession.ps1:196`), so no permission prompt surfaces the destination either. Root `AGENTS.md` `## IMPORTANT: Session rules` requires work in the session's own worktree; nothing at the direct-file tool boundary reports a departure from it.

The idea is adapted from `justi/claude-code-project-boundary` commit `07c5250ec55434c97d3fed48ed29da193c7663a5` ([tree](https://github.com/justi/claude-code-project-boundary/tree/07c5250ec55434c97d3fed48ed29da193c7663a5)). The upstream project provides broader blocking and physical-path checks. This Plan adopts only an advisory for the structured destination already supplied to Claude's direct-file tools, written in original repository code and wording.

The official Claude [Hooks reference](https://code.claude.com/docs/en/hooks) documents:

- `PreToolUse` matching by tool name, where `Edit|Write` is an exact-name list rather than a regular expression ([matcher patterns](https://code.claude.com/docs/en/hooks#matcher-patterns)).
- `tool_input.file_path` as always absolute for `Write` and `Edit` ([PreToolUse input](https://code.claude.com/docs/en/hooks#pretooluse-input)).
- `${CLAUDE_PROJECT_DIR}` as the project root where the session started, which stays fixed even when the event `cwd` moves ([script paths](https://code.claude.com/docs/en/hooks#reference-scripts-by-path)), exported as the `CLAUDE_PROJECT_DIR` environment variable on the hook process in both exec and shell form ([exec form and shell form](https://code.claude.com/docs/en/hooks#exec-form-and-shell-form)).
- Exec form: when `args` is present, `command` is spawned directly with no shell, and on Windows `command` must resolve to a real executable ([exec form and shell form](https://code.claude.com/docs/en/hooks#exec-form-and-shell-form)).
- `hookSpecificOutput.additionalContext` for `PreToolUse`, added to Claude's context alongside the tool result and ignored only when `permissionDecision` is `"defer"` ([decision control](https://code.claude.com/docs/en/hooks#pretooluse-decision-control), [add context](https://code.claude.com/docs/en/hooks#add-context-for-claude)).
- Exit 0 with stdout that starts with `{` and ends with `}` parsed as JSON; exit 2 is the blocking exit code.

The reference does not state what happens when `permissionDecision` is omitted, and states placeholder substitution inside `args` only for path placeholders in general. Both stay acceptance checks against the installed host rather than facts this Plan establishes.

## Design

The author's recommendation is to add one `PreToolUse` matcher group to `.claude/settings.json` with the exact matcher `Edit|Write` and one exec-form command hook: `command` `pwsh`, `args` `-NoProfile`, `-File`, and `${CLAUDE_PROJECT_DIR}/.agents/scripts/Write-DirectFileBoundaryWarning.ps1`. The existing `SessionStart` hooks use a relative script path in shell form because they run at startup, when the working directory is the project root; a `PreToolUse` hook runs mid-session from the current directory, so the documented placeholder is the stable script location.

Add `.agents/scripts/Write-DirectFileBoundaryWarning.ps1` as a small stdin-to-JSON adapter. It imports `AgentScriptCommon.psm1`, accepts only `hook_event_name` `PreToolUse`, `tool_name` `Edit` or `Write`, and a nonempty absolute `tool_input.file_path`, and takes the boundary from `$env:CLAUDE_PROJECT_DIR`. It reuses `Get-AgentCanonicalPath` (`.agents/scripts/AgentScriptCommon.psm1:7-12`) for lexical canonicalization of both paths, and classifies the destination as inside when it equals the root or starts with the root plus one directory separator under `OrdinalIgnoreCase`, the separator-aware containment pattern of `Test-PlanFileContained` (`.agents/scripts/New-PlanFile.ps1:64-67`).

For an inside destination the script writes nothing and exits 0. For an outside destination it writes one compact JSON object containing only `hookSpecificOutput.hookEventName = "PreToolUse"` and a concise `additionalContext` naming the destination and the session root, then exits 0. Missing or malformed input or root yields the same shape with a concise nonblocking diagnostic and exit 0. The script never emits `permissionDecision`, `permissionDecisionReason`, `updatedInput`, a top-level `decision`, or stderr, and never exits 2.

The check stays lexical and advisory: no filesystem access, link resolution, content inspection, Git call, shell-command parsing, or use of the event `cwd`. Its rationale belongs in one local comment beside the containment test; no workflow instruction or skill changes.

## Critical files

- `.claude/settings.json` — add the single `PreToolUse` matcher group; preserve the existing `worktree`, `env`, `permissions`, and `SessionStart` entries.
- `.agents/scripts/Write-DirectFileBoundaryWarning.ps1` — new direct-file hook adapter and lexical worktree-boundary advisory.
- `.agents/scripts/AgentScriptCommon.psm1:7-12` — `Get-AgentCanonicalPath`, reused unchanged.
- `.agents/scripts/New-PlanFile.ps1:64-67` — `Test-PlanFileContained`, the containment pattern mirrored unchanged.

## In scope

- One `.claude/settings.json` `hooks.PreToolUse` matcher group whose matcher is exactly `Edit|Write` and whose sole hook is the exec-form `pwsh -NoProfile -File ${CLAUDE_PROJECT_DIR}/.agents/scripts/Write-DirectFileBoundaryWarning.ps1` command.
- New `.agents/scripts/Write-DirectFileBoundaryWarning.ps1`: read the hook JSON from stdin, validate event, tool, and absolute `tool_input.file_path`, canonicalize root and destination with `Get-AgentCanonicalPath`, classify root equality or a separator-bounded descendant as inside, and emit the advisory or diagnostic object described in `## Design`.
- One local comment in that script explaining why the check is lexical advisory feedback rather than a filesystem security boundary.

## Out of scope

- Any edit to `Get-AgentCanonicalPath`, `Test-PlanFileContained`, `New-PlanFile.ps1`, the wrapper (`.claude/claude-worktree.sh`, `Start-AgentWorktreeSession.ps1`), or the existing `SessionStart` hooks.
- Tools other than `Edit` and `Write`; Bash or PowerShell command parsing; relative-target or event-`cwd` handling; sessions that move into another worktree mid-session.
- Symlink, junction, hard-link, or nearest-existing-ancestor resolution; filesystem inspection; Git calls; network-path policy; allowlists; telemetry; persistent state.
- Any permission, approval, blocking, rerouting, landing, scheduler, or trust-policy change, including `permissionDecision`, `permissionDecisionReason`, `updatedInput`, top-level `decision`, exit code 2, or stderr-based denial.
- Codex or OpenCode equivalents; new skills, workflow steps, unit tests, or compile targets.

## Risk triggers and invariants

Change Workflow Tier 2, trigger: one client's scoped tool behavior (a new advisory Claude hook and its script), with no change to determinism/CRC, wire, serialization, save/replay, threading, trust boundary, permission decision, or build/bootstrap coordination. Invariants: the hook never blocks or alters Claude's permission flow; an inside destination stays silent; the existing `SessionStart` hooks, wrapper startup, explicit user authority for outside work, and every landing and review gate are unchanged.

## Acceptance criteria

- Parsing `.claude/settings.json` shows exactly one new `PreToolUse` group with matcher `Edit|Write`, `command` `pwsh`, and `args` `-NoProfile`, `-File`, `${CLAUDE_PROJECT_DIR}/.agents/scripts/Write-DirectFileBoundaryWarning.ps1`, with every pre-existing key and `SessionStart` hook unchanged.
- Feeding representative hook JSON directly to the script with `CLAUDE_PROJECT_DIR` set: a destination equal to or beneath the root, including a not-yet-existing nested path, writes nothing and exits 0.
- A sibling-prefix destination (root name plus a suffix), the primary checkout, and another session worktree each produce exactly one valid JSON object with `hookSpecificOutput.hookEventName = "PreToolUse"` and one concise `additionalContext`, then exit 0.
- Missing or malformed root, event, tool, or path produces exactly one valid diagnostic object in the same shape and exits 0.
- Fixtures containing `.` and `..` segments classify by their lexically normalized path.
- No emitted object contains `permissionDecision`, `permissionDecisionReason`, `updatedInput`, or a top-level `decision`, and stderr stays empty.
- In one wrapper-started Claude session: an inside `Edit` and `Write` produce no advisory; an outside `Edit` or `Write` proceeds without any permission prompt or denial and Claude's context shows the advisory alongside the tool result. This installed-host check establishes `${CLAUDE_PROJECT_DIR}` substitution in `args`, nonblocking behavior with `permissionDecision` omitted, and context delivery.
- No compile target or unit test. Run the review routes the Change Workflow requires for changed PowerShell and JSON artifacts.

## Coordination

No dependency. At baseline no other live Plan names `.claude/settings.json`, this hook script, or the reused path helpers.

## Notes

Duplicate search covered every live `Documents/Plans` file and `.agents`, `.claude`, `.codex`, and `.opencode` for `PreToolUse`, `Edit|Write`, `CLAUDE_PROJECT_DIR`, `additionalContext`, and worktree-boundary terms; the only matches are an unrelated `${CLAUDE_PROJECT_DIR}` mention in `.agents/skills/external-skill-creator/references/client-compatibility.md:33` and Codex `SessionStart` `additionalContextLimit` settings in `.codex/config.toml`. Each `Edit` and `Write` pays one `pwsh -NoProfile` startup for this hook.
