# Codex Review Worker

The dispatch steps and the rules for the session running this skill. Triggers,
inputs, and the handoff form live in [`../SKILL.md`](../SKILL.md).

## Steps

1. Write the judgment content yourself into `-ScopeFile`: the exact scope, the
   files and regions authorized for review, focus notes, and current residuals.
   The script copies that text verbatim and never authors, summarizes, or edits
   it, and never decides which files are in scope.

   Relay a plan's agent-made decisions there as reviewable claims, never as
   settled constraints;
   [`../../../references/authority-order.md`](../../../references/authority-order.md)
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
   dispatching: `plan-audit`'s draft execution card (`../../plan-audit/SKILL.md`).
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
   `plan-simplicity-review`, for one — is not change evidence: keep it under `Temp/` (gitignored,
   so never named), give its repo-relative path in `-ScopeFile`, and the reviewer reads it from the
   worktree like any other file, because the read-only Codex run is rooted at the worktree.

   Done when every such file sits under `Temp/` with its repo-relative path
   named in `-ScopeFile`.
6. Assemble the prompt with
   [../scripts/New-CodexReviewPrompt.ps1](../scripts/New-CodexReviewPrompt.ps1), which
   writes the prompt file and returns only a small receipt, so the diff never
   enters this session.

   The manager runs the script before dispatch; the reviewer, inside the Codex
   `--sandbox read-only` environment, only reads the prompt file it wrote. From
   the session worktree root:

   ```powershell
   pwsh -NoProfile -File .agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1 -RepositoryRoot <absolute repository toplevel> -Baseline <full 40-character baseline SHA> -AssignedSkill <assigned skill> -ScopeFile <scope file> -PromptPath <new prompt path> [-RiskTier <1|2|3>] [-UntrackedPath <comma-separated paths>] [-Head <rev>] [-AdHocRole]
   ```

   `-RiskTier` adds one `Risk tier: <n>` line above that text. `-PromptPath`
   must not already exist.

   `-UntrackedPath` names every visible, non-gitignored untracked file in the
   worktree — [receipts.md](receipts.md) lists the blocks
   an unnamed, unknown, or ignored path produces — and does not combine with
   `-Head`.

   Done when the script has run from the session worktree root and printed its
   receipt.
7. NEVER reconstruct the prompt assembly, the guardrail block, or the evidence
   collection inline, and never paste diff bytes into the session. The fixed
   wording lives in [prompt-template.md](prompt-template.md)
   and is changed only there.

   Done when the prompt file came only from that script and no diff bytes
   entered the session.
8. Read the receipt, one compact JSON object on stdout. Exit `0` succeeded and
   its `promptPath` is the prompt step 9 runs. Exit `2` is blocked and its
   `code` names the fix, so fix that and re-run this step.

   Exit `1` is a script error: stop and report its `code` and `message` rather
   than hand-assembling a prompt. Receipt fields and the exit `2` codes:
   [receipts.md](receipts.md).

   Done when the receipt's exit status is classified and, on exit `0`, its
   `promptPath` is in hand.
9. Run the review with one bare blocking script call, issued with a call timeout
   of at least `600000` ms and never wrapped in a loop or chained with another
   command:

   ```powershell
   pwsh -NoProfile -File .codex/codex-review.ps1 -Worktree <worktree> -PromptFile <the receipt's promptPath> -OutFile <out>
   ```

   Pass a repo-relative `<out>` such as `Temp/<name>-out.md`; the launch
   tolerates an existing out-file, so `<out>` must be a fresh path that does not
   yet exist. The call starts Codex detached and waits up to 540 seconds for it,
   then returns a single-line JSON receipt, whose fields and blocked exit codes
   [receipts.md](receipts.md) owns for both scripts.

   A `completed` receipt is followed by the separator line `--- findings ---`
   and then the findings themselves, all on that same call's stdout, so the
   success path never reads `<out>`; that file remains the retained full
   critique on disk.

   To verify a change to `.codex/codex-review.ps1` on a machine without the
   Codex CLI, issue that same call from the Bash tool with a stand-in `codex`
   placed first on `PATH` — `PATH=<stub dir>:$PATH pwsh -NoProfile -File
   .codex/codex-review.ps1 ...` — which stays one invocation; PowerShell has
   no inline prefix form, so this verification form is Bash-tool only.

   Done when that single call has returned its receipt.
10. A result counts as well-formed only when its last non-empty line is the final
    verdict line every prompt mandates — `PASS`, `CHANGES-REQUIRED: <n>`, or
    `BLOCKED: <reason>`.

    What each terminal status means for you: `completed` — proceed, and a
    `retried: true` value on it needs no action; `malformed` or `failed` — map it
    to `CODEX-UNAVAILABLE` under `### Fallback`.

    Your own judgment remains the backstop for a result the mechanical check
    accepted: benign CLI notices/deprecations are not failures, but a `completed`
    result that is drafting notes rather than a review of the assigned scope is
    malformed however it ends, and `### Fallback` says what to do about it.

    Done when the receipt is `completed` with the handoff [`../SKILL.md`](../SKILL.md)
    defines and its `<out>` path returned, `malformed` or `failed` mapped to
    `CODEX-UNAVAILABLE`, or `running` with its `runId` carried to step 11.
11. A review that outruns the budget comes back as a `running` receipt naming its
    `runId`. Resume it with exactly one further bare call per wake, also issued
    with a call timeout of at least `600000` ms and never in a loop:

    ```powershell
    pwsh -NoProfile -File .codex/codex-review.ps1 -Wait <runId>
    ```

    That call waits the same bounded budget and answers in the same shapes, so a
    `completed` answer carries the separator and the findings inline exactly as
    above. `completed`, `malformed`, or `failed` ends the wait.

    Because the run is detached, a call killed by its host timeout never kills
    the review and the same `runId` always resumes it, and every call stays
    bounded below the host's 10-minute command cap, so a long review never needs
    a background run or a truncated scope.

    Done when the wait ends in `completed`, `malformed`, or `failed`.

## Rules

- The calling manager session decides each finding under the root
  [AGENTS.md](../../../../AGENTS.md) rule for review findings.
- Never edit code; findings-only conduct is
  [../../../references/subagent-reporting.md](../../../references/subagent-reporting.md).
- An active landing gate records the final result once.

### Fallback

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

On a genuine failure, stop and return the `CODEX-UNAVAILABLE` handoff that
[`../SKILL.md`](../SKILL.md) `## Handoff` defines. When the wait status is
`failed`, take its short reason from the receipt's own `reason` field, without a
further diagnostic call. This is a blocking failure: never dispatch a substitute
reviewer automatically.

The user authorization that unblocks it, and its target, are in
[`../SKILL.md`](../SKILL.md) `## Handoff`. If the `reviewer` subagent type is
also unavailable, route at most once to `subagent_type: "general-purpose"` with
`model: "opus"` and the reviewer or auditor role stated at the top of the
prompt.

Do not retry Codex beyond the single re-dispatch above or add another reviewer
for consensus.
