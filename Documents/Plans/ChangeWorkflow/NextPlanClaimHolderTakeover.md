<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-04T18:22:53.073Z","dependsOn":[]} -->
# Fix: Invoke-NextPlanClaim.ps1 — a Plan claimed by an abandoned session names no holder and has no takeover route

## Context
Observed symptom: during a `/next-plan` run targeting
`Documents/Plans/Engine/StyleGuideSweepEngine.md`,
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1 -Plan 'Documents/Plans/Engine/StyleGuideSweepEngine.md'`
returned `code: none-available` with the message "The requested Plan ... is
claimed by another session." and `claim: null`; nothing named the holding
session, its worktree, or its expiry. `Get-NextPlanList.ps1` projects each row
as `path` and `state` only (plus `blockedBy`/`diagnostic`), so a `claimed` row
also hides the holder. The claim belonged to an abandoned session: its worktree
still existed and was clean, and its work had already landed as `42c379cb`.
Self-healing never releases such a claim, because `HealClaims`
(`Tools/WorktreeCli/PlanScheduler.cpp`) removes only invalid, expired, orphaned
(owning session or worktree gone), or primary-absent records, and this record
was none of those for up to 48 hours.

With the user's explicit instruction to take the Plan over, main had to read
WorktreeCli sources, then run `WorktreeCli.exe plan list` and
`WorktreeCli.exe plan unclaim --worktree <holder> --owner <holder> --session <holder>`
directly by absolute path, outside any documented script.

Evidence in the current tree:
- `Invoke-NextPlanClaim.ps1` `Get-TargetedNoneMessage`, the `$state -ceq 'claimed'`
  line (line 24), returns the fixed sentence and drops the row's `claim` object.
- `Get-NextPlanList.ps1` line 18 builds each projected row from `path` and
  `state` only.
- WorktreeCli `plan list` already emits the holder: the claimed row carries
  `claim` with `session`, `worktree`, and `expiresAt`
  (`PlanScheduler.cpp`, the `row["claim"]` assignment in the list operation),
  as `Tools/WorktreeCli/AGENTS.md` documents. The `/next-plan` context sets
  `Owner` and `Session` to the same session ID (`NextPlanWorkflowCommon.psm1`
  line 44), so `session` plus `worktree` are enough for `plan unclaim`.
- `.agents/skills/next-plan/references/claim-results.md` (the targeted
  `none-available` paragraph) states that "claimed results expose no claim
  identity" — current documented behavior this Plan changes, trusted below in
  favor of the user-approved takeover need recorded in this run.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 0b08a46b-ef82-4a44-a88e-e0397475294e
- Worktree/branch UUID: eae0234f-5ca6-4a98-9443-3a782dcf90e2
- Session branch: claude/eae0234f-5ca6-4a98-9443-3a782dcf90e2
- Worktree: .claude\worktrees\BrokenEngine\eae0234f-5ca6-4a98-9443-3a782dcf90e2
- Landing ref: claude/eae0234f-5ca6-4a98-9443-3a782dcf90e2, whose tip is that
  session's final commit and which survives exactly as long as the worktree
  recorded above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/NextPlanClaimHolderTakeover.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/eae0234f-5ca6-4a98-9443-3a782dcf90e2` in bounded
friction mode, supplying client `claude` and the conversation session ID above.
Then make the smallest fix inside the `## In scope` boundary below. If
root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

Recommended shape (author's recommendation; preparation decides details):
1. Holder visibility, scripts only: carry the WorktreeCli row's existing
   `claim` object (`session`, `worktree`, `expiresAt`) into the targeted
   `none-available` result of `Invoke-NextPlanClaim.ps1` (message and a result
   field) and into each `claimed` row `Get-NextPlanList.ps1` projects. No
   WorktreeCli change is needed, because `plan list` already reports the holder.
   Bump each changed result's `schemaVersion`.
2. Takeover: a documented, user-authorized route in `/next-plan` that releases
   the named holder's claim through `plan unclaim` with that holder's
   `session`/`worktree` and then claims the exact Plan. The author recommends a
   switch on `Invoke-NextPlanClaim.ps1` (for example
   `-UserAuthorizedTakeover`, valid only with an exact `-Plan`) over a new
   script, because the claim script already resolves the listing row; the
   documentation states that only the user's own instruction authorizes it and
   that main reports the holder's worktree state to the user first.

## Critical files
- `.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1`
- `.agents/skills/next-plan/scripts/Get-NextPlanList.ps1`
- `.agents/skills/next-plan/references/claim-results.md`
- `.agents/skills/next-plan/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to: `Get-TargetedNoneMessage` and the
  result envelope in `Invoke-NextPlanClaim.ps1` (plus its parameter block if
  the takeover switch is chosen); the row projection in `Get-NextPlanList.ps1`;
  the targeted `none-available` paragraph and result descriptions in
  `claim-results.md`; the claim step and `## Rules` text in `/next-plan`
  `SKILL.md` that documents the takeover route

## Out of scope
- The landed change the session produced (the Engine style sweep stage)
- `Tools/WorktreeCli/` sources, claim lifetime, and `HealClaims` healing rules
- Any automatic (non-user-authorized) release of another session's live claim
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Change Workflow Tier 2. Trigger: scoped tool behavior of the `/next-plan` claim
scripts. Escalate to Tier 3 if the fix reaches `Tools/WorktreeCli/` (AgentTools
promotion is build/bootstrap coordination that can block other sessions).
Invariants: a live claim of another session is never released without the
user's explicit instruction; one claim per session. Never embed transcript
paths or home paths.

## Acceptance criteria
- A targeted claim of a Plan claimed by another session returns the holder's
  session, worktree, and expiry; a `Get-NextPlanList.ps1` `claimed` row carries
  the same fields
- The documented takeover route releases the holder's claim and claims the
  Plan without any direct `WorktreeCli.exe` invocation
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
