<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T15:53:18.004Z","dependsOn":[]} -->
# Fix: Invoke-NextPlanClaim.ps1 — claim result copies every scheduler notice into main's context

## Context
Observed symptom: in a `/next-plan` run, main ran
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1`
(bare and `-Plan`) twice. `.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1:98`
projects `status`, `code`, `message`, `diagnostics`, `notices`, and
`healedClaims` from the whole-tree `WorktreeCli plan validate` result into the
claim result's `validation` object. Each of the two claim results therefore
carried all 5 `stale-dependency` notices in the tree, each naming a Plan
unrelated to the claim (a notice is `{plan, code, dependency}`, emitted at
`Tools/WorktreeCli/PlanScheduler.cpp:580`). Main never acted on them. The run
checkpoint (`/next-plan-checkpoint-review`) reported this as content that
entered the main session without being used. The measured context-efficiency
envelope for the run was `pass`, with no result over threshold, so this is
repeated low-value content rather than one oversized result.

Consumer check at the time of recording: no file under `.agents/` reads the
claim result's `validation.notices`. `.agents/skills/next-plan/SKILL.md` and
`.agents/skills/next-plan/references/claim-results.md` never mention notices,
and the only other reader of the claim script is the `claim.session-diverged`
route that `.agents/skills/finalize-changes/references/scripts.md:340` names,
which does not read `validation`. `claim-results.md:84` names the envelope
version `/v6`.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 4b6e8e42-1f3e-473f-83e3-513989d4ba89
- Worktree/branch UUID: 5bc39800-5df7-4405-ae11-95a44b8c4298
- Session branch: claude/5bc39800-5df7-4405-ae11-95a44b8c4298
- Worktree: .claude\worktrees\BrokenEngine\5bc39800-5df7-4405-ae11-95a44b8c4298
- Landing ref: claude/5bc39800-5df7-4405-ae11-95a44b8c4298
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/ClaimResultNoticeBound.md`, but a periodic
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
`/next-plan-review <review ref>` in bounded friction mode, using the landing
ref and supplying client `claude` and the recorded conversation session ID.
Then make the smallest fix inside the `## In scope` boundary below. If
root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

The author recommends this fix, assuming the consumer check in `## Context`
still holds. The claim result's `validation` object keeps `status`, `code`,
`message`, `diagnostics`, and `healedClaims` unchanged, because a
`plan.validation-failed` stop needs the diagnostics. It replaces the whole
`notices` list with a `noticeCount` and a `notices` list holding only the
notices whose `plan` or `dependency` is the claimed Plan. The claimed Plan is
known only after `plan claim-next` returns (`Invoke-NextPlanClaim.ps1:118`), so
the filter is applied once the claim resolves. A result that stops before a
Plan is claimed carries the count and an empty list. The rationale is that a
notice about another Plan's stale edge never changes what main does with this
claim, while the count still shows that notices exist. The envelope version
moves from `/v6` to `/v7` because the `validation` shape changes, and
`claim-results.md:84` names the new version.

## Critical files
- `.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1`
- `.agents/skills/next-plan/references/claim-results.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `validation` projection at
  `Invoke-NextPlanClaim.ps1:98`, the claimed-Plan filter applied after the
  claim resolves, the `schemaVersion` string at `Invoke-NextPlanClaim.ps1:4`,
  and the `/v6` envelope mention at `claim-results.md:84`

## Out of scope
- The landed change the session produced
- `Tools/WorktreeCli` notice emission and the `plan validate` result shape
- `.agents/scripts/Test-PlanSchedulerState.ps1`, `.agents/scripts/New-PlanFile.ps1`,
  and `.agents/skills/next-plan/scripts/Get-NextPlanList.ps1`, whose notice
  output serves different callers
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2, because the fix changes the result shape of one `/next-plan`
script, which is scoped tool behavior. Escalate if the fix reaches
build/bootstrap coordination. The claim, fast-forward, and stop outcomes and
their exit codes stay unchanged. Never embed transcript paths or home paths.

## Acceptance criteria
- A claim result whose tree has scheduler notices about Plans other than the
  claimed one carries only `noticeCount` and the notices naming the claimed
  Plan
- A `plan.validation-failed` result still carries the full `diagnostics`
- The claim result's `schemaVersion` and `claim-results.md` name the same
  envelope version
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
