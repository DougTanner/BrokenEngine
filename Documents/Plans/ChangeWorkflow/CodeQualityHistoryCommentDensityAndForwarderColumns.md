<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-08T22:10:18.447Z","dependsOn":["Documents/Plans/ChangeWorkflow/CodeQualityHistoryRemeasurePreBoundaryRows.md"]} -->
# Add commentDensity and sameNameForwarders columns to the code-quality history

## Context

This Plan assumes two earlier changes have landed. The first is the one that
completed `CodeQualityHistoryOneRowPerCommit` (2026-10-08): it made the tracked
table `.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl`
schema `code-quality-metrics-history/v2`, one row per commit keyed by `sha` and
`%cI` `date`, lag-by-one. It also added `Invoke-CodeQualityMetricsHistory.ps1`'s
`Rebuild -SeedPath` mode and the commit Snapshot
(`Invoke-CodeQualityMetrics.ps1 -Mode Snapshot -Commit <sha>`). The second is the
dependency `CodeQualityHistoryRemeasurePreBoundaryRows.md`. It added the
`Rebuild` inputs `-RemeasureFrom`/`-RemeasureBefore` (a forced re-measure range)
and `-CacheDirectory` (a per-commit evidence cache bound to the table schema,
the BootstrapIdentity, the capture manifest, and the analyzer files), and it
re-measured the 191 rows that an older analyzer had measured. After both, every
row after the root's row holds today's analyzer values. Rows 0-646 (the 646
archive rows, then row 646 for this repository's root `e571f6f1`) keep their
stored values. The behavior is owned by
`.agents/skills/code-quality-metrics/references/HistoryContract.md`.

The analyzer has reported `commentDensity` (a ratio) and `sameNameForwarders` (a
count) in every Snapshot's `current.corpusMetrics` since `4137bd70`
(`references/MetricContract.md` `## Output`). The history table does not store
them. The user split them from the one-row-per-commit change into a later Plan
of their own (2026-10-08).

Spot values from the analyzer at `4137bd70` (corpus values: verbosity /
structuralErosion / supported / parsed, then commentDensity and
sameNameForwarders):

| Commit | Four stored-column values | commentDensity | sameNameForwarders |
|---|---|---|---|
| `f0bb81fa` (archive row 0) | 0.033096460061 / 0.689234281971 / 172 / 166 (equal to stored) | 0.031467460283 | 1 |
| `94ee3b01` (archive row 323) | 0.048249027237 / 0.579541100766 / 382 / 381 (stored 0.04825036827 / 0.579544715233) | 0.063861491521 | 30 |
| `e571f6f1` (root) | 0.03843218075 / 0.557880533969 / 513 / 513 (stored 0.038432914146 / 0.557882659401) | 0.09662116041 | 62 |
| `79d9606a` | 0.037726040622 / 0.53159185919 / 525 / 525 | 0.097255231421 | 60 |
| `a98e2fa1` | 0.037454936217 / 0.567489134965 / 555 / 555 | 0.089506077382 | 69 |
| `a2fc386f` | 0.037046042691 / 0.577896274045 / 556 / 556 | 0.070173230989 | 17 |
| `4fe69ffc` | 0.037041539316 / 0.577816172319 / 556 / 556 | 0.0701958564 | 17 |

The archive commits resolve only in the archive clone
`C:\Users\dougt\Documents\BrokenEnginePublic` (read-only), where all 646 archive
SHAs resolve. In this repository, `f0bb81fa` and `e571f6f1` resolve, and so does
`94ee3b01`, which a 2026-10-08 spot check fetched. Fetching that one commit grew
the shared object store's packs from 1.70 GiB to 2.27 GiB. Of archive rows
0-645, 531 are `measured:true`.

## Design

Author's recommendations, each with its rationale.

### Table format v3

- Header `{"schema":"code-quality-metrics-history/v3"}`. The rows are
  `index,sha,date,measured,verbosity,structuralErosion,commentDensity,sameNameForwarders,supported,parsed`.
- `commentDensity` is a finite ratio in [0,1] through `Get-NumberText`.
  `sameNameForwarders` is a non-negative integer. Both take
  `current.corpusMetrics.<name>.value`, and a measurement whose value is not
  applicable fails.
- v2 tables, seeds, and caches are rejected. This follows the
  no-backward-compatibility directive.
- Every row carries both columns, archive rows included, so the format has no
  nullable column. Alternative considered: start the columns at the root and
  leave archive rows without them. That needs a nullable column and a chart gap
  for most of the series.

### One run in which no seed value survives

- `Rebuild` gains `-RemeasureAll`, which cannot be combined with the range. With
  it, every row the seed would otherwise supply is measured instead:
  - each pre-root seed row with `measured:true`, from the archive clone;
  - the root;
  - every `cpp-change` commit.
  Carried rows take their predecessor's values as today, so the receipt's
  `reused` is 0.
- Archive rows need `-ArchiveRepository <clone>` on `Rebuild` and
  `-SourceRepository <path>` on the commit Snapshot (allowed only with
  `-Commit`). `baseline()` reads the corpus from that repository, while the
  analyzer, bootstrap, and capture root stay under `RepositoryRoot`. Archive
  rows keep their stored `sha`, `date`, and `measured`, and the classifier is
  never applied across archive branches.
- Alternative considered: fetch every archive commit into the shared object
  store. One fetch added about 0.57 GiB, and the store is shared by every
  worktree.
- Each row takes all six values from its fresh Snapshot, so one analyzer
  measures the whole row. The analyzer runs once per measured commit either
  way, to obtain the two new columns.
- Alternative considered: keep the stored four values and add only the two new
  columns. That mixes analyzers within a row and needs a merge path in the row
  builder.
- Consequence: rows 0-646 change slightly where today's analyzer differs from
  the one that stored them (`## Context` table). Every later row's four values
  are expected to be reproduced exactly.
- Seed: a one-time, untracked `Temp` script writes the v2 table as a v3 seed,
  with both new columns set to 0. This follows the v2 migration's precedent,
  where landed code reads only the current format. No placeholder survives,
  because `-RemeasureAll` takes no seed values. Acceptance criterion 2 checks
  this.
- The rerun after the shut-down signal uses the same `-CacheDirectory`. The
  cache's evidence gains the two values, and its schema binding becomes v3.
  The rebuild receipt records `remeasureAll` in `source` and bumps its version.

### Generate and chart

- `Get-SnapshotEvidence`, `New-Row`, and the v3 reader carry the two columns.
  `Generate`'s update receipt embeds rows, so it bumps
  `broken-engine-code-quality-history-update/v2` to `/v3`. The Contract receipt's
  shape is unchanged.
- The SVG adds one 230-pixel panel per new column after `structural erosion`
  (verbosity, structural erosion, comment density, same-name forwarders, then
  supported files). Each new column is scaled to its series maximum like
  `supported`, and the size becomes 1800x1610. The landing's reserved pair
  stays one JSONL and one SVG.

### Hand landing

`/finalize-changes` cannot land a replaced table (`history.source-changed`), and
the landing's own `Generate` at the old tip would read v2. Main lands the change
by hand:

1. Commit the session change S (scripts, docs, and the claimed Plan's deletion
   that `/next-plan` completion reports) on the live primary tip P.
2. From the session worktree at S, write the v3 seed from P's table. Run in the
   background `Rebuild -BaseCommit P -SeedPath <seed> -RemeasureAll
   -ArchiveRepository '<archive clone>'` with a new `-CacheDirectory` and a new
   `-OutputDirectory`. Record that HEAD as the Rebuild commit R. The run makes
   about 531 + 1 + the C++ commit count (232 at `87122d01`) commit Snapshots, at
   about 1.5-2 minutes each (unvalidated): about 19-26 hours. Resume an
   interrupted run by rerunning with the same cache.
3. Run acceptance criteria 1-6 on the output. Then dispatch a fresh `reviewer`
   `/verify-acceptance` over the criteria whose evidence exists by then (1-7
   and 10). Criteria 8, 9, and 11 run only from step 6 on.
4. Wait until the user says every other session is landed or discarded and the
   other agents are shut down.
5. If primary moved to P', rebase S onto P'. Stop and ask if any commit in
   P..P' changed an analyzer file, `ThirdParty/scb-check`, or `finalize-changes`.
   Rewrite the seed from P''s table and rerun `Rebuild -BaseCommit P'` with the
   same cache. Only C++ commits in P..P' are measured. R becomes the rebased
   HEAD. Repeat criteria 1-6, then have a fresh `reviewer` `/verify-acceptance`
   check the rebuilt table again before landing. That re-check mirrors the
   user's decision for the dependency's landing.
6. Mint the landing owner token with WorktreeCli `lock token`. Claim the
   landing lock with `Invoke-FinalizeLockClaim.ps1 -LandingOwner '<token>'`
   (default lease, host timeout at least 360,000 ms). Build L as S's tree plus
   the rebuilt pair, with S's metadata and the primary tip as sole parent. Run
   criteria 8 and 9; if any fails, release with `-Release` and return to step 5.
   Then run `git -C '<primary>' merge --ff-only L`. Move the session branch to L
   with `git -C '<session worktree>' reset --hard L`, as
   `Invoke-FinalizeLanding.ps1` does after its fast-forward. Release with
   `-Release`, then run WorktreeCli `plan unclaim` the way that script's
   `Complete-LandedState` does.
7. Run criterion 11 on L.

## Critical files

- `.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1`
- `.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetrics.ps1`
- `.agents/skills/code-quality-metrics/scripts/Analyze-CodeQualityMetrics.py`
- `.agents/skills/code-quality-metrics/references/HistoryContract.md`
- `.agents/skills/code-quality-metrics/references/MetricContract.md`
- `.agents/skills/code-quality-metrics/references/history/README.md`
- `.agents/skills/code-quality-metrics/SKILL.md`
- `.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl` and `.svg` (rebuilt, hand-landed)

## In scope

- `Invoke-CodeQualityMetricsHistory.ps1`:
  - the v3 reader, validator, and row builder;
  - `Get-SnapshotEvidence`;
  - the `-RemeasureAll` and `-ArchiveRepository` parameters and their archive
    measurement path in `Invoke-Rebuild`;
  - the cache evidence and schema binding;
  - the update and rebuild receipt versions;
  - `New-HistorySvg` panels.
- `Invoke-CodeQualityMetrics.ps1`: `-SourceRepository` (only with `-Commit`), its
  validation, and its request field.
- `Analyze-CodeQualityMetrics.py`: in `build`, the source-repository request
  field passed to `baseline()`.
- Docs: `references/HistoryContract.md` (`## Invocation`, `## Source table and
  decision`, `## Capture and drift`, `## Receipt schemas`);
  `references/MetricContract.md` `## Input` (`-SourceRepository`);
  `references/history/README.md` (v3 columns; archive rows measured from the
  archive clone); `SKILL.md` `## Inputs`; `references/worker.md` `### History`
  where it names Rebuild inputs.
- One-time, untracked: the v2-to-v3 seed script under `Temp/`, never committed.
- The rebuilt tracked pair, landed by hand per `### Hand landing`.

## Out of scope

- Metric definitions, pointer lists, digest hints, churn, and any other history
  column.
- `/finalize-changes` scripts and references, and any migration route inside the
  landing flow. The Generate interface and the per-commit budget are unchanged.
- Writing to the archive clone, and every other repository.
- Running `Rebuild` in parallel, or speeding up `baseline()`.

## Risk tier and invariants

Tier 3. Trigger: the tracked history format changes (v2 to v3), together with
the producer every landing runs. The table is also replaced outside the
landing flow under a briefly held global landing lock, a coordination step that
can block other sessions.

Invariants: lag-by-one and one row per commit are unchanged. `Generate` output
stays a pure function of `BaseCommit` for a fixed BootstrapIdentity. Old
formats (v2 tables, seeds, caches, and receipts) are rejected. Archive commits
are read only.

## Acceptance criteria

Rows are matched by `sha`.

1. Before step 2, a reproduction check: one direct commit Snapshot of
   `4fe69ffc163c85fccbfabae67cfb2a0badf128eb` reports exactly the `## Context`
   values for all six columns. A difference means the analyzer changed since
   these values were taken: stop and ask the user before the long run.
2. The output table is v3, with the seed's row count plus one row for P when
   the seed lacks it. `index`, `sha`, `date`, and `measured` equal the v2 table's.
   The receipt reports `reused` 0, and `measured` equals the count of
   `measured:true` rows.
3. Every row after the root's row has `verbosity`, `structuralErosion`,
   `supported`, and `parsed` equal to the v2 table's exactly.
4. Each carried row equals its predecessor's six values.
5. The rows of `f0bb81fa`, `94ee3b01`, `e571f6f1`, `79d9606a`, `a98e2fa1`,
   `a2fc386f`, and `4fe69ffc` equal the `## Context` table exactly.
6. A rerun with the same inputs and cache measures nothing new and writes a
   byte-identical pair. The SVG is `width="1800" height="1610"`, with five
   polylines of one point per row.
7. `pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1
   -RepositoryRoot '<worktree root>' -Baseline <baseline SHA>`: every triggered
   row passes.
8. Before the fast-forward:
   - `git rev-list --parents -n 1 L` shows the primary tip as sole parent.
   - `git diff --name-only S L` lists exactly the pair.
   - `git diff --name-only <primary tip> L` lists exactly S's planned paths
     plus the pair.
   - Author, committer, dates, and message equal S's.
   - `git -C '<primary>' status --porcelain` is empty.
   - The last row of L's table has `sha` equal to L's first parent.
   - `git diff --quiet R L --` over `Analyze-CodeQualityMetrics.py`,
     `normalize_code_quality_metrics_cpp.py`, `Invoke-CodeQualityMetrics.ps1`,
     `Invoke-CodeQualityMetricsHistory.ps1`, and `ThirdParty/scb-check` exits 0.

   Any failure means step 6's release and a return to step 5.
9. Immediately before `merge --ff-only`, `git -C '<primary>' symbolic-ref HEAD`
   prints `refs/heads/main`. Anything else stops the landing.
10. Code reading:
    - v2 tables, seeds, caches, and receipts are rejected.
    - `-SourceRepository` is refused without `-Commit`.
    - `-RemeasureAll` is refused with the range or without `-ArchiveRepository`.
    - `Generate` stores both columns from its commit Snapshot.
11. After landing, `Contract -BaseCommit <L>` exits 0. `Generate -BaseCommit L`
    into a new `Temp` child appends exactly one v3 row: `sha` L, `measured:false`,
    with the table's last row's six values. Its receipt is
    `broken-engine-code-quality-history-update/v3`. The real observation is the
    first ordinary landing after L.

## Notes

- `/external-skill-creator` validation applies, because the
  `code-quality-metrics` package changes. There is no C++ or GLSL change and no
  build.
- The user's rule from 2026-10-08 is never to re-measure stored values that
  today's analyzer reproduces. This run still runs the analyzer on those
  commits, because the two new columns exist nowhere else. Their four stored
  values come out identical (criterion 3). Confirm this reading with the user at
  claim time.
