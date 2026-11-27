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
Delegation form: `../../references/subagent-reporting.md`
`## Delegated execution context`.

## When to use

- After a session changes C++, or when the user asks to review, check, or audit
  C++ code.
- Not for shader-only or non-C++ changes, and not for style, formatting,
  naming, or documentation, which belong to `/code-style-review`, nor general
  comment quality, which belongs to `/comment-review`.

## Inputs

Require a self-contained brief (`../../references/subagent-reporting.md`,
`## Task brief`) containing:

- `Baseline` as the repository root, full 40-character Git SHA, and committed
  head when one applies, plus `Scope` for the authorized diff boundary and any
  untracked files or path prefixes to include, separated from pre-existing and
  concurrently owned changes;
- approved intent, plan and deltas, affected contracts, and declared invariants;
- implementation handoff, acceptance criteria, and notes on which other code
  sites the change may affect;
- checkout path and applicable repository instructions.

Derive changed entries and regions in-process, including on bridge runs (which
pass `Baseline` and `Scope`; add `-Head <commit>` for a committed head and
`-UntrackedPath`/`-IncludeUntracked` and `-PathPrefix` from `Scope`):

```powershell
pwsh -NoProfile -NonInteractive -Command 'Import-Module ./.agents/scripts/SessionChangeInventory.psm1; Get-SessionChangeInventory -RepositoryRoot (Get-Location).Path -Baseline <baseline SHA> -Regions|Select-Object status,coretide,entries,regions|ConvertTo-Json -Depth 5'
```

Require inventory `status` `pass`; otherwise return `BLOCKED` without rebuilding
the diff. Select only entries whose `class` is `cpp` or `dual-language-header`
with their matching `regions` — those script class rules alone decide which `.h`
files are GLSL-only (never restate them), and every `dual-language-header` entry
routes to both this C++ review and `/glsl-review`.

Return `BLOCKED` when the session baseline, inventory, diff boundary, intent, or
invariants are missing or moving. Do not reconstruct them from a mutable merge
base, derive a broader target selection from checkout changes, or expand a
supplied review into an open-ended repository audit.

## Handoff

Order findings by impact and grade each by
[`../../references/subagent-handoff.md`](../../references/subagent-handoff.md)
`## Severity`. Give each finding a stable `CXX###` ID. Omit empty
optional extension rows. Correctness and scope-authorization failures both go
in the shared `Findings` field.

Return the shared handoff form in
[`../../references/subagent-handoff.md`](../../references/subagent-handoff.md),
extended with these fields:

- `Scope` — `PASS`, `NEEDS_ACTION`, `not applicable (Tier 1)`, or
  `not supplied`.
- `API verification requests` — single checkable requests, or none.
- `Project membership trigger` — `/update-vcxproj` with the paths and reason,
  or none.

Each shared `Residuals` row names a pre-existing defect, incomplete trace,
pending external verdict, size observation, or refactor observation, using
`none` when absent. A `refactor observation` row is one line:
`<path:line> — <observed shape> — <suggested restructure>`.

Each `Findings` row is one line on this form:

```text
CXX### Critical|High|Medium|Low path:line — claim — evidence and smallest correction
```

`Changed files` and `Build required` are `none` because this findings-only
review never edits a file.

Use `NEEDS_ACTION` when a finding or pending external verdict requires action,
`BLOCKED` when required review evidence is unavailable, and `PASS` only when
the review is clean. A clean `PASS` means every path in the scope established
by `## Inputs` was traced; a review that leaves any such path not fully traced
is not clean and returns `NEEDS_ACTION` with an incomplete-trace `Residuals`
row naming the path, or `BLOCKED` when the evidence needed to trace it is
unavailable.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. Steps and rules for the dispatched
  `reviewer`.
- [`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
  `## Task brief` — task brief.
- [`../../references/subagent-handoff.md`](../../references/subagent-handoff.md)
  — shared handoff form.
