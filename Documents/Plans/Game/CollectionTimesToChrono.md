<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-06T17:33:04.338Z","dependsOn":[]} -->
# Store every frame-collection time value as std::chrono::duration<float>

## Context

`Documents/C++StyleGuide.txt` rule 12 (`:95-99`) requires `std::chrono` for durations and times. The frame collections store and compute seconds as bare `float`. Collection code already wraps and unwraps those floats with `std::chrono::duration<float>` at many sites, for example `ExplosionsUpdate.cpp:26,36` and `PuffsUpdate.cpp:25`. Some interfaces are already chrono: the controller keyframe `times[]` (`Puffs.h:43`, `WindRadials.h:34`, `CollectionController.h:43`), the Missiles Sync fields (`Missiles.h:178,182-183`) and constants (`Missiles.h:21-22`), `PlayersCombat.cpp:27-28`, and the wire `StatusChange.h` fields (`:79,115-132`). The stored columns are not.

User decision (this session, binding): migrate every float time column and field in the frame collections, engine and game, to `std::chrono::duration<float>`, not only the two sites the style sweep recorded. Origin: `Documents/Investigations/ChangeWorkflow/StyleGuideSweepDeferredFixes.md`. Its `## Engine/` "Out of the fix bound" section recorded the `Puffs.h` rule 12 entry, and its `## StyleGuideSweepProjects` section recorded the Spaceships entry F24. That record is deleted when this Plan is created.

### Inventory (current tree)

Seconds-valued `float` stored columns:

- Engine, client-only collections (no persisted format): `Puffs.h:73` `pfStartTimes`; `SmokeTrails.h:45` `pfStartTimes`; `WindRadials.h:53` `pfStartTimes`; `PointLights.h:63` `pfStartTimes`; `Sounds.h:42` `pfFadeOutTimes`.
- Engine, server-visible: `Explosions.h:189` `pfStartTimes` and `:200` `pfTrailTimes[kiMaxExplosionTrails]` (both `ExplosionsInterpolate`).
- Game: `Players.h:77-78` (`PlayersInterpolate` `pfDestroyedTimes`, `pfAnimationTimes`), `:262-263`, `:268-270`, `:272-274`, `:278` (`PlayersPostRender` `pfNextBlasterFireTimes`, `pfNextSecondarySpawnTimes`, `pfShieldCooldowns`, `pfDestroyedExplosionTimes`, `pfShieldDownSoundCooldowns`, `pfTransferLockTimers`, `pfArrivalGracePeriods`, `pfFrameChangeTimers`, `pfNavigationDelays`). `Spaceships.h:53,61` (`SpaceshipsInterpolate` `pfDestroyedTimes`, `pfAnimationTimes`) and `:149-150,152` (`SpaceshipsPostRender` `pfDestroyedExplosionTimes`, `pfNextBlasterSpawnTimes`, `pfArrivalGracePeriods`). `Missiles.h:66` (`MissilesInterpolate` `pfDestroyedTimes`) and `:126-127,129` (`MissilesPostRender` `pfTimes`, `pfDeltaRotationDelays`, `pfNextJitter`).

Spawn and Sync fields: `Sounds.h:31` `fFadeOutTime`; `Players.h:325-329,333-336`; `Spaceships.h:177-178`. Type data: `Explosions.h:114,116-118` (`fPrimaryTime`, `fTrailDelayTime`, `fTrailTimeMinimum`, `fTrailTimeRandom`).

Constants: `Players.h:21-22,24` (`kfDestroyTime`, `kfDestroyExplosionInterval`, `kfBlasterFireInterval`); `Spaceships.h:19-20`; `Spaceships.cpp:68` `kfBlastersSpawnCooldown`; `Explosions.cpp:24,26,28-30`; `MissilesUpdate.cpp:41` `kfJitterIntervalRandom`.

Time parameters: `CollectionController.h:58,87,99,135` (`fElapsedTime`, `fCurrentTime`); `WindRadials.h:79`; `Players.h:198,204` (the `PostRead` range checks `IsBlasterFireTimeInRange`, `IsNavigationDelayInRange`), `:242-243` (including `rfFrameChangeTimer`); `Spaceships.h:136-137,139`; `Frame.h:132` / `Frame.cpp:455` `BuildSpaceshipRegistryWindow`'s `const float* pfArrivalGracePeriods`.

Seed time locals: `CollectionController.h:77,151`; `Explosions.cpp:205,212-213,231,246,249`; `WindRadialsUpdate.cpp:26,35-36,50`; `MissilesUpdate.cpp:61,68,105,117,119`; `Players.cpp:436,465,471-472,519,675,690-691,696-698,700-703`; `PlayersNavigation.cpp:60,281,457`; `Spaceships.cpp:259,262,273,276,581,599-601`; `SpaceshipsNavigation.cpp:43,116,138`. These are the starting set, not the boundary. The selection rule below is the boundary.

Consumers outside the collection directories that read a migrated column: `Engine/Source/Audio/StaticVoices.cpp:456`; `MissilesRender.cpp:103-111`; `PlayersRender.cpp:85,231-233,274`; `SpaceshipsRender.cpp:116,163-165,195`; `Projects/BrokenEngineSandbox/Source/Ui/Screens/HudScreen.cpp:403`; `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerQueries.cpp:111`; `Projects/BrokenEngineSandbox/Source/Agent/Commands/CollectionLayoutCapacityFixture.cpp:47-68`; `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp`; the generated `Projects/BrokenEngineSandbox/Source/Agent/Commands/AgentFieldNames.h`.

### Serialization, CRC, and copy mechanics

- Element-type agnostic, no constraint: `MultiCrc`, `MultiWrite`, and `MultiRead` (`Engine/Source/Frame/Collections/Collection.h:28-70`) hash, write, and read each column as raw bytes through `common::Crc`/`Write`/`Read` over `std::span<const T>` (`Common/Crc.h:84-88`, `Common/Serialization.h:81-110`). Allocation, copy, and row copy (`CollectionMemory.h:57-87,226-238`) `memcpy` raw elements. They require only `std::is_trivially_copyable_v` and `alignof <= 64` (`CollectionMemory.h:62-63,76-77,229`). `std::chrono::duration<float>` holds exactly one `float`, so it satisfies both. Its object representation is the same 4 bytes as the `float` it replaces, so the CRC, save, and wire bytes of every row are unchanged.
- Constraint 1, difference logging: `common::LogDifference` (`Common/Log/LogDifference.h:28-40`) routes only `float`/`double` to the allocation-free `Wb` formatter. A `duration<float>` would fall to the default heap-allocating `std::format` path. So the `LogDifferences` call sites pass `.count()` on both operands (`Explosions.cpp:274,282`, `Players.cpp:805` and the other time-column rows there, `Spaceships.cpp:656` and peers, `Missiles.cpp:534` and peers). `Common/` stays unchanged.
- Constraint 2, the `edit_frame` agent command: `ServerFrameEdit.cpp` `ReadElement` has no `duration<float>` branch and ends in `static_assert(false)` (`:76-120`). Its array-column branch `static_assert`s a `float` element (`:198`) and assigns `FloatFromValue` (`:206`), and `explosions` `pfTrailTimes` is an editable array column. The change adds a `std::chrono::duration<float>` branch to `ReadElement` assigning `std::chrono::duration<float>(FloatFromValue(rValue, name))`, which mirrors `ServerSimulationFixtures.cpp:332-334`. Change the array branch's `static_assert` and assignment so they accept a `duration<float>` element the same way. The JSON value form stays a finite number of seconds.
- Constraint 3, JSON output: `nlohmann::json` cannot hold a `duration`, so `AgentCommandsServerQueries.cpp:111` emits `.count()`. The JSON key stays unchanged.

## Design

Selection rule: a `float` (or `float*`/`float&` member, parameter, constant, local, or column) whose quantity is seconds, declared in `Engine/Source/Frame/Collections/**` or `Projects/BrokenEngineSandbox/Source/Frame/Collections/**`, becomes `std::chrono::duration<float>` in seconds. Ratios, percents, and multipliers stay `float` (`pfTimePercents`, `fTimePercent`, `fDelayPercent`, `fDurationMultiplier`, keyframe `fPercent`). So do collision sweep fractions (`BlastersUpdate.cpp:233` `fMaxTime`, `MissilesUpdate.cpp:336` `fMaximumTime`), which are normalized tick time feeding `Collision.h:31`.

Naming (rule 3 has no duration prefix): drop only the `f` from the existing name and keep every other prefix and word. Examples: `pfStartTimes` becomes `pStartTimes`, `fNextBlasterSpawnTime` becomes `nextBlasterSpawnTime`, `kfSpaceshipDestroyTime` becomes `kSpaceshipDestroyTime`, `fDeltaTime` becomes `deltaTime`, and `rfFrameChangeTimer` becomes `rFrameChangeTimer`. These follow the existing `kMissileDestroyTime`, `deltaTime`, and `nextJitter` precedent. Where a scope already holds a `duration<float>` twin of a float value, for example `startTime(rPrevious.pfStartTimes[i])` in `ExplosionsUpdate.cpp:26`, delete the twin and use the column directly.

Determinism (`Documents/FloatingPointDeterminism.txt`; PostRender state is CRC-checked per tick). The migrated arithmetic must stay bit-identical. Mechanism:

- Every `duration<float>` operation with the seconds period and a `float` scalar reduces to the same single IEEE `float` operation on `.count()`: `+`, `-`, `*` and `/` by `float`, `duration / duration` giving `float`, comparisons, and `std::min`/`std::max`/`std::clamp`. Preserve operand order and parenthesization exactly when rewriting an expression.
- Construct every constant and literal as `std::chrono::duration<float>(<same float literal>)`, as `Missiles.h:21-22` does. Never use a floating chrono literal such as `0.25s`: its representation is `long double`, so mixed arithmetic would be promoted out of `float`. Never use an integer chrono literal in simulation math, and never `duration_cast` to another period.
- Sentinel comparisons keep their value as a duration: `== std::chrono::duration<float>(-1.0f)` and `> std::chrono::duration<float>(0.0f)`.
- Boundary conversions are exact. The frame scalars `fCurrentTime`/`fDeltaTime` (`FrameBase.h:64-65`) enter collection code as `std::chrono::duration<float>(f)`. Render, audio, HUD, `std::pow`, `EvaluateAnimation`, `std::isfinite`, and JSON consumers read `.count()`.

Format version: the serialized bytes are unchanged (byte-level finding above), so no format version changes; the root `AGENTS.md` bumps a version only for a changed format.

Generated and documentation sites: regenerate `AgentFieldNames.h` with `pwsh -NoProfile -File .agents/scripts/Write-AgentFieldNames.ps1`; never hand-edit it. Update the column names cited in `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:46` (`pfTrailTimes`), `Projects/BrokenEngineSandbox/Source/Frame/Collections/AGENTS.md:13`, and `Players/AGENTS.md:18-19`. Then run `/update-claude-docs` for the remaining AGENTS.md references. Update the column names cited in the manual feature documents `Documents/Features/Graphics/DestructionBuffer.md`, `FlipbookSpriteAnimations.md`, and `HeatDistortionAndShockwave.md`.

Follow `/add-collection-member`'s layout-change checklist for each changed column. Each changed column keeps its position in `Members()`, `SharedMembers()`, `PersistentMembers()`, and `ClientMembers()`, and only its element type changes.

Risk tier: Tier 3. Triggers: serialization and data layout (the element type of serialized, CRC'd, and save/replay columns), and determinism/CRC (rewritten simulation timer arithmetic in CRC-visible collections). It spans engine and game collections.

## Critical files

- `Engine/Source/Frame/Collections/CollectionController.h`
- `Engine/Source/Frame/Collections/Explosions/Explosions.h`, `Explosions.cpp`, `ExplosionsSpawn.cpp`, `ExplosionsUpdate.cpp`
- `Engine/Source/Frame/Collections/Puffs/`, `SmokeTrails/`, `WindRadials/`, `PointLights/`, `Sounds/` (headers and `*Update.cpp`/`*Render.cpp`)
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/` (`Players.h`, `Players.cpp`, `PlayersCombat.cpp`, `PlayersNavigation.cpp`, `PlayersRender.cpp`)
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/` (`Spaceships.h`, `Spaceships.cpp`, `SpaceshipsCombat.cpp`, `SpaceshipsNavigation.cpp`, `SpaceshipsRender.cpp`)
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/` (`Missiles.h`, `Missiles.cpp`, `MissilesUpdate.cpp`, `MissilesRender.cpp`)
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/BlastersUpdate.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.h`, `Frame.cpp`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp`, `CollectionLayoutCapacityFixture.cpp`, `AgentFieldNames.h` (generated)
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerQueries.cpp`
- `Projects/BrokenEngineSandbox/Source/Ui/Screens/HudScreen.cpp`
- `Engine/Source/Audio/StaticVoices.cpp`

## In scope

- Every inventoried column, Spawn/Sync field, type field, constant, parameter, and local, plus every other declaration the selection rule matches in the two collection trees.
- The `.count()`/`duration<float>(…)` boundary conversions at the listed outside consumers, the `LogDifferences` call sites, and `BuildSpaceshipRegistryWindow`'s parameter.
- The `ServerFrameEdit.cpp` `ReadElement` duration branch and the array-column branch; the `CollectionLayoutCapacityFixture.cpp` assignments and comparisons (wrap the fixture scalar in `duration<float>`).
- Regenerating `AgentFieldNames.h` and updating the listed documentation names.

## Out of scope

- Frame-level time scalars and signatures: `FrameInterpolateBase::fCurrentTime`/`fDeltaTime` (`FrameBase.h:64-65`), `Frame::Update`/`FrameInterpolateBase::Update` `float fDeltaTime`, `RunFrameTick`'s `fCurrentTime` (`FrameBase.h:240`), and `game::Frame::fSpawnTimer` (`Frame.h:59`). Collection code converts at the boundary.
- `Engine/Source/Frame/Collision.h:31` `pfStartTimes` and the collision sweep fractions, which are normalized tick time and not seconds.
- `StaticVoice::mfFadeOutTime` and the rest of `Engine/Source/Audio/` beyond the one read at `StaticVoices.cpp:456`.
- `DataPacker/Source/ExportJobs/Scene/SceneAnimationLoader.cpp` `pfTimes` (asset animation data, not a frame collection).
- `StatusChange.h` wire structs (already chrono), any wire, save, or replay layout change, and every `kiVersion`.
- `Common/Log/LogDifference.h` and the other `Common/` serialization and CRC helpers.
- Collection `Members()` order, tuple membership, `PersistentMembers()` choice, and every non-time column.

## Acceptance criteria

- A search of the two collection trees finds no `float` declaration whose quantity is seconds under the selection rule. Every remaining time-named `float` there is a ratio, percent, multiplier, or collision fraction.
- `/compile` builds both the client and the server (`BrokenEngineSandbox`, Debug x64) with no new warnings, Shared data (no shader or asset change). The repository static checks, including the agent field-name check, pass.
- `/agent-harness` replay determinism check passes: record a server session that exercises player and spaceship combat (explosions with trails, missiles, destroyed ships, shield cooldowns, frame transfer), then play it back with every per-tick checksum matching.
- A live client and server run of the same scenario logs no `LogDifferences` CRC mismatch.
- `edit_frame` accepts a number for a migrated scalar column (for example `players` `pNavigationDelays`) and an array of `kiMaxExplosionTrails` numbers for `explosions` `pTrailTimes`, each returning success, and rejects a non-numeric value for either with its existing error.

## Notes

- The sweep record claimed the `Puffs.h` change "alters save bytes". Current code disproves that: Puffs is a client-only collection (`FrameBase.h` `ServerCollections()` holds only `explosions` and `pushers`), so it is in no grid save, and `duration<float>` serializes as the same bytes.
- `Documents/Plans/ChangeWorkflow/ProjectSourceOfTruth.md` changes `Write-AgentFieldNames.ps1`. Whichever change lands second regenerates `AgentFieldNames.h` from its own tree, so no ordering edge is needed.
