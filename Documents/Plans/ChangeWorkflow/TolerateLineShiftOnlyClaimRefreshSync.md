<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-09T15:09:19.343Z","dependsOn":[]} -->
# Fix: /next-plan step 6 — a line-anchor-only touch in the final claim refresh forces a full return to preparation

## Context
Observed symptom: during the `/next-plan` run that claimed
`Documents/Plans/Engine/NetworkServerOverEngineeringCleanup.md`, the step-6 final
claim refresh (`.agents/skills/next-plan/SKILL.md` `## Steps` step 6, lines
137-159) returned a `sync` object twice in a row: first `2ee2a571..a8fb9bac`,
then `a8fb9bac..adec1d07`. Each time main dispatched the step-6 sync
`researcher`, and both researchers reported touches to cited paths whose only
effect was shifted line numbers; the cited statements kept their meaning.
Step 6 (line 155) treats any reported touch as a return to step 4: "When any row
reports a touch, return to step 4, rerun the affected Plan review checks in
step 5, and repeat this final refresh before approval". The first sync therefore
cost one extra preparation snapshot redraft dispatch plus a second refresh and
a second sync researcher. On the second sync main did not loop again; it passed
the corrected line anchors directly into the implementer brief, a workaround
step 6 does not describe.

The researcher's per-path row only states whether the change "touches a
statement the preparation handoff or execution card relies on" (lines 152-154),
so it has no way to distinguish a moved anchor with unchanged meaning from a
changed statement, and step 6 has no route for the former short of step 4.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 488ec650-1b83-4287-8ec0-fe428cc8d548
- Worktree/branch UUID: 1da29a17-c83f-44c7-bead-da6ddce9072e
- Session branch: claude/1da29a17-c83f-44c7-bead-da6ddce9072e
- Worktree: .claude\worktrees\BrokenEngine\1da29a17-c83f-44c7-bead-da6ddce9072e
- Landing ref: claude/1da29a17-c83f-44c7-bead-da6ddce9072e (the observing
  session records and lands this Plan itself; the branch survives exactly as
  long as the worktree recorded above).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/TolerateLineShiftOnlyClaimRefreshSync.md`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above. OpenCode transcript review remains unsupported regardless of worktree
  retention.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode — the landing ref —
supplying the recorded client and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

The author's recommendation, for the fix session to confirm: let the sync
researcher's per-path row distinguish an anchor-only change (line numbers moved,
cited statement unchanged) from a changed statement, and let step 6 carry an
anchor-only result forward as corrected anchors in the implementation brief
instead of returning to step 4, keeping the full return only for a changed
statement. Rationale: the second sync in the observed run already took that
route with no loss, and it removes a whole preparation and review round per
anchor-only sync.

## Critical files
- `.agents/skills/next-plan/SKILL.md` — `## Steps` step 6 (the sync researcher's
  per-path row and the touch-to-step-4 rule) and, only if the corrected anchors
  need a named carrier, step 8's implementer brief

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `.agents/skills/next-plan/SKILL.md`
  `## Steps` step 6 and, only as `## Critical files` states, step 8

## Out of scope
- The landed change the session produced
- `.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1` and the `sync`
  object it returns; the held-claim dirty-worktree refusal, which
  `Documents/Plans/ChangeWorkflow/AllowHeldClaimRefreshWithUncommittedPlans.md` owns
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: one skill's workflow step); escalate if
the fix reaches build/bootstrap coordination. A sync that changes a statement
the preparation handoff or execution card relies on must still return to
step 4. Never embed transcript paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation: a
  `sync` whose cited-path changes only shift line numbers no longer returns the
  run to step 4, and a changed relied-on statement still does
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
