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
authorization pass runs.

## Handoff

Order findings by severity and omit empty sections:

```markdown
## GLSL Review Results

### Findings
- P1 `path:line` — failure, reachable evidence, and smallest correction

### Scope
- `path:lines` — **unauthorized | overbuilt | kiss:** <cited clause or absent authorization, evidence>

### External Claim Verification Requests
<single checkable requests>

### Files Reviewed
- `path`

### Recommendation
PASS | NEEDS_ACTION

Functions/regions touched: none
Scope: PASS | NEEDS_ACTION | not applicable (Tier 1) | not supplied
```

Follow those extension fields with the shared handoff lines (`../../references/subagent-reporting.md`, `## Handoffs`); this findings-only review never changes a file and never requires a build, and pending verification, a pre-existing issue, or an incomplete review item belongs in `Residuals`.

If no issue is found, return `PASS — no issues found`, list the files reviewed, and include the unchanged footer.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you are the session executing this skill. The dispatched reviewer's steps and rules.
