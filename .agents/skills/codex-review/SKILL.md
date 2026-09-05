---
name: codex-review
description: >-
  Retained but inactive route that runs one delegated reviewer or auditor role
  on Codex headless (gpt-5.6-sol). Not part of the Change Workflow: the root
  AGENTS.md role table runs every review on the Opus `reviewer` subagent. Use
  only when the user explicitly asks to run a named review on Codex. Codex
  callers never invoke it.
disable-model-invocation: true
allowed-tools: [Read, Bash, Agent]
---

# Codex Review

## Purpose

Runs one delegated reviewer or auditor role on Codex headless (gpt-5.6-sol)
and returns its findings, without the reviewed evidence entering this session.

## When to use

Only when the user explicitly asks, in the current session, to run a named
review or audit skill on Codex. The Change Workflow never routes here: the root
[AGENTS.md](../../../AGENTS.md) role table runs every delegated review on the
Opus `reviewer` subagent, and this package is retained so the route can be
re-enabled later. A `/codex-review` invocation of an assigned skill constitutes
the delegated-`reviewer` execution context; it is not an "inline run" in the
assigned skills' vocabulary. Codex callers never invoke it.

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
authorization the brief names the assigned skill and states that the user
authorized the fallback in this session; `.claude/agents/reviewer.md` owns what
else it carries.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The dispatch steps, the fallback, and
  the conduct rules.
