# Ship Wreckage

Revisit When: ship deaths feel abrupt in playtests, or after `Documents/Features/Graphics/OceanImpactSplashes.md` lands and gives falling wreckage a sea to splash into.

## Context

A destroyed spaceship explodes in place and vanishes; nothing outlives it:

- A lethal blaster hit (collision) or missile splash (area damage) calls `BeginExplosion` (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/SpaceshipsCombat.cpp`). It sets `kExploding`, starts `pfDestroyedTimes` at `kfSpaceshipDestroyTime` (`Spaceships.h`), plays the death sound, and spawns the first explosion.
- While the ship explodes, `SpaceshipsPostRender::Update` (`Spaceships.cpp`) replaces its velocity with a knockback away from the damage direction at `kfDeathKnockbackSpeed`. The `Spawn` phase hook emits a smaller explosion every `kfSpaceshipDestroyExplosionInterval`, and `SpaceshipsRender.cpp` shrinks the model by the remaining destroy fraction.
- `SpaceshipsPostRender::Destroy` removes the row once `pfDestroyedTimes` reaches zero.

No message tells the client about a destruction. The client runs the same deterministic tick itself, then rolls back and replays a cell whose CRC mismatches the server's (`Documents/Architecture/GameReconciliation.md`), marking replayed ticks with `FrameFlags::kRecalculated` (`Engine/Source/Frame/FrameBase.h`). At both `BeginExplosion` and `Destroy` the client has the ship's position and `SpaceshipsPostRender::pVecVelocities`, which is shared PostRender state. By `Destroy` that velocity is the knockback; the ship's flight velocity was overwritten on the first exploding tick.

Client-only effect collections (`Puffs`, `PointLights`, `SmokeTrails`, and others) live in the frame under `BT_CLIENT`, so they roll back and replay with it. Effects outside the frame, such as GPU particles and audio one-shots, skip `kRecalculated` ticks instead. `Explosions` is shared: its rows are CRC'd and spawning one draws from the frame random engine (`Engine/Source/Frame/Collections/Explosions/AGENTS.md`).

## Design

- **Chunks.** When a ship dies, replace it with a few chunks at its position, each with the ship's velocity plus a small random spread and spin. Chunks fall under gravity, tumble, and render through the model path the spaceship uses.
- **Smoke.** Each chunk owns a `SmokeTrails` trail synced to its position, the way a missile owns its trail (`SynchronizeMissileTrail`), using the dark wreck shade from `Documents/Features/Graphics/SmokeColorBySource.md` once that exists.
- **Secondary bursts.** A few small delayed bursts along the chunks' paths, built from the client-only explosion children (puffs, point lights, GPU particles) rather than `ExplosionsPostRender::Spawn`, whose shared row and random draws would diverge the client's CRC. Puffs already hold a future start time for delayed effects.
- **Impact.** Each tick traces the chunk's movement with `engine::TracePointAgainstTerrain` (`Engine/Source/Frame/IslandTerrain.h`), as falling missiles do. A hit at or below z = 0 (the sea test falling missiles use) triggers an ocean splash from `OceanImpactSplashes.md`; a hit above it triggers a small explosion from the same client-only children. The chunk is then removed.
- **Client-only.** Nothing collides with chunks, the server never sees them, and per-client divergence is acceptable: no shared member, wire data, or CRC exposure. Chunk randomness comes from a client-only random engine, never the frame random engine, so client and server random streams stay in lockstep.

Open choices:

- **Chunk source.** (a) DataPacker pre-splits each ship model into pieces, so the wreck matches the ship; (b) a small generic set of scrap models reused for every ship. Recommended start: (b), which needs no DataPacker change and serves every ship type.
- **Spawn moment and velocity.** At `Destroy`, after the in-place explosion, with the knockback velocity; or at `BeginExplosion`, with the flight velocity, replacing or shortening the in-place sequence.
- **Storage.** A new client-only frame collection (`/add-collection`) rolls back with the frame, so a mispredicted destruction takes its chunks with it. State outside the frame must skip `kRecalculated` ticks as audio does, and keeps the chunks of a destruction the server later contradicts.
- **Cell boundaries.** An entity's position must lie inside its own cell (`Engine/Source/Frame/AGENTS.md` `## Architecture`). A chunk reaching its cell's edge is either removed there or transferred to the neighbouring cell; a `SmokeTrails` trail never crosses a boundary, so a transferred chunk starts a fresh trail.

## Out of scope

- Simulated or colliding debris, and any server-side wreck state.
- 3D volumetric smoke; wreck smoke stays in the existing 2D smoke field.
- Damage visuals before death, which `Documents/Features/Graphics/DestructionBuffer.md` owns.

## Notes

- Origin: the user's notes — hand destroyed objects off to be covered in explosions and break apart, crash them into the ocean with splash particles, split meshes into pieces.
- `Documents/Features/Graphics/FlipbookSpriteAnimations.md` fireball sprites could dress the secondary bursts and island impacts.
- `Documents/Features/Graphics/OceanImpactSplashes.md` already lists falling wreckage as a splash source.
