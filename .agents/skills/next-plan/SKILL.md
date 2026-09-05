---
name: next-plan
description: Validates and deterministically claims one Git-backed Documents/Plans Plan through WorktreeCli, resolves it against current code, and presents the resolved Plan and execution card for implementation approval. Use only when the latest user request explicitly invokes `/next-plan` or `$next-plan`.
disable-model-invocation: true
argument-hint: "[Documents/Plans/... | partial pattern]"
allowed-tools: [Read, Write, Grep, Glob, Agent, Edit, PowerShell, AskUserQuestion]
---

# Next Plan

## Purpose

WorktreeCli alone validates metadata, selects, claims, prepares final state, and releases
claims; `Documents/Features` is never scheduler input. The Verify and land step of root
[AGENTS.md](../../../AGENTS.md) owns the cross-skill stage order, and
`/finalize-changes` the landing confirmation.

## When to use

Only for a current explicit `/next-plan` or `$next-plan` invocation.

## Inputs

The `argument-hint` value selects the Plan:

- Bare invocation selects the newest eligible Plan by immutable `createdUtc`,
  then normalized UTF-8 path.
- A normalized `Documents/Plans/...` argument selects that Plan.
- Any other argument is a case-sensitive partial match against executable Plan
  paths relative to `Documents/Plans/`; exactly one match selects that Plan,
  and zero or multiple matches block.

## Handoff

The preparation handoff extends the shared form in
[`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
with the declared fields below. Main's brief bounds the returned handoff to
every contradiction and unresolved decision, the other verified Plan statements
whose result requires a card or implementation change, and one count of the
unaffected statements; it never asks for a per-statement enumeration of
unaffected results.

```text
Claim: <Plan path or none; resolved state when claimed>
Classification: Tier 1 | Tier 2 | Tier 3 and trigger
Build required: <exact targets, or none>
Residuals: <blocker or none>

Execution card:
### What does this plan do?
<2-4 plain sentences>
### Why this is good for the codebase
<2-4 plain sentences>
- Goal: <result>
- Out of scope: <boundary>
- Tier trigger: <trigger or none>
- Interfaces and invariants: <contracts>
- Acceptance checks: <check and expected observation>
- Roles: <required and conditional assignments>
```

### Implementation approval

Preparation and claim do not require approval. Present the complete resolved
Plan and execution card before implementation: scope, invariants, role
assignments, acceptance criteria, and unresolved decisions. Deliver that
presentation per the User Interaction rules in root
[AGENTS.md](../../../AGENTS.md) — on Codex as exactly one complete
`<proposed_plan>` block, then ending the turn without an approval question; on
Claude Code and every other host as rendered message text whose approval
question is the last thing before the `Follow-up Plans created:` footer, after
which the user's next message is the decision. Any revision is a new complete
replacement presentation.

When preparation shows the problem the Plan describes is gone, ask the user
whether to retain the Plan or to explicitly authorize obsolete final cleanup.

## References

- [references/worker.md](references/worker.md) — private: read it only if you
  are the session executing this skill. The run order; main, running this skill
  itself, reads it as its own steps and rules.
- [references/claim-results.md](references/claim-results.md) — how each claim,
  listing, and claim-exit result is read.
- [references/tier3-workflow.md](references/tier3-workflow.md) — the additional
  Tier-3 preparation route.
- [references/run-checkpoint.md](references/run-checkpoint.md) — the end-of-run
  checkpoint mechanics.
- [references/follow-up-provenance.md](references/follow-up-provenance.md) —
  which session ID applies and the provenance block.
