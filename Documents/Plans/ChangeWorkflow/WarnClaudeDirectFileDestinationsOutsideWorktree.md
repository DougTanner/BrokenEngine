<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-21T22:36:10.698Z","dependsOn":[]} -->
# Warn on Claude direct-file destinations outside the session worktree

## Context

At repository baseline `ab7d62357801233fd32e80a0af2c336ecf89aad5`, `.claude/settings.json:11-30` registers only `SessionStart` hooks, so a Claude `Edit` or `Write` aimed at the primary checkout, a sibling worktree, or another outside path receives no repository-configured destination feedback. The wrapper still starts both new and reattached Claude sessions through `Start-AgentWorktreeSession.ps1` (`.claude/claude-worktree.sh:42-43`), which launches the client with the resolved session worktree as its working directory (`.agents/scripts/Start-AgentWorktreeSession.ps1:189-196`). This leaves a narrow feedback gap at the direct-file tool boundary without weakening the established startup identity.

The idea was adapted from `justi/claude-code-project-boundary` commit `07c5250ec55434c97d3fed48ed29da193c7663a5`, specifically its pinned [hook registrations](https://github.com/justi/claude-code-project-boundary/blob/07c5250ec55434c97d3fed48ed29da193c7663a5/hooks/hooks.json#L15-L55) and [direct-file branch](https://github.com/justi/claude-code-project-boundary/blob/07c5250ec55434c97d3fed48ed29da193c7663a5/hooks/guard.sh#L232-L380). The upstream project provides broader blocking and physical-path checks. This Plan adopts only an advisory for the structured destination already supplied to Claude's direct-file tools and uses original repository code and wording.

The current official Claude [Hooks reference](https://code.claude.com/docs/en/hooks) documents `Edit` and `Write` as `PreToolUse` tools and the exact matcher form `Edit|Write` ([event](https://code.claude.com/docs/en/hooks#pretooluse), [matcher patterns](https://code.claude.com/docs/en/hooks#matcher-patterns)). It also documents absolute `tool_input.file_path` values for those tools ([input](https://code.claude.com/docs/en/hooks#pretooluse-input)), `${CLAUDE_PROJECT_DIR}` as the fixed project root where the session started ([script paths](https://code.claude.com/docs/en/hooks#reference-scripts-by-path)), and `hookSpecificOutput.additionalContext` separately from permission decisions ([context](https://code.claude.com/docs/en/hooks#add-context-for-claude), [decision control](https://code.claude.com/docs/en/hooks#pretooluse-decision-control)). Runtime delivery by the installed Claude host remains future acceptance evidence rather than a fact established by this Plan.

## Design

The author's recommendation is to register one `PreToolUse` command hook in `.claude/settings.json` with the exact matcher `Edit|Write`. Use exec-form fields for `pwsh` with arguments `-NoProfile`, `-File`, and `${CLAUDE_PROJECT_DIR}/.agents/scripts/Write-DirectFileBoundaryWarning.ps1`. This keeps the structured hook narrow and avoids command parsing or a wrapper change.

Add `Write-DirectFileBoundaryWarning.ps1` as a small stdin-to-JSON adapter. It should import `AgentScriptCommon.psm1`, accept only the `PreToolUse` event, `Edit` or `Write`, and a nonempty absolute `tool_input.file_path`, and take the boundary from `$env:CLAUDE_PROJECT_DIR`. Reuse `Get-AgentCanonicalPath` (`.agents/scripts/AgentScriptCommon.psm1:7-12`) for lexical canonicalization. Classify the destination as inside when it equals the root or starts with the root plus one directory separator under `OrdinalIgnoreCase`, matching the repository's existing separator-aware containment pattern in `.agents/scripts/New-PlanFile.ps1:64-66`.

For an inside destination, the script should produce no output and exit 0. For an outside destination, it should emit one compact JSON object containing only `hookSpecificOutput.hookEventName = "PreToolUse"` and concise `additionalContext` naming the destination and session root, then exit 0. Missing or malformed input or root should emit the same shape with a concise nonblocking diagnostic and exit 0. The script should never emit `permissionDecision`, `permissionDecisionReason`, a top-level `decision`, or blocking stderr output.

The check should remain lexical and advisory. It should not touch the filesystem, resolve links, inspect contents, run Git, parse shell commands, or consult the event working directory. The rationale belongs next to the script's containment logic; no new workflow instruction or skill is needed.

## Critical files

- `.claude/settings.json` — register the single Claude `PreToolUse` matcher and command hook while preserving the existing `SessionStart` hooks.
- `.agents/scripts/Write-DirectFileBoundaryWarning.ps1` — new direct-file event adapter and lexical worktree-boundary advisory.
- `.agents/scripts/AgentScriptCommon.psm1:7-12` — existing canonical-path helper to reuse without changing it.
- `.agents/scripts/New-PlanFile.ps1:64-66` — existing separator-aware, ordinal-ignore-case containment pattern to mirror without changing it.

## In scope

- Add one `.claude/settings.json` `PreToolUse` matcher group whose matcher is exactly `Edit|Write` and whose sole command invokes `.agents/scripts/Write-DirectFileBoundaryWarning.ps1` through `pwsh -NoProfile -File` using `${CLAUDE_PROJECT_DIR}`.
- Add `.agents/scripts/Write-DirectFileBoundaryWarning.ps1` to validate the structured direct-file event, lexically canonicalize the project root and absolute destination, and classify root equality or a separator-bounded descendant as inside.
- Emit only nonblocking `hookSpecificOutput.additionalContext` for an outside destination or malformed/missing required input, and remain silent for an inside destination.
- Add only the local script comment needed to explain why this check is lexical advisory feedback rather than a filesystem security boundary.

## Out of scope

- `MultiEdit`, Bash or PowerShell command parsing, relative-target or event-`cwd` handling, and wrapper changes.
- Symlink, junction, hard-link, final-link, or nearest-existing-ancestor resolution; filesystem inspection; Git calls; network-path policy; allowlists; telemetry; or persistent state.
- Permission, approval, blocking, rerouting, landing, scheduler, or trust-policy changes, including any `permissionDecision`, `permissionDecisionReason`, top-level `decision`, or stderr-based denial.
- Promises about Codex, OpenCode, malicious inputs, races, commands that write files, or containment beyond Claude's structured `Edit` and `Write` destination.
- New skills, workflow stages, unit tests, or compile targets.

## Risk triggers and invariants

Change Workflow Tier 2: this changes one client's scoped tool behavior without changing a trust boundary, permission decision, scheduler, build/bootstrap path, protocol, serialization, threading, or deterministic runtime surface. The advisory must never block or alter the tool's normal permission flow. Preserve the existing `SessionStart` hooks, wrapper startup behavior, explicit outside-work authority, and every landing and review gate.

## Acceptance criteria

- Parse `.claude/settings.json` and prove the new matcher is exactly `Edit|Write`, invokes the intended script through `pwsh -NoProfile -File`, and preserves all existing `SessionStart` hooks.
- Feed representative absolute hook JSON directly to the script. A destination equal to or beneath the session root, including a new nested destination, is silent with exit 0.
- A sibling-prefix destination, the primary checkout, and another session worktree each produce exactly one valid JSON object with `hookSpecificOutput.hookEventName = "PreToolUse"` and one concise `additionalContext`, then exit 0.
- Missing or malformed root, event, tool, or path produces exactly one valid diagnostic object in the same advisory shape and exits 0.
- Direct fixtures containing `.` and `..` prove lexical normalization. No link or junction acceptance is added.
- Every emitted object omits `permissionDecision`, `permissionDecisionReason`, top-level `decision`, and other blocking output.
- In one wrapper-started Claude acceptance session, an inside `Edit` and `Write` remain quiet, and an outside `Edit` or `Write` proceeds while placing the advisory in Claude's next model context. This actual-host check establishes installed-host delivery.
- No compile target or unit test is required. Run the repository's applicable static script check and the review routes required for changed PowerShell and JSON artifacts.

## Coordination

No dependency or reciprocal coordination constraint. The other live ChangeWorkflow Plans do not change `.claude/settings.json`, this hook script, or its reused path helpers, so this Plan is independently implementable and landable.

## Notes

Duplicate search covered all live `Documents/Plans` plus `.agents`, `.claude`, and `.codex` for the target paths and the terms `PreToolUse`, `Edit|Write`, direct-file boundary, wrong/cross worktree, `CLAUDE_PROJECT_DIR`, and advisory context. Existing wrapper, scope-review, and landing controls operate at different boundaries; no executable Plan owns this root cause and implementation boundary.
