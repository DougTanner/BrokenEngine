---
name: reduce-file
description: >-
  Analyze or reduce oversized C++ headers and implementation files. Use for a
  standalone /reduce-file request, when preparing a decision-complete reduction
  plan, or when an approved plan assigns a file-size reduction. During code
  review, report only a qualifying size observation and defer planning.
argument-hint: <file-path>
allowed-tools: [Read, Write, Edit, Glob, Grep, PowerShell]
---

# Reduce File

## Purpose

Find the smallest cohesive boundary that brings an oversized C++ file below its
threshold without scattering ownership or disguising size.

## When to use

- A standalone `/reduce-file` request.
- Preparing a decision-complete reduction plan.
- An approved plan assigns a file-size reduction.

## Inputs

Choose exactly one mode from the invocation:

- Review observation: While reviewing a change, measure the modified file
  and apply the review's qualification rules. Report only the size and concrete
  cohesive split opportunity. Do not map the file, draft a plan, or edit code;
  route an accepted out-of-scope residual through `/create-follow-up-plans`.
- Standalone or pre-approval analysis: Inspect the target and return one
  evidence-backed meaningful plan choice. Do not ask the user to select among
  speculative options. Produce the decision-complete draft inline; do not write
  or queue a plan unless the user explicitly authorizes that action.
- Approved-plan execution: Treat the approved plan and deltas as the decision
  authority. Implement its assigned reduction without reopening settled design
  choices. Stop and report a contradiction if repository evidence invalidates a
  meaningful plan assumption.

If no path is supplied, ask for one.

Require the target path and mode, applicable threshold, session baseline and
ownership snapshot when tracked changes are possible, approved scope and
acceptance criteria, and consuming build targets. Approved-plan execution also
requires the approved plan and deltas plus any runtime-observable acceptance
criteria.

## Handoff

Every mode below except Review Observation returns one complete shared handoff
from
[`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
after its domain extension. Keep the existing source-size and boundary evidence
in the appropriate extension and keep `Residuals` last.

### Review Observation

This mode returns no handoff of its own. The reviewing skill reports the
observation as one `size observation` row in its own `Residuals`:
`<file> — <n>/<threshold> bt-token-v1 — <concrete cohesive split opportunity or
no qualifying reduction>`.

### Standalone Plan Draft

Return one recommended-design extension:

```markdown
## File Reduction: <file>
**Measured size:** <n>/<threshold> bt-token-v1 — **Classification:** <concrete class | template | static-method struct | other>
### Evidence and boundary
### Design
- <files retained/created, exact moves, ownership/interface/shared-symbol/include/affinity decisions, ordered buildable steps>
### Expected sizes
- `<file>`: ~<n> bt-token-v1 (from <n>)
### Critical files
### Out of scope
### Risks and verification
- <risk> -> <decisive check>
```

Then return the shared handoff form with `Changed files: none`,
`Build required: none`, and `Residuals: <contradictory boundary evidence,
missing input, or none>`.

The draft must choose the boundary, filenames, ownership, interface shape,
shared-symbol placement, and implementation order. Include all affected files
and consuming build targets; do not leave alternative designs for a later
implementer to decide.

### Approved Execution

- `Reduction result` — the original and final `bt-token-v1` sizes, and the
  approved moves completed.
- `Residuals` — plan contradiction, file still above threshold, or none; last.

Return the `/implement-plan` extension fields with it, including
`Runtime acceptance requests` —
[`../implement-plan/SKILL.md`](../implement-plan/SKILL.md) `## Handoff` — with
`Reviewer focus areas` naming the approved boundary, ownership, includes,
affinity, and behavior preservation.

Main schedules `/compile` and any runtime verification at their owning Change
Workflow stages; approved execution returns those exact requests and does not
claim they ran inline.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. Worker entry: the qualification,
  boundary-analysis, and execution steps, and the rules.
