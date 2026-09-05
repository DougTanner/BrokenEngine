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

Report changed package files, decisive validation evidence, token measurements, and unresolved compatibility or workflow decisions. Do not claim cross-client support unless each intended client's controls were configured and checked.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you are the session executing this skill. The authoring steps, the repository conventions, and the writing guidance.
