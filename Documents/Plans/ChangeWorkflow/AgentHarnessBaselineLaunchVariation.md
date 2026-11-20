<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-20T00:35:12.183Z","dependsOn":["Documents/Plans/Engine/PinWaterAnimationForHarnessCaptures.md"]} -->
# Fix: agent-harness worker.md — two baseline captures miss launch-to-launch variation

## Context
The before/after pixel baseline paragraph in
`.agents/skills/agent-harness/references/worker.md` `## Rules` (lines 874-896
at the recording baseline) has every capture replay the full recipe on its own
fresh client launch, takes two baseline captures, and judges the after-change
capture only where the two baselines agree within the check's pixel threshold.
Every asserted region must lie inside that agreement area, or its criterion
stays unsettled.

A `/next-plan` run used this recipe as a live acceptance check on a
BrokenEngineSandbox client and server, and the recipe failed it twice:

- The two baselines agreed on 99.47% of pixels, so their agreement area looked
  stable. The first after-change capture still differed on 28.10% of judged
  pixels. Further fresh-launch captures of one build showed why: at threshold
  8, captures A11 and A12 agreed with A10 only 0.740 and 0.735, while A13 and
  A14 agreed with baseline B1 at 0.9999. Launch-to-launch variation is large
  but not present in every launch, so two baselines that happen to agree do not
  show it, and the agreement area the rule builds from them admits pixels that
  vary between launches.
- Five of the eight asserted regions differed by 1 to 109 pixels between
  baselines B1 and B2. The rule requires the whole region to lie inside the
  agreement area, so those regions could never pass, however small the
  difference.

The run worked around both: the user approved two amendments to the check (a
per-region tolerance, then a best-of-three rule), an `/external-diagnose-bug`
dispatch took four extra captures, and one more user decision round followed.
The supplied evidence records no harness setup beyond the recipe itself; the
save, camera, and pixel bounds of the regions are not recorded.

Related Plans, neither a duplicate:
`Documents/Plans/Engine/PinWaterAnimationForHarnessCaptures.md` pins the water
inputs the run diagnosed as the source of the launch-to-launch difference, and
keeps the two-baseline agreement area for any input still unpinned;
`Documents/Plans/ChangeWorkflow/AgentHarnessPausedScreenshotComparison.md`
covers two captures within one launch differing. This Plan covers what neither
does: the recipe's two baselines cannot detect a variation that some launches
show and others do not, and the whole-region rule fails regions that differ
between baselines by a few pixels.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 4c7b61a4-ec03-4553-9ab1-e23cc3ac71f1
- Worktree/branch UUID: 2c24c0a5-8dd2-4c11-9afc-991d104208bc
- Session branch: claude/2c24c0a5-8dd2-4c11-9afc-991d104208bc
- Worktree: .claude\worktrees\BrokenEngine\2c24c0a5-8dd2-4c11-9afc-991d104208bc
- Landing ref: claude/2c24c0a5-8dd2-4c11-9afc-991d104208bc
  The review ref the invoker passes to /next-plan-review has two cases. When
  the `Observed in an earlier session` line is present, it is that line's
  landed commit hash, with no lookup. When that line is absent, the recording
  session is the observing session, and worktree reuse moves its branch past
  its landing, so the invoker never reviews the ref itself: it resolves the ref
  to the commit that added this Plan with
  `git log --follow --diff-filter=A -n 1 --format=%H <ref> -- <this plan path>`
  and passes that commit. When the recorded ref is gone or that command prints
  nothing (a later session moved this Plan), it runs the same command with
  `<ref>` omitted. A periodic Plan-history squash can make either command
  return an unrelated aggregate commit, and a landing that also deletes a
  similar file can make it return another session's commit, so review the
  result only when the commit is attributable to the observing session recorded
  above alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
This Plan depends on `PinWaterAnimationForHarnessCaptures.md`, because how much
launch-to-launch variation remains after water pinning decides how much the
recipe must change, and both Plans edit the same paragraph.

First root-cause the friction from the current tree and this Plan's
`## Context`: with the water pins in place, take repeated same-build
fresh-launch captures through the recipe and measure the remaining
launch-to-launch variation and the per-region baseline differences. Only when
the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode, with the review ref
the provenance block above defines, supplying the recorded client and the
recorded conversation session ID. Then make the smallest fix inside the
`## In scope` boundary below. If root-causing shows the fix lies outside that
boundary, such as a new harness command or a comparison script, surface it for
re-planning instead of expanding scope.

The fix is chosen from that measurement; the author's recommendation, not a
binding choice:

- If variation remains that two baselines can miss, require enough baseline
  launches for the agreement area to sample it (agreement taken across every
  baseline pair), recorded with the recipe like the sleep duration.
- Replace the whole-region rule with a per-region excluded-pixel allowance
  recorded with the recipe, so a region differing between baselines by a few
  pixels is judged on its agreeing pixels and reports the excluded count,
  instead of leaving the criterion unsettled.
- If the measurement shows no remaining variation and no region differences,
  the fix is only the region rule, or none, recorded as such.

## Critical files
- `.agents/skills/agent-harness/references/worker.md` (`## Rules`, the
  before/after pixel baseline paragraph)

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the before/after pixel baseline
  paragraph of `worker.md` `## Rules`

## Out of scope
- The landed change the session produced
- The water pinning and its recipe steps, which
  `Documents/Plans/Engine/PinWaterAnimationForHarnessCaptures.md` owns
- The within-launch paused-capture difference, which
  `Documents/Plans/ChangeWorkflow/AgentHarnessPausedScreenshotComparison.md`
  owns
- Engine, game, and AgentHarness C++, new harness commands, and new scripts
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: one live-verification rule); escalate
if the fix reaches build/bootstrap coordination. Never embed transcript paths
or home paths. Screenshots are client visuals outside the simulation CRC.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation:
  live, via `/agent-harness` on a BrokenEngineSandbox client and server, at
  least four fresh-launch captures of one unchanged build, judged under the
  revised rule as baselines plus an after-change capture, settle every asserted
  region as `PASS` without any check amendment, and the run records its scene,
  camera, and region setup with the result
- The revised rule still leaves a region unsettled when it differs between
  baselines beyond the recorded allowance
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Coordination
No mandatory nondirectional coordination; the ordering with
`PinWaterAnimationForHarnessCaptures.md` is the metadata dependency.
