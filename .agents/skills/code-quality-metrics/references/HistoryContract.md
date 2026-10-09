# History Contract

`Invoke-CodeQualityMetricsHistory.ps1` is a deterministic, read-only planning and ignored-output
workflow for the tracked history table. It never writes the tracked JSONL or SVG. `Contract` writes
one JSON receipt to stdout; `Generate` and `Rebuild` each write exactly two files under a
caller-selected, new directory beneath `RepositoryRoot/Temp`, then write one receipt to stdout.
Every receipt is compact UTF-8 JSON with one final LF and no absolute or volatile paths.

## Invocation

```powershell
pwsh -NoProfile -File .agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1 `
  -Mode Contract -RepositoryRoot '<absolute repository root>' -BaseCommit <current-primary-sha>

pwsh -NoProfile -File .agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1 `
  -Mode Generate -RepositoryRoot '<absolute repository root>' -BaseCommit <current-primary-sha> `
  -OutputDirectory <new directory beneath RepositoryRoot/Temp>

pwsh -NoProfile -File .agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1 `
  -Mode Rebuild -RepositoryRoot '<absolute repository root>' -BaseCommit <commit-sha> `
  -SeedPath <v2 seed table> -OutputDirectory <new directory beneath RepositoryRoot/Temp>
```

`-BaseCommit` is a full lowercase commit identity. Contract and Generate read the source JSONL as
the exact raw blob at
`<BaseCommit>:.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl`;
the working-tree file is not consulted. Rebuild reads only its `-SeedPath` file, validated as the
same v2 table. Every measured row comes from a commit Snapshot of that commit's tree, so no mode
reads working-tree C++ or needs a clean checkout.

## Source table and decision

The table (a `BaseCommit` blob or a seed) is UTF-8 without BOM, LF-only, with one final LF. Its
header line is exactly `{"schema":"code-quality-metrics-history/v2"}`; any other header, including
v1, is rejected. Each row has exactly `index,sha,date,measured,verbosity,structuralErosion,supported,parsed`:
`index` contiguous from 0; `sha` the 40-character lowercase commit the row describes; `date` that
commit's committer date in strict ISO 8601 form with offset (`git log --format=%cI`); `measured` a
boolean, false for a carried row; ratios finite in [0,1]; counts non-negative with `parsed` no
greater than `supported`. Rows are not date-ordered. Rows before the row of this repository's root
commit describe archive commits (`history/README.md`).

The table is lag-by-one: the table in commit C holds no row for C. Contract and Generate find the
newest table row whose `sha` is on `BaseCommit`'s first-parent chain and plan one row per
first-parent commit after it through `BaseCommit`, oldest first (one row, `BaseCommit`'s, in the
ordinary case). When no table row is on that chain, as after a squash or rewrite of `main`, they
exit `2` naming the recovery (Rebuild with the current table as its seed) and measure nothing.

Each planned commit is classified by the Git diff from its first parent. The classifier considers
only `.h` and `.cpp` paths, including add/delete/modify/rename records. A header beneath contiguous
`Data/Shaders` components is excluded as pure GLSL except `ShaderLayouts.h` and
`ShaderLayoutsBase.h`. `.agents`, `.claude`, `ThirdParty`, `Temp`, other extensions, and pure GLSL
do not force a capture. The decision is:

| Metric-supported change from the first parent | Capture mode |
| --- | --- |
| yes | `cpp-change` (one commit Snapshot of that commit; `measured:true`) |
| no | `carry-forward` (the previous row's values; `measured:false`) |

Rebuild refuses a seed without a row for `BaseCommit`'s root commit, and one that, from the root's
row on, holds more than one row for the same `sha`. Its output is the seed rows
before the root's row, copied, then one row per first-parent commit of `BaseCommit`, root first:
the root takes its seed row; a `cpp-change` commit whose `sha` has a seed row takes that row's
values and `measured`; any other `cpp-change` commit is measured; every `carry-forward` commit is
carried even when the seed holds its `sha`. First-parent rows take their `date` from `%cI`.

The row index is always the previous row plus one. A run with no row to measure performs no
Python probe, bootstrap/cache access, scb source resolution, or analyzer run.

## Bootstrap identity

`Invoke-CodeQualityMetrics.ps1 -Mode BootstrapIdentity` returns exactly the typed
`broken-engine-code-quality-bootstrap-identity/v1` object:

```json
{
  "schemaVersion":"broken-engine-code-quality-bootstrap-identity/v1",
  "python":{"implementation":"CPython","version":"3.12.10","architecture":"AMD64","executableSha256":"..."},
  "lockSha256":"...",
  "cacheKey":"...",
  "venvPythonSha256":"...",
  "sg":{"sha256":"...","version":"..."},
  "completionIdentitySha256":"...",
  "scbContentDigest":"..."
}
```

The object has no source, cache, venv, or executable paths. The completion identity is a canonical
hash of the validated cache completion fields. The scb content digest is a canonical hash of the
ordinal manifest for `requirements.lock` and the copied tracked ordinary files beneath `src`.

## Capture and drift

An active capture binds `gitlinkCommit`, resolved scb-check `HEAD`, clean status, and the exact
consumed membership. Each membership entry is `{relativePath,gitMode,type,length,rawSha256}` and
is sorted by ordinal relative path. Ordinary `__pycache__` directories are skipped before source
consumption; every other ignored/untracked/reparse/symlink extra blocks the capture. A run that
measures prepares the BootstrapIdentity and manifest once, and every commit Snapshot takes them
again before and after itself; any difference from the prepared capture, or any identity,
membership, or source drift, fails the run. The commit Snapshot is
`Invoke-CodeQualityMetrics.ps1 -Mode Snapshot -Target Engine/Source -Scope Recursive -Commit <sha>`,
and the row stores its corpus values. Contract exposes a frozen `generator.sha256` separately from
the optional active `capture.digest`. For a fixed BootstrapIdentity, the one the receipt records,
and the generating checkout's analyzer files, Generate's two output files are a pure function of
`BaseCommit`, and Rebuild's of `BaseCommit` and the seed bytes. A row is measured by the analyzer of
the Generate or Rebuild run that adds it, not of the commit it describes, so a landing that changes
the analyzer measures its `BaseCommit` row with the new analyzer.

## Receipt schemas

Contract emits `broken-engine-code-quality-history-contract/v2` with construction order:

`schemaVersion,mode,source,series,rows,generator,capture,snapshot`.

`source` contains only `baseCommit`. `series` contains `rows,lastIndex,lastDate,historyBytesSha256`,
where the final field is the SHA-256 of the complete source JSONL bytes. `rows` has one entry per
planned row, oldest first, each with `sha`, `captureMode`, and `patch`: the normalized change rows
from the commit's first parent (`baseCommit`) to the commit (`tipCommit`), the count of
metric-supported changes, and `cppChanged`. `generator` contains its repository-relative script
path and digest. `capture` is null when no planned row is `cpp-change`; otherwise it contains the
path-free BootstrapIdentity digest, scb content digest, manifest digest, manifest, and combined
capture digest. `snapshot` is null with it and otherwise records the fixed `Engine/Source`
recursive Snapshot and that coverage is required.

Generate emits `broken-engine-code-quality-history-update/v2` with construction order:

`schemaVersion,mode,source,generator,capture,rows,series,outputs`.

`rows` has one entry per appended row, in order, each with `sha`, `captureMode`, `patch` (as in
Contract), the appended `row`, and its Snapshot `coverage` (`corpusCounts` and `targetCounts`) or
null for carry-forward. `series` contains the output JSONL SHA-256 (`digest`) and the source
`historyBytesSha256`. JSONL is the exact validated source bytes plus the appended canonical rows.

Rebuild emits `broken-engine-code-quality-history-rebuild/v1` with construction order:

`schemaVersion,mode,source,capture,series,outputs`.

`source` contains `baseCommit` and the seed file's `seedSha256`. `series` contains `rows`,
`reused` (first-parent rows taken from the seed, root included), `measured`, `carried`, and the
output JSONL SHA-256 (`digest`); `rows` is the pre-root seed rows plus `reused`, `measured`, and
`carried`. JSONL is the v2 header plus every canonical row.

In every writing mode, `outputs.jsonl` and `outputs.svg` each contain the repository-relative path,
byte count, and SHA-256. The files are named `CodeQualityMetricsHistory.jsonl` and
`CodeQualityMetricsHistory.svg`. SVG is UTF-8 LF without BOM, fixed at 1800x1150, plots one point
per row in each of its three panels, has no timestamps or machine paths, and embeds the series
digest.
