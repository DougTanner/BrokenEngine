<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-29T02:21:13.637Z","dependsOn":[]} -->
# Fix: finalize-changes — `<session-label>` is an undefined placeholder on every finalize command

## Context
While finalizing this session's landing, the finalizer had to supply
`-SessionLabel '<session-label>'` and could not determine from any document what
value that placeholder means.

`.agents/skills/finalize-changes/references/scripts.md` requires the parameter on
seven documented command lines — `:60`, `:61`, `:64`, `:65`, `:66`, `:67`, `:68`
(four `Invoke-FinalizeLockClaim.ps1` forms, the `-AdvancePrimary`
`Invoke-FinalizeCandidateCommit.ps1` form, and both `Invoke-FinalizeLanding.ps1`
forms) — each written literally as `-SessionLabel '<session-label>'`.
`.agents/skills/finalize-changes/SKILL.md:43-44` (`## Inputs and ownership`) says
only "Resolve checkout, primary, and session identity from
`Get-AgentWorktreeSessionContext`". That function returns
`Worktree`, `Branch`, `SessionId`, `PrimaryRoot`, `PrimaryBranch`, `PrimaryTip`,
`Baseline` (`.agents/scripts/AgentWorktreeSession.psm1:142-144`) — there is no
`SessionLabel` field, and no document states which of those fields
`<session-label>` is.

`grep -rn SessionLabel .agents/scripts/FinalizeWorkflowCommon.psm1` returns
nothing. The remaining prose describes how the value is *used* but never where it
comes from: `references/workflow.md:47-48` and `references/scripts.md:265-266`
both discuss deriving `$SessionLabel/landing`, and
`references/primary-commit.md:29` lists `-SessionLabel` among the required
`-AdvancePrimary` parameters. The only concrete values anywhere are the fixture
literals `'finalize-fixture'`
(`.agents/skills/finalize-changes/scripts/Test-FinalizeWorkflowFixtures.ps1:679`,
`:733`), which are a scratch-repository fixture constant rather than a mapping a
real session can follow.

Workaround: the finalizer chose `SessionId`
(`c6070de5-edc9-436f-ac6c-c12a0a6d6dd2`) by judgment and confirmed only after the
fact that the lock claim echoed it back as `lock.session`.

Why the ambiguity is dangerous rather than cosmetic. Landing accepts a retained
lease only as a same-actor continuation whose recorded session matches exactly:
`Invoke-FinalizeLanding.ps1:359` and `:1258` build
`$script:LandingSession` from `$SessionLabel` (appending `/landing` when
`-OwnerToken` is omitted), `:1351` gates adoption on
`Test-FinalizeLandingLockClaimIdentity ... $SessionLabel ...`, and that function
compares `$Status.session` to it with the case-sensitive `-cne`
(`.agents/scripts/FinalizeWorkflowCommon.psm1:307-315`). So an agent that picks
the branch name in the claim call and the session id in the landing call — both
defensible readings of `<session-label>` today — gets its own live lease treated
as foreign and is blocked mid-landing.

A second gap the mapping has to answer: on the primary-commit route
`Get-AgentWorktreeSessionContext` deliberately resolves `SessionId` as `$null`
(`AgentWorktreeSession.psm1:62-63`), yet `references/primary-commit.md:29` still
requires `-SessionLabel` on that route, and the parameter is
`[Parameter(Mandatory)]` (`Invoke-FinalizeLanding.ps1:16`). A bare "use
`SessionId`" sentence would leave that route undefined.

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
recommends defining `<session-label>` exactly once in the finalize-changes
documentation as a named `Get-AgentWorktreeSessionContext` field — most likely
`SessionId` — stating what the primary-commit route supplies when that field is
`$null`, and referencing that single definition from the command block rather
than repeating it per command. The rationale is that the value is already
Git-derived and single-sourced by that function, one definition is what keeps the
claim call and the landing call byte-identical (which is the property
`Test-FinalizeLandingLockClaimIdentity`'s case-sensitive comparison actually
depends on), and prose alone changes no script behavior or lock semantics. The
reviewing session should confirm or replace it from the transcript, the scripts,
and the fixture's own argument values.

## Critical files
- `.agents/skills/finalize-changes/SKILL.md`
- `.agents/skills/finalize-changes/references/scripts.md`
- `.agents/skills/finalize-changes/references/workflow.md`
- `.agents/skills/finalize-changes/references/primary-commit.md`

## In scope
- Root-cause investigation via /next-plan-review, run with client `claude`, the
  landing ref named in `## Design`, and the recorded conversation session ID
- The smallest resulting documentation fix, confined to the four files above —
  within them, the `<session-label>` placeholder occurrences in
  `references/scripts.md` (`:60`, `:61`, `:64`-`:68`) and their surrounding
  parameter-resolution prose (`:265-266`), the `## Inputs and ownership`
  identity-resolution sentence in `SKILL.md:43-44`, the `$SessionLabel` ownership
  prose in `references/workflow.md:47-48`, and the required-parameter list in
  `references/primary-commit.md:29`

## Out of scope
- Any behavior change to `Invoke-FinalizeLockClaim.ps1`,
  `Invoke-FinalizeLanding.ps1`, `Invoke-FinalizeCandidateCommit.ps1`,
  `FinalizeWorkflowCommon.psm1`, or `AgentWorktreeSession.psm1`, including adding
  a `SessionLabel` field to `Get-AgentWorktreeSessionContext` or defaulting the
  parameter
- WorktreeCli landing-lock lease, claim, and adoption semantics
- `Test-FinalizeWorkflowFixtures.ps1` and its `'finalize-fixture'` literals
- The landed change the observing session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 1 (documentation only: the fix records which existing field the
placeholder names and changes no behavior). Escalate to Tier 2 if root-causing
concludes a script or `Get-AgentWorktreeSessionContext` change is required, which
is outside this Plan's boundary and returns for re-planning. The invariant the
documentation must preserve: the value passed as `-SessionLabel` has to be
byte-identical across a session's claim, candidate-advance, landing, and release
calls, because adoption compares the recorded `session` case-sensitively
(`FinalizeWorkflowCommon.psm1:310-311`); documenting a value that varies between
calls would reintroduce the same foreign-lease block. Never embed transcript
paths or home paths.

## Acceptance criteria
- Every `<session-label>` placeholder in the finalize-changes documentation
  resolves to one named `Get-AgentWorktreeSessionContext` field through a single
  stated definition, and the primary-commit route's value is stated for the case
  where that field is `$null`
- An agent following only the finalize-changes SKILL.md and its references
  derives the identical value for the lock-claim, candidate-advance, landing, and
  release commands
- No file outside the four `## Critical files` changes
- /validate-skill passes for `finalize-changes/SKILL.md`;
  `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  `status: valid`, `code: ok`
