---
name: session-audit
description: >-
  Final fresh-eyes audit of late, reconciled, or previously unseen integration
  hypotheses in a complete logical change. Use only when the user explicitly
  requests a session audit. Findings only; never edits.
disable-model-invocation: true
allowed-tools: [Read, Grep, Glob, PowerShell]
---

# Session Audit

## Purpose

Trace the named late, reconciled, or previously unseen integration hypotheses in
a complete logical change and report findings. Root `../../../AGENTS.md` owns
stage order; this skill owns only that local read-only action.

## When to use

Run only on an explicit user request. Delegation and reviewer conduct follow
`../../references/subagent-reporting.md`.
Audit only hypotheses that earlier domain reviews could not have covered; do not
repeat their artifact-level correctness, style, documentation, shader, or
validation passes.

## Inputs

Require a self-contained, immutable brief containing:

- the absolute adopted worktree and the session baseline commit;
- the changed-file inventory and touched regions with implementation,
  propagation, review-fix, conditional-role, and reconciliation attribution,
  excluding named pre-existing or concurrently owned work. The preparation
  implementer assembles that inventory with the read-only
  `.agents/scripts/Get-SessionChangeInventory.ps1 ... -Regions` run described
  below, never by enumerating the changed files and hunks inline; the
  attribution and the exclusion of pre-existing or concurrently owned work stay
  the implementer's judgment, which the script does not make;
- the final approved plan and the exact changes the user approved after it,
  declared invariants, and acceptance criteria;
- one decision per conditional mode below, marked `triggered` or
  `not triggered`; a triggered mode names its exact regions and hypotheses:
  - late semantic fixes — check newly introduced determinism/CRC, phase,
    guard-affinity, doc-symbol, whole-file coherence, debris, and residual
    regressions only where the late handoff makes them reachable;
  - reconciliation edits or invalidated assumptions — check manual resolutions
    and those assumptions for semantic merge damage, half-applied mirrors, stale
    symbols, duplicated paths, and incorrect version or compatibility integration;
  - Tier-3 cross-file integration;
  - contract-significant regions unseen by domain review — trace only their
    cross-file or contract-significant producer/consumer paths no supplied domain
    review saw, and spot-check that accepted fixes and resolved residuals exist
    in the final tree;
- completed applicable domain-review handoffs for every changed artifact type,
  plus reconciliation, build, external-API-verification, accepted-fix/retest,
  residual, and focus-area handoffs (`none` is valid for each).

The inventory run is `pwsh -NoProfile -File
.agents/scripts/Get-SessionChangeInventory.ps1 -RepositoryRoot <absolute adopted
worktree> -Baseline <full 40-character SHA> -Regions`. It writes no file and
prints one
`broken-engine-session-change-inventory/v1` object with `entries` and their
`class` values, `counts`, `triggers`, and the per-hunk `regions` table. Only
`status` `pass` is usable; any other status means the inventory is missing for
the rule below. The implementer reports `truncated` with the brief so a
shortfall is never presented as a complete inventory. An untracked
file appears only when the caller supplies it with `-IncludeUntracked
<comma-separated paths>`, and `counts.unlistedUntracked` reports how many
untracked files the run did not list.

Main dispatches one preparation `implementer` to assemble that brief from
repository state and to map the user-authorized audit scope to the same named
late, reconciled, or unseen-integration hypotheses; main then dispatches the
reviewer. Return `BLOCKED` when the checkout, session baseline, attribution, intent, a
mode decision, or an applicable prerequisite domain review is missing,
ambiguous, moving, or incomplete. The assigned implementer's assembly from
repository state is the only allowed source; the auditor never reconstructs
these inputs from conversation history.

## Handoff

Return the shared handoff form in `../../references/subagent-reporting.md`,
extended with the audit result block and these narrowed lines:

```markdown
## Session Audit Results

### Findings
- `path:line` — **Critical | Required:** — mode — <reachable failure and evidence>
- none

### API Verification Requests
- <symbol/rule, exact proposition, dependent candidate finding, applicability, official source>
- none

### Traced Clean
<Only when clean: hypotheses traced, decisive refutations, and `PASS — audit complete; stop.`>
```

- `Decisive checks` — the inventory, plus the trace or read and its result per
  authorized hypothesis.
- `Residuals` — pre-existing defect, incomplete trace, pending external
  verdict, or none; last.

`Changed files` and `Build required` are `none` because this findings-only
audit never edits a file.

Use `NEEDS_ACTION` for findings or pending external verification and `BLOCKED`
only for missing required evidence. Critical means data loss, broken
functionality, determinism failure, or an equivalent contract breach; every
other finding is Required.

The main session reads every finding, deduplicates it, and classifies Intent
(`conformance | plan_delta`) and Scope (`non_structural | structural`) before
dispatching any fix.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The audit steps the dispatched reviewer
  runs.
- [`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
  — task brief and shared handoff form.
