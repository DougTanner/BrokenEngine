---
name: codex-review
description: >-
  Primary Claude Code route running any delegated reviewer or auditor role on
  Codex/Sol headless. Codex callers stop because they are already the Sol
  mapping (see the root AGENTS.md role table). Use for every Change Workflow review dispatch in Claude Code
  except the child reviewer a parent/manager orchestrator (such as /next-plan-review) dispatches itself.
allowed-tools: [Read, Bash, Agent]
---

# Codex Review

Claude Code runs every delegated reviewer or auditor role (Sol on Codex, per the
root [AGENTS.md](../../../AGENTS.md) role table) — `plan-audit`,
`plan-simplicity-review`,
`repo-code-review`, `glsl-review`, `scope-review`,
`progressive-disclosure-review`, `adversarial-review`,
`session-audit`, and external review lenses — on Codex/Sol through this skill.
Parent/manager orchestrators that dispatch their own child reviewer, including
`/next-plan-review`, are excluded, and so is the child reviewer they dispatch:
that child routes per the delegated-review routing bullet in the root
[AGENTS.md](../../../AGENTS.md). A `/codex-review` invocation of an assigned
skill constitutes the delegated-`reviewer` execution context; it is not an
"inline run" in the assigned skills' vocabulary, and `/verify-changes`
records it as the delegated reviewer execution. Codex callers must stop
instead of invoking this skill recursively — their reviewer role already
resolves to Sol.

## Inputs

- Assigned skill — the reviewer or auditor role to run, such as `plan-audit`, `repo-code-review`,
  or `session-audit`
- Its normal inputs: plan/intent, changed files and regions, and current
  residuals or reviewer focus
- Repository root — the absolute toplevel of the session worktree, defaulting to
  the current one; a relative path is accepted and resolves against the current
  directory — and session baseline (a full 40-character commit SHA)

## Method

1. Assemble the prompt with
   [scripts/New-CodexReviewPrompt.ps1](scripts/New-CodexReviewPrompt.ps1), which
   writes the prompt file and returns only a small receipt, so the diff never
   enters this session. NEVER reconstruct the prompt assembly, the guardrail
   block, or the evidence collection inline, and never paste diff bytes into the
   session. The fixed wording lives in
   [references/prompt-template.md](references/prompt-template.md) and is changed
   only there. The manager runs the script before dispatch; the reviewer, inside
   the Codex `--sandbox read-only` environment, only reads the prompt file it
   wrote. From the session worktree root:

   ```powershell
   pwsh -NoProfile -File .agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1 -RepositoryRoot <absolute repository toplevel> -Baseline <full 40-character baseline SHA> -AssignedSkill <assigned skill> -ScopeFile <scope file> -PromptPath <new prompt path> [-RiskTier <1|2|3>] [-UntrackedPath <comma-separated paths>] [-Head <rev>] [-AdHocRole]
   ```

   Write the judgment content yourself into `-ScopeFile`: the exact scope, the
   files and regions authorized for review, focus notes, and current residuals.
   The script copies that text verbatim and never authors, summarizes, or edits
   it, and never decides which files are in scope. Relay a plan's agent-made
   decisions there as reviewable claims, never as settled constraints; the
   authority-order directive in the root [AGENTS.md](../../../AGENTS.md) owns
   which decisions bind. When a verification dispatch needs a mechanical tool
   check the read-only sandbox cannot run, run only that non-judgment check
   host-side first and put its verbatim result and identity binding in
   `-ScopeFile` for the reviewer to validate, leaving every
   evaluation of that result to the reviewer alone. Include the assigned
   skill's own required evidence in that
   same file before dispatching: `plan-audit`'s draft execution card
   (`../plan-audit/SKILL.md`) and, for
   `verify-changes`, the reviewed change set's baseline and head SHAs plus each
   typed artifact its `## Required inputs` conditions name
   (`../verify-changes/SKILL.md`). The script blocks the dispatch when a SHA or
   a diff-triggered typed artifact is absent. For a reviewer role with no
   skill file — the Tier-2 coherence review, for one — pass a descriptive role
   name as `-AssignedSkill` together with `-AdHocRole`, and put that role's full
   review contract in `-ScopeFile`. `-RiskTier` adds one
   `Risk tier: <n>` line above that text. `-PromptPath` must not already exist;
   `-UntrackedPath` names every untracked file the review needs, and does not
   combine with `-Head`. Every named value must be untracked *and not
   gitignored*, because the inventory the script consults never reports an
   ignored path: a file under `Temp/` can never be named, since `Temp` is
   gitignored, even though `-ScopeFile` and `-PromptPath` may live there. Put
   review-evidence files the reviewer must read at the session worktree root
   instead — untracked and outside any ignored directory — and name each one in
   `-UntrackedPath`; remove them once the review no longer needs them, because
   any such file left behind stays a mandatory `-UntrackedPath` member.
2. Read the receipt, one compact JSON object on stdout. Exit `0` carries
   `promptPath`, `promptBytes`, `fileCount`, `binaryExcluded`,
   `sectionsWritten`, and `targetsPath`. For `repo-code-review`,
   `targetsPath` is the newly created targets file next to the prompt and that
   file is embedded in the evidence. Other assigned skills report `targetsPath: null`.
   Exit `2` is blocked and its `code` names the fix:
   `prompt.path-exists` — choose an unused prompt path, the existing file is left
   untouched; `prompt.diff-too-large` — the evidence passed the 4 MB budget, so
   split the review into smaller authorized scopes and never truncate the
   evidence; `prompt.inventory-truncated` — name every untracked path or narrow
   the baseline until the evidence is complete; `prompt.untracked-path-unknown` —
   a named path is not an untracked file, which includes a gitignored path such
   as one under `Temp/`; `prompt.head-untracked-conflict` — a
   commit-valued head has no untracked side; `prompt.assigned-skill-unknown` —
   `-AssignedSkill` names no skill file, so fix the name or pass `-AdHocRole`;
   `prompt.head-required` — `verify-changes` needs a commit-valued `-Head` whose
   reviewed paths' working-tree bytes match that commit's tree;
   `prompt.execution-card-required` — a `plan-audit` scope carries no
   `execution card` marker; `prompt.typed-artifacts-required` — a `verify-changes` scope is
   missing the baseline or head SHA, or a typed artifact the reviewed diff
   triggers, and the message names each one. Exit `1` is a
   script error: stop and report its `code` and `message` rather than
   hand-assembling a prompt.
3. Run the review with one bare blocking script call, issued with a call timeout
   of at least `600000` ms and never wrapped in a loop or chained with another
   command:

   ```powershell
   pwsh -NoProfile -File .codex/codex-review.ps1 -Worktree <worktree> -PromptFile <the receipt's promptPath> -OutFile <out>
   ```

   Pass a repo-relative `<out>` such as `Temp/<name>-out.md`; the launch
   tolerates an existing out-file, so `<out>` must be a fresh path that does not
   yet exist. The call starts Codex detached and waits up to 540 seconds for it,
   then returns a single-line JSON receipt. A `completed` receipt is followed by
   the separator line `--- findings ---` and then the findings themselves, all on
   that same call's stdout, so the success path never reads `<out>`; that file
   remains the retained full critique on disk.

   A result counts as well-formed only when its last non-empty line is the final
   verdict line every prompt mandates — `PASS`, `CHANGES-REQUIRED: <n>`, or
   `BLOCKED: <reason>`. The script's `.NOTES` header documents that check, the
   automatic retry behind it, and every receipt field. What each terminal status
   means for you: `completed` — proceed, and a `retried: true` value on it needs
   no action; `malformed` or `failed` — map it to `CODEX-UNAVAILABLE` under
   `## Fallback`. Your own judgment remains the backstop for a result the
   mechanical check accepted: benign CLI notices/deprecations are not failures,
   but a `completed` result that is drafting notes rather than a review of the
   assigned scope is malformed however it ends, and `## Fallback` says what to do
   about it. Return the handoff plus the `<out>` path, and do not paste extra
   narration beyond the concise handoff into the session.

   A review that outruns the budget comes back as a `running` receipt naming its
   `runId`. Resume it with exactly one further bare call per wake, also issued
   with a call timeout of at least `600000` ms and never in a loop:

   ```powershell
   pwsh -NoProfile -File .codex/codex-review.ps1 -Wait <runId>
   ```

   That call waits the same bounded budget and answers in the same shapes, so a
   `completed` answer carries the separator and the findings inline exactly as
   above. `completed`, `malformed`, or `failed` ends the wait. Because the run
   is detached, a call killed by its host timeout never kills the review and the
   same `runId` always resumes it, and every call stays bounded below the host's
   10-minute command cap, so a long review never needs a background run or a
   truncated scope.

## Manager evaluation

Sol over-reports edge cases and tends toward over-engineering, so the calling
manager session decides each finding under the root
[AGENTS.md](../../../AGENTS.md) decide-once and reject-speculative-findings
rules. Interruption findings — power loss, process kill, crash or timeout
mid-operation — are answered by the existing ordered fallback steps (idempotent
re-run, lease expiry, claim healing, Git state), never by new recovery
machinery; accept one only when a named interruption point provably defeats
those steps, and even then present the cost to the user before implementing.

## Fallback

Genuine failure means a `failed` wait status, a `malformed` wait status, or a
non-zero script exit. None of the three is retried. Duration alone is never a
`CODEX-UNAVAILABLE` cause: a run that stays `running` across many waits is
progressing normally, so keep waiting.

One bounded exception covers what the script's mechanical check cannot see. When
a `completed` result passed that check but this session judges it malformed
anyway — leaked drafting notes, or an answer to no assigned scope — re-dispatch
the identical prompt exactly once, with a fresh `<out>` path and `-NoRetry`:

```powershell
pwsh -NoProfile -File .codex/codex-review.ps1 -Worktree <worktree> -PromptFile <the same promptPath> -OutFile <fresh out> -NoRetry
```

`-NoRetry` spends no automatic retry, so this dispatch is the assignment's last
one: its result is final, and a second malformed result is reported as
`CODEX-UNAVAILABLE` under the rule below.

On a genuine failure, stop and report `CODEX-UNAVAILABLE: <short reason>` with
the unchanged target. This is a
blocking failure: never dispatch a substitute reviewer automatically. Only
explicit user authorization in the current session unblocks it, by routing the
same unchanged assignment to the normal Opus `reviewer` subagent — or, if that
subagent type is also unavailable, at most once to `subagent_type:
"general-purpose"` with `model: "opus"` and the reviewer or auditor role stated
at the top of the prompt. Do not retry Codex beyond the single re-dispatch above
or add another reviewer for consensus.

## Notes

- Findings only; never edit code.
- An active landing gate records the final result once.
