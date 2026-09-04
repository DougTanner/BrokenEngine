---
name: coherence-review
description: >-
  Review session-changed non-C++ artifacts for semantic coherence as the fresh
  `reviewer` role of Change Workflow Step 5, in one of two modes: the Tier-1
  combined pass that also carries Steps 6 and 7, and the coherence pass every
  other change uses. Use for every Step 5 review of a changed non-C++ artifact
  type. Findings only, apart from the meaning-preserving wording and formatting
  fixes a dispatch that allows edits requires.
allowed-tools: [Read, Grep, Glob, Edit, PowerShell]
---

# Coherence Review

## Purpose

Review the changed bytes of the non-C++ artifact types a Step-5 dispatch owns
for semantic coherence, and in Tier-1 mode carry Change Workflow Steps 5, 6, and
7 in that one dispatch.

## When to use

- Tier-1 combined mode: the Step 5 combined pass, under the condition the
  `/coherence-review` combined-pass bullet in root
  [AGENTS.md](../../../AGENTS.md) Step 5 states.
- Coherence mode: every Tier-2+ change with a changed non-C++ artifact type, and
  every Tier-1 change with a changed non-C++ artifact type that pass does not
  cover. These keep their separate Step 6 and Step 7 dispatches unchanged, and
  the scope, minimality, and simplicity checks run only at Tier 2+.
- Step 8's landing acceptance table is never folded into either mode; see
  [`/verify-acceptance`](../verify-acceptance/SKILL.md) `## When to use`.

## Inputs

- Tier 2+ only: the authorization source for the scope checks, as
  [`scope-authorization.md`](../../references/scope-authorization.md)
  "Authorization source:" defines it.
- The changed files and the regions to review.
- The tier and the mode; in Tier-1 mode, which Step 6 checks the change triggers
  and whether Step 7 applies.
- Whether the dispatch allows edits. A `/codex-review` dispatch never does.

## Handoff

Return the shared handoff from
[`subagent-reporting.md`](../../references/subagent-reporting.md) `## Handoffs`,
with these declared extension fields:

- `Coherence` — one line per semantic finding, naming its shared `Findings` row
  by ID and adding only what that one-line row cannot carry, or an explicit
  no-finding statement. Outside the verbatim Step 6 handoffs below, files read
  and checks that passed get no block of their own: a check whose result decided
  something is a `Decisive checks` row, and everything else is omitted.
- `Criteria` — as [`/verify-acceptance`](../verify-acceptance/SKILL.md)
  `## Handoff` defines it; Tier-1 mode only, omitted when Step 7 does not apply.

In Tier-1 mode no check is dropped: each component keeps its owning skill, its
own result block, and its own pass condition; only the dispatch is shared. Each
triggered Step 6 skill returns its own complete handoff verbatim — inline when
the whole combined handoff stays within the shared cap in
`subagent-reporting.md` `## Handoffs`, otherwise in one `Temp/` file cited under
`Evidence` as path plus one selector per skill, with each skill's status and
findings rows kept inline and labelled with the skill name. Never summarize,
reformat, or merge them into the fields above.

## References

- [`references/worker.md`](references/worker.md) — the review steps and rules
  the dispatched reviewer runs.
