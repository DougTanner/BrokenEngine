# 03 Runtime acceptance

Read `Temp/lighting-plans/00-context.md` first. Requires plans 01-02 done.

## Objective

Settle every row of `Documents/Investigations/Engine/LightingFoundationHandoff.md`
`## Acceptance table` with evidence from the current binaries, and settle the memory-cost question.

## Steps

1. `implementer` runs `/agent-harness` for rows 3-9 in one client + server session, following the
   handoff's numbered checks. Notes:
   - Row 3 baseline: `Temp/lighting-strip-baseline/` (`temp-replay-steps.md` `## After-run steps`,
     `baseline-close.png`, `baseline-wide.png`, save `lighting-strip-baseline-t400`). Time of day
     advances on the wall clock even when paused, so pixel equality is not expected: use the timed
     schedule, then side-by-side inspection — same terrain, water, units, glows and shadows; no
     effect missing or added; slight sun-angle drift allowed.
   - Restore the minimized agent client with `window_state` and confirm `resize` returned 1920x1080
     before captures; captures advance only on rendered frames while paused (Space).
   - Rows 8-9 search each process's `--log-file`, not `get_logs`.
   - Row 4 off-screen: `traceOnlyInstances > 0` may need units in a neighbouring rendered cell
     (`set_client_grid_coord`); that setup is unverified.
   - The comparison UI is the `Comparison` subtab of the Tweaks Lighting section.
2. Row 1 is plan 01/02's build; row 2 is the `rg` in the handoff (main runs it).
3. Memory-cost question: during the run, read the `gpuTimers` row whose name ends
   `Lighting Terrain` with comparison mode enabled on a view with islands. If it is not far above the
   15 ms order, delete `Documents/Investigations/Engine/LightingTerrainPreparationMemoryCost.md` (and
   its link in the handoff `## Decisions`); otherwise leave it and report the number to the user.
4. Failures: diagnose with `/external-diagnose-bug` before any fix; fixes via `/resolve-findings`,
   then `/code-style-review` if C++ changed, `/compile`, re-review of the changed region, and rerun
   the failed rows. Never loosen a row to pass; a row that is wrong for a prototype goes to the user
   with evidence.

## Acceptance

All 9 rows pass with evidence paths recorded; harness processes and lock released.

## Finish

Tick 03 in `00-context.md` `## Progress` with each row's result and evidence path, and the terrain
timer value.
