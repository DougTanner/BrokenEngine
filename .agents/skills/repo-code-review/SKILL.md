---
name: repo-code-review
description: >-
  Review session-changed C++ for reachable correctness defects and Broken
  Engine contract violations. Use after C++ changes or when the user asks to
  review, check, or audit C++ code. Excludes shader-only and non-C++ changes;
  comment quality belongs to comment-review and style and formatting to
  code-style-review.
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
  naming, or documentation, which belong to `/code-style-review`, nor general
  comment quality, which belongs to `/comment-review`.

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
finding is `Required`. Give each finding a stable `CXX###` ID. Omit empty
optional extension rows. Correctness and scope-authorization failures both go
in the shared `Findings` field.

```text
Scope: PASS | NEEDS_ACTION | not applicable (Tier 1) | not supplied
API verification requests: <single checkable requests, or none>
Files reviewed: <path — regions and affected paths traced, one row each>
Project membership trigger: /update-vcxproj — <paths/reason> | none
Status: PASS | NEEDS_ACTION | BLOCKED
Findings: <CXX### Critical|Required path:line — claim — evidence and smallest correction; or none>
Changed files: none
Decisive checks: <one row per trace/check and result>
Build required: none
Evidence: <existing or Temp/ path plus selector, or none>
Executor: <own model id> <own effort>, each unknown when unreadable
Residuals: <pre-existing defect, incomplete trace, pending external verdict, size observation, or none>
```

Use `NEEDS_ACTION` when a finding or pending external verdict requires action,
`BLOCKED` when required review evidence is unavailable, and `PASS` only when
the review is clean. Never return `LGTM` without decisive trace evidence.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. Steps and rules for the dispatched
  reviewer.
- [`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
  — task brief and shared handoff form.
