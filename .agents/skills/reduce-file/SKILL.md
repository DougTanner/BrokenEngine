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

Every mode returns one complete shared handoff from
[`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
after its domain extension. Keep the existing source-size and boundary evidence
in the appropriate extension and keep `Residuals` last.

### Review Observation

```text
Size observation: <file> — <n>/<threshold> bt-token-v1 — <concrete cohesive split opportunity or no qualifying reduction>
Status: PASS | NEEDS_ACTION | BLOCKED
Findings: <none>
Changed files: none
Decisive checks: <Measure-Tokens invocation and result>
Build required: none
Evidence: <measurement output or path plus selector, or none>
Executor: <own model id> <own effort>, each unknown when unreadable
Residuals: <accepted out-of-scope observation requiring /create-follow-up-plans, missing path, or none>
```

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

Then return its shared envelope:

```text
Status: PASS | NEEDS_ACTION | BLOCKED
Findings: <none>
Changed files: none
Decisive checks: <measurement, source map, caller/dependency, and project-membership checks>
Build required: none
Evidence: <source map and plan draft path plus selectors, or none>
Executor: <own model id> <own effort>, each unknown when unreadable
Residuals: <contradictory boundary evidence, missing input, or none>
```

The draft must choose the boundary, filenames, ownership, interface shape,
shared-symbol placement, and implementation order. Include all affected files
and consuming build targets; do not leave alternative designs for a later
implementer to decide.

### Approved Execution

```text
Reduction result: <original and final bt-token-v1 sizes; approved moves completed>
Runtime acceptance requests: <setup, action, observation, and required evidence per criterion, or none>
Status: PASS | NEEDS_ACTION | BLOCKED
Findings: <none>
Changed files: <exact changed file and region rows>
Decisive checks: <remeasurement and focused static/search checks>
Build required: <exact consuming targets, configuration/platform, and selected project-member .cpp rows>
Evidence: <measurement/check output or path plus selector, or none>
Executor: <own model id> <own effort>, each unknown when unreadable
Self-audit resolved: <Claim -> Check -> Result; fix/recheck, or none>
Affected-site triggers: <moved symbol/include/project-affinity pattern and search scope, or none found>
Propagation required: /update-affected-code — <C++ scope>
Reviewer focus areas: <approved boundary, ownership, includes, affinity, and behavior preservation>
Residuals: <plan contradiction, file still above threshold, or none>
```

Main schedules `/compile` and any runtime verification at their owning Change
Workflow stages; approved execution returns those exact requests and does not
claim they ran inline.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. Worker entry: the qualification,
  boundary-analysis, and execution steps, and the rules.
