<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-19T20:46:32.518Z","dependsOn":[]} -->
# Fix: Change Workflow Step 2 — a Tier-1 brief needing repository evidence is not routed to `/prepare-change`

## Context
Observed during a `/next-plan` run, at that run's checkpoint review
(`/next-plan-checkpoint-review`), as a context-efficiency isolation finding of
class `fixable-defect`. Emitter: `.agents/references/change-workflow.md`
`#### Step 2 — Prepare and explore alternatives`.

Symptom. The user asked mid-run for a Tier 1 change to the `/plan-alternatives`
skill. Main prepared that change inline instead of dispatching a preparation
worker: it ran a Grep over `.agents` for
`hypothes|cannot be trusted|untrusted|not trusted|current code wins`, then read
`.agents/skills/plan-alternatives/references/worker.md`,
`.agents/references/change-workflow.md` (`#### Step 2 — Prepare and explore
alternatives`), `.agents/references/authority-order.md`, and
`.agents/skills/plan-audit/references/worker.md` `## Rules`. All of that is
repository evidence an `implementer` running `/prepare-change` could have
consumed instead and returned condensed; the same four paths were afterwards
named in the `researcher` and `reviewer` briefs, so the reading bought main
nothing the briefs did not already carry. Cost: the four files plus the Grep
result occupied the main context for the rest of the run.

Gap. Step 1 already routes preparation for "any tier where classifying the work
needs repository evidence" — `.agents/references/change-workflow.md:77`. Step 2's
Order sentence — `.agents/references/change-workflow.md:83` — instead says
`/prepare-change` runs "first at Tier 2+", and that "at Tier 1 there is no plan
file, so main briefs from the request". Read on its own at Step 2, that sentence
reads as a positive instruction that main itself composes the Tier-1 brief, with
no pointer back to Step 1's evidence condition, so a Tier-1 change whose brief
can only be written after reading repository files is prepared inline by main.
The two steps disagree about the same decision.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the finding — the session
`/next-plan-review` must reach — while the `Landing ref` line names a ref whose
tree actually contains this Plan:
- Client: claude
- Conversation session ID: bacfd8f1-d6c3-4a4a-8385-de0e6912f3a0
- Worktree/branch UUID: dd213d47-ae5f-4717-a9f4-355f2377d57d
- Session branch: claude/dd213d47-ae5f-4717-a9f4-355f2377d57d
- Worktree: .claude\worktrees\BrokenEngine\dd213d47-ae5f-4717-a9f4-355f2377d57d
- Landing ref: claude/dd213d47-ae5f-4717-a9f4-355f2377d57d — the observing
  session records and lands this Plan itself, and had not landed when this Plan
  was written, so that branch tip carries this Plan only once the session lands;
  the branch survives exactly as long as the worktree recorded above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/Tier1BriefRepositoryEvidenceRouting.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
Recommendation: make Step 2's Tier-1 clause agree with Step 1 rather than
restate a different rule. The smallest shape the author sees is to replace the
"at Tier 1 there is no plan file, so main briefs from the request" clause in the
Order sentence at `.agents/references/change-workflow.md:83` with wording that
keeps the true part (at Tier 1 there is no plan file) and makes the briefing
route conditional: main briefs from the request when no repository read is
needed, and otherwise routes the Tier-1 preparation through `/prepare-change`
exactly as Step 1's bullet at line 77 already does. Referencing Step 1's
condition instead of restating it keeps the fact at one layer.

A second reasonable option, to weigh during implementation: drop the routing
clause from Step 2 entirely and let Step 2's Order sentence cover only the
Tier-2+ ordering, since Step 1 already owns the routing decision. That is
smaller but loses the at-a-glance answer at the step where the brief is written.

The line numbers above were read at this Plan's baseline; re-read the file and
locate the sentences by text before editing. Confirm the `implementer` bullet at
line 85 (`/prepare-change` — Tier 2+) does not contradict whichever wording is
chosen; if it does, that bullet is inside the boundary below.

## Critical files
- `.agents/references/change-workflow.md` — `#### Step 2 — Prepare and explore
  alternatives`

## In scope
- The Order sentence of `#### Step 2 — Prepare and explore alternatives` in
  `.agents/references/change-workflow.md` (line 83 at this Plan's baseline),
  specifically its Tier-1 clause "at Tier 1 there is no plan file, so main
  briefs from the request"
- The `implementer` bullet immediately below that sentence (line 85 at this
  Plan's baseline), only if the chosen wording would otherwise contradict it

## Out of scope
- `#### Step 1 — Approve and classify` and its `/prepare-change` bullet, which
  are already correct and are the authority this change conforms to
- Every other step of `.agents/references/change-workflow.md`
- `.agents/skills/prepare-change/`, `.agents/skills/plan-alternatives/`, and
  `.agents/skills/next-plan-checkpoint-review/`
- The changes the observing session landed; any transcript path or transcript
  text in the repository

## Risk tier and invariants
Tier 1 — mechanical, per `.agents/references/risk-tiers.md`: instruction prose
in a reference document, with no public signature, runtime behavior, or
invariant exposure. Trigger: documentation-only edit. Escalate only if the
chosen wording changes which role performs a workflow step at a tier other than
Tier 1. Never embed transcript paths or home paths.

## Acceptance criteria
- Step 2's Order sentence no longer instructs main to brief a Tier-1 change from
  the request unconditionally: it either routes a Tier-1 change whose brief needs
  repository evidence through `/prepare-change`, or defers that routing to Step 1
  without restating a conflicting rule
- Step 1's `/prepare-change` bullet and Step 2's Tier-1 wording state the same
  routing decision, with the condition itself written in exactly one of the two
  places
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports every row the change triggers passing
