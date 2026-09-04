<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-04T00:17:12.932Z","dependsOn":[]} -->
# Fix: Invoke-StaticChecks.ps1 — the validate-skill check skips a session that changed only skill-package scripts and references

## Context
Observed symptom. In this session the Step-4 gate ran

`pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1 -RepositoryRoot <worktree root> -Baseline 56816149c1c9764d54614350ee45a23146a04899`

on a diff whose only changed files were
`.agents/skills/validate-skill/scripts/Validate-Skill.ps1`,
`.agents/skills/validate-skill/references/frontmatter-schema.md`, and
`.agents/skills/update-claude-docs/references/worker.md`. The runner reported the
`validate-skill` row as `triggered=false status=skipped`, so the gate never ran
the very validator the session had changed. The implementer worked around it by
hand-running a 45-package `Validate-Skill.ps1 -Path` sweep, and the plan-audit
acceptance expectation of a `validate-skill pass` row went unmet and was reported
as a residual.

The mechanism is visible in the current tree.
`.agents/scripts/Get-SessionChangeInventory.ps1:214` classifies a path as `skill`
only when the leaf is `SKILL.md` at depth 4 under `.agents/skills/`;
`:280` sets `validateSkill = $classes.Contains('skill')` from that class alone;
`.agents/scripts/Invoke-StaticChecks.ps1:221-222` returns the skipped row
whenever that trigger is false, and `:223` selects targets by the same `skill`
class. A changed skill-package script or `references/*.md` classifies as `script`
or `doc`, so it neither sets the trigger nor supplies a target.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction, while the
`Landing ref` line names a ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: af9328bb-db16-41cf-b7e4-009b42640e1a
- Worktree/branch UUID: d1676a9b-2e6e-42a5-96dc-0f88042786c9
- Session branch: claude/d1676a9b-2e6e-42a5-96dc-0f88042786c9
- Worktree: .claude\worktrees\BrokenEngine\d1676a9b-2e6e-42a5-96dc-0f88042786c9
- Landing ref: claude/d1676a9b-2e6e-42a5-96dc-0f88042786c9
  Fallback once the recorded ref is gone:
  `git log --diff-filter=A --format=%H -- Documents/Plans/Engine/StaticCheckValidateSkillTrigger.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone; never review an aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`;
the cited lines above are expected to make a transcript unnecessary. Only when
the transcript is genuinely needed, in a new session run
`/next-plan-review claude/d1676a9b-2e6e-42a5-96dc-0f88042786c9` in bounded
friction mode, supplying client `claude` and the recorded conversation session
ID. Then make the smallest fix inside the `## In scope` boundary below. If
root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

This author's recommended shape, for the fix session to confirm or replace:

1. Select the check's targets per skill package rather than per changed
   `SKILL.md`: every package with any changed file under
   `.agents/skills/<package>/**` is validated once, at that package's `SKILL.md`
   path (`Validate-Skill.ps1` accepts either the package directory or its
   `SKILL.md`), and the trigger becomes true whenever that package set is
   non-empty. Whether the package set is derived inside
   `Invoke-StaticChecks.ps1` from the existing entry paths, or from a widened
   inventory trigger, is the fix session's call; the entry table already carries
   every changed path, so no new inventory field is required for this half.
2. Additionally validate every package under `.agents/skills/` when the
   validator itself (`.agents/skills/validate-skill/scripts/Validate-Skill.ps1`)
   or a file under `.agents/references/` that it reads has changed, which is the
   sweep this session ran by hand. Keep the existing per-row `pass`/`fail`/
   `blocked` mapping from the validator's 0/1/2 exit codes unchanged.

## Critical files
- `.agents/scripts/Invoke-StaticChecks.ps1`
- `.agents/scripts/Get-SessionChangeInventory.ps1` (only if the fix widens the
  trigger there)
- `.agents/references/static-checks.md` (the runner's documented contract)

## In scope
- Root-cause investigation as `## Design` states
- `Invoke-ValidateSkillCheck` in `.agents/scripts/Invoke-StaticChecks.ps1`
  (`:220-240`): its trigger test, its target selection, and its whole-tree sweep
  condition
- `Get-PathClass` (`.agents/scripts/Get-SessionChangeInventory.ps1:198-221`) and
  the `validateSkill` line in `Get-RoutingTrigger` (`:280`), only if the fix
  widens the trigger rather than deriving packages in the runner
- The `validate-skill` row's description in `.agents/references/static-checks.md`,
  so the documented trigger matches the implemented one

## Out of scope
- The landed change the session produced: the three changed files listed in
  `## Context`
- `Validate-Skill.ps1`'s own diagnostics, output shape, and exit codes
- Every other row of the runner and every other inventory class and trigger,
  including `progressiveDisclosureReview`, `updateVcxproj`, and `planTouched`
- The `vcxprojCandidates` field and the `-EmitAcceptanceSkeleton` mode owned by
  `Documents/Plans/Engine/InventoryVcxprojAndAcceptanceTriggers.md`
- Unrelated skills and scripts; any transcript path or transcript text in the
  repo

## Coordination
`Documents/Plans/Engine/InventoryVcxprojAndAcceptanceTriggers.md` also changes
`Get-RoutingTrigger` in `.agents/scripts/Get-SessionChangeInventory.ps1`. Neither
Plan blocks the other, so this is not a `dependsOn` edge; whichever lands second
rebases onto the first's shape of that function and must leave every existing
`triggers` field name and meaning intact.

## Risk tier and invariants
Expected Tier 2 (scoped behavior of one static-check tool: it changes which
packages the gate validates); this author's classification, for main to confirm
at Step 1. Escalate if the fix reaches the inventory's schema or exit-code
contract. Invariants to preserve: the schema name
`broken-engine-session-change-inventory/v1` and the runner's own result schema,
field names, and 0/1/2 exit mapping are unchanged; `triggers` keeps every
existing field and name; a deleted skill package still produces the documented
`pass` row with no head-side path; a truncated inventory still produces the
documented `blocked` row. Never embed transcript paths or home paths.

## Acceptance criteria
- For a diff whose only changed file is
  `.agents/skills/<package>/references/worker.md`, the runner reports the
  `validate-skill` row `triggered=true` with a result for that package
- For a diff that changes `Validate-Skill.ps1`, the row covers every package
  under `.agents/skills/`
- For a diff that changes no file under `.agents/skills/` and none of the
  validator's inputs, the row stays `triggered=false status=skipped`
- A package that reports `INVALID` still produces a `fail` row and a validator
  setup error still produces a `blocked` row
- `Validate-Skill.ps1` reports `VALID` for any changed `SKILL.md`;
  `pwsh -NoProfile -File .agents/scripts/Test-PlanSchedulerState.ps1` reports
  `status: valid`
