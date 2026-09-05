<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-04T23:43:31.544Z","dependsOn":[]} -->
# Fix: Invoke-NextPlanClaim.ps1 — a dirty unrelated Plan file makes the advertised claim recovery unreachable

## Context
Observed twice in one `/next-plan` run, in a worktree whose only dirty scheduler
path was one staged-but-uncommitted unrelated Plan
(`Documents/Plans/Engine/NextPlanTargetedClaimCause.md`, left in place by an
earlier run's checkpoint in the same worktree).

First, a targeted claim run exactly as
`.agents/skills/next-plan/references/worker.md` step 3 documents, from the
session worktree root:

`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1 -Plan 'Documents/Plans/<area>/<Plan>.md'`

returned `status: blocked`, `code: claim.worktree-dirty`,
`nextAction: resume-with-flag`, and a message telling main to rerun the same
command with `-ResumeRetained`. That advertised next action is unreachable for
exactly the path the message names:
`.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1:33-34`
unconditionally re-blocks a `-ResumeRetained` run with the same
`claim.worktree-dirty` code whenever any dirty path is under
`Documents/Plans/`, and `nextAction: stop-report-to-user`. The advertised route
is documented at
`.agents/skills/next-plan/references/claim-results.md:75-76` only as the
retained-work resume, and worker.md step 3 documents no recovery route for a
dirty unrelated Plan.

Second, worker.md step 5's "invoke step 3's claim script idempotently"
(`.agents/skills/next-plan/references/worker.md:53-54`) returned that same
`claim.worktree-dirty` block although this session already held the claim,
because the dirty gate at
`.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1:29-31` runs before
the existing-claim reuse path at `:86-90`. After the same set-aside below, the
rerun returned `code: reused`. So the documented idempotent rerun is not
idempotent while the worktree is dirty.

Cost, both times: main read the whole claim script and the whole unrelated
Plan into its own context, then invented an undocumented workaround — copy the
file outside the worktree, `git rm --cached -f`, delete, claim, restore and
re-stage. That workaround also reinstated only the working-tree version of the
unrelated Plan and dropped the distinct older staged version. A documented
recovery route, or a `locator` returning the gate condition as
`Invoke-NextPlanClaim.ps1:29-34` plus that Plan's metadata line and a one-line
disposition, would have replaced both whole-file reads.

Session provenance (machine-local; not reproducible after cleanup):
- Client: claude
- Conversation session ID: d96c3f2e-30ed-4770-9936-a82b5718ab1a
- Worktree/branch UUID: d46a8117-88ec-436b-8080-a1e8c9fa6609
- Session branch: claude/d46a8117-88ec-436b-8080-a1e8c9fa6609
- Worktree: .claude\worktrees\BrokenEngine\d46a8117-88ec-436b-8080-a1e8c9fa6609
- Landing ref: claude/d46a8117-88ec-436b-8080-a1e8c9fa6609
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Engine/NextPlanDirtyPlanClaimRecovery.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/d46a8117-88ec-436b-8080-a1e8c9fa6609` in bounded
friction mode, supplying client `claude` and the conversation session ID above.
Then make the smallest fix inside the `## In scope` boundary below. If
root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

The invariant the fix must preserve is the one stated at
`.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1:32`: selection and
validation read `Documents/Plans` from this tree, so uncommitted scheduler input
must never be claimed against.

Two candidate shapes, offered as the author's recommendation rather than a
binding decision, both confined to the files below and neither needing a
WorktreeCli change:

- (a) Let a targeted claim treat a dirty path that is an untracked-or-staged
  `Documents/Plans/**` file which is neither the requested Plan nor one of its
  dependencies as scheduler-neutral, so the advertised `-ResumeRetained` rerun
  actually succeeds. This keeps the invariant for every path that can affect the
  requested claim, and removes the unreachable-next-action defect at its
  emitter.
- (b) Run the existing-claim reuse check before the dirty gate, so worker.md
  step 5's idempotent rerun reports the held claim regardless of dirt, and
  document an explicit set-aside recovery route for a dirty unrelated Plan in
  `claim-results.md` and worker.md step 3.

Recommendation: (a) as the primary fix, because it repairs the advertised
`resume-with-flag` route the message already emits; (b)'s reuse-before-dirty
reordering is the smaller companion for the step 5 symptom if (a) alone leaves
that rerun blocked. Whichever is chosen, the emitted message must name a next
action the same script can actually accept.

Related, not duplicated: `Documents/Plans/Engine/NextPlanTargetedClaimCause.md`
and `Documents/Plans/Engine/NextPlanClaimSquashDivergence.md` both record other
defects in this script and both explicitly leave dirty-worktree handling and
`-ResumeRetained` out of scope.

## Critical files
- `.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1`
- `.agents/skills/next-plan/references/claim-results.md`
- `.agents/skills/next-plan/references/worker.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the files above: the dirty gate and
  `-ResumeRetained` scheduler-path block at
  `Invoke-NextPlanClaim.ps1:29-34`, its ordering relative to the existing-claim
  reuse path at `:86-90`, the `nextAction`/`resume-with-flag` description in
  `claim-results.md`, and the step 3 and step 5 claim instructions in
  `next-plan/references/worker.md`

## Out of scope
- Any WorktreeCli change
- The landed change the observing session produced
- The defects owned by `NextPlanTargetedClaimCause.md` and
  `NextPlanClaimSquashDivergence.md`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior — one script's claim gating plus its own
documentation); escalate if the fix reaches build/bootstrap coordination or
WorktreeCli's claim contract. Uncommitted scheduler input must never be claimed
against. Never embed transcript paths or home paths.

## Acceptance criteria
- A targeted claim whose only dirty path is an unrelated uncommitted
  `Documents/Plans/**` file either succeeds by the route its own message
  advertises, or is refused with a `nextAction` and message naming a route that
  works
- Worker.md step 5's idempotent rerun reports the held claim in that same
  worktree state
- /validate-skill passes for any changed SKILL.md; plan validate exits 0
