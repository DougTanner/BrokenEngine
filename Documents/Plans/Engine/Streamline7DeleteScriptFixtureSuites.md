<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-01T19:32:41.984Z","dependsOn":[]} -->
# Streamline 7: Delete the PowerShell fixture suites for workflow scripts

## Context

Twenty `Test-*.ps1` scripts live under `.agents`. Eight of them are
validators that check an engine or project invariant on demand
(`Test-CollectionLayout.ps1` and its fixtures, `Test-VcxprojPair.ps1`,
`Test-PlanSchedulerState.ps1`, `Test-DataPackerMaterializeData.ps1`,
`Test-AgentToolsCapabilities.ps1`, the two agent-harness readiness checks) or
a lookup tool (`plan-audit/scripts/Test-PlanCitations.ps1`). The other eleven
are unit-test suites for the workflow's own PowerShell scripts, built
against scratch repositories and asserting on JSON result shapes:

| Suite | Lines | Owning prose that requires running it |
| --- | --- | --- |
| `.agents/skills/finalize-changes/scripts/Test-FinalizeWorkflowFixtures.ps1` | 2170 | `finalize-changes/references/scripts.md` `## Fixture suites` (line 372 on) |
| `.agents/skills/finalize-changes/scripts/Test-LandingLockStatusFixtures.ps1` | 383 | same section |
| `.agents/skills/finalize-changes/scripts/Test-AgentToolsPromotionFixtures.ps1` | 268 | same section |
| `.agents/skills/codex-review/scripts/Test-CodexReviewPromptFixtures.ps1` | 1166 | none |
| `.agents/scripts/Test-SessionChangeInventoryFixtures.ps1` | 1041 | `repo-code-review/SKILL.md:53-56` |
| `.agents/skills/next-plan-review/scripts/Test-Find-AgentSessionTranscript.ps1` | 377 | none |
| `.agents/scripts/Test-WorktreeCliPlanScheduler.ps1` | 299 | none |
| `.agents/skills/code-quality-metrics/scripts/Test-CodeQualityMetricsHistoryFixtures.ps1` | 306 | none |
| `.agents/skills/next-plan/scripts/Test-NextPlanWorkflowScripts.ps1` | 204 | `next-plan/SKILL.md:159-165` |
| `.agents/skills/compile/scripts/Test-BuildResultFixtures.ps1` | 201 | none |
| `.agents/skills/context-efficiency-review/scripts/Test-MeasureSessionContext.ps1` | 147 | deleted by Streamline 2 if that lands first |

Roughly 6,500 lines. The repository directive says "Do not add unit tests"
(`AGENTS.md:102`); none of these was requested by the user. Their cost is
real: any change to a landing or review script obliges an agent to read and
update a fixture file larger than the script (224 KB of fixtures against a
124 KB landing script), locators and reviewers sweep them whenever they
search `.agents`, and the commit history holds several "fix fixture"
landings. Their protection is weaker than what already exists: every landing
is one commit on a local branch, recoverable from the reflog, behind one
explicit user confirmation; a bad review prompt or inventory result is
visible to the agent that consumes it in the same turn.

Production scripts also carry fixture-only hooks (`-FixtureFailure`
parameters, `$FixtureSmartGitExecutable`, and
`BROKEN_ENGINE_FINALIZE_WORKFLOW_FIXTURE` / `BROKEN_ENGINE_FINALIZE_*_FIXTURE`
environment branches) in `Invoke-FinalizeLanding.ps1`,
`Invoke-FinalizeApprovalPreparation.ps1`, `Invoke-FinalizeCandidateCommit.ps1`,
`Invoke-FinalizePrimaryMovementCheck.ps1`, and
`Show-FinalizeApprovalReview.ps1`.

## Design

Author's recommendation: delete the eleven suites and the prose that
requires running them, in one change.

1. `git rm` the eleven files in the table (skipping any Streamline 2 already
   removed).
2. Delete the `## Fixture suites` section from
   `finalize-changes/references/scripts.md` and its entry in that file's
   contents list; delete the `Test-SessionChangeInventoryFixtures` paragraph
   at `repo-code-review/SKILL.md:53-56`; delete the
   `Test-NextPlanWorkflowScripts` sentence at `next-plan/SKILL.md:159-165`.
3. Remove the fixture-only hooks named in Context from the five finalize
   scripts: each `-FixtureFailure` parameter and every branch that reads it,
   `$FixtureSmartGitExecutable` and its branch, and every
   `BROKEN_ENGINE_FINALIZE_*_FIXTURE` environment check, keeping the
   non-fixture behavior of each branch. Where Streamline 1 or 5 has already
   deleted a function, there is nothing to do there.
4. Leave the eight validators and `Test-PlanCitations.ps1` untouched.

## Critical files

- The eleven suite files in Context (delete).
- `.agents/skills/finalize-changes/references/scripts.md`
- `.agents/skills/repo-code-review/SKILL.md`
- `.agents/skills/next-plan/SKILL.md`
- `.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1`,
  `Invoke-FinalizeApprovalPreparation.ps1`,
  `Invoke-FinalizeCandidateCommit.ps1`,
  `Invoke-FinalizePrimaryMovementCheck.ps1`,
  `Show-FinalizeApprovalReview.ps1`

## In scope

- Whole-file deletion of the eleven suites.
- `scripts.md` `## Fixture suites` section and its contents-list entry;
  `repo-code-review/SKILL.md:53-56`; `next-plan/SKILL.md:159-165`.
- In the five finalize scripts: the `-FixtureFailure` parameter declarations
  and `ValidateSet` values, the `$FixtureSmartGitExecutable` parameter, and
  every `if` branch or environment read that exists only to serve a fixture
  run, with the surrounding non-fixture logic kept as is.

## Out of scope

- The eight validators, `Test-PlanCitations.ps1`, and
  `Test-CollectionLayoutFixtures.ps1`.
- Any behavior of the finalize, claim, inventory, or prompt-assembly scripts
  beyond removing fixture hooks.
- Existing tracked Plans that cite a suite in their acceptance criteria
  (`FinalizeSessionLabelMapping.md`, `PlanMalformedDependencyTerminalHandling.md`,
  Streamline 1 through 5): a cited suite that no longer exists is recorded on
  that Plan's execution card as "fixture deleted by Streamline 7; not
  applicable".
- The `AGENTS.md:102` directive wording (Streamline 6).

## Coordination

Streamline 1 and 5 rewrite the same five finalize scripts. Whichever lands
second resolves the overlap by keeping both deletions; a fixture hook inside
a function the other Plan removed needs no further action.

## Risk tier and invariants

Expected Tier 2: script and skill behavior (file removals plus dead-branch
removal in scripts that move primary). The lock, advance, rollback, and
confirmation paths are not changed in meaning.

Invariants:

- Every finalize script still behaves identically on its non-fixture inputs.
- No skill or reference instructs running a deleted suite.

## Acceptance criteria

- `rg -n "Fixtures\.ps1|Test-NextPlanWorkflowScripts|Test-WorktreeCliPlanScheduler|Test-Find-AgentSessionTranscript|Test-MeasureSessionContext|Test-BuildResultFixtures|FixtureFailure|FixtureSmartGitExecutable|_FIXTURE" .agents AGENTS.md --glob '!**/Test-CollectionLayout*'` returns no hits.
- `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports `status: valid`.
- `/validate-skill` passes for every changed `SKILL.md`.
- One ordinary landing through `/finalize-changes` succeeds after the change.
