# Code-quality metrics history

`CodeQualityMetricsHistory.jsonl` holds one row per commit, keyed by that commit's `sha` and
committer `date`; [`../HistoryContract.md`](../HistoryContract.md) owns the v2 row format and the
receipts. The matching deterministic SVG is the only supported chart format.

Rows come only from `Invoke-CodeQualityMetricsHistory.ps1`: `Generate`, which `/finalize-changes`
runs at every landing, and `Rebuild`. Both write into an ignored `Temp` directory; the tracked pair
changes only when their output lands, never by a working-tree edit. The lag-by-one rule is in
[`../HistoryContract.md`](../HistoryContract.md) `## Source table and decision`, and which analyzer
measures a row is in its `## Capture and drift`.

Rows before row 646, the row of this repository's root commit `e571f6f1`, are archive rows from
before 2026-08-10, when the whole history was squashed into one baseline commit on `main`:

- Rows 0–640: the original `2.0.0`-era history (row 0 is the pre-2.0.0 snapshot later kept as
  branch `1.0.0`).
- Rows 641–645: the short `2.1.0` rebuild era (binary asset history stripped with git-filter-repo).

Archive SHAs do not resolve in this repository, only in offline archives of the old
`BrokenEnginePublic` repositories; their rows are trend data. From row 646 on, rows follow `main`'s
first-parent commits, root first.
