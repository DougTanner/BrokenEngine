<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-15T20:47:57.326Z","dependsOn":[]} -->
# Fix: Invoke-FinalizeLanding — transient index.lock yields terminal git.rollback-failed

## Context
During post-confirmation finalization Step 5, the prescribed
`.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1`
invocation ran with `-ReleasePlanClaim` and the original approved arguments.
It exited `1` with `status=error`, `code=git.rollback-failed`, and
`disposition=terminal`. The exact reported message was:

```text
Exact candidate advance rollback did not restore the expected primary checkout: Primary checkout did not update to the exact candidate: fatal: Unable to create '<primary-git-common-dir>/.git/index.lock': File exists.

Another git process seems to be running in this repository, or the lock file may be stale
```

The result projection reported `primaryAdvanced=true`,
`candidate.treeVerified=true`, `landed.commit=null`, `landed.tree=null`,
`landed.rebaseAttempts=1`, `planClaim.requested=true`,
`planClaim.released=false`, `lock.claimed=true`, `lock.released=true`, and
`cleanup.worktreesClear=true` with zero problems or residuals. The terminal
result forced rework: the primary Git-operation marker had to clear, the exact
confirmed session commit had to be restored, a new 3,600-second landing lease
had to be claimed, and the original approved invocation had to be repeated.
That retry performed one byte-identity-preserving rebase and succeeded at
commit `ea1855d7c3d6c11d7816206fb7a4043df67ec54a`; both branches and
worktrees were clean and the Plan claim was released.

The claimed intent was
`Documents/Plans/Profile/FormatGameScreensDecomposition.md`; its approved
in-scope files were only `Projects/BrokenEngineSandbox/Source/Profile/ProfileManager.cpp`
and `ProfileManager.h`. The finalize landing script and its documentation were
outside that boundary, so this is tooling friction rather than an in-scope
acceptance failure. The active Plan has since landed and been deleted.

The observed gap is an undocumented/ad-hoc recovery route after a transient
Git operation marker or `.git/index.lock` prevents restoring the expected
primary checkout; the terminal result accurately records that the guarded
restore failed at that point. Durable source evidence shows the guarded
restore and result projection at
`.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1:133-136`
and `:254-278`; the retry and crash-recovery policy is at
`.agents/skills/finalize-changes/references/scripts.md:80-98`. This evidence
does not establish why the lock remained at the failed restore, and the later
manual recovery demonstrates eventual recoverability without establishing a
different result for the original attempt. The underlying root cause is
deferred to `/next-plan-review` below.

Session provenance (machine-local; not reproducible after cleanup):
- Client: codex
- Conversation session ID: none
- Worktree/branch UUID: 8fcbc57e-c17d-499d-b646-de10f0ffd5c3
- Session branch: codex/8fcbc57e-c17d-499d-b646-de10f0ffd5c3 (retained
  session provenance only; this mutable ref is not the landing selector)
- Worktree: .codex\worktrees\BrokenEngine\8fcbc57e-c17d-499d-b646-de10f0ffd5c3
- Landing commit: immutable landed commit
  `ea1855d7c3d6c11d7816206fb7a4043df67ec54a`; use this commit as the
  `/next-plan-review` target. The retained session branch may now point to a
  follow-up candidate and must not be used to select this landing. If this
  immutable hash is unavailable, stop and report the review blocked; do not
  substitute a Plan-history or session-branch commit.
- Run the review before /cleanup-worktrees removes this worktree: Codex
  transcript discovery requires the producing worktree to remain registered.

## Design
In a new session, run `/next-plan-review
ea1855d7c3d6c11d7816206fb7a4043df67ec54a` supplying the recorded Codex
client and the immutable landed commit as the review target. Root-cause the
friction from the proven result and
source evidence, then make the smallest fix inside the `## In scope` boundary
below. Decide whether the remedy is (a) a proven safe bounded automatic
recovery/retry in the landing script, or (b) a truthful terminal result with
complete actionable caller-facing recovery guidance that avoids ad-hoc
reconstruction; do not infer the root cause from the observed `index.lock` text
alone. If root-causing shows the fix lies outside the named skill, reference,
or landing script regions, surface it for re-planning instead of expanding
scope.

## Critical files
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1` — the
  `Advance-PrimaryExactCandidate` guarded advance/rollback and the final
  projection (`:133-136`, `:254-278`).
- `.agents/skills/finalize-changes/references/scripts.md` — retryable-wait,
  crash-recovery, and release guidance (`:80-108`).
- `.agents/skills/finalize-changes/SKILL.md` — Step 5/6 landing result handling
  and recovery guidance (`:114-126`, `:176-188`), only if the reviewed caller
  contract must change.

## In scope
- Root-cause investigation via `/next-plan-review`, run with the recorded
  immutable landed commit and Codex client.
- The smallest resulting fix, confined to the
  `Advance-PrimaryExactCandidate` rollback/result paths in
  `.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1`, the
  retry and crash-recovery guidance in
  `.agents/skills/finalize-changes/references/scripts.md`, and the matching
  Step 5/6 or Recovery prose in
  `.agents/skills/finalize-changes/SKILL.md` only when the review proves that
  caller guidance must change.

## Out of scope
- The landed ProfileManager change and its deleted Plan.
- Primary-branch changes, live Plan claims, landing-lock ownership, the
  WorktreeCli scheduler or lock implementation, host Git-operation-marker
  infrastructure, and unrelated finalize scripts or skills.
- Any transcript path or transcript text in the repository.

## Risk tier and invariants
Expected Tier 3 if the smallest fix changes primary compare-and-swap,
rollback, retry, or landing-lock cleanup behavior: this path is a landing
integration and safety invariant. If `/next-plan-review` proves the remedy is
documentation or result-contract wording only, reclassify to Tier 2 before
implementation. Preserve the exact candidate commit/tree guarantee, guarded
primary rollback and truthful terminal state, byte-identity-only rebase retry,
worktree/status restoration, lock release and Plan-claim behavior, and the
existing `broken-engine-finalize-landing/v3` result shape. No determinism/CRC,
serialization, replay, wire, runtime allocation, shader, or live-verification
state is exposed. Never embed transcript paths or home paths.

## Coordination
No dependency edge is required. The named documentation regions are disjoint
from the existing finalize Plans: `FinalizeLockClaimHostTimeout` owns lock-claim
guidance, `FinalizeCandidateResumedOwnedPaths` owns candidate path validation,
`FinalizeFixtureSuiteInvocationUndocumented` owns fixture-suite invocations,
and `FinalizeApprovalReviewInvocationContract` owns approval-review
invocation. A future implementation must rebase and re-read shared files
before editing if any of those Plans lands first.

## Acceptance criteria
- `/next-plan-review` confirms the recorded terminal result and identifies the
  smallest remedy within this Plan's boundary; an outside root cause is
  surfaced for re-planning.
- Repeating the documented post-confirmation landing invocation under the
  recorded transient-lock condition is handled by either (a) a proven safe
  bounded automatic recovery/retry, or (b) a truthful terminal result with
  complete actionable recovery guidance that avoids ad-hoc reconstruction; the
  recoverable case no longer requires an undocumented manual
  wait/restore/reclaim cycle.
- A genuinely unrecoverable rollback still returns a truthful
  `git.rollback-failed` outcome and never reports a landed commit/tree that was
  not verified.
- The successful path still lands only the exact candidate tree, permits at
  most the existing byte-identity-preserving rebase retry, leaves branches and
  worktrees clean, releases the landing lock, and releases the Plan claim when
  `-ReleasePlanClaim` is requested.
- `/validate-skill` passes for any changed `SKILL.md`; WorktreeCli `plan
  validate` exits `0` with `status:valid` and `code:ok`.

## Notes
This Plan is keyed to the pair (`Invoke-FinalizeLanding.ps1`, a transient
`.git/index.lock` causing terminal `git.rollback-failed` during guarded
rollback despite a successful byte-identity-preserving retry). A later
observation of that same pair is a duplicate, not a new residual. The existing
finalize Plans named in `## Coordination` cover different scripts or symptoms
and do not own this pair. The underlying root cause is intentionally deferred
to `/next-plan-review`; this body records only the exact observed result,
source locations, and forced rework.
