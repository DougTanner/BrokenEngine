<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-07T17:24:32.859Z","dependsOn":[]} -->
# Fix: /resolve-findings — handoff re-declares shared `Build required` and `Residuals`

## Context

`.agents/skills/resolve-findings/SKILL.md` `## Handoff` tells the worker to
return the shared handoff form from `.agents/references/subagent-reporting.md`
and then lists extension bullets after its item table that include
`- Build required — ...` and `- Residuals — ...`. Both of those are fields the
shared form already carries.

Observed symptom in this session: the `/resolve-findings` worker returned
`Build required: none` and `Residuals: none` inside the shared form, and again
as `- Build required — none.` and `- Residuals — none.` in the bullet list after
the item table. The result was two fields present twice and a shared `Residuals`
that was not last, which the manager had to read past and reconcile before
routing the handoff.

`.agents/references/subagent-reporting.md` `## Handoffs` states that a skill
extends the form only by adding rows inside an existing field or by declaring
extra fields, never by re-rendering the form itself, and that "`Build required`
stays present and `Residuals` stays last". The skill's own extension list is
what produced the duplication, so the wording is the defect rather than worker
conduct.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 69bbf8b5-adb7-4a69-819f-cb2508d8fbc0
- Worktree/branch UUID: 9698e458-3242-4687-bda8-debbe6e9fd4a
- Session branch: claude/9698e458-3242-4687-bda8-debbe6e9fd4a
- Worktree: .claude\worktrees\BrokenEngine\9698e458-3242-4687-bda8-debbe6e9fd4a
- Landing ref: claude/9698e458-3242-4687-bda8-debbe6e9fd4a
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design

First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <landing ref>` in bounded friction mode, supplying the
recorded client and conversation session ID above. Then make the smallest fix
inside the `## In scope` boundary below.

The author's recommendation, to be confirmed by that root-cause pass: delete the
`Build required` and `Residuals` bullets from the `## Handoff` extension list in
`.agents/skills/resolve-findings/SKILL.md`, so the skill declares only fields the
shared form does not already own. Where either deleted bullet carries detail the
shared form does not state — the `Build required` bullet's requirement to name
the selected project-member `.cpp` and, for headers, every consuming target and
configuration/platform — the recommendation is to preserve that detail as
row-form guidance for the existing shared field rather than as a re-declared
field, which `.agents/references/subagent-reporting.md` `## Handoffs` permits.
If root-causing shows the fix lies outside the boundary below, surface it for
re-planning instead of expanding scope.

## Critical files

- `.agents/skills/resolve-findings/SKILL.md` — `## Handoff`
- `.agents/references/subagent-reporting.md` — `## Handoffs`, read-only
  authority for the shared form

## In scope

- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `## Handoff` section of
  `.agents/skills/resolve-findings/SKILL.md`

## Out of scope

- The landed change the session produced
- Changing `.agents/references/subagent-reporting.md` or the shared handoff form
- Other extension fields in `/resolve-findings` `## Handoff` that the shared form
  does not already own
- Unrelated skills and scripts; any transcript path or transcript text in the
  repository

## Risk tier and invariants

Expected Tier 2: the edit changes an output/handoff contract, which
`/plan-simplicity-review` `### Trigger: when a skill edit is behavior` classifies
as behavior rather than documentation. Escalate if the fix reaches
build/bootstrap coordination. Invariants: the shared form stays authoritative,
`Build required` stays present, `Residuals` stays last, and no field appears
twice. Never embed transcript paths or home paths.

## Acceptance criteria

- `.agents/skills/resolve-findings/SKILL.md` `## Handoff` declares no field the
  shared form in `.agents/references/subagent-reporting.md` `## Handoffs` already
  carries.
- Any detail the removed bullets carried that the shared form does not state is
  either preserved as row-form guidance for the existing shared field or
  recorded as a deliberate drop in the change report.
- `/validate-skill` passes for the `resolve-findings` package, and
  `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  `status: valid` with `code: ok`.
