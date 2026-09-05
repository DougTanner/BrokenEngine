<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-05T14:46:36.094Z","dependsOn":[]} -->
# Fix: next-plan worker step 5 — the idempotent re-claim fast-forwards the tree the card and plan reviews were verified against

## Context
Observed symptom in one `/next-plan` run. Step 4 of
`.agents/skills/next-plan/references/worker.md:26-52` dispatched the preparation
`implementer`, which verified every claimed-Plan citation against the tree at
session baseline `54661caf`, and `/plan-audit` and `/plan-simplicity-review`
then ran on the resulting execution card. Step 5
(`.agents/skills/next-plan/references/worker.md:53-54`) re-ran, from the session
worktree root:

`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1 -Plan 'Documents/Plans/Engine/InventoryVcxprojAndAcceptanceTriggers.md'`

That run returned `sync: {fastForwarded: true, from: 54661caf, to: c70df111}`
together with the held claim. The fast-forward brought in three commits; one of
them removed lines from `.agents/skills/finalize-changes/references/worker.md`,
so the region the card and an already-returned audit finding cited moved from
`:79-84` to `:71-76`, and the card's recorded baseline SHA became stale. Main
then spent three extra tool calls patching an already-reviewed card by hand: a
diff-stat over the cited paths, a diff plus grep to relocate the moved region,
and the card rewrite.

The fast-forward itself is documented, intended behavior:
`.agents/skills/next-plan/references/claim-results.md:18-22` states the claim
script brings the session branch up to the primary tip before it validates and
claims, and `Invoke-NextPlanClaim.ps1:48-62` performs it on every invocation, so
the step-5 invocation moves the tree the step-4 verification and the plan
reviews already ran against.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: b40c2d6e-8b1e-4e48-9e60-1f27a1490f11
- Worktree/branch UUID: 56d59457-5288-4998-9d24-a776f1d082d8
- Session branch: claude/56d59457-5288-4998-9d24-a776f1d082d8
- Worktree: .claude\worktrees\BrokenEngine\56d59457-5288-4998-9d24-a776f1d082d8
- Landing ref: claude/56d59457-5288-4998-9d24-a776f1d082d8 — the observing
  session records and lands this Plan itself, so its session branch tip is that
  session's final commit and survives exactly as long as the worktree above.
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Engine/NextPlanStepFiveReclaimSync.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/56d59457-5288-4998-9d24-a776f1d082d8` in bounded
friction mode, supplying the recorded client `claude` and the recorded
conversation session ID. Then make the smallest fix inside the `## In scope`
boundary below.

The author's recommendation, for the fix session to confirm or replace: keep the
one tree-moving claim at step 3, ahead of the preparation dispatch, so the card
and both plan reviews are produced and read against a single fixed tree, and make
step 5 a non-syncing confirmation that the claim is still held. Step 3 already
performs the sync, and nothing in `worker.md` or `claim-results.md` requires a
second one, so re-verifying citations after a step-5 sync (the alternative shape)
would leave the plan reviews reading a tree that has since moved and would repeat
work on every run rather than remove the cause.

The recommended mechanism is a switch on `Invoke-NextPlanClaim.ps1` — for
example `-HeldCheck` — that skips only the fast-forward block at
`Invoke-NextPlanClaim.ps1:46-62` and otherwise runs unchanged, so the step-5
invocation still reports the held claim through `plan claim-next` (`existing`)
and still cannot move the branch. `worker.md` step 5 then documents that switch,
and `claim-results.md` records that a held-check invocation reports no `sync`
object and therefore never changes the recorded session baseline. A fix session
that finds a smaller equivalent inside the same files may take it, stating why.

If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files
- `.agents/skills/next-plan/references/worker.md`
- `.agents/skills/next-plan/references/claim-results.md`
- `.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to: the fast-forward block
  `Invoke-NextPlanClaim.ps1:46-62` and that script's parameter block at `:2`;
  `worker.md` step 5 (`:53-54`) and, only where the step-5 wording forces it,
  step 3 (`:19-25`); the `## Session baseline and the sync object` section of
  `claim-results.md` (`:8-29`)

## Out of scope
- The landed change the observing session produced, including
  `Documents/Plans/Engine/InventoryVcxprojAndAcceptanceTriggers.md` and the
  `/finalize-changes` and `/update-affected-code` regions it touched
- The step-3 sync behavior itself, the `claim.session-diverged` and
  `claim.worktree-dirty` paths, and the retained-work/`-ResumeRetained` rules
- Other `/next-plan` scripts, unrelated skills, and any transcript path or
  transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: one script's claim path plus the prose
that documents it); escalate if the fix reaches build/bootstrap coordination or
changes how claims themselves are stored or healed. The session branch must stay
fast-forward-only and never be moved by a step-5 invocation; a claim result that
carries no `sync` object must leave the recorded session baseline unchanged.
Never embed transcript paths or home paths.

## Acceptance criteria
- A step-5 invocation run as `worker.md` documents it reports the held claim and
  no `sync` object, leaving `git rev-parse HEAD` in the session worktree
  unchanged across the call, while a step-3 invocation still fast-forwards a
  behind session as `claim-results.md` describes
- `/validate-skill` passes for the `next-plan` package; plan validate exits 0
