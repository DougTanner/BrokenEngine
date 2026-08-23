<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-23T22:11:32.625Z","dependsOn":[]} -->
# Fix: /finalize-changes delegation brief — preserve stale-base rebase authority

## Context

The first `/finalize-changes` dispatch brief in the historical session that
landed as `5760c05f74dc52a9816254e0f9415608170686fe` prohibited the finalizer
from using the documented linear-rebase recovery after a stale-base result. The
proven parent session `01a02963-5c0c-7863-9a0e-59143d858a7a` recorded at event
line 434 that the brief was too restrictive. The finalizer child spent 127.6
seconds before returning blocked; after the manager restored the documented
rebase authority, that same finalizer continued and succeeded. The residual is
therefore a delegation-contract gap, not a missing landing-script capability.

Current source documents the authority that the brief must preserve:

- `.agents/skills/finalize-changes/SKILL.md:12-16` assigns one implementer and
  requires linear reconciliation with `git rebase`.
- `.agents/skills/finalize-changes/SKILL.md:87-90` requires the authorized
  source commit to be squashed and rebased onto the current primary tip, with
  overlap and meaning changes inspected.
- `.agents/skills/finalize-changes/SKILL.md:204-210` permits a conflict to be
  resolved under approved invariants and says to abort only when the approved
  decisions cannot determine a valid resolution.
- `.agents/skills/finalize-changes/references/scripts.md:100-104` defines
  `git.primary-not-ancestor` as a caller stop-and-report result followed by a
  rebase and re-invocation.

The active user-approved Plan is
`Documents/Plans/Skills/NextPlanReviewTranscriptFinderNotFound.md`, whose
`## In scope` contains only
`.agents/skills/next-plan-review/SKILL.md` and
`.agents/skills/next-plan-review/scripts/Find-AgentSessionTranscript.ps1`.
Finalizer briefing and contracts are explicitly outside that boundary, so this
is a proven out-of-scope residual rather than an acceptance failure of the
active change. The current session baseline is
`b8ae1cfbdfb1325774f269c2f8740db2ca8ab6d2`.

The live `Documents/Plans/Skills/FinalizeChangesStep4TipMatchRelaxation.md`
Plan owns a different root cause and boundary: the exact `PrimaryTip` equality
check in Step 4 and harmless foreign primary movement. It does not own the
delegation brief's stale-base/rebase authority, so it is not a duplicate.

## Design

The author's recommendation is to make the `/finalize-changes` delegation
contract explicit in `.agents/skills/finalize-changes/SKILL.md`: a stale-base
result is a stop-and-report control return, not a terminal prohibition on the
documented recovery. After manager classification, the caller retains authority
to perform the skill's linear rebase and re-invoke the approval-preparation
step. The brief should carry the existing boundaries alongside that authority:
never merge or use `--rebase-merges`, never invent conflict behavior, preserve
the approved caller-owned paths, and return a blocker when the approved
decisions do not determine a valid resolution.

This keeps the correction in the skill's delegation contract and reuses the
existing scripts and recovery rules. No new retry mechanism, conflict policy,
or landing-script behavior is recommended.

## Critical files

- `.agents/skills/finalize-changes/SKILL.md` — opening delegation contract and
  the Step 2/recovery wording that the finalizer brief must preserve.
- `.agents/skills/finalize-changes/references/scripts.md` — read-only authority
  source for `git.primary-not-ancestor`, caller rebase/re-invocation, and
  blocked conflict outcomes.

## In scope

- The `/finalize-changes` delegation guidance in
  `.agents/skills/finalize-changes/SKILL.md`, including the opening worker
  contract and the Step 2/recovery statements that define stale-base stop,
  manager classification, linear rebase, and re-invocation.
- The smallest wording or brief-contract correction that prevents a generated
  finalizer brief from forbidding that documented sequence while retaining the
  existing scope and conflict boundaries.

## Out of scope

- `.agents/skills/finalize-changes/scripts/` mechanics, including the landing
  lock, compare-and-swap, primary mutation, receipt checks, and internal
  landing rebase.
- The Step 4 `PrimaryTip` equality/reachability behavior owned by
  `Documents/Plans/Skills/FinalizeChangesStep4TipMatchRelaxation.md`.
- `.agents/references/subagent-reporting.md` and generic delegation rules.
- The active transcript-finder Plan or either of its two changed files.
- New retry loops, new conflict-resolution behavior, scope expansion, builds,
  harness scenarios, or transcript paths/text in the repository.

## Risk tier and invariants

Expected Tier 2: scoped workflow behavior in one skill's delegation and
reconciliation contract. Root `AGENTS.md` treats a skill delegation or routing
change as behavior; escalate to Tier 3 if the implementation reaches the
landing lock, primary mutation, trust-boundary handling, or another excluded
invariant surface.

The implementation must preserve:

- linear history through the documented `git rebase` path; never merge or use
  `--rebase-merges`;
- the stop-and-report behavior for a stale-base/`git.primary-not-ancestor`
  result before manager classification;
- conflict resolution only under approved decisions, with no invented
  behavior, and a blocker when no valid resolution is determined; and
- the approved caller-owned path set and all existing confirmation, lock, and
  primary-advance boundaries.

## Acceptance criteria

- The finalizer's generated delegation brief explicitly preserves the
  documented sequence: stop and report a stale-base result, then—after manager
  classification—perform the linear rebase and re-invoke the documented step.
- A stale-base result is no longer made terminal solely by a delegation-brief
  prohibition, while unresolved or unauthorized conflicts remain blockers.
- The brief still forbids invented conflict behavior, merges, and unauthorized
  path or scope changes.
- `/validate-skill` passes for any changed
  `.agents/skills/finalize-changes/SKILL.md`; `plan validate` exits 0.

## Coordination

- None. The existing Step 4 Plan is a separate, non-directional boundary and
  does not block this delegation-contract correction.

## Notes

- Originating finding: `/next-plan-review` Proposed improvement P2, accepted as
  a proven out-of-scope residual of Change Workflow Step 7.
- The correction is intentionally recorded as ordinary debt, not tooling
  friction: the evidence is a finalizer delegation brief that contradicted the
  current documented authority, not a misbehaving `/next-plan` script or
  invocation.
