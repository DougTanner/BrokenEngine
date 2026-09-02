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

Under its landing lease, finalization follows the [root
`AGENTS.md` Step 8 landing invariant](../../../../AGENTS.md) and the exact
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
   meaning. When the landing content changed after the commit was first
   created, pass approval preparation the `-CommitMessageFile` override
   `scripts.md` documents so the prepared commit's message describes
   what it now contains.
3. Fill the acceptance table on the resulting diff, loading and following
   [`landing-acceptance-table.md`](landing-acceptance-table.md) first, which
   owns the typed-artifact rules. Fill it from the hygiene handoffs that already
   exist — the session-change inventory's `triggers` object reports which ones
   the session owes: `/code-style-review` for changed C++, `/update-vcxproj`,
   `/validate-skill`, `/update-claude-docs`, `/progressive-disclosure-review`.
   Never withhold the table waiting for a missing one: give each triggered
   handoff that does not exist its own row with the status that reference
   assigns a missing artifact. A triggered `/progressive-disclosure-review`
   handoff counts as existing only as a PASS whose `Baseline:` satisfies the
   rule in that skill's `## Output` section. Consume every typed receipt
   verbatim, each `broken-engine-build-result/v1` envelope included, never
   summarized. A meaningful change to that diff re-runs review of the changed
   regions only.
4. After the step-3 acceptance table on the `session-landing` route, invoke
   the read-only primary-movement checker using the canonical command in
   [`scripts.md#invocation`](scripts.md#invocation), and
   consume its fixed result and terminal table in
   [`scripts.md#primary-movement-check`](scripts.md#primary-movement-check).
   A `pass` result continues
   to `Show-FinalizeApprovalReview.ps1` and the existing landing summary. A
   `blocked` or `error` result, or a malformed result or schema/exit mismatch,
   opens no review window and returns no landing summary; return a blocker that
   names the candidate parent and live primary whenever either is available.
   A `needs-review` result with code `primary.disjoint-needs-review` follows the
   documented lossless checker-result handoff in
   [`scripts.md#bundled-scripts`](scripts.md#bundled-scripts) and is consumed as
   a usable non-conflict terminal. It proceeds directly to
   `Show-FinalizeApprovalReview.ps1` and the existing landing summary; apply the
   [root `AGENTS.md` Step 8 landing invariant](../../../../AGENTS.md) and
   [`scripts.md#landing-and-recovery`](scripts.md#landing-and-recovery) for the
   movement and lease rules. A candidate/session change still returns its
   changed bytes through normal review. Any later primary movement is handled by
   landing's existing bounded internal rebase and terminal result.
   On the `session-landing` route, invoke
   `../scripts/Show-FinalizeApprovalReview.ps1 -LaunchSmartGit` only after
   this checker has returned a usable terminal result. Its gate codes are in
   [`scripts.md#approval-review-receipt`](scripts.md#approval-review-receipt).
   Redirect its stdout to
   `Temp/finalize-approval-review-result.json` exactly as
   [`scripts.md`](scripts.md) shows: that artifact is a required landing input,
   not a record of one.
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
   commit before that refreshed confirmation. Then return one handoff carrying
   the complete step-3 acceptance table, then this script's SmartGit result,
   then a landing summary: `## Context`,
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
   the exact-tip acceptance-to-summary flow in `primary-commit.md`
   and do not invoke this checker.
5. Only that affirmative response permits the same worker to claim the landing
   lock and invoke landing with that owner token, in the order
   `## Bundled scripts` states. That invocation passes
   `-ApprovalReviewResultFile` naming the step-4 artifact; the receipt gate in
   [`scripts.md`](scripts.md#approval-review-receipt) owns when landing blocks on
   it, and step 6 disposes of such a block like any other blocked
   landing. Under the landing lease, landing follows the
   [root `AGENTS.md` Step 8 landing invariant](../../../../AGENTS.md) and the
   exact mechanics in
   [`scripts.md`](scripts.md). Landing then advances
   primary by compare-and-swap with
   rollback, resets and verifies both checkouts, releases the lock, and deletes
   the machine-local claim. For a claimed Plan pass `-ReleasePlanClaim` so the
   machine-local claim is deleted best-effort; a failed claim delete is a reported
   residual, never a landing blocker.
6. If primary advanced before the advance succeeds, landing does its own bounded
   rebase and retry, following the [root `AGENTS.md` Step 8 landing
   invariant](../../../../AGENTS.md) and [`scripts.md`](scripts.md)
   for the resulting disposition. Never rebase or resolve by hand. Act on the
   blocked result's reported `disposition` and `lock` projection exactly as
   `scripts.md` specifies, never a memorized list of codes. A
   `terminal` result stops this caller; one reporting a changed reviewed contract
   or reachable source patch returns to `SKILL.md` `## Landing confirmation`
   for focused re-review and a refreshed confirmation.
7. Retain the session branch and worktree; only `/cleanup-worktrees` or explicit
   user direction removes them.

`/session-audit` runs only on explicit user request.

## Recovery

- Postconfirmation `rebase.conflicted`: the landing script has aborted the rebase,
  restored the approved session commit, and released its normal postconfirmation
  landing lease. The original landing arguments, candidate, baseline, expected
  tips, verification evidence, SmartGit receipt, summary, and confirmation are
  invalid. Only after abort, restoration, and lease release are proven, restart
  preconfirmation reconciliation: re-resolve the current session and primary
  tips, and a fresh candidate baseline; perform the ordinary linear rebase and
  hunk resolution under the `Recovery` section, recreate and prepare a new
  candidate, rerun the affected reviews and the acceptance table, reopen SmartGit,
  produce a fresh summary, and obtain fresh user confirmation. If any abort,
  restoration, or release is
  unproven, retain the existing blocker and lease state and stop; do not begin
  recovery or expose a user wait. No primary change is attempted until the
  fresh review and confirmation complete. Never reuse the original approved
  landing arguments.
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
- Approval preparation blocked with `git.primary-not-ancestor` (primary advanced
  under the session): use the single rebase invocation and re-invocation rule
  stated in [`scripts.md`](scripts.md)'s
  `Invoke-FinalizeApprovalPreparation.ps1` entry. This is not the
  rewritten-history case below, whose `--onto` form does not apply.
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
