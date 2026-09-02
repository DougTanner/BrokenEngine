<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-02T21:33:29.988Z","dependsOn":[]} -->
# Split the Gaea intermediate loaders out of BakeRoute.cpp

## Context

`DataPacker/Source/ExportJobs/Island/BakeRoute.cpp` is over the repository's
implementation-file reduction threshold. Measured with the repository-owned
tool:

```
pwsh -NoProfile -File .agents/scripts/Measure-Tokens.ps1 -Path DataPacker/Source/ExportJobs/Island/BakeRoute.cpp
-> Lines 743, Bytes 41987, Tokens 10497 (bt-token-v1)
```

`/reduce-file` sets the `.cpp` reduction threshold at over 10,000 bt-token-v1
(`.agents/skills/reduce-file/SKILL.md:56`), so the file is 497 tokens over.

The overage is pre-existing. The session that recorded this residual
(baseline `0c120eefd9fe12f3d9a79ed5d33083a66a510d24`) measured the file at
roughly 10,398 tokens before its own edits and added about 99; the file was
already over the threshold at that session's baseline, and reducing it was
outside that session's approved implementation boundary. The `/repo-code-review`
reviewer of that session reported the size observation and deferred planning, as
`/reduce-file` directs for a review-time size finding.

The same reviewer observed the separable shape: the file mixes two unrelated
responsibilities. One is route orchestration and cache lifecycle — fingerprints,
dirty checks, staging, archetype patching, running Gaea, and the two-stage
bake/split flow in `BakeRoute` itself. The other is a cohesive group of
Gaea-intermediate file readers that parse and validate what Gaea wrote on disk:
`CheckedTexturePixelCount` (`BakeRoute.cpp:77`), `LoadElevationMeters`
(`BakeRoute.cpp:334`), `LoadAmbientOcclusion` (`BakeRoute.cpp:375`), and
`LoadMesherMesh` (`BakeRoute.cpp:418`). The three public loaders are called from
exactly one place each, inside `BakeRoute`'s split stage
(`BakeRoute.cpp:694,696,700`); `CheckedTexturePixelCount` is called only by the
other two pixel loaders (`BakeRoute.cpp:337,378`).

Two live Plans cite this file — `Documents/Plans/Engine/IslandFlowMaskCleanup.md`
and `Documents/Plans/Engine/GaeaExecutableResolution.md` — but neither owns file
size: the first removes a Flow mask input, the second defers Gaea executable
resolution. Neither touches the loader group or the file's length, so this is not
a duplicate. See `## Coordination` for the textual overlap.

## Design

The author's recommendation is a pure code move with no behavior change: relocate
the four loader functions into a new translation unit
`DataPacker/Source/ExportJobs/Island/GaeaIntermediateLoaders.cpp`, with a new
sibling private header `GaeaIntermediateLoaders.h` declaring the three loaders
`BakeRoute` calls.

Recommended sibling-header placement rather than
`BakeIslandIntermediatesInternal.h`: the directory already has exactly this
pattern in `SubdivideBeachBand.h` / `SubdivideBeachBand.cpp` — a helper
translation unit with its own header, consumed only by `BakeRoute.cpp`. The
internal header documents itself as the place for declarations shared by the
three `BakeIslandIntermediates` translation units and states that constants used
by only one TU stay file-local, so adding a single-consumer helper contract there
would work against its stated purpose. Reusing the established sibling pattern
keeps the two helper TUs parallel.

Recommended boundary for `LoadMesherMesh`: move it whole, together with the four
`kfBeachSubdivision*` / `kiBeachSubdivisionMaxDepth` constants
(`BakeRoute.cpp:67-70`) and its `SubdivideBeachBand` call and depth-cap logging
(`BakeRoute.cpp:588-605`). Rationale: those constants have no other consumer in
`BakeRoute.cpp`, and the beach-band densification is unconditionally part of
producing the loaded mesh — splitting the call away from the parse would leave a
half-loaded mesh crossing the new TU boundary for no benefit. `CheckedTexturePixelCount`
moves as an anonymous-namespace helper inside the new `.cpp`, since only the two
moved pixel loaders call it.

Everything else stays: `BakeRoute`, both fingerprint builders, both dirty checks,
the text-file helpers, orphan-leaf sweeping, archetype patching, staging, and the
Gaea launch remain in `BakeRoute.cpp`. Function bodies, comments, error text,
validation order, and log lines move verbatim; the three moved declarations gain
external linkage in the new header and lose their anonymous-namespace placement.
The new `.cpp` includes what those bodies need — `BakeIslandIntermediatesInternal.h`
for the shared context types, `SubdivideBeachBand.h`, and its own header.

Projected result: the moved region `BakeRoute.cpp:326-606` measures 4,305 tokens
and `CheckedTexturePixelCount` adds roughly 200 more, leaving `BakeRoute.cpp`
near 6,000 tokens, comfortably under the 10,000 threshold, and putting the new TU
near 4,600.

The new file must be added to the DataPacker Visual Studio project and filters
through `/update-vcxproj`, and the `AGENTS.md` sentence that locates the
`kfBeachSubdivision*` constants in `BakeRoute.cpp`
(`DataPacker/Source/ExportJobs/Island/AGENTS.md:11`) must be repointed at the new
file through `/update-claude-docs`.

Do not change what any moved function computes, reads, validates, throws, or
logs; do not change any cache version, fingerprint, Gaea command, or output byte.

## Critical files

- `DataPacker/Source/ExportJobs/Island/BakeRoute.cpp:67-70,77-90,326-606` — the constants and four functions to move out.
- `DataPacker/Source/ExportJobs/Island/BakeRoute.cpp:694-700` — the three call sites that must resolve through the new header.
- `DataPacker/Source/ExportJobs/Island/SubdivideBeachBand.h` — the sibling-header pattern the new header follows.
- `DataPacker/Source/ExportJobs/Island/BakeIslandIntermediatesInternal.h` — shared context types the new TU includes; not the place for the new declarations.
- `DataPacker/Source/ExportJobs/Island/AGENTS.md:11` — locates the `kfBeachSubdivision*` constants in `BakeRoute.cpp`.
- `DataPacker/Platforms/VisualStudio2026/DataPacker.vcxproj` and its `.filters` — project membership for the new file pair.

## In scope

- Creating `DataPacker/Source/ExportJobs/Island/GaeaIntermediateLoaders.h` declaring `LoadElevationMeters`, `LoadAmbientOcclusion`, and `LoadMesherMesh` with their current signatures and their existing contract comments.
- Creating `DataPacker/Source/ExportJobs/Island/GaeaIntermediateLoaders.cpp` holding the verbatim bodies of `CheckedTexturePixelCount` (file-local), `LoadElevationMeters`, `LoadAmbientOcclusion`, and `LoadMesherMesh`, plus the `kfBeachSubdivisionMinMeters`, `kfBeachSubdivisionMaxMeters`, `kfBeachSubdivisionMaxEdgeMeters`, and `kiBeachSubdivisionMaxDepth` constants and the `SubdivideBeachBand` invocation and depth-cap log inside `LoadMesherMesh`.
- Deleting those functions, constants, and their comments from `BakeRoute.cpp` and adding the new header include so `BakeRoute`'s three call sites still resolve.
- Adjusting only the `#include` lines each of the two `.cpp` files now needs.
- Adding the new file pair to the DataPacker project and filters via `/update-vcxproj`.
- Repointing the `AGENTS.md` sentence that names `BakeRoute.cpp` as the home of the `kfBeachSubdivision*` constants via `/update-claude-docs`.

## Out of scope

- Any change to what the moved functions compute, validate, reject, throw, or log, including error text, validation order, the elevation sea-level conversion, the glTF axis mapping, or the beach-band tuning values.
- Any change to `BakeRoute` itself beyond deleting the moved code and adding the include: fingerprints, dirty checks, orphan sweeping, staging, archetype patching, the Gaea launch, and the two-stage flow stay byte-equivalent in behavior.
- Any bake, split, texture, or payload version bump; any cache marker, `.pack`, or manifest format change; any runtime or shader change.
- Splitting `SubdivideBeachBand`, `ProcessBakedRegion`, or `BakeIslandIntermediates.cpp`; moving anything into `BakeIslandIntermediatesInternal.h`; renaming any moved function; or any further decomposition of `BakeRoute.cpp` once it is under threshold.
- Style, naming, or comment rewrites in the moved code, and unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 1 (mechanical). Trigger: behavior-preserving code
relocation plus project membership, with no public signature, determinism/CRC,
serialization, wire, save/replay, threading, or trust-boundary exposure — the
moved functions are DataPacker-internal helpers with a single consumer, and the
new header is private to this directory.

Preserve these invariants:

- Every moved function's observable behavior is unchanged: same inputs, same
  computed buffers, same validation order, same thrown messages, same log lines.
- A DataPacker run over the same inputs produces byte-identical cache markers,
  intermediates, and `.pack`/`.manifest` outputs before and after the move.
- Bake, split, texture, and payload versions are unchanged, so no cache is
  invalidated by this change.
- The new file pair's project membership matches the DataPacker executable that
  already owns `BakeRoute.cpp`, and the filters entry matches the source folder.

## Acceptance criteria

- `pwsh -NoProfile -File .agents/scripts/Measure-Tokens.ps1 -Path DataPacker/Source/ExportJobs/Island/BakeRoute.cpp` reports at or under 10,000 bt-token-v1, and the same measurement on `GaeaIntermediateLoaders.cpp` is also at or under 10,000.
- An exact diff review shows the moved function bodies, constants, comments, and error strings are identical to their pre-move text apart from namespace placement and include lines.
- `BakeRoute.cpp` contains no definition of the four moved functions and no `kfBeachSubdivision*` constant, and its three loader call sites are unchanged.
- The DataPacker target builds through `/compile`, and `/update-vcxproj` reports the new file pair correctly placed in the project and filters with no other membership drift.
- `DataPacker/Source/ExportJobs/Island/AGENTS.md` names the new file as the home of the beach-band constants; no other authority prose changed.
- No unit tests are added.

## Coordination

`Documents/Plans/Engine/IslandFlowMaskCleanup.md` edits `BakeRoute.cpp:24-35,48-56,318-323`
(the raw-output list and diagnostics) and `Documents/Plans/Engine/GaeaExecutableResolution.md`
edits `BakeRoute.cpp:242-311,536-575` (the dirty-route resolution path). None of
those regions is inside this Plan's moved block, and neither Plan depends on this
one in either direction. Whichever lands second rebases and re-resolves line
numbers; if this Plan lands first, the other two apply their edits to the smaller
`BakeRoute.cpp` with their target regions intact.

## Notes

Origin: a `/repo-code-review` size observation recorded as a pre-existing
out-of-scope residual at baseline `0c120eefd9fe12f3d9a79ed5d33083a66a510d24`. The
implementing session should run `/reduce-file` on `BakeRoute.cpp` first to confirm
the measurement and the separation boundary against the then-current file before
executing the move described above.
