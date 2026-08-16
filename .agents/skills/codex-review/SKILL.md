---
name: codex-review
description: >-
  Primary Claude Code route running any delegated reviewer or auditor role on
  Codex/Sol headless. Codex callers stop because they are already the Sol
  mapping (see the root AGENTS.md role table). Use for every Change Workflow review dispatch in Claude Code.
allowed-tools: [Read, Bash]
---

# Codex Review

Claude Code runs every delegated reviewer or auditor role (Sol on Codex, per the
root [AGENTS.md](../../../AGENTS.md) role table) — `plan-audit`,
`repo-code-review`, `glsl-review`, `scope-review`, `adversarial-review`,
`session-audit`, and external review lenses — on Codex/Sol through this skill.
Parent/manager orchestrators that dispatch their own child reviewer, including
`/next-plan-review`, are excluded. A `/codex-review` invocation of an assigned skill constitutes the
delegated-`reviewer` execution context; it is not an "inline run" in the assigned
skills' vocabulary, and `/verify-changes` records it as the delegated reviewer
execution. Codex callers must stop instead of invoking this skill recursively — their reviewer
role already resolves to Sol.

## Inputs

- Assigned skill — the reviewer or auditor role to run, such as `plan-audit`, `repo-code-review`,
  or `session-audit`
- Its normal inputs: plan/intent, changed files and regions, and current
  residuals or reviewer focus
- Worktree (default to current repository root) and session baseline (a full 40-character commit SHA)

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
   pwsh -NoProfile -File .agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1 -RepositoryRoot <worktree> -Baseline <full 40-character baseline SHA> -AssignedSkill <assigned skill> -ScopeFile <scope file> -PromptPath <new prompt path> [-RiskTier <1|2|3>] [-UntrackedPath <comma-separated paths>] [-Head <rev>] [-AdHocRole] [-PreflightTargets | -ReuseTargets]
   ```

   `/repo-code-review` has an ordered path that only handles the targets file
   for the host-side metrics run. Add `-PreflightTargets` to this invocation
   to perform the ordinary repository, inventory, revision, untracked-path,
   scope-file, and sibling-path checks while bypassing only the metrics-digest
   requirement. It creates no prompt and writes the exact canonical targets
   file to `<PromptPath>.targets.json`; give the receipt's `targetsPath` to
   Compare. Add that digest, together with the baseline and head identities it
   used, to the scope file, then invoke the script again with the same prompt
   path and `-ReuseTargets`. Reuse requires that sibling, regenerates the
   canonical targets file contents in memory, compares the captured file bytes
   exactly, and embeds those captured bytes. The caller owns the targets file
   from a successful preflight; reuse never rewrites or deletes it, including
   if prompt assembly later fails. The two switches are mutually exclusive,
   valid only for `repo-code-review`, and there is no parameter for the
   targets file path.

   Write the judgment content yourself into `-ScopeFile`: the exact scope, the
   files and regions authorized for review, focus notes, and current residuals.
   The script copies that text verbatim and never authors, summarizes, or edits
   it, and never decides which files are in scope. When a verification dispatch
   needs a mechanical tool check the read-only sandbox cannot run, run only that
   non-judgment check host-side first and put its verbatim result and identity
   binding in `-ScopeFile` for the reviewer to validate, leaving every
   evaluation of that result to the reviewer alone. Include the assigned
   skill's own required evidence in that
   same file before dispatching: `plan-audit`'s draft execution card
   (`../plan-audit/SKILL.md`), the host-run `code-quality-metrics` Compare digest
   for `repo-code-review` alongside the baseline and head identity values it ran
   against, as `../repo-code-review/references/metrics-protocol.md` requires
   (`../code-quality-metrics/SKILL.md`), and, for
   `verify-changes`, the reviewed change set's baseline and head SHAs plus each
   typed artifact its `## Required inputs` conditions name
   (`../verify-changes/SKILL.md`). The script blocks the dispatch when one is
   absent. For a reviewer role with no
   skill file — the Tier-2 coherence review, for one — pass a descriptive role
   name as `-AssignedSkill` together with `-AdHocRole`, and put that role's full
   review contract in `-ScopeFile`. `-RiskTier` adds one
   `Risk tier: <n>` line above that text. `-PromptPath` must not already exist;
   `-UntrackedPath` names every untracked file the review needs, and does not
   combine with `-Head`. Ordinary assembly still requires the metrics digest;
   only `-PreflightTargets` bypasses that guard.
2. Read the receipt, one compact JSON object on stdout. Exit `0` carries
   `promptPath`, `promptBytes`, `fileCount`, `binaryExcluded`,
   `sectionsWritten`, and `targetsPath`. For ordinary `repo-code-review`,
   `targetsPath` is the newly created targets file next to the prompt and that
   file is embedded in the evidence. A successful preflight reports
   `promptPath: null`, its sibling `targetsPath`, and zero `promptBytes` and
   `sectionsWritten`; a successful reuse reports the caller-owned targets file
   and embeds its matching bytes. Other assigned skills report `targetsPath: null`.
   Exit `2` is blocked and its `code` names the fix:
   `prompt.path-exists` — choose an unused prompt path, the existing file is left
   untouched; `prompt.diff-too-large` — the evidence passed the 4 MB budget, so
   split the review into smaller authorized scopes and never truncate the
   evidence; `prompt.inventory-truncated` — name every untracked path or narrow
   the baseline until the evidence is complete; `prompt.untracked-path-unknown` —
   a named path is not an untracked file; `prompt.head-untracked-conflict` — a
   commit-valued head has no untracked side; `prompt.assigned-skill-unknown` —
   `-AssignedSkill` names no skill file, so fix the name or pass `-AdHocRole`;
   `prompt.head-required` — `verify-changes` needs a commit-valued `-Head` whose
   reviewed paths' working-tree bytes match that commit's tree;
   `prompt.execution-card-required` — a `plan-audit` scope carries no
   `execution card` marker; `prompt.metrics-digest-required` — a
   `repo-code-review` scope carries no `broken-engine-code-quality-evidence/v2`
   digest; `prompt.targets-mode-invalid` — the target switches are both
   supplied or a target switch is used with another assigned skill;
   `prompt.targets-missing` — reuse has no sibling targets file;
   `prompt.targets-mismatch` — the sibling bytes differ from the regenerated
   canonical targets file contents; `prompt.typed-artifacts-required` — a `verify-changes` scope is
   missing the baseline or head SHA, or a typed artifact the reviewed diff
   triggers, and the message names each one. Target-mode blockers leave the
   prompt absent; reuse blockers preserve the caller-owned targets file bytes. Exit `1` is a
   script error: stop and report its `code` and `message` rather than
   hand-assembling a prompt.
3. Launch and poll — two separate calls, because no call ever waits for Codex.
   Launch with `pwsh -NoProfile -File .codex/codex-review.ps1 -Worktree
   <worktree> -PromptFile <the receipt's promptPath> -OutFile <out>`, which
   returns in seconds with a single-line JSON launch receipt carrying `runId`
   and the absolute `outFile`. Then re-invoke `pwsh -NoProfile -File
   .codex/codex-review.ps1 -Poll <runId>`, which also returns immediately, until
   its `status` is `completed` or `failed`; `running` means keep polling. Every
   call is bounded far below the host's 10-minute command cap, so a long review
   is never killed and never needs a background run or a truncated scope. Only a
   `completed` status guarantees `<out>` is fully written.
4. After a `completed` status, read `<out>`; success requires a non-empty
   structured result — a skill-native status line or a verdict token, either
   vocabulary counts. Benign CLI
   notices/deprecations are not failures. Return the handoff plus the `<out>`
   path; the output file is the retained full critique — do not paste
   extra narration beyond the concise handoff into the session.

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

Genuine failure means a `failed` poll status, a non-zero script exit, or
malformed output. Duration alone is never a `CODEX-UNAVAILABLE` cause: a run
that stays `running` across many polls is progressing normally, so keep polling.
On a genuine failure, stop and report `CODEX-UNAVAILABLE: <short reason>` with
the unchanged target. This is a
blocking failure: never dispatch a substitute reviewer automatically. Only
explicit user authorization in the current session unblocks it, by routing the
same unchanged assignment to the normal Opus `reviewer` subagent — or, if that
subagent type is also unavailable, at most once to `subagent_type:
"general-purpose"` with `model: "opus"` and the reviewer or auditor role stated
at the top of the prompt. Do not retry Codex or add another reviewer for
consensus.

## Notes

- Findings only; never edit code.
- An active landing gate records the final result once.
