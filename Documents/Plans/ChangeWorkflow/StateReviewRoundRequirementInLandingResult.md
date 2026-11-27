<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-27T01:04:49.476Z","dependsOn":[]} -->
# State the review-round requirement in the landing result

## Context

When the landing check refuses a candidate, both landing scripts return
disposition `recovery` for an `inventory.review-round-*` code, the same
disposition a rebase refusal returns:
`.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1`
`Assert-LandingReviewRoundCoverage` and
`.agents/skills/finalize-changes/scripts/Invoke-FinalizeCoretide.ps1` (the
`Test-ReviewRoundCoverage` refusal before the fast-forward).

The instructions then tell the two cases apart by the code prefix: the
`### Landing confirmation` paragraph of
`.agents/skills/finalize-changes/SKILL.md` ("unless landing refuses it with an
`inventory.review-round-*` code") and two bullets of
`.agents/skills/finalize-changes/references/worker.md` `## Recovery` ("except
after an `inventory.review-round-*` refusal" and "After an
`inventory.review-round-*` refusal from either script, return once the
rebased bytes are reproduced"). A progressive-disclosure review classed this
as misplacement: a program decides whether a review round is owed, so the
result should state it and both instructions route on that field.

The routing on the code prefix is also incomplete. The worker's `## Recovery`
bullet for the overlap-recovery rebase, at its "Exit 0 (clean):" branch
(`.agents/skills/finalize-changes/references/worker.md:346`), sends the worker
on to re-run step 3 and step 4 (about :361) and names no
`inventory.review-round-*` exception, while the bullet above it (:311) and
`SKILL.md` `### Landing confirmation` (:197) require returning before the
acceptance table so main can run the owed round. An adversarial review
reproduced the reach on a scratch fixture: a rebase that kept the session
patch byte-identical on a file primary also changed was refused with
`inventory.review-round-unfinished`, and the clean branch then led into step 3.

## Design

Recommended: the landing result states that a review round is required before
landing resumes (one new disposition value, or one field beside `disposition`;
the executing session picks the smaller), and the instructions route on it
instead of the code prefix. Bump the result's schema version per the root
`AGENTS.md` format rule and reject the old one.

Change Workflow tier: Tier 2 — scoped tool behavior: one result field of the
two finalizer scripts and the prose that reads it. No determinism, wire, or
trust-boundary change; no C++, shader, build, `.pack`, replay, or
live-verification exposure.

## Critical files

- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1`
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeCoretide.ps1`
- `.agents/skills/finalize-changes/SKILL.md`
- `.agents/skills/finalize-changes/references/worker.md`
- `.agents/skills/finalize-changes/references/scripts.md`

## In scope

- `Assert-LandingReviewRoundCoverage` and its result emission in
  `Invoke-FinalizeLanding.ps1`; the coverage refusal in
  `Invoke-FinalizeCoretide.ps1`; each script's result schema version.
- The three instruction sentences quoted in `## Context`, the
  overlap-recovery "Exit 0 (clean):" branch of `worker.md` `## Recovery`, and
  the `scripts.md` description of the result fields.
- Any other reader of the two result schemas that the change would break.

## Out of scope

- The landing check's decision (`Test-ReviewRoundCoverage`).
- Sweep recovery, owned by
  `Documents/Plans/ChangeWorkflow/RecoverSweepAfterLandingRefusesItsCandidate.md`.
- Other dispositions and codes.

## Acceptance criteria

- A review-round refusal and a rebase refusal from each landing script return
  distinguishable values in the result, and neither instruction names a code
  prefix to tell them apart.
- Every `worker.md` `## Recovery` branch reachable after a review-round
  refusal, the overlap-recovery clean branch included, returns before the
  acceptance table instead of entering step 3.
- `Validate-Skill.ps1` returns `VALID` for `finalize-changes`.
- The static-checks runner passes for the changed files.
