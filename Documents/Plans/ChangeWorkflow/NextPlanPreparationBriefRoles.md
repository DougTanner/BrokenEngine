<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-26T13:50:09.891Z","dependsOn":[]} -->
# Fix: /next-plan — preparation brief leaves the execution card's Roles field unfilled

## Context
Observed during a `/next-plan` claim of
`Documents/Plans/Engine/AgentConnectionDeferredDisconnectRecovery.md`. Main
dispatched the step 4 preparation `implementer` on the brief that step
prescribes. The returned `Temp/` snapshot left the card's `Roles` field
unfilled, stating that the preparation brief supplied no step or role
assignments and that main assigns them for Tier 3. Main then derived the full
Tier-3 role list itself and hand-edited the worker-owned `Temp/` snapshot to
fill the field — extra main-session work plus an edit outside the preparation
worker's ownership.

Current-tree evidence:
- `.agents/skills/next-plan/SKILL.md` step 4 (the `4. Dispatch one preparation
  implementer` item and its paragraphs through its Done condition) lists what
  the brief carries — `Skill: none`, the `Return:` contract, the Plan
  citations, the evidence bound, and the `Temp/` snapshot instructions — but
  never has main supply step and role assignments, nor tells the worker to
  derive them.
- The same step's Done condition requires the card to carry every field of
  `### Execution card presentation/template`, whose template includes
  `- Roles: <required and conditional assignments>`.
- The mirrored non-Plan route already closes this gap:
  `.agents/skills/prepare-change/references/worker.md` step 3 fills `Roles`
  "from the step and role assignments the brief supplies" and records a tier
  mismatch under `Unresolved decisions`. `/prepare-change` excludes a claimed
  Plan, so `/next-plan` step 4 does not inherit that instruction.
- `.agents/references/change-workflow.md` `#### Step 1 — Approve and classify`
  has main lock in the roles.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 6bbb2d65-8498-4fd2-8983-1ed5252b8039
- Worktree/branch UUID: 0cf2309b-7f76-48d7-b660-0063cddbb5a9
- Session branch: claude/0cf2309b-7f76-48d7-b660-0063cddbb5a9
- Worktree: .claude\worktrees\BrokenEngine\0cf2309b-7f76-48d7-b660-0063cddbb5a9
- Landing ref: claude/0cf2309b-7f76-48d7-b660-0063cddbb5a9 (the observing
  session records and lands this Plan itself; the branch tip is that session's
  final commit and survives exactly as long as the worktree recorded above).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/NextPlanPreparationBriefRoles.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode — the landing ref
above — supplying client `claude` and the recorded conversation session ID.
Then make the smallest fix inside the `## In scope` boundary below. If
root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

The author recommends mirroring `/prepare-change` (mirrored patterns stay
parallel): step 4's brief supplies the Change Workflow step and role
assignments for the tier main expects, and the preparation worker fills the
card's `Roles` field from them, recording a tier mismatch under
`## Unresolved decisions` instead of reconciling it. Rationale: the Change
Workflow already assigns roles to main, and `/prepare-change` already states
this exact contract, so one added brief item and one worker instruction close
the gap without a new mechanism. The alternative — the worker deriving roles
from the Change Workflow step list for its classified tier — removes main's
work entirely but has the worker read the workflow, which `/prepare-change`
deliberately avoids; the fix session chooses between them after root-causing.

## Critical files
- `.agents/skills/next-plan/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `.agents/skills/next-plan/SKILL.md`
  step 4 (the preparation `implementer` brief paragraphs and, only if needed,
  its Done condition)

## Out of scope
- The landed change the session produced
- `### Execution card presentation/template` and its `Roles` field definition
- `.agents/skills/prepare-change/` and `.agents/references/change-workflow.md`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Change Workflow Tier 2 — trigger: scoped tool behavior (one skill's
preparation-dispatch contract). Escalate if the fix reaches build/bootstrap
coordination. Never embed transcript paths or home paths.

## Acceptance criteria
- A `/next-plan` preparation handoff's `Temp/` snapshot carries a filled
  `Roles` card field without main editing the worker-owned snapshot
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports every row the change triggers passing

## Notes
- The `/external-skill-creator` validation applies to the fix because it
  changes a `.agents/skills/*/` package.
