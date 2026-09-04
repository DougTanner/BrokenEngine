---
name: validate-skill
description: Validate repository skill packages with the authoritative Claude and Codex mechanical and semantic contracts. Use after creating, revising, auditing, or final-tree verifying any `.agents/skills/*/SKILL.md`, and whenever frontmatter, `agents/openai.yaml`, invocation policy, trigger quality, or bundled links need review.
allowed-tools: [Read, Grep, Glob, PowerShell]
---

# Validate Skill

## Purpose

Validate one repository skill package against the authoritative Claude and Codex
mechanical and semantic contracts, and report findings without modifying the
package.

## When to use

- After creating, revising, or auditing a `.agents/skills/*/SKILL.md`, or when
  verifying a final tree.
- When frontmatter, `agents/openai.yaml`, invocation policy, trigger quality, or
  bundled links need review.
- Dispatch per `.agents/references/subagent-reporting.md`; the fresh delegated
  `reviewer` running it may be the Step 5 combined pass of `/coherence-review`
  (`.agents/skills/coherence-review/SKILL.md`).

## Inputs

Accept one repository skill directory or its `SKILL.md`. The optional Codex `agents/openai.yaml` is validated with the package. Disposable fixtures may be outside `.agents/skills/` only with `-Fixture`.

## Handoff

Return the shared handoff from `.agents/references/subagent-reporting.md`
`## Handoffs`, with these declared extension fields:

```markdown
Status: PASS | NEEDS_ACTION | BLOCKED
Mechanical evidence:
- command, exit, decisive output
Critical findings:
- `path:line` — finding and concrete correction
- none
Recommended findings:
- `path:line` — advisory improvement
- none
Accurate checks:
- confirmed check
```

Use `PASS` only when both mechanical runs succeed and no Critical finding remains. Use `NEEDS_ACTION` for target content or semantic Critical findings. Use `BLOCKED` for setup, invocation, read, or internal-validator failures. A Critical finding blocks a passing result; a Recommended finding is advisory and does not. Per-package detail of a multi-package run stays inline unless it would exceed the size limits in `.agents/references/subagent-reporting.md` `## Handoffs`, in which case it moves to one `Temp/` file cited under `Evidence` as path plus one selector per package, leaving only the summary rows inline.

## References

- [`references/worker.md`](references/worker.md) — steps and rules for the
  dispatched reviewer.
