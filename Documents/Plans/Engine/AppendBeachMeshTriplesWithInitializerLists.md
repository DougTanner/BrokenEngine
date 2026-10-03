<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:37:44.509Z","dependsOn":[]} -->
# Append complete beach mesh triples with initializer lists

## Context

`DataPacker/Source/ExportJobs/Island/SubdivideBeachBand.cpp` stores positions as packed XYZ floats and triangle indices as packed ABC `uint32_t` values. `BeachSubdivider::AddVertex` and `BeachSubdivider::AppendTriangle` each append one logical record through three adjacent `push_back` calls. One initializer-list insertion expresses that record directly without adding a helper or changing its representation.

The independent B08 adoption review accepted only these two sites. This Plan includes their rationale and evidence and needs no feature audit document. A search of existing Plans for the filename, function names, initializer-list adoption, and B08 found no competing ownership.

## Design

1. In `BeachSubdivider::AddVertex`, replace only the three position appends with `meshPositions.insert(meshPositions.end(), {fX, fY, fZ});`.
2. In `BeachSubdivider::AppendTriangle`, replace only the three index appends with `meshIndices.insert(meshIndices.end(), {iA, iB, iC});`.

Keep each pre-insertion index calculation and return unchanged. Keep `triangleAlive`, `triangleDepth`, and the three `EdgeAdd` calls in their existing order after the complete triangle append.

All six inputs are scalar parameters passed by value, so the temporary initializer lists contain independent values of exactly the vector element types. The lists preserve XYZ and ABC order, perform no arithmetic or narrowing, and own no heap allocation. There is no retained element reference or iterator across these calls; subdivision callers retain indices and retrieve values again through indexed access.

### Capacity and cost

The installed VS 2026 MSVC 14.51.36231 `include/vector` implements initializer-list `insert` through `_Insert_counted_range`. With three free slots it copies the three scalars into unused storage; insertion at `end()` shifts no existing elements. Otherwise it performs one allocation for the complete triple and relocates the existing prefix once. `_Calculate_growth` uses the larger of required size and 1.5 times the old capacity, subject to the maximum-size limit. Three individual appends can instead encounter multiple growths at very small capacities. Exact final capacity and allocation timing are therefore not preserved or promised; no caller uses them as data. The initializer list introduces a temporary three-scalar backing array and scalar copies, with no ownership or nontrivial-constructor cost. Both approaches retain amortized linear total append work, but that alone does not establish no performance regression. Adoption requires the bounded baseline/candidate check below; no speedup is claimed. Do not add `reserve`, a persistent array, or allocation instrumentation to this cleanup. Fatal allocation/length failure can occur at a different point; rule 9 supplies no recoverable partial-record contract.

### Version and documentation implications

`DataPacker/Source/ExportJobs/Island/AGENTS.md` `## Cache Lifecycle` says a subdivision change bumps the split version, not the bake version. Read together with its explicit `## Versioning` rule, "bump only the one whose behavior changed," this identifies the owner of a subdivision behavior change. The parent ExportJobs hub likewise ties stage bumps to changed behavior. These two equivalent append substitutions alter no subdivision decision, value, ordering, exported byte, or format, so they require no split, bake, texture, chunk, or shared-format version bump. This is the explicit reconciliation of the broad lifecycle wording with the version policy; it is not an exemption for changed subdivision behavior.

Style rules 20 and 22 already accommodate initializer lists. No style rule or architecture documentation amendment is warranted. Required future documentation review should record that no documented contract changed.

## Critical files

- `DataPacker/Source/ExportJobs/Island/SubdivideBeachBand.cpp`: the two append blocks and their immediate callers/consumers.
- `DataPacker/Source/ExportJobs/Island/BakeRoute.cpp`: read-only evidence that the subdivision result is consumed by element count and contents.
- `DataPacker/Source/ExportJobs/Island/AGENTS.md`: read-only cache/version policy.
- `Documents/C++StyleGuide.txt`: read-only rules 9, 20, and 22.

## In scope

The retained implementation changes only the six scalar `push_back` statements in `BeachSubdivider::AddVertex` and `BeachSubdivider::AppendTriangle`, replacing them with the two specified initializer-list insertions. Temporary verification-only edits to the DataPacker entry point are permitted solely for the bounded performance check below and must be restored before final review.

## Out of scope

The compaction loop's other triple appends; any other vector or file cleanup; helpers, signatures, members, allocation policy, reserve calls, subdivision arithmetic, worklist behavior, metadata, adjacency ordering, formats, version constants, style/documentation changes, permanent benchmark or measurement infrastructure, generated assets, export execution, and unit tests. The bounded temporary append-cost measurement below is required verification, not retained implementation scope.

## Risk and invariants

Future implementation is Tier 1: two local behavior-preserving substitutions with no public signature or invariant exposure. Packed element types and order, returned indices, triangle metadata/adjacency order, subdivision decisions, and exported contents remain identical for successful operations. Container capacity is not a serialized or algorithmic invariant. A discovered need to change arithmetic, output, or serialization falls outside this Plan and requires fresh classification; do not extend the implementation to resolve it.

## Acceptance criteria and verification

| Criterion | Required evidence |
| --- | --- |
| Each intended record is appended by one statement | Diff shows precisely the two specified insertions replacing six appends. |
| Output values, order, and indices remain equivalent | Source review verifies identical scalar types/order, unchanged pre-insertion index calculations, no arithmetic additions, and no retained references across either call. |
| Triangle bookkeeping remains equivalent | Diff/source review verifies unchanged metadata pushes, three ordered `EdgeAdd` calls, return value, and callers. |
| No performance regression in either changed append path | Complete the bounded baseline/candidate check below for both scalar types and growth/non-growth cases; reject adoption on a regression or inconclusive evidence. Amortized complexity and allocation-source equivalence do not satisfy this criterion. |
| No version or scope expansion | Diff contains no version, generated-data, documentation, compaction-loop, or unrelated changes. |
| Supported compiler accepts both overloads | Future `/compile` builds DataPacker `Release|x64` successfully, without running exports. |

### Bounded performance check

Use the pre-edit source as baseline and the exact two-insertion diff as candidate. Build both through `/compile` as DataPacker `Release|x64` with the same supported compiler/STL and project options. Retain optimized disassembly of the actual append sites, including inlined callers, to compare capacity checks, scalar loads/stores, temporary backing-array materialization, and relocation paths. Identical optimized operations can settle the non-growth case; a source-level operation count or unchanged asymptotic complexity cannot settle differing instructions or growth trajectories.

For every case not settled by identical generated code, measure the exact old/new append blocks in a temporary DataPacker entry-point probe built through `/compile` in the same configuration. Run only the probe and exit before export dispatch; do not invoke the island export routine, whose asset/cache work would obscure these small costs. Keep probe code and results under `Temp/`, include it temporarily from `DataPacker/Source/Main.cpp`, and restore that file afterward. Compare its optimized append instructions with the actual sites so different optimization or dead-code elimination cannot manufacture a pass. Use runtime scalar inputs and consume final contents and returned indices outside timing; unchanged triangle metadata and adjacency work must not mask the changed append cost.

Cover both `float` XYZ and `uint32_t` ABC appends: non-growing batches; a single growing triple with 0, 1, and 2 free slots at starting element counts 3, 3,072, and 3,145,728; and unreserved streams from empty through 1, 1,024, and 1,048,576 triples. Record and verify actual starting size/capacity for each pair, keeping sizes divisible by three. The isolated cases exercise temporary copies and growth boundaries; the streams include each variant's entire allocation/relocation trajectory. Set up/reset vectors outside timed intervals, but include all allocations and copies caused by appending inside them. Any reserve used to prepare a probe case stays in the temporary probe.

Warm both variants, then collect 30 paired batches per measured case with alternating baseline/candidate order and enough repetitions for at least 100 ms of timed work per variant per batch. Record elapsed time, compiler/options, workload sizes, and capacity trajectories. Require the upper endpoint of a one-sided 95% confidence interval for the paired mean log(candidate/baseline time) to be at most zero for every measured case; report each case separately. At most one complete repeat is allowed for an interrupted/noisy run, retaining both results. A slower or inconclusive case rejects adoption of these substitutions; do not relax the gate, add reserve policy, or land only part of this Plan to rescue it. This is a bounded practical no-regression gate, not a claim of universal cycle equivalence.

Remove the temporary probe integration and rebuild the final candidate through `/compile`; the final diff must contain only the two specified source substitutions, with no version bump or measurement code.

Run the normal C++ change review and triggered cleanup workflow during implementation. No game client/server build, `/agent-harness` scenario, asset regeneration, Gaea bake, or new test is needed: source equivalence settles the behavioral criteria, the DataPacker build checks the actual overloads, and the bounded check settles performance acceptance. None of these future checks is claimed to have run during Plan authoring.
