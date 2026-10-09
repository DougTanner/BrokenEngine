<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-09T23:10:12.044Z","dependsOn":[]} -->
# Investigate: what the structural-erosion metric tells us, and whether to emphasize controlling it

## Context
Origin: the user's review of the regenerated code-quality history chart
(`/next-plan` acceptance criterion 5 of the chart-readability change, passed).
The authoring agent's record of the user's direction: verbosity reads as a
clear metric that matches the user's expectation of the codebase, while
structural erosion looks more random but with clear up and down periods; the
user asked for a deep-dive into what structural erosion tells us and whether
it deserves emphasis on controlling it, recorded as a Plan.

Facts at baseline `be855f6a`:
- Definition: `.agents/skills/code-quality-metrics/references/MetricContract.md`
  `## Output` (line 65): `structuralErosion` is the mass of functions with CC
  above ten divided by total mass, where mass is `CC * sqrt(SLOC)`. Computed in
  `metrics()` of
  `.agents/skills/code-quality-metrics/scripts/Analyze-CodeQualityMetrics.py`
  (lines 121-126: `high` sums `mass` over units with `cc>10`, divided by the
  sum of `mass` over all units); `aggregate()` (line 134) sums numerators and
  denominators for areas.
- Guidance: `references/Remediation.md` treats erosion as navigation only
  ("structural erosion alone is navigation"); metrics are never a gate.
- History: `references/history/CodeQualityMetricsHistory.jsonl` has 1,156
  rows (index 0-1155; last row `b72ba6fd`, lag-by-one), 766 measured and the
  rest carried forward. Structural erosion spans 0.4658 (row 404, `e10c809e`)
  to 0.6974 (row 17, `e648aa64`); row 646 (root commit `e571f6f1`) is 0.5579
  and row 1155 is 0.5678. The largest row-to-row steps are at rows 241
  (-0.0608), 197 (-0.0499), 195 (-0.0423), 211 (-0.0378), 395 (-0.0341), and
  186 (+0.0281) — all in the archive era.
- Rows 0-645 are archive rows whose SHAs resolve only in offline archives of
  the old repositories (`references/history/README.md`); rows 646 onward
  follow `main`'s first-parent commits and resolve here.
- Each row stores corpus values of one commit Snapshot
  (`references/HistoryContract.md` `## Capture and drift`), measured by the
  analyzer of the run that added it; commit `1e9e821d` re-measured
  first-parent positions 78-492 with the current analyzer to remove a
  tool-change step.

Root-cause candidates the investigation must confirm or rule out, none yet
proven: (a) threshold crossing — one function moving across CC 10/11 moves
its whole mass into or out of the numerator; (b) denominator dilution — new
low-CC code lowers the ratio with no change to complex functions, and its
removal raises it; (c) corpus membership changes — files added, deleted,
moved into or out of supported or excluded roots, or parse failures changing
`parsed`; (d) analyzer or measurement differences between rows; (e) genuine
growth or decomposition of complex functions.

## Design
The deliverable is a findings record, not a change to any metric, tool, or
table. The author's recommendation for the record's path, mirroring the Plans
area: `Documents/Investigations/ChangeWorkflow/StructuralErosionDeepDive.md`
(`Documents/Investigations/AGENTS.md` asks for existing subject-area
subdirectories and only `Engine/` exists; the executor confirms the
subdirectory with the user if `Engine/` is preferred).

Method, as the author's recommendation:
1. Restate the computation from the analyzer source, including what a
   function unit is (`MetricContract.md` `## Output`, function-unit
   paragraph) and how numerator and denominator move independently.
2. Segment the history series into its up/down periods (index ranges, start
   and end values, dates), from the tracked table read at the baseline commit.
3. For periods in rows 646 onward, attribute each move by running
   `/code-quality-metrics` Snapshot with `-Commit` at the period boundaries
   (and at the commits of the largest single steps), in the same corpus scope
   the history rows use (`HistoryContract.md` `## Capture and drift`). From
   each pair, split the change into numerator movement (functions entering,
   leaving, or changing mass in `highComplexityFunctions`) and denominator
   movement (total mass), and name the commits, files, and functions that
   account for most of the move, with the share they account for.
4. For archive-era periods (rows 0-645), characterize them from the table's
   values and `supported`/`parsed` counts only, and mark them unattributable
   from this repository.
5. Test each root-cause candidate (a)-(e) against that evidence, and compare
   with verbosity's behavior over the same periods. Any sensitivity check
   (for example, another threshold or a continuous weighting) runs as a
   scratch computation over Snapshot output under `Temp/`, never as an
   analyzer change.
6. Recommend one of: emphasize controlling structural erosion (and how),
   keep it advisory as now, or de-emphasize or replace it — with rationale.
   List any concrete follow-on changes as options for the user's decision.

Snapshot outputs and scratch files go under `Temp/`; the record cites the
commits and values, not machine paths.

## Critical files
- `.agents/skills/code-quality-metrics/scripts/Analyze-CodeQualityMetrics.py`
  — `metrics()`, `aggregate()` (read only)
- `.agents/skills/code-quality-metrics/references/MetricContract.md` —
  `## Output` (read only)
- `.agents/skills/code-quality-metrics/references/HistoryContract.md` —
  `## Source table and decision`, `## Capture and drift` (read only)
- `.agents/skills/code-quality-metrics/references/Remediation.md` (read only)
- `.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl`
  and `README.md` (read only)
- New: the findings record under `Documents/Investigations/`

## In scope
- Read-only analysis of the files above, Git history, and commit Snapshots
  produced through `/code-quality-metrics`
- Scratch computations under `Temp/`
- Creating the one findings record named in `## Design`

## Out of scope
- Any change to the structural-erosion definition, threshold, or mass
  formula; to `Analyze-CodeQualityMetrics.py`,
  `Invoke-CodeQualityMetrics.ps1`, `Invoke-CodeQualityMetricsHistory.ps1`, or
  any other file under `.agents/skills/code-quality-metrics/`
- Any change to the history table, the SVG chart, or their contracts;
  running `Generate` or `Rebuild` to change tracked output
- Refactoring any C++ to lower the metric
- Implementing any recommendation the record makes; follow-on changes wait
  for the user's decision
- Verbosity, beyond the comparison `## Design` step 5 needs

## Risk tier and invariants
Tier 1 — trigger: documentation only (one new findings record; no code,
script, metric, or table change). Invariants: the metrics stay advisory
(`Remediation.md`); the tracked history pair stays byte-identical; nothing is
written outside `Temp/` except the findings record.

## Acceptance criteria
- The findings record exists at the path `## Design` names (or the user's
  chosen subdirectory) and contains: the computation, with numerator and
  denominator stated; the segmented up/down periods with index ranges,
  values, and dates; attribution of each period from row 646 on to commits,
  files, and functions, with the share of the move they account for;
  archive-era periods marked unattributable here; a verdict per root-cause
  candidate (a)-(e) with evidence; and one recommendation with rationale and
  follow-on options left to the user.
- Every attributed move cites a commit SHA and Snapshot values that a reader
  can reproduce with `/code-quality-metrics` Snapshot `-Commit`.
- `git diff --name-only` from the executing session's baseline to the
  finished change lists no path under `.agents/skills/code-quality-metrics/`
  and no C++ file.
- The recommendation is presented to the user for decision.

## Notes
Not a duplicate: the only other live Plan naming structural erosion,
`Documents/Plans/ChangeWorkflow/CodeQualityHistoryChartReadability.md`,
concerns chart rendering and is completed at this Plan's creating landing;
this Plan does not depend on it.
