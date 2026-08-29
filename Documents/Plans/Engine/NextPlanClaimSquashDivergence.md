<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-29T01:38:35.582Z","dependsOn":[]} -->
# Fix: Invoke-NextPlanClaim.ps1 — squashed primary history is misclassified as `unlanded-work`

## Context
The user squashed all of the day's primary commits into one
(`1041358859ad8b3dbcd288680db37e285ca53a0c`, "Falbe plans"). The session branch
was still at `d4bd19d18054bb50b3308104ef3abcb545211ddb`. Both commits had the
byte-identical tree `2b5ae8a91997f23faa9fde2985ca48f9fcce6f2f`
(`git rev-parse main^{tree}` equalled `git rev-parse HEAD^{tree}`), and the
session worktree was clean, so the session held no work primary lacked.

Run exactly as `.agents/skills/next-plan/SKILL.md:50` documents:

`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1 -Plan 'Documents/Plans/Engine/FinalizeFixtureOmittedTokenIndexLock.md'`

Observed output: exit 2, `status: blocked`, `code: claim.session-diverged`,
`divergence.verdict: "unlanded-work"`, `recovery: null`, with the message
beginning "Session HEAD d4bd19d1… has commits the primary tip 1041358… does not,
so the session cannot be fast-forwarded; … 34 of 34 session commit(s) hold work
that is not on primary (…), so the session branch must not be reset; report this
to the user." All 34 rows carried `verdict: "unlanded"` and
`matchedPrimaryCommit: null`.

That verdict was wrong for the repository state. The correct answer was
`safe-reset` with the documented `git reset --hard <primary tip>` recovery. The
workaround was a hand `git reset --hard 1041358859ad8b3dbcd288680db37e285ca53a0c`
under explicit user direction; the rerun claim then returned `status: pass`,
`code: reused`.

Evidence in the current tree, none of it changed by the observing session:

- `Invoke-NextPlanClaim.ps1:16-37` — `Get-SessionDivergence` matches each
  session commit's filtered patch hash one-to-one against a single primary
  commit's hash (`Invoke-NextPlanClaim.ps1:24`, `:29-31`), and any unmatched
  session commit clears `Safe` (`:33`), so a primary squash that folds N session
  commits into one commit matches none of them.
- `Invoke-NextPlanClaim.ps1:80-96` — the unsafe branch emits
  `verdict: unlanded-work` with `recovery: null` and the "must not be reset"
  message.
- `NextPlanWorkflowCommon.psm1:58` — `Get-NextPlanFilteredPatchHash`, the
  per-commit hash the match uses.
- `.agents/skills/next-plan/SKILL.md:56-66` and
  `.agents/skills/next-plan/references/claim-results.md:45-55` — the documented
  `divergence` contract this behavior must stay consistent with; both describe
  `safe-reset` only in per-commit byte-identical-duplicate terms, so a fix that
  adds a whole-tree route has to update that prose too.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: a0d3be2c-8f69-4879-901e-c868f3669342
- Worktree/branch UUID: c6070de5-edc9-436f-ac6c-c12a0a6d6dd2
- Session branch: claude/c6070de5-edc9-436f-ac6c-c12a0a6d6dd2
- Worktree: .claude\worktrees\BrokenEngine\c6070de5-edc9-436f-ac6c-c12a0a6d6dd2
- Landing ref: claude/c6070de5-edc9-436f-ac6c-c12a0a6d6dd2
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
In a new session, run `/next-plan-review claude/c6070de5-edc9-436f-ac6c-c12a0a6d6dd2`,
supplying client `claude` and conversation session ID
`a0d3be2c-8f69-4879-901e-c868f3669342`.
Root-cause the friction from the proven transcript, then make the smallest fix
inside the `## In scope` boundary below. If root-causing shows the fix lies
outside that boundary, surface it for re-planning instead of expanding scope.

As a starting hypothesis for that root-causing, not a decision: the author
recommends adding a whole-state check ahead of the per-commit match in
`Get-SessionDivergence` — comparing the session HEAD tree with the primary tip
tree, or otherwise confirming the session-only commits' cumulative diff against
the merge base is already contained in primary's tree — and reporting
`safe-reset` with the existing `git reset --hard <primary tip>` recovery when it
holds. The rationale is that this is the condition the recovery's safety
actually depends on (no session-only content), it reuses the existing verdict,
recovery string, and result shape rather than adding a new one, and it leaves
the per-commit path untouched for the cases it already classifies correctly.
The reviewing session should confirm or replace it from the transcript and the
code.

## Critical files
- `.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1`
- `.agents/skills/next-plan/scripts/NextPlanWorkflowCommon.psm1`
- `.agents/skills/next-plan/SKILL.md`
- `.agents/skills/next-plan/references/claim-results.md`

## In scope
- Root-cause investigation via /next-plan-review, run with client `claude`, the
  landing ref named in `## Design`, and the recorded conversation session ID
- The smallest resulting fix, confined to the divergence classifier
  (`Get-SessionDivergence` and the `claim.session-diverged` result-building
  branch in `Invoke-NextPlanClaim.ps1`, plus `Get-NextPlanFilteredPatchHash` in
  `NextPlanWorkflowCommon.psm1` only if the classification needs it), and to the
  matching `divergence` contract prose in `next-plan/SKILL.md` and
  `references/claim-results.md`

## Out of scope
- Every other part of the claim flow: dirty-worktree handling, `-ResumeRetained`,
  the fast-forward merge, selection, WorktreeCli claim calls, and the claim
  result schema beyond the `divergence` object
- The landed change the observing session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior). Escalate if the fix reaches
build/bootstrap coordination or changes the claim result schema. The safety
invariant the classifier exists to protect must hold: a session branch may be
reset only when it provably holds no content primary lacks, so any new
`safe-reset` route must fail closed, and a classification error must still
produce no verdict rather than a safe-looking one
(`Invoke-NextPlanClaim.ps1:82`, `:95`). Never embed transcript paths or home
paths.

## Acceptance criteria
- With a clean session worktree whose HEAD tree is byte-identical to a primary
  tip that squashed the session's commits, the documented claim invocation
  reports `divergence.verdict: safe-reset` with the
  `git reset --hard <primary tip>` recovery, and after that reset the rerun
  claim passes
- A session that genuinely holds content primary lacks still reports
  `unlanded-work` with `recovery: null`
- /validate-skill passes for any changed SKILL.md;
  `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  `status: valid`, `code: ok`
