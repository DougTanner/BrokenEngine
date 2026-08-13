<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-09T13:58:39.901Z","dependsOn":[]} -->
# Fix: codex-review / next-plan / verify-changes / finalize-changes — nine tooling-friction defects converging on the dispatch contract

## Context

Nine distinct tooling-friction defects, observed across four separate
`/next-plan` sessions, are recorded in this one Plan because their decided or
candidate fixes converge on the same small regions — chiefly the `-ScopeFile`
host-side mechanical-check and scope-file guidance at
`.agents/skills/codex-review/SKILL.md:56-70`, and `## Required inputs` at
`.agents/skills/verify-changes/SKILL.md:16-27`. Implementing them as separate
Plans would mean repeated rebases over the same lines and a standing risk of
parallel statements of one mechanism.

Each defect keeps its own citation, its own decided fix or recorded candidate
shapes, its own authorized file set, and its own acceptance criterion. Defects
A-F were observed in one claim-to-landing cycle and two of them are ordered
against each other; G, H, and I each came from a different session. The exact
root cause of each is deferred to `/next-plan-review`.

Every skill and script named below is outside the `## In scope` of the Plan
claimed by the session that observed it, so all nine are `/next-plan` tooling
friction rather than in-scope acceptance failures. Each defect section states
the claimed Plan it was outside of.

Defects A-F were observed in session `73bf953f-b39f-40ad-a012-60c3ab59786e`
(provenance block 1). The claimed Plan for that run was
`Documents/Plans/Frame/FrameUtilsProjectMembership.md`, whose `## In scope`
covered only four `.vcxproj`/`.filters` files. Every skill and script named in
Defects A-F is outside that boundary.

### Defect A — claim-exit instructions name `Get-NextPlanContext`, a result main can never hold

`/next-plan`'s claim-exit paragraph tells main to substitute "the provisioned
`WorktreeCli` path resolved by `Get-NextPlanContext` during Preconditions and
selection" for the `<worktree-cli-path>` placeholder
(`.agents/skills/next-plan/SKILL.md:125-128`), and the tooling-friction section
repeats it for the provenance block: "Take those values from the
`Get-NextPlanContext` result already resolved in Preconditions and selection"
(`:149-151`).

No such result exists in main's hands:

- Preconditions and selection derive context from
  `Get-AgentWorktreeSessionContext`, not `Get-NextPlanContext`
  (`.agents/skills/next-plan/SKILL.md:20-24`).
- `Get-NextPlanContext` is an internal function of
  `.agents/skills/next-plan/scripts/NextPlanWorkflowCommon.psm1:18-46`, called
  only from inside the bundled scripts.
- `Invoke-NextPlanClaim.ps1` emits only
  `schemaVersion`/`status`/`code`/`message`/`claim`/`validation`
  (`:4`, `:23`, `:46`). It carries no context fields and no WorktreeCli path.

Symptom in this run: needing the session context the instruction names, the
manager guessed at a module import and PowerShell returned
`The term 'Get-AgentWorktreeSessionContext' is not recognized as a name of a
cmdlet, function, script file, or executable program.` The correct load form is
documented at `.agents/references/subagent-reporting.md:26-38`; failing to open
that reference was operator error. The unfollowable `Get-NextPlanContext`
citation is the defect: the instruction points at a result the reader cannot
obtain, and points away from the reference that would have worked.

A second, narrower ambiguity sits in the same sentence. `:124-128` says to run
`Test-NextPlanWorkflowScripts.ps1` "only when one of those scripts changes", but
"those scripts" scopes only to the claim-exit scripts named in the preceding
paragraph (`Complete-NextPlan.ps1`, `Defer-NextPlan.ps1`). Whether
`Invoke-NextPlanClaim.ps1`, `Get-NextPlanList.ps1`, and
`NextPlanWorkflowCommon.psm1` also trigger the test is undecidable from the
text, even though the test file covers the whole script set.

### Defect B — the dispatcher is told to run a `plan validate` whose argument list is never given

`.agents/skills/codex-review/SKILL.md:59-64` instructs the caller: "When a
verification dispatch needs a mechanical tool check the read-only sandbox cannot
run — WorktreeCli `plan validate`, for one — run only that non-judgment check
host-side first and put its verbatim result and identity binding in
`-ScopeFile`". It names the command and states no argument list, and no bundled
script exposes a standalone `plan validate` to a caller. Running it as named
produced:

```text
{"code":"invalid-context","schemaVersion":2,"status":"error"}
```

with exit `1`. The working form exists in the repository, but only at
`.agents/skills/create-follow-up-plans/SKILL.md:39` — a skill a manager has no
reason to open while landing — and encoded in
`.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1:22`. Recovery required
reading that script source.

### Defect C — the reviewer blocks on a receipt its Required inputs never asks the caller for

`.agents/skills/verify-changes/SKILL.md:105-117` mandates that when the reviewed
diff touches `Documents/Plans/**` a valid `plan validate` result is required;
that under `/codex-review` the read-only sandbox cannot run it — its scheduler
guard reports `guard-unavailable` there — so the dispatcher must supply a
host-side verbatim result with worktree path and baseline/head SHAs; and that
the reviewer leaves the row `BLOCKED` when that identity is missing.
`:127-129` forbids retrying. But
`.agents/skills/verify-changes/SKILL.md:16-27` "Required inputs" enumerates the
inputs the caller must supply and does not include this receipt, and neither
`.agents/skills/finalize-changes/SKILL.md:20-21` — which dispatches the landing
verify pass and states its own required inputs — nor
`.agents/skills/next-plan/SKILL.md` mentions it anywhere.

The result was:

```text
BLOCKED: missing identity-bound executable-Plan validation receipt
```

with every other acceptance row PASS, costing a full extra review round-trip on
an otherwise clean Tier-1 change.

This is reproducible for every completed-Plan landing, not a one-off:
`.agents/skills/finalize-changes/SKILL.md:66-70` makes final preparation delete
the claimed Plan file, so every such landing diff touches `Documents/Plans/**`
and always trips the trigger at `verify-changes/SKILL.md:107`.

Defects B and C are one contract: the receipt the reviewer requires is the one
the dispatcher cannot produce. Fixing either alone still leaves a landing that
either blocks or needs source-reading to unblock.

### Defect D — the tooling-friction review ends before the stage that produces most friction

`/next-plan`'s `## Tooling friction follow-ups` section
(`.agents/skills/next-plan/SKILL.md:130-158`) fixes exactly two review points:
"the claim-exit scripts above are themselves in scope: review once before
running them and again after they run, before `/finalize-changes`"
(`:141-143`). Both points precede `/finalize-changes`.

In this run all three distinct friction items that mattered at landing were
observed *during* `/finalize-changes`, after both mandated review points had
already passed: the argument-less `plan validate` failure of Defect B, the
`BLOCKED: missing identity-bound executable-Plan validation receipt` row of
Defect C, and the unfollowable context citation of Defect A.

The section's own inclusion sentence covers all three — "could not be run as
documented" and "work was repeated because a skill's instructions were unclear,
wrong, or contradicted repository state" (`:132-136`) — and the exclusion at
`:138-139` does not apply. The mandated review points provably could not observe
any of them. `.agents/skills/finalize-changes/SKILL.md` contains no occurrence of
"friction", and a repository search finds friction obligations only in
`.agents/skills/next-plan/SKILL.md` and
`.agents/skills/create-follow-up-plans/SKILL.md`, so no other skill closes the
gap.

The workaround this run used was to notice the friction manually after the fact
and rebuild the already-prepared landing commit to carry the friction Plans.

A second-order constraint any fix must respect: `:155-156` requires that "the
friction Plan file joins the landing commit alongside the `changedPaths` the
claim-exit script reported". A Plan authored after the landing commit has been
squashed cannot join it without a further candidate commit and a further
approval-preparation squash — which is exactly the rework this run performed. So
simply moving the phrase "before `/finalize-changes`" later is insufficient; the
relocated checkpoint has to fall late enough to observe landing-stage friction,
and the commit-membership sentence has to be reconciled with the rerun that
position forces.

### Defect E — no supported route rebuilds or re-messages a landing candidate

A landing candidate `e4bb8146d45321acfc6c07f5e3fa6623aa4bb2b5` had already been
created and verified when the user authorized additional in-scope content
(tooling-friction Plan content) that, per `.agents/skills/next-plan/SKILL.md:155-156`,
must join the same landing commit. The candidate therefore had to be rebuilt from
a larger authorized path set with an accurate commit message, and
`/finalize-changes` exposes no documented route to do so.

This defect was hit three separate times inside this one session, not once: the
rebuild that added tooling-friction Plan content, a second rebuild after review
fixes, and a third after the friction Plans were merged into this single file. The
second rebuild survived without a message edit only because the message then in
place happened to be count-free; the third did not, because the message's plural
description of the friction Plans went stale the moment they became one file. So
writing the message so it does not name a count postpones the defect but does not
prevent it — any rebuild whose content description changes still needs a message
the scripts cannot supply.

Content can be added: `Invoke-FinalizeCandidateCommit.ps1` can be re-run for the
newly uncommitted paths, and `Invoke-FinalizeApprovalPreparation.ps1` squashes the
resulting multi-commit range. The message cannot. Preparation selects the session
range oldest-first
(`.agents/skills/finalize-changes/scripts/Invoke-FinalizeApprovalPreparation.ps1:301`),
passes `$range[0]` as the squash's source commit (`:337`), and takes that
commit's `%B` verbatim (`:166-168`, written to the `commit-tree` message file at
`:183-196`). Its own blocker text names that source: "The oldest session commit
does not provide complete message, author, and committer metadata." (`:179`). So
stacking a second candidate on top of an already-prepared one preserves the first
candidate's subject — here `Add FrameUtils.h to both game project files`, which no
longer described the change.

No parameter overrides that. The preparation script's `param(` block declares only
`CurrentWorktree`, `PrimaryWorktree`, `CurrentBranch`, `PrimaryBranch`,
`ExpectedCurrentTip`, `ExpectedPrimaryTip`, `VerifiedCandidateCommit`,
`VerifiedCandidateTree`, and a fixture-failure switch
(`Invoke-FinalizeApprovalPreparation.ps1:26-36`) — no message input. Only
`Invoke-FinalizeCandidateCommit.ps1:14` accepts `-CommitMessageFile`, and that
message is discarded for any range the squash does not draw its oldest commit
from. No bundled script under `.agents/skills/finalize-changes/scripts/` discards
or rebuilds an already-created candidate, and no documentation supplies the route:
`.agents/skills/finalize-changes/SKILL.md` `## Normal workflow` (`:64-113`) has no
step for a candidate whose authorized content grew after creation, `## Recovery`
(`:161-176`) covers only reconciliation conflict, a death after primary advanced,
and a rewritten primary, and `references/scripts.md:13-17` describes preparation
without naming any message source at all.

Forced workaround: one raw `git reset --mixed 2c607ef8` in the session worktree to
return it to the pre-candidate state, then `Invoke-FinalizeCandidateCommit.ps1` and
`Invoke-FinalizeApprovalPreparation.ps1` re-run as documented. That deviates from
root `AGENTS.md` "run a repository script exactly as its skill documents it —
never wrap, reimplement, or work around one". The outcome was verified benign —
`git diff e4bb8146 7ccbf82e` touched only the user-authorized tooling-friction
Plan files under `Documents/Plans/Agents/` and nothing else, primary had not moved
from `2c607ef8` at that point, the discarded commit stayed reachable, and the
worktree was clean — so this is a missing supported capability, not data loss.
Primary did advance later in the session, which is what made the next paragraph's
case reachable.

When foreign primary movement and a stale message coincide, that rewind alone is
not even sufficient. Rewinding straight from the old candidate leaves the session
worktree holding pre-advance content for the paths the foreign movement changed,
and approval preparation refuses to start on it: `Test-CleanSession`
(`Invoke-FinalizeApprovalPreparation.ps1:232-235`) requires the session
worktree's `git status --porcelain --untracked-files=all` to be completely empty,
and any remaining difference blocks with
`git.session-dirty` / "Session worktree or index is not clean."
(`Invoke-FinalizeApprovalPreparation.ps1:285-288`). The sequence that actually
worked was: commit the candidate through the script, rebase that commit onto the
new primary tip, and only then rewind. So whenever primary has advanced — the
case for the later rebuilds in this session — the workaround's real cost is a
full commit-plus-rebase-plus-rewind cycle, not the single rewind the first
rebuild needed.

Reachability is routine, not incidental: any landing gate that acquires
user-authorized content after candidate creation hits it, and
`next-plan/SKILL.md:155-156` makes friction Plans exactly that case.

### Defect F — the candidate script's own precondition cannot be satisfied from inside it, and its blocker names no remedy

`Invoke-FinalizeCandidateCommit.ps1` refuses to run when any authorized path has
both a non-blank index column and a non-blank worktree column in `git status`:
`Assert-OwnedNotMixed` stops with exit `2`, code `git.owned-path-mixed-state`,
and the message "Authorized path '<path>' has mixed index and worktree state."
(`.agents/skills/finalize-changes/scripts/Invoke-FinalizeCandidateCommit.ps1:34`,
called on both routes at `:53` and `:56`).

This session's Plan file was in exactly that state, `AM`: `New-PlanFile.ps1`
stages every Plan it writes — "stage only this new Plan while preserving every
other index and worktree path", `git add -- :(literal)<plan>`
(`.agents/scripts/New-PlanFile.ps1:163-164`) — and the file was then edited in
the worktree afterwards, which is the ordinary course whenever a review finding
or a merge changes a Plan's wording. Staged add plus later unstaged edit is
precisely the rejected combination, so the script could not be run at all until
the caller ran a plain `git add -- <path>` to collapse the two columns into one.

The friction is that the precondition is unreachable from inside the script and
undocumented outside it. The blocker states the condition and not the remedy, and
no bundled documentation supplies the remedy: the only mention anywhere is
`.agents/skills/finalize-changes/references/scripts.md:9-12`, which says the
script "blocks mixed owned paths" without saying what to do about it, and
`.agents/skills/finalize-changes/SKILL.md` never names this script at all. A
caller who has not read the script source therefore has no documented route from
the blocker to the fix, and recovery once again required reading source.

### Defect G — metrics Compare blocked by the read-only sandbox

Observed in session `54c5a880-1579-4009-8eb5-24dccfa6d10e` (provenance block 2).

During this session's Change Workflow Step 5, the delegated `/repo-code-review` ran on Codex through
`/codex-review`, whose Codex invocation uses `--sandbox read-only`
(`.agents/skills/codex-review/SKILL.md`). `repo-code-review`'s `## Workflow` step 1
(`.agents/skills/repo-code-review/SKILL.md:64-71`) mandates a `code-quality-metrics` Compare run:

```
pwsh -NoProfile -File .agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetrics.ps1 -Mode Compare -Targets <supplied-targets-file> -Baseline <fixed-full-sha> -RepositoryRoot <absolute-checkout-root> -Digest
```

That run needs to write the `Temp/CodeQualityMetrics` cache, which
`.agents/skills/repo-code-review/references/metrics-protocol.md:18-20` explicitly permits as the one allowed
write. The read-only sandbox denied that write, so the Compare could not complete and the reviewer returned
`BLOCKED: mandatory code-quality metrics cache write denied` with no review content. The manager then re-ran
the same Compare host-side, where it passed, and had to reconcile that result back into the review by hand —
one wasted reviewer dispatch plus a manual rework step.

`/codex-review` already has a documented mechanism for exactly this shape of problem:
`.agents/skills/codex-review/SKILL.md:59-64` tells the caller to run a mechanical, non-judgment tool check
host-side first and embed its verbatim result in `-ScopeFile` when the read-only sandbox cannot run it. The
metrics Compare is such a check but is not covered by that path, and `repo-code-review` states no sandbox
fallback of its own, so the two contracts disagree at the point where they meet.

The failing skills are outside the claimed Plan's `## In scope`
(`Documents/Plans/Network/ClientLoadResetEpochBarrier.md`), which covers only client network C++, the project
agent fixture, and AgentHarness/AGENTS documentation.

### Defect H — scope-file guidance omits the Tier-3 plan-audit draft execution card

Observed in session `0d0c7774-565c-4822-bf8c-ff2ca3578181` (provenance block 3).

A Tier-3 `/plan-audit` was dispatched through `/codex-review` with a manager-authored `-ScopeFile` that
described the plan, the tier, and the review focus, but did not contain a draft execution card. The audit
returned no findings at all:

```
Traceability checked: blocked before execution-card comparison
Required next step: Supply the mandatory Tier-3 draft execution card: proposed implementation/review roles
and, for every acceptance criterion, its decisive check, expected result, and independent signal for
duplicated checks.
Status: BLOCKED
Residuals: Required Tier-3 draft execution card is missing from the supplied evidence.
BLOCKED: missing Tier-3 draft execution card
```

(symptom citation: `Temp/codex-plan-audit-out.md`, `Status: BLOCKED` and the final `BLOCKED:` line). The full
dispatch — prompt assembly, headless Codex run, and reviewer pass — was spent producing no review content.
The manager then re-authored the scope file with the card and re-ran the identical dispatch, which succeeded.

`/plan-audit` states the requirement in its own contract (`.agents/skills/plan-audit/SKILL.md:28`, "Draft
execution card for every Tier-2 and Tier-3 plan", consumed again at `:90` and `:96`). `/codex-review` is the
skill that assembles what the reviewer actually receives, and its scope-file guidance
(`.agents/skills/codex-review/SKILL.md:56-70`) tells the caller only to write "the exact scope, the files and
regions authorized for review, focus notes, and current residuals" — it enumerates no per-assigned-skill
required inputs. `New-CodexReviewPrompt.ps1` already branches on `$AssignedSkill` for `repo-code-review`
(`:490`) and `verify-changes` (`:480`, `:543`), so the assembly layer does know which skill it is serving; it
simply carries no obligation, check, or documentation for `plan-audit`'s card.

The two skills involved are outside the claimed Plan's `## In scope`
(`Documents/Plans/Frame/FrameUtilsSharedHelpers.md`), which authorizes only engine and game frame C++ plus the
frame AGENTS.md sentences naming moved helpers.

### Defect I — landing blocked on typed artifacts no dispatch-point list names

Observed in session `85f5008e-16f5-4f2f-b6d4-2c220ff07550` (provenance block 4).

A landing-gate `/verify-changes` dispatched through `/codex-review` returned

```text
BLOCKED: three required typed landing artifacts are incomplete or absent
```

on its first pass, while marking every one of its roughly 24 substantive
acceptance criteria `PASS`. Two of the three blocked rows are the subject of this
defect (the third is Defect C of this Plan; see `## Out of scope`):

- **Data-oracle receipt.** `.agents/skills/verify-changes/SKILL.md:96-99` requires
  every game build to carry "current passing data-oracle verification — the typed
  receipt recording that the data files still match their expected contents". The
  manager had relayed only an abbreviated SHA-256 and the builder's narrative
  "verified pass", not the exact receipt digest and the
  `broken-engine-data-oracle-verifier-result/v1` before/after results that
  `.agents/skills/compile/references/runtime-data-mode.md:34` makes the producing
  contract.
- **`/validate-skill` evidence.** `.agents/skills/verify-changes/SKILL.md:101-103`
  requires "a complete `/validate-skill` PASS handoff with mechanical self-check,
  target validator exit/output, semantic review, and no Critical finding". The
  delegated reviewer's summary stated PASS with zero findings but omitted the
  bootstrap mechanical self-check (`.agents/skills/validate-skill/SKILL.md:21`)
  and the target validator's command, exit code, and decisive output, all of which
  that skill's own `## Output` block (`:43-59`) does emit.

Both are non-judgment mechanical checks that a reviewer running under
`--sandbox read-only` (`.agents/skills/codex-review/SKILL.md:41`) cannot produce
itself. Recovery cost one full extra Codex review round at the landing gate on a
Tier-3 change: the manager ran all three host-side, appended their verbatim
results and identity bindings to the `-ScopeFile`, regenerated the prompt, and
re-dispatched a second complete `/verify-changes` round, which then returned
`PASS` on all 36 criteria against the identical diff.

The general remedy is documented — `.agents/skills/codex-review/SKILL.md:59-64`
tells the caller that "when a verification dispatch needs a mechanical tool check
the read-only sandbox cannot run ... run only that non-judgment check host-side
first and put its verbatim result and identity binding in `-ScopeFile`". What is
missing is the list: nothing at the dispatch point enumerates *which* typed
artifacts a `/verify-changes` dispatch will demand, and
`verify-changes/SKILL.md`'s own `## Required inputs` (`:16-27`) names none of
them. The list is only discoverable by reading `verify-changes`'s `## Acceptance
table` prose after the review has already blocked on it, so the wasted round is
structurally unavoidable for any manager following the documented dispatch
instructions rather than a one-off operator mistake.

A related discoverability failure cost a separate failed invocation in the same
recovery: the verifier's parameter is `-ExpectedDataRoot`
(`.agents/skills/compile/scripts/Test-DataOracleReceipt.ps1:6`), but
`.agents/skills/compile/references/runtime-data-mode.md:34` describes it in prose
only as "the exact receipt path/hash, Data path, mode, and baseline", so a first
host-side invocation using `-DataPath` failed to bind. It is recorded here rather
than separately because any fix that tells a caller to produce the data-oracle
artifact host-side must give the exact command that produces it.

The failing skills are outside the claimed Plan's `## In scope`. That Plan was
`Documents/Plans/Engine/AppDataDirectoryLaunchOverride.md`, deleted by its own
completion in landing commit `9e9e6398c62a3698a3fc9dd71718b6693a7d787e` and
readable at `git show 9e9e6398^:Documents/Plans/Engine/AppDataDirectoryLaunchOverride.md`;
it authorized only Engine `LaunchOptions`/`FileManager` C++ and harness
documentation.

### Session provenance

Machine-local; not reproducible after cleanup. Each block covers the defects
named in its first line, and `/next-plan-review` is run once per block.

Provenance block 1 — Defects A, B, C, D, E, F:

- Client: claude
- Session: 73bf953f-b39f-40ad-a012-60c3ab59786e
- Session branch: claude/73bf953f-b39f-40ad-a012-60c3ab59786e
- Worktree: .claude\worktrees\BrokenEngine\73bf953f-b39f-40ad-a012-60c3ab59786e
- Landing commit: `git log --diff-filter=A --format=%H -- Documents/Plans/Agents/NextPlanLandingCycleToolingFriction.md`
- Run the review before /cleanup-worktrees removes this worktree: Codex
  transcript discovery requires the producing worktree to remain registered,
  and Claude review requires the exact session id above.

Provenance block 2 — Defect G:

- Client: claude
- Session: 54c5a880-1579-4009-8eb5-24dccfa6d10e
- Session branch: claude/54c5a880-1579-4009-8eb5-24dccfa6d10e
- Worktree: .claude\worktrees\BrokenEnginePublic\54c5a880-1579-4009-8eb5-24dccfa6d10e
- Landing commit: `git log --diff-filter=A --format=%H -- Documents/Plans/Agents/CodexReviewMetricsCompareSandbox.md`
- Run the review before /cleanup-worktrees removes this worktree: Codex transcript discovery requires the
  producing worktree to remain registered, and Claude review requires the exact session id above.

Provenance block 3 — Defect H:

- Client: claude
- Session: 0d0c7774-565c-4822-bf8c-ff2ca3578181
- Session branch: claude/0d0c7774-565c-4822-bf8c-ff2ca3578181
- Worktree: .claude\worktrees\BrokenEngine\0d0c7774-565c-4822-bf8c-ff2ca3578181
- Landing commit: `git log --diff-filter=A --format=%H -- Documents/Plans/Agents/CodexReviewPlanAuditCardInput.md`
- Run the review before /cleanup-worktrees removes this worktree: Codex transcript discovery requires the
  producing worktree to remain registered, and Claude review requires the exact session id above.

Provenance block 4 — Defect I:

- Client: claude
- Session: 85f5008e-16f5-4f2f-b6d4-2c220ff07550
- Session branch: claude/85f5008e-16f5-4f2f-b6d4-2c220ff07550
- Worktree: .claude\worktrees\BrokenEngine\85f5008e-16f5-4f2f-b6d4-2c220ff07550
- Landing commit: `git log --diff-filter=M -S'landing blocked on typed artifacts no dispatch-point list names' --format=%H -- Documents/Plans/Agents/CodexReviewMetricsCompareSandbox.md`
  (Defect I was recorded in a Plan file that was merged into this one before it
  ever landed, so the commit to review is the one that added Defect I's section
  to this Plan, not the earlier one that first added this Plan file. The search
  string is the `### Defect I` heading text, which that commit introduces.)
- Run the review before /cleanup-worktrees removes this worktree: Codex
  transcript discovery requires the producing worktree to remain registered, and
  Claude review requires the exact session id above.

## Design

In a new session, run `/next-plan-review <landing commit>` supplying the
recorded client and session id for each provenance block, root-cause the nine
defects from the proven transcripts, then make the smallest fix inside the
`## In scope` boundary below. If root-causing shows a fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

Defects A through F carry decided fix directions; none of them is an open
option. Defects G, H, and I record the candidate shapes their symptoms already
narrow to, and root-causing decides among those recorded candidates — they are
alternatives, not sets to implement together.

### Defect A — two corrections in `next-plan/SKILL.md`

- Both sentences name the context source main actually has,
  `Get-AgentWorktreeSessionContext`, and cite its load form to
  `.agents/references/subagent-reporting.md:26-38` instead of restating it. The
  reason: root `AGENTS.md` DRY and "define a genuinely new term once at its
  owning doc and reference it elsewhere" make that reference the single
  statement of the load form, and it is already correct as written, so an inline
  copy in `next-plan/SKILL.md` is rejected.
- The `Test-NextPlanWorkflowScripts.ps1` trigger names its exact set: any file
  under `.agents/skills/next-plan/scripts/` — `Complete-NextPlan.ps1`,
  `Defer-NextPlan.ps1`, `Get-NextPlanList.ps1`, `Invoke-NextPlanClaim.ps1`, and
  `NextPlanWorkflowCommon.psm1` — because the test file covers that whole set,
  not only the claim-exit scripts named in the preceding paragraph.

### Defects B and C — one receipt contract, one authoritative argument list

Treat the two halves as one contract with one owner per end: the requirement is
declared where the caller reads it before dispatching, and the command is
reachable in one step from where the caller is told to run it.

The decided authoritative location is the existing statement at
`.agents/skills/create-follow-up-plans/SKILL.md:39`, which stays the repository's
single statement of the
`plan validate --repo <absolute-git-common-dir> --worktree <checkout>` argument
list. `.agents/skills/codex-review/SKILL.md` and
`.agents/skills/verify-changes/SKILL.md` cite that statement by path; neither
repeats the argument list, and no third copy is created anywhere. The reason:
root `AGENTS.md` DRY and "define a genuinely new term once at its owning doc and
reference it elsewhere", plus the fact that a citation already satisfies the
one-step reachability this Plan requires, so copying would add a second thing to
keep in sync for no gain.

The identity-bound receipt is added to `verify-changes/SKILL.md`
`## Required inputs` as a caller-supplied item conditioned on a
`Documents/Plans/**` diff, consistent with the existing trigger at `:105-117`.

One sentence in `finalize-changes/SKILL.md` `## Inputs and ownership` (`:20-21`)
also names that receipt among the inputs the landing verify dispatch carries.
This is decided, not conditional on what a later review finds. The reason: that
sentence is where the landing dispatcher's own required inputs are stated, and
`finalize-changes/SKILL.md:21-23` makes the landing `/verify-changes` pass part
of this skill's workflow, so a dispatcher reading only its own inputs would
otherwise learn of the receipt only after the reviewer blocks on it — which is
the defect. Because final preparation always deletes the claimed Plan file
(`finalize-changes/SKILL.md:66-70`), this case fires at every completed-Plan
landing, so the sentence is always load-bearing rather than an edge case.

### Defect D — where the final friction review point sits

The outcome the fix must deliver: the final friction review point falls after
`/finalize-changes` `## Normal workflow` step 3 — the landing `/verify-changes`
acceptance pass on the final diff — has returned PASS, and before the step-4
landing summary and the single user landing confirmation. That position, not
merely "after preparation", is what makes landing-stage friction observable: the
receipt failure of Defect C surfaced inside that verification pass, so a
checkpoint placed before it would miss the case again.

That position necessarily forces a rerun, and the decided fix accepts a bounded
one rather than claiming to avoid it. A Plan file authored after preparation has
squashed the session range is a new path in the worktree; no message capability
can put it into an already-built commit. `Invoke-FinalizeApprovalPreparation.ps1`
takes its replacement tree from the session tip (`:323`) and its message from the
oldest commit of the range above primary (`:301`, `:337`, `:166-168`), so the
only bundled route from "new authorized path" to "one prepared landing commit"
is: create a second candidate commit for that path through
`Invoke-FinalizeCandidateCommit.ps1`, then re-invoke
`Invoke-FinalizeApprovalPreparation.ps1` over the now two-commit range with the
Defect E `-CommitMessageFile` override supplying a message that describes the
enlarged content. Defect E is what makes that rerun produce an accurate message
instead of the first candidate's stale subject; it does not remove the rerun.

The commit-membership sentence at `:155-156` therefore states that route
explicitly and bounds it: exactly one further candidate commit and one further
approval-preparation run, using only bundled scripts invoked as documented with
no hand-run Git, followed by re-review of the affected regions of the changed
diff, and then the landing summary and the single landing confirmation. The
bound is what keeps the checkpoint from looping: the friction Plan is authored
once, so the rerun happens at most once per landing.

`/next-plan` owns the checkpoint alone; the Defect D fix does not edit
`.agents/skills/finalize-changes/SKILL.md`. The reason: `next-plan/SKILL.md`
already orders its friction review against the other skill by citation ("before
`/finalize-changes`", `:141-143`) without that skill carrying a reciprocal
sentence, and the milestones the relocated checkpoint needs are already named and
numbered in `finalize-changes/SKILL.md` `## Normal workflow` (`:75-81`
verification, `:82-98` landing summary, `:99-104` confirmation), so the ordering
is expressible wholly from `/next-plan` by citing them. That keeps the Defect D
edit inside one skill package.

### Defect E — an optional message override on approval preparation

The outcome the fix must deliver: a caller whose landing candidate gains
authorized content after creation reaches a single prepared commit carrying both
the full authorized path set and a message that describes it, using only bundled
scripts invoked as documented.

The decided mechanism is an optional `-CommitMessageFile` message override on
`Invoke-FinalizeApprovalPreparation.ps1`. When it is supplied, preparation takes
that file's text as the replacement commit's message instead of the oldest
session commit's `%B` (`Invoke-FinalizeApprovalPreparation.ps1:168`), and builds
the replacement commit even when the session range holds a single commit, whose
current `one-commit-no-op` path (`:328-334`) would otherwise leave the stale
message in place; author and committer metadata still come from the oldest
session commit. The reason: content can already be added by re-running
`Invoke-FinalizeCandidateCommit.ps1`, so only the message is unreachable, and an
input parameter is the smallest change that closes that gap while leaving the
script's compare-and-swap, tree-identity, and rollback guarantees untouched. The
rejected alternative — a new discard-and-rebuild operation returning the session
to its pre-candidate state — is strictly larger for the same outcome and can
reach landing coordination. `-CommitMessageFile` reuses the parameter name
`Invoke-FinalizeCandidateCommit.ps1:14` already uses for the same concept.

A binding constraint on that mechanism: it must work in the case where the
candidate must also be rebased. Foreign primary movement and a stale message
coincide routinely, and the rewind workaround is only sound after the candidate
has been committed and rebased onto the new primary tip, because rewinding first
leaves foreign-path content that `git.session-dirty` rejects
(`Invoke-FinalizeApprovalPreparation.ps1:232-235`, `:285-288`). The
`-CommitMessageFile` override satisfies this by construction — preparation
already runs after the caller has reconciled the session onto the current primary
tip, and the override changes only the message the replacement commit carries —
but any fix accepted in its place must be usable at that same point, on an
already-rebased session range, without a preparatory rewind. A fix that requires
the worktree to be returned to a pre-candidate state before rebasing is rejected
for that reason.

The resulting route is stated in `finalize-changes/SKILL.md` and
`references/scripts.md`, including where preparation's message comes from when no
override is supplied, so the current silent oldest-commit inheritance stops being
invisible.

### Defect F — the remedy is stated in the blocker and in the script contract

The outcome the fix must deliver: a caller blocked by `git.owned-path-mixed-state`
can act on the blocker alone, without opening the script source.

Two edits, both stating the same remedy — stage the named path with a plain
`git add -- <path>` so its index and worktree columns collapse to one, then
re-invoke the script:

- `Invoke-FinalizeCandidateCommit.ps1:34` extends its blocker message text to name
  that remedy alongside the condition it already names. Exit code `2` and the
  `git.owned-path-mixed-state` code string are unchanged, so nothing that keys on
  the result contract moves.
- `.agents/skills/finalize-changes/references/scripts.md:9-12`, the script's
  contract entry and the repository's only prose about this check, states the same
  remedy where a caller reads the contract before invoking. `finalize-changes/SKILL.md`
  is not edited for this defect: it never names this script, and adding a first
  mention there would create a second place to keep in sync for no gain.

The reason for fixing both ends rather than one: the blocker is what a caller
sees at the moment of failure, and the contract entry is what a caller reads
before invoking. Stating the remedy in only one of them still leaves the other
naming a condition with no action, which is the defect.

### Defect G — the recorded candidate shapes

Two candidate shapes are already visible from the symptom, and root-causing decides between them: either
`New-CodexReviewPrompt.ps1` pre-runs the metrics Compare host-side for the `repo-code-review` assigned skill
and embeds the verbatim digest in the prompt, the way it already writes the targets file for that same skill;
or `repo-code-review`'s step 1 and `metrics-protocol.md` state the read-only-sandbox fallback explicitly so
the reviewer proceeds on an embedded result instead of returning `BLOCKED`.

### Defect H — the recorded candidate shapes

The symptom already narrows the candidates: either `/codex-review`'s scope-file guidance enumerates the
per-assigned-skill required inputs — at minimum the Tier-3 `plan-audit` draft execution card — so the caller
authors it before dispatching, or `New-CodexReviewPrompt.ps1` fails fast on a `plan-audit` dispatch whose
scope text carries no card, the way it already special-cases other assigned skills. Root-causing decides
between them; a caller-side documentation fix and a script-side guard are not both required.

### Defect I — the recorded candidate shapes

The outcome the fix must deliver: a manager dispatching `/verify-changes` through
`/codex-review`, reading only what the dispatch route tells it to read, can
produce every typed mechanical artifact that dispatch will demand before the
review runs — so a first pass does not block on a missing artifact whose
requirement was only discoverable from the reviewer's own acceptance prose.

Three candidate shapes are visible from the symptom, and root-causing decides
among them; they are alternatives, not a set to implement together:

- `verify-changes/SKILL.md` publishes an explicit pre-dispatch list of the typed
  artifacts it requires and the exact command producing each, in or beside its
  `## Required inputs`, so the caller authors them before dispatching.
- `New-CodexReviewPrompt.ps1` runs and embeds them automatically for
  `-AssignedSkill verify-changes`, the way it already writes and embeds the
  targets file for `repo-code-review` (`:490`) and already special-cases
  `verify-changes` at `:480` and `:543`.
- `/codex-review`'s host-side mechanical-check sentence (`SKILL.md:59-64`) names
  them at the dispatch point.

Whichever shape is chosen, the exact `Test-DataOracleReceipt.ps1` parameter names
— `-ExpectedDataRoot` among them — must be reachable in one step from wherever
the caller is told to produce the data-oracle artifact, either by stating them or
by citing the script's parameter block.

### Converging regions — one statement per mechanism

Four of these defects change the same two regions, and this Plan owns all of
them, so the reconciliation that would otherwise be cross-Plan coordination is an
internal implementation constraint:

- `.agents/skills/codex-review/SKILL.md:56-70` is touched by Defect B (a
  citation making the `plan validate` argument list reachable from the host-side
  mechanical-check sentence), by Defect G (extending that same host-side
  mechanical-check mechanism to the `code-quality-metrics` Compare case), by
  Defect H (the per-assigned-skill required inputs a caller must author before
  dispatching), and possibly by Defect I depending on which candidate shape
  root-causing selects. The content is disjoint, but the region is one: state the
  host-side mechanical-check mechanism exactly once and add each case to that one
  statement, never a parallel one. If a per-assigned-skill mechanism is
  established under Defect H, Defect I's typed-artifact list is one entry in it
  rather than a second mechanism beside it.
- `.agents/skills/verify-changes/SKILL.md` `## Required inputs` (`:16-27`) is
  touched by Defect C (the identity-bound executable-Plan `plan validate`
  receipt) and possibly by Defect I (the data-oracle receipt and the
  `/validate-skill` handoff fields). Those artifacts are stated once as one list,
  not as two lists added separately.

Whichever defect is implemented second in either region re-reads that region
before editing it.

### Implementation ordering constraint

Defect E's message-override route must exist before Defect D's acceptance
criterion "the landing-commit membership requirement remains satisfiable from
that position through the bounded rerun" can be met: the relocated checkpoint's
Plan file joins the landing commit through a second candidate commit and a
re-run of approval preparation, and without the `-CommitMessageFile` override
that rerun inherits the first candidate's stale message, leaving hand-run Git as
the only way to describe the enlarged commit. Implement E before D, or implement
them together and land them in one change; never land the relocated checkpoint
while the only route to satisfy its membership requirement is hand-run Git.

The other seven defects — A, B, C, F, G, H, and I — are order-independent with
those two and with each other, subject to the region re-read stated above.
Defects E and F both edit
`.agents/skills/finalize-changes/references/scripts.md`, in different entries
(`:13-17` and `:9-12`); that is an editing adjacency to re-read before the second
edit, not an ordering requirement.

## Critical files

- `.agents/skills/next-plan/SKILL.md` — the claim-exit paragraph at `:124-128`
  and the provenance sentence at `:149-151` (Defect A); `## Tooling friction
  follow-ups` (`:130-158`), specifically the review-point sentence at `:141-143`
  and the commit-membership sentence at `:155-156` (Defect D)
- `.agents/skills/codex-review/SKILL.md` — the `-ScopeFile` host-side mechanical
  check guidance at `:56-70` (Defects B, G, H, and possibly I)
- `.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1` — the
  `$AssignedSkill` branches at `:480`, `:490`, and `:543` (Defects G, H, I)
- `.agents/skills/codex-review/references/prompt-template.md` (Defects G, H, I)
- `.agents/skills/verify-changes/SKILL.md` — `## Required inputs` (`:16-27`) and
  the `## Executable Plan check` trigger it must match (`:105-117`) (Defect C);
  the typed-artifact requirements in `## Acceptance table` (`:96-103`) (Defect I)
- `.agents/skills/finalize-changes/SKILL.md` — `## Inputs and ownership`
  (`:20-21`), which names what the landing verify dispatch must carry (Defect C);
  `## Normal workflow` step 2 (`:71-74`) and `## Recovery` (`:161-176`), which
  must state the message-override route (Defect E)
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeApprovalPreparation.ps1`
  — the `param(` block (`:26-36`), oldest-first range selection (`:301`), the
  squash source-commit argument (`:337`), and `New-ReplacementCommit`'s `%B`
  derivation (`:166-186`) (Defect E)
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeCandidateCommit.ps1` —
  `Assert-OwnedNotMixed`'s blocker message at `:34`, which must name the remedy
  (Defect F). Its call sites at `:53` and `:56` are read-only context
- `.agents/skills/finalize-changes/references/scripts.md` — the preparation
  contract at `:13-17`, which must state the message source (Defect E); the
  candidate-commit contract at `:9-12`, which must state the mixed-state remedy
  (Defect F)
- `.agents/skills/repo-code-review/SKILL.md` — `## Workflow` step 1 (`:64-71`),
  the mandated Compare invocation (Defect G)
- `.agents/skills/repo-code-review/references/metrics-protocol.md` — the allowed
  cache write at `:18-20` (Defect G)
- `.agents/skills/plan-audit/SKILL.md` — the draft execution card requirement at
  `:28`, consumed at `:90` and `:96` (Defect H)
- `.agents/skills/compile/references/runtime-data-mode.md` — the receipt
  producer/verifier contract sentence at `:34` (Defect I)
- `.agents/scripts/New-PlanFile.ps1` — `:163-164`, which stages every Plan it
  writes and so produces the mixed state whenever that Plan is later edited.
  Read-only evidence for Defect F; correct as written and not edited
- `.agents/skills/create-follow-up-plans/SKILL.md` — `:39`, the sole and
  authoritative statement of the `--repo`/`--worktree` argument list. Cited by
  `codex-review` and `verify-changes`, and not edited
- `.agents/skills/compile/scripts/Test-DataOracleReceipt.ps1` — the `param(`
  block at `:3-9`, read-only evidence for the parameter names; correct as
  written and not edited
- `.agents/skills/validate-skill/SKILL.md` — the bootstrap self-check at `:21`
  and the `## Output` block at `:43-59`, read-only evidence that the handoff
  fields `/verify-changes` demands already exist; not edited
- `.agents/skills/finalize-changes/SKILL.md` `## Normal workflow` (`:75-81`
  verification, `:82-98` landing summary, `:99-104` confirmation) — read-only for
  Defect D: the relocated checkpoint cites these steps and does not edit them

## In scope

- Root-cause investigation via `/next-plan-review` with the recorded provenance,
  covering all nine defects, once per provenance block
- Defect A: the two sentences in `.agents/skills/next-plan/SKILL.md:124-128` and
  `:149-151` — naming `Get-AgentWorktreeSessionContext` with a citation to
  `.agents/references/subagent-reporting.md` rather than an inline copy of its
  load form, and the `Test-NextPlanWorkflowScripts.ps1` trigger scope
- Defect B: making the `plan validate` argument list reachable in one step from
  `.agents/skills/codex-review/SKILL.md:56-70` by citing
  `.agents/skills/create-follow-up-plans/SKILL.md:39`, the authoritative
  statement
- Defect C: adding the identity-bound `plan validate` receipt to
  `.agents/skills/verify-changes/SKILL.md` `## Required inputs` (`:16-27`) as a
  caller-supplied item conditioned on a `Documents/Plans/**` diff, consistent
  with the existing trigger at `:105-117`, citing the same authoritative argument
  list; and exactly one sentence in
  `.agents/skills/finalize-changes/SKILL.md` `## Inputs and ownership` (`:20-21`)
  naming the receipt among the inputs the landing verify dispatch carries. That
  sentence is authorized unconditionally, not subject to a reviewer's judgment
- Defect D: moving the second of the two friction review points that
  `.agents/skills/next-plan/SKILL.md:141-143` mandates — the one currently stated
  as "again after they run, before `/finalize-changes`" — so it falls after the landing
  `/verify-changes` acceptance pass on the final diff has returned PASS and
  before the landing summary and the single landing confirmation, and rewriting
  the landing-commit membership sentence at `:155-156` to state the bounded rerun
  that position forces — one further candidate commit for the friction Plan path
  and one further approval-preparation run carrying the Defect E
  `-CommitMessageFile` override, then re-review of the affected regions. The
  authorized file set for this defect is exactly one file:
  `.agents/skills/next-plan/SKILL.md`
- Defect E: the optional `-CommitMessageFile` parameter and the message
  derivation and single-commit replacement behavior it controls in
  `.agents/skills/finalize-changes/scripts/Invoke-FinalizeApprovalPreparation.ps1`
  (`param(` block `:26-36`, `New-ReplacementCommit` `:166-186`, the
  `one-commit-no-op` path `:328-334`, and the squash source at `:337`), plus the
  `.agents/skills/finalize-changes/SKILL.md` (`:71-74`, `:161-176`) and
  `.agents/skills/finalize-changes/references/scripts.md` (`:13-17`) prose that
  documents the route and the default message source. Any accepted mechanism must
  be usable on a session range that has already been rebased onto a moved primary
  tip, without a preparatory rewind
- Defect F: the `Assert-OwnedNotMixed` blocker message text in
  `.agents/skills/finalize-changes/scripts/Invoke-FinalizeCandidateCommit.ps1:34`,
  which gains the `git add -- <path>` remedy while keeping its exit code and its
  `git.owned-path-mixed-state` code string unchanged; and the candidate-commit
  contract entry in
  `.agents/skills/finalize-changes/references/scripts.md:9-12`, which states the
  same remedy. The authorized file set for this defect is exactly those two files
- Defect G: the `/codex-review` prompt assembly and its documented host-side
  mechanical-check path, and `repo-code-review`'s `## Workflow` step 1 plus its
  metrics protocol reference. The authorized file set for this defect is exactly
  `.agents/skills/codex-review/SKILL.md`,
  `.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1`,
  `.agents/skills/codex-review/references/prompt-template.md`,
  `.agents/skills/repo-code-review/SKILL.md`, and
  `.agents/skills/repo-code-review/references/metrics-protocol.md`
- Defect H: `/codex-review`'s scope-file authoring guidance and its
  assigned-skill handling in prompt assembly, and `/plan-audit`'s statement of
  the evidence its caller must supply. The authorized file set for this defect is
  exactly `.agents/skills/codex-review/SKILL.md`,
  `.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1`,
  `.agents/skills/codex-review/references/prompt-template.md`, and
  `.agents/skills/plan-audit/SKILL.md`
- Defect I: the pre-dispatch statement of which typed mechanical artifacts a
  `/verify-changes` dispatch requires and the exact command producing each,
  placed in whichever one of `.agents/skills/verify-changes/SKILL.md`,
  `.agents/skills/codex-review/SKILL.md`, or
  `.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1` root-causing
  shows is the owner; and the one-step reachability of
  `Test-DataOracleReceipt.ps1`'s parameter names from
  `.agents/skills/compile/references/runtime-data-mode.md:34`. The authorized
  file set for this defect is exactly those three files plus
  `.agents/skills/codex-review/references/prompt-template.md` and
  `.agents/skills/compile/references/runtime-data-mode.md`

Each defect's authorized file set is the one stated in its own clause above; the
union below is the whole-Plan ceiling and never widens an individual defect's
boundary. The complete authorized file set for the whole Plan is exactly these
thirteen files: `.agents/skills/next-plan/SKILL.md`,
`.agents/skills/codex-review/SKILL.md`,
`.agents/skills/codex-review/scripts/New-CodexReviewPrompt.ps1`,
`.agents/skills/codex-review/references/prompt-template.md`,
`.agents/skills/verify-changes/SKILL.md`,
`.agents/skills/finalize-changes/SKILL.md`,
`.agents/skills/finalize-changes/scripts/Invoke-FinalizeApprovalPreparation.ps1`,
`.agents/skills/finalize-changes/scripts/Invoke-FinalizeCandidateCommit.ps1`,
`.agents/skills/finalize-changes/references/scripts.md`,
`.agents/skills/repo-code-review/SKILL.md`,
`.agents/skills/repo-code-review/references/metrics-protocol.md`,
`.agents/skills/plan-audit/SKILL.md`, and
`.agents/skills/compile/references/runtime-data-mode.md`.

## Out of scope

- The landed change each recorded session produced, and its completed Plan
- Fixing any of these nine defects by editing a skill outside the authorized file
  set above: a defect whose fix provably lies elsewhere is surfaced for
  re-planning, never absorbed by an unrelated skill
- `.agents/skills/next-plan/scripts/**`, `.agents/references/subagent-reporting.md`,
  `.agents/scripts/New-PlanFile.ps1`, and WorktreeCli itself, all correct as
  written. In particular, New-PlanFile.ps1 staging what it writes is correct and
  is merely why the mixed state arises
- Any second copy of the `plan validate` argument list: the statement at
  `.agents/skills/create-follow-up-plans/SKILL.md:39` is authoritative and
  unchanged, and every other skill cites it
- `/create-follow-up-plans` generally, whose friction-proposal contract is
  unchanged
- The substance of the `## Executable Plan check` rule itself, its bounded `busy`
  re-run allowance, and the no-retry rule at `verify-changes/SKILL.md:127-129`
- The substance of the typed-artifact requirements themselves: that a game build
  needs a passing data-oracle verification, that a skill change needs a complete
  `/validate-skill` PASS handoff, all stay exactly as they are. Making an
  artifact easier to supply must never make it optional
- The data-oracle producer and verifier scripts' behavior, schemas, exit codes,
  and fail-closed checks, and `/validate-skill`'s own workflow and output
  contract
- The read-only sandbox itself and the `/codex-review` fallback contract
- `.agents/skills/code-quality-metrics/` and its analyzer or cache behavior; the
  metrics result stays advisory and must not become a review finding
- The third blocked row of Defect I's first pass — the missing identity-bound
  executable-Plan `plan validate` receipt — is Defect C of this Plan and is not a
  separate item under Defect I
- Whether final preparation deletes the claimed Plan file, which is correct
  behavior and merely the reason the trigger always fires
- `.agents/skills/finalize-changes/SKILL.md` edits under Defect D: that
  checkpoint cites `## Normal workflow` steps 3-5 and does not change them
- `Invoke-FinalizeCandidateCommit.ps1`'s authorized-path validation and resumed
  `-OwnedPaths` contract, owned by
  `Documents/Plans/Agents/FinalizeCandidateResumedOwnedPaths.md`. Defect F's edit
  to that script is confined to the `Assert-OwnedNotMixed` blocker message text at
  `:34` and touches neither of those
- The mixed-state check itself: which column combinations it rejects, that it
  rejects them, its exit code `2`, and its `git.owned-path-mixed-state` code
  string all stay exactly as they are — Defect F adds a remedy to the message and
  changes no behavior a caller or fixture keys on
- Any discard-and-rebuild operation that returns the session to its
  pre-candidate state; the decided message override replaces it
- The landing lock, compare-and-swap primary advance, rollback, claim release,
  and the landing confirmation contract, none of which any of these fixes changes
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Change Workflow Tier 3 for the Plan as a whole, triggered by root `AGENTS.md` "a
change spanning independently owned subsystems": the work spans six
independently owned skill packages — `next-plan`, `codex-review`,
`verify-changes`, `finalize-changes`, `repo-code-review`, and `plan-audit`, plus
one `compile` reference file — and Defects B and C form a cross-skill
handoff contract whose producing half (`codex-review`) and consuming half
(`verify-changes`) are owned separately, so an edit to one that does not match
the other reintroduces the block. Defects G, H, and I extend that same
producing-half contract, so the same mismatch risk applies to them.

Taken alone, Defects A and F would each be Tier 1 ("documentation, style, project
membership, or local behavior-preserving work with no public signature or
invariant exposure") and Defects D, E, G, H, and I would each be Tier 2 ("one
subsystem's runtime or tool behavior"). Defect F is Tier 1 despite touching a
script because its only script edit is the text of a blocker message: the check's
condition, its exit code, and its `git.owned-path-mixed-state` code string — the
parts callers and the bundled fixture suite key on — are unchanged, so no
signature or invariant is exposed. Those lower per-defect tiers do not lower the
Plan's tier. A future implementer may stage the work — for example landing
Defects A and F separately, or landing E before D per the ordering constraint
above — and each stage is classified at its own highest applicable tier; the
combined change remains Tier 3.

Invariants that must survive:

- The reviewer still evaluates each supplied artifact and receipt itself and
  still blocks on a missing, incomplete, or identity-mismatched one — making an
  artifact easier to supply must not make it optional, and must not let the
  sandbox-side result substitute for the host-side one.
- `New-CodexReviewPrompt.ps1` must keep copying manager-authored scope text
  verbatim and must never author, summarize, or edit review judgment content.
- Exactly one explicit user confirmation authorizes changing primary. A
  checkpoint added before that confirmation must not introduce a second approval
  round-trip or delay the confirmation past a meaningful diff change, and a
  rebuilt candidate is a changed diff that re-runs review of the affected regions
  rather than reusing an earlier confirmation.
- Preparation still produces exactly one commit on the current primary tip whose
  tree is identical to the verified candidate tree, still blocks
  `git.primary-not-ancestor` and merge-containing ranges, still rolls back a
  replacement whose postconditions fail, and never advances primary.

Never embed transcript paths or home paths. No determinism/CRC, serialization,
replay, wire, threading, allocation, shader, build, or live-verification
exposure.

## Coordination

- `Documents/Plans/Agents/FinalizeCandidateResumedOwnedPaths.md` edits the same
  `.agents/skills/finalize-changes/SKILL.md` normal-workflow and recovery prose,
  the same `references/scripts.md` contract list including the candidate-commit
  entry at `:9-12`, and — under Defect F — the same
  `Invoke-FinalizeCandidateCommit.ps1`. The two script edits are disjoint by
  construction: that Plan owns literal authorized-path validation and the
  `-OwnedPaths` contract, this one owns only the `Assert-OwnedNotMixed` blocker
  message text. Whichever lands second rebases onto the first and re-reads those
  regions before editing.

## Acceptance criteria

- Defect A: every function or result name the corrected sentences cite is
  reachable by the agent the sentence addresses, proven by naming where that
  agent obtains it, and the `Test-NextPlanWorkflowScripts.ps1` trigger names its
  exact script set
- Defects B and C: a completed-Plan landing whose diff deletes the claimed Plan
  reaches `Verification: PASS` without the `BLOCKED: missing identity-bound
  executable-Plan validation receipt` row and without an extra review round-trip;
  the `plan validate` argument list is reachable in one step from the sentence
  that tells the caller to run it, and the repository still states that argument
  list authoritatively in one place; `finalize-changes/SKILL.md` `## Inputs and
  ownership` names the receipt among the inputs the landing verify dispatch
  carries; and the reviewer still returns `BLOCKED`
  when the supplied receipt's worktree or baseline/head SHAs are missing or do
  not match the reviewed checkout and diff
- Defect D: the final friction review point is stated at a position after the
  landing `/verify-changes` acceptance pass on the final diff has returned PASS
  and before the landing summary and the landing confirmation, so a friction item
  first observable during `/finalize-changes` — including one raised by that
  verification pass itself — falls inside it; the landing-commit membership
  requirement remains satisfiable from that position through the bounded rerun
  stated in the text — exactly one further `Invoke-FinalizeCandidateCommit.ps1`
  candidate commit for the friction Plan path and one further
  `Invoke-FinalizeApprovalPreparation.ps1` run carrying the Defect E
  `-CommitMessageFile` override, using only bundled scripts invoked as
  documented with no hand-run Git, followed by re-review of the affected regions
  — and the number of user confirmations required to land is still exactly one
- Defect E: adding authorized content to an already-created landing candidate
  reaches a single prepared commit carrying the full authorized path set and a
  message describing it, using only bundled scripts invoked exactly as
  documented, with no raw `git reset`, `commit --amend`, or other hand-run Git;
  the message source for the squashed commit is stated in
  `finalize-changes/SKILL.md` and `references/scripts.md`, including the behavior
  when no override is supplied; the same route reaches that outcome when the
  candidate must also be rebased onto a moved primary tip, with no rewind of the
  session worktree ahead of the rebase and no `git.session-dirty` block; and the
  prepared commit's tree still equals the verified candidate tree, primary is
  still unchanged by preparation, and a failed replacement still rolls back
- Defect F: a caller whose authorized path is in mixed index-and-worktree state
  can reach a created candidate acting only on what the blocker and
  `references/scripts.md` state, without opening the script source — the remedy
  `git add -- <path>` followed by re-invocation appears in both the blocker
  message and the candidate-commit contract entry; and the check still blocks the
  same states with exit `2` and code `git.owned-path-mixed-state`
- Defect G: the recorded symptom no longer reproduces: a `/repo-code-review`
  dispatched through `/codex-review` in the read-only sandbox completes its
  mandated Compare step without a cache-write denial and without the manager
  re-running it host-side
- Defect H: the recorded symptom no longer reproduces: a Tier-3 `/plan-audit`
  dispatched through `/codex-review` by a caller following the documented
  guidance does not return `BLOCKED: missing Tier-3 draft execution card`
- Defect I: the recorded symptom no longer reproduces: a `/verify-changes`
  dispatched through `/codex-review` by a caller following the documented
  dispatch route does not return a first-pass `BLOCKED` whose only blocking rows
  are the data-oracle receipt or the `/validate-skill` handoff fields, and needs
  no second full review round to supply them; every typed artifact that dispatch
  requires is named before dispatch together with the exact command that produces
  it, and each named command binds its parameters on a first invocation —
  including the data-oracle verifier's `-ExpectedDataRoot`; and the reviewer
  still returns `BLOCKED` when a required artifact is genuinely absent,
  incomplete, or identity-mismatched against the reviewed checkout and diff
- The `.agents/skills/codex-review/SKILL.md:56-70` region ends up with one
  statement of the host-side mechanical-check mechanism covering every case this
  Plan adds, and `verify-changes/SKILL.md` `## Required inputs` ends up with one
  list of caller-supplied artifacts, not parallel statements added per defect
- `/validate-skill` passes for every changed `SKILL.md`
- WorktreeCli `plan validate` exits `0` with `status: valid` and `code: ok`

## Notes

The `Get-AgentWorktreeSessionContext` import failure observed in the Defect A
run was operator error, not a defect in
`.agents/references/subagent-reporting.md`. It is recorded only because the
misdirecting citation is what sent the reader away from that reference.

This Plan is the consolidation of four separately recorded tooling-friction
Plans whose fixes converged on `.agents/skills/codex-review/SKILL.md:56-70` and
`.agents/skills/verify-changes/SKILL.md:16-27`. It keeps the earliest
`createdUtc` of the four, and therefore the queue position the earliest of them
had already earned. The deleted files were
`Documents/Plans/Agents/CodexReviewPlanAuditCardInput.md` (Defect H),
`Documents/Plans/Agents/NextPlanLandingCycleToolingFriction.md` (Defects A-F),
and `Documents/Plans/Agents/VerifyChangesTypedArtifactPreDispatch.md`
(Defect I); the provenance blocks above name each one where its landing commit
must still be looked up.
