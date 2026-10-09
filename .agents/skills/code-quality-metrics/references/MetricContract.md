# Metric Contract

## Input

`Invoke-CodeQualityMetrics.ps1` takes exactly one `-Mode Snapshot|Compare`. Snapshot requires
`-Target <relative-POSIX-path> -Scope Exact|Directory|Recursive`; Compare requires `-Targets <UTF-8
JSON file> -Baseline <full lowercase Git SHA>`. Snapshot alone accepts `-Commit <full lowercase Git
SHA>`, which measures that commit's tree instead of the working tree: the corpus is the commit's
supported blobs, a `Directory` or `Recursive` target must be a tree in that commit, and no
working-tree drift check runs. Both require an existing `-RepositoryRoot`, absolute
or resolved against the current directory, and accept
`-OutputPath`, `-Digest`, `-DigestPath`, and `-Phase0Hints` (which implies `-Digest`). `-DigestPath`
names the file the digest is written to and is rejected together with `-Digest`, which already puts
the digest on stdout. A `-DigestPath` that already exists is refused before any bootstrap or analyzer
work, and the file is never overwritten, so a retry after a partly written file uses a fresh path.

The targets file is UTF-8 JSON with unique, ordinal-sorted, normalized relative POSIX paths:

```json
{"schemaVersion":"broken-engine-code-quality-targets/v1","paths":["Engine/Source/A.cpp","Engine/Source/B.cpp"]}
```

Supported C++ paths are `.c++`, `.cc`, `.cpp`, `.cxx`, `.h`, `.hh`, `.hpp`, and `.hxx` outside the
excluded roots `ThirdParty`, `.agents`, `.claude`, and `Temp`. A `.h` beneath contiguous
`Data/Shaders` components is pure GLSL and unsupported except `ShaderLayouts.h` and
`ShaderLayoutsBase.h`, whose direct `BT_ENGINE` C++ branch is captured; a listed or Exact-Snapshot
GLSL header exits `2` with `target is not classified as C++ for BrokenEngineExtended: <path>`. Every
listed path must exist in the baseline corpus, the current corpus, or both; a path in neither —
excluded-root, non-C++, or absent — exits `2` naming the path rather than being silently dropped.

The analyzer derives Compare pairs from the two corpora: a listed path on both sides is a same-path
pair; each remaining current-only listed path, walked in ascending ordinal order, consumes the first
ascending-ordinal baseline-only listed path that Git's deterministic `--no-index --no-ext-diff
--find-renames=50%` detection reports as a rename; leftovers become one-sided add or delete pairs.
Emitted pairs sort by baseline path, then current path.

## Output

Output is compact UTF-8 JSON without BOM and with exactly one final LF. Object keys retain the
construction order below; arrays whose members are sets use the stated ordinal sort. Finite floats
round to 12 fractional digits and negative zero is zero. No absolute or volatile value appears.
Top-level fields, in order, are `schemaVersion` (`broken-engine-code-quality-metrics/v3`), `mode`,
`profile` (always `BrokenEngineExtended`), `tool`, `targetSelection`, `baseline`, `current`, and
`comparison`.

`targetSelection` is `{kind,scope,target,paths}` for Snapshot and `{kind,pairs,paths}` for Compare,
each pair `{baseline,current}` of nullable `{path,mode,sha256}` identities.

`baseline` and `current` are CaptureViews containing `corpusManifest`, `targetManifest`,
`corpusCounts`, `targetCounts`, `skips`, `corpusMetrics`, `targetMetrics`, `files`, `areas`,
`outliers`, `targetOutliers`, `cloneGroups`, `highComplexityFunctions`, `longLowComplexityFunctions`,
`deepNestingFunctions`, `sameNameForwarderFunctions`, and `uncalledFunctions`. A MetricValue is exactly
`{applicable,value,numerator,denominator}`; counts are `{supported,parsed,omitted}`; skips are
`{path,code:"upstream-omitted"}`; file and area rows are `{path,area,metrics}` and `{area,metrics}`.
`outliers` are corpus-wide positive top-ten `structuralErosion` and `verbosity` metrics, and
`targetOutliers` are target-only positive top-ten file and area buckets measured against the
corresponding corpus metric; the `excessDecisions`, `commentDensity`, and `sameNameForwarders`
metric values never enter outlier or Phase-0 hint buckets. Both use `items`, `totalCount`, and
`truncatedCount`, ordered delta-descending then key. Areas are
`Engine/Source/<child>`, `Projects/<project>/Source/<child>`, `Common/<child>`, `Tools/<child>`, a
direct parent label, another nested first component, or `[root]`. Clone instances are complete,
deduplicated by `(groupHash,path,startLine,endLine)`, and contain
`groupHash,path,startLine,endLine,sloc,sourceSha256,occurrenceOrdinal`, where the source hash covers
the exact LF-normalized inclusive span. `verbosity` is the union of emitted CloneBlock intervals
intersected with parsed SLOC, divided by parsed SLOC; `structuralErosion` is mass of functions with
CC above ten divided by total mass, mass being `CC * sqrt(SLOC)`; `excessDecisions` is the total
`sum(max(CC - 1, 0))` over functions in the scope. For `excessDecisions`, numerator and value are
that total, and denominator is `1` when the scope contains at least one function, otherwise `0`
with `applicable:false` and a null value. `commentDensity` is comment-only lines divided by
comment-only plus code lines of parsed files, classified over the raw bytes with CRLF and CR read as
LF and no line after the final LF: a line with any non-whitespace code or string/char-literal byte
(preprocessor lines included) is code, a line whose non-whitespace bytes are all comment is
comment-only, and any other line is blank. `sameNameForwarders` takes the `excessDecisions` shape
over the count of same-name forwarders in the scope. All five metrics appear in corpus, target, file,
area, common-parsed-cohort, and comparison delta maps. Excess decisions are net scope evidence:
unchanged means only no net decision removal, never redistribution without source-diff evidence. A
target decrease proves simplification only when the diff shows decision removal and the corpus shows
no attributable offset elsewhere, or a separately evidenced structural benefit independently
justifies extraction.

The four lists after `highComplexityFunctions` are corpus-wide, uncapped navigation pointers, sorted
by path, then numeric `startLine`, then name. A function unit is a tree-sitter `function_definition`
with a body; a nested definition is its own unit and a lambda belongs to its enclosing unit. Its name
is the declarator name, `~Name` for a destructor, and null for a nameless (operator) unit.
`longLowComplexityFunctions` holds `highComplexityFunctions`-shaped rows spanning more than 100 lines
at CC ten or less. `deepNestingFunctions` rows are `{path,name,startLine,endLine,depth}` for units of
depth at least four, `name` possibly null; depth is the largest count of level-opening ancestors
(`if`, `for`, range `for`, `while`, `do`, `switch`, `try`, lambda) of any node in the unit, where an
`else if` stays at its parent's level and an `if constexpr` opens none. `sameNameForwarderFunctions`
rows are `{path,name,startLine,endLine}` for units whose body is one non-comment `return` or
expression statement holding exactly one call whose callee name equals the unit's name. The callee
name is read through plain (`Name`), member (`obj.Name`, `ptr->Name`), qualified (`A::Name`), and
template-function (`Name<T>`) callees, possibly nested; any other callee form, such as
`obj.Name<T>()`, never matches. `uncalledFunctions` rows have that shape for named units whose name
has zero references in the parsed corpus files — an identifier or field-identifier leaf of the
normalized tree other than a function declarator's own name, or an identifier token on a `#define`
line and its continuations — excluding constructors, destructors, the entry points `main`, `wmain`,
`WinMain`, and `wWinMain`, and any name carrying `virtual`, `override`, or `final` in those files. A
reference elsewhere — in an omitted file, outside the corpus, or in normalizer-masked code off a
`#define` line — is not counted. Matching is by name only, so overloads hide each other and the list
can miss dead code but never lists a name with a counted reference.

`comparison` contains `contextChanges` (`{path,change}` rows), `corpus`, `target`,
`commonParsedCohort`, `coverage`, `cloneGroups`, `cloneInstances`, and `functions`. Delta families
use `{baseline,current,valueDelta,numeratorDelta,denominatorDelta,classification,suppressionReasons}`
and `coverage` uses `{baseline,current,deltas}` over the CaptureView counts. Clone-group rows are
`{groupHash,change}`; clone-instance rows are `{key,baseline,current}` with key
`{groupHash,sourceSha256,sloc}`, sorted by group hash, source hash, SLOC, path, inclusive span, then
side presence; only group hashes touched by target-side instances are selected, classified from
complete corpus membership, with unchanged external counterparts emitted outside the target paths.
Function rows are `{identity,ambiguous,baseline,current}` plus `deltaCc` when directional; identity is
nullable owner/name/signature with the exact current filename module prefix plus `.` removed from a
non-null owner, and a directional comparison requires one non-null identity per side with no overloads
or duplicate identities for that owner/name. `commonParsedCohort` lists the `baselinePaths` and
`currentPaths` of two-sided pairs that parsed on both sides, so a derived rename participates without
pretending its paths are equal; it suppresses only for applicability and context changes, while corpus
and target deltas also suppress for logical membership and paired parse-status changes. Suppression
reasons are only `baseline-inapplicable`, `context-change`, `current-inapplicable`,
`membership-change`, or `parse-status-change`, sorted ordinally.

`-Digest` replaces stdout with `{schemaVersion:"broken-engine-code-quality-evidence/v3",profile,
targetSelection,coverage,comparison}` selected from that same report, adding capped `hints` under
`-Phase0Hints`. `-OutputPath` always receives the full report. `hints` holds eight
`{items,total,emitted}` categories of target-path rows, at most ten items each, in this order:
`targetOutliers`, `cloneGroups`, `extremeComplexityFunctions` (`highComplexityFunctions` rows above
CC 30, by mass), `longLowComplexityFunctions` (by line span), `deepNestingFunctions` (by depth),
`sameNameForwarderFunctions` (unweighted), `uncalledFunctions` (by line span), and `skips`. Function
categories order descending by that weight, then by path, numeric `startLine`, and name.

`-DigestPath` writes those same digest bytes — UTF-8 without BOM and with exactly one final LF — to
the named file and replaces stdout with one receipt line
`{"schemaVersion":"broken-engine-code-quality-digest-receipt/v1","mode":...,"digestPath":<absolute
path written>,"digestBytes":<byte count>}`, so the digest itself never reaches the caller's console.
It applies to both modes, and `-Phase0Hints` adds the same capped hints to the written digest.

## Failures

Compare requires complete parsing and signature extraction for every listed target, and a Compare
result cannot support PASS until every listed target meets that requirement. A target dispatch-parse
failure exits `2`, writes no stdout or `-OutputPath` file, and forwards the analyzer's single stderr
line verbatim:

```json
{"code":"target-parse-failure","message":"sanitize the listed target spelling and rerun Compare","failures":[{"side":"baseline","path":"...","stage":"dispatch-parse","code":"normalized-tree-error","line":1,"column":0}]}
```

Each failure has exactly `side`, `path`, `stage`, `code`, `line`, and `column`, sorted baseline before
current, then path and normalized location. Dispatch-parse codes are exactly `dispatch-parse-failure`,
`normalized-tree-error`, `normalized-tree-missing-node`, or `normalized-tree-unavailable`; line is
one-based and column is the normalized byte column. Fix the narrow parser-only sanitizer spelling and
rerun; never accept this result as a score or coverage omission. A per-file metric walk (line
classification, nesting, forwarders, references) that raises also records
`normalized-tree-unavailable` and omits the file; on a listed Compare target, that code caused by a
walk is an analyzer defect to investigate, not a sanitizer case. Signature extraction is distinct: the
same failure shape with top-level code `target-signature-extraction-failure` and message `investigate
target signature extraction and rerun Compare`, which is an investigation, not a sanitizer
instruction. Corpus-only parser omissions stay advisory `upstream-omitted` coverage rows. Every other
input, capture, bootstrap, drift, digest, or output failure exits `2` with a `CodeQualityMetrics: `
stderr line.

## Capture and bootstrap

Identity, drift, and `sourceSha256` values always use raw source bytes. Capture then applies the fixed
normalizer, a parser-only, byte-preserving transform that replaces masked non-newline bytes with ASCII
spaces — retaining every byte count, CR/LF byte, physical line, and byte column — by skipping strings
and comments, blanking directives, structurally selecting direct `defined(BT_ENGINE)`/`ifdef
BT_ENGINE` branches, and masking the Broken Engine compatibility, SAL, and calling-convention token
families plus conditional declarator alternatives and the few declaration and expression forms upstream
C++ cannot parse. It never alters real C++ files, compiler input, or coverage policy. Parser-derived
group hashes may reflect this capture; typed signatures, coordinates, SLOC, and raw-span hashes retain
source coordinates and raw identities.

The entry point selects the first `python` Application resolved through normal PATH precedence and
requires x64 CPython 3.12 or newer. It copies the checkout's own `ThirdParty/scb-check` `src` and
`requirements.lock` into a fresh ignored stage under `Temp/CodeQualityMetrics` and imports that copy.
The venv key and `complete.json` are keyed only by Python interpreter identity and the
`requirements.lock` hash.

`-Mode BootstrapIdentity` uses that same resolution, probe, bootstrap, and operational validation
without starting `Analyze-CodeQualityMetrics.py`. Its path-free output is the typed
`broken-engine-code-quality-bootstrap-identity/v1` object documented in
[HistoryContract.md](HistoryContract.md). It contains Python implementation/version/architecture
and executable SHA, lock SHA, cache key, venv Python SHA, `sg` SHA/version, canonical completion
identity SHA, and the scb content digest. Snapshot and Compare continue to emit the exact v3 shape
above; BootstrapIdentity is a separate mode and does not add fields to those reports.
