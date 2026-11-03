<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-02T19:29:42.400Z","dependsOn":[]} -->
# Inline Ui Wrapper Initialization

## Context

The all-rule lint review confirmed 541 remaining obligations in
the saved ledger. The source forms remain present and fail the named style
rules; the declarations, missing defaults, parameter forms or ordered guard
groups in candidates.json establish the local cause. These were pre-existing
forms at the recorded baseline, outside the newly narrowed landing boundary.
The user deferred implementation explicitly. False positives and completed
mechanics are excluded. The support ledger is durable evidence, not a new scan
or permission to expand a hand audit.

## Design

Implement only `inline-wrappers`, using every ledger.definition as the exact initializer. Preserve header
declaration order, guards, all types/constructor arguments/defaults/ranges/steps/allowed order, quartet
groups, both CurveData objects, guarded bool, curve points and invariant comments. Remove old definitions,
retain .cpp files and membership. Constructors remain untouched. Existing inline SoundSettings quartets are
the pattern. Normal dynamic initialization remains below library-phase logging (`Common/Log/Log.cpp:7`);
allocation tracking starts false.

Projects wrapper globals (162 rows).** Own six Ui header/.cpp pairs: Lighting, Map, Particle, Smoke, Sound,
WindDeposits. Apply the ledger's exact three-float initializer tokens once per object, preserving header order
and comments. Lighting/Map/Smoke/Sound remain BT_CLIENT; Particle/WindDeposits remain shared. Keep files,
includes, namespace skeletons and project membership. No Wrapper, registrar, consumer, reset/load/save or
default/range change.

## Critical files

- `Engine/Source/Ui/GraphicsQualityWrappersBase.h` — only the named candidate regions in candidates.json.
- `Engine/Source/Ui/GraphicsSettingsWrappersBase.h` — only the named candidate regions in candidates.json.
- `Engine/Source/Ui/HexShieldWrappersBase.h` — only the named candidate regions in candidates.json.
- `Engine/Source/Ui/LightingWrappersBase.h` — only the named candidate regions in candidates.json.
- `Engine/Source/Ui/PbrWrappersBase.h` — only the named candidate regions in candidates.json.
- `Engine/Source/Ui/ShadowWrappersBase.h` — only the named candidate regions in candidates.json.
- `Engine/Source/Ui/SmokeWrappersBase.h` — only the named candidate regions in candidates.json.
- `Engine/Source/Ui/SunMoonWrappersBase.h` — only the named candidate regions in candidates.json.
- `Engine/Source/Ui/TerrainWrappersBase.h` — only the named candidate regions in candidates.json.
- `Engine/Source/Ui/WaterWrappersBase.h` — only the named candidate regions in candidates.json.
- `Engine/Source/Ui/WindWrappersBase.h` — only the named candidate regions in candidates.json.
- `Engine/Source/Ui/WrapperBase.h` — only the named candidate regions in candidates.json.
- `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h` — only the named candidate regions in
  candidates.json.
- `Projects/BrokenEngineSandbox/Source/Ui/MapWrappers.h` — only the named candidate regions in
  candidates.json.
- `Projects/BrokenEngineSandbox/Source/Ui/ParticleWrappers.h` — only the named candidate regions in
  candidates.json.
- `Projects/BrokenEngineSandbox/Source/Ui/SmokeWrappers.h` — only the named candidate regions in
  candidates.json.
- `Projects/BrokenEngineSandbox/Source/Ui/SoundWrappers.h` — only the named candidate regions in
  candidates.json.
- `Projects/BrokenEngineSandbox/Source/Ui/WindDepositsWrappers.h` — only the named candidate regions in
  candidates.json.

- `Engine/Source/Ui/GraphicsQualityWrappersBase.cpp` — matching definitions or directly dependent contract.
- `Engine/Source/Ui/GraphicsSettingsWrappersBase.cpp` — matching definitions or directly dependent contract.
- `Engine/Source/Ui/HexShieldWrappersBase.cpp` — matching definitions or directly dependent contract.
- `Engine/Source/Ui/LightingWrappersBase.cpp` — matching definitions or directly dependent contract.
- `Engine/Source/Ui/PbrWrappersBase.cpp` — matching definitions or directly dependent contract.
- `Engine/Source/Ui/ShadowWrappersBase.cpp` — matching definitions or directly dependent contract.
- `Engine/Source/Ui/SmokeWrappersBase.cpp` — matching definitions or directly dependent contract.
- `Engine/Source/Ui/SunMoonWrappersBase.cpp` — matching definitions or directly dependent contract.
- `Engine/Source/Ui/TerrainWrappersBase.cpp` — matching definitions or directly dependent contract.
- `Engine/Source/Ui/WaterWrappersBase.cpp` — matching definitions or directly dependent contract.
- `Engine/Source/Ui/WindWrappersBase.cpp` — matching definitions or directly dependent contract.
- `Engine/Source/Ui/WrapperBase.cpp` — matching definitions or directly dependent contract.
- `Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.cpp` — matching definitions or directly dependent
  contract.
- `Projects/BrokenEngineSandbox/Source/Ui/MapWrappers.cpp` — matching definitions or directly dependent
  contract.
- `Projects/BrokenEngineSandbox/Source/Ui/ParticleWrappers.cpp` — matching definitions or directly dependent
  contract.
- `Projects/BrokenEngineSandbox/Source/Ui/SmokeWrappers.cpp` — matching definitions or directly dependent
  contract.
- `Projects/BrokenEngineSandbox/Source/Ui/SoundWrappers.cpp` — matching definitions or directly dependent
  contract.
- `Projects/BrokenEngineSandbox/Source/Ui/WindDepositsWrappers.cpp` — matching definitions or directly
  dependent contract.

## In scope

Implement exactly 541 saved candidate rows in `InlineUiWrapperInitialization/candidates.json` and their
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
- Before the first edit retain current-tree build identity and a /agent-harness baseline with isolated
  settings: connected rendering and advancing server tick; get_wrapper for Water Scale, Lighting Blur Sigma,
  four Detail Normals Multiplier keys and one setting from each of Lighting, Map, Particle, Smoke, Sound and
  WindDeposits. Retain values/ranges/defaults, main-menu describe_ui, one label resolution and existing
  text-value capture; exercise Tweaks reset on representative settings. Repeat matching setup after the
  change, require identical values/labels/IDs/reset results and clean shutdown without duplicate registration,
  constructor or lifetime diagnostics. The supplied evidence records no executed runtime setup; capture this
  prospective baseline before editing. Compare all 541 exact definition tokens and guards with candidates.json
  and preserve header order, curve points/comments, quartets and allowed order. Build sandbox client/server
  and ModelViewer Debug|x64 and Release|x64 via /compile.
- Required reviews and checks pass; no format/layout/CRC or compatibility
  behavior changes beyond the expressly recorded internal-offset choice.

Complete original-row reconciliation is retained in
`InlineUiWrapperInitialization/coverage.json`: it records every one of the
2,056 non74 original candidates, including false positives, completed rows,
SAL contract rejections and the twelve earlier deferred rows. Only this Plan's
541 rows in candidates.json are implementation scope. The additional 736
rule74 rows were already completed and are excluded from every follow-up.

## Notes

Future implementation is Change Workflow Tier 3: dynamic initialization crosses Engine UI/graphics/audio and
game UI/frame/Tweaks integration.

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
