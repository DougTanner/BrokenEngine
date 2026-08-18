<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T18:48:03.635Z","dependsOn":[]} -->
# Terrain Steering Tuning Contract

## Context

Terrain steering in Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.cpp:8-130
contains 18 file-scope tuning constants, including preferred elevation,
steer rate, and look-ahead distance. The algorithms have no game-specific
symbols, but they run in deterministic per-unit tick code.

The unresolved D13 question is caller-supplied runtime parameters versus
compile-time game overrides. The user-approved conversion permits both
bounded options to remain until implementation.

## Design

Choose either an engine-owned constexpr tuning type with game-provided
defaults, or a caller-supplied parameters value threaded through both
steering functions. The researched recommendation is an
engine::TerrainSteeringTuning with today’s 18 values as default member
initializers and one inline constexpr game value overriding only selected
fields. Preserve function signatures if the constexpr option is selected;
do not add runtime indirection merely to expose constants.

## Critical files

- Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.cpp:8-130
- Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.h
- The engine/game frame aggregation headers that expose steering functions
- Existing deterministic steering call sites

## In scope

- Inventory and ownership of all 18 terrain-steering constants.
- The selected compile-time or runtime tuning contract.
- Direct call-site updates required to supply the selected values.
- Preservation of current defaults, formulas, thresholds, and field order.

## Out of scope

- Steering algorithm changes, terrain tracing, navigation policy, or Player
  constants.
- Explosion/wind tuning, which has a separate Plan despite the shared pattern.
- New tuning files or runtime configuration unless the selected option needs
  one and it is explicitly justified.

## Risk tier and invariants

Expected Change Workflow Tier 3: steering participates in deterministic CRC
state and hot loops. Preserve all default values, floating-point operation
order, DirectXMath conventions, tick timing, and allocation behavior. A
runtime option must prove it does not change deterministic results.

## Coordination

No directional prerequisite is required; the historical TerrainTraceToEngine
change is already landed. ExplosionWindTuningToEngine must not duplicate this
contract or move terrain constants as part of its work.

## Acceptance criteria

- All 18 fields are enumerated and the selected ownership is compile-time
  checked for every build.
- Default steering output and CRC/replay sequences match the current game.
- A second-game configuration can omit Player concepts and override only
  intended fields.
- Client and server compile with no hot-loop allocation or new per-tick
  logging.

## Notes

The struct-of-defaults recommendation is recorded as research, not as a
preselected implementation.
