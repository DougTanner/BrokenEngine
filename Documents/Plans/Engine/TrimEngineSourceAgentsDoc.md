<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-31T17:10:46.327Z","dependsOn":[]} -->
# Bring Engine/Source/AGENTS.md back within its advisory size budget

## Context

`/update-claude-docs` reported this residual while syncing an unrelated
change: `Engine/Source/AGENTS.md` is over its advisory size budget. The
discovery script
(`pwsh -NoProfile -File .agents/skills/update-claude-docs/scripts/Get-AffectedAgentsDocs.ps1 Engine/Source/AGENTS.md`)
reports for it `tokens` 3846 against `budget` 3530 (`budgetRule` `formula`,
`codeTokens` 25129, 11 direct child documents), verdict `over-target`, about
316 tokens (9%) over. The document was last changed by commit `3350148b` and
was unchanged by the session that reported it, so the excess predates that
session. The budget formula (`Get-AffectedAgentsDocs.ps1`: floor 1000, plus 35
tokens per 1000 code tokens, plus a per-child allowance) moves by only a few
tokens with small code changes, so the excess is in the document's prose, not
in its inputs.

`.agents/skills/update-claude-docs/references/content-rules.md` owns the
response: an over-budget verdict never justifies deleting an operative rule or
contract; remove inventories, repeated introductions, navigation pointers,
duplicated mechanics, and redundant prose first, and if needed guidance still
leaves the document over budget, retain it and report the advisory excess.

Crash-reporting code lives directly in `Engine/Source/` (`CrashReport.cpp`,
`CrashReport.h`, `Main.cpp`), with no subdirectory to hold a leaf document, so
`## Crash Reporting` (lines 82-162, the largest section) correctly stays in
this hub.

## Design

Recommended: tighten wording in place, without moving or deleting any
operative rule, contract, or settled finding. The largest candidates are
`## Crash Reporting`, where several sentences restate an ordering or an
ownership already given earlier in the same section, and the longer bullets of
`## Startup and Main Loop` and `## Hub Conventions`. Each removed or shortened
sentence passes the `## Removing Text` test in `content-rules.md`. If a
sentence's knowledge must leave this document, it moves to a named place
(a specific source comment in `CrashReport.cpp` or `Main.cpp`, confirmed to
exist after the edit), per that same section; the author recommends avoiding
moves, since the excess is small enough for in-place tightening.

## Critical files

- `Engine/Source/AGENTS.md`

## In scope

- Prose in `Engine/Source/AGENTS.md` sections `## Overview`,
  `## Hub Conventions`, `## Startup and Main Loop`, `## Frame Ownership`, and
  `## Crash Reporting`.

## Out of scope

- `## Subsystems` routing entries (each is the only route to a child
  document).
- Any other `AGENTS.md`, `Documents/Architecture/`, source code, and the
  budget calculation in `Get-AffectedAgentsDocs.ps1`.
- Removing or weakening any operative rule or contract, including the
  IMPORTANT engine/game contract bullet in `## Hub Conventions`.

## Acceptance criteria

- `Get-AffectedAgentsDocs.ps1 Engine/Source/AGENTS.md` reports verdict `ok`
  for `Engine/Source/AGENTS.md`; or, only if every remaining sentence is
  needed guidance under `content-rules.md`, the handoff reports the remaining
  advisory excess with that justification.
- Every rule and contract the document states before the change is still
  stated in it, or in a named source comment confirmed to exist.

## Notes

- Change Workflow tier: Tier 1 — trigger: documentation-only change with no
  code, signature, or invariant exposure.
- No determinism/CRC, serialization, replay, wire, affinity, threading,
  allocation, shader, or build exposure; no live verification needed.
