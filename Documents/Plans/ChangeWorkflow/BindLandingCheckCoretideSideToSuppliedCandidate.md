<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-27T01:04:44.789Z","dependsOn":[],"agentCli":"claude"} -->
# Bind the landing check's Coretide side to the supplied candidate

## Context

`Get-SessionChangeInventory` in `.agents/scripts/SessionChangeInventory.psm1`
reads the `Coretide-Snapshot` and `Coretide-Baseline` lines from the message
of any commit-valued `-Head` (`Get-InventoryRoundField $headSha`) and, when
present, uses them as the Coretide side's head and baseline instead of the
clone's HEAD and its merge base with upstream (module header and the
`$headFields` use in the Coretide block). That rule exists for review-round
snapshots, whose messages carry those lines.

The landing check `Get-ReviewRoundCoverageFailure` verifies the supplied
`-CoretideCandidate` is the clone's HEAD, then evaluates the rounds through
`Get-ReviewRoundState -Head $engineHead`, which inventories the engine
candidate through `Get-ReviewRoundInventory`. If the engine landing commit's
message carries those two lines, the evaluation reads the Coretide side from
the commit they name rather than from the supplied candidate. The landing
commit message comes from the oldest session commit or from
`Invoke-FinalizeApprovalPreparation.ps1 -CommitMessageFile`.

Observed by review on a two-repository fixture: changing only the engine
candidate's message turned an `inventory.review-round-unfinished` refusal into
a pass, the check evaluating an older Coretide commit than the supplied one.
Reachable only when a landing commit message contains those lines.

## Design

Recommended: the landing check evaluates the Coretide side at the supplied
Coretide candidate and its merge base with upstream, by passing that side
explicitly from `Get-ReviewRoundCoverageFailure` into the evaluation, so the
landing candidate's message lines are never read for it.

Constraint: provisional snapshot reads must keep working. `-StartRound` in
`.agents/scripts/Get-ChangeWorkflowSteps.ps1` (the `if ($StartRound)` block)
freezes the Coretide clone first (`New-ReviewRoundCoretideSnapshot`), then
plans the round from a provisional engine commit whose message carries that
frozen Coretide snapshot, before either `refs/worktree/review-rounds` ref
advances. The provisional commit is on neither chain yet, and its Coretide
snapshot holds the clone's uncommitted changes. A rule that honors the message
lines only for a commit on the engine chain would read the clone's unchanged
HEAD there instead, hide Coretide-only working-tree edits, and could report
that no round is owed, so the author recommends against it.

Change Workflow tier: Tier 3 — changes what the landing check trusts
(a trust boundary of the landing gate) across the engine and Coretide sides.
No C++, shader, build, `.pack`, replay, or live-verification exposure.

## Critical files

- `.agents/scripts/SessionChangeInventory.psm1`

## In scope

- `Get-SessionChangeInventory`: the `$headFields` read and its use for the
  Coretide head and baseline.
- `Get-ReviewRoundCoverageFailure`, and the `Get-ReviewRoundState` and
  `Get-ReviewRoundInventory` parameters that carry the Coretide side.
- The module header sentence describing the `Coretide-Snapshot` rule.

## Out of scope

- The review-round message format in the `Get-ChangeWorkflowSteps.ps1` header.
- Commit message validation in the finalizer scripts.
- The split `/save-plan` path check, owned by
  `Documents/Plans/ChangeWorkflow/AcceptLandedCoretideSideInSplitSavePlanLanding.md`.

## Acceptance criteria

- In a two-repository scratch scenario with a closed round, an engine landing
  candidate whose message carries `Coretide-Snapshot`/`Coretide-Baseline` lines
  naming an older Coretide commit gets the same landing-check result as the
  same candidate without those lines.
- Review-round snapshots still read their recorded Coretide side: a closed
  round followed by a Coretide rebase onto upstream owes no review.
- Provisional snapshot reads still work: with a closed engine round and
  Coretide-only edits left uncommitted in the clone, `-StartRound` freezes a
  round that owes the review those edits trigger.
- The static-checks runner passes for the changed files.

## Notes

- `.agents/scripts/ReviewRoundScenarios/RoundSequence.ps1` holds the
  two-repository scenario setup to start from.
