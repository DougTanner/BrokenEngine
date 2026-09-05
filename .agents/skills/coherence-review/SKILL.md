---
name: coherence-review
description: >-
  Review session-changed non-C++ artifacts for semantic coherence as the fresh
  `reviewer` role of the Change Workflow Review and resolve correctness step, in
  one of two modes: the Tier-1 combined pass that also carries the Apply the
  triggered cleanup and Verify the acceptance table steps, and the coherence pass
  every other change uses. Use for every Review and resolve correctness review of
  a changed non-C++ artifact type. Findings only, apart from the
  meaning-preserving wording and formatting fixes a dispatch that allows edits
  requires.
allowed-tools: [Read, Grep, Glob, Edit, PowerShell]
---

# Coherence Review

## Purpose

Review the changed bytes of the non-C++ artifact types a Review and resolve
correctness dispatch owns for semantic coherence, and in Tier-1 mode carry the
Change Workflow Review and resolve correctness, Apply the triggered cleanup, and
Verify the acceptance table steps in that one dispatch.

## When to use

- Tier-1 combined mode: the Review and resolve correctness combined pass, under
  the condition the `/coherence-review` combined-pass bullet in the root
  [AGENTS.md](../../../AGENTS.md) Review and resolve correctness step states.
- Coherence mode: every Tier-2+ change with a changed non-C++ artifact type, and
  every Tier-1 change with a changed non-C++ artifact type that pass does not
  cover. These keep their separate Apply the triggered cleanup and Verify the
  acceptance table dispatches unchanged, and the scope, minimality, and
  simplicity checks run only at Tier 2+.
- The Verify and land step's landing acceptance table is never folded into
  either mode; see
  [`/verify-acceptance`](../verify-acceptance/SKILL.md) `## When to use`.

## Inputs

- Tier 2+ only: the authorization source for the scope checks, as
  [`scope-authorization.md`](../../references/scope-authorization.md)
  "Authorization source:" defines it.
- The changed files and the regions to review.
- The tier and the mode; in Tier-1 mode, which Apply the triggered cleanup
  checks the change triggers and whether the Verify the acceptance table step
  applies.
- Whether the dispatch allows edits. A `/codex-review` dispatch never does.

## Handoff

Return the shared handoff from
[`subagent-reporting.md`](../../references/subagent-reporting.md) `## Handoffs`,
with these declared extension fields:

- `Coherence` — one line per semantic finding, naming its shared `Findings` row
  by ID and adding only what that one-line row cannot carry, or an explicit
  no-finding statement. Outside the verbatim cleanup handoffs below, files read
  and checks that passed get no block of their own: a check whose result decided
  something is a `Decisive checks` row, and everything else is omitted.
- `Apply the triggered cleanup` — Tier-1 mode only; one row per triggered
  cleanup skill, naming the skill, its status, and the `Findings` row IDs it
  reports.
- `Criteria` — as [`/verify-acceptance`](../verify-acceptance/SKILL.md)
  `## Handoff` defines it; Tier-1 mode only, omitted when the Verify the
  acceptance table step does not apply.

In Tier-1 mode no check is dropped: each component keeps its owning skill, its
own result block, and its own pass condition; only the dispatch is shared. Each
triggered cleanup skill's own complete handoff goes verbatim into one `Temp/`
file, cited under `Evidence` as that path plus one selector per skill, with that
skill's `Apply the triggered cleanup` row inline. Never summarize, reformat, or
merge them into the fields above. A finding more than one triggered skill
reports appears once as a shared `Findings` row, and each skill's verbatim
handoff and `Apply the triggered cleanup` row names it by that ID instead of
restating it.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The review steps and rules the
  dispatched reviewer runs.
