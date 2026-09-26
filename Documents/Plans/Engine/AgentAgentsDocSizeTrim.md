<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-26T18:26:28.883Z","dependsOn":[]} -->
# Bring Engine/Source/Agent/AGENTS.md within its size budget

## Context

`pwsh -NoProfile -File .agents/skills/update-claude-docs/scripts/Get-AffectedAgentsDocs.ps1 Engine/Source/Agent/AGENTS.md`
reports `Engine/Source/Agent/AGENTS.md` at 1830 `bt-token-v1` tokens against a
formula budget of 1676 (19301 code tokens, no child documents), verdict
`over-target`. The document's chain total (6235) is fine; only this one document
is over. These figures were measured at `693a7b0c`; primary `54e38696` then made
the document 27 bytes smaller and the code slightly larger, so by estimate it is
still over budget. Re-measure when claiming, as the acceptance criteria direct.
`.agents/skills/update-claude-docs/references/content-rules.md`
`## Removing Text` sets how to respond: remove inventories, repeated
introductions, navigation pointers, duplicated mechanics, and redundant prose
first, and never delete an operative rule or contract to meet the budget.

The overage was already present at baseline `693a7b0c`. The `/update-claude-docs`
run for `Documents/Plans/Engine/BeginScriptAndDeferErrorWindowComment.md` found
it. That Plan changes one source comment only, so the trim was left for a
separate change.

## Design

Recommended trims, all removing repeated or duplicated text rather than rules:

- `## Overview`: the long list of handlers and fixtures repeats what the
  `## Architecture` bullets say one by one. Recommended: shorten it to one
  sentence on what the Agent subsystem is, and keep the link to the project
  Agent document.
- The `ExecuteClientAgentCommand` bullet: the list of handler names is an
  inventory. Recommended: keep the ownership, dispatch order, and the rule that
  `query_profile` reads existing main-thread telemetry without forcing query
  reads, and drop the name list.
- The `ExecuteSharedAgentCommand` bullet: shorten the sentences about the
  focused modules and the shared position shape to their ownership facts, and
  keep the link to the harness command reference.

Keep every constraint, threading, teardown, lifetime, and socket rule; these
change decisions and reading the code cannot bring them back easily. If the
document is still over budget after removing repeated text, stop there and
report the remaining advisory excess, as `content-rules.md` directs.

## Critical files

- `Engine/Source/Agent/AGENTS.md`

## In scope

- Wording in `## Overview` and `## Architecture` of
  `Engine/Source/Agent/AGENTS.md`.

## Out of scope

- Any source or script change.
- `## Constraints` rules and `## See Also` targets in
  `Engine/Source/Agent/AGENTS.md`.
- Any other `AGENTS.md` or harness command reference. Text is removed only when
  it already lives in a named place; nothing moves out of this document.

## Risk tier

Tier 1 (mechanical). Trigger: documentation only. No behavior, signature,
determinism/CRC, wire, serialization, replay, threading, affinity, or build
exposure.

## Acceptance criteria

- Re-running the `Get-AffectedAgentsDocs.ps1` command above reports
  `Engine/Source/Agent/AGENTS.md` with verdict `ok`, or the remaining excess is
  reported as advisory with the operative rules that required keeping it.
- Every rule removed from the document is confirmed present in the named place
  that owns it.

## Notes

- No live verification is needed. The diff and the size script settle it.
