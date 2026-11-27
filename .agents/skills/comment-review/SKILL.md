---
name: comment-review
description: >-
  Findings-only review of C++ and GLSL `//` comments for boilerplate,
  change-history or hypothetical narration, navigation pointers, false claims,
  coined terms, copied values, and overlong blocks, against
  `Documents/C++StyleGuide.txt` rule 64. Use at the Change Workflow Review and
  resolve correctness step after any C++ or GLSL change, or over a
  caller-supplied path scope for a cleanup sweep. Never edits; correctness
  defects belong to `/repo-code-review` and formatting to `/code-style-review`.
allowed-tools: [Read, Grep, Glob, PowerShell]
---

# Comment Review

## Purpose

Findings on C++ and GLSL comment blocks that break the sole comment authority,
[`Documents/C++StyleGuide.txt`](../../../Documents/C++StyleGuide.txt) rule 64,
each with the shortest present-tense replacement keeping every preserved fact.

## When to use

- The Change Workflow Review and resolve correctness step, after any session
  C++ or GLSL change.
- For a comment cleanup sweep over a scope the caller supplies.
- Not for correctness or contract defects, which are `/repo-code-review` work,
  and not for formatting or code identifier naming, which are
  `/code-style-review` work.

Run in the delegated execution context of
[`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
`## Delegated execution context` as one `mechanic`. Dispatch is once per printed
step-script row.

## Inputs

- `Scope` — the C++ and shader files or directories of a caller-supplied sweep
  scope, or none to review the ranges changed in this session.
- `Baseline` — the full 40-character session baseline SHA and the absolute
  repository toplevel, required for a session-changed scope, plus any untracked
  paths the review must cover.

Use `Scope` and `Baseline` alone to set the review scope.

## Handoff

Return the shared handoff form in
[`../../references/subagent-handoff.md`](../../references/subagent-handoff.md)
`## Handoffs`, with these declared extension lines above `Findings`:

```text
Skill: comment-review
Scope: session-changed ranges | caller-supplied scope
Blocks scanned: <count>
```

Each `Findings` row is one line on this form:

```text
<ID> Medium|Low path:start-end — class: boilerplate | history | speculative | navigation | false | term | copied | dense — evidence — replacement: delete | <present-tense text> | route: /repo-code-review
```

The evidence slot opens with the flagged block's current comment text in
double quotes, each line's comment text as `scripts/Find-CommentBlocks.ps1`
defines it beside `CommentPrefixPattern`, joined by single spaces, then gives
the reason.

Example:

```text
F1 Medium Common/WindowsUtils.h:35-43 — class: boilerplate — "Parameters: rCommandLine - CreateProcessW writes into it, so it is not const. Returns: true on success. Thread-safety: none." Parameters/Returns/Thread-safety fields restate the signature at :44 — replacement: CreateProcessW writes into rCommandLine, so it cannot be const.
```

`Changed files` and `Build required` are `none` because this findings-only
review never edits a file.

The manager decides each finding on whether the failure is concrete and
meaningful under the standard defaults.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. Steps and rules for the dispatched
  `mechanic`, and the class reference it reads.
