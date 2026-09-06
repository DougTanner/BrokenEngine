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

Return the shared handoff form in `../../references/subagent-reporting.md`,
extended with the per-path membership outcome, one row per path on this form:

```text
<path> — <client|server|both|DataPacker|AgentHarness|WorktreeCli|AgentTools|non-member> — <project> — filter <path|none> — verified|fixed|NOTE <detail>|FAIL <detail>
```

and with these fields:

- `Regions touched` — item groups and filter declarations, or none.
- `Build required` — the exact target/configuration/platform, or none.
- `Reviewer focus` — authority, affinity, or XML risk; or none.
- `Residuals` — a FAIL, conflict, or NOTE requiring action; or none; last.

Use `Debug|x64` for game client/server unless approved otherwise,
`Release|x64` for DataPacker, and the AgentTools promotion route for
tool source membership. Verify-only/`None`-only membership requires no build.
Never claim a build ran.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. Worker entry: reconcile and validate
  steps, and the ownership rules.
