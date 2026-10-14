<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-13T13:25:33.378Z","dependsOn":[]} -->
# Split Code-Quality History Verbosity by Area and Drop Structural Erosion

## Context

- Recorded user direction, as relayed in the authoring brief: the history keeps overall `verbosity` and adds one verbosity value per history area — Engine = `Engine/` minus `Engine/Source/Agent/`; Common = `Common/`; DataPacker = `DataPacker/`; Game = `Projects/` minus `Projects/BrokenEngineSandbox/Source/Agent/`; AgentTools = `Tools/` plus both `Agent/` folders. `structuralErosion` leaves the history and the Snapshot report everywhere. `/external-deep-analysis` takes the existing `extremeComplexityFunctions` hint as a "look at this function" signal, never "split this function". The table is converted once and lands by a manual commit, as `9c746fa0` did. No backward compatibility.
- The user's words on the back-fill: "We'll need to re-measure the entire history (including BrokenEnginePublic), BUT keep the overall measures we've already done if their commit hash still exists, just re-measure the new areas, but for squashed commits we may need to also re-measure the overall. Note: there should now be graphs for each area. and we are completely dropping the erosion-over-time so that can be removed during this pass".
- Why: `Documents/Investigations/ChangeWorkflow/StructuralErosionDeepDive.md` `## Recommendation` and `## Root-cause verdicts` (e): the corpus erosion ratio is a lumpy summary of a few dozen functions, and the per-function mass ranking is the useful signal; `## Verbosity comparison`: verbosity is the steadier series. Agent code holds 26% of the high-complexity mass at row 1164 and drove 56% of one period's rise, so per-area series stop agent growth masking runtime trends.
- The area split needs no new analyzer output: a commit Snapshot's `current.files[].metrics.verbosity` numerators and denominators sum exactly to `current.corpusMetrics.verbosity` (checked at HEAD: 2026/56881 both ways); `Analyze-CodeQualityMetrics.py` `aggregate` builds corpus metrics as those sums.
- History mechanics today: `.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1` has only `Contract` and `Generate` (the earlier `Rebuild` mode was removed in `3727351a`); it validates the exact v2 header and eight row fields (`$script:RowFields`, `Read-History`), measures through `Measure-Commit`/`Get-SnapshotEvidence`, builds rows in `New-Row`, and draws two bands in `New-HistorySvg`. `/finalize-changes` runs Generate at every landing with the session worktree's generator (`Invoke-FinalizeLanding.ps1` history step) and refuses a session patch that touches the tracked JSONL/SVG (`history.source-changed`), so a converted table cannot land through it.
- Row resolvability on the authoring machine (2026-10-13; `git cat-file --batch-check` over every row `sha`): the table has rows 0-1169. Rows 0-648 resolve in the local clone of the old `BrokenEnginePublic` repository (row 602 only as an object no ref reaches); rows 0-323 and 646-1169 resolve in this repository's object store; rows 324-645 resolve only in that archive. Rows 1141-1159 are commits squashed into `3c3cfce5` (row 1160) and resolve only as objects no ref reaches. Archive commits carry the same top-level roots (`Common`, `DataPacker`, `Engine/Source`, `Projects`, and from later rows `Tools`). 768 rows are measured (531 in 0-645, 224 in 646-1140, 13 from 1141); every carried row's values equal its predecessor's.
- Which analyzer measured each row: `9c746fa0` last changed the analyzer (row 1139) and `1e9e821d` re-measured rows 724-1138 with it (deep dive `## Computation`); rows 0-723 came from older analyzers, and rows 0-503 carry parse omissions.

## Design

Author's recommendations, with rationale:

### Row format v3

- Header `{"schema":"code-quality-metrics-history/v3"}`; rows `index,sha,date,measured,verbosity,verbosityEngine,verbosityCommon,verbosityDataPacker,verbosityGame,verbosityAgentTools,supported,parsed`. The exact-header check then rejects v2 with no extra code. Flat fields keep rows parallel to today's shape.
- An area value is the sum of verbosity numerators over that area's `current.files[]` rows divided by the sum of their denominators, rounded like the other ratios (`Get-NumberText`). An area with a zero denominator stores `null` (AgentTools has no files before `Tools/` and the `Agent/` folders exist); overall `verbosity` stays non-null. A file outside the five roots counts only toward overall `verbosity`.
- The five definitions live once in `HistoryContract.md`, under the name "history area" so they are not confused with the Snapshot's own `areas` rows. The generator matches each POSIX `path` by ordinal prefix, checking the two `Agent/` folders before `Engine/` and `Projects/`.
- Generate receipt `broken-engine-code-quality-history-update/v3` carries the row, so it becomes `/v4`. The Contract receipt carries no row values and keeps `/v3`.
- SVG: the structural-erosion band goes; six stacked bands of equal height inside today's plot area (y 230-2040) — overall verbosity, then Engine, Common, DataPacker, Game, AgentTools — each built by the existing per-band code from its own non-null values (range, five labels, grid, polyline; a null point is skipped).

### Snapshot `structuralErosion` removal

- `Analyze-CodeQualityMetrics.py`: drop `structuralErosion` from the file/area/corpus/target metric maps, the `empty` map, the outlier loop (outlier buckets become verbosity-only), and the comparison delta names. Keep `mass` on function rows and `highComplexityFunctions` (CC above ten), which the hint uses. `SCHEMA` becomes `broken-engine-code-quality-metrics/v4`, and the generator's `Get-SnapshotEvidence` check follows.
- `Invoke-CodeQualityMetrics.ps1`: the digest content changes with the report, so `broken-engine-code-quality-evidence/v3` becomes `/v4`.
- `MetricContract.md` `## Output`: both versions; outliers name only `verbosity`; the erosion definition goes while the `mass` definition (`CC * sqrt(SLOC)`) moves to `highComplexityFunctions`; "All five metrics" becomes four. `references/worker.md` names the `/v4` digest.
- `Remediation.md`: the "For erosion evidence ... decomposition" sentence and the closing "structural erosion alone is navigation" clause go; the sentences on source inspection and advisory metrics stay.

### `/external-deep-analysis`

- Step 6 and `## Handoff` keep only the target and corpus `verbosity` and `excessDecisions` values (four). The `#### Triage Gate` (steps 12-16), the `## Purpose` clause naming it, and the `triage skip` paragraph in `## Handoff` go, because their only trigger is a `structuralErosion` outlier; later steps renumber. `## Rules` keeps "`CC > 30` remains navigation only" without the erosion clause.
- The hint-treatment step (today 23) names `hints.extremeComplexityFunctions` as the target's worst-offending functions by complexity mass: an entry is a reason to read that function, never a reason to split it. Caveat beside it: CC is inflated by counted `&&`/`||`, the one-`if`-per-guard style of `Documents/C++StyleGuide.txt` rule 62, both client/server `#if` branches counted together, lambdas folded into the enclosing function, and per-check validation at trust boundaries; complexity is often earned (one cohesive dispatch, per-check rejections at a trust boundary, ordered setup sharing local state), and code that belongs together stays together. A finding names a specific unearned piece (a fused separate job, a copy-paste), never size alone. The hint's selection (CC above 30, by mass, top ten) and shape stay unchanged.

### One-time conversion

A temporary `Convert` mode in the generator produces the v3 pair; it is deleted before the session commit, so no landed code reads v2. Rationale: Generate cannot convert (backward-compatibility code), `/finalize-changes` cannot land a replaced table, and a temporary mode reuses `Measure-Commit`, `New-Row`, and `Write-HistoryPair` instead of a second implementation.

- Inputs: `-BaseCommit <primary tip P>`, `-OutputDirectory` and `-CacheDirectory` beneath `Temp`, and `-ArchiveRepository <local BrokenEnginePublic clone>`. Convert sets `GIT_ALTERNATE_OBJECT_DIRECTORIES` to that clone's object directory for its own process, so every Git call it and its child Snapshots make resolves archive commits without fetching into or changing either repository.
- Per v2 row of P's table, in order:
  - carried (`measured:false`): copy the previous converted row's values, as Generate's carry-forward does;
  - measured and its `sha` resolves: Snapshot that commit; take the five area values from it; keep the stored `verbosity`, `supported`, and `parsed` (the user's rule);
  - measured, at index 646 or later, and its `sha` resolves nowhere (a squashed commit whose objects were pruned): its commit is the squash result, the nearest later row whose `sha` is reachable from P (row 1160 `3c3cfce5` for rows 1141-1159); Snapshot that commit and take overall `verbosity`, `supported`, `parsed`, and the area values from it, so the row stays internally consistent. Rationale: the squash result is the only surviving tree carrying those commits' changes, and `HistoryContract.md` gives a squash result its own row;
  - measured, at index 645 or earlier, and unresolvable: fail naming the row, so an absent archive never flattens the archive trend onto `e571f6f1`.
- Then it appends P's own row exactly as v3 Generate would, keeping lag-by-one.
- Cache: each Snapshot writes `<sha>.json` holding its corpus verbosity, counts, and five area numerator/denominator pairs; `cache.json` binds the BootstrapIdentity digest, the capture manifest digest, and the SHA-256 of `Analyze-CodeQualityMetrics.py`, `normalize_code_quality_metrics_cpp.py`, and `Invoke-CodeQualityMetrics.ps1`. A rerun reuses matching entries and refuses a mismatched cache. Rationale: about 768 Snapshots (unvalidated estimate 1.5-2 min each from the `9c746fa0` precedent, archive commits smaller) must survive interruption and a moved primary tip.
- Receipt: rows kept, rows re-measured from a squash result, cached count, and the indexes of kept-overall rows whose Snapshot `corpusMetrics.verbosity` differs from the stored value.
- Kept overall and mismatch: rows 0-723 keep overall values from older analyzers while their area values come from the current one, so their area values will not combine to their stored overall; that is accepted under the user's rule and documented in `history/README.md`. Rows from 724 on are expected to match.
- Alternative considered: Snapshot every carried row too (about 1170 Snapshots). It adds about 400 runs to produce values the carry rule already defines as equal.

### Order and hand landing

1. Implement every change above, and finish the reviews of the analyzer and generator edits before the long run, because the cache binds the analyzer digests.
2. Add `Convert`, run it in the background at the live primary tip P, then run acceptance criteria 2-4 on the output.
3. Wait until the user says every other session has landed or been discarded: a session still holding the v2 generator would fail its next Generate on the v3 header. If primary moved to P', rebase, stop and ask if P..P' changed an analyzer file or `ThirdParty/scb-check`, and rerun `Convert` at P' with the same cache.
4. Delete `Convert`, commit the session change S, and hand-land S's tree plus the converted pair on the primary tip under the landing lock, following the steps of `git show 9c746fa0:Documents/Plans/ChangeWorkflow/CodeQualityHistoryRemeasurePreBoundaryRows.md` `### Hand landing` step 6 (lock token, `Invoke-FinalizeLockClaim.ps1`, `merge --ff-only`, session branch reset, release, claim release).

## Critical files

- `.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetricsHistory.ps1`
- `.agents/skills/code-quality-metrics/scripts/Analyze-CodeQualityMetrics.py`
- `.agents/skills/code-quality-metrics/scripts/Invoke-CodeQualityMetrics.ps1`
- `.agents/skills/code-quality-metrics/references/HistoryContract.md`, `MetricContract.md`, `Remediation.md`, `worker.md`
- `.agents/skills/code-quality-metrics/references/history/README.md`, `CodeQualityMetricsHistory.jsonl`, `CodeQualityMetricsHistory.svg`
- `.agents/skills/external-deep-analysis/SKILL.md`

## In scope

- Generator: `$script:HeaderText`, `$script:RowFields`, `Read-History` row checks (nullable area fields), `Get-SnapshotEvidence` (schema `/v4`, area sums), `New-Row`, `New-HistorySvg` bands and grid lines, Generate receipt `schemaVersion`; the temporary `Convert` mode, added and deleted within the session.
- Analyzer: `metrics` return map, `aggregate` names, `outlier_buckets`, the `empty` map, comparison delta names, `SCHEMA`.
- `Invoke-CodeQualityMetrics.ps1` digest `schemaVersion`.
- `HistoryContract.md` `## Source table and decision` (header, row fields, ratio rule, the history area definitions), `## Capture and drift` (what a row stores), `## Receipt schemas` (Generate version, SVG paragraph).
- `MetricContract.md` `## Output` as listed in the design; `worker.md` digest version; `Remediation.md` erosion text.
- `history/README.md`: row-format version and one statement that area values were measured once by the v3 conversion while overall values keep their original analyzers (rows 0-723 older ones).
- The converted `CodeQualityMetricsHistory.jsonl` and `.svg`, landed by hand.
- `/external-deep-analysis` `SKILL.md`: `## Purpose`, step 6, the Triage Gate removal and renumbering, the hint-treatment step, `## Handoff`, `## Rules`.

## Out of scope

- `/finalize-changes` scripts and references, WorktreeCli, and `ThirdParty/scb-check`.
- The Snapshot's `areas` labels, every remaining metric formula, `highComplexityFunctions`, and the `extremeComplexityFunctions` selection, ordering, and shape.
- Creating refs to protect unreferenced commits, and fetching archive objects into this repository.
- `Documents/Investigations/ChangeWorkflow/StructuralErosionDeepDive.md` and `/external-refactor-clean`.

## Acceptance criteria

1. Contract and Generate reject a table with the v2 header; Generate over the landed v3 table appends one v3 row whose area values equal the per-area sums of that Snapshot's `files` rows and whose `verbosity` equals `corpusMetrics.verbosity`.
2. The converted table has the source table's rows with unchanged `index`, `sha`, `date`, and `measured`, plus one row for the conversion's base commit; every row whose `sha` resolved keeps its stored `verbosity`, `supported`, and `parsed`; no row has `structuralErosion`.
3. Fresh Snapshots of the measured rows 0, 645, 699, 1150, and the newest measured row reproduce their stored area values.
4. The SVG has no structural-erosion band and draws the overall and five area bands.
5. A Snapshot report is `broken-engine-code-quality-metrics/v4` with no `structuralErosion` key; a `-Digest` run reports `broken-engine-code-quality-evidence/v4`.
6. `git grep -i -e structuralErosion -e "structural erosion" -e structural-erosion` outside `Documents/Investigations` finds nothing, and the landed generator has no `Convert` mode.
7. The first `/finalize-changes` landing after this change appends a v3 row.

## Notes

- Change Workflow Tier 3. Triggers: a serialization/data-layout change to the tracked history table and the Snapshot report and digest formats, and build/bootstrap coordination that can block other sessions — every landing runs this generator, and a v2 generator meets the v3 table after the hand landing.
- No determinism/CRC, `.pack`, wire, replay, threading, allocation, shader, or C++ build exposure.
- On the authoring machine the archive clone is `C:\Users\dougt\Documents\BrokenEnginePublic` (remotes `old-public` and `old-public2`, branches `1.0.0`, `2.0.0`, `2.1.0`).
- Rows 1141-1159 (and archive row 602) depend on objects no ref reaches; a `git gc` prune before the run sends rows 1141-1159 to the squash-result rule.

## Status

- Deferred on 2026-10-13 by user direction after implementation and review; nothing of it has landed.
- Decisions the user made in that run: the drafted approach over the three alternatives `/plan-alternatives` presented; squashed rows 1141-1148, 1150, 1151, and 1153 are measured from the commit that actually landed (`3c3cfce5`, row 1160), overall and areas, keeping their position and commit ID; the user holds off `/cleanup-worktrees` until this lands; main lands the converted table by hand following the precedent (no `/finalize-changes`), and before that landing the user shuts down all other agent sessions.
- Decided in that run's review, not by the user: Convert lists rows whose area denominators do not sum to the corpus denominator in its receipt instead of stopping.
- The resolved plan (corrections C1-C17 to this Plan's statements, the execution card with acceptance checks 1-7, and slices S1-S4) and the reviewed implementation are saved on the authoring machine only, under `C:\Users\dougt\Documents\BrokenEngine\Temp\CodeQualityHistoryAreaVerbosity\` (`preparation.md`, `implementation.patch`, `ConvertCache/`, listed in its `README.md`); they are not tracked. `/cleanup-worktrees` may delete files older than 48 hours in the primary `Temp` folder.
- Resume: claim this Plan, apply `implementation.patch`, rerun the static checks, copy `ConvertCache/` into the worktree's `Temp`, and run Convert from a separate terminal window (Claude Code ends idle background commands under memory pressure; one run was stopped that way after 6 Snapshots). Measured cost: 73-108 s per Snapshot for about 757 Snapshots, about 15-18 h. A stopped run resumes from the cache with a new `-OutputDirectory`. Then run the acceptance checks and the hand landing in the saved card.
