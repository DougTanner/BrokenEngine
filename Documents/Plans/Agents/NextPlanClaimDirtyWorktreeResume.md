<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-16T14:17:14.351Z","dependsOn":[]} -->
# Fix: next-plan claim resume — retained dirty implementation blocks documented re-claim

## Context

After an explicit instruction to stop and leave the implementation to Plans, the documented deferral command was run:

```powershell
pwsh -NoProfile -File .agents/skills/next-plan/scripts/Defer-NextPlan.ps1
```

It returned a passing release result (`status: pass`, released), but `DataPacker/Source/Attribution.cpp` remained dirty. On explicit resume, the documented targeted claim command was run:

```powershell
pwsh -NoProfile -File .agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1 -Plan 'Documents/Plans/DataPacker/AttributionStaleOutputReconciliation.md'
```

It returned blocked `claim.context-conflict` with the message `Session worktree must be clean before a plan claim.` The documented workflow provided no resume route for the retained dirty implementation. The user-authorized workaround stashed only `Attribution.cpp`, reran the claim, accepted the fast-forward from `ea1855d7c3d6c11d7816206fb7a4043df67ec54a` to `b2a5c51ab984cb1bf355f9a92c462af0af670bcb`, reapplied the exact stash, verified that the scoped primary movement did not touch `Attribution.cpp`, and then dropped the stash. This is tooling friction outside the claimed Plan, whose scope is only Attribution stale-output reconciliation.

Session provenance (machine-local; not reproducible after cleanup):

- Client: codex
- Conversation session ID: none
- Worktree/branch UUID: d8d1a590-d3ad-43cb-a16c-c176595b72b1
- Session branch: codex/d8d1a590-d3ad-43cb-a16c-c176595b72b1
- Worktree: .codex\worktrees\BrokenEngine\d8d1a590-d3ad-43cb-a16c-c176595b72b1
- Landing ref: the session branch above, whose tip is the session's final commit and which survives exactly as long as the worktree recorded above. Fallback once that branch is gone: `git log --diff-filter=A --format=%H -- <this plan path>`, but a periodic Plan-history squash can make it return an unrelated aggregate commit, so review its result only when the commit is attributable to this session alone (its diff limited to this session's files); never review an aggregate or multi-session squash commit.
- Run the review before `/cleanup-worktrees` removes this worktree: Codex transcript discovery requires the producing worktree to remain registered.

## Design

In a new session, run `/next-plan-review <landing ref>` supplying the recorded Codex client and landing ref. Root-cause the friction from the proven run, then make the smallest fix inside the `## In scope` boundary below. If root-causing shows the fix lies outside that boundary, surface it for re-planning instead of expanding scope.

## Critical files

- `.agents/skills/next-plan/SKILL.md` — the documented deferral and clean-session preconditions.
- `.agents/skills/next-plan/scripts/Defer-NextPlan.ps1` — the deferral/release result and retained-worktree transition.
- `.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1` — the targeted resume claim and clean-worktree context check.

## In scope

- Root-cause investigation via `/next-plan-review` with the recorded provenance.
- The smallest resulting fix confined to the deferral/release and targeted-claim resume handling in the three named skill/script files. If the review proves that the common workflow module or WorktreeCli owns the root cause, stop at that boundary and surface the required re-planning pivot instead of expanding this Plan.

## Out of scope

- The Attribution stale-output reconciliation attempt and its rejected Plan.
- Attribution implementation behavior, scheduler/claim storage, WorktreeCli, unrelated skills/scripts, and any transcript path or transcript text in the repository.
- Broad staging, stashing, or automatic disposal of user implementation changes; any remedy must preserve the no-loss and clean-claim safety contract.

## Risk tier and invariants

Expected Tier 2 (scoped next-plan workflow behavior); escalate if the smallest fix reaches WorktreeCli scheduler or build/bootstrap coordination. Preserve one live claim per session, clean-claim safety, truthful pass/blocked/error result envelopes, and user changes across deferral and resume. Never embed transcript paths or home paths.

## Acceptance criteria

- The recorded deferral-then-targeted-resume sequence no longer reaches `claim.context-conflict` solely because the implementation intentionally retained dirty changes after deferral, or it returns a truthful actionable result that preserves those changes without an undocumented stash workaround.
- A genuinely unsafe or ambiguous dirty-worktree state remains blocked without silently discarding or broadening user changes.
- `/next-plan-review` records the proven root cause and smallest fix within the named boundary; an outside root cause is surfaced for re-planning.
- `/validate-skill` passes for any changed `SKILL.md`, and WorktreeCli `plan validate` exits `0` with `status: valid` and `code: ok`.

## Notes

This Plan is keyed to the pair (`Defer-NextPlan.ps1` followed by targeted `Invoke-NextPlanClaim.ps1`, retained dirty implementation, and `claim.context-conflict` / `Session worktree must be clean before a plan claim`). A later observation of the same script-plus-symptom pair is a duplicate, not a new residual. The root cause is intentionally deferred to `/next-plan-review`; this body records the exact commands, observed result, and authorized workaround without embedding transcript material.
