<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-15T16:10:12.110Z","dependsOn":[]} -->
# Fix: next-plan / external-grill-plan — the required claim result never reaches the preparation brief

## Context
`.agents/skills/external-grill-plan/SKILL.md:129-140` ("Plan Context") requires
the preparation `implementer`'s brief to carry the passing claim result from
`.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1` — exit `0`,
top-level `status: pass`, nested `validation.status: valid` and
`validation.code: ok` — and states that missing or non-passing evidence blocks
the skill. The same section forbids the worker from running that validation
itself, and the claim script is mutation-capable
(`.agents/skills/next-plan/SKILL.md:42-68`).

Nothing in `.agents/skills/next-plan/SKILL.md` tells the manager to forward that
claim result into the preparation dispatch: its preparation section
(`:70-79`) names the execution card and the immutability rule only, and the
string "brief" appears there just as the pointer to the shared task brief
(`:87`).

In this session the preparation worker therefore had no claim result to cite. It
substituted a read-only
`pwsh -NoProfile -File .agents/skills/next-plan/scripts/Get-NextPlanList.ps1`
run for the claim evidence and returned a formal residual, and the manager had
to supply the original claim result afterwards — an extra round trip on every
Tier-3 `/next-plan` run.

The claimed active intent was `Documents/Plans/Frame/TerrainTraceToEngine.md`,
whose `## In scope` covered only moving `SegmentHit`,
`TracePointAgainstTerrain`, and `TracePointToFrameExit` and requalifying their
call sites; both skill files are outside that boundary, so this is `/next-plan`
tooling friction rather than an in-scope failure of the active change. The exact
root cause is intentionally deferred to `/next-plan-review`.

Session provenance (machine-local; not reproducible after cleanup):
- Client: claude
- Conversation session ID: ccaa8c39-19a7-4b6c-b2b6-3b6fb1bb0de1
- Worktree/branch UUID: ec7d0a29-fe8c-4d31-b355-17406342ca2e
- Session branch: claude/ec7d0a29-fe8c-4d31-b355-17406342ca2e
- Worktree: .claude\worktrees\BrokenEngine\ec7d0a29-fe8c-4d31-b355-17406342ca2e
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
In a new session, run `/next-plan-review <landing ref>` supplying the recorded
client and the recorded conversation session ID; a Codex review supplies the
client and landing ref only. Root-cause the friction from the proven transcript,
then make the smallest fix inside the `## In scope` boundary below: close the
gap in one place, either by directing the `/next-plan` manager to include the
claim script's own result in the preparation dispatch, or by making
`/external-grill-plan` accept the read-only `Get-NextPlanList.ps1` listing as
the standard scheduler evidence. Whichever side is chosen, the two skills must
end up naming the same evidence, and the worker must still never run the
mutation-capable claim script. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

## Critical files
- `.agents/skills/next-plan/SKILL.md` — claim invocation (`:42-68`) and
  preparation dispatch (`:70-79`)
- `.agents/skills/external-grill-plan/SKILL.md` — `## Plan Context` required
  evidence (`:129-140`)

## In scope
- Root-cause investigation via /next-plan-review, run with the recorded landing
  ref and client, plus, on Claude, the recorded conversation session ID; a Codex
  review supplies the client and landing ref only
- The smallest resulting fix, confined to the two SKILL.md sections named above

## Out of scope
- The landed change the session produced
- `Invoke-NextPlanClaim.ps1`, `Get-NextPlanList.ps1`, and any other bundled
  script behavior, JSON shape, or exit mapping
- Claim lifecycle, scheduler state, and WorktreeCli
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior); escalate if the fix reaches claim
lifecycle or build/bootstrap coordination. A preparation worker must still never
run a mutation-capable scheduler command, and non-passing scheduler evidence
must still block `/external-grill-plan`. Never embed transcript paths or home
paths.

## Acceptance criteria
- Following `/next-plan` as written produces a preparation brief that already
  carries exactly the scheduler evidence `/external-grill-plan` requires, with
  no manager round trip and no worker-run mutation-capable command
- /validate-skill passes for both changed SKILL.md files; plan validate exits 0

## Notes
This Plan is keyed to the pair (`/next-plan` preparation dispatch, required
`Invoke-NextPlanClaim.ps1` result absent from the `/external-grill-plan`
preparation brief). A later observation of the same pair is a duplicate, not a
new residual. This body records the observed gap and the forced substitution and
round trip without embedding transcript material.
