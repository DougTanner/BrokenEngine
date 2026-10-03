---
name: codex-review
description: >-
  Retained but inactive route that runs one delegated reviewer, auditor, or
  researcher role on Codex headless using the selected Sol or Opus role
  configuration. Not part of the Change Workflow. Use only when the user
  explicitly asks to run a named review, audit, or /plan-alternatives
  researcher on Codex. Codex callers never invoke it.
disable-model-invocation: true
allowed-tools: [Read, Bash, Agent, Skill]
---

# Codex Review

## Purpose

Runs one delegated reviewer, auditor, or `/plan-alternatives` researcher on
Codex headless with the selected Sol or Opus role configuration and returns its
handoff without the task evidence entering this session.

## When to use

Only when the user explicitly asks, in the current session, to run a named
review or audit skill or a `/plan-alternatives` researcher on Codex. The Change
Workflow never routes here: the [change-workflow.md](../../references/change-workflow.md) role table
runs its normal delegated subagents, and this package is retained so the route
can be re-enabled later.

A `/codex-review` invocation of an assigned review skill constitutes the
delegated-`reviewer` execution context; it is not an "inline run" in the
assigned skills' vocabulary. Codex callers never invoke this skill.

## Inputs

- Assigned skill — `plan-alternatives`, or the reviewer or auditor role to run,
  such as `plan-audit`, `repo-code-review`, or `session-audit`
- Its normal inputs: plan/intent, changed files and regions, and current
  residuals or reviewer focus
- For `/plan-alternatives`, the assigned axis's complete shared task brief
  instead of review scope
- Repository root — the absolute toplevel of the session worktree, defaulting to
  the current one; a relative path is accepted and resolves against the current
  directory — and session baseline (a full 40-character commit SHA)
- `-Agent` — optional `sol` or `opus`, default `sol`; use `opus` for an
  explicitly requested `/plan-alternatives` researcher. The selection reads
  the model and effort pins from the matching repository role configuration.

`## Steps` owns prompt preparation and dispatch mechanics for both routes.

## Steps

Steps 1-8 assemble reviewer and auditor prompts only. For an explicitly
requested `/plan-alternatives` researcher dispatch, write the complete per-axis
shared task brief required by `../plan-alternatives/SKILL.md` directly to a
new repo-relative file under `Temp/`.

For that dispatch, do not run the review prompt builder or add its reviewer
guardrails. The brief must instruct the researcher to return the normal axis
handoff and then append a standalone final `PASS` line for the dispatch
wrapper. Use that file as `<promptPath>`, then continue at step 9 with
`-Agent opus`.

1. Write the judgment content yourself into `-ScopeFile`: the exact scope, the
   files and regions authorized for review, focus notes, and current residuals.
   The script copies that text verbatim and never authors, summarizes, or edits
   it, and never decides which files are in scope.

   Relay a plan's agent-made decisions there as reviewable claims, never as
   settled constraints;
   [`../../references/authority-order.md`](../../references/authority-order.md)
   owns which decisions bind.

   Done when `-ScopeFile` states the scope, the authorized files and regions,
   the focus notes, and the current residuals.
2. When a verification dispatch needs a mechanical tool check the read-only
   sandbox cannot run, run only that non-judgment check host-side first and put
   its verbatim result and identity binding in `-ScopeFile` for the reviewer to
   validate, leaving every evaluation of that result to the reviewer alone.

   Done when that check's verbatim result and identity binding are in
   `-ScopeFile`, or no such check applies.
3. Include the assigned skill's own required evidence in that same file before
   dispatching: `plan-audit`'s draft execution card (`../plan-audit/SKILL.md`).
   The script blocks the dispatch when that card is absent or leaves a field
   unfilled.

   Copy the card's fields into `-ScopeFile` even when a plan file or snapshot
   named there also carries them: that inline copy is the one the script judges.

   Done when that evidence is in `-ScopeFile`.
4. For a reviewer role with no skill file, pass a descriptive role name as
   `-AssignedSkill` together with `-AdHocRole`, and put that role's full review
   contract in `-ScopeFile`.

   Done when that role name and its full review contract are in place, or the
   assigned skill has a skill file.
5. A file the reviewer only needs to read — a plan snapshot for `plan-audit` or
   `plan-simplicity-review`, for one — is not change evidence: keep it under
   `Temp/` (gitignored, so never named) and give its repo-relative path in
   `-ScopeFile`.

   The reviewer reads it from the worktree like any other file, because the
   read-only Codex run is rooted at the worktree.

   Done when every such file sits under `Temp/` with its repo-relative path
   named in `-ScopeFile`.
6. Assemble the prompt with
   [scripts/New-CodexReviewPrompt.ps1](scripts/New-CodexReviewPrompt.ps1), which
   writes the prompt file and returns only a small receipt, so the diff never
   enters this session.

   The manager runs the script before dispatch; the reviewer, inside the Codex
   `--sandbox read-only` environment, only reads the prompt file it wrote. From
   the session worktree root:

   ```powershell
   pwsh -NoProfile -File .agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1 -RepositoryRoot '<absolute repository toplevel>' -Baseline <full 40-character baseline SHA> -AssignedSkill <assigned skill> -ScopeFile <scope file> -PromptPath <new prompt path> [-RiskTier <1|2|3>] [-UntrackedPath <comma-separated paths>] [-Head <rev>] [-AdHocRole]
   ```

   `-RiskTier` adds one `Risk tier: <n>` line above that text. `-PromptPath`
   must not already exist.

   `-UntrackedPath` names every visible, non-gitignored untracked file in the
   worktree — [receipts.md](references/receipts.md) lists the blocks
   an unnamed, unknown, or ignored path produces — and does not combine with
   `-Head`.

   Done when the script has run from the session worktree root and printed its
   receipt.
7. NEVER reconstruct the prompt assembly, the guardrail block, or the evidence
   collection inline, and never paste diff bytes into the session. The fixed
   wording lives in [prompt-template.md](references/prompt-template.md)
   and is changed only there.

   Done when the prompt file came only from that script and no diff bytes
   entered the session.
8. Read the receipt, one compact JSON object on stdout. Exit `0` succeeded and
   its `promptPath` is the prompt step 9 runs. Exit `2` is blocked and its
   `code` names the fix, so fix that and re-run this step.

   Exit `1` is a script error: stop and report its `code` and `message` rather
   than hand-assembling a prompt. Receipt fields and the exit `2` codes:
   [receipts.md](references/receipts.md).

   Done when the receipt's exit status is classified and, on exit `0`, its
   `promptPath` is in hand.
9. Run the prompt through `/claude-to-codex`
   ([../claude-to-codex/SKILL.md](../claude-to-codex/SKILL.md)) with
   the Repository root input as the Worktree, `-Sandbox read-only`, the
   `-Agent` that `## Inputs` selects, and neither `-Model` nor `-Effort`.

   For a review or audit, the prompt is the receipt's `promptPath`. For a
   researcher, it is the raw brief path prepared before step 9.

   Done when `/claude-to-codex` has returned a terminal status or a genuine
   failure, such as a model-check block, that ended the launch before one.
10. Map the outcome: `completed` — proceed; `malformed`, `failed`, or another
    genuine failure — map it to `CODEX-UNAVAILABLE` under `### Fallback`.

    A `completed` result that is not a review of the assigned scope is one this
    session judges malformed under `/claude-to-codex` `## Rules`.

    Done when the status is `completed` with the handoff this file defines and
    its `<out>` path returned, or the failure is mapped to `CODEX-UNAVAILABLE`.

## Handoff

Return the assigned skill's handoff plus the `<out>` path — the retained full
result on disk — and do not paste extra narration beyond the concise handoff
into the session.

On genuine failure the handoff is `CODEX-UNAVAILABLE: <short reason>` with the
unchanged target. A researcher failure has no substitute. Only explicit user
authorization given in the current session unblocks a review or audit failure,
by routing the same unchanged assignment to the Opus `reviewer` subagent. With
that authorization the brief names the assigned skill and states that the user
authorized the fallback in this session; `.claude/agents/reviewer.md` owns what
else it carries. `/claude-to-codex` `## Rules` defines genuine failure and the
single `-NoRetry` re-dispatch; `## Rules` `### Fallback` owns the
`general-purpose` last resort.

## Rules

- The calling manager session decides each finding under the
  [change-workflow.md](../../references/change-workflow.md) rule for review findings.
- Never edit code; findings-only conduct is
  [../../references/subagent-reporting.md](../../references/subagent-reporting.md)
  `## Delegated execution context`.
- An active landing gate records the final result once.

### Fallback

On a genuine failure, return this file's `## Handoff` failure form.

For a review or audit, this is a blocking failure: never dispatch a substitute
reviewer automatically. The user authorization that unblocks it, and its target,
are in this file's `## Handoff`. If the `reviewer` subagent type
is also unavailable, route at most once to `subagent_type: "general-purpose"`
with `model: "opus"` and the reviewer or auditor role stated at the top of the
prompt.

Add no other reviewer for consensus.
