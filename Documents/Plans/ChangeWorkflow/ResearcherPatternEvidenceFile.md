<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-17T15:19:00.246Z","dependsOn":[]} -->
# Fix: /external-design-interface researcher — `Pattern evidence` returns inline and main copies it to a file for the planners

## Context
Context-efficiency finding from the `/next-plan` run checkpoint
(`/next-plan-checkpoint-review`) of the session recorded below.

- Tool: `Agent` (the `/external-design-interface` `researcher` handoff)
- Invocation: the step 6 `researcher` dispatch; its returned handoff carried
  the `Pattern evidence` field inline
- Measured size: 11,570 characters of `Pattern evidence` entered the main
  context
- Checkpoint: the `/next-plan` run checkpoint of the session recorded below

Main then wrote that text back verbatim to a `Temp/` file so it could hand it to
the three planners. The text crossed the main context twice and main used none
of it to make a decision.

`.agents/skills/external-design-interface/SKILL.md` `## Handoff` (`:112-138`)
declares the researcher extension's `Pattern evidence` as an inline field and
says both workers return `Changed files: none` because the workflow changes no
file (`:132-133`); the skill's description calls it a read-only workflow, and
`## Rules` (`:159-161`) bars the manager and workers from creating scratch
source. Step 9 (`:80-90`) gives every planner the repository
evidence. So the skill offers no way for the researcher to hand that evidence
to the planners except through main.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 6bdbf3e5-d34d-4e81-a914-5ad25ff31358
- Worktree/branch UUID: b30e196d-9362-434f-8887-7f86ca9a2631
- Session branch: claude/b30e196d-9362-434f-8887-7f86ca9a2631
- Worktree: .claude/worktrees/BrokenEngine/b30e196d-9362-434f-8887-7f86ca9a2631
- Landing ref: claude/b30e196d-9362-434f-8887-7f86ca9a2631 (the observing
  session records and lands this Plan itself; the branch tip is that session's
  final commit and survives exactly as long as the worktree recorded above).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/ResearcherPatternEvidenceFile.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode, using the landing ref
and supplying client `claude` and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

The author recommends that the researcher write its full pattern evidence to
`Temp/<name>-pattern-evidence.md` and cite it under the shared `Evidence` field
as path plus `##` selector, per `.agents/references/subagent-handoff.md`
`## Handoffs`. The inline `Pattern evidence` field then carries only the short
summary main needs to continue. Step 9 passes the planners that path plus
selector instead of the evidence text. The `## Handoff` sentence that says both
workers change no file, and the `## Rules` bullet on scratch source, must then
allow that one `Temp/` file, as `/coherence-review` already allows its one
`Temp/` handoff file. The `<name>` stem is the author's choice; the fix may pick
another under the same pattern.

## Critical files
- `.agents/skills/external-design-interface/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to
  `.agents/skills/external-design-interface/SKILL.md`: the `## Handoff`
  researcher extension and its changed-files sentence, `### Research Existing
  Patterns` steps 7-8, step 9's evidence hand-off to the planners, and the
  `## Rules` bullet on what the manager and workers may create

## Out of scope
- The landed change the session produced
- Main writing the design brief, owned by
  `Documents/Plans/ChangeWorkflow/DesignBriefWrittenByImplementer.md`
- The planner extension, the three design axes, and `### Compare and Decide`
- `.agents/references/subagent-handoff.md`, whose `Temp/` `Evidence` rule is
  reused unchanged
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Coordination
- `Documents/Plans/ChangeWorkflow/DesignBriefWrittenByImplementer.md` edits the
  `## Handoff` and `## Rules` sections of the same skill to let a worker write
  the design brief as a `Temp/` file. Whichever lands second keeps the two
  `Temp/` file allowances in one consistent wording.

## Risk tier and invariants
Expected Tier 2. Trigger: scoped tool behavior, because one skill's handoff
contract changes. Every planner still receives the same complete repository
evidence (step 9), and the researcher's evidence keeps its `file:line` or
symbol citations. Never embed transcript paths or home paths.

## Acceptance criteria
- The researcher's full pattern evidence reaches the planners as a `Temp/`
  path plus selector, and main receives only the summary and the `Evidence` row
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes
- `.agents/skills/next-plan/references/run-checkpoint.md`
  `## Follow-up routing` routed this finding here. The 11,570-character figure
  is the checkpoint's measured size of the returned field.
