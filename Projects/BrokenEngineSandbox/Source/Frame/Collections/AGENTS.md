# Collections - Game Entities

## Overview

Game-specific SOA collections built on the engine Collection framework (`../../../../../Engine/Source/Frame/Collections/AGENTS.md`). Children document behavior-specific invariants only.

## Game-Specific Rules

- Transferable entity collections resolve collision/terrain outcomes before marking surviving out-of-bounds rows for Transfer.
- Players and Spaceships stop at terrain in PostRender Update: each sweeps its disc from the previous to the current Interpolate position against the cell's elevation grid, blocked at the base height (`gBaseHeight`), and writes the resolved position back to the current Interpolate row. On contact Players slide along the terrain once and Spaceships bounce off it. A start already inside terrain logs a warning; spawn sites test at most the body's center against terrain, so a warning right after a spawn is expected.
- Collision scratch uses heap-suppressed `thread_local` vectors because cell ticks dispatch in parallel and their backing storage must survive through PostCollision; the frame workbuffer cannot own it.
- Collections with client-owned visual/audio objects hydrate them after shared state arrives. Players instead recreate missing owned visuals during Update. Hydration is filling a newly allocated collection slot from transferred or loaded data.
- Players and Spaceships receive a fresh one-second arrival grace timer after transfer. It suppresses targeting and behavior scans, not collision or damage.
- A transfer arrival restores carried gameplay state verbatim except arrival grace, which is an intentional fresh one-second arrival default, Players' navigation state, which resets to spawn defaults, Spaceships' previous-frame reaction flags, which restart from defaults, and client-only visual or debug tuning and shield animation state, which use canonical client defaults; other fresh gameplay spawn defaults and fresh random draws apply only to genuine spawns. Select between transferred gameplay values and fresh gameplay spawn defaults from an explicit transfer marker, not by testing a carried value for zero or non-positive. One legacy check on a carried value's magnitude predates the rule and survives as a proven non-defect, not a counterexample: Spaceships' `pfHealths` (the misread it could cause is unreachable — a spaceship at or below zero health explodes before it can be marked for transfer). Do not copy it.
- Spawn and Transfer sites validate position and velocity XMVECTORs through `common::ValidateVector<IS_POSITION>()`, Transfer sites by way of the shared `PushTransferRequest` in `Frame.h`. Cross-cell handoff is where those vectors get rebuilt from wire data, so these checks are what turn a wrong W lane into an immediate assert instead of a slowly drifting position. New collections and new spawn overloads keep those `ValidateVector` calls. In a `Spawn(SpawnInfo)` overload the bounds test runs before them (`../../../../../Engine/Source/Frame/AGENTS.md`).
- Shared Update fields are loaded from the previous row and stored to the current row unconditionally. Persistent transition-only fields copied by `AllocateAndCopy` are the documented exception.
- Pack related multi-valued state into the collection `Flags` type where appropriate. Use `/add-collection-member` for every layout change.
- Debug geometry reads positions from the fully interpolated frame; PostRender may supply only flags, metadata, and static world positions.

## Collections

- `Blasters/AGENTS.md`, `Missiles/AGENTS.md`, `Players/AGENTS.md`, `Spaceships/AGENTS.md`

## See Also

- `../../../../../Engine/Source/Frame/Collections/AGENTS.md`
