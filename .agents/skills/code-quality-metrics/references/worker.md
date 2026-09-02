# Code Quality Metrics Worker

The invocation steps, the per-mode contracts, and the rules for the worker
dispatched with [`../SKILL.md`](../SKILL.md), which owns how the result is
reported.

## Steps

1. Read [MetricContract.md](MetricContract.md) before creating a targets file or consuming a report; read [Remediation.md](Remediation.md) only when explaining advisory results. Done when each reference the request needs has been read.
2. For the immutable history workflow, read [HistoryContract.md](HistoryContract.md) before using `BootstrapIdentity`, `Contract`, or `Generate`. Done when that reference has been read, or no history mode is requested.
3. Run the public PowerShell entry point from the repository root, using the mode section below that matches the request. Done when that mode's invocation has been issued from the repository root.
   - Both Snapshot and Compare write canonical compact JSON to stdout and, when requested, the same full report to `-OutputPath`.
   - Add `-Digest` to emit a compact `broken-engine-code-quality-evidence/v2` summary to stdout instead of the full report, or `-Phase0Hints` (which implies `-Digest`) to add capped outlier, clone, complexity, and skip hints for the target paths.
   - Add `-DigestPath <file>` instead of `-Digest` to write those digest bytes to that file and print only a `broken-engine-code-quality-digest-receipt/v1` line; an existing path is refused before any analyzer work and is never overwritten.

### Bootstrap identity

`Invoke-CodeQualityMetrics.ps1 -Mode BootstrapIdentity -RepositoryRoot <absolute repository root>`
emits one path-free identity object and never starts the analyzer.

### History

`Invoke-CodeQualityMetricsHistory.ps1 -Mode Contract` is a read-only decision; `-Mode Generate`
writes only the `CodeQualityMetricsHistory.jsonl`/`.svg` pair into the supplied ignored `Temp`
directory.

Contract/Generate behavior is owned here. Landing is owned by
[`/finalize-changes`](../../finalize-changes/SKILL.md).

### Snapshot

Capture one current scope:

```powershell
pwsh -NoProfile -File .agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetrics.ps1 -Mode Snapshot -Target Engine/Source -Scope Recursive -RepositoryRoot <absolute repository root>
```

Use `Exact` for one file, `Directory` for direct files, and `Recursive` for descendants. `.h` files
are C++ inputs, except a `.h` beneath contiguous `Data/Shaders` components is pure GLSL and rejected
(`ShaderLayouts.h` and `ShaderLayoutsBase.h` stay C++). Treat corpus-only parse omissions as reported
advisory coverage, not failures.

### Compare

Compare only the paths listed in a UTF-8 targets file against a full-SHA baseline:

```powershell
pwsh -NoProfile -File .agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetrics.ps1 -Mode Compare -Targets Temp/targets.json -Baseline <full-commit-sha> -RepositoryRoot <absolute repository root>
```

Compare parses the full corpus on both the baseline and the current side, so it takes roughly 2 to
3.5 minutes; issue it with a call timeout of at least `600000` ms, never an ordinary ~120 s call
cap. Stdout is written only after the run completes, so a call killed at that cap yields nothing
recoverable and must not be retried at the same cap.

The analyzer derives pairing itself: a listed path in both corpora pairs with itself, a current-only
path pairs with the first similar baseline-only path as a rename, and the rest become one-sided
add or delete pairs. Do not broaden targets from checkout changes. Context changes remain visible
but suppress target attribution.

## Rules

- Diagnostics go to stderr; exit `2` means inputs, capture, bootstrap, analyzer, drift, digest, or
  output persistence failed.
- A target parse or signature-extraction failure exits `2` with the analyzer's one-line JSON
  diagnostic forwarded verbatim.
- Read [MetricContract.md](MetricContract.md) `## Failures` for both target-failure contracts,
  advisory `upstream-omitted` rows, and the complete-parsing requirement for PASS.
