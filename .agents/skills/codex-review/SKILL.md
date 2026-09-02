---
name: codex-review
description: >-
  Primary Claude Code route running any delegated reviewer or auditor role on
  Codex/Sol headless. Codex callers stop because they are already the Sol
  mapping (see the root AGENTS.md role table). Use for every Change Workflow review dispatch in Claude Code
  except the child reviewer a parent/manager orchestrator (such as /next-plan-review) dispatches itself.
allowed-tools: [Read, Bash, Agent]
---

# Codex Review

## Purpose

Runs one delegated reviewer or auditor role (Sol on Codex, per the root
[AGENTS.md](../../../AGENTS.md) role table) headless and returns its findings,
without the reviewed evidence entering this session.

## When to use

Claude Code runs every delegated reviewer or auditor role — `plan-audit`,
`plan-simplicity-review`, `repo-code-review`, `glsl-review`,
`progressive-disclosure-review`, `adversarial-review`,
`next-plan-checkpoint-review`, `session-audit`, and external review lenses —
on Codex/Sol through this skill.
Parent/manager orchestrators that dispatch their own child reviewer, including
`/next-plan-review`, are excluded, and so is the child reviewer they dispatch:
that child routes per the delegated-review routing bullet in the root
[AGENTS.md](../../../AGENTS.md). A session that already holds the explicit
user authorization [`references/worker.md`](references/worker.md)
`### Fallback` requires, given up front because Codex is unavailable, is
excluded too: it routes the same unchanged assignment straight to the Opus
`reviewer` subagent instead of spending a dispatch known to fail. A
`/codex-review` invocation of an assigned skill constitutes the
delegated-`reviewer` execution context; it is not an "inline run" in the
assigned skills' vocabulary. Codex callers must stop instead of invoking
this skill recursively — their reviewer role already resolves to Sol.

## Inputs

- Assigned skill — the reviewer or auditor role to run, such as `plan-audit`, `repo-code-review`,
  or `session-audit`
- Its normal inputs: plan/intent, changed files and regions, and current
  residuals or reviewer focus
- Repository root — the absolute toplevel of the session worktree, defaulting to
  the current one; a relative path is accepted and resolves against the current
  directory — and session baseline (a full 40-character commit SHA)

## Handoff

Return the handoff plus the `<out>` path — the retained full critique on disk —
and do not paste extra narration beyond the concise handoff into the session.

On genuine failure the handoff is `CODEX-UNAVAILABLE: <short reason>` with the
unchanged target, blocked pending explicit user authorization per
[`references/worker.md`](references/worker.md) `### Fallback`. With that
authorization — given after such a failure, or up front per `## When to use` —
the brief names the assigned skill and states that the user authorized the
fallback in this session; `.claude/agents/reviewer.md` owns what else it
carries.

## References

- [`references/worker.md`](references/worker.md) — the dispatch steps, the
  fallback, and the conduct rules.
