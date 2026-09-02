---
name: update-claude-docs
description: >-
  Synchronize AGENTS.md documentation and sibling CLAUDE.md import stubs after
  every C++ or GLSL change, and for an explicit request to update AGENTS.md.
  Also use for an explicit AGENTS.md audit or audit-and-fix request; audits
  report only unless improvement edits were already authorized.
allowed-tools: [Read, Edit, Write, Grep, Glob, Bash, PowerShell]
---

# Update AGENTS.md Documentation

## Purpose

Keeps AGENTS.md guidance and its sibling `CLAUDE.md` import stubs matching the
current code, or grades a requested AGENTS.md scope. Runs inside one delegated
`implementer`; never delegate, and return any separate-role requirement to the
manager.

## When to use

Use one of these modes:

- Sync (default): inspect documentation governed by a caller-supplied changed-file list and session baseline. Edit only when affected guidance is stale or a durable invariant is missing.
- Audit: grade the requested AGENTS.md scope and report findings without edits.
- Audit and fix: grade first, then improve only the scope whose edits were explicitly authorized in the request. Do not add another approval pause.

An audit request that asks only to report, assess, review, or grade never
authorizes fixes.

## Inputs

Require the caller's complete changed-file list and session baseline. Treat the list as authoritative scope and that commit as the attribution point. Never infer session scope from `git status`, a dirty-tree diff, a moving merge base, or unrelated working-tree changes. If either required input is absent, request only the missing input.

For an explicit documentation-only request, use the named AGENTS.md or directory paths as the changed-file list while retaining the session-baseline requirement.

Exclude `CLAUDE.local.md` and other local overrides unless the user explicitly includes them. Ask for direction only when missing scope, a missing session baseline, or a documentation conflict would meaningfully change the result.

## Handoff

Return the shared handoff form in
[`subagent-reporting.md`](../../references/subagent-reporting.md)
`## Handoffs` in every mode, with these rows:

- `Changed files` — one row per edited `AGENTS.md` or `CLAUDE.md` path and the document section touched, or `none`.
- `Build required: none` — this skill changes no C++ or GLSL.
- `Residuals` — conflict, deletion candidate, or pre-existing excess, or `none`; last.

Audit and audit-and-fix modes return the quality report defined in
`references/audit-mode.md` before the handoff.

## References

- [`references/worker.md`](references/worker.md) — worker entry: the steps and
  rules for every mode, and the references they use.
