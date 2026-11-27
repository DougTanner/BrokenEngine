---
name: progressive-disclosure-review
description: >-
  Review session-changed instruction prose — AGENTS.md,
  `.agents/skills/**/*.md`, `.agents/references/**/*.md`,
  `Documents/Architecture/**/*.md` — against the root AGENTS.md
  progressive-disclosure and instruction-changes directives. Use during the
  Change Workflow Apply the triggered cleanup step after `/update-claude-docs`
  whenever the session changed such a file, and when a reviewer suspects a
  skill body restates a reference, a script, or a parent AGENTS.md, or exceeds
  the skill size thresholds. Findings only; never edits.
allowed-tools: [Read, Grep, Glob, PowerShell]
---

# Progressive Disclosure Review

## Purpose

Findings on changed instruction prose that breaks the `Progressive disclosure`
or `Instruction changes` directive in root [AGENTS.md](../../../AGENTS.md)
`## Directives`. Those directives are the sole statement of their rules; this
skill only enforces them.

## When to use

- The Change Workflow Apply the triggered cleanup step, whenever the session
  change inventory's `progressiveDisclosureReview` trigger fires.
- When a reviewer suspects a skill body restates a reference, a script, or a
  parent `AGENTS.md`, or exceeds the skill size thresholds.

Run in the delegated execution context of
[`subagent-reporting.md`](../../references/subagent-reporting.md)
`## Delegated execution context`; inline review by the main session is
prohibited. Outside [`/coherence-review`](../coherence-review/SKILL.md)
combined mode, use one separate dispatch per printed step-script row; when
combined mode carries this check, the delegated `/coherence-review` reviewer
runs it in its own context with no separate dispatch.

## Inputs

- `Baseline` — the full 40-character session baseline SHA and the absolute
  repository toplevel, plus the committed head when the review runs against one.
- `Scope` — any untracked paths the review must cover.
- `Stated net size` — the net instruction size change the session's plan
  states; omitted when the session has no plan or the plan states none.

The official region inventory supplies the review regions, including for
bridge runs (`-Head` for a committed head; `-UntrackedPath` or
`-IncludeUntracked` from `Scope`):

```powershell
pwsh -NoProfile -NonInteractive -Command 'Import-Module ./.agents/scripts/SessionChangeInventory.psm1; Get-SessionChangeInventory -RepositoryRoot (Get-Location).Path -Baseline <baseline SHA> -Regions|Select-Object status,coretide,counts,entries,regions|ConvertTo-Json -Depth 5'
```

With `Stated net size`, use the `instruction-size` row from the Change Workflow
`Run targeted pre-review checks` command (`-Head` for a committed head).

If the baseline is unavailable or the change inventory cannot produce the
changed regions, return `BLOCKED` naming the missing input.

## Handoff

Return the shared handoff form in
[`subagent-handoff.md`](../../references/subagent-handoff.md), `## Handoffs`,
with these declared extension lines above `Findings`:

```text
Skill: progressive-disclosure-review
Baseline: <full SHA>
Files checked: <count and paths>
```

Each `Findings` row is one line on this form, with a stable `PD###` ID, graded
by [`subagent-handoff.md`](../../references/subagent-handoff.md) `## Severity`:

```text
PD### Critical|High|Medium|Low file:line — class: duplication | misplacement | size | instruction-change — evidence: owning location, measurement, or replacing text or mechanism — fix direction
```

Example:

```text
PD001 Medium .agents/skills/code-quality-metrics/SKILL.md:120 — class: duplication — evidence: references/history-contract.md:14-38 states the same JSONL field list — replace with a link to that reference
```

`Changed files` and `Build required` are `none` because this findings-only
review never edits a file.

Whatever gates on this handoff for changed instruction prose, the block must
carry four things, none of which may be dropped or reworded: the
`Skill: progressive-disclosure-review` marker line, the `Files checked:` line, a
`Status:` line, and a `Baseline:` line. A `BLOCKED` handoff is gated exactly
like a missing handoff; whether a handoff still covers the landing candidate is
the review-round check's decision
([`landing-acceptance-table.md`](../finalize-changes/references/landing-acceptance-table.md)
step 3), and what a `NEEDS_ACTION` handoff needs to score is the consuming
gate's rule.

The manager decides each finding on whether the failure is concrete and
meaningful under the standard defaults.

When an inbound workflow requirement has a worker run this skill inside that
worker's own task, rather than as a separate dispatch, as evidence for that
task, the decisive field is `Status`. That worker adds one row per run under its
own shared `Decisive checks` — whether the expected observation was met and
that run's `Status` — plus one row summarizing the resulting working-tree state,
never pasting `git status --porcelain` output inline. It writes every such
run's complete handoff, not only past the character cap, to one gitignored
`Temp/` file with one `## Run <n>` heading per run, by the overflow rule in
[`subagent-handoff.md`](../../references/subagent-handoff.md) `## Handoffs`. A
running skill whose own `## Handoff` declares a field for the run's result puts
that run's row there instead of under `Decisive checks`; one that also places
each run's complete handoff follows its own placement instead of these rows and
this `Temp/` file.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. Steps and rules for the dispatched
  `reviewer`.
