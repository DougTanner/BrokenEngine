# Code-quality metrics history

`CodeQualityMetricsHistory.jsonl` holds one row per described commit, keyed by that commit's
`sha` and committer `date`; [`../HistoryContract.md`](../HistoryContract.md) owns the v3 row
format and the receipts. The matching deterministic SVGs — `CodeQualityMetricsHistory.svg` with one
band per series and `CodeQualityMetricsHistoryOverlay.svg` with every series in one plot — are the
only supported chart format.

After the one-time v3 conversion, whose files were landed once by hand, rows come only from
`Invoke-CodeQualityMetricsHistory.ps1` `Generate`, which `/finalize-changes` runs at every landing.
It writes into an ignored `Temp` directory; the tracked files change only when its output lands,
never by a working-tree edit. The lag-by-one rule is in
[`../HistoryContract.md`](../HistoryContract.md) `## Source table and decision`, and which analyzer
measures a row Generate adds is in its `## Capture and drift`.

Rows 0–645 are archive rows from before 2026-08-10, when the whole history was squashed into one
baseline commit on `main`, `e571f6f1` (row 646), this repository's root commit as of that date:

- Rows 0–640: the original `2.0.0`-era history (row 0 is the pre-2.0.0 snapshot later kept as
  branch `1.0.0`).
- Rows 641–645: the short `2.1.0` rebuild era (binary asset history stripped with git-filter-repo).

Rows 324–645 resolve only in offline archives of the old `BrokenEnginePublic` repositories. Rows
0–323 also resolve here today — row 0 through branch `1.0.0`, rows 1–323 only as objects no ref
reaches, which a prune can remove — so treat every archive row as trend data. From row 1165 on,
one row is appended per landing; [`../HistoryContract.md`](../HistoryContract.md)
`## Source table and decision` owns which rows resolve here and how planning treats those that do
not.

The one-time conversion to v3 measured the history-area values of every measured row once, with
its own analyzer; carried rows copy the row before. A kept overall value (`verbosity`, `supported`,
`parsed`) still comes from the analyzer that first measured it, an older one for rows 0–723, so
those rows' area values need not combine to their overall value. Measured rows from 646 on whose
commits are not ancestors of the conversion's base commit (1141–1148, 1150, 1151, 1153, 1173, and
1175) took both overall and area values from their squash result: the nearest later ancestor row,
row 1160 `3c3cfce5`, or for 1173 and 1175, which have none, the base commit `b169282c` itself.
