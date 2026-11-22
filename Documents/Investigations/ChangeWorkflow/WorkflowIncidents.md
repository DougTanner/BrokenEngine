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
| `.agents/skills/create-follow-up-plans` | main copies reviewer finding prose into brief instead of citing `Temp/` output path | 2026-11-21 | agy/524cb3a1-b765-4407-bc2e-ade8b4f9209b | selector: `/create-follow-up-plans` brief for sweep findings VS001 and VS002; result: copied finding text inline; bounding: cite `Temp/` review output path plus finding IDs |
| `.agents/skills/next-plan` | Antigravity CLI PowerShell shell lacks `rg` when locating headings | 2026-11-22 | agy/bfc232ef-cb23-48db-a353-2bd2f14023ec | selector: `rg -n "^## " .agents/references/subagent-reporting.md`; result: exit 1 command not found; cost: repeated heading lookup with `Select-String` |
| `.agents/scripts/Invoke-Codex.ps1` | Codex read-only sandbox Git ownership check blocks inspecting primary checkout | 2026-11-22 | agy/bfc232ef-cb23-48db-a353-2bd2f14023ec | selector: `Invoke-Codex.ps1 -Skill adversarial-review`; result: sandbox Git ownership check rejected primary checkout access; cost: skipped independent end-to-end reattach |
| `.agents/skills/external-grill-plan` | `## Inputs` requires referenced Plan text in brief while `tier3-workflow.md` keeps reads in planner | 2026-11-22 | agy/bfc232ef-cb23-48db-a353-2bd2f14023ec | selector: `/external-grill-plan` dispatch brief; result: skill asked for referenced Plan text while Tier-3 workflow assigns reads to planner; cost: substituted paths for text |
| `.agents/skills/next-plan` | `tier3-workflow.md` combines accepted-finding write-in and grill without stating audit-revision match | 2026-11-22 | agy/bfc232ef-cb23-48db-a353-2bd2f14023ec | selector: `/external-grill-plan` combined write-in and grill dispatch; result: revised and grilled snapshot in one pass using prior audit; cost: revision-match ambiguity |
| `.agents/skills/next-plan` | main reads extra claimed Plan sections beyond risk tier before preparation dispatch | 2026-11-22 | agy/bfc232ef-cb23-48db-a353-2bd2f14023ec | selector: claimed Plan `## Critical files` and `## Notes`; result: main read both sections before preparation; bounding: pass heading ranges to planner and let it return scope and conditions |
| `.agents/references/codex-bridge.md` | main reads `Invoke-Codex.ps1` header and notes instead of only `$routes` lines | 2026-11-22 | agy/bfc232ef-cb23-48db-a353-2bd2f14023ec | selector: `.agents/scripts/Invoke-Codex.ps1` lines 1-120; result: 7,708 chars entered main; bounding: select only the required skill and mode rows in `$routes` |
| `.agents/skills/next-plan` | main paraphrases Plan review findings inline instead of citing `Temp/` review files by finding ID | 2026-11-22 | agy/bfc232ef-cb23-48db-a353-2bd2f14023ec | selector: `/external-grill-plan` brief decisions list; result: copied finding text inline; bounding: cite `Temp/` audit output files by finding ID with decisions only |
| `.agents/skills/next-plan` | main reads overlapping tail of injected `SKILL.md` after host truncation | 2026-11-22 | agy/4004df43-5824-4718-822f-007c9e5d0963 | selector: `.agents/skills/next-plan/SKILL.md` lines 320-422; result: 2,349-byte host clipping led main to re-read overlapping lines; bounding: read only lines after the truncation point |
| `.agents/scripts/Invoke-Codex.ps1` | broad `Select-String` for `$routes` clips output and triggers follow-up read | 2026-11-22 | agy/4004df43-5824-4718-822f-007c9e5d0963 | selector: `Select-String -Path '.agents/scripts/Invoke-Codex.ps1' -Pattern '\$routes' -Context 0,30`; result: 6,837 bytes clipped, re-read lines 95-145; bounding: match `^\$routes =` directly |
| `.agents/skills/next-plan` | main reads source files directly to judge Plan review findings instead of dispatching a locator | 2026-11-22 | agy/4004df43-5824-4718-822f-007c9e5d0963 | selector: `Invoke-Sweep.ps1` lines 45-245 and other cited files during Plan review triage; result: 12,662+ chars entered main; bounding: dispatch `locator` with finding IDs to return deciding locations |
| `.agents/references/change-workflow.md` | main reads reviewed file directly to judge review findings instead of passing finding IDs to `/resolve-findings` | 2026-11-22 | agy/4004df43-5824-4718-822f-007c9e5d0963 | selector: `.agents/skills/sweep/SKILL.md` lines 80-125 during coherence re-review; result: reviewed prose entered main; bounding: give `/resolve-findings` `implementer` finding IDs and section selector |
| `.agents/skills/next-plan` | main copies review findings or execution card checks inline into subagent briefs instead of citing `Temp/` paths and selectors | 2026-11-22 | agy/4004df43-5824-4718-822f-007c9e5d0963 | selector: Plan review write-in, pre-edit, and slice briefs; result: restated review rows and card fixtures inline; bounding: cite `Temp/` review outputs and `## Execution card` selectors with decisions |
| `.agents/skills/resolve-findings` | main copies reviewer finding prose into brief instead of citing `Temp/` output path | 2026-11-22 | agy/4004df43-5824-4718-822f-007c9e5d0963 | selector: `/resolve-findings` briefs for coherence and Step-7 rounds; result: repeated review prose inline; bounding: cite `Temp/` review out-files by finding ID plus accepted scope and decisions |
