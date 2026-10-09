<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-08T19:36:25.743Z","dependsOn":[]} -->
# Keep Spaceships at the base height

## Context

Spaceships spawn at `engine::gBaseHeight.mfCurrent` (`Frame/Frame.cpp:387`), but their Z can drift away from it and nothing restores it. Paths below are under `Projects/BrokenEngineSandbox/Source/` unless they start with `Engine/`.

- **Source of the Z component.** Missile-death knockback normalizes a 3D source-minus-position vector: `XMVector3Normalize(XMVectorSubtract(vecClosestSource, position))` (`Frame/Collections/Spaceships/SpaceshipsCombat.cpp:220`). The source is the missile explosion position (`Frame/Collections/Missiles/Missiles.cpp:515-521`). Missiles fall under gravity (`Frame/Collections/Missiles/MissilesUpdate.cpp:217`, `Missiles.cpp:449`), so that position is usually not at the ship's Z. While exploding, the ship's velocity is set from that direction at `kfDeathKnockbackSpeed` (`Frame/Collections/Spaceships/Spaceships.cpp:626`).
- **Spread to live ships.** `SpaceshipsInterpolate::Update` integrates the position from velocity for every ship, exploding or not (`Spaceships.cpp:279-281`), and syncs the ship's pusher to that position (`Spaceships.cpp:311`, `SyncSpaceship` at `:181-191`). `PushersInterpolate::ApplyPush` pushes along the 3D normal from the pusher to the body (`Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp:160-165`). So a drifted exploding ship pushes nearby live ships off the base height too (`SpaceshipsPostRender::ApplyPusherResponse`, `Frame/Collections/Spaceships/SpaceshipsNavigation.cpp:125-136`).
- **Nothing restores Z.** `engine::ApplyMovement<true>` (`Engine/Source/Frame/FrameUtils.h:35-52`) blends the velocity toward the ship's horizontal heading, which decays the Z velocity but never undoes the Z already integrated. `ApplyTerrainBounce` writes back the swept disc result (`SpaceshipsNavigation.cpp:140`, `:147`), which keeps the integrated position's Z (`DiscTerrainResult` comment at `Engine/Source/Frame/IslandTerrain.h:187`, written by `engine::ResolveDiscAgainstTerrain` in `Engine/Source/Frame/IslandTerrain.cpp`), and reflects the velocity about a horizontal contact normal (`SpaceshipsNavigation.cpp:154-155`). Cross-cell transfer carries the drifted position into the new cell (`SpawnTransfer.cpp:18`).
- **Effect.** The terrain contact no longer depends on ship Z: `ApplyTerrainBounce` blocks every Spaceship at `engine::gBaseHeight.mfCurrent` (`SpaceshipsNavigation.cpp:140`). Drift therefore changes only where a ship flies and renders, and the height of everything read from its position: its pusher (`Spaceships.cpp:311`), missile splash distance and knockback direction (`SpaceshipsCombat.cpp:209`, `:220`), and the blasters and explosions it spawns (`Spaceships.cpp:466-470`, `:429`). A sunk ship still draws inside terrain the contact lets it cross, below the base height.
- **Precedent.** Players pin Z to the base height on every Interpolate Update, after integrating, exploding or not (`Frame/Collections/Players/Players.cpp:474-480`).

This was found while reviewing the Spaceship swept terrain contact in `ApplyTerrainBounce` (`SpaceshipsNavigation.cpp:138-157`). That change fixes the terrain-contact threshold at the base height but does not restore the drifted Z.

## Design

The author recommends one change, mirroring Players: in `SpaceshipsInterpolate::Update`, right after the position integration (`Spaceships.cpp:279`), set the position Z to `engine::gBaseHeight.mfCurrent` before the existing W = 1 enforcement (`:281`). Since the pusher sync (`:311`) and the stored position (`:302`) both read `vecPosition`, every ship's position and pusher stay on the base plane. Every pusher is then on that plane, because Players pin theirs too, so `ApplyPush` normals become horizontal with no engine change.

Rationale: one statement removes the drift whatever its source (missile knockback, pusher chains, transferred or `edit_frame`-edited positions), keeps the mirrored Player and Spaceship patterns parallel, and touches no engine code. The other option, flattening the knockback direction (`SpaceshipsCombat.cpp:220`) and the pusher normal (`PushersUpdate.cpp:163-164`), is not recommended. It changes shared engine push behavior for every pusher user, and it leaves any other Z source, plus any position that has already drifted, unrestored.

The velocity Z component stays as today, as it does for Players. With the position pinned it has no positional effect. During a missile death the horizontal slide is unchanged, because the exploding branch already sets the velocity from the knockback direction every tick (`Spaceships.cpp:626`); the only visible difference is that a dying ship no longer rises or sinks during the knockback. The implementer reports that visible difference at review.

Bump `SpaceshipsInterpolate::kiVersion` (`Frame/Collections/Spaceships/Spaceships.h:36`, 3 to 4). The Interpolate positions are CRC'd and the change shifts computed CRCs, so older saves and replays must be rejected through the `Frame::kiVersion` sum (`Frame/Frame.cpp:35-38`).

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.h`

## In scope

- `SpaceshipsInterpolate::Update`, the position integration statements only (`Spaceships.cpp:279-281`).
- `SpaceshipsInterpolate::kiVersion` (`Spaceships.h:36`).
- The `AGENTS.md` lines `/update-claude-docs` finds stale, such as a Spaceships behavior invariant that Interpolate Update keeps ships at the base height.

## Out of scope

- `engine::PushersInterpolate::ApplyPush` and every other file under `Engine/`.
- The knockback direction computations (`SpaceshipsCombat.cpp:175`, `:220`) and `pVecDamageDirections`.
- Spaceship velocity Z, `ApplyMovement`, `ApplyPusherResponse`, and `ApplyTerrainBounce` (its swept contact already blocks at the base height).
- Players, Missiles, Blasters, spawn sites, transfer, and grid-save validation.
- Collection members, wire format, save layout, `.pack`, and shaders.

## Risk triggers and invariants

- Change Workflow Tier 3. Trigger: a determinism/CRC and save/replay compatibility surface. The CRC'd `SpaceshipsInterpolate::pVecPositions` and the pusher positions derived from it compute differently, and the `kiVersion` bump rejects older saves and replays.
- Bit determinism under `/fp:strict` (`Documents/FloatingPointDeterminism.txt`). The change adds one `XMVectorSetZ` in the shared client/server path, adds no FMA, and keeps the existing operation order.
- No allocation, threading, wire, `.pack`, or shader change.

## Acceptance

| Criterion | Expected observation | Method |
|---|---|---|
| A Spaceship moved off the base height returns to it on the next tick | After `edit_frame` sets one ship's position Z (and, separately, its velocity Z) away from `gBaseHeight`, the next `query_collection` shows that ship's position Z equal to `gBaseHeight` | Harness `edit_frame` + `query_collection` (`Projects/BrokenEngineSandbox/Documents/AgentHarness/verification.md`) |
| Ships stay on the base plane through missile kills | Every queried Spaceship position Z equals `gBaseHeight` after missile kills among grouped ships | Harness smoke run with missile fire, then `query_collection`; code reading for the knockback and pusher chain |
| Determinism holds | A recorded replay reproduces with matching CRCs; an older replay is rejected by the version gate | Harness replay determinism check |

## Notes

- No `dependsOn` edge: the Spaceship swept terrain contact this Plan builds on is in `ApplyTerrainBounce` (`SpaceshipsNavigation.cpp:138-157`), a region this Plan does not touch. After this Plan lands, a ship's Z equals the threshold that contact uses.
