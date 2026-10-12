# Code-quality metrics history

`CodeQualityMetricsHistory.jsonl` holds one row per described commit, keyed by that commit's
`sha` and committer `date`; [`../HistoryContract.md`](../HistoryContract.md) owns the v2 row
format and the receipts. The matching deterministic SVG is the only supported chart format.

Rows come only from `Invoke-CodeQualityMetricsHistory.ps1` `Generate`, which `/finalize-changes`
runs at every landing. It writes into an ignored `Temp` directory; the tracked pair changes only
when its output lands, never by a working-tree edit. The lag-by-one rule is in
[`../HistoryContract.md`](../HistoryContract.md) `## Source table and decision`, and which analyzer
measures a row is in its `## Capture and drift`.

Rows 0–645 are archive rows from before 2026-08-10, when the whole history was squashed into one
baseline commit on `main`, `e571f6f1` (row 646), this repository's root commit as of that date:

- Rows 0–640: the original `2.0.0`-era history (row 0 is the pre-2.0.0 snapshot later kept as
  branch `1.0.0`).
- Rows 641–645: the short `2.1.0` rebuild era (binary asset history stripped with git-filter-repo).

Archive SHAs do not resolve in this repository, only in offline archives of the old
`BrokenEnginePublic` repositories; their rows are trend data. From row 1165 on, one row is appended
per landing; [`../HistoryContract.md`](../HistoryContract.md) `## Source table and decision` owns
which rows resolve here and how planning treats those that do not.
