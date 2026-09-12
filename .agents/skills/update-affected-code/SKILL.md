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

Return the shared handoff form in `../../references/subagent-handoff.md`,
extended with these fields:

- `Trigger outcomes` — a `<settled>/<total> settled` count line, where
  `RESOLVED` and `REFUTED` are settled, followed only by `UNRESOLVED` triggers
  on the row form below.
- `Project membership trigger` — `/update-vcxproj` with the paths and reason,
  or none.
- `Reviewer focus areas` — the contract and failure condition to try to
  disprove, or none.

Each shared `Build required` row names its distinct target and that target's
configuration/platform inline in every case, using `none` when absent; only the
per-file project-member paths move with an overflow, and the row stays
executable without rediscovery wherever that detail sits.
Each shared `Residuals` row names an affected site not updated, incomplete
search, ownership conflict, or unclassified hit, using `none` when absent.

Each unresolved row cites the handoff row or path plus selector that holds its
settling evidence and never restates it:

```text
Trigger outcomes: <settled>/<total> settled
<trigger> — UNRESOLVED — owner <owner> — action <action> — <path-plus-selector | Decisive checks row | Residuals row>
```

Name each changed file once, and when the changed set would push a field past
the shared row cap give the count in `Changed files` and move the per-file rows
to the file cited under `Evidence` as path plus `##` selector, on the shared
form's terms. `PASS` requires every trigger resolved or refuted
and every planned search complete; requested builds remain `builder` work
dispatched by the manager rather than passed checks.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. Worker entry: propagation steps and
  rules.
