---
name: save-plan
description: Save an explicitly supplied complete plan-mode proposal into the correct Broken Engine planning tree. Documents/Plans receives Git-backed metadata; Documents/Features remains manual.
argument-hint: [PascalCase.md]
allowed-tools: [Read, Write, Edit, Glob, Grep, PowerShell, Bash]
disable-model-invocation: true
---

# Save Plan

## Purpose

Persist exactly one complete client-supplied proposal.

## Inputs

For Codex use the latest complete `<proposed_plan>` body; for Claude use the explicitly supplied absolute plan-file path. Do not discover or infer a source from a client-local plan store.

## Handoff

Report source, selected path, duplicate outcome, tier trigger, dependency decision, validation command/result where applicable, and changed files. A saved Plan is tracked Git content, not a queue change and not a claim.

### Landing pre-approval

Invoking this skill is the user's explicit approval to commit or land the saved file (Plan or Feature) to the primary branch: its content was already approved during planning, and only that file lands. Per the authoritative landing-confirmation contract (`../finalize-changes/SKILL.md`, "Landing confirmation"), when the diff that changes primary consists solely of the saved file, this invocation is a yes given in advance that stays in effect, so `/finalize-changes` proceeds without asking the confirmation question; any additional changed file in the diff voids the exception and the entire change uses the landing gate.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. Worker entry: the body, review,
  classification, and write steps, and the rules.
