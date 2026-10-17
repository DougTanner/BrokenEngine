<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-17T13:25:12.869Z","dependsOn":[]} -->
# Fix: /next-plan preparation handoff — main reads the snapshot to score candidate zero

## Context
Isolation finding from the `/next-plan` run checkpoint
(`/next-plan-checkpoint-review`) of the session recorded below.

`/plan-alternatives` `### Comparison`
(`.agents/skills/plan-alternatives/SKILL.md:79-93`) has main score candidate
zero and every returned candidate on the same criteria, read from the rows each
researcher returns per that skill's `## Handoff` (`:57-67`): `Mechanism`,
`Adds/Deletes`, `Invariant surfaces`, `Existing guarantees`,
`Self-serving machinery`, and `Pays off when`. The returned candidates arrive in
that row shape; candidate zero does not.

The `/next-plan` preparation handoff gives candidate zero only as the one-line
`Candidate zero` field (`.agents/skills/next-plan/SKILL.md:266-271`), written
for the researcher brief and bound by its exclusions
(`.agents/references/change-workflow.md:139-151`). That line names the
mechanism and nothing the comparison scores beside it. In the observing run,
after the three `/plan-alternatives` researcher handoffs returned, main read the
preparation snapshot's `### Resolved fix` section (58 lines) to score candidate
zero on those criteria. The preparation `implementer` already held that
snapshot and could have returned the rows instead.

The one-line field came from the completed Plan
`ReturnCandidateZeroInPreparationHandoff` (landed as commit 261d2c7d), which
covered only writing the brief's candidate-zero line, not scoring it.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 981d5b50-e3eb-4ecf-8423-076611e011eb
- Worktree/branch UUID: ca2082a9-6616-4eac-94fe-d7156a0cf2ae
- Session branch: claude/ca2082a9-6616-4eac-94fe-d7156a0cf2ae
- Worktree: .claude/worktrees/BrokenEngine/ca2082a9-6616-4eac-94fe-d7156a0cf2ae
- Landing ref: claude/ca2082a9-6616-4eac-94fe-d7156a0cf2ae (the observing
  session records and lands this Plan itself; the branch tip is that session's
  final commit and survives exactly as long as the worktree recorded above).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/ReturnCandidateZeroScoringRows.md`,
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

The author recommends returning candidate zero in the `/plan-alternatives`
`## Handoff` row shape: `Mechanism`, `Adds/Deletes`, `Invariant surfaces`,
`Existing guarantees`, `Self-serving machinery`, and `Pays off when`, each one
line, all `none` for an empty realized change. Rationale: main then scores
candidate zero from the handoff alone, exactly as it scores the returned
candidates.

Only the `Mechanism` row becomes the brief's candidate-zero line, and it keeps
today's brief exclusions. The other rows serve main's comparison only and never
enter a researcher brief: `Pays off when` and `Adds/Deletes` can carry the
plan's rationale, which `change-workflow.md:139-151` bars from every brief.
Step 2's sentence that sources the candidate-zero line should name the
`Mechanism` row instead of the whole field.

The author recommends making the same change to the `/prepare-change` handoff's
`Candidate zero` line (`.agents/skills/prepare-change/SKILL.md:58`). Rationale:
the two fields mirror each other and share the one Step 2 sentence, and the
comparison asks main for the same scores on that route. The finding observed
only the `/next-plan` route; keeping the mirror parallel follows the root
`AGENTS.md` mirrored-patterns directive rather than evidence from that route.

## Critical files
- `.agents/skills/next-plan/SKILL.md`
- `.agents/skills/prepare-change/SKILL.md`
- `.agents/references/change-workflow.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to:
  - `.agents/skills/next-plan/SKILL.md` — the `Candidate zero` entry in
    `## Handoff`
  - `.agents/skills/prepare-change/SKILL.md` — the `Candidate zero` line in the
    `## Handoff` extended field block
  - `.agents/references/change-workflow.md` — `#### Step 2 — Prepare and
    explore alternatives`, only the sentence that sources the brief's
    candidate-zero line from the preparation handoff
  - `.agents/skills/prepare-change/references/worker.md`, only where the new
    rows must be produced by a worker step

## Out of scope
- The landed change the observing session produced
- `/plan-alternatives` itself (`SKILL.md` and `references/worker.md`): its
  axes, its researcher `## Handoff` rows, its `### Comparison` criteria, and its
  user presentation
- The researcher brief's other fields and exclusions, and the Tier-1 route in
  Step 2 where no plan file exists
- The snapshot's content and headings, `Test-PlanCitations.ps1`, and the
  execution card template
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2. Trigger: scoped tool behavior, because the handoff contract of
the two preparation skills and the Step 2 brief-sourcing sentence change
together within the Change Workflow. The researcher-blindness exclusions in
`change-workflow.md:139-151` must keep holding: only the `Mechanism` row enters
a brief, and no rationale, scope section, or plan path enters any brief field.
Never embed transcript paths or home paths.

## Acceptance criteria
- Both preparation handoffs return candidate zero in the six named rows, with
  the `none` value rule, and Step 2 names the `Mechanism` row as the brief's
  candidate-zero source
- Main can score candidate zero on every `/plan-alternatives` `### Comparison`
  criterion from the preparation handoff alone, without reading the snapshot or
  plan file
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes
- `.agents/skills/next-plan/references/run-checkpoint.md`
  `## Follow-up routing` routed this finding here. It is an isolation finding,
  so it records no measured tool-result envelope. The 58-line figure is the
  checkpoint's observation of main's snapshot read.
