# Collections - Game Entities

## Overview

Game-specific SOA collections built on the engine Collection framework (`../../../../../Engine/Source/Frame/Collections/AGENTS.md`). Children document behavior-specific invariants only.

## Game-Specific Rules

- Transferable entity collections resolve collision/terrain outcomes before marking surviving out-of-bounds rows for Transfer.
- Ship terrain contact: [shared ship policy](Ships/AGENTS.md#ship-invariants).
- Collections with client-owned visual/audio objects hydrate them after shared state arrives. Omnis instead recreate missing owned visuals in PostRender Spawn, except the heal beam, which PostRender Update recreates. Hydration is filling a newly allocated collection slot from transferred or loaded data.
- Every ship type receives a fresh one-second arrival grace timer after transfer. It suppresses targeting and behavior scans, not collision or damage.
- A transfer arrival restores carried gameplay state verbatim except arrival
  grace, which is an intentional fresh one-second arrival default, each ship's
  navigation state, which resets to spawn defaults, Fighters' and Battleships'
  previous-frame reaction flags, which restart from defaults, every ship type's
  kept attack target, which clears because registry ids are per cell, and
  client-only
  shield animation state, which uses canonical client defaults; other fresh
  gameplay spawn defaults and fresh random draws apply only to genuine spawns.
  Select between transferred gameplay values and fresh
  gameplay spawn defaults from an explicit transfer marker, not by testing a
  carried value for zero or non-positive.
- Spawn and Transfer sites validate position and velocity XMVECTORs through
  `common::ValidateVector<IS_POSITION>()`, Transfer sites by way of the shared
  `FramePostRenderBase::PushTransferRequest` (`FrameBase.h`). Cross-cell handoff
  is where those vectors get rebuilt from wire data, so these checks are what
  turn a wrong W lane into an immediate assert instead of a slowly drifting
  position. New collections and new spawn overloads keep those `ValidateVector`
  calls. In a `Spawn(SpawnInfo)` overload the bounds test runs before them
  (`../../../../../Engine/Source/Frame/AGENTS.md`).
- Shared Update fields are loaded from the previous row and stored to the current row unconditionally. Persistent transition-only fields copied by `AllocateAndCopy` are the documented exception.
- Pack related multi-valued state into the collection `Flags` type where appropriate; transfer, destroy, and dying states go in the engine lifecycle column instead (`../../../../../Engine/Source/Frame/Collections/AGENTS.md` `## Core Contract`). Use `/add-collection-member` for every layout change.
- Debug geometry reads positions from the fully interpolated frame; PostRender may supply only flags, metadata, and static world positions.

## Model Rendering

Asset alignment comes from the scene's referenced packed model header; its
[metadata contract](../../../../../DataPacker/Source/ExportJobs/AGENTS.md#model-alignment)
is owned by DataPacker. Resolve the complete alignment matrix through
`common::GetModelAlignment` before per-entity work and worker dispatch. Append
scalar death scaling, the collection's pose rotations in their existing order,
and world translation. Keep death scaling after alignment so authored offset
shrinks with the model. Gameplay radii govern independent movement, terrain and
navigation dimensions.

## Collections

- `Battleships/AGENTS.md`, `Blasters/AGENTS.md`, `Fighters/AGENTS.md`, `Missiles/AGENTS.md`, `Omnis/AGENTS.md`
- `Ships/AGENTS.md` - Code the three ship collections share

## See Also

- `../../../../../Engine/Source/Frame/Collections/AGENTS.md`
