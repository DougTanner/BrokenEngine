<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-13T23:05:27.441Z","dependsOn":[]} -->
# Rename the remaining ArtificialIntelligence identifiers to Ai

## Context

The direction a ship's AI wants to travel has two names in the Sandbox game
code. Most of the code spells it `Ai`: the collection member
`pVecAiDirections`, the flag `ShipFlags::kNoAiDirection`, the helper
`AimAiDirection`, the type `AiSteeringResult`, and parameters named
`vecAiDirection` and `rVecAiDirection`. A smaller set of identifiers in two
translation units still spells it `ArtificialIntelligence`.

The two spellings name the same value, sometimes on the same line:

- `ComputeNavigation` is declared in
  `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.h`
  with the parameter `XMVECTOR& rVecAiDirection` and defined in
  `ShipBehaviours.cpp` with `XMVECTOR& rVecArtificialIntelligenceDirection`.
- `RecomputeNavigationPath` in `ShipBehaviours.cpp` passes its
  `rVecArtificialIntelligenceDirection` straight to `AimAiDirection`, whose
  own parameter is `rVecAiDirection`.
- `AiSteeringResult` in
  `Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.h` holds one member,
  `vecArtificialIntelligenceDirection`, and is returned by
  `ComputeArtificialIntelligenceSteering`.

`Documents/C++StyleGuide.txt` rule 56 lists `Ai` as an approved term "for any
entity the simulation spawns, owns, or controls rather than a human player" and
gives `pVecAiDirections` as its example, so `Ai` is the established repository
term. Root `AGENTS.md` `## Directives` "One term per concept" requires the
established term; a search for `AiDirection` today misses every long-form site.

A case-insensitive search of every tracked file for `ArtificialIntelligence`
at commit `213d2577` finds the long form only in
`ShipBehaviours.cpp`, `TerrainUtils.h`, and `TerrainUtils.cpp` (plus the
investigation row this Plan came from). No `AGENTS.md` file, agent command
name, log string, serialized name, shader, or project file contains it, and
`ComputeArtificialIntelligenceSteering` has one caller, in `ComputeNavigation`.

Origin: a refactor candidate logged in
`Documents/Investigations/Engine/RefactorCandidates.md`, validated against
commit `213d2577`.

## Design

Recommended: rename every long-form identifier to the `Ai` form the rest of
the code already uses, because rule 56 approves it and the short form is the
one the collection member, the flag, and the header declaration carry. The
change is identifier text only; no type, parameter order, value, or statement
changes, so the compiled simulation is unchanged.

Rename set (line numbers as of commit `213d2577`):

| Old identifier | New identifier | Sites |
|---|---|---|
| `ComputeArtificialIntelligenceSteering` | `ComputeAiSteering` | `TerrainUtils.h:24`, `TerrainUtils.cpp:29`, `ShipBehaviours.cpp:351` (comment), `ShipBehaviours.cpp:775` |
| `AiSteeringResult::vecArtificialIntelligenceDirection` | `vecAiDirection` | `TerrainUtils.h:19`, `TerrainUtils.cpp:76` (designated initializer) |
| local `vecArtificialIntelligenceDirection` in the steering function | `vecAiDirection` | `TerrainUtils.cpp:74`, `:76` |
| local `fElevationArtificialIntelligence` | `fElevationAi` | `TerrainUtils.cpp:50`, `:51` |
| parameter `vecArtificialIntelligenceDirection` of `ShouldRecomputeNavigation` | `vecAiDirection` | `ShipBehaviours.cpp:339`, `:355` |
| parameter `rVecArtificialIntelligenceDirection` of `RecomputeNavigationPath` | `rVecAiDirection` | `ShipBehaviours.cpp:510`, `:518`, `:522` |
| parameter `rVecArtificialIntelligenceDirection` of `ComputeNavigation` | `rVecAiDirection` | `ShipBehaviours.cpp:567`, `:598`, `:619`, `:742`, `:769`, `:775`, `:776` |
| structured binding `vecNewArtificialIntelligenceDirection` | `vecNewAiDirection` | `ShipBehaviours.cpp:775`, `:776` |

The comment at `ShipBehaviours.cpp:350`, inside `ShouldRecomputeNavigation`,
names `rVecArtificialIntelligenceDirection`, but that function's parameter has
no `r` prefix. Recommended: write it as `vecAiDirection`, the renamed
parameter it describes.

None of the new names collides with an existing name in its scope: the short
forms already in `ShipBehaviours.cpp` are parameters and locals of other
functions (`IsWithinHeadingCone`, `AimAiDirection`,
`ApplyAcceleratedMovement`), and `TerrainUtils.cpp` has no short-form name
today.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.h`
- `Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp`

## In scope

- The identifiers in the `## Design` rename table, at every site listed, and
  the two comment lines at `ShipBehaviours.cpp:350` and `:351` that name them.
- Any further `ArtificialIntelligence` identifier a fresh case-insensitive
  search of the tracked tree finds at implementation time, renamed the same
  way.

## Out of scope

- Any change to behaviour, types, parameter order, or statement order.
- Identifiers that already use `Ai`; `ShipBehaviours.h` needs no edit.
- The word "AI" in comments and documentation prose.
- Rewrapping or otherwise reformatting the long lines the renamed identifiers
  sit on.
- `Documents/C++StyleGuide.txt` and every `AGENTS.md` file: none names a
  long-form identifier.
- `Documents/Investigations/Engine/RefactorCandidates.md`.
- The private Coretide project, which contains no long-form identifier.

## Acceptance criteria

- A case-insensitive search of the tracked tree for `ArtificialIntelligence`
  matches no file under `Projects/`, `Engine/`, `Common/`, `DataPacker/`, or
  `Tools/`.
- The diff changes identifier and comment text only, in the three critical
  files.
- The BrokenEngineSandbox client and server build.

## Notes

- Change Workflow tier: Tier 1 — mechanical, by the "style ... or local
  behavior-preserving work" trigger in `.agents/references/risk-tiers.md`.
  `ComputeAiSteering` is declared in a game header, but only its name changes,
  it has one caller, and no signature, layout, or invariant changes.
- Exposure: the renamed code is simulation code that feeds the per-tick CRC,
  but a rename changes no computed value. No serialization, `.pack`,
  `kiVersion`, wire, replay, threading, allocation, or shader exposure. No
  live verification is needed; the build settles it.
