<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-02T19:29:48.859Z","dependsOn":[]} -->
# Engine Ordered Guard And Difference Reads

## Context

The all-rule lint review confirmed 26 remaining obligations in
the saved ledger. The source forms remain present and fail the named style
rules; the declarations, missing defaults, parameter forms or ordered guard
groups in candidates.json establish the local cause. These were pre-existing
forms at the recorded baseline, outside the newly narrowed landing boundary.
The user deferred implementation explicitly. False positives and completed
mechanics are excluded. The support ledger is durable evidence, not a new scan
or permission to expand a hand audit.

## Design

Own all 22 `guard-sequences` and four `difference-bindings` rows together to settle their overlapping
DifferenceStream row once. Split unrelated concerns with identical ordered exits and retain iterator/candidate
scope in TextureDescriptors. LoadPostDispatch checks end before binding the record by const reference, then
tick, copy/increment/log. LoadDifference keeps terminal-tick guard, initializes fallback from
mCurrentDifference, binds only within valid-iterator scope, and changes output/state/log/increment only on
matching tick, retaining final true. Single-get validation rows remain unchanged. No iterator may be bound at
end; fallback and error behavior remain.

## Critical files

- `Engine/Source/Agent/AgentCommandsClientGeneric.cpp` — only the named candidate regions in candidates.json.
- `Engine/Source/Agent/AgentUiRegistry.cpp` — only the named candidate regions in candidates.json.
- `Engine/Source/Audio/AudioManager.cpp` — only the named candidate regions in candidates.json.
- `Engine/Source/CrashReport.cpp` — only the named candidate regions in candidates.json.
- `Engine/Source/File/DifferenceStream.h` — only the named candidate regions in candidates.json.
- `Engine/Source/File/FileManager.cpp` — only the named candidate regions in candidates.json.
- `Engine/Source/Frame/Alignments.cpp` — only the named candidate regions in candidates.json.
- `Engine/Source/Frame/IslandTerrainResidency.cpp` — only the named candidate regions in candidates.json.
- `Engine/Source/Graphics/Managers/TextureDescriptors.cpp` — only the named candidate regions in
  candidates.json.
- `Engine/Source/Graphics/Managers/TextureUploadManager.cpp` — only the named candidate regions in
  candidates.json.
- `Engine/Source/Graphics/Screenshot.cpp` — only the named candidate regions in candidates.json.
- `Engine/Source/Network/Client/ClientDesyncCore.cpp` — only the named candidate regions in candidates.json.
- `Engine/Source/Network/Client/ReconcileReplay.h` — only the named candidate regions in candidates.json.
- `Engine/Source/Network/NetworkDiscoveryScanner.cpp` — only the named candidate regions in candidates.json.
- `Engine/Source/Network/NetworkMessages.h` — only the named candidate regions in candidates.json.
- `Engine/Source/Ui/Screens/MainMenuScreen.cpp` — only the named candidate regions in candidates.json.
- `Engine/Source/Ui/Screens/PauseMenuScreen.cpp` — only the named candidate regions in candidates.json.

## In scope

Implement exactly 26 saved candidate rows in `EngineOrderedGuardAndDifferenceReads/candidates.json` and their
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
- Trace all 22 guard and four binding rows; resolve the overlapping DifferenceStream row once. Verify
  end-before-binding, same ordered exits, fallback initialized from mCurrentDifference, match-only
  output/log/increment/state changes, retained final true and scoped TextureDescriptors iterators. Record
  pre-edit current-tree Debug startup/UI and one short bounded replay using the existing /agent-harness replay
  recipe; repeat matching post-change setup, require endpoint completion and no newly appended
  CRC/desync/read/persistence failure. Speculative reconciliation alone is not failure. No executed
  reproduction setup is supplied. Build sandbox client/server and ModelViewer Debug|x64 and Release|x64
  through /compile.
- Required reviews and checks pass; no format/layout/CRC or compatibility
  behavior changes beyond the expressly recorded internal-offset choice.

## Notes

Future implementation is Change Workflow Tier 3: ordered replay-reader iteration, fallback and return
invariants plus Engine resource/UI consumer integration.

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
Reciprocal coordination with `Documents/Plans/Engine/EngineTextViewParameters.md`: preserve disjoint candidate
regions and serialize overlapping-file edits. Refresh coordinates and baseline evidence after a preceding
implementation.
Share matching
runtime evidence only when it settles each Plan's own acceptance criteria.
The four existing filesystem/publication Plans own separate deferred choices
and need no dependency or new reciprocal constraint from this Plan.
