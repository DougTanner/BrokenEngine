---
name: update-affected-code
description: >-
  Propagate an owned set of C++ or GLSL changes to every correctness-dependent
  caller, producer, consumer, mirror, serialization identity, and CPU/GPU
  contract the implementation did not update. Use during the Implement and
  propagate stage after any C++ or GLSL change, and for candidates outside a
  scoped review-fix round. Search-and-update only; no refactoring, style work,
  or scope expansion.
allowed-tools: [Read, Edit, Grep, Glob, Bash, PowerShell]
---

# Update Affected Code

## Purpose

Propagate an owned set of C++ or GLSL changes to every correctness-dependent
caller, producer, consumer, mirror, serialization identity, and CPU/GPU contract
the implementation did not update.

## When to use

- During the Implement and propagate stage after any C++ or GLSL change.
- For propagation candidates outside a scoped review-fix round.

## Inputs

Require:

- session baseline and the exact owned changed files/regions,
  separated from pre-existing and concurrent work;
- approved plan or concise intent, applicable repository instructions, and
  implementation handoff;
- every note that another code site may be affected. Each signature, semantic,
  layout, and identity note states the old contract and new contract explicitly,
  plus its symbol/pattern and search scope. The notes for sweeping callers,
  checking client/server guards, and updating mirrored code state the invariant
  and counterpart scope.

The session-change inventory receipt, whose `triggers.vcxprojCandidates` rows
name the project-membership candidates, is optional: the worker produces it
from its documented run when the assignment supplies none.

Return `BLOCKED` without editing when the ownership boundary, controlling
intent, or a required old/new contract is missing. When the implementation
reports no triggers, inspect the owned diff and changed regions, construct the
applicable searches, and report the verified absence; do not infer it from the
handoff.

## Handoff

Return the shared handoff form in `../../references/subagent-reporting.md`,
extended with one outcome per trigger:

```text
Trigger outcomes: <trigger — RESOLVED with updated sites or verified no-op;
  REFUTED with evidence; or UNRESOLVED with owner/action>
Project membership trigger: /update-vcxproj — <paths/reason> | none
Build required: <exact targets/configuration/platform and project-member paths,
  or none>
Reviewer focus areas: <contract and failure condition to try to disprove, or none>
Residuals: <affected site not updated, incomplete search, ownership conflict,
  or unclassified hit, or none>
```

Name each changed file once. `PASS` requires every trigger resolved or refuted
and every planned search complete; requested builds remain `builder` work
dispatched by the manager rather than passed checks.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. Worker entry: propagation steps and
  rules.
