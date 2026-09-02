---
name: repo-code-review
description: >-
  Review session-changed C++ for reachable correctness defects and Broken
  Engine contract violations. Use after C++ changes or when the user asks to
  review, check, or audit C++ code. Excludes shader-only and non-C++ changes;
  style and formatting belong to code-style-review.
allowed-tools: [Read, Grep, Glob, Bash, PowerShell]
---

# Repository C++ Review

## Purpose

Findings on reachable correctness defects and Broken Engine contract violations
in session-changed C++, each with the smallest correction, and never a fix.
Delegation form: `../../references/subagent-reporting.md`.

## When to use

- After a session changes C++, or when the user asks to review, check, or audit
  C++ code.
- Not for shader-only or non-C++ changes, and not for style, formatting,
  naming, general comment quality, or documentation, which belong to
  `/code-style-review`.

## Inputs

Require a self-contained brief containing:

- the session baseline as a full Git SHA, complete immutable authorized C++ diff,
  and exact changed files/regions, separated from pre-existing and concurrently
  owned changes;
- a `broken-engine-code-quality-targets/v1` targets file produced by
  `.agents/scripts/Get-SessionChangeInventory.ps1 -EmitTargets` from that
  authorized diff (or focused re-review), listing the baseline and current paths
  of additions, deletions, and renames; it excludes pre-existing and concurrently
  owned changes;
- that targets file's C++ target selection, which is the same run's `cpp` and
  `dual-language-header` classes; those class rules are the only statement of
  which `.h` files are GLSL-only, and every `dual-language-header` entry routes
  to both this C++ review and the GLSL review;
- approved intent, plan and deltas, affected contracts, and declared
  invariants;
- implementation handoff, acceptance criteria, notes on which other code sites
  the change may affect, and any prior findings relevant to a focused re-review;
- checkout path and applicable repository instructions.

The targets file is the authoritative supplied input. A
`/codex-review` prompt supplies it as the `Targets file: <path>` entry in its
evidence section (the receipt's `targetsPath`, written next to the prompt file),
and inlines the same bytes there as a copy of that file. Otherwise the dispatching manager saves one
read-only run to a file:
`pwsh -NoProfile -File .agents/scripts/Get-SessionChangeInventory.ps1
-RepositoryRoot <absolute repository toplevel> -Baseline <full 40-character SHA>
-EmitTargets`, adding `-IncludeUntracked <comma-separated paths>` for
authorized untracked additions and `-Head <commit>` for a committed head. On `status` `pass` (exit 0) stdout carries
only the targets bytes; `blocked` (exit 2) or `error` (exit 1) leaves stdout
empty and reports the envelope on stderr, which counts as a missing targets file
below. Never rebuild the targets file or restate the class decision inline.

Return `BLOCKED` when the session baseline, diff boundary, targets file, intent,
or invariants are missing or moving. Do not reconstruct them from a mutable merge
base, derive a broader target selection from checkout changes, or expand a supplied
review into an open-ended repository audit. After an accepted fix, review only the
fixed region and directly affected paths unless a reproducible failure justifies
another round.

## Handoff

Order findings by impact. `Critical` means data loss, broken functionality,
determinism failure, or an equivalent contract breach; every other reported
finding is `Required`. Omit empty optional sections.

```markdown
## C++ Review Results

### Findings
- `path:line` — **Critical | Required:** <reachable failure, evidence, smallest correction>

### Scope
- `path:lines` — **unauthorized | overbuilt | kiss:** <cited clause or absent authorization, evidence>

### API Verification Requests
<single checkable requests>

### Size Observations
- `path` (`N bt-token-v1`) — <cohesive split and why it is a manager follow-up candidate>

### Files Reviewed
- `path` — <regions and affected paths traced>

### Recommendation
PASS | NEEDS_ACTION | BLOCKED

Functions/regions touched: none
Scope: PASS | NEEDS_ACTION | not applicable (Tier 1) | not supplied
Project membership trigger: /update-vcxproj — <paths/reason> | none
```

Follow those extension fields with the shared handoff lines
(`../../references/subagent-reporting.md`, `## Handoffs`); this findings-only
review never changes a file and never requires a build, and a pre-existing
defect, incomplete trace, or pending external verdict belongs in `Residuals`.

For a clean review, state `PASS — no issues found`, list the evidence and files,
and keep the unchanged footer. Never return `LGTM` without decisive trace
evidence.

## References

- [`references/worker.md`](references/worker.md) — steps and rules for the
  dispatched reviewer.
- [`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
  — task brief and shared handoff form.
