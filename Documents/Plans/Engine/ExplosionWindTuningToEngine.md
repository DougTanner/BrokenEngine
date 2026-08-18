<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T18:48:08.779Z","dependsOn":[]} -->
# Explosion and Wind Tuning Contract

## Context

Engine/Source/Frame/Collections/Explosions/Explosions.cpp:91-178 reads
approximately two dozen game explosion tuning wrappers, and
ExplosionsSpawn.cpp:120 reads game wind-deposit explosion values. The values
are game content while the mechanism is engine-owned. The mixed wrapper
headers may contain Player-named declarations, but those declarations are
allowed game-local contents under the approved Player boundary when Engine
does not name them.

The unresolved overview residual is an extraction pass that enumerates these
fields and chooses a generic game-value seam. The user-approved conversion
allows the bounded alternatives to remain.

## Design

Choose an engine-owned constexpr struct with today’s defaults and a required
game instance, or a caller-supplied parameters record. The researched
direction is the same struct-of-defaults shape as terrain steering:
engine-owned defaults, game overrides only for fields it changes, and no
Player-specific declaration in Engine-facing code. Keep the existing
per-type logging, payload, spawn, and phase behavior.

## Critical files

- Engine/Source/Frame/Collections/Explosions/Explosions.cpp:91-178
- Engine/Source/Frame/Collections/Explosions/ExplosionsSpawn.cpp:120
- Projects/BrokenEngineSandbox/Source/Ui/LightingWrappers.h
- Projects/BrokenEngineSandbox/Source/Ui/ParticleWrappers.h
- Projects/BrokenEngineSandbox/Source/Ui/SmokeWrappers.h
- Projects/BrokenEngineSandbox/Source/Ui/WindDepositsWrappers.h
- Projects/BrokenEngineSandbox/Source/Pch.h

## In scope

- Enumerating every explosion and wind field read by Engine.
- Selecting and implementing the generic tuning-value boundary.
- Updating direct consumers while preserving current defaults, logs,
  spawn/materialization behavior, and collection phase ordering.
- Removing only the Engine requirement for mixed Player-named wrapper
  declarations; game-owned wrapper contents remain game-owned.

## Out of scope

- Terrain steering tuning, Player dependency contracts, or graphics-quality
  wrapper moves already landed.
- New explosion types, content changes, or visual effect redesign.
- Moving the game transfer buffer or changing CRC/wire payload formats.

## Risk tier and invariants

Expected Change Workflow Tier 3: explosion collections and wind effects
participate in frame phase/CRC-sensitive behavior and cross-language tuning
contracts. Preserve every default, field meaning, phase order, deterministic
math, logging cadence, and allocation rule.

## Coordination

No directional prerequisite is required. TerrainSteeringTuningToEngine is a
pattern reference only; neither Plan may edit the other’s constants. The four
D15 seam Plans — `FrameCollectionsPlayerIncludeBoundary.md`,
`PusherAnchorContract.md`, `NavigationClearanceContract.md`, and
`ProfileCounterAnchorContract.md` — own their distinct direct Player-symbol
boundaries; this Plan must not duplicate those seams or move unrelated
Player-named declarations from the game-owned wrappers.

## Acceptance criteria

- A complete field inventory and selected boundary are recorded before edits.
- Existing explosion and wind outputs, logs, frame CRCs, and replay sequences
  match the current values.
- Engine code reads only the selected generic tuning contract and does not
  name Player declarations.
- Client and server compile with no new main-loop allocation.

## Notes

The Plan schedules the actionable residual pass; FleetSelection and other
keep-in-game mechanisms are intentionally omitted.
