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

A preparation dispatch runs steps 1-8; a landing dispatch runs steps 9-15 from
the continuation capsule `SKILL.md` `### Landing confirmation` lists and skips
1-8. A blocked result that needs recovery is returned as a handoff for the
recovery dispatch: main dispatches a fresh preparation worker whose
continuation capsule carries that blocked disposition as its unresolved-issue
item and, for a blocker that in-worker overlap recovery returned — the
original `primary.path-overlap` result, or the original `needs-review` result
on step 5's build route — `Temp/finalize-primary-movement-result.json` as path
plus selector.
Every preparation dispatch recovers `primary.path-overlap` itself under
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
     owns the typed-artifact rules.
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
   - A triggered `/progressive-disclosure-review` handoff counts as existing on
     the terms the landing acceptance table states for that row.
   - Consume every typed receipt verbatim, each `broken-engine-build-result/v1`
     envelope included, never summarized, reading an envelope the brief cites as
     a `Temp/` path plus selector from that file.
   - A meaningful change to that diff re-runs review of the changed regions
     only.
   - Reconcile the Coretide landing set against the `Projects/Coretide/` rows
     of `landing.reviewed`, and the engine landing set against the rest. When
     this worker's own `Invoke-FinalizeCoretide.ps1 -Prepare` reports `none`
     with code `coretide.no-changes`, each Coretide path of `landing.reviewed`
     counts as a recorded no-change decision citing that result, because the
     clone's upstream is the primary Coretide checkout.
   - Done when every row of that table carries a status.
4. Run the primary-movement check.
   - A Coretide-only landing skips this step and step 5 and goes to step 6,
     which fills only its Coretide line.
   - After the step-3 acceptance table, invoke the read-only primary-movement
     checker using the canonical command in
     [`scripts.md#invocation`](scripts.md#invocation).
   - Done when its fixed result and terminal table in
     [`scripts.md#primary-movement-check`](scripts.md#primary-movement-check)
     have been consumed.
5. Branch on that terminal result.
   - Act on the terminal mapping in
     [`scripts.md#primary-movement-check`](scripts.md#primary-movement-check),
     which owns every status and code.
   - A blocker returns no launch line and no landing summary, and names the
     candidate parent and live primary whenever either is available.
   - When a `needs-review` or `primary.path-overlap` result's `rebuildProjects`
     is non-empty
     ([`scripts.md#primary-movement-check`](scripts.md#primary-movement-check)),
     the session's source builds with that foreign source before the summary:
     a `needs-review` result runs the overlap recovery in `## Recovery` in this
     same dispatch, returning the original
     `needs-review` result wherever that recovery returns the original
     `primary.path-overlap` result, and on either route the recovery's clean
     path returns the meaning-change handoff with `Build required` naming the
     client and server builds of each game `rebuildProjects` lists, even when
     its inspection finds no meaning change.
   - Any other `needs-review` result follows the documented lossless
     checker-result handoff in
     [`scripts.md#bundled-scripts`](scripts.md#bundled-scripts), is consumed as
     a usable non-conflict terminal, and proceeds directly to the
     `SmartGit launch:` line in step 6 and the existing landing summary.
   - A `primary.path-overlap` result runs the overlap recovery in
     `## Recovery` in this same dispatch.
   - A candidate/session change still returns its changed bytes through normal
     review. Any later primary movement is handled by landing's existing bounded
     internal rebase and terminal result.
   - Done when the result is a usable terminal, a blocker was returned, or the
     overlap recovery returned a blocker or the meaning-change handoff.
6. Fill in the SmartGit launch line.
   - Only after this checker has returned a usable terminal result, take the
     `Show-FinalizeApprovalReview.ps1` command from
     [`scripts.md#invocation`](scripts.md#invocation), redirect included, and
     fill `<primary-worktree>` and, as `<landing-commit>`, the prepared landing
     commit.
   - When `-Prepare` returned `pass`, also fill in a `Coretide SmartGit launch:`
     line from the Coretide review command in
     [`scripts.md#invocation`](scripts.md#invocation), redirect included, with
     `<primary-worktree>` and its `coretideCommit`.
   - Do not run them: main runs them after this handoff, per `SKILL.md`
     `### Landing confirmation`.
   - Done when each line is filled in for the commit the summary describes, or
     a blocker was returned.
7. Whenever `SKILL.md` `### Landing confirmation` requires a refreshed summary,
   refill the launch line for the newly reviewed commit and return it with the
   summary. Done when the line names that commit.
8. Assemble and return one handoff carrying the complete step-3 acceptance
   table, the `SmartGit launch:` and `Coretide SmartGit launch:` rows holding
   the step-6 lines, and a landing summary.
   - The summary carries `## Context`, outcome-focused `## What landed`, and
     `## Landing`.
   - `## Landing` states the checked candidate parent and live primary (and,
     after harmless primary movement, that any internal rebase can land only a
     byte-identical patch).
   - It also states both branches, objective decisions, changed-file count/kind,
     the Coretide commit and its changed-file count or none, and the exact
     remaining operation.
   - Retain all existing summary fields and do not state
     `Primary has not advanced.` when the live tip is newer.
   - Main runs the launch line, then presents the summary immediately before the
     authoritative confirmation question. That summary is a terminal return: the
     worker ends its turn with it as its final answer.
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
     engine landing set is non-empty. Continue on `landed` or `none`.
   - A Coretide-only landing passes `-ReleasePlanClaim` to `-Land` for a
     claimed Plan in place of step 10's, skips steps 10-12, and releases the
     lease through `Invoke-FinalizeLockClaim.ps1 -Release`.
   - Done when that claim has returned the owner token landing carries and any
     Coretide `-Land` has returned its result.
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

- A `blocked` `Invoke-FinalizeCoretide.ps1 -Land` result stops before the
  engine landing; release the lease as for any blocked landing and return the
  result as a blocker for the user.
- Postconfirmation `rebase.conflicted`: landing aborted the rebase, restored
  the approved commit, and released its lease. Prior arguments, candidate,
  baseline, expected tips, verification evidence, SmartGit receipt, and summary
  are invalid (`SKILL.md` `### Landing confirmation` owns confirmation
  authority and drift routing). Once abort, restoration, and release are
  proven, return for recovery: a fresh worker re-resolves tips, runs the linear
  rebase and hunk resolution below, and returns resolved regions before
  candidate preparation for main's recovery steps and fresh preparation under
  `SKILL.md` `### Landing confirmation`. If abort, restoration, or release is
  unproven, retain the blocker and lease and stop. Never change primary until
  fresh review and `SKILL.md` `### Landing confirmation` complete, and never
  reuse prior landing arguments.
- Preconfirmation reconciliation conflict: resolve under the approved
  invariants, then re-review the affected regions and follow `SKILL.md`
  `### Landing confirmation`. Resolve hunk by hunk, tracing each side's
  intent to its originating commit or Plan before choosing, and preserve both
  intents where they are compatible. Keep the resolution to behavior one side
  already had; never invent new behavior in a resolution. A conflict on a path
  that the primary-movement result triggering this reconciliation lists in
  `changes.exportOutputPaths`
  ([`scripts.md#primary-movement-check`](scripts.md#primary-movement-check))
  resolves to the primary tip's bytes, except that a modify/delete conflict
  where the session deleted the path keeps the session's deletion; that
  resolution counts as a meaning change, and the returned meaning-change
  handoff's `Build required` names the client and server builds of each game
  that result's `rebuildProjects` lists. Any other binary conflict, island
  textures included, aborts the rebase to return a blocker, as does every binary
  conflict of a reconciliation with no triggering result, such as the restart
  after `rebase.conflicted`. Abort the rebase only to return a blocker when the
  approved decisions do not determine a valid resolution; otherwise resolve
  rather than abort.
- Process died after primary advanced: re-invoke
  `../scripts/Invoke-FinalizeLanding.ps1` with the original approved arguments.
  [`scripts.md`](scripts.md) owns the rules by which recovery matches the landed
  commit. If the primary ref advanced before its checkout reset, recovery
  recognizes the ref/tree mismatch, acquires or adopts the landing lease, resets
  and verifies the primary checkout, then continues ordinary recovery; initial
  non-recovery sanity remains strict. Then delete the claim.
- The movement check blocked with `primary.path-overlap` (foreign primary
  movement touched a session-owned path): this preparation worker recovers it
  in the same dispatch, with no manager or user authorization. It runs the
  single ordinary linear rebase in [`scripts.md`](scripts.md)'s
  `Invoke-FinalizeApprovalPreparation.ps1` entry, and that rebase's exit status
  alone decides clean or conflict.
  - Exit 0 (clean): before re-invoking approval preparation, inspect the rebased
    diff for code-meaning changes and — when the brief's authorization kind
    (`SKILL.md` `## Inputs`) is `standing confirmation` or
    `standalone Plan rejection` — patch-byte changes against its confirmed
    candidate diff identity (or this dispatch's pre-rebase candidate). If
    either changed, or `rebuildProjects` is non-empty (step 5), return the
    meaning-change handoff to main without asking (`## Rules`), naming changed
    regions and `Build required` targets (including step 5's `rebuildProjects`
    client/server builds), so main runs needed reviews, `/compile`, fix rounds,
    and `/create-follow-up-plans` (`SKILL.md` `### Landing confirmation`), then
    dispatches fresh preparation re-resolving tips, verified-candidate pair,
    and baseline per that `scripts.md` entry. Otherwise (no meaning change,
    empty `rebuildProjects`, and byte-identical when authorized above),
    re-invoke approval preparation with those re-resolved inputs, re-run step 3
    for changed regions, then re-run step 4 and act on step 5; a re-run
    `primary.path-overlap` repeats this recovery. That inspection and step-3
    re-run cover the rebased tip a re-resolved pair now names, keeping a
    supplied gate supplied. Return a blocked or failed approval-preparation
    re-invocation as a blocker, and any non-PASS step-3 changed-region row as a
    `BLOCKED` handoff for main (`SKILL.md` `### Landing confirmation`).
  - Any other exit that leaves a rebase in progress (conflict): resolve under
    the preconfirmation reconciliation conflict rule above and finish the
    rebase; resolved hunks count as a meaning change, so return the resolved
    regions to main as the meaning-change handoff before candidate preparation.
  - Abort instead when the approved decisions, including any user answer the
    continuation capsule carries, do not determine a valid resolution, such as
    when the two sides' intents contradict: run `git rebase --abort`. After
    that abort, or after any other exit that leaves no rebase in progress (the
    rebase refused to start), prove the session tip is again the candidate
    commit with no rebase in progress, and return the original
    `primary.path-overlap` result as a blocker for the user, carrying the
    rebase's stderr verbatim. If that proof fails, keep the blocker and stop.

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
