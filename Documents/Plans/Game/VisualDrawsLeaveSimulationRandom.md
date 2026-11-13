<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-12T17:04:37.942Z","dependsOn":[]} -->
# Move blaster impact and missile exhaust draws off the simulation random engine

## Context

Stage S2 of the split between simulation state and client-only visual state.
It follows the stage that made Explosions client-only. That stage kept the
frame engine's name, `FramePostRenderBase::randomEngine`, and added no
visual-random helper: each client-only explosion draws from a local
`common::RandomEngine` seeded from one
`engine::Uuid::GenerateVisual(rFrame.postRender)` mint per explosion, inside
the `BT_CLIENT` guard (for example
`Frame/Collections/Fighters/Fighters.cpp:213`,
`Frame/Collections/Missiles/Missiles.cpp:233`). The rule is
`Engine/Source/Frame/AGENTS.md` `## Simulation and Visual State` (bullet
"Random draws") and `Common/Math/AGENTS.md` `## Random Streams`; both name
the two draws below as client-only values still on `randomEngine`. Line
numbers are from the tree after that stage; game paths are relative to
`Projects/BrokenEngineSandbox/Source/`.

- Blaster terrain impact
  (`Frame/Collections/Blasters/BlastersUpdate.cpp:224-241`): the position
  jitter (`:230`) and crater rotation (`:232`) draw from
  `rFrame.postRender.randomEngine` in both builds, but only the client
  consumes them (crater point light, puff, one-shot sound, all under
  `BT_CLIENT`). The simulation part is `MarkDestroy` (`:226`).
- Missile exhaust length: drawn at spawn (`Missiles/Missiles.cpp:357`) and
  every tick per flying missile (`Missiles/MissilesUpdate.cpp:146`) from
  `randomEngine` into the shared, CRC'd column
  `MissilesPostRender::pfExhaustLengths` (`Missiles/Missiles.h:142`, in
  `SharedMembers()` at `:149`). Its only reader is the client exhaust light
  sync, `SynchronizeMissile` (`Missiles.cpp:90`, `:102` intensity, called
  from `MissilesUpdate.cpp:81` and from `MissilesInterpolate::ClientInitialize`
  at `Missiles.cpp:160`), which skips falling and dying missiles.
  `MissilesPostRender` already has a `BT_CLIENT` member block and
  `ClientMembers()` (`Missiles.h:143-155`, holding `puiSounds`), and
  `ClientInitialize` (`Missiles.cpp:136`) initializes that client column for
  every row the client hydrates.

Change Workflow tier: Tier 3. Trigger: removing these draws changes the
shared random sequence and every later CRC'd value, and moving the column
out of `SharedMembers()` changes the Missiles PostRender data layout and
save/replay compatibility.

## Design

Both draws move to the established explosion pattern: a local
`common::RandomEngine` seeded from
`engine::Uuid::GenerateVisual(rFrame.postRender)` inside `BT_CLIENT`. The
author recommends these placements:

1. Blaster impact: merge the two `BT_CLIENT` blocks of the terrain-impact
   branch into one, and inside it seed one local engine from one
   `GenerateVisual` mint per impact, then draw the jitter and the rotation
   from it before the light, puff, and sound calls. Delete the
   `[[maybe_unused]]` marker and the jitter comment's shared-draw premise.
   `MarkDestroy` stays outside the guard.
2. Missile exhaust: keep `pfExhaustLengths` but make it client-only: move the
   member into the `BT_CLIENT` member block and from `SharedMembers()` into
   `ClientMembers()`; delete its `LogDifferences` line (`Missiles.cpp:478`;
   `puiSounds` has none) and its `edit_frame` column in
   `Agent/Commands/ServerFrameEdit.cpp:24`. Delete the shared draw and write
   at spawn (`Missiles.cpp:357-358`) and the shared draw in Update
   (`MissilesUpdate.cpp:146`). `ClientInitialize` writes
   `kfMissileExhaustLength` to the row before its falling early return. In
   `MissilesPostRender::Update`, under `BT_CLIENT`, seed one local engine
   from one `GenerateVisual` mint per Update call when the collection has
   rows (an exhaust draw is per tick rather than per spawned effect), draw
   each flying row's length from it where `:146` draws today, and carry the
   previous value for other rows (`:105`, `:211` move under the guard). The
   Interpolate path keeps reading the previous PostRender column, so the
   fixed tick and every render interpolation inside the window agree, as
   today. `SynchronizeMissile` keeps its `fExhaustLength` parameter.
3. Bump `MissilesPostRender::kiVersion` (`Missiles.h:119`) by one; the
   `Frame::kiVersion` total rises with it, which also covers the blaster draw
   change, so the base literal does not move.
4. Update the comment at `Missiles.h:26-27`, which states exhaust length is
   drawn from the shared engine into a CRC-covered column.
5. Update the `## Simulation and Visual State` "Random draws" bullet and the
   `Common/Math/AGENTS.md` `## Random Streams` sentence that name these two
   draws as still on `randomEngine`.

## Critical files

- `Frame/Collections/Blasters/BlastersUpdate.cpp`
- `Frame/Collections/Missiles/Missiles.h`, `Missiles.cpp`,
  `MissilesUpdate.cpp`
- `Agent/Commands/ServerFrameEdit.cpp`
- `Frame/Collections/Missiles/AGENTS.md`, `Frame/Collections/Blasters/AGENTS.md`
- `Engine/Source/Frame/AGENTS.md`, `Common/Math/AGENTS.md`

## In scope

- The terrain-impact branch of the Blasters PostCollision outcome handling
  (`BlastersUpdate.cpp:224-241`).
- `MissilesPostRender::pfExhaustLengths` and every site Design 2 lists:
  the member block, `SharedMembers()`, `ClientMembers()`, `LogDifferences`,
  `Spawn`, `MissilesPostRender::Update`, `MissilesInterpolate::ClientInitialize`;
  `MissilesPostRender::kiVersion`; the `Missiles.h:26-27` comment.
- `Frame/Collections/Missiles/AGENTS.md`: the bullet "Client-only pitch and
  exhaust values still draw from shared deterministic state" keeps pitch
  only and names the cross-cell exception in
  `Engine/Source/Frame/AGENTS.md` `## Simulation and Visual State`; the
  falling bullet drops "exhaust" from the draws falling disables.
- `Frame/Collections/Blasters/AGENTS.md`: the shared impact-jitter and
  crater-rotation bullet becomes a statement that they draw from a local
  engine seeded by a `Uuid::GenerateVisual` mint.
- `Engine/Source/Frame/AGENTS.md` `## Simulation and Visual State` "Random
  draws" bullet and `Common/Math/AGENTS.md` `## Random Streams`: drop the
  sentence naming these draws as still on `randomEngine`.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`
  only if it names `pfExhaustLengths`.

## Out of scope

- `MissilesPostRender::pfPitches` and every other missile draw (ramp delay,
  rotation limit, jitter), which are simulation or the cross-cell exception.
- Renaming `FramePostRenderBase::randomEngine` or adding a shared
  visual-random helper.
- Explosion spawning, culling, and the death timers (other stages).
- Exhaust look or tuning; backward compatibility.

## Coordination

- `Documents/Plans/Engine/CullOffscreenUnownedEffectsAtSpawn.md` also edits
  the blaster terrain-impact block in `BlastersUpdate.cpp`. Whichever lands
  second rebases onto the other: the cull test gates only the client effect
  calls and tests the jittered impact position, so inside the client block
  the jitter and rotation draws come first and the cull test follows them.
- `Documents/Plans/Game/ClientOnlyDeathExplosionTimers.md` also edits the
  Missiles `edit_frame` column list (`Agent/Commands/ServerFrameEdit.cpp:24`),
  `MissilesPostRender::SharedMembers()` (`Missiles.h:149`),
  `MissilesPostRender::LogDifferences` (`Missiles.cpp:468`, `:478`), and
  `MissilesPostRender::kiVersion`. Whichever lands second rebases onto the
  other, removes only its own column from each list, and bumps `kiVersion`
  by one more from the landed value.

## Acceptance criteria

- Builds: `/compile` BrokenEngineSandbox client and BrokenEngineSandboxServer
  Debug|x64. No ModelViewer build: no header in its project changes.
- No `randomEngine` draw remains in `BlastersUpdate.cpp`'s terrain-impact
  branch or for exhaust length, and `pfExhaustLengths` is absent from
  `SharedMembers()`.
- Determinism: a blaster that hits terrain and a flying missile, under
  `replay_record` then `replay_play`, report no CRC mismatch, and the
  connected client's `get_logs` with a `[Dd]esync` pattern returns no lines.
  The author recommends an `inject_payload` `SpawnBlaster` aimed at an island
  and a `SpawnMissile` flying in open water; no harness run has exercised
  this setup.
- Visuals: screenshots show the crater light and puff at blaster terrain
  impacts and a flickering exhaust light on flying missiles.
- Version gate: a replay or grid save from before the change is rejected.

## Notes

- Originating gap: the remaining visual draws on the simulation engine after
  the client-only Explosions stage.
- Exposure: determinism and CRC, Missiles data layout, save/replay
  compatibility, live verification.
- The visual sequence may differ between clients and after a re-simulated
  tick; the exhaust value is redrawn every tick anyway, so no visible
  continuity is lost.
- An earlier version of this Plan used a `MakeVisualRandom` helper and a
  `simulationRandom` rename; the preceding stage adopted neither, so this
  Plan uses the per-mint seeding that stage established.
