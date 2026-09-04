---
name: adversarial-review
description: >-
  Scoped fresh-eyes review that tries to disprove a Tier-3 change across every
  changed artifact type. Use automatically only for Tier-3 changes, when
  correctness review leaves one concrete reachable unresolved failure
  hypothesis, or when the user requests an adversarial second opinion on a
  supplied diff. Findings only; never edits.
allowed-tools: [Read, Grep, Glob, PowerShell]
---

# Adversarial Review

## Purpose

Attempt to disprove a change across every changed artifact type and report the
reachable failures that survive refutation. Runs as one delegated `reviewer` per
[`subagent-reporting.md`](../../references/subagent-reporting.md).

## When to use

- A Tier-3 change (see root AGENTS.md, Risk tiers).
- Correctness review leaves one concrete unresolved reachable hypothesis.
- The user requests an adversarial second opinion on a supplied diff.

## Inputs

Require the implementation handoff and complete changed-artifact list; plan or
intent with declared invariants; approved Tier-3 triggers (see root AGENTS.md, Risk tiers) or the exact unresolved
reachable hypothesis; and relevant prior findings, residuals, and focus areas.

Whenever a session baseline exists, the complete changed-artifact list is the
`entries` rows and their `class` values from the read-only inventory: `pwsh
-NoProfile -File .agents/scripts/Get-SessionChangeInventory.ps1 -RepositoryRoot
<absolute repository toplevel> -Baseline <full 40-character SHA>` (add `-Head
<commit>` for a committed head). It writes no file and prints one
`broken-engine-session-change-inventory/v1` object with `entries`, `counts`, and
`triggers`. The run is usable only when `status` is `pass` and `truncated` is
false; otherwise the complete changed-artifact list is unavailable, so return
`BLOCKED`. An untracked file appears only when the caller supplies it
with `-IncludeUntracked <comma-separated paths>`, and `counts.unlistedUntracked`
reports how many untracked files the run did not list. Never enumerate the
changed artifacts inline.

For a direct user request about a supplied diff, treat the supplied intent,
declared invariants, and behavioral or contract statements expressed by the diff as
the authorized hypotheses. If no briefing exists, reconstruct these inputs from
conversation history. Do not turn either case into an open-ended repository audit.

## Handoff

```markdown
## Adversarial Review Results

### Findings
- `path:line` — **Critical:** or **Required:** — <input/state -> wrong outcome that matters, and why>
- none

### API Verification Requests
- <symbol/rule> — <exact proposition> — <dependent proposed finding> — <applicability> — <official source>
- none

### Traced Clean
<Only when there are no findings: hypotheses traced, decisive refutation, and
`PASS — disproof attempt complete; stop.`>
```

Close with the shared handoff lines (`../../references/subagent-reporting.md`,
`## Handoffs`).

Use `NEEDS_ACTION` for findings or pending external verification and `BLOCKED`
only when required evidence could not be obtained. Critical means data loss,
broken functionality, determinism failure, or equivalent contract breach;
everything else reported is required, never optional.

## References

- [`references/worker.md`](references/worker.md) — the steps and rules the
  dispatched reviewer follows.
