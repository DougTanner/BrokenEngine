# History Contract

`Invoke-CodeQualityMetricsHistory.ps1` is a deterministic, read-only planning and ignored-output
workflow for the tracked history table. It never appends the source JSONL or writes the SVG beside
the tracked references. `Contract` writes one JSON receipt to stdout and `Generate` writes exactly
two files under a caller-selected, new directory beneath `RepositoryRoot/Temp`, then writes one
update receipt to stdout. Both receipts are compact UTF-8 JSON with one final LF and no absolute or
volatile paths.

## Invocation

```powershell
pwsh -NoProfile -File .agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1 `
  -Mode Contract -RepositoryRoot <absolute repository root> `
  -BaseCommit <current-primary-sha> -TipCommit <approved-source-sha>

pwsh -NoProfile -File .agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1 `
  -Mode Generate -RepositoryRoot <absolute repository root> `
  -BaseCommit <current-primary-sha> -TipCommit <final-source-sha> -DateUtc YYYY-MM-DD `
  -OutputDirectory <new directory beneath RepositoryRoot/Temp>
```

`-BaseCommit` and `-TipCommit` are full lowercase commit identities in every production call. When
`-BaseCommit` is supplied, the source JSONL is read as the exact raw blob at
`<BaseCommit>:.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl`;
the working-tree file is not consulted. A no-base invocation is retained only for standalone
fixture mode and compares the current working tree (including staged changes) with `HEAD`. With
both commits, the patch classifier uses the Git diff between those commits. Generate requires `-DateUtc` exactly in UTC
`YYYY-MM-DD` form and rejects an earlier table date. Contract may receive the date to perform the same date check,
but does not approve an output date or output hash.

## Source table and decision

The source `CodeQualityMetricsHistory.jsonl` must begin with exactly 133323 bytes, 648 LF lines,
and SHA-256 `5a39debf4be41abebd8496b9f25ee4023d109813788e95b30da8f74474fe75ed`. Legacy rows are
validated for contiguous indices 0–646, lowercase SHA identities, finite metric values in [0,1],
and `0 <= parsed <= supported`. A live suffix starts at index 647 and contains exactly
`index,date,captureMode,verbosity,structuralErosion,supported,parsed`; dates are nondecreasing
calendar dates, so consecutive points may share one UTC day. Metrics are finite in [0,1], counts are non-negative with parsed no greater than
supported, and no suffix row contains a SHA. The first suffix row must be `catch-up`.

The patch classifier considers only `.h` and `.cpp` paths, including add/delete/modify/rename
records. A header beneath contiguous `Data/Shaders` components is excluded as pure GLSL except
`ShaderLayouts.h` and `ShaderLayoutsBase.h`. `.agents`, `.claude`, `ThirdParty`, `Temp`, other
extensions, and pure GLSL do not force a capture. The decision is:

| Existing live suffix | Metric-supported patch | Capture mode |
| --- | --- | --- |
| none | either | `catch-up` (one current Snapshot) |
| present | yes | `cpp-change` (one current Snapshot) |
| present | no | `carry-forward` (copy the previous effective values) |

No Git-history backfill is performed. The point index is always the previous table row plus one.
Carry-forward performs no Python probe, bootstrap/cache access, scb source resolution, or analyzer
run.

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
consumption; every other ignored/untracked/reparse/symlink extra blocks the capture. Generate takes
the BootstrapIdentity and manifest before Snapshot and again afterward; any identity, membership,
or source drift fails the run. Contract exposes a frozen `generator.sha256` separately from the
optional active `capture.digest`.

## Receipt schemas

Contract emits `broken-engine-code-quality-history-contract/v1` with construction order:

`schemaVersion,mode,source,prefix,series,patch,decision,generator,capture,snapshot`.

`source` contains only base/tip commit identities. `prefix` contains `bytes,lines,sha256`.
`series` contains `rows,liveRows,lastIndex,lastDate,historyBytesSha256`, where the final field is
the SHA-256 of the complete immutable source JSONL bytes (including the live suffix). `patch` contains the normalized change rows,
the count of metric-supported changes, and `cppChanged`. `decision` contains `captureMode`,
`reason`, and `forceSnapshot`. `generator` contains its repository-relative script path and digest.
`capture` is null for carry-forward; otherwise it contains the path-free BootstrapIdentity digest,
scb content digest, manifest digest, manifest, and combined capture digest. `snapshot` is null for
carry-forward and otherwise records the fixed `Engine/Source` recursive Snapshot and that coverage
is required.

Generate emits `broken-engine-code-quality-history-update/v1` with construction order:

`schemaVersion,mode,date,captureMode,source,prefix,patch,generator,capture,series,outputs`.

`series` contains the new index, final JSONL SHA-256, immutable source `historyBytesSha256`, self-contained row, and Snapshot coverage
(`corpusCounts` and `targetCounts`) or null for carry-forward. `outputs.jsonl` and `outputs.svg`
each contain the repository-relative path, byte count, and SHA-256. The files are named
`CodeQualityMetricsHistory.jsonl` and `CodeQualityMetricsHistory.svg`; JSONL is the exact validated
source bytes plus one canonical live row. SVG is UTF-8 LF without BOM, fixed at 1800x1150, has no
timestamps or machine paths, and embeds series/generator/runtime/scb digests when capture applies.
