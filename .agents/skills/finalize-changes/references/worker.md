# Finalizer Worker

Worker-only mechanics for `/finalize-changes`.

## Bundled scripts

Every script this skill runs lives under `../scripts/`; its command line, result
contract, and lease rules are in [`scripts.md`](scripts.md).

Use the root AGENTS.md canonical invocation form.

Invoke them directly, exactly as documented; never write a wrapper,
orchestrator, or replacement around them, and never reconstruct their steps by
hand. An improvised invocation can send wrong arguments into an operation that
changes primary. A script that cannot be run as documented is a bug: stop and
report it.

Under its landing lease, finalization follows the [root `AGENTS.md` Verify and
land step's landing invariant](../../../../AGENTS.md) and the exact mechanics in
[`scripts.md`](scripts.md).

Immediately after the affirmative confirmation the caller claims the landing
lock once through
`../scripts/Invoke-FinalizeLockClaim.ps1`, with the landing lease duration
`scripts.md` states, and passes that owner token to
`../scripts/Invoke-FinalizeLanding.ps1` as `-OwnerToken`, which continues under that
same lease through the advance. This supplied-token route keeps the raw
`$SessionLabel`; when `-OwnerToken` is omitted, landing uses
`$SessionLabel/landing`, adopts a live retained claim only when that exact
session and the same canonical worktree match, and otherwise mints its own
token through WorktreeCli `lock token` before claiming under the derived
identity.

Ownership rule: a lease is a same-actor continuation when its recorded session
and worktree match the current landing identity; an omitted-token invocation may
also adopt the live owner recorded for that exact derived landing identity;
every other lease is foreign.

## Steps

1. Run final preparation first when a claimed Plan finished.
   - Run `plan complete`, or `plan reject --user-authorized-rejection` after
     explicit user-authorized rejection.
   - It rewrites direct dependency-children markers and deletes the target Plan
     in the worktree, and returns the changed paths the landing commit must
     contain, which the `/next-plan` claim-exit script reports as
     `changes.items[].path`.
   - The claim stays held. Final preparation is not completion.
   - Done when that command has returned those changed paths, or when no claimed
     Plan finished.
2. Create the authorized source landing commit, then squash it onto the
   session's merge-base with primary.
   - Reconciliation never advances primary.
   - Inspect dependency overlap.
   - When the landing content changed after the commit was first created, pass
     approval preparation the `-CommitMessageFile` override `scripts.md`
     documents so the prepared commit's message describes what it now contains.
   - Done when one prepared commit whose parent is the session's merge-base with
     primary exists.
3. Fill the acceptance table on the resulting diff.
   - Load and follow
     [`landing-acceptance-table.md`](landing-acceptance-table.md) first, which
     owns the typed-artifact rules.
   - Fill the landing receipt's `acceptanceSkeleton` rows with evidence from the
     handoffs they name, and the `Executable Plan check` row from the
     `plan validate` run [`landing-acceptance-table.md`](landing-acceptance-table.md)
     `## Executable Plan check` requires, then add the remaining rows per
     [`landing-acceptance-table.md`](landing-acceptance-table.md)
     `## Acceptance table`.
   - Never withhold the table waiting for a missing one: give each triggered
     handoff that does not exist its own row with the status that reference
     assigns a missing artifact.
   - A triggered `/progressive-disclosure-review` handoff counts as existing on
     the terms the landing acceptance table states for that row.
   - Consume every typed receipt verbatim, each `broken-engine-build-result/v1`
     envelope included, never summarized.
   - A meaningful change to that diff re-runs review of the changed regions
     only.
   - Done when every row of that table carries a status.
4. Run the primary-movement check.
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
   - A blocker returns no SmartGit review and no landing summary, and names the
     candidate parent and live primary whenever either is available.
   - A `needs-review` result follows the documented lossless checker-result
     handoff in [`scripts.md#bundled-scripts`](scripts.md#bundled-scripts), is
     consumed as a usable non-conflict terminal, and proceeds directly to the
     step-6 SmartGit review and the existing landing summary.
   - Apply the [root `AGENTS.md` Verify and land step's landing
     invariant](../../../../AGENTS.md) and
     [`scripts.md#landing-and-recovery`](scripts.md#landing-and-recovery) for the
     movement and lease rules.
   - A candidate/session change still returns its changed bytes through normal
     review. Any later primary movement is handled by landing's existing bounded
     internal rebase and terminal result.
   - Done when the result is a usable terminal or a blocker was returned.
6. Run the SmartGit approval review.
   - Only after this checker has returned a usable terminal result, run the
     `Show-FinalizeApprovalReview.ps1` command from
     [`scripts.md#invocation`](scripts.md#invocation), redirect included, with
     `<primary-worktree>` filled in and, as `<landing-commit>`, the prepared
     landing commit.
   - Read `status`, `message`, and `manualCommand` from the receipt artifact
     that redirect writes. A receipt that is unreadable, or whose status is
     outside the set
     [`scripts.md#approval-review-receipt`](scripts.md#approval-review-receipt)
     accepts, is a blocker to return.
   - Done when those fields are in hand for the commit the summary describes, or
     a blocker was returned.
7. Re-run that review only for a meaningful change.
   - Whenever `SKILL.md` `### Landing confirmation` requires a refreshed
     confirmation, re-run step 6 for the newly reviewed landing commit and
     return its status with the refreshed summary.
   - Done when the review has run against the commit the current summary
     describes.
8. Assemble and return one handoff carrying the complete step-3 acceptance
   table, the `SmartGit review` row, and a landing summary.
   - The summary carries `## Context`, outcome-focused `## What landed`, and
     `## Landing`.
   - `## Landing` states the checked candidate parent and live primary (and,
     after harmless primary movement, that any internal rebase can land only a
     byte-identical patch).
   - It also states both branches, objective decisions, changed-file count/kind,
     and the exact remaining operation.
   - Retain all existing summary fields and do not state
     `Primary has not advanced.` when the live tip is newer.
   - Main presents the summary immediately before the authoritative
     confirmation question. That summary is a terminal return: the
     worker ends its turn with it as its final answer.
   - This worker's steps 1-8 change nothing on primary, so a brief that says to
     stop before any primary change still ends with this summary, per
     `SKILL.md` `## Inputs`.
   - Done when that single handoff has been returned.
9. Claim the landing lock.
   - Only that affirmative response permits the same worker to claim the landing
     lock and invoke landing with that owner token, in the order
     `## Bundled scripts` states.
   - Done when that claim has returned the owner token landing carries.
10. Invoke landing with that owner token.
    - That invocation passes `-ApprovalReviewResultFile` naming the receipt the
      step-6 run wrote; the receipt gate in
      [`scripts.md`](scripts.md#approval-review-receipt) owns when landing blocks
      on it, and step 14 disposes of such a block like any other blocked landing.
    - For a claimed Plan pass `-ReleasePlanClaim` so the machine-local claim is
      deleted best-effort.
    - Under the landing lease, landing follows the [root `AGENTS.md` Verify and
      land step's landing invariant](../../../../AGENTS.md) and the exact
      mechanics in [`scripts.md`](scripts.md).
    - Done when landing has returned its result.
11. Read the advance and cleanup landing performed.
    - Landing advances primary by compare-and-swap with rollback, resets and
      verifies both checkouts, and releases the lock.
    - Done when that result reports the advance or the rollback that replaced it,
      both checkouts verified, and the lock released.
12. Read what landing proved it added to the landed commit.
    - An exit-0 landing already proves the landed commit is the reviewed source
      commit, or its clean patch-identical rebase, plus exactly the two generated
      metrics-history files
      ([`scripts.md`](scripts.md#primary-movement-check) owns the generator and
      the output names) and nothing else;
      [`scripts.md`](scripts.md#landing-and-recovery) owns the guard behind that
      proof.
    - Return the message of a run blocked with `history.overlay-invalid`;
      [`scripts.md`](scripts.md#landing-and-recovery) owns what that code proves.
    - Done when the exit-0 result is recorded as that proof, or the blocked
      message has been returned.
13. Record the machine-local claim deletion.
    - A failed claim delete is a reported residual, never a landing blocker.
    - Done when the deletion is recorded as performed or as that residual.
14. Dispose of a landing blocked by primary advancing.
    - If primary advanced before the advance succeeds, landing does its own
      bounded rebase and retry, following the [root `AGENTS.md` Verify and land
      step's landing invariant](../../../../AGENTS.md) and
      [`scripts.md`](scripts.md) for the resulting disposition.
    - Never rebase or resolve by hand.
    - Act on the blocked result's reported `disposition` and `lock` projection
      exactly as `scripts.md` specifies, never a memorized list of codes.
    - A `terminal` result stops this caller; one reporting a changed reviewed
      contract or reachable source patch returns to `SKILL.md`
      `### Landing confirmation` for focused re-review and a refreshed
      confirmation.
    - Done when the reported disposition has been acted on.
15. Retain the session branch and worktree; only `/cleanup-worktrees` or explicit
    user direction removes them. Done when this worker has removed neither.

`/session-audit` runs only on explicit user request.

## Rules

- Never prompt, delegate, push, create or remove worktrees, or disturb unrelated
  changes.
- Keep history linear: reconcile with `git rebase`, never merge or use
  `--rebase-merges`.
- Resolve checkout, primary, and session identity from
  `Get-AgentWorktreeSessionContext`.

## Recovery

- Postconfirmation `rebase.conflicted`: the landing script has aborted the rebase,
  restored the approved session commit, and released its normal postconfirmation
  landing lease. The original landing arguments, candidate, baseline, expected
  tips, verification evidence, SmartGit receipt, summary, and confirmation are
  invalid. Only after abort, restoration, and lease release are proven, restart
  preconfirmation reconciliation: re-resolve the current session and primary
  tips, and a fresh candidate baseline; perform the ordinary linear rebase and
  hunk resolution under the `Recovery` section, recreate and prepare a new
  candidate, rerun the affected reviews and the acceptance table, produce a fresh
  summary and re-run the SmartGit review for that candidate, and obtain fresh
  user confirmation. If any abort, restoration, or release is unproven, retain
  the existing blocker and lease state and stop; do not begin recovery or
  expose a user wait. No primary change is attempted until the fresh review and
  confirmation complete. Never reuse the original approved landing arguments.
- Preconfirmation reconciliation conflict: resolve under the approved
  invariants, then re-review the affected regions and re-ask the confirmation.
  Resolve hunk by hunk, tracing each side's intent to its originating commit or
  Plan before choosing, and preserve both intents where they are compatible.
  Keep the resolution to behavior one side already had; never invent new
  behavior in a resolution.
  Abort the rebase only to return a blocker when the approved decisions do not
  determine a valid resolution; otherwise resolve rather than abort.
- Process died after primary advanced: re-invoke
  `../scripts/Invoke-FinalizeLanding.ps1` with the original approved arguments.
  [`scripts.md`](scripts.md) owns the rules by which recovery matches the
  landed commit.
  If the primary ref advanced before its checkout reset, recovery recognizes the
  ref/tree mismatch, acquires or adopts the landing lease, resets and verifies the
  primary checkout, then continues ordinary recovery; initial non-recovery sanity
  remains strict. Then delete the claim.
- The movement check blocked with `primary.path-overlap` (foreign primary
  movement touched a session-owned path): stop and report the result to the
  manager. When the manager authorizes recovery, this same finalizer performs
  the single ordinary linear rebase and the re-invocation of approval
  preparation with the re-resolved tips and any verified-candidate pair stated
  in [`scripts.md`](scripts.md)'s `Invoke-FinalizeApprovalPreparation.ps1`
  entry, inspecting any place that rebase merged cleanly but changed the code's
  meaning, re-runs step 3 for the changed regions, and re-runs the movement
  check. That inspection and the step-3 re-run, including the review of the
  changed regions that re-run triggers, cover the rebased tip a re-resolved pair
  now names, so a supplied gate stays supplied instead of being dropped. This is
  not the rewritten-history case below, which the fork-point repair script
  handles
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
`Tools/WorktreeCli/`, `Tools/AgentHarness/`, or `Tools/ToolCommon/`, load
`agenttools.md` for shared-artifact idle waiting, disclosure,
promotion, and bootstrap coordination. This is the sole AgentTools trigger;
non-triggering routes do not load it.
