---
name: plan-audit
description: >-
  Adversarially audit a Tier-2 or Tier-3 implementation plan before
  implementation; for Tier 3 it runs before /external-grill-plan. Do not add it
  to Tier-1 mechanical changes.
  Runs inside one delegated `reviewer`; findings only, with no edits, harness
  work, user interview, or further delegation.
allowed-tools: [Read, Grep, Glob, PowerShell]
---

# Plan Audit

## Purpose

Audit a complete implementation plan against the current repository and return
concrete findings and improvement suggestions for the manager to resolve —
through `/external-grill-plan` for Tier 3, or directly with the user for Tier 2.

## When to use

- Every Tier-2 and Tier-3 change, before implementation; Tier-1 mechanical work
  skips it.
- Runs in the delegated execution context of
  [`../../references/subagent-reporting.md`](../../references/subagent-reporting.md);
  if the mandatory reviewer is unavailable, the manager reports a blocker.
- The audit is findings-only work and never creates an approval gate; a
  `/next-plan` invocation additionally follows the authoritative
  implementation-approval contract (`../next-plan/SKILL.md`, "Implementation
  approval").

## Inputs

- Immutable complete plan as a file path readable from the worktree root — the exact claimed-Plan path or
  a complete resolved Plan snapshot the preparation `implementer` wrote and cited under `Evidence` (a gitignored `Temp/` path is fine),
  whose body carries the plan's `## In scope` and `## Out of scope` sections — headings verbatim, with their content intact — because
  the citation check reads heading presence from the supplied file alone. Never inline text: the citation check takes only a path.
- Draft execution card for every Tier-2 and Tier-3 plan, carrying every field of
  the card template in [`../next-plan/SKILL.md`](../next-plan/SKILL.md)
  `## Handoff`. Head it with the words `execution card`, in any casing, which
  `/codex-review`'s prompt assembly requires in the scope file before it will
  dispatch this audit.
- Approved execution card, superseding the draft above, and session baseline
  only when root `AGENTS.md` triggers them.
- User intent and applicable repository instructions
- Relevant repository paths and every cited code region
- Accumulated constraints or known residuals

## Handoff

For each finding:

> `PA-F-###` Critical|Required|Recommended `plan-path:line` — category — concrete problem — evidence: `repository-path:line` — proposed improvement

Each finding's problem, evidence, and proposed improvement appear exactly once,
as one single-line `Findings` row of the shared handoff in the form above; every
other field that refers to a finding (such as `API Verification Requests:`)
names it by its `PA-F-###` ID and adds only its own required content, never
restating that finding's problem, evidence, or proposed improvement.

If clean, state `PASS — no meaningful plan flaws found.` Return:

```text
API Verification Requests: <single checkable requests or none>
Traceability checked: <one requirement or invariant> <-> <implementation site or check>; return one single-line row per mapping, put supporting detail under `Evidence`, and follow the shared handoff overflow rule
Required next step: Tier 3 -> manager decision, then /external-grill-plan | Tier 2 -> manager decision
```

Follow those extension fields with the shared handoff lines
(`../../references/subagent-reporting.md`, `## Handoffs`); this findings-only
audit never changes a file and never requires a build.

Use `NEEDS_ACTION` for findings or pending external verdicts and `BLOCKED` only
when required input or evidence is unavailable.

After the manager decides on findings and external verdicts, every Tier-3
result, including a clean pass, proceeds to `/external-grill-plan`; Tier 2
returns to the manager.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The audit steps and rules the
  dispatched reviewer runs.
