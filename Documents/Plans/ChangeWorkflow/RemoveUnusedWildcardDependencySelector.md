<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-27T03:32:31.272Z","dependsOn":[]} -->
# Remove the unused wildcard dependency selector from the step script

## Context

`.agents/scripts/Get-ChangeWorkflowSteps.ps1` `Format-Rows` expands a row's
`After` entries: `'review'` to the active Review and resolve correctness rows,
and `'*'` to every other active row (the `if ($dep -ceq '*') { $activeKeys }`
branch, the `$activeKeys` list built in the first loop, and the header comment
above `Format-Rows` describing `'*'`). Its only caller, the
`/verify-acceptance` row's `After = @('*')`, was removed by the landed
review-round change (commit `94fa96136`); no table the script builds (the
prepare rows, `Get-RoundLines`, `Get-DoneLines`) supplies `'*'` now. The
`$activeKeySet` set stays in use by the `$resolve` filter.

## Design

Delete the `'*'` branch, the `$activeKeys` list and its `Add` call, and the
`'*'` clause of the comment above `Format-Rows`. The printed rows do not change.

Change Workflow tier: Tier 1 — local behavior-preserving dead-code removal in
one script. No determinism, wire, or trust-boundary change; no C++, shader,
build, `.pack`, replay, or live-verification exposure.

## Critical files

- `.agents/scripts/Get-ChangeWorkflowSteps.ps1` (`Format-Rows` and the comment
  above it)

## In scope

- The `'*'` branch, `$activeKeys`, and the `'*'` comment clause.

## Out of scope

- The `'review'` selector, `$activeKeySet`, `$activeReviewKeys`, and every row
  table.

## Acceptance criteria

- No `'*'` selector or `$activeKeys` remains in `Get-ChangeWorkflowSteps.ps1`.
- A default run on an unchanged scratch scenario prints the same rows before
  and after the change.
- The static-checks runner passes for the changed files.
