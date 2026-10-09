---
name: over-engineering-review
description: >-
  Two-pass review that removes provably useless defensive code from
  session-changed C++: checks for impossible states, unreachable fallbacks,
  protection against ultra-rare events, unread hashes, and speculative
  generality. A findings-only finder pass lists removal candidates and
  root-cause design suggestions; a fresh validator pass re-proves each
  candidate and applies only the proven removals. Use at the Change Workflow
  Review and resolve correctness step after any session C++ change. Not for
  correctness defects (`/repo-code-review`), comments (`/comment-review`),
  style (`/code-style-review`), GLSL, non-C++ files, or on-demand or
  path-scoped cleanup sweeps.
allowed-tools: [Read, Grep, Glob, Edit, Write, Bash, PowerShell]
---

# Over-Engineering Review

## Purpose

Removes defensive code proven useless from session-changed C++: a finder pass
proposes candidates, a validator pass re-proves and applies each removal, and
both return root-cause design suggestions for the user.

## When to use

- The Change Workflow Review and resolve correctness step, after any session
  change to a `cpp`-class file (the inventory's `overEngineeringReview`
  trigger), at every tier.
- Not for correctness defects (`/repo-code-review`), comment content
  (`/comment-review`), style (`/code-style-review`), `dual-language-header`,
  GLSL, and other non-C++ files, or on-demand or path-scoped cleanup sweeps.

Run each pass in the delegated execution context of
[`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
`## Delegated execution context`: the finder as one fresh `reviewer`, then the
validator as one fresh `implementer`.

## Inputs

- `Pass` — `finder` or `validator`.
- `Baseline` — both passes: the full 40-character session baseline SHA and the
  absolute repository toplevel, plus any untracked paths the review must cover.
- Validator only:
  - `Removal candidates` — the finder's `Evidence` path plus selector that
    holds its candidate rows.
  - `Inventory` — the finder's inventory output file, cited by its `Evidence`;
    the validator reuses it instead of re-running the inventory.
  - `Authorization` — the approved plan's `## In scope` and `## Out of scope`
    and the execution card, or the user-instruction list for unplanned work
    ([`../../references/scope-authorization.md`](../../references/scope-authorization.md)).

## Handoff

Return the shared handoff form in
[`../../references/subagent-handoff.md`](../../references/subagent-handoff.md)
`## Handoffs`. `Findings` is `none` in both passes: a removal candidate is a
proposal the validator re-proves, and a dropped candidate is not a review
finding. `Status` is `PASS` once the pass completes, whatever its counts, and
`BLOCKED` only for a missing input, named in `Residuals`: for the finder, no
baseline or toplevel, or an inventory run whose `status` is not `pass`; for the
validator, no candidate rows, no inventory output file, or no authorization
source.

Each `Design suggestions` row describes, in plain language, a root-cause change
that would make a check unnecessary. Every row stays inline even past the shared
character cap, because main puts each one to the user.

### Finder

`Changed files` and `Build required` are `none`; `Evidence` cites
`Temp/over-engineering-review-inventory.json` and, when `Removal candidates` is
above zero, the candidate file by path plus selector, for the validator.

Declared extension fields, after `Evidence`:

```text
Removal candidates: <count>
Design suggestions: <count>
<ID> <path:line> — <the root-cause change> — makes unnecessary: <candidate IDs or path:line>
```

### Validator

`Changed files` names each applied site, and `Build required` names every
target that compiles a changed file, with its configuration/platform. Declared
extension fields, after `Evidence`:

```text
Candidates: <n>/<total> applied
<ID> dropped — <reason> — <evidence path plus selector, or the handoff row that holds it>
Design suggestions: <count>
<ID> <path:line> — <the layout or format change, with its version bump, that the removal needs> — candidate: <ID>
```

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. Finder and validator steps, candidate
  classes, proof standards, drop reasons, and protected checks.
