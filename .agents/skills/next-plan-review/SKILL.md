---
name: next-plan-review
description: Review a landed change from its Git commit and proven parent/child session transcripts. Use when prioritized process improvements are wanted for plan/objective conformance, solution minimality/overengineering, review/testing coverage, main-session token efficiency, workflow friction, or landing speed, including when a tooling-friction Plan directs this session to run `/next-plan-review <review ref>`.
user-invocable: true
argument-hint: "[commit-ish]"
allowed-tools: [Read, Grep, Glob, Agent, PowerShell]
shell: powershell
---

# Next Plan Review

## Purpose

Audit one completed landing read-only, producing an evidence-based,
priority-sorted improvement backlog; never retry the change, edit files, alter
Plan claims, or inspect unrelated sessions. One fresh `reviewer` executes the
audit directly.

## When to use

Dispatch one fresh `reviewer` directly, routed per the Change Workflow
`### Delegation roles` rule.

### Bounded friction mode

This mode runs only when the invoker names bounded friction mode and the one
recorded friction to root-cause, the form the `/create-follow-up-plans`
tooling-friction template emits. Every other invocation, a landing review
included, runs the full audit in [`references/worker.md`](references/worker.md).

## Inputs

- The commit-ish argument, default `HEAD`.
- The optional source client whose session produced the landing or recorded
  friction; recorded provenance supplies it when available.
- The optional exact session ID.
- The optional bounded-friction-mode designation with its one recorded friction.
- The optional usage-analysis request, which the caller supplies only when the
  user explicitly asked for it.
- The authoritative shared task-brief fields from
  [`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
  `## Task brief` and `## Session baseline`,
  including repository identity and session baseline.

Run the finder once, adding `-SessionId <exact-id>` when supplied.

```powershell
pwsh -NoProfile -NonInteractive -File .agents/skills/next-plan-review/scripts/Find-AgentSessionTranscript.ps1 -RepositoryRoot '<absolute repository root>' -Commit <requested commit>
```

Run the projection once per applicable Claude or `agy` parent.

```powershell
pwsh -NoProfile -NonInteractive -File .agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1 -TranscriptPath '<parent transcript path>'
```

Before launch with inaccessible stores, stage bounded, redacted production
records and matching projection rows for applicable candidates by client,
session ID, and event or line selector. Stage more only for the producing
parent and proven children by source identity. The reviewer independently
verifies each proof and uses only matching evidence.

A producing parent started before and spanned the commit and recorded
finalization producing its full hash. An ordinary child needs the parent's
own delegation event and fixed return window.

In full reviews with an explicit usage request, run once per proven Claude or
`agy` transcript. For inaccessible stores, the host runs on originals before
launch and supplies parsed results by client and `## Handoff` label.

```powershell
pwsh -NoProfile -NonInteractive -File .agents/skills/next-plan-review/scripts/Get-TranscriptUsage.ps1 -TranscriptPath '<path>' | ConvertFrom-Json
```

Use the proven parent's path. Use literal child paths only as helper arguments.
For Claude, derive
`<parent transcript dir>/<parent session ID>/subagents/agent-<agentId>.jsonl`
from the parent tool-result's `toolUseResult.agentId` (`^[A-Za-z0-9]+$`).
For `agy`, import `./.agents/scripts/AgentTranscriptCommon.psm1`.
Call `Resolve-AgentAgyTranscriptPath -SessionId` with the proven
`invoke_subagent` `GENERIC` `conversationId` and any finder root override
as `-BrainRoot`. Use `Path` only for `safe` and matching `SessionId`.

## Handoff

Require the shared handoff form in
`../../references/subagent-handoff.md`, extended with:

- `Timeline` — one row per lifecycle event named in `references/concerns.md`
  that occurred, in time order, each `<time> | <event> | <citation>`.
- `Root cause` — up to seven rows, in time order, each
  `<event> | <evidence> | <session ID> <timestamp or record>`.
- `Assessment` — one row per assessed concern,
  `<concern> | <verdict> | <one-sentence basis> | <citation>`.
- `Observed usage` — present only when the usage-analysis request was made,
  absent in bounded friction mode, and `none` when provenance is `BLOCKED`. Its
  first row states that the counters are observed transcript usage, distinct
  from main-context chars, elapsed time, and billed cost. Then one row per
  transcript and model,
  `<transcript label> | <status> | <model> | <messageCount> | <inputTokens> | <outputTokens> | <cacheReadInputTokens> | <cacheCreationInputTokens>`,
  with `<status>` `complete` or `partial`; one
  `<transcript label> | partial | none` row for a `partial` result with no
  resolved model; and one `<transcript label> | unsupported (<reason>)` row per
  unsupported transcript. The label is the parent's session ID, a Claude child's
  `<parent session ID>/agent-<agentId>`, or an `agy` child's or Codex row's own
  session ID; child rows come after the parent's rows and are never merged with
  them.

The shared `Decisive checks` rows carry provenance, sessions read, sourced
timeline, pauses, and conformance, minimality, and process evidence. Each
shared `Residuals` row names a missing transcript or unverifiable fact, using
`none` when absent.

`Changed files` and `Build required` are `none` because this retrospective
changes no file.

Use `NEEDS_ACTION` when the retrospective produces actionable findings, `PASS`
only when it is clean, and `BLOCKED` when required evidence is missing.
Main presents the retrospective, decides each returned finding, and owns every
follow-up action. After a full audit, main records each finding it accepts in
`Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md` as its header
directs.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The direct review, provenance,
  measurement, assessment, and evidence-validation steps and rules.
