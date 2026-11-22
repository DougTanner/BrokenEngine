<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-22T15:00:51.194Z","dependsOn":[]} -->
# Verify /save-plan-only landing diffs deterministically in inventory script

## Context

Root `AGENTS.md` (`## Directives`, instruction changes) requires moving every
deterministic rule a program can decide into a script or tool result, leaving
prose for judgment. During `/progressive-disclosure-review` of the always-land
`/save-plan` update, finding 5 (`instruction-change`) identified a deterministic
path-comparison rule kept in prose because PowerShell scripts were outside that
session's implementation boundary:

- `.agents/scripts/Get-SessionChangeInventory.ps1:618-712` (`Get-LandingState`)
  already computes `landing.reviewed` (every path, status, `oldPath`, and mode
  in the reviewed three-dot diff, merged with Coretide paths under
  `Projects/Coretide/`) and `landing.acceptanceSkeleton` (`:677-690`, emitting
  `/coherence-review` as `BLOCKED` for `plan`/`doc` classes per `:42, 678-680`
  and `Executable Plan check` for `Documents/Plans/` or
  `Projects/Coretide/Documents/Plans/` per `:681-690`), returning `landing` at
  `:705-711`.
- `.agents/skills/finalize-changes/references/landing-acceptance-table.md:68-70`
  exempts `/save-plan`-only diffs (only the saved file and any `/save-plan`
  companion Plan) from `/coherence-review` by instructing the finalizer in prose
  to verify that every path in `landing.reviewed` qualifies before marking the
  aggregate `/coherence-review` row `PASS`.
- `.agents/skills/save-plan/SKILL.md:110-121` already reports the exact saved
  file path and any companion Plan path in its handoff, and
  `landing-acceptance-table.md:8-12` is the sole caller of
  `Get-SessionChangeInventory.ps1 -Landing`.

Because `Get-SessionChangeInventory.ps1` does not compare `landing.reviewed`
against the authorized `/save-plan` paths, every `/save-plan` landing relies on
prose path-set inspection rather than a deterministic script verdict.

## Design

Recommended approach: add an optional `-Landing`-only parameter to
`.agents/scripts/Get-SessionChangeInventory.ps1` that compares the full
`landing.reviewed` set against the supplied authorized `/save-plan` saved and
companion paths and emits a deterministic verdict object on `landing`, then
update `.agents/skills/finalize-changes/references/landing-acceptance-table.md`
to pass that parameter and read the emitted verdict. Rationale: computing the
exact set comparison over the untruncated `landing.reviewed` list inside
`Get-LandingState` handles renames, `oldPath`, Coretide prefixes, and empty or
mixed diffs deterministically without duplicating Git diff logic in a second
script.

1. In `.agents/scripts/Get-SessionChangeInventory.ps1`:
   - Add an optional `[string] $SavePlanPaths` parameter (`:9-20`), accepted
     only with `-Landing` (reject without `-Landing` as an input error). Pass
     multiple paths as one comma-separated token (matching `New-PlanFile.ps1
     -DependsOn`) so callers stay on the canonical `pwsh -NoProfile -File` form
     in root `AGENTS.md` `## Directives`.
   - Normalize and validate each supplied path (non-empty, repository-relative
     POSIX path, no `..`, under `Documents/Plans/`, `Documents/Features/`,
     `Documents/Investigations/`, or their `Projects/Coretide/` equivalents,
     excluding `AGENTS.md`).
   - In `Get-LandingState` (`:618-712`), when `-SavePlanPaths` is supplied,
     evaluate an exact set comparison against the full untruncated `$reviewed`
     list: `$reviewed` must be non-empty, every reviewed row must be an
     authorized `/save-plan` addition/modification with no unexpected `path` or
     `oldPath`, and every supplied path in `authorizedPaths` must appear in
     `reviewedPaths`. Emit a `savePlanOnly` object on `landing` (`:705-711`)
     containing `qualified` (bool, `true` only when `unexpectedPaths` and
     `missingPaths` are both empty and `reviewedPaths` is non-empty),
     `authorizedPaths`, `reviewedPaths`, `unexpectedPaths` (any unauthorized
     `path`/`oldPath` or disallowed status such as deletion/rename), and
     `missingPaths` (all ordinal-sorted). Keep `acceptanceSkeleton` emitting
     `Executable Plan check` whenever any reviewed path sits under
     `Documents/Plans/` or `Projects/Coretide/Documents/Plans/`.
2. In `.agents/skills/finalize-changes/references/landing-acceptance-table.md`:
   - In `## Reviewed diff and authorization` (`:8-18`, `:43-50`): document
     passing `-SavePlanPaths '<comma-separated saved and companion paths>'` on
     `/save-plan` landings and reading `landing.savePlanOnly`.
   - In `## Acceptance table` (`:68-75`): replace manual prose
     path-qualification with requiring `landing.savePlanOnly.qualified` to be
     `true` (exact set match: empty `unexpectedPaths`, empty `missingPaths`, and
     non-empty `reviewedPaths`) and `truncated` `false` before marking the
     aggregate `/coherence-review` row `PASS` under the `/save-plan` exemption,
     keeping the evidence rule (recording `authorizedPaths`, `reviewedPaths`,
     and the exemption) and keeping `Invoke-StaticChecks.ps1` and
     `Executable Plan check` mandatory. When `qualified` is `false` because
     `missingPaths` is non-empty (with `unexpectedPaths` empty and
     `reviewedPaths` non-empty), block unless a recorded no-change decision
     accounts for every missing path per `## Reviewed diff and authorization`
     step 2 (`:22`); clearing that missing-path blocker leaves `qualified`
     `false`, so the `/save-plan` exemption does not apply and
     `/coherence-review` requires a normal review handoff (remaining `BLOCKED`
     without one).

Change Workflow tier: Tier 2 — trigger: scoped behavior of one tooling
subsystem (`.agents/scripts/Get-SessionChangeInventory.ps1` and
`.agents/skills/finalize-changes/references/landing-acceptance-table.md`). No
C++ source, determinism/CRC, wire, serialization, `.pack`/`kiVersion`, replay,
affinity, threading, allocation, shader, build, live `/agent-harness`, or
shared-tool build/bootstrap coordination exposure.

## Critical files

- `.agents/scripts/Get-SessionChangeInventory.ps1` — parameter declarations
  (`:9-20`), input validation, and `Get-LandingState` (`:618-712`).
- `.agents/skills/finalize-changes/references/landing-acceptance-table.md` —
  `## Reviewed diff and authorization` (`:8-50`) and `## Acceptance table`
  (`:68-75`).

## In scope

- `.agents/scripts/Get-SessionChangeInventory.ps1` (`:9-20`, input validation,
  and `Get-LandingState` `:618-712`): add optional `-SavePlanPaths` for
  `-Landing`, validate supplied paths, compute `landing.savePlanOnly`
  (including `qualified`, `authorizedPaths`, `reviewedPaths`,
  `unexpectedPaths`, and `missingPaths`) via exact set comparison over the
  untruncated reviewed diff, and preserve existing output when omitted.
- `.agents/skills/finalize-changes/references/landing-acceptance-table.md`
  (`:8-50`, `:68-75`): pass `-SavePlanPaths` on `/save-plan` landings and
  require `landing.savePlanOnly.qualified: true` (exact set match) for the
  `/save-plan` `/coherence-review` exemption instead of prose path verification
  (blocking on non-empty `missingPaths` unless a recorded no-change decision
  clears the missing-path blocker per `:22`, in which case `qualified` remains
  `false` and `/coherence-review` requires a normal review handoff), retaining
  the evidence and mandatory-check rules.

## Out of scope

- Changing `Get-SessionChangeInventory.ps1` outside `-Landing` mode or altering
  existing fields when `-SavePlanPaths` is omitted.
- Automating the other two exemptions in `landing-acceptance-table.md:61-67`
  (`Complete-NextPlan.ps1` deletions and `RefactorCandidates.md` /
  `WorkflowIncidents.md` edits).
- Changes to `.agents/skills/save-plan/SKILL.md`,
  `.agents/skills/finalize-changes/SKILL.md`, or `WorktreeCli`.

## Acceptance criteria

1. Running `Get-SessionChangeInventory.ps1 -Landing -SavePlanPaths ...` on a
   commit containing the exact set of supplied saved file and optional
   companion Plan emits `landing.savePlanOnly.qualified: true` with empty
   `unexpectedPaths` and empty `missingPaths`, while still emitting
   `Executable Plan check` in `acceptanceSkeleton` when a path under
   `Documents/Plans/` or `Projects/Coretide/Documents/Plans/` is present.
2. Running the same command on a mixed commit with any extra path (including a
   rename `oldPath`, a deleted file, or a non-authorized file) emits
   `landing.savePlanOnly.qualified: false` and lists every extra path in
   `unexpectedPaths`; passing `-SavePlanPaths` without `-Landing` or with an
   invalid/outside-planning-tree path fails with a non-zero exit code.
3. Running the same command on a diff missing any supplied path (for example, a
   companion-only diff where the supplied saved-file path is absent) emits
   `landing.savePlanOnly.qualified: false` and lists every absent supplied path
   in `missingPaths`; `landing-acceptance-table.md` blocks on non-empty
   `missingPaths` unless a recorded no-change decision accounts for every
   missing path while `unexpectedPaths` is empty and `reviewedPaths` is
   non-empty, and even when that decision clears the missing-path blocker,
   `qualified` stays `false`, the `/save-plan` exemption does not apply, and the
   `/coherence-review` row requires a normal review handoff (remaining `BLOCKED`
   without one, or `PASS` only with that handoff).
4. Omitting `-SavePlanPaths` preserves existing `Get-SessionChangeInventory.ps1
   -Landing` output unchanged.
5. `Invoke-StaticChecks.ps1` exits `0`, and `/external-skill-creator` validate
   mode on `finalize-changes` plus `/progressive-disclosure-review` pass.

## Notes

- Because `landing-acceptance-table.md:8-12` is the only caller of
  `Get-SessionChangeInventory.ps1 -Landing`, no other skill needs a command
  line change.
- Evaluate `savePlanOnly` before stdout shedding/truncation (`:694-704`) so
  `qualified` reflects the complete reviewed diff, while still requiring
  `truncated: false` for a valid landing table.
