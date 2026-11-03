<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-02T19:29:43.581Z","dependsOn":[]} -->
# Core String View Parameters

## Context

The all-rule lint review confirmed 11 remaining obligations in
the saved ledger. The source forms remain present and fail the named style
rules; the declarations, missing defaults, parameter forms or ordered guard
groups in candidates.json establish the local cause. These were pre-existing
forms at the recorded baseline, outside the newly narrowed landing boundary.
The user deferred implementation explicitly. False positives and completed
mechanics are excluded. The support ledger is durable evidence, not a new scan
or permission to expand a hand audit.

## Design

Implement all 11 `S-string-views` rows with `std::string_view`/`std::wstring_view`, leaving return types
unchanged. The two Common formatter parameter edits keep the already completed explicit iterator returns.
`RunGit` still constructs an owned command line; `FindAttribute` constructs the existing owned string only for
its nontransparent map lookup; JSON-key sites retain the same key bytes; BuildCommand/PlanMetadata construct
existing owned map keys as needed. Do not change the null-terminated SearchPathW, ParseUtcTimestamp or
retained logging pointer APIs.

## Critical files

- `Common/Log/LogFormatters.h` — only the named candidate regions in candidates.json.
- `DataPacker/Source/ExportJobs/ExportIsland.cpp` — only the named candidate regions in candidates.json.
- `DataPacker/Source/ExportJobs/Island/BakeRoute.cpp` — only the named candidate regions in candidates.json.
- `DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp` — only the named candidate regions in
  candidates.json.
- `DataPacker/Source/FileManager.cpp` — only the named candidate regions in candidates.json.
- `Tools/ToolCommon/CoordinationStore.cpp` — only the named candidate regions in candidates.json.
- `Tools/WorktreeCli/BuildCommand.cpp` — only the named candidate regions in candidates.json.
- `Tools/WorktreeCli/PlanMetadata.cpp` — only the named candidate regions in candidates.json.
- `Tools/WorktreeCli/PlanScheduler.cpp` — only the named candidate regions in candidates.json.

## In scope

Implement exactly 11 saved candidate rows in `CoreStringViewParameters/candidates.json` and their
correctness-dependent declarations/consumers. The ledger preserves original identity, source text and exact
wrapper definitions; this Design overrides historical alternatives or stale action labels. Do not repeat
completed formatter return-type, namespace or SAL edits.

## Out of scope

All candidates outside the saved ledger; new scanner rules or suppression;
filesystem policy and publication-container decisions; Wrapper constructors,
registrars, defaults/ranges, project membership or removal of emptied files;
new helpers/configuration; wire/save/replay/.pack formats and versions; frame
or Collection layout, CRC computation or trust changes; ThirdParty edits;
new unit tests, harness APIs, speculative cleanup or performance claims.
An unexpected behavior change returns for re-planning instead of widening scope.

## Acceptance criteria

- Every saved row is implemented once, using the exact symbol/type/initializer,
  order, guard and lifetime contracts in Design; no rejected row is changed.
- Compare every parameter, declaration and consumer against the eleven saved rows. Preserve existing formatter
  iterator return types, owned command line/map keys and JSON bytes; retain null-terminated and nullable APIs.
  Before edits record smallest read-only WorktreeCli single-Plan validate --lint-only and documented lock
  status with fixed state; repeat and compare exit/JSON diagnostics. Do not claim/delete/land to exercise
  branches. Build DataPacker Release|x64; sandbox client/server and ModelViewer Debug|x64 and Release|x64;
  AgentHarness, WorktreeCli and ServerMonitor Release|x64 via /compile. The supplied evidence contains no
  executed tool baseline; record it prospectively.
- Required reviews and checks pass; no format/layout/CRC or compatibility
  behavior changes beyond the expressly recorded internal-offset choice.

## Notes

Future implementation is Change Workflow Tier 3: Common formatting, asset pipeline and scheduler/build
coordination signatures cross independently owned subsystems.

The user explicitly deferred all remaining implementation while preparing the
completed cleanup to land. This Plan records wanted debt, remains unclaimed,
and supplies no new implementation approval. Claim and obtain normal Change
Workflow approval before edits; reuse earlier analysis only after checking it
against the then-current tree. Tier 2+ needs plan audit, applicable simplicity
review, and Tier 3 needs grill and adversarial review. Apply triggered C++/GLSL
propagation, owner-document synchronization, style, correctness, comments,
over-engineering and independent acceptance verification; finalize verified
work through /finalize-changes. No unit tests or new harness commands.

Baseline evidence is static source at ad5d456751c76bf43c86b7cc5bd038946c81b1c0
plus the saved current candidate forms. Original coordinates may drift: match
path, symbol and text. A recorded runtime baseline must identify its actual
current-tree build; it is not an unedited-session baseline. All live operations
route through /agent-harness and all builds through /compile. Keep each live
run bounded; a proposed individual run over ten minutes requires its purpose,
estimate and user permission before launch. No runtime performance claim.

## Coordination

No directional prerequisites: this Plan can build and verify independently.
Reciprocal coordination with `Documents/Plans/Engine/CoreOrderedGuardSequences.md`,
`Documents/Plans/Engine/SceneVertexInternalOffsets.md`: preserve disjoint candidate regions and serialize
overlapping-file edits. Refresh coordinates and baseline evidence after a preceding implementation.
Share matching
runtime evidence only when it settles each Plan's own acceptance criteria.
The four existing filesystem/publication Plans own separate deferred choices
and need no dependency or new reciprocal constraint from this Plan.
