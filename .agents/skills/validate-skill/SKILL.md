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
  `reviewer` running it may be the Review and resolve correctness combined pass
  of `/coherence-review`
  (`.agents/skills/coherence-review/SKILL.md`).

## Inputs

Accept one repository skill directory or its `SKILL.md`, passed as `-Path`. The optional Codex `agents/openai.yaml` is validated with the package. Disposable fixtures may be outside `.agents/skills/` only with `-Fixture`.

## Handoff

Return the shared handoff from `.agents/references/subagent-reporting.md`
`## Handoffs`, with these declared extension fields:

- `Decisive checks` — one row per mechanical run: the command, its exit, and
  its decisive output.

Each `Findings` row is one line on this form:

```text
VS### Critical|Recommended path:line — finding — correction
```

Use `PASS` only when every mechanical run succeeds and no Critical finding remains. Use `NEEDS_ACTION` for target content or semantic Critical findings. Use `BLOCKED` for setup, invocation, read, or internal-validator failures. A Critical finding blocks a passing result; a Recommended finding is advisory and does not.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. Steps and rules for the dispatched
  reviewer.
