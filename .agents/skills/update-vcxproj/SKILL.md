---
name: update-vcxproj
description: >-
  Verify or reconcile Visual Studio project/filter membership for file additions,
  removals, renames, or whole-file affinity changes using deterministic validation.
allowed-tools: [Read, Write, Edit, Grep, Glob, PowerShell]
---

# Update vcxproj Membership

## Purpose

One delegated `mechanic` owns only project/filter affinity, membership edits,
and deterministic pair validation.

## When to use

- A file addition, removal, rename, or whole-file affinity change needs Visual
  Studio project/filter membership verified or reconciled.
- An ordinary source edit that does not change which executable a whole file
  belongs to does not trigger this skill.

## Inputs

Require affected paths, change kind, session baseline, ownership snapshot, and
explicit mode:

- `verify`: read-only diagnosis;
- `fix`: reconcile only authorized additions/removals/renames/whole-file
  affinity changes, then verify.

## Handoff

Return the shared handoff form in `../../references/subagent-handoff.md`,
extended with these fields:

- `Membership outcomes` — a `<settled>/<total> settled` count line, where
  `verified` and `fixed` are settled, followed by every `NOTE` and `FAIL` path
  on the row form below.
- `Regions touched` — item groups and filter declarations, or none.
- `Reviewer focus` — authority, affinity, or XML risk; or none.

Each shared `Build required` row names the exact target/configuration/platform,
using `none` when absent. Each shared `Residuals` row names a `FAIL`, conflict,
or `NOTE` requiring action, using `none` when absent.

Each exception row cites the handoff row or path plus selector that holds its
settling evidence and never restates it. A `NOTE` may be informational; only a
`NOTE` requiring action belongs in `Residuals`.

```text
Membership outcomes: <settled>/<total> settled
<path> — <client|server|both|DataPacker|AgentHarness|WorktreeCli|AgentTools|non-member> — <project> — filter <path|none> — NOTE <detail>|FAIL <detail> — <path-plus-selector | Decisive checks row | Residuals row>
```

Use `Debug|x64` for game client/server unless approved otherwise,
`Release|x64` for DataPacker, and the AgentTools promotion route for
tool source membership. Verify-only/`None`-only membership requires no build.
Never claim a build ran.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. Worker entry: reconcile and validate
  steps, and the ownership rules.
