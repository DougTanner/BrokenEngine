<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-02T19:29:40.040Z","dependsOn":[]} -->
# Engine Initialization Mechanics

## Context

The all-rule lint review confirmed 4 remaining obligations in
the saved ledger. The source forms remain present and fail the named style
rules; the declarations, missing defaults, parameter forms or ordered guard
groups in candidates.json establish the local cause. These were pre-existing
forms at the recorded baseline, outside the newly narrowed landing boundary.
The user deferred implementation explicitly. False positives and completed
mechanics are excluded. The support ledger is durable evidence, not a new scan
or permission to expand a hand audit.

## Design

`member-init`: `PriorityEntry` in `Engine/Source/Audio/StaticVoices.cpp:310` gets `fAttenuated = 0.0f` and
`iSoundIndex = 0`; retain every complete designated assignment. `constinit-globals`: replace the declarations
in `Engine/Source/Memory/GlobalAllocator.h:3` and `:5` with `inline constinit std::atomic<int64_t>
giAllocationsThisFrame = 0;` and `inline constinit std::atomic<bool> gbAllocationTrackingReady = false;`;
remove only their existing .cpp definitions. Preserve guards, declaration order and one shared identity.

## Critical files

- `Engine/Source/Audio/StaticVoices.cpp` — only the named candidate regions in candidates.json.
- `Engine/Source/Memory/GlobalAllocator.h` — only the named candidate regions in candidates.json.

- `Engine/Source/Memory/GlobalAllocator.cpp` — matching definitions or directly dependent contract.

## In scope

Implement exactly 4 saved candidate rows in `EngineInitializationMechanics/candidates.json` and their
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
- Static comparison settles all four rows: exact zero/false initializers, single shared atomic identity,
  guards and declaration order, complete PriorityEntry assignments. Through /compile build BrokenEngineSandbox
  and BrokenEngineSandboxServer Debug|x64 and Release|x64, and ModelViewer Debug|x64 and Release|x64. No
  runtime criterion is needed for these mechanical rows.
- Required reviews and checks pass; no format/layout/CRC or compatibility
  behavior changes beyond the expressly recorded internal-offset choice.

## Notes

Future implementation is Change Workflow Tier 1: local complete designated assignments and already
constant-initialized atomic definitions retain their effective types and initialization.

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
No mandatory nondirectional coordination: no sibling lint Plan owns these candidate files. Other
initialization Plans must independently preserve the existing library-phase logging lifetime.
Share matching
runtime evidence only when it settles each Plan's own acceptance criteria.
The four existing filesystem/publication Plans own separate deferred choices
and need no dependency or new reciprocal constraint from this Plan.
