# Bundled Scripts

Use the bundled scripts; never reconstruct their Git, lock, or WorktreeCli
operations. Parse their single fixed-shape JSON result and show only status, code,
message, next-stage state, short counts/paths, and retry or authority
outcome when applicable. Never return a nested tool response or
file/XML/log body. Exit/result/schema mismatches block.

## Invocation

Use the root AGENTS.md canonical form — from the session worktree root, one
script invocation per shell call, repo-relative path, no `-ExecutionPolicy`.
Angle-bracket values are placeholders; quote every one. `<owner-token>` is a
canonical lowercase GUID in `8-4-4-4-12` form. For the lock claim, either omit
`-LandingOwner` so `Invoke-FinalizeLockClaim.ps1` creates and returns one, or
create one explicitly with `WorktreeCli lock token`; later commands that supply
the already-held token, including `-AdvancePrimary`, carry it and do not create
one.

```text
pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Invoke-FinalizeCandidateCommit.ps1 -Route session-landing -CurrentWorktree '<current-worktree>' -PrimaryWorktree '<primary-worktree>' -CurrentBranch '<session-branch>' -PrimaryBranch '<primary-branch>' -Baseline '<baseline>' -ExpectedCurrentTip '<current-tip>' -ExpectedPrimaryTip '<primary-tip>' -OwnedPaths '<path>,<path>' -CommitMessageFile '<message-file>'
pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Invoke-FinalizeCandidateCommit.ps1 -Route primary-commit -CurrentWorktree '<primary-worktree>' -PrimaryWorktree '<primary-worktree>' -CurrentBranch '<primary-branch>' -PrimaryBranch '<primary-branch>' -Baseline '<baseline>' -ExpectedCurrentTip '<primary-tip>' -ExpectedPrimaryTip '<primary-tip>' -OwnedPaths '<path>,<path>' -CommitMessageFile '<message-file>'
pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Invoke-FinalizeLockClaim.ps1 -WorktreeCliExecutable '<worktreecli-exe>' -GitCommonDirectory '<git-common-dir>' -SessionLabel '<session-label>' -Worktree '<primary-worktree>' -LandingOwner '<owner-token>' -LeaseSeconds '3600'
pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Invoke-FinalizeCandidateCommit.ps1 -Route primary-commit -CurrentWorktree '<primary-worktree>' -PrimaryWorktree '<primary-worktree>' -CurrentBranch '<primary-branch>' -PrimaryBranch '<primary-branch>' -Baseline '<baseline>' -ExpectedCurrentTip '<candidate-parent>' -ExpectedPrimaryTip '<candidate-parent>' -OwnedPaths '<path>,<path>' -CommitMessageFile '<message-file>' -VerifiedCandidateCommit '<candidate-commit>' -VerifiedCandidateTree '<candidate-tree>' -HistoryContractDigest '<contract-digest>' -HistoryContractGeneratorDigest '<generator-digest>' -HistoryContractCaptureDigest '<capture-digest-or-empty>' -HistoryContractRuntimeDigest '<runtime-digest-or-empty>' -HistoryContractPatchDigest '<patch-digest>' -HistoryContractMode '<catch-up|cpp-change|carry-forward>' -WorktreeCliExecutable '<worktreecli-exe>' -SessionLabel '<session-label>' -OwnerToken '<owner-token>' -AdvancePrimary
pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Invoke-FinalizeApprovalPreparation.ps1 -CurrentWorktree '<current-worktree>' -PrimaryWorktree '<primary-worktree>' -CurrentBranch '<session-branch>' -PrimaryBranch '<primary-branch>' -ExpectedCurrentTip '<current-tip>' -ExpectedPrimaryTip '<primary-tip>'
pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Invoke-FinalizeLockClaim.ps1 -WorktreeCliExecutable '<worktreecli-exe>' -GitCommonDirectory '<git-common-dir>' -SessionLabel '<session-label>' -Worktree '<current-worktree>'
pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Invoke-FinalizeLockClaim.ps1 -WorktreeCliExecutable '<worktreecli-exe>' -GitCommonDirectory '<git-common-dir>' -SessionLabel '<session-label>' -Worktree '<current-worktree>' -LandingOwner '<owner-token>' -Release
pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Invoke-FinalizeLockClaim.ps1 -WorktreeCliExecutable '<worktreecli-exe>' -GitCommonDirectory '<git-common-dir>' -SessionLabel '<session-label>' -Worktree '<current-worktree>' -LandingOwner '<owner-token>' -LeaseSeconds '3600'
pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1 -CurrentWorktree '<current-worktree>' -PrimaryWorktree '<primary-worktree>' -CurrentBranch '<session-branch>' -PrimaryBranch '<primary-branch>' -ExpectedCurrentTip '<current-tip>' -ExpectedPrimaryTip '<primary-tip>' -SessionLabel '<session-label>' -ApprovedSessionCommit '<approved-commit>' -ApprovedCandidateTree '<approved-tree>' -HistoryContractDigest '<contract-digest>' -HistoryContractGeneratorDigest '<generator-digest>' -HistoryContractCaptureDigest '<capture-digest-or-empty>' -HistoryContractRuntimeDigest '<runtime-digest-or-empty>' -HistoryContractPatchDigest '<patch-digest>' -HistoryContractMode '<catch-up|cpp-change|carry-forward>' -OwnerToken '<owner-token>'
pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1 -CurrentWorktree '<current-worktree>' -PrimaryWorktree '<primary-worktree>' -CurrentBranch '<session-branch>' -PrimaryBranch '<primary-branch>' -ExpectedCurrentTip '<current-tip>' -ExpectedPrimaryTip '<primary-tip>' -SessionLabel '<session-label>' -ApprovedSessionCommit '<approved-commit>' -ApprovedCandidateTree '<approved-tree>' -HistoryContractDigest '<contract-digest>' -HistoryContractGeneratorDigest '<generator-digest>' -HistoryContractCaptureDigest '<capture-digest-or-empty>' -HistoryContractRuntimeDigest '<runtime-digest-or-empty>' -HistoryContractPatchDigest '<patch-digest>' -HistoryContractMode '<catch-up|cpp-change|carry-forward>' -HistoryContractRowDate '<historyUpdate.rowDate>' -HistoryJsonSha256 '<historyUpdate.jsonl.sha256>' -HistoryJsonBytes '<historyUpdate.jsonl.bytes>' -HistorySvgSha256 '<historyUpdate.svg.sha256>' -HistorySvgBytes '<historyUpdate.svg.bytes>' -HistorySvgEmbeddedSha256 '<historyUpdate.svg.embeddedSha256>' -OwnerToken '<owner-token>'
pwsh -NoProfile -File .agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1 -Mode Contract -RepositoryRoot '<current-worktree>' -BaseCommit '<primary-tip>' -TipCommit '<approved-commit>'
pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Show-FinalizeApprovalReview.ps1 -PrimaryWorktree '<primary-worktree>' -ApprovedTip '<landing-commit>' -LaunchSmartGit
pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Wait-AgentToolsQuiescence.ps1 -RepositoryRoot '<current-worktree>'
pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Invoke-AgentToolsPromotion.ps1 -PrimaryRoot '<primary-worktree>' -WorktreeCliCandidate '<worktreecli-candidate>' -AgentHarnessCandidate '<agentharness-candidate>' -LandedCommit '<landed-commit>'
```

Add `-ReleasePlanClaim` to the landing command when a claimed Plan reached final
preparation, `-CommitMessageFile '<message-file>'` to the approval-preparation
command when the rules below call for the message override, and `-LeaseSeconds`,
`-CooperatingSessionOwner`, or `-WaitSeconds`
only where the rules below call for a non-default value.
A landing claim's `-Worktree` is the route's own worktree —
`'<current-worktree>'` on the session route and `'<primary-worktree>'` on the
separately requested primary-commit route — because landing accepts the lease
only when its recorded worktree matches the landing identity.
The second landing form is the recovery invocation when a structured result
survived: map all six history values from its `historyUpdate` fields exactly as
shown. After a hard crash with no result, use the first form and omit all six;
never pass a partial recovery tuple.

## Contracts

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
- On `-Route primary-commit` without `-AdvancePrimary`, the candidate result is
  bound to the read-only history Contract before verification. On the confirmed
  `-AdvancePrimary` resume, `-OwnerToken` is mandatory and must name the caller's
  live 3600-second landing lease; no omitted-token primary mutation is allowed.
  The result is `broken-engine-finalize-candidate/v3` with `historyContract`,
  `historyUpdate`, and `final` fields. The final commit is the deterministic
  sole-parent replacement, while `candidate.commit` remains the reviewed source
  candidate.
- `Invoke-CodeQualityMetricsHistory.ps1 -Mode Contract -RepositoryRoot <root>
  -BaseCommit <primary-tip> -TipCommit <source-tip>` is read-only and writes no
  tracked file. Generate uses the same exact `RepositoryRoot,BaseCommit,TipCommit,DateUtc,OutputDirectory`
  interface. Production reads history bytes from the supplied immutable BaseCommit;
  it never uses a working-tree JSONL suffix. Its exact typed result is
  `broken-engine-code-quality-history-contract/v1`; approval preparation returns
  that receipt plus a compact canonical receipt digest, generator digest, optional
  capture digest, patch digest, and capture mode. `series.historyBytesSha256` binds
  the complete BaseCommit history bytes, including the live suffix. Contract mode
  does not choose a final date, row index, or output hash — those are dynamic under
  the landing lock.
- `Invoke-FinalizeApprovalPreparation.ps1` squashes the session work to
  one commit on the current primary tip, and blocks with
  `git.primary-not-ancestor` when the session tip does not already contain that
  primary tip; the caller rebases and re-invokes. It returns the only
  landing commit sent to verification. That commit inherits the oldest session
  commit's message unless the optional `-CommitMessageFile` supplies an existing
  non-empty file whose text replaces it; the override also rebuilds the commit
  when the session range holds a single commit, so a candidate that gained
  content after creation can be re-messaged to describe it.
- `Invoke-FinalizeLockClaim.ps1` makes one blocking lease claim —
  WorktreeCli owns the bounded wait and the guarded expiry recovery — and
  separately performs standalone release through `-Release` with the held
  lease's owner token.
  A claim emits its single result only when that wait resolves, so under
  contention it legitimately blocks for the full `-WaitSeconds` bound — 300
  seconds by default. Run every claim invocation, with or without
  `-LandingOwner`, under a host command timeout comfortably above that bound:
  at least 360,000 ms for the default, scaled up correspondingly for a longer
  `-WaitSeconds`. `-Release` never enters the wait and is exempt. That timeout
  is a setting on the shell tool or runner, never text appended to the
  canonical command, which stays byte-identical. A host kill returns no
  structured result while the surviving child can still claim the lease:
  without `-LandingOwner` the minted owner token dies with that result,
  orphaning the lease under a token nobody holds and blocking every later
  claim until natural expiry, while a claim that supplied `-LandingOwner`
  still knows its token and releases the orphaned lease with `-Release`.
  Invoke it successfully before approval preparation begins
  reconciliation, retain or refresh the lease throughout agent-driven
  reconciliation, and release it with `-Release` before any user wait, in the
  order `SKILL.md` `## Bundled scripts` states; a release of an already-absent
  lease passes. The post-confirmation landing claim uses the landing lease
  duration — `-LeaseSeconds 3600`, its default, so omitting the parameter is
  correct; a refresh keeps a lease's original duration, so landing refuses to
  continue a shorter one. Live contention is
  retryable; only validated expiry recovers through WorktreeCli's
  compare-and-swap against the recorded owner, run only when no registered
  worktree has a Git operation in progress; unverifiable state requires user
  authority and is never overridden.
- `Invoke-FinalizeLanding.ps1` exclusively advances primary by
  compare-and-swap under the landing lock, rolls back on postcondition failure,
  and releases the lock. Its guarded primary checkout — the advance to the
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
  catch-up/C++ path — so a caller raising that budget must raise its host timeout
  by the same amount. Pass the post-confirmation claim's owner token as
  `-OwnerToken` so landing continues under that same lease, which it accepts only
  as a same-actor continuation under the `SKILL.md` `## Bundled scripts` ownership
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
  Under the held 3600-second lease it re-runs Contract against current primary and
  source, permits only dynamic corpus/history data plus the documented
  `catch-up`/`cpp-change`/`carry-forward` mode reclassification, and runs Generate
  with one captured UTC date into a unique ignored `Temp` directory. Generate must
  classify the mode before capture comparison: only an approved `catch-up` that
  narrows to `carry-forward` may lose its active capture/runtime identity; every
  other missing or changed active identity blocks. Generate must return
  `broken-engine-code-quality-history-update/v1` with exactly
  `CodeQualityMetricsHistory.jsonl` and `CodeQualityMetricsHistory.svg`; hashes,
  sizes, row date/index, coverage, and SVG embedded series/generator digests are
  validated before a temporary-index overlay creates one sole-parent replacement
  commit with frozen approved metadata. The producer receives a new, absent child
  directory beneath an existing ignored `Temp` parent; it never reuses an output
  directory. The public v4 receipt stores only the two repository-relative
  reserved paths, hashes, sizes, dates, indices, and digests — never an absolute
  or GUID-bearing Temp path. No branch ref moves while that object is built. The final result is `broken-engine-finalize-landing/v4` and separates
  `approvedSource`, `rebasedSource`, `historyUpdate`, and `final` commit/tree
  fields. The reviewed PNG deletion remains an ordinary source change; only the
  two reserved generated paths are allowed after confirmation. When primary
  advanced first it makes at most one internal rebase and lands only a provably
  byte-identical non-history patch plus a valid regenerated overlay, so report the
  commit from the result's `landed` block rather than `candidate`. A blocked result reports its
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
  the confirmed session commit restored. Re-invoking it with the original approved
  arguments after a crash is idempotent, including against a tip its own internal
  rebase produced. Recovery searches only first-parent descendants back to the
  approved ancestor for exactly one replacement matching frozen metadata, the
  approved non-history patch digest, one valid committed row date, and exact
  JSONL/SVG plus embedded SVG digests. It never assigns indices or reruns
  historical Snapshot, never changes newer primary, and resets only the original
  session branch/worktree; zero, multiple, or non-ancestor matches block. When a
  structured result survived, pass its complete row-date/hash/size/embedded-digest
  tuple for an additional exact match; omit the whole tuple after a hard crash,
  because a partial tuple is invalid.
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

## Fixture suites

Three bundled suites cover the scripts above against disposable scratch
repositories. Run one only when its trigger list below changes; each uses the
same canonical invocation form as the commands above.

```text
pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Test-FinalizeWorkflowFixtures.ps1 -WorktreeCliExecutable '<worktreecli-exe>'
pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Test-LandingLockStatusFixtures.ps1 -WorktreeCliExecutable '<worktreecli-exe>'
pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Test-AgentToolsPromotionFixtures.ps1 -WorktreeCliExecutable '<worktreecli-exe>' -AgentHarnessExecutable '<agentharness-exe>'
```

Substitute the session worktree's provisioned tool binaries that `/compile`
documents — `Tools/WorktreeCli/Platforms/VisualStudio2026/Output/WorktreeCli.exe`
for `<worktreecli-exe>` and
`Tools/AgentHarness/Platforms/VisualStudio2026/Output/AgentHarness.exe` for
`<agentharness-exe>` — and never pass a placeholder literally.

- `Test-FinalizeWorkflowFixtures.ps1` runs when
  `Invoke-FinalizeCandidateCommit.ps1`,
  `Invoke-FinalizeApprovalPreparation.ps1`, `Invoke-FinalizeLockClaim.ps1`,
  `Invoke-FinalizeLanding.ps1`, `Show-FinalizeApprovalReview.ps1`,
  `.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1`,
  `.agents/scripts/AgentScriptCommon.psm1`,
  `.agents/scripts/AgentWorktreeSession.psm1`,
  `.agents/scripts/FinalizeWorkflowCommon.psm1`,
  `.agents/scripts/WorktreeCliSessionExclusion.psm1`, or the suite itself
  changes.
- `Test-LandingLockStatusFixtures.ps1` exercises WorktreeCli's `lock` CLI
  directly rather than a bundled script, so it runs when WorktreeCli's
  landing-lock implementation, `.agents/scripts/FinalizeWorkflowCommon.psm1`, or
  the suite itself changes.
- `Test-AgentToolsPromotionFixtures.ps1` runs when
  `Invoke-AgentToolsPromotion.ps1`,
  `.agents/scripts/Test-AgentToolsCapabilities.ps1`,
  `.agents/scripts/AgentScriptCommon.psm1`,
  `.agents/scripts/AgentWorktreeSession.psm1`,
  `.agents/scripts/FinalizeWorkflowCommon.psm1`,
  `.agents/scripts/WorktreeCliSessionExclusion.psm1`, the AgentTools promotion
  contract in `agenttools.md`, or the suite itself changes. Both supplied
  executables must pass the capability pre-flight the suite runs before it
  allocates any scratch repository.
