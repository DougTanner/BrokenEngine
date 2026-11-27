<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-26T02:34:18.379Z","dependsOn":[],"agentCli":"claude"} -->
# Shrink subagent-reporting.md Task brief section by promoting non-template subtopics

## Context
Three `/next-plan` runs flagged full reads of
`.agents/references/subagent-reporting.md` `## Task brief` (lines 24-111, 88
lines / ~5,169 chars) in the main session before preparation dispatch:

- 2026-11-25 (`agy/d7526070-354e-4684-946f-69451661d537`): main read lines
  24-111 to compose the step 4 brief.
- 2026-11-25 (`agy/8546d4d8-447a-43bf-bb88-7607c7955b01`): main read lines
  24-111 (5,169 chars) for the brief schema and `/next-plan` identity mapping.
- 2026-11-25 (`agy/67048fc4-9412-489e-bec4-ad39620046af`): main read lines
  24-111, pulling in unrelated session-baseline, pre-dispatch inventory,
  bounded-reading, and worker-verification prose.

In the current tree, `.agents/skills/next-plan/SKILL.md` step 4 (lines 66-68)
cites `.agents/references/subagent-reporting.md` `## Task brief` (step 6, lines
130-134, already cites `claim-results.md` instead). Because `## Task brief`
currently bundles four distinct topics (the brief template and `/next-plan`
identity mapping, session baseline and `Get-AgentWorktreeSessionContext`,
pre-dispatch inventory and bounded-reading exemptions, and worker verification
conduct), it exceeds the small-read floor in
`.agents/skills/next-plan-checkpoint-review/scripts/Get-TranscriptProjection.ps1`
(`$script:SmallReadMaxLines` and `$script:SmallReadMaxChars`).

`.agents/skills/next-plan/SKILL.md` is already over the skill token threshold,
so shrinking `## Task brief` in place by promoting the unrelated subtopics to
sibling `##` sections (and moving worker verification conduct into
`## Delegated execution context`) brings `## Task brief` within both small-read
size limits without editing `.agents/skills/next-plan/SKILL.md` or adding
sub-headings inside `## Task brief`. Note that `Get-TranscriptProjection.ps1`
exempts a small read only when no second read of the same file occurs in the
same checkpoint span; if step 4 also follows `prepare-change/SKILL.md`
`## Inputs` into `## Pre-dispatch inventory and bounded reading`, avoiding a
repeat read of `subagent-reporting.md` is a separate check from size.

## Design
1. Restructure `.agents/references/subagent-reporting.md` with no net prose
   growth (reorganizing existing paragraphs only):
   - Move the worker verification paragraph (currently lines 109-111, "Workers
     verify supplied hints against the tree...") to the end of
     `## Delegated execution context` (currently lines 3-22), which already
     owns reporting-local worker conduct.
   - Keep `## Task brief` holding only:
     - the opening line, fenced brief template, and `Required sections` /
       readable-review-outputs paragraph (currently lines 26-45),
     - the `/next-plan` `Get-NextPlanContext` identity-mapping paragraph
       (currently lines 65-71), and
     - the empty-fields / `Get-AffectedAgentsDocs.ps1` / no-pasting paragraph
       (currently lines 101-107).
     This leaves `## Task brief` at ~38 lines / ~1,940 chars, well within
     `Get-TranscriptProjection.ps1`'s `$script:SmallReadMaxLines` and
     `$script:SmallReadMaxChars` limits.
   - Promote the session-baseline and `Get-AgentWorktreeSessionContext`
     paragraphs (currently lines 47-63 and lines 73-80, keeping "The leading
     `./` is required..." and "That output's `SessionId`..." directly after
     the `Import-Module` PowerShell block) into a new sibling section
     `## Session baseline` immediately after `## Task brief`.
   - Promote the pre-dispatch `Get-SessionChangeInventory.ps1` receipt and
     whole-read / bounded-reading paragraph (currently lines 82-99) into a new
     sibling section `## Pre-dispatch inventory and bounded reading`
     immediately after `## Session baseline`.
2. Leave `.agents/skills/next-plan/SKILL.md` untouched: its existing step 4
   citation (lines 66-68) to `subagent-reporting.md` `## Task brief` now
   targets the shrunk ~38-line section.
3. Update only the repository callers that specifically cite
   `subagent-reporting.md` `## Task brief` (or `#task-brief`) for one of the
   promoted subtopics:
   - `AGENTS.md` (lines 97-99, `## Directives` `Bounded reading`): change
     `## Task brief` whole-read exemptions to
     `## Pre-dispatch inventory and bounded reading` whole-read exemptions
     (leave lines 33-34 citing `## Task brief`).
   - `.agents/skills/prepare-change/SKILL.md` (lines 53-54, `## Inputs`
     `mode: claimed-plan`): change `subagent-reporting.md` `## Task brief` for
     workflow-reference reads to
     `## Pre-dispatch inventory and bounded reading` (leave lines 33-34, 41, and
     143-144 citing `## Task brief`).
   - `.agents/skills/eod/SKILL.md` (lines 33-35, `## Steps` step 4): change
     the `Get-AgentWorktreeSessionContext` citation from `## Task brief` to
     `## Session baseline`.
   - `.agents/skills/next-plan/references/claim-results.md` (lines 16-18,
     `## Session baseline and the sync object`): change
     `../../../references/subagent-reporting.md#task-brief` to
     `../../../references/subagent-reporting.md#session-baseline`.
   - `.agents/skills/next-plan-review/SKILL.md` (lines 40-43, `## Inputs`):
     cite both `## Task brief` and `## Session baseline` on the bullet naming
     shared task-brief fields including repository identity and session
     baseline.
4. Leave all other `subagent-reporting.md` `## Task brief` citations unchanged,
   as they refer to the brief template, field rules, or readable-review-output
   citation rule that remain in `## Task brief`.

## Critical files
- `.agents/references/subagent-reporting.md`
- `AGENTS.md`
- `.agents/skills/prepare-change/SKILL.md`
- `.agents/skills/eod/SKILL.md`
- `.agents/skills/next-plan/references/claim-results.md`
- `.agents/skills/next-plan-review/SKILL.md`

## In scope
- `.agents/references/subagent-reporting.md`: `## Delegated execution context`
  (lines 3-22), `## Task brief` (lines 24-111), and new sibling sections
  `## Session baseline` and `## Pre-dispatch inventory and bounded reading`
- `AGENTS.md`: `## Directives` `Bounded reading` bullet (lines 97-101)
- `.agents/skills/prepare-change/SKILL.md`: `## Inputs` `mode: claimed-plan`
  ordinary-claim bullet (lines 50-56)
- `.agents/skills/eod/SKILL.md`: `## Steps` step 4 (lines 33-37)
- `.agents/skills/next-plan/references/claim-results.md`:
  `## Session baseline and the sync object` (lines 16-18)
- `.agents/skills/next-plan-review/SKILL.md`: `## Inputs` shared task-brief
  fields bullet (lines 40-43)

## Out of scope
- `.agents/skills/next-plan/SKILL.md` (explicitly unchanged)
- Any other skill, reference, or script citing `subagent-reporting.md`
  `## Task brief` for the brief template, field rules, or review-output
  citations
- Rewriting the rules inside the moved paragraphs beyond minimal heading
  transitions, or changing `Get-TranscriptProjection.ps1` thresholds
- Any transcript path or transcript text in the repository

## Risk tier and invariants
Tier 1 (mechanical documentation restructure with no behavioral change to
scripts, tools, or build/bootstrap coordination; net prose stays flat).
Invariants:
- Every rule currently in `subagent-reporting.md` `## Task brief` remains
  intact in its new section with no dropped requirements.
- Every citation across the repository to `subagent-reporting.md` sections or
  anchors resolves to the section owning the cited rule.
- Never embed transcript paths or home paths.

## Acceptance criteria
- `.agents/references/subagent-reporting.md` `## Task brief` contains only the
  brief template, field and review-output rules, `/next-plan` identity mapping,
  and governing-path lookup rules, and fits within
  `Get-TranscriptProjection.ps1`'s `$script:SmallReadMaxLines` and
  `$script:SmallReadMaxChars` limits.
- `.agents/references/subagent-reporting.md` has sibling sections
  `## Session baseline` and `## Pre-dispatch inventory and bounded reading`,
  and `## Delegated execution context` holds the worker verification paragraph.
- `.agents/skills/next-plan/SKILL.md` is unmodified.
- The five caller citations in `## In scope` point to the matching new section
  or anchor, and no remaining `subagent-reporting.md` `## Task brief` or
  `#task-brief` citation in the repository points to a moved subtopic.
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing.

## Notes
- Originally converted from two archived rows in
  `Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md` at commit
  `c0f69b15e` (lines 106 and 110) for key `(.agents/skills/next-plan, step 4
  cites full Task brief section instead of dedicated brief-template selector)`
  (removed from the current ledger in commit `6fd9c57dc`), plus the third
  observation recorded in the original Plan at commit `6fd9c57dc` (lines
  13-16 of this file).
- Revised to specify Option 2 (promoting non-template subtopics to sibling `##`
  sections) so `## Task brief` fits the small-read size limits without editing
  `.agents/skills/next-plan/SKILL.md`.
