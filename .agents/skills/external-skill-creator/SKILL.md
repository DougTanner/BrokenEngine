---
name: external-skill-creator
description: Create, revise, or review repository skills. Use when the user explicitly requests external-skill-creator to define a skill workflow, improve SKILL.md instructions, design trigger descriptions or output formats, organize progressive disclosure, or audit skill quality and client compatibility.
allowed-tools: [Read, Write, Edit, Glob, Grep, Agent, Bash, PowerShell]
disable-model-invocation: true
---

# Skill Creator

## Purpose

Create or improve a skill from the user's intent, repository conventions, and evidence from existing workflows. Keep shared instructions client-neutral; isolate client syntax and invocation controls in `references/client-compatibility.md`.

## When to use

- The user explicitly requests external-skill-creator to define a skill workflow or improve `SKILL.md` instructions.
- The user explicitly requests it to design trigger descriptions or output formats, or to organize progressive disclosure.
- The user explicitly requests it to audit skill quality and client compatibility.

## Handoff

Return the shared handoff from
[`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
`## Handoffs`, extended with these fields:

- `Client evidence` — one row per intended client: client and version; structural,
  loader, observed runtime, and documentation evidence; and unverified behavior.
- `Size measurements` — one row per changed Markdown file: path, measured
  `bt-token-v1` count, and applicable threshold result.
- `Unresolved decisions` — one row per compatibility or workflow choice the
  manager must settle, or `none`.

Claim support only for the clients and behavior the evidence checks. A loader
result establishes discovery and parsing, not invocation or tool behavior.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you are the session executing this skill. The authoring steps, the repository conventions, and the writing guidance.
- [`references/validation.md`](references/validation.md) — evidence classes and conditional client checks used in the handoff.
