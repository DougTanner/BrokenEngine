<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-15T16:27:48.174Z","dependsOn":[]} -->
# Fix: external-grill-plan Plan Context — preparation worker cannot produce the required claim receipt

## Context
During the Tier-3 preparation for `Documents/Plans/Network/OwnedEntityRegistryToEngine.md`,
the preparation `implementer` had to assemble the `/external-grill-plan` decision
brief but could not satisfy that skill's Plan Context evidence requirement, and
returned the gap as a residual for the manager to fill from its own earlier
`/next-plan` claim run (grill brief, Residuals item 1). The two contracts
disagree as written:

- `.agents/skills/external-grill-plan/SKILL.md:131-140` requires the brief to
  carry the passing claim result from
  `.agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1` — exit `0`,
  top-level `status: pass`, nested `validation.status: valid` and
  `validation.code: ok` — plus that same result's `validation.notices`, and it
  states "Missing or non-passing evidence blocks this skill" while also
  forbidding the reader from running validation itself or "a command that
  changes scheduler state".
- `.agents/skills/next-plan/references/tier3-workflow.md:28-37` assigns the
  preparation `implementer` "every repository read, search, and WorktreeCli
  validation `/external-grill-plan` requires", and states that main "performs no
  repository read, search, or WorktreeCli work".

`Invoke-NextPlanClaim.ps1` claims a Plan, so it changes scheduler state. Only
main runs it, once, at the start of `/next-plan`; the preparation worker is
barred from re-running it merely to reproduce the output, so the one artifact the
grill skill declares blocking is the one artifact the preparation worker can
never produce. No rework resulted in this session — the manager supplied the
receipt from its own earlier run — but the written contract has no route that
satisfies it.

The claimed Plan's `## In scope` covered only the `OwnedEntity` registry move
into `engine::OwnedEntityRegistry` and its call sites, so both skill files are
outside the active change's boundary.

Session provenance (machine-local; not reproducible after cleanup):
- Client: claude
- Conversation session ID: 453f8b44-3b01-43d5-940c-c8364bdfb854
- Worktree/branch UUID: cb6e15ef-fb81-4c19-957d-fd0ecbfeb5a5
- Session branch: claude/cb6e15ef-fb81-4c19-957d-fd0ecbfeb5a5
- Worktree: .claude\worktrees\BrokenEngine\cb6e15ef-fb81-4c19-957d-fd0ecbfeb5a5
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
client and the recorded conversation session ID, root-cause the friction from the
proven transcript, then make the smallest fix inside the `## In scope` boundary
below. The fix direction the observation already points at is to make the claim
receipt an explicitly manager-supplied hand-off: `/external-grill-plan`'s
`## Plan Context` accepts the claim result as evidence relayed into the brief by
whoever holds it, and `tier3-workflow.md` states that hand-off so the preparation
worker's assignment no longer implies producing it. The fix chooses one owning
side rather than restating the requirement in both. If root-causing shows the fix
lies outside that boundary, surface it for re-planning instead of expanding
scope.

## Critical files
- `.agents/skills/external-grill-plan/SKILL.md` — `## Plan Context` (`:129-145`),
  the blocking evidence requirement and its self-service prohibition.
- `.agents/skills/next-plan/references/tier3-workflow.md` — `## Plan review`
  (`:25-40`), the preparation worker's evidence assignment and main's
  no-repository-work rule.

## In scope
- Root-cause investigation via /next-plan-review, run with the recorded landing
  ref, client, and conversation session ID
- The smallest resulting fix, confined to the `## Plan Context` section of
  `.agents/skills/external-grill-plan/SKILL.md` and the `## Plan review` section
  of `.agents/skills/next-plan/references/tier3-workflow.md`

## Out of scope
- The landed change the session produced
- `Invoke-NextPlanClaim.ps1`, `Get-NextPlanList.ps1`, `NextPlanWorkflowCommon.psm1`,
  and every other scheduler script — no script behavior or output shape changes
- The claim lifecycle itself, the prohibition on subagents changing scheduler
  state, and the rule that machine-local claims are never read
- The rest of `/external-grill-plan` (interview format, library gate, closing
  checks) and the rest of the Tier-3 route
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior); escalate if the fix reaches
build/bootstrap coordination or the scheduler claim lifecycle. A subagent must
still never run a scheduler-mutating command, the grill must still refuse to
proceed on missing or non-passing claim evidence, and the evidence relayed into
the brief must remain the verbatim passing result rather than a worker's summary.
Never embed transcript paths or home paths.

## Acceptance criteria
- Reading the two files in sequence yields exactly one route by which the
  preparation worker's brief carries the passing claim receipt, with no
  instruction that requires the worker to run a scheduler-mutating command
- The grill still blocks on missing or non-passing claim evidence
- /validate-skill passes for the changed SKILL.md; plan validate exits 0

## Notes
This Plan is keyed to the pair (external-grill-plan `## Plan Context`,
preparation worker cannot produce the required `Invoke-NextPlanClaim.ps1`
receipt). A later observation of the same pair is a duplicate, not a new
residual.
