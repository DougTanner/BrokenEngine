---
name: glsl-review
description: >-
  Review changed Vulkan GLSL shaders and shader-facing shared headers for
  correctness, performance, and Broken Engine layout and binding contracts. Use
  after changing GLSL shader sources, includes, or dual-language headers under
  Data/Shaders, and when the user asks to review, audit, or verify shader code.
allowed-tools: [Read, Grep, Glob, Bash, PowerShell]
paths: ["**/*.vert", "**/*.frag", "**/*.comp", "**/*.geom", "**/*.tesc", "**/*.tese", "**/*.mesh", "**/*.task", "**/*.rgen", "**/*.rmiss", "**/*.rchit", "**/*.rahit", "**/*.rint", "**/*.rcall", "**/*.glsl", "**/Data/Shaders/**/*.h"]
---

# GLSL Review

## Purpose

Findings-only review of changed Vulkan GLSL shaders and shader-facing shared
headers, covering correctness, performance, and the repository's layout,
binding, and dependency contracts.

## When to use

- After changing GLSL shader sources, shader includes, or dual-language headers
  under `Data/Shaders`.
- When the user asks to review, audit, or verify shader code.

## Inputs

From the task brief (`../../references/subagent-reporting.md`, `## Task brief`):
`Baseline` for the repository root, the full 40-character SHA, and a committed
head when one applies; `Scope` for any untracked shader files to include; and
`Fixed decisions` for the change's risk tier, which decides whether the scope
authorization pass runs. Also require the authorization source: either the
approved plan's exact in-scope and out-of-scope text or an explicit list of the
authorizing user instructions, plus the execution card when one exists.

## Handoff

Order findings by severity, give each one a stable `GLSL###` ID, and omit empty
optional extension rows. Correctness, performance, and scope-authorization
failures all go in the shared `Findings` field.

Return the shared handoff form in
[`../../references/subagent-reporting.md`](../../references/subagent-reporting.md),
extended with these fields:

- `Scope` — `PASS`, `NEEDS_ACTION`, `not applicable (Tier 1)`, or
  `not supplied`.
- `External claim verification requests` — single checkable requests, or none.
- `Files reviewed` — one row per path: the regions and affected paths traced.
- `Sibling review required` — `/repo-code-review` and the shader-facing shared
  headers, or none.
- `Residuals` — pre-existing issue, incomplete review, pending external
  verdict, size observation, or none; last.

Each `Findings` row is one line on this form:

```text
GLSL### Critical|Required path:line — claim — evidence and smallest correction
```

`Changed files` and `Build required` are `none` because this findings-only
review never edits a file.

Use `NEEDS_ACTION` when a finding or pending external verdict requires action,
`BLOCKED` when required review evidence is unavailable, and `PASS` only when
the review is clean.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you are the session executing this skill. The dispatched reviewer's steps and rules.
