<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-30T15:54:03.136Z","dependsOn":[]} -->
# Bound finalize movement reviews after one optimistic invalidation

## Context

`/next-plan-review` proved the originating Claude session and landing
`9ca74c155653633db4bd8853be5807d95623812d`.

The historical and current workflow accepts an `independent` focused
dependency verdict only when candidate commit, candidate tree, candidate
parent, and live primary all match the confirming movement-check rerun. Every
live-primary change discards that verdict. There is no retry cap, reservation,
or starvation fallback, and no lease is held across the reviewer.

The observed accounting is four wrapper invocations producing five full
headless attempts. Two attempts returned substantive `independent` findings
but were unusable because a conflicting final-line instruction made the
wrapper result malformed. Two clean independent verdicts were discarded when
primary moved, and the final clean verdict was accepted. Reviewer execution
totaled 16m22s; the two clean movement-invalidated runs cost 398s. The prompt-
format defect is causally separate from movement starvation.

The checked primary tips were `3756c9d` (1 foreign commit, 5 paths, 0
overlaps), `62bcaff` (2, 15, 0), and `297d325` (3, 18, 0); `80d93b7` landed
later through the existing bounded internal rebase. Zero path overlap is
insufficient for lock-free rebinding: `62bcaff` changed the finalize workflow
itself while remaining disjoint from candidate-owned files, so semantic
dependency review was meaningful.

The root cause remains present at
`.agents/skills/finalize-changes/references/workflow.md:87-114`. The movement
checker result and terminal contract remain at
`.agents/skills/finalize-changes/references/scripts.md:88-130`, and the checker
still returns `primary.disjoint-needs-review` at
`.agents/skills/finalize-changes/scripts/Invoke-FinalizePrimaryMovementCheck.ps1:348-377`.

## Design

The user-selected conditional-lease design below is binding:

1. Keep the first `primary.disjoint-needs-review` dependency review lease-free.
2. Accept a valid `independent` verdict exactly as now when the confirming
   rerun remains `needs-review` and all four identities match.
3. If and only if that valid independent verdict is rejected solely because
   live primary changed and the new checker result is again `needs-review`,
   resume the same finalizer, claim the existing 3,600-second landing lease
   through `Invoke-FinalizeLockClaim.ps1`, and rerun the checker after the claim
   to freeze the actual review tip.
4. If the under-lease checker passes, release and continue without another
   reviewer. If it blocks, errors, or malforms, release and propagate the exact
   result. If it remains `needs-review`, dispatch exactly one fresh focused
   reviewer over the complete current movement evidence, not merely the
   suffix, still bound to candidate commit, tree, parent, and live primary.
5. Accept `independent` from the serialized reviewer only after one confirming
   checker rerun under the same lease with all four identities exact.
   `reachable`, `unknown`, malformed or unavailable review output, candidate
   or session change, or primary movement despite the lease blocks. There is
   no third dependency-review round.
6. Every terminal path releases the conditional lease through the existing
   release command before SmartGit, the landing summary, fallback
   authorization, or any user wait. Release failure blocks. After successful
   release, normal SmartGit, summary, and confirmation proceed; post-
   confirmation landing claims its normal fresh lease.
7. Reuse the existing lock, movement checker, and finalizer resume mechanisms.
   Add no configuration, new script, result field, status/code, or schema.

## Critical files

- `.agents/skills/finalize-changes/SKILL.md:54-74` — worker phase summary.
- `.agents/skills/finalize-changes/references/workflow.md:87-114` — movement-
  review state machine and lease lifecycle.
- `.agents/skills/finalize-changes/references/scripts.md:1-20,59-70,118-130,185-205,378-385` — lossless artifact handoff, existing claim/release commands, terminal mapping, and release guarantees.
- `.agents/skills/finalize-changes/scripts/Test-FinalizeWorkflowFixtures.ps1:2038-2113` — movement cases plus existing lock/continuity cases around lines 900-984 and 1546-1569.

## In scope

- The finalize-changes worker summary and reference state-machine edits in the
  critical ranges above, covering one optimistic invalidation threshold, one
  lease-backed final reviewer, and unconditional pre-wait release.
- The smallest fixture composition in
  `Test-FinalizeWorkflowFixtures.ps1` proving the existing movement checker,
  3,600-second landing claim, release, contention, continuity, and resume
  mechanisms support the design.
- Exact identity binding for candidate commit, candidate tree, candidate
  parent, and live primary; complete-current-evidence review after the
  under-lease `needs-review`; and propagation of exact checker/reviewer
  failures.
- The corrected observed evidence accounting recorded in `## Context`.

## Out of scope

- Any behavioral change to
  `Invoke-FinalizePrimaryMovementCheck.ps1`, `Invoke-FinalizeLockClaim.ps1`,
  `Invoke-FinalizeLanding.ps1`, their JSON schemas or codes,
  post-confirmation late-movement behavior, `/verify-changes`, confirmation,
  the history overlay, or blocking codes.
- Lock-free prefix rebinding, incremental-only reviews, new configuration, or
  a new coordination subsystem.
- The separate Codex review prompt/final-line validation defect; do not create
  a second Plan in this user-scoped stage.
- Changes to unrelated Plans, dependency metadata, or scheduler behavior.
- Implementation during this promotion stage.

## Risk tier and invariants

Future implementation is Change Workflow Tier 3 because it intentionally holds
the global landing lease across a focused reviewer and can queue other
sessions.

- The checker remains read-only and never acquires a lease.
- Any owned non-history overlap still blocks `primary.path-overlap`.
- Candidate commit, tree, parent, and live-primary binding remains exact.
- No lease is held across SmartGit or user wait; the same finalizer owns claim
  and release, and natural expiry remains crash recovery.
- Normal no-movement and stable first-review paths take no conditional lease.
- A lease-backed reviewer is limited to one serialized dependency-review round;
  unexpected movement or any non-independent result blocks.

## Coordination

This Plan has no dependencies. During the one serialized final-review round,
the existing global landing lease intentionally prevents a competing landing
claim; release restores normal claims before SmartGit, the summary, fallback
authorization, or user wait. No other Plan is jointly affected.

## Acceptance criteria

- Fixture and static workflow evidence prove that normal pass and stable first
  review use no conditional lease.
- A second `needs-review` after the first valid independent verdict causes
  exactly one existing 3,600-second claim, one post-claim checker rerun, at
  most one serialized reviewer, exact-identity confirmation, and release before
  SmartGit or the summary.
- A competing compliant landing claim cannot succeed during serialized review;
  after release, normal claims can proceed.
- An under-lease pass proceeds without review. Overlap, block, error, reviewer
  failure, or unexpected movement cannot open SmartGit and releases or reports
  the retained lease.
- Existing movement cases and lock continuity/release cases continue passing;
  complete `Test-FinalizeWorkflowFixtures.ps1` passes with
  `pwsh -NoProfile -File .agents/skills/finalize-changes/scripts/Test-FinalizeWorkflowFixtures.ps1`.
- `/validate-skill` passes for changed `SKILL.md`,
  `/progressive-disclosure-review` passes for changed instruction prose, and
  `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` exits 0.

## Notes

- No dependencies.
- Provenance (machine-local selection only): client `claude`, observing
  session `bf5fb2c4-ef24-4315-9cbe-2ab34ae1ec00`, worktree UUID
  `ca447260-1257-440a-889e-71d936639fcd`, profile-relative worktree
  `.claude\\worktrees\\BrokenEngine\\ca447260-1257-440a-889e-71d936639fcd`,
  landed commit `9ca74c155653633db4bd8853be5807d95623812d`. This Plan does not
  require another `/next-plan-review`; the root cause is already proven.
- Future implementation must run Tier-3 plan-audit, plan-simplicity-review,
  external-grill-plan, coherence, scope, adversarial, validate,
  progressive-disclosure, and verify steps before landing.
- The current promotion stage creates the executable Plan only and implements
  no workflow behavior.
