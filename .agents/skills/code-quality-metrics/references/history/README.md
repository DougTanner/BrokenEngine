# Pre-squash metrics history — do not remove

`CodeQualityMetricsHistory.jsonl` and `CodeQualityMetricsHistory.png` record per-commit
code-quality metrics from the project's pre-squash development history. On 2026-08-10 the entire
history was squashed into a single baseline commit on `main`; the commits these rows reference no
longer exist in this repository:

- Rows 0–640: the original `2.0.0`-era history (row 0 is the pre-2.0.0 snapshot later kept as
  branch `1.0.0`).
- Rows 641–645: the short `2.1.0` rebuild era (binary asset history stripped with git-filter-repo).
- Row 646 onward: the live squashed `main` era; only these SHAs resolve in this repository.

Pre-squash SHAs are trend data only, resolvable solely in offline archives of the old
`BrokenEnginePublic` repositories. The trend remains valid and continuous; never delete these
entries.
