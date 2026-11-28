# Finalizer Worker

Worker-only mechanics for `/finalize-changes`.

## Bundled scripts

Every script this skill runs lives under `../scripts/`; its command line, result
contract, and lease rules are in [`scripts.md`](scripts.md).

Run them as the root AGENTS.md `## Directives` bundled-scripts bullet requires,
in its canonical invocation form, and stop on a script that cannot be run as
documented: an improvised invocation can send wrong arguments into an operation
that changes primary. `Show-FinalizeApprovalReview.ps1` is the one script this
worker fills in and returns for main to run verbatim; this worker never invokes
it.

Under its landing lease, finalization follows the Verify and land step's rule
that exactly one explicit user confirmation authorizes changing primary, and the
exact mechanics in [`scripts.md`](scripts.md).

Immediately after confirmation the caller claims the landing lock once through
`../scripts/Invoke-FinalizeLockClaim.ps1`, with the landing lease duration
`scripts.md` states, and passes that owner token to
`../scripts/Invoke-FinalizeLanding.ps1` as `-OwnerToken`, which continues under
that same lease through the advance. This supplied-token route keeps the raw
`$SessionLabel`; when `-OwnerToken` is omitted, landing uses
`$SessionLabel/landing`, adopts a live retained claim only when that exact
session and the same canonical worktree match, and otherwise mints its own token
through WorktreeCli `lock token` before claiming under the derived identity.

Ownership rule: a lease is a same-actor continuation when its recorded session
and worktree match the current landing identity; an omitted-token invocation may
also adopt the live owner recorded for that exact derived landing identity;
every other lease is foreign.

## Steps

Ordinary preparation runs steps 1-8; post-confirmation engine-landing recovery
(`## Recovery`) enters at step 4;
landing runs steps 9-15 from `SKILL.md` `### Landing confirmation`'s capsule.
A blocked result that needs recovery returns a handoff: main dispatches a fresh
preparation worker whose capsule carries that blocked disposition as its
unresolved issue and, for an overlap-recovery blocker (original
`primary.path-overlap` or step-5 `needs-review`),
`Temp/finalize-primary-movement-result.json` as path plus selector. Every
preparation dispatch recovers `primary.path-overlap` itself under
`## Recovery`.

1. Take the final preparation result first when a claimed Plan finished.
   - Take the `/next-plan` step-10 `Complete-NextPlan.ps1` result the brief
     supplies. When a Plan completion or rejection belongs to the landing and
     the brief carries no such result, stop and report the missing input.
   - Add each changed path listed in the file that result's `changes.path`
     names to the landing set; its fields are in
     [`claim-results.md`](../../next-plan/references/claim-results.md)
     `## Claim-exit result fields`.
   - The claim stays held. Final preparation is not completion.
   - Split the landing set by repository: the engine landing set is every path
     outside `Projects/Coretide/`, and only it goes into the `-OwnedPathsFile`;
     the `Projects/Coretide/` paths are the Coretide landing set.
   - Done when a completion result and the changed-path file its
     `changes.path` names are in hand, or when no claimed Plan finished, and
     the landing set is split.
2. Create the authorized source landing commit, then squash it onto the
   session's merge-base with primary.
   - Reconciliation never advances primary.
   - Inspect dependency overlap.
   - When the landing content changed after the commit was first created, pass
     approval preparation the `-CommitMessageFile` override `scripts.md`
     documents so the prepared commit's message describes what it now contains.
   - When the brief carries an `Attribution trailer` line, every
     `-CommitMessageFile` text this worker writes ends with exactly that line;
     this worker's own session configuration never overrides it. When the brief
     omits the field or gives `none`, the text ends with no trailer.
   - Only when the brief carries the `-RequireExportOutputsRewritten` switch
     (`SKILL.md` `### Landing confirmation`), add it to the candidate-commit
     invocation. Return a `candidate.export-output-not-regenerated` block as a
     `BLOCKED` handoff carrying the script's message verbatim for main to route
     per `SKILL.md` `### Landing confirmation`.
   - When the Coretide landing set is non-empty, write
     `Temp/finalize-coretide-commit-message.txt` describing only the Coretide
     changes and run `Invoke-FinalizeCoretide.ps1 -Prepare` with it
     ([`scripts.md#invocation`](scripts.md#invocation)), so the step-3
     `-Landing` inventory covers the Coretide commit. The engine message never
     names or describes a Coretide file or its content.
   - A Coretide-only landing (an empty engine landing set) creates no engine
     commit.
   - Done when one prepared commit whose parent is the session's merge-base with
     primary exists, or the landing is Coretide-only, and `-Prepare` returned
     `pass` or `none` for a non-empty Coretide landing set.
3. Fill the acceptance table on the resulting diff.
   - Load and follow
     [`landing-acceptance-table.md`](landing-acceptance-table.md) first, which
     owns the typed-artifact rules. When its `-Landing` inventory reports the
     review-round refusal, return the `BLOCKED` table to main at once with that
     result's `status`, `code`, and `message`, skipping steps 4-8 from every
     entry to this step, step 5's skipped-Steps-1-3 completion sequence and
     `## Recovery` included.
   - Fill the landing receipt's `acceptanceSkeleton` rows with evidence from the
     handoffs they name, and the `Executable Plan check` row from the
     `plan validate` run
     [`landing-acceptance-table.md`](landing-acceptance-table.md)
     `## Executable Plan check` requires, then add the remaining rows per
     [`landing-acceptance-table.md`](landing-acceptance-table.md)
     `## Acceptance table`.
   - Never withhold the table waiting for a missing one: give each triggered
     handoff that does not exist its own row with the status that reference
     assigns a missing artifact.
   - Consume every typed receipt verbatim, each `broken-engine-build-result/v1`
     envelope included, never summarized, reading an envelope the brief cites as
     a `Temp/` path plus selector from that file.
   - After a meaningful change to that diff, return its changed paths for main
     to route under
     [change-workflow.md `#### Step 6 — Review and resolve correctness`](../../../references/change-workflow.md#step-6--review-and-resolve-correctness).
   - Reconcile the Coretide landing set against the `Projects/Coretide/` rows
     of `landing.reviewed`, and the engine landing set against the rest. When
     this worker's own `Invoke-FinalizeCoretide.ps1 -Prepare` reports `none`
     (`coretide.no-changes`), each Coretide path of `landing.reviewed` counts
     as a recorded no-change decision citing that result, because the clone's
     upstream is the primary Coretide checkout; when the brief carries the
     step-9 proven Coretide landing result, reconcile each declared Coretide
     landing-set path missing from `landing.reviewed` against its landed
     commit's diff from its parent — paths in that diff count as recorded
     no-change decisions citing both results, while any other missing declared
     path still blocks.
   - Done when every row of that table carries a status.
4. Run the primary-movement check.
   - A Coretide-only landing skips this step and step 5 and goes to step 6,
     which fills only its Coretide line.
   - Invoke `Invoke-FinalizePrimaryMovementCheck.ps1` per
     [`scripts.md#invocation`](scripts.md#invocation) and
     [`scripts.md#bundled-scripts`](scripts.md#bundled-scripts): after step 3 on
     ordinary entry, or on direct `## Recovery` entry consuming
     `candidate.commit`, `candidate.tree`, and `candidate.parent` from
     `Temp/finalize-approval-preparation-result.json` (`SKILL.md` `## Inputs`;
     block under `scripts.md#bundled-scripts` if missing, unreadable, or
     malformed).
   - Done when its fixed result and terminal table in
     [`scripts.md#primary-movement-check`](scripts.md#primary-movement-check)
     have been consumed.
5. Branch on that terminal result.
   - Act on the terminal mapping in
     [`scripts.md#primary-movement-check`](scripts.md#primary-movement-check),
     which owns every status and code.
   - A blocker returns no launch line and no landing summary, and names the
     candidate parent and live primary whenever either is available.
   - Route `primary.path-overlap` — and `needs-review` when `rebuildProjects`
     is non-empty or on the initial step-4 check of a post-confirmation
     engine-landing recovery entry — to
     `## Recovery`'s overlap recovery in this same dispatch (`## Recovery` owns
     the clean, conflict, `rebuildProjects` build, and abort/refusal outcomes).
   - Any other `needs-review` result (empty `rebuildProjects` on ordinary
     preparation or post-recovery step-4 recheck) follows the lossless
     checker-result handoff in
     [`scripts.md#bundled-scripts`](scripts.md#bundled-scripts) directly to
     steps 6 and 8.
   - Skipped-Steps-1-3 completion sequence (for direct step-4 recovery entry,
     only while the initial full acceptance table is still owed, so at most
     once per dispatch): run step 1, step 2's Coretide `-Prepare` substep, and
     full step 3 on the candidate diff (already-landed Coretide paths reconcile
     under step 3). Block if step 1 or Coretide `-Prepare` blocks; never block
     on non-PASS rows of that full step-3 table, which continue through steps 6
     and 8 for `SKILL.md` `### Landing confirmation`. On direct `pass` while
     that table is still owed, run this sequence before step 6.
   - A candidate/session change still returns its changed bytes through normal
     review. Any later primary movement is handled by landing's existing bounded
     internal rebase and terminal result.
   - Done when the result is a usable terminal, a blocker was returned, or the
     overlap recovery returned a blocker or the meaning-change handoff.
6. Fill in the SmartGit launch line.
   - After a usable terminal result, fill the engine (when non-empty) and
     Coretide (when `-Prepare` returned `pass`)
     `Show-FinalizeApprovalReview.ps1` commands from
     [`scripts.md#invocation`](scripts.md#invocation) (redirects included).
   - Emit `none` for a row only on standing-confirmation recovery outside
     standalone Plan rejection — for engine when no code-meaning change occurred
     (empty `rebuildProjects`), and for Coretide when no code-meaning change
     occurred or paired `coretideCommit` is unchanged — and only after
     `Test-FinalizeApprovalReviewReceipt`
     (`.agents/scripts/FinalizeWorkflowCommon.psm1`) returns `$null` for that
     invocation's receipt path, commit, and primary repository; any non-`$null`
     result keeps the filled command for refreshed review.
   - Do not run them: main handles them per `SKILL.md`
     `### Landing confirmation`.
   - Done when each line is filled (or `none`) or a blocker was returned.
7. When `SKILL.md` `### Landing confirmation` requires a refreshed summary,
   refill the launch line for the newly reviewed commit (or `none` per step 6)
   and return it with the summary. Done when the line names that commit or
   `none`.
8. Assemble and return one handoff carrying the complete step-3 acceptance
   table, the `SmartGit launch:` and `Coretide SmartGit launch:` rows holding
   the step-6 lines, and a landing summary.
   - The summary carries `## Context`, outcome-focused `## What landed`, and
     `## Landing`.
   - `## What landed` names once each distinct landed change whose
     [`landing-acceptance-table.md`](landing-acceptance-table.md) step-2 record
     carries a finding ID or user decision, stating every behavior it changes
     and those IDs or decisions, or `none`.
   - `## Landing` states the checked candidate parent and live primary (and,
     after harmless primary movement, that any internal rebase can land only a
     byte-identical patch).
   - It also states both branches, objective decisions, changed-file count/kind,
     the Coretide commit and its changed-file count or none, and the exact
     remaining operation.
   - Retain all existing summary fields and do not state
     `Primary has not advanced.` when the live tip is newer.
   - When [`agenttools.md`](agenttools.md) requires a pause warning (including a
     Plan-marker format bump), include that warning in the handoff.
   - Main decides launch, summary presentation, and pause `(d)` per `SKILL.md`
     `### Landing confirmation`. That handoff is a terminal return: the worker
     ends its turn with it as its final answer.
   - The preparation dispatch changes nothing on primary, so a brief that says
     to stop before any primary change still ends with this summary, per
     `SKILL.md` `## Inputs`.
   - Done when that single handoff has been returned.
9. Claim the landing lock.
   - Only authorization under `SKILL.md` `### Landing confirmation` (standing
     confirmation or standalone user-authorized Plan rejection) permits claiming
     the landing lock and invoking landing with that owner token, in
     `## Bundled scripts` order.
   - When a Coretide commit lands, land it next, before the engine landing:
     run `Invoke-FinalizeCoretide.ps1 -Land`
     ([`scripts.md#invocation`](scripts.md#invocation)) with that owner token
     and the Coretide receipt main's run of the `Coretide SmartGit launch:`
     line wrote, adding the approved engine commit and its receipt when the
     engine landing set is non-empty. Continue on `landed` or `none`; if step
     10 blocks, include the proven Coretide landing result (`status`, `code`,
     `coretideCommit`) in the blocked handoff only for `landed`/`ok`, or for
     `none`/`coretide.already-landed` when `coretideCommit` equals the approved
     Coretide commit.
   - A Coretide-only landing passes `-ReleasePlanClaim` to `-Land` for a claimed
     Plan instead of step 10's, skips steps 10-12, releases the lease with
     `Invoke-FinalizeLockClaim.ps1 -Release` only on `landed` or `none`, and
     routes `blocked` through `## Recovery`.
   - Done when that claim returns landing's owner token and any Coretide
     `-Land` returns its result.
10. Invoke landing with that owner token.
    - That invocation passes `-ApprovalReviewResultFile` naming the receipt that
      main's run of the step-6 line wrote; the receipt gate in
      [`scripts.md`](scripts.md#approval-review-receipt) owns when landing
      blocks on it, and step 14 disposes of such a block like any other blocked
      landing.
    - For a claimed Plan pass `-ReleasePlanClaim` so the machine-local claim is
      deleted best-effort.
    - Done when landing has returned its result.
11. Read the advance and cleanup landing performed.
    - Landing advances primary by compare-and-swap with rollback, resets and
      verifies both checkouts, and releases the lock.
    - Done when that result reports the advance or the rollback that replaced
      it, both checkouts verified, and the lock released.
12. Read what landing proved it added to the landed commit.
    - An exit-0 landing already proves the landed commit is the reviewed source
      commit, or its clean patch-identical rebase, plus exactly the three
      generated metrics-history files
      ([`scripts.md`](scripts.md#primary-movement-check) owns the generator and
      the output names) and nothing else;
      [`scripts.md`](scripts.md#landing-and-recovery) owns the guard behind that
      proof.
    - Return the message of a run blocked with `history.overlay-invalid`;
      [`scripts.md`](scripts.md#landing-and-recovery) owns what that code
      proves.
    - Done when the exit-0 result is recorded as that proof, or the blocked
      message has been returned.
13. Record the machine-local claim deletion.
    - A failed claim delete is a reported residual, never a landing blocker.
    - On a Coretide-only landing, read the deletion or its residual from the
      `-Land` result's `planClaim` and `residuals`.
    - Done when the deletion is recorded as performed or as that residual.
14. Dispose of a landing blocked by primary advancing: landing runs its own
    bounded rebase/retry; never rebase or resolve by hand. Act on the reported
    `disposition` and `lock` projection per
    [`scripts.md`](scripts.md#landing-and-recovery), which owns retry,
    recovery-return, and stop dispositions. Done when acted on.
15. Retain the session branch and worktree; only `/cleanup-worktrees` or
    explicit user direction removes them. Done when this worker has removed
    neither.

`/session-audit` runs only on explicit user request.

## Rules

- Never prompt, delegate, push, create or remove worktrees, or disturb unrelated
  changes.
- Keep history linear: reconcile with `git rebase`, never merge or use
  `--rebase-merges`.
- Resolve checkout, primary, and session identity from
  `Get-AgentWorktreeSessionContext`.

## Recovery

- A `blocked` `Invoke-FinalizeCoretide.ps1 -Land` stops before engine landing
  and reports a `disposition` with the engine landing result's values
  ([`scripts.md`](scripts.md#landing-and-recovery)). On `recovery` or
  `review-round`, release the lease (`Invoke-FinalizeLockClaim.ps1 -Release`)
  and return for recovery to rebase Coretide onto `@{upstream}` and re-run
  `-Prepare`, then continue through steps 3-6 (fresh step-3 acceptance table,
  applicable movement checks, and step-6 receipt reuse or refreshed review, or
  return the meaning-change handoff if code meaning changed). If `-Prepare`
  returns `coretide.no-changes` after a recovery rebase, stop, release any held
  landing lease, and report to main without auto-releasing the Plan claim or
  auto-completing. Any other disposition returns a user blocker, retaining the
  lease only on `rebase.abort-failed`.
- Postconfirmation engine `Invoke-FinalizeLanding.ps1` `recovery` or
  `review-round` result: prior landing arguments, baseline, expected tips, and
  acceptance table are invalid; confirmation authority, drift routing, and
  receipt reuse follow `SKILL.md` `### Landing confirmation` and step 6, while
  prior verification handoffs stay valid where their evidence still applies
  under the
  [table rules](landing-acceptance-table.md#reviewed-diff-and-authorization).
  When `lock.released` is true, return for a fresh preparation worker entering
  at step 4 (`SKILL.md` `## Inputs`); otherwise retain the blocker and lease and
  stop. Never change primary before a fresh landing acceptance table and
  `SKILL.md` `### Landing confirmation` complete, or reuse prior landing
  arguments.
- Reconciliation conflict: resolve under the approved invariants, return the
  changed paths as step 3 states for a meaningful change, and follow `SKILL.md`
  `### Landing confirmation`. Resolve hunk by hunk, tracing
  each side's intent to its originating commit or Plan before choosing, and
  preserve both intents where they are compatible. Keep the resolution to
  behavior one side already had; never invent new behavior in a resolution. A
  conflict on a path that the primary-movement result triggering this
  reconciliation lists in `changes.exportOutputPaths`
  ([`scripts.md#primary-movement-check`](scripts.md#primary-movement-check))
  resolves to the bytes at that result's `tips.livePrimary`, except that a
  modify/delete conflict where the session deleted the path keeps the session's
  deletion; that resolution counts as a meaning change. Any other binary
  conflict, island textures included, aborts the rebase to return a blocker.
  Abort the rebase only to return a blocker when the approved decisions do not
  determine a valid resolution; otherwise resolve rather than abort.
- Process died after primary advanced: re-invoke
  `../scripts/Invoke-FinalizeLanding.ps1` with the original approved arguments.
  [`scripts.md`](scripts.md) owns the rules by which recovery matches the landed
  commit. If the primary ref advanced before its checkout reset, recovery
  recognizes the ref/tree mismatch, acquires or adopts the landing lease, resets
  and verifies the primary checkout, then continues ordinary recovery; initial
  non-recovery sanity remains strict. Then delete the claim.
- Overlap recovery for a triggering step-5 movement-check result: this
  preparation worker recovers it in the same dispatch, without manager or user
  authorization, running the rebase in
  [`scripts.md`](scripts.md#primary-movement-check)'s
  `Invoke-FinalizeApprovalPreparation.ps1` entry; its exit status alone decides
  clean or conflict. Every meaning-change handoff overlap recovery returns
  includes in `Build required` the client and server builds of each game that
  triggering result's `rebuildProjects` lists. For a standalone Plan rejection,
  main routes that handoff per `SKILL.md` `### Landing confirmation`'s
  separate-confirmation sequence, even with unchanged paths and no further
  primary movement.
  - Exit 0 (clean): before rerunning preparation, inspect the rebased diff for
    code-meaning changes and — when `Complete-NextPlan.ps1`'s
    `claim.disposition` is `rejected` — patch-byte changes against this
    dispatch's pre-rebase candidate (`candidate.commit`/`candidate.parent`). If
    either changed, or `rebuildProjects` is non-empty, return the meaning-change
    handoff without asking (`## Rules`), naming changed regions and any
    `Build required` targets above (even without a meaning change), so main runs
    needed reviews, `/compile`, fix rounds, and `/create-follow-up-plans` when
    `SKILL.md` `### Landing confirmation` requires, then dispatches fresh
    preparation re-resolving tips, verified-candidate pair, and baseline per
    that `scripts.md` entry. Otherwise, read `verifiedCandidate.supplied` from
    the prior `Temp/finalize-approval-preparation-result.json` before
    overwriting it (passing the re-resolved verified-candidate pair when `true`,
    omitting it when `false`), then re-invoke approval preparation with those
    re-resolved inputs, run step 5's skipped-Steps-1-3 completion sequence if
    the initial full acceptance table is still owed — or re-run step 3 for
    changed regions once that full table exists (ordinary preparation or any
    later primary-advance repeat) — then re-run step 4 and act on step 5 under
    ordinary preparation rules; a re-run `primary.path-overlap` repeats this
    recovery. That inspection and step-3 work cover the rebased tip a
    re-resolved pair now names, keeping a supplied gate supplied. Return a
    blocked or failed approval-preparation re-invocation as a blocker, and any
    changed-region step-3 non-PASS row as `BLOCKED` for main except `/sweep`
    `UNVERIFIED` owed rows; those rows (continuing through steps 6 and 8 for
    `SKILL.md` `### Landing confirmation` `(d)`) and full step-3 tables follow
    step 5 after the step-4 recheck.
  - Any other exit leaving a rebase in progress (conflict): resolve under
    `Reconciliation conflict` and finish the rebase. When every conflict was a
    Plan metadata (`Documents/Plans/AGENTS.md`) `dependsOn` merge,
    `rebuildProjects` is empty, and this is not standalone Plan rejection,
    re-invoke approval preparation with the re-resolved inputs above and
    continue through step 3 (`plan validate`) and steps 4-6, where
    `Test-FinalizeApprovalReviewReceipt` verifies the resolved commit's
    dependency deltas and preserved bytes and decides `none` vs refreshed
    review. Any other conflict returns resolved regions as the meaning-change
    handoff (with `Build required`) before candidate preparation.
  - Abort instead when approved decisions, including any user answer the
    continuation capsule carries, do not determine a valid resolution (such as
    contradicting intents): run `git rebase --abort`. After that abort, or any
    other exit leaving no rebase in progress (refused to start), prove the
    session tip is again the candidate commit with no rebase in progress, and
    return the triggering movement-check result (`primary.path-overlap` or
    `needs-review`, preserving its
    [`scripts.md#bundled-scripts`](scripts.md#bundled-scripts) artifact) as a
    blocker for the user, carrying the rebase's stderr verbatim. If that proof
    fails, keep the blocker and stop.

  This is not the rewritten-history case below, which the fork-point repair
  script handles
  ([`scripts.md#session-fork-point-repair`](scripts.md#session-fork-point-repair)).
- Primary history rewritten under the session: reattaching through the wrapper
  repairs this, rebasing the session branch onto the new primary tip; the step-4
  re-check detects it mid-session. To repair it in place, run the canonical
  invocation of `.agents/scripts/Repair-SessionForkPoint.ps1` in
  [`scripts.md#invocation`](scripts.md#invocation) and act on its result per
  [`scripts.md#session-fork-point-repair`](scripts.md#session-fork-point-repair),
  returning a `blocked` result as a blocker with the script's message. Never
  perform the rebase by hand.

## Exceptional references

When the landing-commit/landed diff contains a non-Markdown path under
`Tools/WorktreeCli/`, `Tools/AgentHarness/`, or `Tools/ToolCommon/`, or
`Common/Sha256Hasher.h` or `Common/DebugDefines.h`, load `agenttools.md` for
shared-artifact idle waiting, disclosure, promotion, and bootstrap
coordination. This is the sole AgentTools trigger; non-triggering routes do not
load it.
