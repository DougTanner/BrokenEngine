---
name: progressive-disclosure-review
description: >-
  Review session-changed instruction prose — AGENTS.md, CLAUDE.md,
  `.agents/skills/**/*.md`, `.agents/references/**/*.md` — against the root
  AGENTS.md
  progressive-disclosure directive. Use during Change Workflow Step 6 after
  `/update-claude-docs` whenever the session changed such a file, and when a
  reviewer suspects a skill body restates a reference, a script, or a parent
  AGENTS.md, or exceeds the skill size thresholds. Findings only; never edits.
allowed-tools: [Read, Grep, Glob, PowerShell]
---

# Progressive Disclosure Review

## Purpose

Findings on changed instruction prose that breaks the `Progressive disclosure`
directive in root [AGENTS.md](../../../AGENTS.md) `## Directives`. That
directive is the sole statement of the layering; this skill only enforces it.

## When to use

- Change Workflow Step 6, after `/update-claude-docs`, whenever the session
  changed `AGENTS.md`, `CLAUDE.md`, `.agents/skills/**/*.md`, or
  `.agents/references/**/*.md`.
- When a reviewer suspects a skill body restates a reference, a script, or a
  parent `AGENTS.md`, or exceeds the skill size thresholds.

Run in the delegated execution context of
[`subagent-reporting.md`](../../references/subagent-reporting.md); inline review
is prohibited. Dispatch is once per review round; after the manager accepts
findings and fixes land, only the affected files receive a focused re-review.

## Inputs

- Session baseline (full 40-character SHA) and the changed instruction-doc list,
  or the diff needed to derive it.
- The immutable snapshot of those files as changed.

If the baseline or the changed bytes are unavailable, return `BLOCKED` naming
the missing input.

## Handoff

Return the shared handoff form in
[`subagent-reporting.md`](../../references/subagent-reporting.md), `## Handoffs`,
with these declared extension lines above `Findings`:

```text
Skill: progressive-disclosure-review
Baseline: <full SHA>
Files checked: <count and paths>
```

Each `Findings` row is one line on this form:

```text
file:line — class: duplication | misplacement | size — evidence: owning location or measurement — fix direction
```

Example:

```text
.agents/skills/code-quality-metrics/SKILL.md:120 — class: duplication — evidence: references/history-contract.md:14-38 states the same JSONL field list — replace with a link to that reference
```

`Changed files` and `Build required` are `none` because this findings-only
review never edits a file.

Whatever gates on this handoff for changed instruction prose, the block must
carry four things, none of which may be dropped or reworded: the
`Skill: progressive-disclosure-review` marker line, the `Files checked:` line, a
`Status:` line, and a `Baseline:` line whose SHA is either the reviewed diff's
baseline or another resolvable commit at which the session's own bytes in the
instruction docs that diff changes are identical to their bytes at that
baseline, which is what makes those docs' reviewed bytes the bytes this review
read. A `BLOCKED` handoff, and one whose `Baseline:` is absent, unresolvable, or
a commit at which those bytes differ, are gated exactly like a missing handoff;
what a `NEEDS_ACTION` handoff needs to score is the consuming gate's rule.
Re-run this review only when those bytes differ at the two baselines.

The manager decides each finding on whether the failure is concrete and
meaningful under the standard defaults; this review adds no extra rounds.

## References

- [`references/worker.md`](references/worker.md) — steps and rules for the
  dispatched reviewer.
