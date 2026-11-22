<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-22T20:33:19.488Z","dependsOn":[]} -->
# Confirm SessionChangeInventory Rebase Recovery

## Context

Under standing landing confirmation (`Confirm Landing`), landing preparation
hit `rebase.conflicted` against `main` commit
`adb2c09d6659920398b197d21e570d0205ade616` (`Verify /save-plan-only landing
diffs in inventory and add Invoke-Codex -ListRoutes`). Resolving that rebase
changed bytes after user confirmation across three files:

- `.agents/scripts/Get-SessionChangeInventory.ps1:24-25,103`: preserved the
  thin CLI wrapper (`-OutputPath`, `-EmitTargets`, `-EmitManifest`) and
  forwarded optional `-SavePlanPaths` to `Get-SessionChangeInventory` in
  `SessionChangeInventory.psm1`.
- `.agents/scripts/SessionChangeInventory.psm1` (`:44-53`, `:580-631`,
  `:687-689`, `:818`, `:856-858`, `:878-885`, `:1021`): ported
  `adb2c09d6659920398b197d21e570d0205ade616`'s `$script:SavePlanRoots`,
  `Get-SavePlanAuthorizedPaths`, `Test-SavePlanOnlyReviewedRows`,
  `-SavePlanPaths` parameter and `-Landing`-only validation
  (`inventory.mode-conflict`, `inventory.save-plan-paths-invalid`), and
  `Get-LandingState` `coherenceReview` suppression onto the extracted module
  under the module's invariants.
- `Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md:46-54` (merge
  boundary at `:46`): clean auto-merge preserving both
  `adb2c09d6659920398b197d21e570d0205ade616`'s two deletions and five added
  rows (`:46-50`) and the landing session's three deletions and four added
  rows (`:51-54`).

Post-recovery checks passed before landing on the resolved regions in the
landing commit introducing this Plan: focused `/coherence-review` returned
`PASS` with zero findings, `Invoke-StaticChecks.ps1` passed 6/6
(`netTokens=97 <= 100`), and CLI/module checks `T1`-`T10` passed.

Per `.agents/skills/finalize-changes/SKILL.md` `### Landing confirmation`, any
byte-changing post-confirmation recovery under standing confirmation files a
follow-up Plan so the user can inspect and confirm the conflict resolution or
request an adjustment. No defect is currently known, and no determinism/CRC,
serialization/`.pack`/`kiVersion`, replay, wire, affinity, threading,
allocation, shader, C++ build, or live-harness surface is affected.

## Unresolved decisions

Because the conflict resolution changed bytes after user confirmation, the
user decides whether to accept the merged regions as landed or adjust them:

- **Option 1 (Author recommendation):** Inspect and confirm the resolved
  regions across `Get-SessionChangeInventory.ps1`,
  `SessionChangeInventory.psm1`, and `WorkflowIncidents.md` with no further
  edits, completing this Plan via the empty-realized-change path in
  `.agents/skills/next-plan/SKILL.md` `## Rules`.
  - **Trade-offs:** Keeps the verified wrapper forwarding, module-side
    `-SavePlanPaths` port, and incident-log rows intact with zero extra churn;
    assumes the conflict resolution matches user intent.
- **Option 2:** Specify a concrete adjustment to one or more of the resolved
  regions listed in `## In scope` (for example, altering `-SavePlanPaths`
  validation, `Get-LandingState` `coherenceReview` suppression, or the merged
  rows in `WorkflowIncidents.md`) and apply it in the executing session.
  - **Trade-offs:** Aligns the merged regions with a different user preference,
    at the cost of a scoped edit and re-verification of the touched region
    (Tier 2 if the adjustment changes script/tool behavior; Tier 1 for an
    incident-log-only edit).

Recommendation rationale: focused `/coherence-review`, static checks, and
`T1`-`T10` CLI/module checks already passed on the resolved regions with no
known defect.

## Design

1. Present the three resolved regions in `## Critical files` and the two
   options in `## Unresolved decisions` to the user.
2. If the user confirms Option 1, make no edits and complete this Plan through
   the empty-realized-change path in `.agents/skills/next-plan/SKILL.md`
   `## Rules`.
3. If the user selects Option 2, apply only the user-specified adjustment
   within the regions named in `## In scope` (surfacing any request outside
   those regions for re-planning) and rerun the checks in
   `## Acceptance criteria`.

## Critical files

- `.agents/scripts/Get-SessionChangeInventory.ps1:24-25,103` — optional
  `-SavePlanPaths` parameter declaration and `$invokeParams` forwarding to
  `Get-SessionChangeInventory`.
- `.agents/scripts/SessionChangeInventory.psm1` (`:44-53`, `:580-631`,
  `:687-689`, `:818`, `:856-858`, `:878-885`, `:1021`) —
  `$script:SavePlanRoots`, `Get-SavePlanAuthorizedPaths`,
  `Test-SavePlanOnlyReviewedRows`, `Get-LandingState` `coherenceReview`
  suppression, and `Get-SessionChangeInventory` `-SavePlanPaths` parameter,
  `-Landing`-only mode check, path validation, and call-site forwarding.
- `Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md:46-54` —
  auto-merged `## Entries` rows combining
  `adb2c09d6659920398b197d21e570d0205ade616` (`:46-50`) and the landing
  session (`:51-54`).

## In scope

Pending the user's answer to `## Unresolved decisions`, authorized edits are an
empty realized change (`.agents/skills/next-plan/SKILL.md` `## Rules`). If the
user selects Option 2, edits are bounded strictly to these regions:

- `.agents/scripts/Get-SessionChangeInventory.ps1`: `param` block lines
  `24-25` (`-OutputPath`, `-SavePlanPaths`) and `$invokeParams.SavePlanPaths`
  forwarding at line `103`.
- `.agents/scripts/SessionChangeInventory.psm1`: `$script:SavePlanRoots`
  (lines `44-53`), `Get-SavePlanAuthorizedPaths` (lines `580-620`),
  `Test-SavePlanOnlyReviewedRows` (lines `622-629`), `Get-LandingState`
  signature and `coherenceReview` suppression (lines `631`, `687-689`), and
  `Get-SessionChangeInventory` `-SavePlanPaths` parameter, mode-conflict
  check, validation, and forwarding (lines `818`, `856-858`, `878-885`,
  `1021`).
- `Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md`: `## Entries`
  rows at lines `46-54` around the `:46` merge boundary.

## Out of scope

- Any region of `.agents/scripts/Get-SessionChangeInventory.ps1`,
  `.agents/scripts/SessionChangeInventory.psm1`, or
  `Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md` outside the
  regions named in `## In scope`.
- Other ChangeWorkflow scripts, skills, references, or Plans (including
  `.agents/skills/finalize-changes/references/landing-acceptance-table.md`,
  `.agents/scripts/Get-ChangeWorkflowSteps.ps1`,
  `.agents/scripts/Invoke-StaticChecks.ps1`, and
  `.agents/scripts/Find-SessionCandidates.ps1`).
- Any schema change to `broken-engine-session-change-inventory/v2`, any C++ or
  shader file, and adding unit tests.

## Acceptance criteria

- Option 1 (no-edit confirmation): the user explicitly confirms the resolved
  regions in `## In scope`, no implementation edits are made to tracked files
  (only Plan completion's own deletion of this Plan and removal of any
  dependency edges pointing to it), and the Plan completes through
  `.agents/skills/next-plan/SKILL.md` `## Rules`'s empty-realized-change path.
- Option 2 (user-specified adjustment): only regions in `## In scope` change,
  `Invoke-StaticChecks.ps1` passes with zero failures, the diff in
  `Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md` matches any
  incident-log adjustment the user specifies, and when the user specifies a
  script or module adjustment, CLI/module checks confirm `-SavePlanPaths`
  validation, `-Landing`-only enforcement, and `coherenceReview` suppression
  match the user's chosen behavior.

## Notes

- Change Workflow risk tier: Tier 1 (mechanical / empty realized change; no
  public signature or invariant exposure) under the recommended Option 1
  confirmation or an incident-log-only Option 2 edit; highest risk trigger is
  Tier 2 (scoped tool behavior in the ChangeWorkflow inventory script/module,
  with no determinism/CRC, wire/protocol, serialization/data layout,
  save/replay, threading, trust-boundary, or build/bootstrap coordination
  surface) if Option 2 changes script behavior.
- Originating event: standing-landing confirmation (`Confirm Landing`)
  post-confirmation `rebase.conflicted` recovery onto
  `adb2c09d6659920398b197d21e570d0205ade616` per
  `.agents/skills/finalize-changes/SKILL.md` `### Landing confirmation`.
- Dependencies: `dependsOn` stays empty (`[]`) per `Documents/Plans/AGENTS.md`
  `## Plan files` so the Plan is immediately eligible for bare `/next-plan`
  selection; no reciprocal `## Coordination` edges apply.
