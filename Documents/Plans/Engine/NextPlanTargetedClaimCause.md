<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-04T22:50:15.701Z","dependsOn":[]} -->
# Fix: Invoke-NextPlanClaim.ps1 — a targeted `none-available` claim names no cause

## Context
Run exactly as `.agents/skills/next-plan/references/worker.md:23` documents, from
the session worktree root:

`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1 -Plan 'Documents/Plans/Engine/InventoryVcxprojAndAcceptanceTriggers.md'`

Observed output: exit 0, `status: pass`, `code: none-available`,
`message: "No eligible Plans plan is available."`, `nextAction:
stop-report-to-user`, `claim: null`. The message names no cause for that one
requested Plan, and its wording ("eligible Plans plan") is itself garbled.

Because the result named no cause, main improvised `git ls-files
Documents/Plans/Engine/` — roughly 137 unread paths entering the main session
context — to check whether the Plan's prerequisite still existed. The actual
cause was that the requested Plan was blocked by its `dependsOn` prerequisite
`Documents/Plans/Engine/ExecutionCardChecker.md`, which the result could have
named directly.

Evidence in the current tree, none of it changed by the observing session:

- `Tools/WorktreeCli/PlanScheduler.cpp:793-813` — `RunClaimNext`'s candidate
  loop `continue`s past the requested Plan for four distinct reasons (invalid or
  blocked in the session tree `:795`, absent or demoted at the primary tip
  `:804`, blocked at the primary tip `:808`) and `:823-826` skips a Plan claimed
  by another session, discarding the known reason each time.
  `PlanScheduler.cpp:836` then emits `{"status":"ok","code":"none"}` with no
  reason field.
- `.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1:87` — turns that
  bare `none` into the observed `none-available` message; the script has no
  other information to add, and the typo lives on this line.
- `Tools/WorktreeCli/PlanScheduler.cpp:660-708` — `plan list` already computes
  the exact classification the claim discards: `blocked` with a `blockedBy`
  array (`:680-703`), `excluded` with a `diagnostic` (`:672-678`), `claimed`
  with the live claim's session, worktree, and expiry (`:667-671`), and no row
  at all for a Plan absent from the session tree.
- `.agents/skills/next-plan/references/claim-results.md:49-50` — documents that
  an exact path "absent, blocked, excluded, or claimed by another session yields
  `none-available`", so the contract itself collapses four causes into one code
  and one message; a fix has to update this prose.
- `.agents/skills/next-plan/references/worker.md:12-18` — routes the bounded
  listing `Get-NextPlanList.ps1` only for a tier-constrained request or a user
  queue request, so a `none-available` diagnosis has no routed bounded selector.
- `.agents/skills/next-plan/scripts/Get-NextPlanList.ps1:2`, `:18` — the listing
  projects only the first `-Top` rows (default 5) and offers no per-path filter,
  so in the observed queue (1 blocked Plan among 135) the default projection
  could not have shown the requested Plan's `blockedBy` either.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 54b6ae61-79d9-432d-8b6e-5bf03c168457
- Worktree/branch UUID: d46a8117-88ec-436b-8080-a1e8c9fa6609
- Session branch: claude/d46a8117-88ec-436b-8080-a1e8c9fa6609
- Worktree: .claude\worktrees\BrokenEngine\d46a8117-88ec-436b-8080-a1e8c9fa6609
- Landing ref: claude/d46a8117-88ec-436b-8080-a1e8c9fa6609
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`,
which cites the discarding code and the contract prose directly. Only when the
transcript is genuinely needed, in a new session run `/next-plan-review
claude/d46a8117-88ec-436b-8080-a1e8c9fa6609` in bounded friction mode, supplying
client `claude` and conversation session ID
`54b6ae61-79d9-432d-8b6e-5bf03c168457`. Then make the smallest fix inside the
`## In scope` boundary below. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

Decision: the fix lives in `Invoke-NextPlanClaim.ps1` alone; WorktreeCli is
not changed. On a targeted (`-Plan`) run that receives `code: none`, the script
calls the WorktreeCli `plan list` surface it already has provisioned, selects
the single row whose `path` equals the requested Plan, and folds that row's
cause into the existing `none-available` result: the row's `blockedBy` paths,
its `excluded` diagnostic, or a claimed-by-another-session note, with "absent
from the session tree" when no row matches. `plan list` already computes
exactly this classification (`PlanScheduler.cpp:660-708`), takes no scheduler
guard and changes nothing, and the claim script has already fast-forwarded the
session to the primary tip by the time it reaches the claim call
(`Invoke-NextPlanClaim.ps1:48-62`), so the listing's session-tree view and the
claim's primary-tip view agree at that point. This route needs no C++ change
and therefore no shared WorktreeCli rebuild or promotion. The same change fixes
the garbled message wording on `Invoke-NextPlanClaim.ps1:87` and updates
`references/claim-results.md:49-50` to describe what the result now names.

If root-causing shows the listing cannot name a cause the claim path knows for
one of the four causes, that cause is a residual for re-planning, not grounds
to change `PlanScheduler.cpp` inside this Plan.

## Critical files
- `.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1`
- `.agents/skills/next-plan/references/claim-results.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the targeted-claim `none` result path:
  the `code: none` branch and its result assembly in `Invoke-NextPlanClaim.ps1`
  (line 87 and any helper it needs)
- The matching contract prose in `.agents/skills/next-plan/references/claim-results.md`

## Out of scope
- Every other part of the claim flow: dirty-worktree handling, `-ResumeRetained`,
  the fast-forward and divergence classification, pattern matching, validation
  folding, and the claim result schema beyond the `none-available` result
- `Tools/WorktreeCli` source and `Tools/WorktreeCli/AGENTS.md`: the `claim-next`
  and `plan list` contracts are unchanged
- `Get-NextPlanList.ps1`, the `plan list` row shape, and any change to when
  `.agents/skills/next-plan/references/worker.md` step 2 routes the listing
- The landed change the session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (one tool's scoped behavior — the claim result a single skill
consumes); no WorktreeCli source changes, so no shared binary rebuild or
promotion. Invariants: the result must stay a bounded projection that cannot flood a
session context — never emit the whole Plan tree, and cap any list of blocking
paths; a bare (untargeted) claim keeps its current `none-available` behavior;
the diagnosis must add no scheduler state change, claim, or guard acquisition;
and no transcript path or home path is ever embedded.

## Acceptance criteria
- A targeted claim for a Plan blocked by an unmet `dependsOn` prerequisite
  returns `none-available` whose result names that prerequisite path, with no
  further command needed to learn the cause
- The remaining three targeted-claim causes — absent, excluded, and claimed by
  another session — are each distinguishable from the result alone
- The result's message reads as correct English
- A bare claim run with no eligible Plan still returns `status: pass`,
  `code: none-available`, `nextAction: stop-report-to-user`
- `/validate-skill` passes for any changed SKILL.md;
  `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  `status: valid`, `code: ok`
