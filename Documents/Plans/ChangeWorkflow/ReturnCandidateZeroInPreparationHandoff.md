<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-16T14:18:47.796Z","dependsOn":[]} -->
# Fix: /next-plan and /prepare-change preparation handoffs — main reads the snapshot to write the candidate-zero line

## Context
Isolation finding from the `/next-plan` run checkpoint
(`/next-plan-checkpoint-review`) of the session recorded below.

`.agents/references/change-workflow.md:91`, the Step 2 `/plan-alternatives`
brief recipe, requires each researcher brief to carry "one line naming the
drafted mechanism as candidate zero". The same line forbids the plan's
rationale, its `## In scope`/`## Out of scope` sections, and the plan file's
path in any brief field. Neither preparation handoff returns that line:

- The `/next-plan` preparation handoff (`.agents/skills/next-plan/SKILL.md`
  `## Handoff`, `:230-259`) declares `Claim`, `Classification`, `Verification`,
  and `Slices`. The step-4 brief (`:93-108`) has the mechanism written only into
  the gitignored `Temp/` preparation snapshot.
- The `/prepare-change` handoff (`.agents/skills/prepare-change/SKILL.md`
  `## Handoff`, `:48-64`) declares `Prepared plan`, `Classification`,
  `Execution card`, and `Unresolved decisions`, and bars inlining plan or card
  text.

To write that one line, main read about 19k characters of the preparation
snapshot, including its `## In scope` and `## Out of scope` sections. The
recipe bars those sections from the researcher briefs, and `/next-plan` step 8
(`SKILL.md:180`) says main never reads the snapshot's `## In scope` itself. A
dispatched worker that already holds the snapshot could have returned the one
line instead.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: d597a401-d119-45ae-80c8-4e1e9bd47ecb
- Worktree/branch UUID: 965c0c6c-0185-449d-8bf2-5c1e0bbcf561
- Session branch: claude/965c0c6c-0185-449d-8bf2-5c1e0bbcf561
- Worktree: .claude/worktrees/BrokenEngine/965c0c6c-0185-449d-8bf2-5c1e0bbcf561
- Landing ref: claude/965c0c6c-0185-449d-8bf2-5c1e0bbcf561 (the observing
  session records and lands this Plan itself; the branch tip is that session's
  final commit and survives exactly as long as the worktree recorded above).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/ReturnCandidateZeroInPreparationHandoff.md`,
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

The author recommends adding one declared handoff field, `Candidate zero`, to
both preparation handoffs. It is a single line naming the drafted mechanism,
written so main can copy it verbatim into a `/plan-alternatives` researcher
brief. It never restates the plan's rationale or its scope sections and never
names the plan file's path, which are the exclusions
`change-workflow.md:91` sets. Its value is `none` when the dispatch prepared no
plan. Step 2 (`change-workflow.md:91`) should then name that field as the source
of the candidate-zero line, so main does not read the snapshot or plan file to
produce it. The field name is the author's choice; it reuses the term
`/plan-alternatives` already uses.

## Critical files
- `.agents/skills/next-plan/SKILL.md`
- `.agents/skills/prepare-change/SKILL.md`
- `.agents/references/change-workflow.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to:
  - `.agents/skills/next-plan/SKILL.md` — `## Handoff` (the declared field
    list) and step 4's preparation-brief paragraph that lists what the
    `implementer` writes and returns
  - `.agents/skills/prepare-change/SKILL.md` — `## Handoff` (the extended
    field block)
  - `.agents/references/change-workflow.md` — `#### Step 2 — Prepare and
    explore alternatives`, the `/plan-alternatives` brief bullet
  - `.agents/skills/prepare-change/references/worker.md`, only where the new
    field must be produced by a worker step

## Out of scope
- The landed change the session produced
- `/plan-alternatives` itself (`SKILL.md` and `references/worker.md`), its axes,
  and its user presentation
- The Tier-1 route in Step 2 where no plan file exists, and the brief's other
  fields
- The snapshot's content and headings, `Test-PlanCitations.ps1`, and the
  execution card template
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2. Trigger: scoped tool behavior, because the handoff contract of
two preparation skills and the Step 2 brief recipe change together within the
Change Workflow. The researcher-blindness exclusions in `change-workflow.md:91`
must keep holding: no rationale, no scope sections, and no plan path in any
brief field. Never embed transcript paths or home paths.

## Acceptance criteria
- Both preparation handoffs declare the candidate-zero line, with its value
  rule. Step 2 names that field as the source of the brief's candidate-zero
  line, so main writes a `/plan-alternatives` brief from the handoff alone,
  without reading the snapshot or plan file
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes
- `.agents/skills/next-plan/references/run-checkpoint.md`
  `## Follow-up routing` routed this finding here. It is an isolation finding,
  so it records no measured tool-result envelope. The 19k-character figure is
  the checkpoint's observation of main's snapshot read.
