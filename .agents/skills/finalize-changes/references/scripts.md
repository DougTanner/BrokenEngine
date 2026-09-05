# Bundled Scripts

Use the bundled scripts; never reconstruct their Git, lock, or WorktreeCli
operations. Parse their single fixed-shape JSON result and show only status, code,
message, next-stage state, short counts/paths, and retry or authority
outcome when applicable. Never return a nested tool response or
file/XML/log body. The public `Invoke-FinalizePrimaryMovementCheck.ps1`
invocation below redirects its complete stdout losslessly to the ignored
repository-relative `Temp/finalize-primary-movement-result.json` artifact.
The caller parses that artifact for every result. For the exact result with
`status` `needs-review` and `code` `primary.disjoint-needs-review`, the finalizer
handoff returns the artifact path and selector `$`; main reads the complete typed
`broken-engine-finalize-primary-movement/v2` result, including the
foreign-commit manifest, losslessly from that artifact. Never inline-copy,
summarize, or reconstruct it; the artifact remains byte-identical and available
through the SmartGit/summary handoff. Other results surface only short status,
code, message, counts, and paths after parsing. Exit/result/schema mismatches
block. For
`Invoke-FinalizeApprovalPreparation.ps1`, preserve the complete command stdout
losslessly in the ignored repository-relative
`Temp/finalize-approval-preparation-result.json` artifact during the invocation
below. The `Show-FinalizeApprovalReview.ps1` invocation
below likewise redirects its single-line stdout to the ignored repository-relative
`Temp/finalize-approval-review-result.json` artifact, and that artifact path is
what the landing invocations pass as `-ApprovalReviewResultFile`; its values are
never re-typed by hand.

## Contents

- [Invocation](#invocation)
- [Contracts](#contracts)
  - [Primary movement check](#primary-movement-check)
  - [Approval review receipt](#approval-review-receipt)
  - [Landing and recovery](#landing-and-recovery)
  - [Session fork-point repair](#session-fork-point-repair)

## Invocation

Use the root AGENTS.md canonical form — from the session worktree root, one
script invocation per shell call, repo-relative path, no `-ExecutionPolicy`.
Angle-bracket values are placeholders; quote every one. `<owner-token>` is a
canonical lowercase GUID in `8-4-4-4-12` form. For the lock claim, either omit
`-LandingOwner` so `Invoke-FinalizeLockClaim.ps1` creates and returns one, or
create one explicitly with `WorktreeCli lock token`; later commands that supply
the already-held token carry it and do not create one. `<baseline>` is the
`Baseline` that `Get-AgentWorktreeSessionContext` reports when run from the
command's `<current-worktree>` after that branch's most recent rebase; a rebase
invalidates an earlier-resolved value, so re-resolve it from a fresh run, never
from earlier command text or the dispatch brief. `<session-label>` is the
`SessionId` that `Get-AgentWorktreeSessionContext` reports; pass that
identical value to every command below that takes `-SessionLabel`.

```text
pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Invoke-FinalizeCandidateCommit.ps1 -CurrentWorktree '<current-worktree>' -PrimaryWorktree '<primary-worktree>' -CurrentBranch '<session-branch>' -PrimaryBranch '<primary-branch>' -Baseline '<baseline>' -ExpectedCurrentTip '<current-tip>' -ExpectedPrimaryTip '<primary-tip>' -OwnedPaths '<path>,<path>' -CommitMessageFile '<message-file>'
pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Invoke-FinalizeApprovalPreparation.ps1 -CurrentWorktree '<current-worktree>' -PrimaryWorktree '<primary-worktree>' -CurrentBranch '<session-branch>' -PrimaryBranch '<primary-branch>' -ExpectedCurrentTip '<current-tip>' -ExpectedPrimaryTip '<primary-tip>' > 'Temp/finalize-approval-preparation-result.json'
pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Invoke-FinalizePrimaryMovementCheck.ps1 -CurrentWorktree '<session>' -PrimaryWorktree '<primary>' -CurrentBranch '<session-branch>' -PrimaryBranch '<primary-branch>' -CandidateCommit '<candidate-commit>' -CandidateTree '<candidate-tree>' -CandidateParent '<candidate-parent>' -OwnedPaths '<path>,<path>' > 'Temp/finalize-primary-movement-result.json'
pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Invoke-FinalizeLockClaim.ps1 -WorktreeCliExecutable '<worktreecli-exe>' -GitCommonDirectory '<git-common-dir>' -SessionLabel '<session-label>' -Worktree '<current-worktree>'
pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Invoke-FinalizeLockClaim.ps1 -WorktreeCliExecutable '<worktreecli-exe>' -GitCommonDirectory '<git-common-dir>' -SessionLabel '<session-label>' -Worktree '<current-worktree>' -LandingOwner '<owner-token>' -Release
pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Invoke-FinalizeLockClaim.ps1 -WorktreeCliExecutable '<worktreecli-exe>' -GitCommonDirectory '<git-common-dir>' -SessionLabel '<session-label>' -Worktree '<current-worktree>' -LandingOwner '<owner-token>' -LeaseSeconds '3600'
pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1 -CurrentWorktree '<current-worktree>' -PrimaryWorktree '<primary-worktree>' -CurrentBranch '<session-branch>' -PrimaryBranch '<primary-branch>' -ExpectedCurrentTip '<current-tip>' -ExpectedPrimaryTip '<primary-tip>' -SessionLabel '<session-label>' -ApprovedSessionCommit '<approved-commit>' -ApprovedCandidateTree '<approved-tree>' -ApprovalReviewResultFile 'Temp/finalize-approval-review-result.json' -OwnerToken '<owner-token>'
pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Show-FinalizeApprovalReview.ps1 -PrimaryWorktree '<primary-worktree>' -ApprovedTip '<landing-commit>' -LaunchSmartGit > 'Temp/finalize-approval-review-result.json'
pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Wait-AgentToolsQuiescence.ps1 -RepositoryRoot '<current-worktree>'
pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Invoke-AgentToolsPromotion.ps1 -PrimaryRoot '<primary-worktree>' -WorktreeCliCandidate '<worktreecli-candidate>' -AgentHarnessCandidate '<agentharness-candidate>' -LandedCommit '<landed-commit>'
pwsh -NoProfile -File .agents/scripts/Repair-SessionForkPoint.ps1
```

Add `-ReleasePlanClaim` to the landing command when a claimed Plan reached final
preparation, `-CommitMessageFile '<message-file>'` to the approval-preparation
command when the rules below call for the message override,
`-VerifiedCandidateCommit '<reviewed-commit>' -VerifiedCandidateTree
'<reviewed-tree>'` to that same approval-preparation command to prove the
reconciled session tree still equals a tree that was already reviewed, and
`-LeaseSeconds`, `-CooperatingSessionOwner`, or `-WaitSeconds` only where the
rules below call for a non-default value.
A landing claim's `-Worktree` is `'<current-worktree>'`, because landing accepts
the lease only when its recorded worktree matches the landing identity.
A recovery invocation after a crash repeats that same landing command with its
original approved arguments.

## Contracts

### Primary movement check

`Invoke-FinalizePrimaryMovementCheck.ps1` is read-only: it takes no lease and
does not change a ref, checkout, index, or worktree. It emits one fixed-shape
JSON result. The schema version is
`broken-engine-finalize-primary-movement/v2`. Its top-level fields are
`schemaVersion`, `status`, `code`, `message`, `candidate`, `tips`,
`ownedPaths`, `foreignCommits`, and `changes`:

- `status` is exactly `pass`, `needs-review`, `blocked`, or `error`.
- `candidate` is `{commit,tree,parent}`, with each member individually nullable.
  `tips` is `{session,livePrimary,relation}`.
- `ownedPaths` and `foreignCommits` are evidence objects
  `{totalCount,items,truncated}`. `ownedPaths.items` contains normalized
  relative POSIX paths. `foreignCommits.items` contains foreign commit
  identities ordered oldest-to-newest; the aggregate changed-path manifest is
  in `changes`.
- `changes` is `{totalCount,items,truncated,overlapPaths}`. Each evidence
  collection has `{totalCount,items,truncated}`. Each `items` row has exactly
  the tuple fields `(path,status,oldMode,newMode)`, and `overlapPaths` contains
  the overlap paths, computed over every changed path.
- Path collections are ordinal-sorted and deduplicated. Foreign commit items
  retain oldest-to-newest order, and change rows sort by the tuple
  `(path,status,oldMode,newMode)`. Every bounded collection has a cap of 500;
  truncation sets its `truncated` field and blocks the result with
  `primary.evidence-truncated` rather than allowing partial evidence to
  proceed.

The fixed terminal mapping is:

| Exit | Status | Codes | Finalizer action |
| ---: | --- | --- | --- |
| 0 | `pass` | `ok`, `primary.tree-identical` | Continue to the SmartGit launch line and the landing summary. |
| 0 | `needs-review` | `primary.disjoint-needs-review` | Follow the [finalizer worker workflow](worker.md#steps) for terminal handling and the SmartGit/summary sequence; the [root `AGENTS.md` Verify and land step's landing invariant](../../../../AGENTS.md) owns primary-movement policy. No conditional/preconfirmation landing lease is acquired; the normal postconfirmation claim uses the existing 3,600-second lease and owner-token continuation. |
| 2 | `blocked` | `candidate.session-tip-changed`, `candidate.tree-mismatch`, `candidate.parent-mismatch`, `primary.not-descendant`, `primary.path-overlap`, `primary.evidence-truncated` | Stop before SmartGit or the landing summary and return a blocker. |
| 1 | `error` | `input.invalid`, `assessment.failed` | Stop before SmartGit or the landing summary and return a blocker. |

Malformed JSON, a schema mismatch, or an exit/status mismatch is itself a
blocker. `code` and `message` are retained verbatim for the caller; no caller
reconstructs the assessment from Git output.

- `Invoke-FinalizeCandidateCommit.ps1` stages only the authorized caller
  paths, preserves disjoint state, and blocks mixed owned paths with a message
  naming the remedy — stage the named path with `git add -- <path>`, or restore
  it, so its index and worktree agree, then re-invoke. `pwsh -File`
  hands every argument over as one literal string, so several owned paths can
  only travel as one comma-separated `-OwnedPaths` token. `-OwnedPaths` carries
  the session's full caller-owned landing set, never a hand-trimmed subset: an
  entry already committed as a deletion is a satisfied no-op, so a resumed
  invocation passes the same set unchanged. A path found in neither the baseline
  tree nor the expected-tip tree nor the worktree still blocks with
  `input.path-not-single-entry`.
- `Invoke-FinalizeCandidateCommit.ps1` never advances primary. Its result is
  `broken-engine-finalize-candidate/v5`, carrying only the candidate.
- `Invoke-CodeQualityMetricsHistory.ps1 -Mode Generate` uses the exact
  `RepositoryRoot,BaseCommit,TipCommit,DateUtc,OutputDirectory` interface and
  writes only `CodeQualityMetricsHistory.jsonl` and
  `CodeQualityMetricsHistory.svg` into that directory. Production reads history
  bytes from the supplied immutable BaseCommit; it never uses a working-tree
  JSONL suffix. The producer alone decides what the new row contains.
- `Invoke-FinalizeApprovalPreparation.ps1` squashes the session work to
  one commit on the current primary tip, and blocks with
  `git.primary-not-ancestor` when the session tip does not already contain that
  primary tip; the caller recovers it from the session worktree root with the
  ordinary linear `git rebase refs/heads/<primary-branch>` — never the `--onto`
  form and never another ref — and then re-invokes this script with
  `-ExpectedCurrentTip` re-resolved to the rebased session tip and
  `-ExpectedPrimaryTip` re-resolved to the live primary tip, its other arguments
  unchanged; that rebase also invalidates any `<baseline>` already resolved for
  `Invoke-FinalizeCandidateCommit.ps1`, which `## Invocation` requires
  re-resolving. It returns the only landing commit sent to verification. That
  commit inherits the oldest session commit's message unless the optional
  `-CommitMessageFile` supplies an existing non-empty file whose text replaces
  it; the override also rebuilds the commit when the session range holds a
  single commit, so a candidate that gained content after creation can be
  re-messaged to describe it. The optional `-VerifiedCandidateCommit` and
  `-VerifiedCandidateTree` pair is supplied together or not at all; each must be
  a lowercase 40-character object ID naming an existing commit and that same
  commit's own tree; supplying one alone, a malformed ID, a missing commit, or a
  tree that is not that commit's tree fails with exit 1, `input.invalid`. When
  supplied, the script compares the reconciled session tip's tree against
  `-VerifiedCandidateTree` before squashing and blocks with exit 2,
  `candidate.tree-changed`, when they differ. The result's `verifiedCandidate`
  block reports `supplied`, true when the pair was passed, and `matched`, true
  once the comparison passed; a run omitting the pair reports both as false and
  performs no comparison.
- `Invoke-FinalizeLockClaim.ps1` makes one blocking lease claim —
  WorktreeCli owns the bounded wait and the guarded expiry recovery — and
  separately performs standalone release through `-Release` with the held
  lease's owner token.
  A claim emits its single result only when that wait resolves, so under
  contention it legitimately blocks for the full `-WaitSeconds` bound — 300
  seconds by default. Run every claim invocation, with or without
  `-LandingOwner`, under a host command timeout comfortably above that bound:
  at least 360,000 ms for the default, scaled up correspondingly for a longer
  `-WaitSeconds`. A contention result that arrives after that bound still
  reports `attempts: 1` and `retryAfterMilliseconds` equal to the poll
  interval; neither field records elapsed time, so judge whether the wait
  happened by wall-clock, never by those fields. `-Release` never enters the
  wait and is exempt. That timeout is a setting on the shell tool or runner,
  never text appended to the canonical command, which stays byte-identical. A
  host kill returns no structured result while the surviving child can still
  claim the lease: without `-LandingOwner` the minted owner token dies with
  that result, orphaning the lease under a token nobody holds and blocking
  every later claim until natural expiry, while a claim that supplied
  `-LandingOwner` still knows its token and releases the orphaned lease with
  `-Release`.
  Invoke it successfully before approval preparation begins
  reconciliation, retain or refresh the lease throughout agent-driven
  reconciliation, and release it with `-Release` before any user wait, in the
  order `worker.md` `## Bundled scripts` states; a release of an already-absent
  lease passes. The post-confirmation landing claim uses the landing lease
  duration — `-LeaseSeconds 3600`, its default, so omitting the parameter is
  correct; a refresh keeps a lease's original duration, so landing refuses to
  continue a shorter one. Live contention is
  retryable; only validated expiry recovers through WorktreeCli's
  compare-and-swap against the recorded owner, run only when no registered
  worktree has a Git operation in progress; unverifiable state requires user
  authority and is never overridden.

### Approval review receipt

`Show-FinalizeApprovalReview.ps1` emits one JSON line whose schema version is
`broken-engine-finalize-approval-review/v1`, carrying exactly `schemaVersion`,
`status`, `code`, `message`, `approvedTip`, `executable`, `arguments`,
`manualCommand`, and `processId`. `approvedTip` is the full 40-character
reviewed commit, expanded from an abbreviated `-ApprovedTip`. The redirected
artifact is that receipt: landing reads `approvedTip` and `status` from the
file named by `-ApprovalReviewResultFile`.

The review outcome stays non-blocking — `opened`, `unavailable`, and `failed` all
satisfy the gate, because only an attempted launch is required, not a successful
one. `preview` and every error status do not. A receipt that is absent,
unreadable, not valid JSON, or not a `broken-engine-finalize-approval-review/v1`
result carrying `status` and `approvedTip` blocks with `approval-review.missing`,
one whose `approvedTip` is not the commit being landed blocks with
`approval-review.candidate-mismatch`, and one recording no attempted launch
blocks with `approval-review.not-launched`. All three are exit 2, `blocked`,
`terminal`, and happen before the landing changes anything on primary. The
caller-owned lease claimed in the invocation order above is already live, and
the worker releases it with `-Release` exactly as for any other blocked landing.

A refreshed confirmation reruns the review against the newly reviewed candidate
and overwrites the artifact; the stale receipt is never reused, because its
`approvedTip` no longer matches the commit being landed.

### Landing and recovery

- `Invoke-FinalizeLanding.ps1` exclusively advances primary by
  compare-and-swap under the landing lock, rolls back on postcondition failure,
  and releases the lock. Before it registers the session, scans recovery, or
  claims or refreshes any lock itself, it
  enforces the [approval review receipt](#approval-review-receipt) contract on
  the receipt named by `-ApprovalReviewResultFile`.
  Its guarded primary checkout — the advance to the
  candidate and the rollback restore alike — waits out a foreign
  `.git/index.lock` on the primary repository, re-running the same checkout every
  500 milliseconds while git reports that lock. That waiting is bounded by a
  default budget of 500 seconds across the whole invocation, and every sleep pays
  for one more attempt, so a lock clearing right at the bound still lands; only
  when that last attempt still reports the lock does the run return the truthful
  terminal `git.rollback-failed` result, and it never reports a landed commit or
  tree it did not verify. A run that waited reports one `git.index-lock-wait`
  diagnostic naming the seconds spent, on a landed result as well as a failed
  one. Allow a landing invocation on the default budget a command timeout of up
  to 20 minutes; its worst case is about 13 minutes — the omitted-token route's
  300-second lock-claim wait, this 500-second index-lock budget, and the
  landing's own Git work, plus the measured roughly 60-second history Snapshot
  path — so a caller raising that budget must raise its host timeout
  by the same amount. Pass the post-confirmation claim's owner token as
  `-OwnerToken` so landing continues under that same lease, which it accepts only
  as a same-actor continuation under the `worker.md` `## Bundled scripts` ownership
  rule, preserving the raw `$SessionLabel`. Without `-OwnerToken`, it derives
  `$SessionLabel/landing`, first inspects the lock, and adopts a live owner only
  when that exact derived session and the same canonical worktree match and the
  recorded integer lease duration is at least the 3600-second landing duration; it
  immediately refreshes a matching retained lease before proceeding, preserving
  ownership across a possible rebase; otherwise
  it mints its own token through WorktreeCli `lock token` and claims under the
  derived identity. A recovery invocation applies the identity matching rule
  without the duration gate to release a live retained claim because it performs
  no rebase or advance; foreign, mismatched, and unverifiable claims are untouched.
  Under the held 3600-second lease, after any internal rebase, it runs Generate
  with one UTC row date frozen once for the whole landing and reused by every
  attempt, so an internal rebase cannot shift the recorded date. Landing
  measures the approved source patch from the confirmed candidate's own parent,
  so a candidate whose parent is behind live primary — including one where the
  intervening commits only rewrote the two generated history paths — is the
  ordinary bounded internal rebase case rather than a blocked result, and
  `-ExpectedPrimaryTip` remains the approved primary ancestor used for recovery
  matching. The producer receives a new, absent child
  directory beneath an existing ignored `Temp` parent; it never reuses an output
  directory, and landing makes no capture decision of its own. Both generated
  files are then committed over the approved source commit through a temporary
  index as one sole-parent commit with the frozen approved metadata; no branch
  ref moves while that object is built. The final result is
  `broken-engine-finalize-landing/v5` and separates
  `approvedSource`, `rebasedSource`, `historyUpdate`, and `final` commit/tree
  fields. The reviewed PNG deletion remains an ordinary source change; only the
  two generated paths are allowed after confirmation. When primary
  advanced first it makes at most one internal rebase, which requires a provably
  byte-identical patch outside those two paths. Report the commit from the
  result's `landed` block rather than `candidate`. A blocked result reports its
  `disposition` and a `lock` projection; act on those, never a memorized code
  list. A `retryable-wait` result may be re-invoked with the approval-bound
  arguments after its reported `retryAfterMilliseconds`. When it acquired no lock
  (`lock.claimed` false), simply re-invoke — on the caller-token route claim the
  landing lock anew through `Invoke-FinalizeLockClaim.ps1` first and pass the new
  owner token. When it acquired but did not release the lock (`lock.claimed`
  true, `lock.released` false), the claim is retained: handle it per the release
  paragraph below before claiming anew. Blocked
  `rebase.patch-not-identical` means the clean rebase changed the patch, which
  returns for re-review and a refreshed confirmation; `rebase.conflicted` leaves
  the session branch restored; `rebase.abort-failed` leaves restoration unproven
  and always retains the lease; `landing.retry-exhausted` is retryable and leaves
  the confirmed session commit restored. Blocked `history.source-changed` means
  the approved source patch itself — measured from that candidate's own parent —
  rewrites one of the two generated history paths, or that patch changed after
  confirmation; it is a genuine block that returns for re-review and a refreshed
  confirmation, never a condition a rebase can cure. Blocked
  `history.overlay-invalid` means the temporary overlay tree changed something
  other than the exact reserved JSONL/SVG pair; its message names the unexpected
  paths, and any reserved path the overlay failed to add. That guard runs before
  the compare-and-swap advance, so the run changed nothing on primary and the
  message is the whole evidence. A rolled-back
  `candidate.postcondition-failed` landing likewise leaves the session branch and
  worktree at the approved tip — not at the commit its own internal rebase
  produced — so re-invoking it with the original approved arguments passes strict
  sanity and needs no fresh approval preparation. Re-invoking a crashed landing
  that left primary advanced, with the original approved arguments, is idempotent,
  including against a tip its own internal rebase produced. Recovery walks
  first-parent from the primary ref back to `-ExpectedPrimaryTip` and accepts a
  commit only when it has exactly one parent, carries the frozen approved commit
  metadata, contains both generated history paths, and its patch identity
  excluding those two paths equals the approved patch identity. No match is
  ordinary foreign primary movement, not a blocker, when the primary head itself
  does not carry the frozen metadata; a head that does carry it with no matching
  commit blocks, as do multiple matches and a primary head that does not descend
  from `-ExpectedPrimaryTip`. It never assigns indices or reruns
  historical Snapshot, never rewrites a newer primary ref, and reconciles a
  proven-stale primary checkout plus the original session branch/worktree only
  after guarded checks prove each expected state; it never overwrites a mismatched
  checkout state. After the recovery lock, guarded checkout reconciliation
  re-reads the ref and proves the expected primary and session checkouts before
  resetting them. Before resetting the session checkout, its branch ref must be
  exactly the approved source/recovered commit or a proven single-parent
  patch-equivalent; an arbitrary ancestor is never a valid reset target.
  Pass `-ReleasePlanClaim` when a claimed Plan reached final preparation; the
  script then deletes the claim best-effort. Without the switch it invokes no
  `plan` command at all.

Release every caller-owned lease with `Invoke-FinalizeLockClaim.ps1 -Release`
before an open-ended user wait. After a failed landing the lock is released once
every registered worktree is inspectable and provably free of Git operation
markers, on both the supplied-token and omitted-token routes. When that cannot be
proven the claim is retained and reported: a supplied token's retained claim
stays the caller's lease, released the same way once the worktrees are provably
clear; an omitted-token claim is discoverable by a later invocation only when
its derived session and canonical worktree match, and every foreign or
unverifiable claim stays untouched until its normal expiry or external repair.

### Session fork-point repair

`.agents/scripts/Repair-SessionForkPoint.ps1` repairs one session branch whose
fork point left the primary branch's history because primary was rewritten under
it. `finalize-changes` is its sole documented caller; the `Recovery` entry in
[`worker.md`](worker.md#recovery) owns when to run it. It takes no parameters,
resolves the session from the worktree root it is run in, touches no lock, lease,
or primary ref, and never rebases primary.

It emits one compressed JSON line whose schema version is
`broken-engine-fork-point-repair/v1`, carrying exactly `schemaVersion`, `status`,
`code`, `message`, `exitCode`, `forkPoint`, `primaryTip`, `branch`, `baseline`,
and `rebased`:

| Exit | Status | Codes | Caller action |
| ---: | --- | --- | --- |
| 0 | `pass` | `ok` | The session branch was replayed onto `primaryTip` and `baseline` was recorded as the session's new baseline. Continue. |
| 2 | `blocked` | `fork-point.not-recoverable`, `fork-point.repair-refused` | The session branch was not replayed: `fork-point.not-recoverable` means the resolved fork point is still on primary's history, so there is nothing to replay, which after a detected rewrite means the true fork point has expired from the primary branch's reflog; `fork-point.repair-refused` means the shared repair threw — a dirty tree, a failed replay, or a Git failure. The message states any cleanup the worktree still needs. Return the message as a blocker. |
| 1 | `error` | `session.unresolved` | The session worktree could not be resolved. Return the message as a blocker. |
