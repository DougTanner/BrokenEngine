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

A second `/next-plan` run, whose claimed Engine Plan was split into stages,
showed the same gap at an intermediate stage landing:

- Stage 1 landed through `/finalize-changes`, rebased from the step-6 baseline
  `7db5da95` onto primary `97fc2976`; the primary-movement check
  (`Invoke-FinalizePrimaryMovementCheck.ps1`) returned
  `primary.disjoint-needs-review`, because it tests only stage 1's owned paths.
- No sync check ran over `7db5da95..97fc2976` before stage 2. `97fc2976` had
  already bumped `ExportScene::kiVersion` 73 to 74
  (`DataPacker/Source/ExportJobs/ExportScene.h`), so the execution card's
  stage-2 "73 to 74" bump was stale.
- The stage-2 implementer found the contradiction mid-implementation, bumped
  74 to 75 on its own, and reported it only as a residual.
  `claim-results.md` `## Final-refresh sync brief` covers only step 6's final
  claim refresh, not a baseline moved by an intermediate stage landing.

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

Second-observation session provenance (machine-local; not reproducible after
cleanup), for the intermediate stage landing above; its fields have the same
meaning as the block above:
- Client: claude
- Conversation session ID: f8d3b92d-a9e2-4bf3-bd98-6a05a1cf0039
- Worktree/branch UUID: 63a744e9-264f-4640-9d1d-449723b1fe0b
- Session branch: claude/63a744e9-264f-4640-9d1d-449723b1fe0b
- Worktree: .claude\worktrees\BrokenEngine\63a744e9-264f-4640-9d1d-449723b1fe0b
- Landing ref: claude/63a744e9-264f-4640-9d1d-449723b1fe0b, whose tip is the
  observing session's final commit and which survives exactly as long as the
  worktree recorded above. Fallback once that ref is gone: the commit that last
  changed this Plan's `## Context`, under the same attributable-to-one-session
  condition as the block above.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <landing ref>` in bounded friction mode, supplying the
recorded client and the recorded conversation session ID of the observation
under review. Then make the
smallest fix inside the `## In scope` boundary below. If root-causing shows the
fix lies outside that boundary, surface it for re-planning instead of expanding
scope.

The author recommends, pending that root cause, that `/next-plan` name one
public route for primary drift detected between step 6 and `/finalize-changes`
— reusing the existing sync `researcher` (`claim-results.md`
`## Sync researcher steps`) over the late range rather than adding a new
mechanism — and that the route state when a change regenerating tracked outputs
must re-sync and rebuild before landing, so main never reads another skill's
private reference to find it. The author recommends the same route also cover
the range an intermediate stage landing absorbs (the step-6 baseline to the
landed stage's rebase target), checked against the remaining stages' card
statements before the next stage's implementation dispatch.

## Critical files
- `.agents/skills/next-plan/SKILL.md`
- `.agents/skills/next-plan/references/claim-results.md`
- `.agents/skills/finalize-changes/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `.agents/skills/next-plan/SKILL.md`
  `## Steps` steps 6 through 10, `.agents/skills/next-plan/references/claim-results.md`
  `## Session baseline and the sync object`, `## Final-refresh sync brief`,
  and `## Sync researcher steps`, and
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
- The second recorded symptom no longer reproduces: after an intermediate stage
  landing rebases onto a newer primary, the absorbed range is checked against
  the remaining stages' card statements before the next stage is implemented
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
