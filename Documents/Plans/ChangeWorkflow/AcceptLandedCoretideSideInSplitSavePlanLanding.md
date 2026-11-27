<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-27T01:04:46.923Z","dependsOn":[]} -->
# Accept a landed Coretide side in a split /save-plan landing

## Context

A `/save-plan` landing may add Plans on both the engine and the Coretide side.
The landing check `Get-ReviewRoundCoverageFailure` in
`.agents/scripts/SessionChangeInventory.psm1` skips the review rounds when the
supplied sides' diff holds exactly the authorized save paths
(`Test-SavePlanOnlyReviewedRows`, an exact set match). It builds that path set
from every authorized path whose side was supplied, so a supplied Coretide
side always contributes its authorized Coretide paths.

Engine preparation (`Get-LandingState`) always supplies the clone's HEAD as the
Coretide candidate. After the Coretide side has landed and the engine landing
needs recovery, the Coretide diff from the clone's merge base with upstream is
empty, yet the path set still requires the Coretide paths. The set match fails
and the check falls through to the rounds, which a `/save-plan` session never
ran, refusing with `inventory.review-round-missing`; the engine-only landing
check passes the same candidate.

Observed by review on a split-save fixture, changing only the Coretide merge
base to its landed HEAD. The finalize worker's acceptance table already
reconciles already-landed Coretide paths
(`.agents/skills/finalize-changes/references/worker.md` `## Steps` step 5,
the skipped-Steps-1-3 completion sequence), and `Invoke-FinalizeCoretide.ps1`
returns `coretide.already-landed` for a clone HEAD already in its upstream.

## Design

An empty Coretide diff alone cannot drive the exemption: a landed Coretide side
and an unlanded one missing its authorized Plan both have an empty diff, and
the second must still refuse. The requirement: the save-path set drops the
authorized Coretide paths only on evidence that those paths already landed in
the Coretide upstream; without it, the exact match over both sides stands, and
the engine side always matches its own authorized paths exactly.

What that evidence is (for example, each authorized Coretide path present in
the clone's upstream with the bytes the save wrote, or the
`coretide.already-landed` proof `Invoke-FinalizeCoretide.ps1` returns) is not
settled here: the executing session first establishes which evidence the
landing check can read at engine preparation and recovery, then makes the
smallest change to `$sidePaths` that uses it, and surfaces for re-planning any
fix that needs a change outside `## In scope`.

Change Workflow tier: Tier 3 — the landing check's `/save-plan` exemption
starts trusting evidence that the Coretide side landed, a change to what the
landing gate trusts. No C++, shader, build, `.pack`, replay, or
live-verification exposure.

## Critical files

- `.agents/scripts/SessionChangeInventory.psm1`

## In scope

- `Get-ReviewRoundCoverageFailure`: the `$sidePaths` construction for the
  `/save-plan` exemption, and the read of the landed-paths evidence it uses.

## Out of scope

- `Test-SavePlanOnlyReviewedRows` matching rules.
- `Get-LandingState`'s choice of Coretide candidate.
- The Coretide-side message rule, owned by
  `Documents/Plans/ChangeWorkflow/BindLandingCheckCoretideSideToSuppliedCandidate.md`.

## Acceptance criteria

- A split `/save-plan` scratch scenario whose Coretide side has landed passes
  engine preparation's landing check with the engine-side Plan alone.
- The same scenario with an extra unauthorized engine path, and one with an
  unlanded Coretide side whose diff is empty because it is missing its
  authorized Plan, still refuse.
- The static-checks runner passes for the changed files.

## Notes

- `Documents/Plans/ChangeWorkflow/KeepReviewRoundScenarioScripts/check5.ps1`
  holds the split-save scenario setup to start from.
