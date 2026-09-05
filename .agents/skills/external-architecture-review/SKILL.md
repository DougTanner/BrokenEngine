---
name: external-architecture-review
description: >-
  Review a code area for architectural shape: dependencies and layering,
  simulation and threading invariants, client/server and data shape, module
  cohesion, AI-generation residue, and ThirdParty replacement opportunities.
  Use only when the user explicitly requests an architecture review or an
  explicit parent workflow such as external-deep-analysis chains to it. Do not
  select it for routine code changes or general code questions.
allowed-tools: [Read, Grep, Glob, Agent, PowerShell]
---

# Architecture Review

## Purpose

Run a findings-only, multi-perspective review from the main invoking context. This skill owns cross-file shape: dependency structure, module depth, coupling, phase and thread alignment, and inter-module contracts. Hand function complexity, nesting, hot-path allocation, bool-to-flags, and file-size mechanics to `external-refactor-clean`. Exclude security auditing.

## When to use

- The user explicitly requests an architecture review.
- An explicit parent workflow such as `external-deep-analysis` chains to it.
- Never for routine code changes or general code questions.

## Inputs

Require a review scope path. If absent, ask for it.

## Handoff

Return `## Architecture Review: <target>` followed by these `###` sections, with
these exact names, in this order:

Scope and Authorities (scope mode, file count, `AGENTS.md` paths); Overview;
Dependencies and Layering; Simulation and Threading; Client/Server and Data
Shape; Cohesion and Generation Residue; ThirdParty Replacement Opportunities
(with removable size and license evidence); Cross-Cutting Concerns; Handoff to
external-refactor-clean (one concise scope note, never line-level findings);
Prioritized Recommendations (ranked, each with rationale and rough effort);
`### Architecture Health: <HEALTHY | MINOR CONCERNS | NEEDS ATTENTION |
CRITICAL>` plus a brief justification; External-Claim Residuals.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The invoking context's review steps,
  plus the lens checklists and finding contracts the dispatched reviewers run.
