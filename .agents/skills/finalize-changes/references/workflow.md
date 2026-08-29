# Finalizer Workflow

Worker-only mechanics for `/finalize-changes`. The finalizer loads this whole
file on entry, after [`../SKILL.md`](../SKILL.md), whose `## Inputs and
ownership`, `## Landing confirmation`, and `## Completion and output` stay
authoritative; script commands, result contracts, and lease rules are in
[`scripts.md`](scripts.md).

## Contents

- [Bundled scripts](#bundled-scripts)
- [Normal workflow](#normal-workflow)
- [Recovery](#recovery)
- [Exceptional references](#exceptional-references)

## Bundled scripts

Every script this skill runs lives under `../scripts/`; load `scripts.md`
for their exact commands, contracts, result handling, and lease rules.

Use the root AGENTS.md canonical invocation form.

Invoke them directly, exactly as documented; never write a wrapper,
orchestrator, or replacement around them, and never reconstruct their steps by
hand. An improvised invocation can send wrong arguments into an operation that
changes primary. A script that cannot be run as documented is a bug: stop and
report it.

Approval preparation invokes the read-only history producer in Contract mode,
and landing reads its receipt's compact generator/capture/patch identities
directly from the saved approval preparation result file
(`-ApprovalPreparationResultFile`); no digest is ever hand-copied onto a
command line. Under its landing lease, finalization follows the [root
`AGENTS.md` Step 8 landing invariant](../../../../AGENTS.md) and the exact mode
mechanics in [`scripts.md`](scripts.md). The separately
requested primary-commit route follows the same ceremony, as
`primary-commit.md` states.

The caller releases its reconciliation lease through
`../scripts/Invoke-FinalizeLockClaim.ps1 -Release` before the confirmation pause, so
no lease is ever held while waiting on the user. Immediately after the
affirmative confirmation the caller claims the landing lock once through
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

## Normal workflow

1. When a claimed Plan finished, run final preparation first: `plan complete`,
   or `plan reject --user-authorized-rejection` after explicit user-authorized
   rejection. It rewrites direct dependency-children markers and deletes the
   target Plan in the worktree, and returns the changed paths the landing commit
   must contain, which the `/next-plan` claim-exit script reports as
   `changes.items[].path`. The claim stays held. Final preparation is not
   completion.
2. Create the authorized source landing commit, then squash and rebase it onto the
   current primary tip. Reconciliation never advances primary. Inspect dependency
   overlap and any place the rebase merged cleanly but changed the code's
   meaning. After the final source candidate is prepared, approval preparation
   runs the history producer's Contract mode and returns the typed
   `broken-engine-code-quality-history-contract/v1` receipt before
   `/verify-changes`. When the landing content changed after the commit was first
   created, pass approval preparation the `-CommitMessageFile` override
   `scripts.md` documents so the prepared commit's message describes
   what it now contains.
3. Main dispatches `/verify-changes` on the resulting diff, only once every
   hygiene handoff the session-change inventory's `triggers` object reports
   true already exists — `/code-style-review` for changed C++,
   `/update-vcxproj`, `/validate-skill`, `/update-claude-docs`,
   `/progressive-disclosure-review` — and passes
   every typed receipt verbatim, each `broken-engine-build-result/v1` envelope
   included, never summarized. A triggered `/progressive-disclosure-review`
   handoff counts as existing only as a PASS whose `Baseline:` satisfies the
   rule in that skill's `## Output` section. A meaningful change
   to that diff re-runs review of the changed regions only.
4. After the step-3 `/verify-changes` PASS on the `session-landing` route, invoke
   the read-only primary-movement checker using the canonical command in
   [`scripts.md#invocation`](scripts.md#invocation), and
   consume its fixed result and terminal table in
   [`scripts.md#primary-movement-check`](scripts.md#primary-movement-check).
   A `pass` result continues
   to `Show-FinalizeApprovalReview.ps1` and the existing landing summary. A
   `blocked` or `error` result, or a malformed result or schema/exit mismatch,
   opens no review window and returns no landing summary; return a blocker that
   names the candidate parent and live primary whenever either is available.
   A `needs-review` result, including
   `primary.disjoint-needs-review`, follows the documented lossless
   checker-result handoff in
   [`scripts.md#bundled-scripts`](scripts.md#bundled-scripts),
   and returns control to main without a terminal summary or lease. Main
   dispatches one fresh focused reviewer over the exact candidate commit,
   candidate tree, candidate diff and inventory and the handoff's complete
   movement evidence. That reviewer checks direct and transitive
   include, call, data, configuration, producer, and consumer dependencies in
   both directions and returns
   `independent`, `reachable`, or `unknown`, bound to the candidate commit,
   candidate tree, candidate parent, and live primary it reviewed. `reachable`
   and `unknown` block. For `independent`, resume this same finalizer; it reruns
   the checker and accepts the verdict only when the result is still
   `needs-review` and all four identities match exactly. A candidate/session
   change returns its changed bytes through normal review. A live-primary
   change discards the verdict and follows the new checker result.
   No lease is held across the reviewer or user wait.
   On the `session-landing` route, invoke
   `../scripts/Show-FinalizeApprovalReview.ps1 -LaunchSmartGit` with the
   `/verify-changes` prompt and output paths main hands over when it resumes
   this worker, and only after this checker has returned a usable terminal
   result. Its gate codes are in
   [`scripts.md#approval-review-receipt`](scripts.md#approval-review-receipt).
   Redirect its stdout to
   `Temp/finalize-approval-review-result.json` exactly as
   [`scripts.md`](scripts.md) shows: that artifact is a required landing input,
   not a record of one. On the separately requested direct-primary (`primary-commit`)
   route, invoke it only after `/verify-changes` has returned PASS on the final
   diff, without invoking this checker, and immediately before the landing
   summary — never alongside step-2 preparation — so the user reviews the
   SmartGit window with verification already complete, right before being asked
   to confirm, and the review window is open whenever SmartGit is available.
   Exit-0 statuses
   `opened`, `unavailable`, and `failed` are non-blocking: carry the status into
   the summary's `SmartGit review` line, and for `unavailable`/`failed` the
   authoritative `manualCommand` and the message as well, in the exact line form
   `SKILL.md` `## Landing confirmation` states, which main copies into the
   confirmation.
   Any exit `1`, `error` status, malformed result, or schema mismatch blocks. Do
   not reinvoke after landing's own clean identical internal rebase (step 6),
   which preserves the existing confirmation; a meaningful change requiring a
   refreshed confirmation launches it again against the newly reviewed landing
   commit before that refreshed confirmation. Then return a landing summary: `## Context`,
   outcome-focused `## What landed`, and `## Landing` stating the
   checked candidate parent and live primary (and, after harmless primary
   movement, that any internal rebase can land only a byte-identical patch),
   both branches, objective decisions, changed-file count/kind, and the exact
   remaining operation. Retain all existing summary fields and do not state
   `Primary has not advanced.` when the live tip is newer. Main presents it
   immediately before the authoritative confirmation question. That summary is
   a terminal return: the worker ends its turn with it as its final answer.
   Steps 1-4 change nothing on primary, so a brief that says to stop before any
   primary change still ends with this summary, per `SKILL.md` `## Worker
   workflow`.
   For the separately requested direct-primary (`primary-commit`) route, preserve
   the exact-tip verification-to-summary flow in `primary-commit.md:23-32`
   and do not invoke this checker.
5. Only that affirmative response permits the same worker to claim the landing
   lock and invoke landing with that owner token, in the order
   `## Bundled scripts` states. That invocation passes
   `-ApprovalReviewResultFile` naming the step-4 artifact; the receipt gate in
   [`scripts.md`](scripts.md#approval-review-receipt) owns when landing blocks on
   it, and step 6 disposes of such a block like any other blocked
   landing. Under the landing lease, landing freezes the
   approved commit message, complete author/committer identities and dates, and
   the non-history patch identity and rechecks the approved Contract against
   current history/mode. It follows the [root `AGENTS.md` Step 8 landing
   invariant](../../../../AGENTS.md) and exact mode mechanics in
   [`scripts.md`](scripts.md). Landing then advances
   primary by compare-and-swap with
   rollback, resets and verifies both checkouts, releases the lock, and deletes
   the machine-local claim. For a claimed Plan pass `-ReleasePlanClaim` so the
   machine-local claim is deleted best-effort; a failed claim delete is a reported
   residual, never a landing blocker.
6. If primary advanced before the advance succeeds, landing does its own bounded
   rebase and retry, following the [root `AGENTS.md` Step 8 landing
   invariant](../../../../AGENTS.md) and [`scripts.md`](scripts.md)
   for the resulting mode and disposition. Never rebase or resolve by hand. Act on the
   blocked result's reported `disposition` and `lock` projection exactly as
   `scripts.md` specifies, never a memorized list of codes. A
   `terminal` result stops this caller; one reporting a changed reviewed contract
   or reachable source patch returns to `SKILL.md` `## Landing confirmation`
   for focused re-review and a refreshed confirmation.
7. Retain the session branch and worktree; only `/cleanup-worktrees` or explicit
   user direction removes them.

`/session-audit` runs only on explicit user request.

## Recovery

- Reconciliation conflict: resolve under the approved invariants, then re-review
  the affected regions and re-ask the confirmation. Resolve hunk by hunk, tracing
  each side's intent to its originating commit or Plan before choosing, and
  preserve both intents where they are compatible. Keep the resolution to
  behavior one side already had; never invent new behavior in a resolution.
  Abort the rebase only to return a blocker when the approved decisions do not
  determine a valid resolution; otherwise resolve rather than abort.
- Process died after primary advanced: re-invoke
  `../scripts/Invoke-FinalizeLanding.ps1` with the original approved arguments.
  Choose the second form in [`scripts.md`](scripts.md) only
  when a surviving structured result provides the complete active-overlay
  history tuple; choose the first form for a carry-forward source-only result or
  a hard crash with no result. That reference owns the tuple, source-match, and
  mode mechanics.
  If the primary ref advanced before its checkout reset, recovery recognizes the
  ref/tree mismatch, acquires or adopts the landing lease, resets and verifies the
  primary checkout, then continues ordinary recovery; initial non-recovery sanity
  remains strict. Then delete the claim.
- Approval preparation blocked with `git.primary-not-ancestor` (primary advanced
  under the session): use the single rebase invocation and re-invocation rule
  stated in [`scripts.md`](scripts.md)'s
  `Invoke-FinalizeApprovalPreparation.ps1` entry. This is not the
  rewritten-history case below, whose `--onto` form does not apply.
- Landing blocked `terminal` with `history.contract-changed` reporting that the
  aggregate digest changed while the approved candidate parent remained
  unchanged, after the session branch was rebased by hand once approval
  preparation had already run — the caller-side rebase step 6 forbids: that
  run's frozen history Contract still names the pre-rebase base, so landing
  blocks even when the patch is byte-identical, and the blocked result carries
  only the approved digests. The approval-review receipt is not stale here: it
  already names the rebased commit, or landing would have stopped earlier with
  `approval-review.candidate-mismatch`. This is a stale artifact, not a changed
  reviewed contract: re-invoke `Invoke-FinalizeApprovalPreparation.ps1` per its
  [`scripts.md`](scripts.md) entry, refresh the approval-review receipt only
  when that run produces a commit other than the receipt's `approvedTip` (its
  `one-commit-no-op` disposition keeps the same commit), and re-invoke landing;
  the byte-identical patch keeps the existing confirmation. Any other
  `history.contract-changed` message is the changed-reviewed-contract case
  step 6 routes to re-review.
- Primary history rewritten under the session: reattaching through the wrapper
  repairs this, rebasing the session branch onto the new primary tip; the step-4
  re-check detects it mid-session. To repair it in place, recover the old fork
  point with `git merge-base --fork-point refs/heads/<primary-branch> HEAD`,
  then require `git merge-base --is-ancestor <old-fork-point> <new-primary-tip>`
  to exit non-zero before using it: once the pre-rewrite tip has expired from
  that branch's reflog, `--fork-point` succeeds with an older surviving
  ancestor, and rebasing from it replays commits primary already carries. When
  it is an ancestor the old fork point is not recoverable — stop and report a
  blocker rather than guess one. Otherwise rebase the session branch with
  `git rebase --onto <new-primary-tip> <old-fork-point>` and re-export
  `BROKEN_ENGINE_BASELINE` to the new tip so attribution stays correct.

## Exceptional references

When the landing-commit/landed diff contains a non-Markdown path under
`Tools/WorktreeCli/`, `Tools/AgentHarness/`, or `Tools/ToolCommon/`, load
`agenttools.md` for shared-artifact idle waiting, disclosure,
promotion, and bootstrap coordination. This is the sole AgentTools trigger;
non-triggering routes do not load it.
