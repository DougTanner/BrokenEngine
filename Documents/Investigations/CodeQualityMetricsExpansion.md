# Expanding the code-quality metrics beyond verbosity and erosion

Open question: which of SlopCodeBench's per-checkpoint quality metrics should
`/code-quality-metrics` also compute, and how does the tracked history table
grow to carry them across the whole pre-squash and post-squash history? The
benchmark's own comparison shows only two of its fifteen rate metrics separate
coding models sharply, but a metric that does not rank models can still show
this one codebase drifting over time, which is what the history table is for.
This document lists every candidate, says for each what data it needs and
whether the pinned `scb-check` already yields that data for C++, records where
each era of history can be recomputed from, compares the ways the history table
could carry new columns, and lists the decisions a Plan would have to make.
Nothing here is implemented.

The benchmark's `src/slop_code/metrics/` package emits 41 deterministic
code-quality keys per checkpoint, plus four LLM-rubric counts and three
checkpoint-to-checkpoint deltas. The rubric counts are graded by a language
model and are out of scope here. Of the 41, six come straight from
`scb-check`'s report (`verbosity`, `erosion`, `cloned_sloc_lines`,
`cloned_pct`, `verbosity_flagged_sloc_lines`, `verbosity_flagged_pct`). Of the
other 35, the two mass keys are a fold over the symbol list
(`checkpoint/mass.py`) and the two churn counts are a textual diff; the rest
come from the benchmark's own analyzers under `languages/python/`, the only
registered language. Those analyzers lint with ruff and walk Python ASTs for
symbols, waste, and the call graph, so none of them runs on C++ as-is; the
candidate table below covers every key except `verbosity` itself and says for
each what the C++ equivalent would take.

## What is measured today

`Invoke-CodeQualityMetrics.ps1` runs the pinned `ThirdParty/scb-check` over the
corpus and `Analyze-CodeQualityMetrics.py` reduces its output to three metrics,
defined in `.agents/skills/code-quality-metrics/references/MetricContract.md`:

- `verbosity` — union of clone-block lines over parsed SLOC. This is
  SlopCodeBench's `cloned_pct`. The benchmark's wider `verbosity_flagged_pct`
  adds ast-grep slop rules and structural rules, but `scb-check` applies those
  to Python only, so for C++ the two coincide.
- `structuralErosion` — mass of functions with cyclomatic complexity above ten
  over total mass, mass being `CC * sqrt(SLOC)`. This is SlopCodeBench's
  `erosion`, read straight from `scb-check`'s report; the benchmark also
  recomputes the same ratio from its own symbol list as `mass.high_cc_pct`
  (`checkpoint/mass.py`).
- `excessDecisions` — `sum(max(CC - 1, 0))` over functions. Local addition with
  no benchmark row.

The vendored `ThirdParty/scb-check` is version `0.2.0`; the benchmark pins
`scb-check==0.1.3` through `uvx` (`checkpoint/driver.py`,
`SCB_CHECK_VERSION`) because its rule set changes between releases. Any
cross-check of a local value against a benchmark number has to account for
that version gap.

The analyzer already holds, per function, `path`, `owner`, `name`,
`signature`, `startLine`, `endLine`, `sloc`, `cc`, and `mass`
(`Analyze-CodeQualityMetrics.py`, `capture`). Every complexity-distribution
and function-size metric below is a fold over that list and needs no new
`scb-check` output.

The history table `references/history/CodeQualityMetricsHistory.jsonl` opens
with a schema header line, then carries one row per first-parent commit. Legacy
rows hold `index,sha,date,cppChanging` plus `verbosity`, `structuralErosion`,
`supported`, and `parsed` only when `cppChanging` is true; live rows hold
`index,date,captureMode` plus those same four values on every row. Its
648-line prefix is immutable by the validator in
`Invoke-CodeQualityMetricsHistory.ps1` (`$script:PrefixLines = 648`, legacy
rows must carry a SHA, live rows must not), which `HistoryContract.md`
documents, so a new column cannot be added to the existing file without a
schema change; see the options below.

## Candidate metrics

Definitions were read from the benchmark source in `SprocketLab/slop-code-bench`,
paths relative to `src/slop_code/metrics/` (`checkpoint/extractors.py` for the
key list, `driver.py` for the function statistics, `checkpoint/mass.py` for
mass, `checkpoint/delta.py` for deltas, and `languages/python/graph.py` for
the graph metrics). "Source" says where the value would come from here. "C++"
says whether the pinned `scb-check` yields the input for C++ today.

| Metric | Definition | Source | C++ |
| --- | --- | --- | --- |
| `cc_max` | Highest CC in any function | fold over `_functions` | yes |
| `cc_mean` | Mean CC across functions | fold | yes |
| `cc_std` | Sample standard deviation of CC across functions | fold | yes |
| `cc_high_count`, `cc_extreme_count` | Count of functions with CC > 10 and CC > 30 | fold | yes |
| `high_cc_mean` | Mean CC over the functions with CC > 10 | fold | yes |
| `cc_top20` | Share of total CC held by the top 20% of functions by CC (`ceil(0.2 * n)` functions, zero-CC functions excluded) | fold | yes |
| `cc_concentration` | Gini coefficient of the CC distribution over functions with CC > 0 (0 uniform, 1 concentrated) | fold | yes |
| `cc_normalized` | `(Σcc² − n·mean²) / (max(Σcc, 50)² − n·mean²)`: 0 when every function has the same CC, 1 when all CC sits in one function, worst case floored at 50 | fold | yes |
| `high_cc_pct` (`erosion`, `mass.high_cc_pct`) | Mass share of functions with CC > 10 | already `structuralErosion` | yes |
| `mass.cc` | Total mass `Σ CC * sqrt(SLOC)` | fold | yes |
| `mean_func_loc`, `lines_per_symbol` | Both are the mean `lines` over functions and methods; the benchmark computes the same number twice (`_compute_distributions` and `FunctionStats.lines_mean`) | fold | yes |
| `cloned_pct`, `cloned_sloc_lines` | Clone SLOC over total SLOC, and the raw clone line count | already `verbosity` and its numerator | yes |
| `verbosity_flagged_pct`, `verbosity_flagged_sloc_lines` | Clone plus ast-grep plus structural flagged SLOC over total, and the raw count | equals `cloned_pct` for C++ because the rules are Python-only | degenerate |
| `max_nesting_depth` | Deepest block nesting in any function | `scb-check` computes nesting only inside its cognitive-complexity walk (`tree_walking/languages/generic.py`); not exported per function | export needed |
| `cognitive erosion` | Mass share with cognitive complexity > 10, `scb-check`'s own `cog_erosion` | per-function `cog_complexity` from `scb-check`, which the analyzer does not capture yet; not a benchmark row | capture needed |
| `functions`, `methods`, `classes`, `statements`, `symbols_total` | Symbol counts by kind | functions and methods are a fold; classes and statements need a tree walk | partial |
| `loc`, `sloc`, `total_lines`, `single_comments`, `files` | Line and file counts; the benchmark's `loc` is total lines and `sloc` is source lines | `supported`, `parsed`, and parsed SLOC already exist; comment lines need a line classifier | partial |
| `lines_added`, `lines_removed`, `delta.churn_ratio` | Lines added and removed since the previous checkpoint, and their sum over the previous total lines | Git diff between consecutive commits; only meaningful in the history table | new, cheap |
| `single_use_functions`, `trivial_wrappers`, `unused_variables` | Functions called exactly once, functions that only delegate to another call, variables assigned but never read | Python AST call and name resolution (`languages/python/waste.py`); C++ needs real call resolution | new tool |
| `lint_errors`, `lint_fixable`, `lint_per_loc` | ruff diagnostics, the fixable subset, and errors over `loc` | needs a linter | needs clang-tidy |
| `type_check` errors and warnings (per-file only, not one of the 41 keys) | ty diagnostics per file | Python-only; the C++ compiler already enforces this | no analogue |
| `graph_cyclic_dependency_mass` | Edge weight inside strongly connected components of size 2 or more, over total edge weight | needs the graph below | new extractor |
| `graph_propagation_cost` | Mean fraction of nodes reachable from each node in the transitive closure | same graph | new extractor |
| `graph_dependency_entropy` | Mean over nodes of the normalized Shannon entropy of each node's outgoing edge weights | same graph | new extractor |

The benchmark's graph is a function-level call graph: nodes are functions and
methods, edges are caller-to-callee with the call count as weight, built by
resolving Python call expressions (`build_dependency_graph`). It is not a
module import graph. A faithful C++ port needs resolved calls, which ast-grep
does not provide; an include graph over `#include "..."` lines is a different
graph with a different meaning, and a Plan that uses it must name the metrics
as its own rather than as the benchmark's.

Three tiers fall out of the table:

1. Fold-only (`cc_max`, `cc_mean`, `cc_std`, `cc_high_count`,
   `cc_extreme_count`, `high_cc_mean`, `cc_top20`, `cc_concentration`,
   `cc_normalized`, `mass.cc`, `mean_func_loc` and its twin
   `lines_per_symbol`, `functions`, `methods`, and `symbols_total` as far as
   functions go): a few lines in the analyzer's
   `metrics` function and new keys in every metric map the contract names
   (corpus, target, file, area, common-parsed cohort, comparison deltas).
   Deterministic for free, because the inputs already are. Cognitive erosion
   is one step behind: `scb-check` already computes `cog_complexity` per
   function, but the analyzer's `capture` copies only `cyc_complexity`, so it
   needs one more captured field before the same fold applies. The churn trio
   is a Git diff per history row and belongs with the backfill, not the
   analyzer.
2. Export-needed (`max_nesting_depth`, `classes`, `statements`,
   `single_comments`): `scb-check` walks the data but does not surface it;
   either a small upstream-style change in the vendored copy or a second tree
   walk in the analyzer. `ThirdParty` is do-not-modify, so this means the
   analyzer.
3. New-tool (`lint_*`, the waste trio, the three graph metrics):
   clang-tidy already exists in the repository (`.clang-tidy`,
   `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md` owns
   enablement), but a per-commit run across roughly a thousand historical
   commits needs a compile database at every one of them, which the old eras
   cannot cheaply provide. The waste trio and the call graph both need
   resolved C++ calls, which is a parser on the scale of clang's, not a
   pattern matcher. An include graph over `#include "..."` lines mapped to the
   same `area` buckets the analyzer already computes is cheap and fully
   deterministic, but it is a local metric, and its meaning depends on choosing
   the node granularity (file, area, or `Engine/Source/<child>` module).

## Where each era of history can be recomputed from

The history README records that pre-squash SHAs are "resolvable solely in
offline archives of the old `BrokenEnginePublic` repositories". The local clone at
`C:\Users\dougt\Documents\BrokenEnginePublic` is such an archive: it carries
branches `1.0.0`, `2.0.0`, `2.1.0`, and `main`, plus remotes `old-public`,
`old-public2`, and `origin`, and every SHA sampled from the table resolves as a
commit there (row 0 `f0bb81fa`, row 645 `4a718c7c`, row 646 `e571f6f1`).

| Rows | Era | Recompute from | First-parent commits |
| --- | --- | --- | --- |
| 0 | pre-`2.0.0` snapshot | `BrokenEnginePublic` branch `1.0.0` | 1 |
| 1–640 | `2.0.0` | `BrokenEnginePublic` branch `2.0.0` | 640 |
| 641–645 | `2.1.0` rebuild | `BrokenEnginePublic` branch `2.1.0`, whose 642 first-parent commits share all but its last few with `2.0.0` | 5 |
| 646 | squashed baseline | either repository, `e571f6f1` | 1 |
| 647 onward | live | this repository's `main` first-parent history after `e571f6f1` | 330 at time of writing |

Three facts constrain the backfill:

- Live rows carry a date and a capture mode but no SHA, so they cannot be
  recomputed row-for-row. A backfill of the live era means walking this
  repository's `main` first-parent commits and producing one row per commit,
  which will not have the same row count or dates as the existing suffix.
- The legacy prefix is one row per first-parent commit, but rows whose commit
  touched no C++ carry no metrics and inherit the previous computed row
  (`cppChanging: false`). That flag was written by the pre-squash generator,
  which no longer exists; the live script's patch classifier emits a different
  field, `cppChanged`, and a `captureMode`. A backfill has to pick one
  classifier for every era so the new columns and the old ones agree on which
  commits were actually measured, and must check that it reproduces the legacy
  `cppChanging` flags before trusting it on the old eras.
- The history contract states that no Git-history backfill is performed and
  that a capture-mode Generate requires a clean tree checked out at
  `TipCommit`. Any option that backfills changes both rules, so the contract,
  the script, and the README move together.

Every capture must run under one pinned BootstrapIdentity so old and new rows
are comparable; the existing Generate path already refuses drift. A full
backfill is roughly 650 Snapshot runs on the pre-squash eras plus 330 on the
live era, minus the rows whose commit touched no C++.

## Options for carrying new columns

### A. Second table, legacy table untouched

Add `CodeQualityMetricsHistoryExtended.jsonl` beside the existing file with a
`code-quality-metrics-history/v2` schema carrying every column, backfilled over
both eras with SHAs on every row (the live era gets SHAs because the backfill
walks real commits). The old file and its 648-line prefix rule stay as they
are; `Invoke-CodeQualityMetricsHistory.ps1` gains a second series.

- For: no change to the immutable prefix or the validator's row shape; the
  existing SVG and receipts keep their digests.
- Against: two files describe one history; the live-era rows in the old file
  and the per-commit rows in the new one disagree on row count and dates, so
  the README has to say which one is authoritative for what.

### B. Replace the table with a v2 that is fully backfilled

Regenerate one table from scratch over both eras, every row with a SHA and
every column, and drop the prefix/suffix distinction: the validator checks one
row shape and the "prefix" becomes the rows whose SHAs live only in the public
clone. The live-era `carry-forward` rows are discarded in favor of per-commit
rows.

- For: one file, one row shape, one validator path; the `verbosity` and
  `structuralErosion` columns are recomputed under the current adapter and
  BootstrapIdentity, so the whole series is internally consistent instead of
  spanning the `adapterVersion 5` legacy capture and the live one.
- Against: contradicts the README's "never rewrite or append the tracked JSONL
  prefix" and the contract's immutability guarantees, which exist so a Contract
  receipt binds to exact bytes; the guarantee moves to "immutable once
  generated" for the new file. Loses the existing live-era rows' dates, which
  are the only record of when a carry-forward happened.

### C. Extended metrics in Snapshot and Compare only, no history

Add the fold-only tier to the analyzer output and the digest, so every review
advisory and Compare delta sees them, but leave the history table alone.

- For: smallest change; delivers the per-change advisory value immediately.
- Against: gives up the measure-over-time the request asks for; the only
  history would be whatever Compare receipts sessions happen to keep.

## Recommendation

Option A for the table, with the fold-only tier plus cognitive erosion as the
first Plan and the include-graph trio as a second Plan that depends on it. The
fold-only tier costs nothing beyond the analyzer fold and the backfill runs, and
the backfill script is the same for every later column, so it is worth building
once with SHAs on every row. Option B is the cleaner end state but overturns the
immutability rules the history contract is built around; that is a user
decision, not a Plan's. Leave the linter group out unless a way to get a
compile database at historical commits appears, and leave the waste trio and a
faithful call graph out entirely: both need resolved C++ calls, and that is a
larger job than the rest of this document combined.

## Decisions a Plan needs

1. Option A, B, or C for the history table. A or B also rewrites the history
   contract's "no Git-history backfill" and clean-tree-at-`TipCommit` rules to
   admit a backfill run.
2. Which columns the first Plan carries: fold-only tier alone, fold-only plus
   cognitive erosion's one captured field, or those plus `max_nesting_depth`
   via a second tree walk in the analyzer.
3. Which C++-change classifier the backfill applies across eras, and whether
   the backfill re-derives the two existing columns for every era under
   the current BootstrapIdentity (consistent series, all rows comparable) or
   copies them from the legacy table (byte-faithful to the archive, mixed
   adapters).
4. Node granularity for the include graph: file, `area` bucket, or module
   directory. This fixes the meaning of cyclic mass, propagation cost, and
   entropy, and the names must say the graph is includes, not calls.
5. Where the backfill script lives and how it names the public clone: a
   `-ArchiveRepository <path>` parameter on `Invoke-CodeQualityMetricsHistory.ps1`
   is the smallest surface; it must refuse a clone in which any referenced SHA
   does not resolve.
6. Whether the SVG gains one panel per column or a second SVG per metric family.
