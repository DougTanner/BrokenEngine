# History Contract

`Invoke-CodeQualityMetricsHistory.ps1` is a deterministic, read-only planning and ignored-output
workflow for the tracked history table. It never writes the tracked JSONL or SVGs. `Contract` writes
one JSON receipt to stdout; `Generate` writes exactly three files under a caller-selected, new
directory beneath `RepositoryRoot/Temp`, then writes one receipt to stdout.
Every receipt is compact UTF-8 JSON with one final LF and no absolute or volatile paths.

## Invocation

```powershell
pwsh -NoProfile -File .agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1 `
  -Mode Contract -RepositoryRoot '<absolute repository root>' -BaseCommit <current-primary-sha>

pwsh -NoProfile -File .agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1 `
  -Mode Generate -RepositoryRoot '<absolute repository root>' -BaseCommit <current-primary-sha> `
  -OutputDirectory <new directory beneath RepositoryRoot/Temp>
```

`-BaseCommit` is a full lowercase commit identity. Contract and Generate read the source JSONL as
the exact raw blob at
`<BaseCommit>:.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl`;
the working-tree file is not consulted. Every measured row comes from a commit Snapshot of that
commit's tree, so no mode reads working-tree C++ or needs a clean checkout.

## Source table and decision

The table (a `BaseCommit` blob) is UTF-8 without BOM, LF-only, with one final LF. Its
header line is exactly `{"schema":"code-quality-metrics-history/v3"}`; any other header, including
v2, is rejected. Each row has exactly
`index,sha,date,measured,verbosity,verbosityEngine,verbosityCommon,verbosityDataPacker,verbosityGame,verbosityAgentTools,supported,parsed`:
`index` contiguous from 0; `sha` the 40-character lowercase commit the row describes; `date` that
commit's committer date in strict ISO 8601 form with offset (`git log --format=%cI`); `measured` a
boolean, false for a carried row; `verbosity` finite in [0,1]; each history-area value finite in
[0,1] or `null`; counts non-negative with `parsed` no greater than `supported`. Rows are in landing
order, not date order. Rows 0–645 describe archive commits (`history/README.md`).

A history area is a fixed group of repository paths, distinct from the Snapshot's own `areas` rows.
Each Snapshot `current.files[]` row joins the first history area whose prefix its POSIX `path`
starts with (ordinal), checked in this order:

| Prefix | History area (row field) |
| --- | --- |
| `Engine/Source/Agent/`, `Projects/BrokenEngineSandbox/Source/Agent/`, `Tools/` | AgentTools (`verbosityAgentTools`) |
| `Engine/` | Engine (`verbosityEngine`) |
| `Common/` | Common (`verbosityCommon`) |
| `DataPacker/` | DataPacker (`verbosityDataPacker`) |
| `Projects/` | Game (`verbosityGame`) |

A history-area value is the sum of its files' verbosity numerators over the sum of their
denominators; it is `null` when that denominator is zero. A file under none of the prefixes counts
only toward the overall `verbosity`, which is never `null`.

The table is append-only and never rewritten, including across a squash of `main`; the one
exception is the one-time v3 conversion (`history/README.md`). A row's `sha`
may not resolve in this repository: archive rows, and rows for commits since squashed off `main`.

The table is lag-by-one: the table in commit C holds no row for C. Contract and Generate plan
exactly one row per run, `BaseCommit`'s, dated by its `%cI`. Its comparison is the newest table
row's commit, because a carried row copies that row's values. When that commit does not resolve here
(`git cat-file -e <sha>^{commit}` fails), the comparison is the empty tree
`4b825dc642cb6eb9a060e54bf8d69288fbee4904`, so every metric-supported path counts as changed and
the row is measured. A commit that reaches `main` without a `/finalize-changes` landing gets no row
of its own unless it becomes a later run's `BaseCommit`, as a squash result does; its C++ change is
still caught by the next row's diff.

The planned row is classified by the Git diff from the comparison to `BaseCommit`. The classifier
considers only `.h` and `.cpp` paths, including add/delete/modify/rename records. A header beneath
contiguous `Data/Shaders` components is excluded as pure GLSL except `ShaderLayouts.h` and
`ShaderLayoutsBase.h`. `.agents`, `.claude`, `ThirdParty`, `Temp`, other extensions, and pure GLSL
do not force a capture. The decision is:

| Metric-supported change from the comparison | Capture mode |
| --- | --- |
| yes | `cpp-change` (one commit Snapshot of `BaseCommit`; `measured:true`) |
| no | `carry-forward` (the previous row's values; `measured:false`) |

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
and the row stores its corpus `verbosity`, `supported`, and `parsed` and its history-area values.
Contract exposes a frozen `generator.sha256` separately from the optional active `capture.digest`.
For a fixed BootstrapIdentity, the one the receipt records, and the generating checkout's analyzer
files, Generate's two output files are a pure function of `BaseCommit` and whether the newest
row's commit resolves locally (the comparison choice). A row Generate adds is measured by the
analyzer of the run that adds it, not of the commit it describes, so a landing that changes the
analyzer measures its `BaseCommit` row with the new analyzer. Rows the v3 conversion measured:
`history/README.md`.

## Receipt schemas

Contract emits `broken-engine-code-quality-history-contract/v3` with construction order:

`schemaVersion,mode,source,series,rows,generator,capture,snapshot`.

`source` contains only `baseCommit`. `series` contains `rows,lastIndex,lastDate,historyBytesSha256`,
where the final field is the SHA-256 of the complete source JSONL bytes. `rows` has exactly one
entry, the planned row, with `sha`, `captureMode`, and `patch`: the normalized change rows from the
comparison (`baseCommit`: the newest row's commit, or the empty tree) to `BaseCommit`
(`tipCommit`), the count of metric-supported changes, and `cppChanged`. `generator` contains its
repository-relative script path and digest. `capture` is null when no planned row is `cpp-change`;
otherwise it contains the path-free BootstrapIdentity digest, scb content digest, manifest digest,
manifest, and combined capture digest. `snapshot` is null with it and otherwise records the fixed `Engine/Source`
recursive Snapshot and that coverage is required.

Generate emits `broken-engine-code-quality-history-update/v4` with construction order:

`schemaVersion,mode,source,generator,capture,rows,series,outputs`.

`rows` has exactly one entry, the appended row, with `sha`, `captureMode`, `patch` (as in
Contract), the appended `row`, and its Snapshot `coverage` (`corpusCounts` and `targetCounts`) or
null for carry-forward. `series` contains the output JSONL SHA-256 (`digest`) and the source
`historyBytesSha256`. JSONL is the exact validated source bytes plus the appended canonical row.

In Generate's receipt, `outputs.jsonl`, `outputs.svg`, and `outputs.overlaySvg` each contain the
repository-relative path, byte count, and SHA-256. The files are named
`CodeQualityMetricsHistory.jsonl`, `CodeQualityMetricsHistory.svg`, and
`CodeQualityMetricsHistoryOverlay.svg`. Each SVG is UTF-8 LF without BOM, fixed at 3840x2160, has no
timestamps or machine paths, and embeds the series digest. Both plot the same six series — overall
verbosity, then the Engine, Common, DataPacker, Game, and AgentTools history areas — with one point
per row that has a non-null value; a row's x position is set by its index. Every plot area shares
one y range, the minimum and maximum over the non-null values of all six series plus a margin, and
carries five left-axis value labels with fixed four decimals in the invariant culture.
`CodeQualityMetricsHistory.svg` draws the series in six stacked bands of equal height;
`CodeQualityMetricsHistoryOverlay.svg` draws them as six lines in one plot area, with a legend along
the bottom. The count columns `supported` and `parsed` are not plotted.
