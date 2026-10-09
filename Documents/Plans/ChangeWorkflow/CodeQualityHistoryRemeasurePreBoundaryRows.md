<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-08T22:10:14.263Z","dependsOn":[]} -->
# Re-measure the pre-boundary code-quality history rows with the current analyzer

## Context

This Plan assumes the change that completed
`CodeQualityHistoryOneRowPerCommit` (2026-10-08) has landed: the tracked
`.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl`
is schema `code-quality-metrics-history/v2`, one row per commit keyed by `sha`
and `%cI` `date`, lag-by-one (the table in commit C holds no row for C);
`Invoke-CodeQualityMetricsHistory.ps1` has the `Contract`, `Generate`, and
`Rebuild -SeedPath` modes; and `Invoke-CodeQualityMetrics.ps1 -Mode Snapshot`
takes `-Commit <sha>` (a commit Snapshot read through `baseline()`). The
behavior is owned by
`.agents/skills/code-quality-metrics/references/HistoryContract.md`.

That change kept every stored measurement and measured only the C++ commits
that had no row. As a result, 191 rows still hold values from an older
analyzer. These are the C++ commits on `main`'s first-parent chain from
`48cebb3f42bde7b9c0bdd00dff5541520011b578` (inclusive; position 78 from the
root, where live rows began) up to
`8939b9371f8017c68c1f38e9bbb7aa1eac202140` (exclusive; position 501). A C++
commit here is one the history classifier counts as a metric-supported change
from its first parent. Of the 191, 169 added one v1 row and 22 added several,
and each of those 22 kept the last row it added. `8939b937` rewrote the
analyzer's normalizer (the parse-failure fix). Every stored row before it
omits at least one file. The rows from it on omit none, and today's analyzer
reproduces them exactly.

Spot-checks with the analyzer at `4137bd70` (the same verbosity,
structural-erosion, supported, and parsed computation as `8939b937`),
written as verbosity / structuralErosion / supported / parsed:

| Commit | Stored | Recomputed |
|---|---|---|
| `79d9606a48205d8dcc793cd14c47a2020630b235` | 0.037725614888 / 0.529379137643 / 525 / 524 | 0.037726040622 / 0.53159185919 / 525 / 525 |
| `a98e2fa17d971537a6158140ecaf68de8e1979a9` | 0.037455275329 / 0.566028094997 / 555 / 554 | 0.037454936217 / 0.567489134965 / 555 / 555 |
| `4fe69ffc163c85fccbfabae67cfb2a0badf128eb` (after the boundary) | 0.037041539316 / 0.577816172319 / 556 / 556 | identical |

Without this Plan, the chart's points for those 191 commits keep the older
analyzer's `parsed` and `structuralErosion`. For example, `structuralErosion`
is about 0.0015 to 0.0022 low at the two sampled commits, so the chart shows a
step at `8939b937` that is a tool change, not a code change.

Recorded user decisions (2026-10-08): the long re-measure is a separate
follow-up run, planned at 6-7 hours. Stored values that today's analyzer
reproduces are never re-measured. Before the shut-down signal, the user lands
or discards every other session. When the table is rebuilt again after that
signal, a fresh reviewer checks it again before landing.

## Design

### Rebuild additions

The current `Rebuild` takes a seed row for every C++ commit whose `sha` the seed
holds, and keeps no state between runs. The 191 rows are in the seed, so a
forced re-measure needs a new input. Commit Snapshots run at about 1.5-2 minutes
each (unvalidated estimate: about 60 s of analysis plus one `git show` per
supported file), so the run takes about 5-6.5 hours and needs to survive an
interruption and a moved primary tip. Author's recommendation, with the
alternative considered:

- Forced range: optional `-RemeasureFrom <sha>` and `-RemeasureBefore <sha>`.
  They must be given together. Each is a full lowercase SHA on `BaseCommit`'s
  first-parent chain, with `RemeasureFrom` before `RemeasureBefore`. A
  `cpp-change` commit at or after `RemeasureFrom` and before `RemeasureBefore`
  ignores its seed row and is measured. Every other commit keeps today's rule.
  Rationale: the 191 are exactly the C++ commits in one half-open range of
  durable SHAs. A list file would need its own copy of the classifier to
  produce. A single `-RemeasureBefore` would also re-measure the 32 rows at
  positions 4-77, which today's analyzer already measured.
- Resume: optional `-CacheDirectory <directory beneath RepositoryRoot/Temp>`.
  Each forced or seedless measurement writes `<sha>.json` with that commit's
  Snapshot evidence (the four row values and coverage) as soon as it finishes.
  `cache.json` binds the cache to the table schema
  (`code-quality-metrics-history/v2`), the BootstrapIdentity digest, the capture
  manifest digest, and the SHA-256 of `Analyze-CodeQualityMetrics.py`,
  `normalize_code_quality_metrics_cpp.py`, and `Invoke-CodeQualityMetrics.ps1`.
  A rerun takes cached evidence when all of them match and refuses the cache when
  any differs. Rationale: one cache covers both an interruption and the moved-tip
  rerun at landing. It is also the resumable mode the dependent new-columns Plan
  (`CodeQualityHistoryCommentDensityAndForwarderColumns.md`) reuses, and that
  Plan needs a run in which no seed value survives. Alternative considered:
  chunked runs, each using the previous output as the next seed, with no cache.
  That needs no new parameter beyond the range, but a forced-range rerun would
  measure the range again, and it cannot give the dependent Plan a single run
  that proves no seed value survived.
- Receipt: the inputs and the cache use change the rebuild receipt, so
  `broken-engine-code-quality-history-rebuild/v1` becomes `/v2`. `source` adds
  `remeasureFrom` and `remeasureBefore` (null when absent). `series` adds
  `cached`, the count of rows taken from the cache, which are also counted in
  `measured`. Per the no-backward-compatibility directive, `/v1` is not emitted.

`Generate` and `Contract` are unchanged. They share `New-CommitRow` and
`Measure-Commit` with `Rebuild`, so the cache must stay a `Rebuild`-only input.

### Hand landing

`/finalize-changes` cannot land a replaced table. A session patch that changes
the reserved JSONL/SVG pair is refused with `history.source-changed`. So this
change is landed by hand, the way the v2 migration was, and main runs every
step:

1. Commit the session change S (scripts, docs, and the claimed Plan's deletion
   that `/next-plan` completion reports) on the live primary tip P.
2. From the session worktree at S, run in the background `Rebuild -BaseCommit P`.
   Use the tracked table at S (equal to P's) as `-SeedPath`, add
   `-RemeasureFrom 48cebb3f42bde7b9c0bdd00dff5541520011b578`,
   `-RemeasureBefore 8939b9371f8017c68c1f38e9bbb7aa1eac202140`, a new
   `-CacheDirectory`, and a new `-OutputDirectory`. Record that HEAD as the
   Rebuild commit R (about 5-6.5 hours, 191 measurements, plus one for P when P
   changes C++).
3. Run acceptance criteria 1-6 on the output. Then dispatch a fresh `reviewer`
   `/verify-acceptance` over the criteria whose evidence exists by then (1-7
   and 10). Criteria 8, 9, and 11 run only from step 6 on.
4. Wait until the user says every other session is landed or discarded and the
   other agents are shut down.
5. If primary moved to P' (expected, since other sessions land during step 4),
   rebase S onto P'. Stop and ask if any commit in P..P' changed an analyzer
   file, `ThirdParty/scb-check`, or `finalize-changes`. Rerun `Rebuild
   -BaseCommit P'` with the rebased S's tracked table as the seed and the same
   range and `-CacheDirectory`. R becomes the rebased HEAD. Repeat criteria 1-6.
   Criterion 2's expectation becomes `cached` 191 and `measured` 191 plus any
   C++ commit in P..P' with no seed row. Then a fresh `reviewer`
   `/verify-acceptance` checks the rebuilt table again before landing.
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
- `.agents/skills/code-quality-metrics/references/HistoryContract.md`
- `.agents/skills/code-quality-metrics/SKILL.md`
- `.agents/skills/code-quality-metrics/references/history/CodeQualityMetricsHistory.jsonl` and `.svg` (rebuilt, hand-landed)

## In scope

- `Invoke-CodeQualityMetricsHistory.ps1`:
  - the `-RemeasureFrom`, `-RemeasureBefore`, and `-CacheDirectory` parameters
    and their validation (`Rebuild` only);
  - the forced-range rule in `Invoke-Rebuild`;
  - the cache read/write and its identity binding around the `Rebuild`
    measurement path;
  - the rebuild receipt `/v2` fields.
- `references/HistoryContract.md` `## Invocation`, `## Source table and
  decision` (the Rebuild rule), `## Capture and drift` (the cache binding), and
  `## Receipt schemas` (rebuild `/v2`); `SKILL.md` `## Inputs` (the optional
  Rebuild inputs).
- The rebuilt tracked pair, produced by `Rebuild` and landed by hand per
  `### Hand landing`.

## Out of scope

- Re-measuring any other stored row: archive rows 0-646 (row 646 is the root
  `e571f6f1`), the 32 C++ rows at positions 4-77, and every row from
  `8939b937`'s on.
- Measuring archive commits, or adding a source-repository input.
- `Generate`, `Contract`, the commit Snapshot, the analyzer, metric definitions,
  history columns (the new-columns Plan owns `commentDensity` and
  `sameNameForwarders`), and the SVG layout.
- `/finalize-changes` scripts and references, and any migration route inside the
  landing flow.
- Running `Rebuild` in parallel, or speeding up `baseline()`.

## Risk tier and invariants

Tier 3. Trigger: the tracked table every landing reads is replaced outside the
landing flow under a briefly held global landing lock. That build/landing
coordination can block other sessions. The change also edits the script whose
`Generate` every landing runs.

Invariants: the v2 row format and the lag-by-one rule are unchanged. `Generate`
output stays a pure function of `BaseCommit` for a fixed BootstrapIdentity.
`Rebuild` output is a pure function of `BaseCommit`, the seed bytes, and the
range, and cached evidence never changes it. A cache whose binding differs is
refused, never mixed.

## Acceptance criteria

Rows are matched by `sha`; v1 row numbers do not apply.

1. Before step 2, a reproduction check: one direct commit Snapshot
   (`Invoke-CodeQualityMetrics.ps1 -Mode Snapshot -Target Engine/Source -Scope
   Recursive -Commit 4fe69ffc163c85fccbfabae67cfb2a0badf128eb`) reports
   0.037041539316 / 0.577816172319 / 556 / 556 exactly. A different value means
   the analyzer changed after the migration, so the stored rows this Plan keeps
   are no longer today's values: stop and ask the user.
2. The output table is v2, with the seed's row count, plus one row for P when
   the seed lacks it. Every row's `index`, `sha`, `date`, and `measured` equal
   the seed's. The receipt reports `measured` 191 (plus P when P changes C++)
   and `reused` equal to the seed's other first-parent C++ rows plus the root.
3. Unchanged: archive rows 0-646, the 32 rows of C++ commits at positions 4-77,
   and every row from `8939b937`'s on are byte-identical to the seed's rows.
4. Each carried row (`measured:false`) equals its predecessor's values.
5. The rows of `79d9606a` and `a98e2fa1` equal the Recomputed values in
   `## Context` exactly.
6. A rerun with the same inputs and `-CacheDirectory` reports `measured` equal
   to `cached`, and writes a pair byte-identical (SHA-256) to the first run's.
   The SVG is `width="1800" height="1150"`, with three polylines of one point
   per row.
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

   Any failure means step 6's release and a return to step 5, whose Rebuild
   reruns.
9. Immediately before `merge --ff-only`, `git -C '<primary>' symbolic-ref HEAD`
   prints `refs/heads/main`. Anything else stops the landing.
10. Code reading: the range is refused unless both bounds are given, are on the
    chain, and are in order. The cache is refused on any binding mismatch.
    `Generate` and `Contract` accept none of the new parameters.
11. After landing, `Contract -BaseCommit <L>` exits 0. `Generate -BaseCommit L`
    into a new `Temp` child appends exactly one row: `sha` L, `measured:false`,
    with the values of the table's last row. The real observation is the first
    ordinary landing after L, which must add exactly that row.

## Notes

- `/external-skill-creator` validation applies, because the
  `code-quality-metrics` package changes. There is no C++ or GLSL change and no
  build.
- Positions count first-parent commits of `main` from the root (position 0).
  The 191 count and the range bounds hold while `main` is not rewritten before
  `8939b937`. If it is rewritten, recount with the classifier before step 2.
