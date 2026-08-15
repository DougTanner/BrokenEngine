<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-15T14:09:00.870Z","dependsOn":[]} -->
# Fix: finalize-changes landing sanity gate — transient primary-dirty blocks landing after its own approval review

## Context
During the `/finalize-changes` landing of
`Documents/Plans/Agents/FrictionPlanTemplateProvenanceFields.md`, the first
invocation of
`.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1` returned
`blocked` with code `sanity.git.primary-dirty` and `lock.claimed` `false`: it
stopped at the pre-landing sanity gate without touching primary or taking the
landing lock. The primary checkout was actually clean. The gate's own exact
check, `git status --porcelain -z --untracked-files=all`, run twice immediately
afterwards against that same checkout, returned zero bytes both times, with
primary still at the expected tip
`1229eabbe453385d8deace2523dd675f20812955`.

As observed, the false dirty reading occurred moments after
`.agents/skills/finalize-changes/scripts/Show-FinalizeApprovalReview.ps1
-LaunchSmartGit` opened the SmartGit approval-review window (`--log` anchored on
the candidate) on that same primary checkout. That is a coincidence in timing
only; the likely but unproven trigger is SmartGit's background index refresh
briefly disturbing the index. An unchanged idempotent re-invocation under the
same still-live owner token then landed cleanly (`status` `landed`,
`rebaseAttempts` `0`).

So a transient dirty reading can trip the landing sanity gate one step after
the workflow's own approval-review step, producing a result that contradicts
the checkout's real state and forcing a repeated landing invocation. The dirty
check is at `.agents/scripts/FinalizeWorkflowCommon.psm1:237-239`; the
post-lock counterpart is at
`.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1:237-239`.

The claimed active intent was
`Documents/Plans/Agents/FrictionPlanTemplateProvenanceFields.md`, whose
`## In scope` covered only the create-follow-up-plans friction Plan template's
provenance fields; the finalize-changes scripts and their shared sanity module
were outside that boundary.

Session provenance (machine-local; not reproducible after cleanup):
- Client: claude
- Conversation session ID: d24b91c7-dc21-4771-a7b9-1806cff0a0d6
- Worktree/branch UUID: 2eec1675-bfcb-4084-9e33-f4508894eb55
- Session branch: claude/2eec1675-bfcb-4084-9e33-f4508894eb55
- Worktree: .claude\worktrees\BrokenEngine\2eec1675-bfcb-4084-9e33-f4508894eb55
- Landing ref: the session branch above, whose tip is the session's final
  commit and which survives exactly as long as the worktree recorded above.
  Fallback once that branch is gone:
  `git log --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to this session alone
  (its diff limited to this session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes this worktree: Codex
  transcript discovery requires the producing worktree to remain registered,
  and Claude review requires the exact conversation session ID above.

## Design
The landing sanity gate's primary dirty check performs a small bounded retry
before returning `sanity.git.primary-dirty`. When its
`git status --porcelain -z --untracked-files=all` run reports a dirty primary,
it re-runs that exact same check up to 3 more times, waiting 1 second between
attempts, and blocks only if the primary still reads dirty on the last attempt;
a clean reading at any attempt lets the gate proceed. The retry is keyed on the
symptom — a dirty reading that does not persist — not on SmartGit or any other
specific process, and is confined to the finalize-changes bundled scripts named
under `## Critical files`.

`/next-plan-review <landing ref>`, run with the recorded client and, on Claude,
the recorded conversation session ID, serves only to confirm the recorded
symptom from the session's own evidence. It does not select the fix and does
not re-open it.

## Critical files
- `.agents/scripts/FinalizeWorkflowCommon.psm1` — the pre-landing sanity gate's
  primary dirty check (`:237-239`).
- `.agents/skills/finalize-changes/SKILL.md` — the landing blocked-result and
  retry guidance for `sanity.git.primary-dirty`, which the retry changes.

## In scope
- Symptom confirmation via /next-plan-review, run with the recorded landing ref
  and client, plus, on Claude, the recorded conversation session ID; a Codex
  review supplies the client and landing ref only
- The bounded retry decided in `## Design`, confined to the primary dirty check
  in the finalize sanity gate named above and the matching finalize skill
  documentation

## Out of scope
- The landed change the session produced
- SmartGit's own behavior, the approval-review script, landing-lock, primary
  advance, rollback, and claim-release logic
- The session dirty check, which must keep failing immediately on real leftover
  session state
- The post-lock primary dirty check in
  `.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1:237-239`,
  which stays unchanged and keeps no retry: it was never observed failing
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior); escalate if the fix reaches
build/bootstrap coordination or landing-lock and primary-advance safety. A
genuinely dirty primary must still block landing, no retry may run while the
landing lock is held any longer than the check itself, and the gate must remain
side-effect free when it blocks. Never embed transcript paths or home paths.

## Acceptance criteria
- A transient dirty reading proceeds instead of blocking: with primary made
  dirty for the first check and clean again within the retry bound — for
  example a temporary file created in the primary checkout and removed during
  the retry window, or an equivalent deterministic setup — the landing run does
  not return `sanity.git.primary-dirty`
- A primary checkout left dirty for the whole retry window by real uncommitted
  or untracked content still blocks with `sanity.git.primary-dirty` and
  `lock.claimed` `false`
- /validate-skill passes for any changed SKILL.md; plan validate exits 0

## Notes
This Plan is keyed to the pair (finalize landing sanity gate, false
`sanity.git.primary-dirty` while primary is clean). A later observation of the
same pair is a duplicate, not a new residual. This body records the observed
symptom and the forced repeat invocation without embedding transcript material.
