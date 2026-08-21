<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-21T13:38:36.335Z","dependsOn":["Documents/Plans/Skills/FinalizeCandidateStaleBaseHistoryBlock.md","Documents/Plans/Skills/FinalizeChangesCandidateOrderContradiction.md"]} -->
# Fix: /finalize-changes step 4 — relax the exact PrimaryTip match to disjoint foreign movement

## Context

`.agents/skills/finalize-changes/SKILL.md` step 4 requires the re-resolved live
`PrimaryTip` to equal the approval-preparation result's `candidate.parent`
exactly; on any mismatch it forbids opening the review window or returning a
landing summary and returns a blocker so main re-reconciles before the
confirmation is asked.

Observed in the session landed as
`6f1738b6d377e25164bf068b670cc3cde0b30b58`: during finalization, primary
advanced three times — `aa69f29` -> `7c5c543` -> `4e00512` — and every one of
those foreign commits touched a file set fully disjoint from the session's 11
landing paths. Because step 4 compares commit identity rather than
reachability, each advance forced a full pre-confirmation re-reconcile round:
a new rebase that was provably byte-identical (the patch digest stayed
`fc006c60...` throughout), a regenerated Contract receipt (its aggregate digest
is bound to `source.baseCommit`, so it goes stale on every base move), and a
repeat of step 4. Two rounds were wasted outright, plus one mid-rebase
`sanity.git.primary-tip-changed` block when the tip moved again during
reconciliation.

The post-confirmation path already tolerates exactly this movement. The
confirmation contract binds the reviewed diff, not hashes — "A clean identical
rebase onto an advanced primary lands without re-asking" — and
`.agents/skills/finalize-changes/references/scripts.md` documents that
`Invoke-FinalizeLanding.ps1`, "when primary advanced first[,] makes at most one
internal rebase and lands only a provably byte-identical non-history patch plus
a valid regenerated overlay". So the strict pre-confirmation match buys nothing
the landing invocation does not already enforce, while costing a full round per
foreign advance.

User decision (locked): relax the step-4 requirement for a smoother flow —
approximately 99% of the time the changes presented to the user do not change
on a rebase.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed this friction:
- Client: claude
- Conversation session ID: f94320e0-0a2e-4f6d-86f1-f503f3412cd2
- Worktree/branch UUID: a33e5f42-2f30-40dc-ab87-1ac73dfba01a
- Session branch: claude/a33e5f42-2f30-40dc-ab87-1ac73dfba01a
- Worktree: .claude\worktrees\BrokenEngine\a33e5f42-2f30-40dc-ab87-1ac73dfba01a
- Landed commit of the observing session:
  6f1738b6d377e25164bf068b670cc3cde0b30b58
- Landing ref: claude/a33e5f42-2f30-40dc-ab87-1ac73dfba01a — the observing
  session recorded this Plan itself, so its own branch tip carries it.
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Skills/FinalizeChangesStep4TipMatchRelaxation.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above.

## Design

The mechanism is decided; no investigation is required before implementing it.

1. `.agents/skills/finalize-changes/SKILL.md` step 4: replace the
   exact-equality requirement with a reachability test. On a `PrimaryTip`
   mismatch, block — with today's blocker naming both values — only when the
   foreign movement between `candidate.parent` and the live tip overlaps the
   session's landing paths or is reachable from the session's changed code.
   Otherwise proceed to `Show-FinalizeApprovalReview.ps1` and the landing
   summary against the existing candidate, unchanged. The summary's
   `## Landing` section then states both the candidate parent and the newer
   live tip, and that landing will rebase internally and land only a
   byte-identical patch.
2. No landing-script change is needed; the relaxed step 4 relies on behavior
   `Invoke-FinalizeLanding.ps1` already has.
   `Validate-HistoryContractForLanding` (`Invoke-FinalizeLanding.ps1:295`)
   already accepts the approved scalars when the live base differs from
   `-ExpectedPrimaryTip`: line 314 applies the aggregate contract-digest
   equality check only when `$BaseCommit -ceq $ExpectedPrimaryTip`, so a
   receipt that went stale purely because the base moved is not rejected, while
   the generator identity (line 298), capture mode (299-301), capture/runtime
   identity (302-310), history patch digest (311), and non-history patch
   identity (312-313) are each still checked independently. The Contract and
   the overlay are regenerated under the landing lock. So the post-confirmation
   path already absorbs disjoint tip movement and lands only a byte-identical
   patch; every non-byte-identical divergence keeps today's blocked meaning and
   returns for refreshed review and confirmation.

Both `dependsOn` prerequisites rewrite the neighbouring wording region of the
same `SKILL.md`, so they land first and this Plan starts from their wording.

## Critical files

- `.agents/skills/finalize-changes/SKILL.md` — step 4's tip-match requirement
  and the landing summary's `## Landing` contents.
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1` —
  read-only reference: `Validate-HistoryContractForLanding` (line 295, and the
  base-conditional aggregate check at line 314) is the existing behavior the
  relaxed step 4 relies on. Not changed by this Plan.
- `.agents/skills/finalize-changes/references/scripts.md` — read-only reference
  for the documented invocation, scalar contract, and blocked-result wording,
  changed only in the conditional case named in `## In scope`.

`SKILL.md` is the authorized fix boundary, plus `references/scripts.md` only in
that conditional case.

## In scope

- Step 4 of `.agents/skills/finalize-changes/SKILL.md`: the `PrimaryTip`
  comparison, its blocker condition, and the `## Landing` section's required
  contents, exactly as decided in `## Design`.
- The matching wording in
  `.agents/skills/finalize-changes/references/scripts.md`, and only if that
  wording misstates the landing script's existing behavior described in
  `## Design` item 2; if it already states it correctly, leave it unchanged.

## Out of scope

- Any edit to
  `.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1`,
  including its receipt re-check, the `HistoryContract*` scalar contract, and
  `-ExpectedPrimaryTip` handling.
- `Invoke-FinalizeCandidateCommit.ps1`, `Invoke-FinalizeApprovalPreparation.ps1`,
  `Invoke-FinalizeLockClaim.ps1`, and `Show-FinalizeApprovalReview.ps1`.
- Step 2's ordering wording (owned by
  `Documents/Plans/Skills/FinalizeChangesCandidateOrderContradiction.md`) and
  the stale-base history block message (owned by
  `Documents/Plans/Skills/FinalizeCandidateStaleBaseHistoryBlock.md`).
- The `primary-commit` route, the landing lock's own lease and compare-and-swap
  machinery, and the history producer under
  `.agents/skills/code-quality-metrics/scripts/`.
- The confirmation contract's post-confirmation behavior, which already
  tolerates a clean identical rebase and stays as written.
- New scripts, new retry mechanisms, or automatic reconciliation added anywhere
  else in the flow.
- Unrelated skills/scripts; any transcript path or transcript text in the repo.

## Risk tier and invariants

Expected Tier 2: scoped workflow-step behavior in
`.agents/skills/finalize-changes/SKILL.md` step 4, with no script change.
Escalation: any change that reaches
`.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1` or the
landing-lock machinery is Tier 3 — stop and re-plan rather than making it under
this Plan. Invariants that must survive:

- Primary advances only under the landing lock, by compare-and-swap.
- Only a byte-identical patch — the same patch digest over the same paths — may
  land without a refreshed confirmation.
- A session patch that touches the reserved history JSONL/SVG paths keeps its
  current blocked meaning.
- No lease is held while waiting on the user.
- Never embed transcript paths or home paths.

## Acceptance criteria

- A finalization whose primary advances by foreign commits disjoint from the
  session's landing paths between verification and confirmation opens the
  review window and asks the confirmation with no pre-confirmation
  re-reconcile round.
- Foreign movement that overlaps the session's landing paths, or is reachable
  from the session's changed code, still blocks at step 4 exactly as today,
  naming both values.
- The landing summary's `## Landing` section states both the candidate parent
  and the newer live tip, and that landing rebases internally and lands only a
  byte-identical patch.
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1` is
  byte-unchanged by this Plan.
- /validate-skill passes for `.agents/skills/finalize-changes/SKILL.md`; plan
  validate exits 0.
