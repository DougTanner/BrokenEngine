<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-15T18:21:43.484Z","dependsOn":[]} -->
# Fix: finalize lock claim — default host timeout kills the blocking call before its JSON result

## Context
The `/finalize-changes` reconciliation lease claim was invoked in the exact
documented shape from
`.agents/skills/finalize-changes/references/scripts.md:15-19`:

```text
pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Invoke-FinalizeLockClaim.ps1 -WorktreeCliExecutable '<worktreecli-exe>' -GitCommonDirectory '<git-common-dir>' -SessionLabel '<session-label>' -Worktree '<current-worktree>'
```

No `-LandingOwner` was supplied. Neither `SKILL.md` nor `references/scripts.md`
specifies a required host execution timeout, so the host's nominal 10,000 ms
default was used. The tool yielded a running cell and then terminated with
`Exit code: 124` / `command timed out after 14014 milliseconds`; no structured
lock-claim result or owner token was produced. The script's own bounded
`-WaitSeconds` default is 300 seconds at
`.agents/skills/finalize-changes/scripts/Invoke-FinalizeLockClaim.ps1:13-15`,
which does not protect the call from the shorter host timeout.

Rework repeated the identical invocation with only the host timeout increased
to 60,000 ms. The known successful retry (owner token beginning `607d...`,
claimed at `2026-08-15T18:14:56.207Z`) completed in about 39.8 seconds with
the structured result `status=pass`, `code=ok`, message `Landing lock is live
and owned by this transaction`, a 3,600-second lease, `attempts=1`, and
`disposition=terminal`. That known retry lease was subsequently released; this
does not prove that every lease state was resolved.

Later candidate preparation was blocked by another lock-claim result:
`status=blocked`, `code=landing-lock.retryable-wait`,
`disposition=retryable-wait`, `attempts=1`,
`retryAfterMilliseconds=500`, and no user authority. It reported live foreign
owner `ac429ab9-d375-4cbb-93c9-73ca0fbeb714`; the record carried the same
session UUID and canonical worktree/logical Git key, but the attempted new
owner differed. Its claim/heartbeat was
`2026-08-15T18:16:23.201Z`, expiry was `2026-08-15T19:16:23.201Z`, and
`leaseDuration` was 3,600 seconds. This blocked further candidate preparation;
no repository change occurred, and the undisclosed-token lease required
natural expiry.

The later owner cannot be correlated to the earlier killed default-timeout
call: that call emitted no owner or timestamp, and child continuation was
unobservable. It is definitively not the later successful retry owner beginning
`607d...`, which was claimed at `2026-08-15T18:14:56.207Z` and released. The
blocked claim is additional consequence/ambiguity evidence for this same
skill/script-and-symptom key, not proof that the killed child created the
later lease.

The false required condition was that the documented one-call lock-claim form
could run under the host's default command timeout. The observed symptom and
its forced retry are the tooling-friction evidence; `/next-plan-review` must
root-cause whether the smallest remedy belongs in the caller documentation or
in the lock-claim script's bounded-wait/result contract.

Session provenance (machine-local; not reproducible after cleanup):
- Client: codex
- Conversation session ID: none
- Worktree/branch UUID: 64b3ed0d-6e92-4e96-a927-5eb6d7c15cf3
- Session branch: codex/64b3ed0d-6e92-4e96-a927-5eb6d7c15cf3
- Worktree: .codex\worktrees\BrokenEngine\64b3ed0d-6e92-4e96-a927-5eb6d7c15cf3
- Landing ref: the session branch above, whose tip is the session's final
  commit and which survives exactly as long as the worktree recorded above.
  Fallback once that branch is gone:
  `git log --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to this session alone
  (its diff limited to this session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes this worktree: Codex
  transcript discovery requires the producing worktree to remain registered.

## Design
In a new session, run `/next-plan-review <landing ref>` supplying the recorded
Codex client and landing ref; a Codex review supplies the client and landing ref
only. Root-cause the friction from the observed command/result evidence, then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows that the remedy requires the host runner, WorktreeCli, or another file
outside that boundary, surface it for re-planning instead of expanding scope.

## Critical files
- `.agents/skills/finalize-changes/SKILL.md` — bundled-script execution and
  reconciliation lease guidance (`:36-63`).
- `.agents/skills/finalize-changes/references/scripts.md` — the lock-claim
  invocation and blocking-wait/result contract (`:15-20`, `:49-64`).
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeLockClaim.ps1` —
  the `WaitSeconds`/`PollMilliseconds` defaults and fixed-shape result path
  (`:13-15`, `:26-45`, `:86-112`), only if review proves the script is the
  direct source of the observed friction.

## In scope
- Root-cause investigation via `/next-plan-review`, run with the recorded
  landing ref and Codex client.
- The smallest resulting fix, confined to the finalize lock-claim guidance in
  `.agents/skills/finalize-changes/SKILL.md` and
  `.agents/skills/finalize-changes/references/scripts.md`, including the
  documented host-timeout requirement and its distinction from the script's
  internal bounded wait.
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeLockClaim.ps1` only
  if `/next-plan-review` proves that its `WaitSeconds`/polling or fixed-shape
  completion path must change to make the documented invocation return its
  result; any such edit stays within those parameters and completion paths.

## Out of scope
- The active GameBase Render decomposition change, its modified
  `GameBase.cpp`/`.h`, and its deleted Plan.
- Primary-branch changes, Plan claims, landing locks, WorktreeCli scheduler or
  lock implementation, host execution infrastructure, and unrelated finalize
  scripts or skills.
- Any transcript path or transcript text in the repository.

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior); escalate if the fix reaches WorktreeCli,
host execution infrastructure, build/bootstrap coordination, or landing-gate
authorization. Preserve the lock-claim script's
`broken-engine-finalize-lock-claim/v1` result shape, bounded internal wait,
retryable/blocked/terminal dispositions, 3,600-second default lease, and
explicit owner-token release behavior. No determinism/CRC, serialization,
replay, wire, runtime allocation, shader, or live-verification state is
exposed. Never embed transcript paths or home paths.

## Acceptance criteria
- The recorded lock-claim invocation, run with its documented host timeout,
  no longer reproduces the host kill before a structured result; a successful
  claim returns its owner token and the documented pass result, while
  contention still reports the script's retryable or blocked result.
- The documentation distinguishes the host execution timeout from the
  script's internal `WaitSeconds` bound and gives a runnable canonical form.
- Releasing the resulting owner token remains possible through the documented
  `-Release` invocation, with no lease held across the user-confirmation wait.
- `/validate-skill` passes for any changed `SKILL.md`; WorktreeCli `plan
  validate` exits `0` with `status:valid` and `code:ok`.

## Notes
This Plan is keyed to the pair (`/finalize-changes` /
`Invoke-FinalizeLockClaim.ps1`, the host's default command timeout kills the
blocking reconciliation lease claim before its JSON result). The later
retryable-wait observation above is additional evidence for this same key,
not a separate Plan or proof that the killed child created that lease. A later
observation of the same skill/script and symptom remains a duplicate, not a
new residual. It is distinct from
`Documents/Plans/Agents/CodexReviewLongRunHostTimeout.md`, which covers the
`.codex/codex-review.ps1` ten-minute cap; from
`Documents/Plans/Agents/FinalizeApprovalReviewInvocationContract.md`, which
covers missing mandatory approval-review parameters; and from
`Documents/Plans/Agents/FinalizeFixtureSuiteInvocationUndocumented.md`, which
covers absent fixture-suite invocation documentation.
