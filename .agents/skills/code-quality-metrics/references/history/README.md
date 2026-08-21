# Code-quality metrics history

`CodeQualityMetricsHistory.jsonl` records the immutable pre-squash development history. The first
647 points (indices 0–646) are the byte-stable legacy table. A live point is generated into a
caller-provided ignored `Temp` directory by `Invoke-CodeQualityMetricsHistory.ps1`; the source
table is never appended in the working tree. The matching deterministic SVG is generated beside
that output JSONL and is the only supported chart format. Production Contract/Generate calls bind
the source JSONL to the exact raw blob at their supplied `BaseCommit`; a working-tree history edit
cannot change the source used for a receipt.

On 2026-08-10 the entire history was squashed into a single baseline commit on `main`; the
commits these rows reference no longer exist in this repository:

- Rows 0–640: the original `2.0.0`-era history (row 0 is the pre-2.0.0 snapshot later kept as
  branch `1.0.0`).
- Rows 641–645: the short `2.1.0` rebuild era (binary asset history stripped with git-filter-repo).
- Row 646 onward: the live squashed `main` era; only these SHAs resolve in this repository.

Pre-squash SHAs are trend data only, resolvable solely in offline archives of the old
`BrokenEnginePublic` repositories. The trend remains valid and continuous. Never rewrite or
append the tracked JSONL prefix. See `MetricContract.md` for the typed BootstrapIdentity and
history receipt schemas.
