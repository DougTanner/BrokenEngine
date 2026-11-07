<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-07T15:21:01.926Z","dependsOn":[]} -->
# Fix: /next-plan — primary drift found after the final claim refresh has no route

## Context
Observed symptom, from one `/next-plan` run whose claimed Engine Plan changed
texture encoding and regenerated tracked texture outputs:

- Step 6's final claim refresh,
  `pwsh -NoProfile -File .agents/skills/next-plan/scripts/Invoke-NextPlanClaim.ps1 -Plan '<claimed Plan path>'`,
  returned `reused` with no `sync` object about 7 seconds before primary
  advanced to `fef6e571`. Step 6 (`.agents/skills/next-plan/SKILL.md` `## Steps`)
  and `.agents/skills/next-plan/references/claim-results.md`
  `## Final-refresh sync brief` route drift only through that refresh's `sync`
  range, so nothing re-checked primary afterwards.
- Eight primary commits landed during implementation (`fef6e571` through
  `65d3b615`). `65d3b615` changed a source input,
  `Engine/Data/Models/SeaCreature2/textures/defaultMat_normal.png`, and its
  tracked output `scene.gltf.Texture2.BC5_UNORM_BLOCK`, an output the run's
  change also regenerates. The drift surfaced only at the adversarial review.
- `/next-plan` has no instruction for primary drift found after step 6 and
  before `/finalize-changes`. Main improvised the route: a search of
  `claim-results.md`, a search and read of `.agents/skills/finalize-changes/SKILL.md`,
  an empty search for a sync script, then a read of
  `.agents/skills/finalize-changes/references/worker.md`, which is private to
  the session executing that skill (a `researcher` dispatch could have returned
  the route as one handoff row). It then restored the changed source input from
  primary with `git checkout main -- <path>` and ran an extra resolve, rebuild,
  and re-review cycle.
- A change that regenerates tracked outputs from sources cannot rely on the
  landing rebase: `.agents/skills/finalize-changes/SKILL.md`
  `### Landing confirmation` lets a clean rebase land without rebuild, and a
  conflict in a binary generated output has no resolution short of a rebuild
  from the rebased sources.

Root cause is not confirmed; the missing route is the observed symptom.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: fa735a45-0504-4cb8-84e6-cd8493db81ce
- Worktree/branch UUID: 368c7c75-c9f8-4760-a509-68af8e9ceae0
- Session branch: claude/368c7c75-c9f8-4760-a509-68af8e9ceae0
- Worktree: .claude\worktrees\BrokenEngine\368c7c75-c9f8-4760-a509-68af8e9ceae0
- Landing ref: claude/368c7c75-c9f8-4760-a509-68af8e9ceae0, whose tip is the
  observing session's final commit and which survives exactly as long as the
  worktree recorded above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <landing ref>` in bounded friction mode, supplying the
recorded client and the recorded conversation session ID. Then make the
smallest fix inside the `## In scope` boundary below. If root-causing shows the
fix lies outside that boundary, surface it for re-planning instead of expanding
scope.

The author recommends, pending that root cause, that `/next-plan` name one
public route for primary drift detected between step 6 and `/finalize-changes`
— reusing the existing sync `researcher` (`claim-results.md`
`## Sync researcher steps`) over the late range rather than adding a new
mechanism — and that the route state when a change regenerating tracked outputs
must re-sync and rebuild before landing, so main never reads another skill's
private reference to find it.

## Critical files
- `.agents/skills/next-plan/SKILL.md`
- `.agents/skills/next-plan/references/claim-results.md`
- `.agents/skills/finalize-changes/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `.agents/skills/next-plan/SKILL.md`
  `## Steps` steps 6 through 10, `.agents/skills/next-plan/references/claim-results.md`
  `## Final-refresh sync brief` and `## Sync researcher steps`, and
  `.agents/skills/finalize-changes/SKILL.md` `### Landing confirmation`

## Out of scope
- The landed change the session produced
- `.agents/skills/finalize-changes/references/worker.md` and the landing scripts
- `Invoke-NextPlanClaim.ps1` and WorktreeCli claim behavior
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 2, trigger: scoped tool behavior of one subsystem, the `/next-plan`
workflow (`.agents/references/risk-tiers.md` `### Risk tiers`). Escalate if the
fix reaches build/bootstrap coordination. Never embed transcript paths or home
paths.

## Acceptance criteria
- The recorded symptom no longer reproduces: `/next-plan` publicly names the
  route for primary drift found after step 6 and before `/finalize-changes`,
  including the rebuild case for changes that regenerate tracked outputs, so
  main reaches it without reading a skill-private reference
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
