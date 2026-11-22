# Workflow Incidents

One row per routed friction, context-efficiency, or isolation finding, and per
accepted finding of a full `/next-plan-review` audit other than governing-scope
conformance and solution minimality. Not recorded: an objectively wrong
statement or bug — instruction text that is untrue or contradicts its owner, or
a script or skill breaking its documented contract. Cost-only findings, even
unclear but true wording, are recorded.

Key: owner plus symptom. Owner is the path of the script, `AGENTS.md`, or
`.agents/references/` file at fault, or the skill directory
(`.agents/skills/<name>`) when a skill's text is. Rows share a symptom when one
fix removes both. Main, reading no code, adds its row first, reusing a matching
symptom verbatim. Evidence: selector, result, cost or bounding mechanism;
`|` as `;`; no transcript text or path, no absolute path; the row stays
within the static-checks `line-length` limit.

At a key's third row, never sooner, main has one `implementer` run
`/create-follow-up-plans` on its rows, which deletes them once the Plan is
created, mapped, or rejected.

## Entries

| Owner | Symptom | Date | Session branch | Evidence |
| --- | --- | --- | --- | --- |
| `.agents/references/change-workflow.md` | main reads and edits a reviewed file itself for a one-line accepted finding | 2026-11-20 | claude/76739fc5-521a-4c6f-907a-53abe8ac86f7 | selector: Step 7 heading and Step 4 paragraph ranges of change-workflow.md; result: main read both ranges and added one pointer line; bounding: `/resolve-findings` `implementer` briefed with path plus selector |
| `.agents/references/change-workflow.md` | main reads and edits a reviewed file itself for a one-line accepted finding | 2026-11-20 | claude/76739fc5-521a-4c6f-907a-53abe8ac86f7 | selector: Step 5 build-timing paragraph of change-workflow.md; result: main read it and applied a re-review finding; bounding: `/resolve-findings` `implementer` briefed with path plus selector |
| `.agents/skills/coherence-review` | `## Inputs` edit permission does not say the `## Handoff` `Temp/` file is written when edits are not allowed | 2026-11-20 | claude/76739fc5-521a-4c6f-907a-53abe8ac86f7 | selector: Tier-1 combined dispatch briefed "no edits to any file"; result: reviewer wrote the `Temp/` cleanup-handoffs file and flagged the conflict; cost: main reworded the round-2 brief to "no edits to tracked files" |
| `.agents/skills/next-plan` | main reads overlapping tail of injected `SKILL.md` after host truncation | 2026-11-21 | agy/524cb3a1-b765-4407-bc2e-ade8b4f9209b | selector: `.agents/skills/next-plan/SKILL.md` lines 260-422; result: 11,198 chars entered main after 2,349-byte truncation; bounding: read only lines after the truncation point |
| `.agents/scripts/Get-SessionChangeInventory.ps1` | main prints full inventory JSON with blob hashes instead of writing `-OutputPath` | 2026-11-21 | agy/524cb3a1-b765-4407-bc2e-ade8b4f9209b | selector: `Get-SessionChangeInventory.ps1 -IncludeUntracked`; result: 5,379 chars of JSON with blob hashes entered main; bounding: pass `-OutputPath` and cite the receipt file plus entries selector |
| `.agents/skills/create-follow-up-plans` | main copies reviewer finding prose into brief instead of citing `Temp/` output path | 2026-11-21 | agy/524cb3a1-b765-4407-bc2e-ade8b4f9209b | selector: `/create-follow-up-plans` brief for sweep findings VS001 and VS002; result: copied finding text inline; bounding: cite `Temp/` review output path plus finding IDs |
