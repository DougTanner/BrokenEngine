# Jev for finding Plans that should be one Plan

Open question: can Jev tell `/create-follow-up-plans` that a new follow-up
duplicates or overlaps an existing Plan, and find existing Plans that describe
the same fix in different places? Part of the series in
`JevDecisionModelWorkflowUses.md`. A pilot has been run against a known merge.

## The decision today

`/create-follow-up-plans` owns duplicate detection: its worker searches all
live Plans "by symbols, paths, root-cause terms, outcome, and `## Coordination`"
and treats a Plan as a duplicate "when it owns the same root cause and
implementation boundary" (`references/worker.md:16`). That search is by
matching terms, so two Plans that describe one defect pattern in different
collections, with different symbols, pass it. Nothing else looks for overlap
between Plans already in the tree. Commit `2f42cc52` shows
the cost: three Plans — `AreaLightsGraphicsRecovery.md`,
`ExplosionParticleGraphicsRecovery.md`, `PointLightsGraphicsRecovery.md` —
described the same defect pattern with the same remedy in three collections and
were replaced by one registry Plan only after a human noticed.

## The question to Jev

One `score` per pair of Plans with three levels, written so the levels are the
three actions the workflow can take, in the shape the TypeSafe entity-alignment
cookbook uses: `0` distinct problems (either order, neither makes the other
redundant); `1` related (same subsystem or mechanism, but each fixes a defect
the other leaves open); `2` same fix (same defect pattern and remedy in
different places, so one Plan covering both would replace them). State is a
JSON object with `plan_a` and `plan_b` as whole files. The instruction says to
judge by the defect and mechanism, not surface wording.

Pairs come from code, not from Jev. For a new follow-up the candidates are every
current Plan in the same area (at most a few dozen calls); for a whole-tree
sweep, a title or path shortlist in code keeps the pair count down, since 58
Plans are 1,653 pairs.

## What still needs a full model

The merge decision and everything it produces. A high score is a residual
naming the other Plan; a session still reads both Plans, decides whether one
Plan covers both, and writes the replacement Plan and the rejections that
follow. Jev never merges, blocks, or authors anything.

## Pilot

Positives: the three pairs among the three Plans commit `2f42cc52` merged, read
from the parent commit. Negatives: each of those three against 12 randomly
chosen current Plans (36 pairs), plus 12 random pairs among current Plans (11
after removing self-pairs). 50 calls, about 3,000 to 3,900 input tokens each.

| pair | score (0 to 2) | confidence |
|---|---|---|
| AreaLights / PointLights | 1.50 | 0.26 |
| AreaLights / ExplosionParticle | 1.19 | 0.67 |
| ExplosionParticle / PointLights | 1.05 | 0.85 |
| highest distinct pair (WeaponModePendingIdentity / NavigationDelayEqualResponse) | 0.99 | 0.86 |
| next distinct pairs (each merged Plan against LightingReblurFenceOrdering) | 0.66 to 0.81 | — |
| remaining 43 distinct pairs | 0.00 to 0.27 | — |

What the numbers say. The three merged pairs are the top three of fifty, and no
distinct pair reached 1.0, so a threshold anywhere in `[1.0, 1.05]` separates
this corpus perfectly. Jev did not call any merged pair "same fix" outright —
its mass sat on "related" for two of them and split evenly for the third — so
the useful output is the ranking and the score, not the level name. The pairs
just below the line are real neighbours (a lighting-recovery Plan against a
lighting fence-ordering Plan), which is what "related" should mean.

Caveats. Three positives is a tiny set, and they share a naming pattern
(`*GraphicsRecovery.md`) that Jev may have read even though the instruction
told it not to; a second labelled merge from history would show whether the
separation survives without that cue. The distinct-pair with the highest score
paired two `Game/` Plans that both touch pending-state identity, and 0.99 may
be a fair "related" rather than a false alarm.

## What would make this a Plan

Success: the score ranks every pair a past merge or rejection joined above
every pair a human confirms as distinct, over at least two historical merges,
with a threshold that keeps false "same fix" flags under one in twenty pairs.
The wiring reports "possible overlap with `<path>` (score N)" as a residual
when a follow-up Plan is authored; it never blocks creation or merges anything.

## Decisions a Plan needs

1. Where it runs: only at `/create-follow-up-plans` authoring time (new Plan
   against its area), or also as a whole-tree sweep in
   `Test-PlanSchedulerState.ps1` with a code-side shortlist.
2. The shortlist rule for the sweep, if any: same area only, shared title
   words, or shared paths in `## Critical files`.
3. The threshold and whether the residual names the level or only the score.
4. The shared decisions in `JevDecisionModelWorkflowUses.md`.
