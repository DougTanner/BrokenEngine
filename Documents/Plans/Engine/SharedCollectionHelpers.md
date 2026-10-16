<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-15T23:37:22.857Z","dependsOn":[]} -->
# Add engine helpers for the render, collision, transfer, and owned-object code the Sandbox collections each copy

## Context

The authoring session recorded two directions from the user: a second game project will be created beside `Projects/BrokenEngineSandbox`, and engine code must not name Sandbox gameplay (Blaster, Spaceship, Missile, Player, `bUseMissiles`, Fleet). The four Sandbox collections (Players, Spaceships, Missiles, Blasters) each hand-copy the generic mechanics below. None of these mechanics carries a gameplay concept, so a second game would copy them again. Each one belongs in the engine, next to the mechanism it wraps. Line numbers are at `f1d140ae`.

1. **Model-instance rendering.** Three paths share one shape: `Players/PlayersRender.cpp:46-51,177-285`, `Spaceships/SpaceshipsRender.cpp:25-214`, and `Missiles/MissilesRender.cpp:17-136`. Each has:
   - the same three `GraphicsResources` calls: `CreateDynamicBuffer` with `sizeof(shaders::ModelLayout)`, `CreateModelPipeline`, and `CreateModelPipelineShadow`;
   - a file-static `siRendered` running total and an `sbRenderActive` re-entry tripwire with its RAII guard;
   - a `BeginRender` that sums capacity over the active coordinates, resizes the buffer, and repoints binding 2 of both model pipelines;
   - per-instance `ModelLayout` writes: position, transform, inverse-transpose normal transform, zero `f4ColorAdd`, and `uiMeshDataBase` 0;
   - an `EndRender` that writes both indirect buffers.

   Players and Spaceships also skin. Each looks up the material count in the eager chunk, calls `SkinnedMaterialCount`, `AllocateMeshData`, and `AllocateJointMatrices`, then calls `EvaluateAnimation` for animation 0. Players allocates per instance (`PlayersRender.cpp:257-275`); Spaceships reserves one block per cell (`SpaceshipsRender.cpp:99-151,186-196`). `PlayersRender.cpp:199-206` re-implements `BufferManager::ResizeDynamicBufferIfNeeded` (`Engine/Source/Graphics/Managers/BufferManager.cpp:466-476`). `SpaceshipsRender.cpp:58-66` and `MissilesRender.cpp:50-58` re-implement `AccumulateRenderCapacity` (`Engine/Source/Frame/Collections/Collection.h:370-383`), which eight engine collections already call. No engine code draws a model instance.
2. **Looping animation time.** `Players/Players.cpp:509-519` and `Spaceships/Spaceships.cpp:256-262,303-312` advance a clock and wrap it with `std::fmod` against `mpAnimations[0].fDuration`. The two copies are identical apart from their guards.
3. **Owned-object removal.** Only HexShields (`HexShieldsUpdate.cpp:74-80`) and Pushers (`PushersUpdate.cpp:190-196`) have an engine `Remove(rFrame, rId)`. Fourteen sites spell out the five-argument `RemoveIndexableElementAndClearHandle` call for WindTrails, AreaLights, PointLights, SmokeTrails, or Sounds:
   - `Blasters/Blasters.cpp:147,151,155`;
   - `Missiles/Missiles.cpp:287,292,296,448,453,458,488,497`;
   - `Players/Players.cpp:186`;
   - `Spaceships/Spaceships.cpp:361`;
   - the engine's own `Explosions/Explosions.cpp:224`.

   `Missiles.cpp:449,454,459,489,498` then set to `{}` a handle that `RemoveIndexableElementAndClearHandle` has already cleared (`CollectionLifecycle.h:101-107`). Every `WindTrailsPostRender::Add` caller syncs the new trail immediately: `Players.cpp:361-368`, `Spaceships.cpp:505-513`, and `Blasters.cpp:57-64`.
4. **Point-light tuned values.** `Blasters/BlastersUpdate.cpp:118-133` chooses between each `PointLightsType` wrapper and its registration default for three values. AreaLights makes the same choice inside the engine (`AreaLightsRender.cpp:84-86`). `Blasters.cpp:67-80` syncs the same light with the raw defaults.
5. **Explosion effect defaults.** `Players.cpp:88-97`, `Spaceships.cpp:109-118`, and `Missiles.cpp:217-226` each copy the engine's six default effect indices (`Explosions.h:178-183`) into their `ExplosionType`. Those fields default to invalid (`Explosions.h:89-94`), which `ExplosionsSpawn.cpp:71-118` reads as "skip this effect". No `ExplosionType` registration anywhere relies on that skip.
6. **Client hydration.** `Network/Client/ClientSession.cpp:336-340` calls each collection's hook by hand. The names do not match: `Missiles.h:60-61` declares `ClientInit`/`ClientInitAll`, while `Blasters.h:39-40` and `Spaceships.h:61-62` declare `ClientInitialize`/`ClientInitializeAll`. `ForEachRegister` and `ForEachGraphicsResources` (`Engine/Source/Frame/FrameUtils.h:238-260`) already dispatch optional hooks over a type list. `GameInterpolateTypes` (`FrameCollections.h:12-21`) orders Blasters, Missiles, Spaceships, the same order as today's calls.
7. **Collision layer setup and outcome.** Four sites build a collision layer the same way: `Players.cpp:594-645`, `Spaceships/SpaceshipsCombat.cpp:22-38,80-119`, `BlastersUpdate.cpp:24-41,194-248`, and `Missiles/MissilesUpdate.cpp:16-32,298-351`. Each:
   - keeps thread-local flags and maximum-time vectors;
   - traces the cell exit (`TracePointToCellExit`, `FrameUtils.h:81`);
   - calls `Collision::AddLayer`.

   Blasters and Missiles also trace terrain (`TracePointAgainstTerrain`, `TerrainCollisionUtils.h:10`) and keep both hits. In PostCollision (`BlastersUpdate.cpp:261-295`, `MissilesUpdate.cpp:364-410`) they choose the entity contact first, then terrain, which wins an exact-time tie with the cell exit. That tie rule is an engine contract: `Engine/Source/Frame/AGENTS.md` "Per-object maximum-time cutoffs are exclusive".

   Four PostCollision sites read `Collision::sResultSpans` and `sLayerBaseOffsets` by raw offset: `Players/PlayersCombat.cpp:231`, `SpaceshipsCombat.cpp:142`, `BlastersUpdate.cpp:265`, and `MissilesUpdate.cpp:378`. `Collision::GetCollisions` (`Collision.cpp:628-636`) performs the same test. Nothing else outside `Collision.cpp` reads either member.
8. **Transfer walk.** Four Transfer functions hand-write the reverse walk that `engine::DestroySweep` (`FrameUtils.h:357-372`) already provides: `Players/PlayersNavigation.cpp:203-261`, `Spaceships.cpp:366-407`, `Blasters.cpp:160-197`, and `Missiles.cpp:305-350`.

## Design

This is one Plan in two stages. Each stage builds, runs, and can land on its own. Stage 1 touches only client-only state, so it cannot reach the CRC. Stage 2 edits the PostRender code that computes CRC'd state, so it must stay bit-identical, which a replay recorded by the baseline build and played back by the changed build proves. Every helper replaces one expression with the same operations in the same order. The signatures below are the author's recommendation; naming and small shape details are the implementer's choice, provided the behaviour statements hold.

### Stage 1 — client-only helpers

**A. `engine::ModelInstances` (new `Engine/Source/Graphics/ModelInstances.h/.cpp`, whole-file `BT_CLIENT`).** Recommended shape:

```cpp
class ModelInstances
{
public:
	class [[nodiscard]] Run;   // holds the re-entry flag for one Render call
	constexpr ModelInstances(common::crc_t crc, std::string_view name, common::crc_t modelCrc);
	void CreateGraphicsResources() const;                        // today's three GraphicsResources calls
	void BeginFrame(int64_t iCommandBuffer, int64_t iCapacity);  // zero the total; when iCapacity > 0, ResizeDynamicBufferIfNeeded + repoint binding 2 on both pipelines
	Run BeginRun(int64_t iCommandBuffer, int64_t iMaximumCount); // asserts no run is active and total + iMaximumCount <= buffer capacity
	int64_t EndFrame(int64_t iCommandBuffer) const;              // WriteIndirectBuffer on both pipelines; returns the total
};
void XM_CALLCONV WriteModelLayout(shaders::ModelLayout& rLayout, FXMVECTOR vecPosition, FXMMATRIX matTransform);
class ModelSkinning
{
public:
	ModelSkinning(common::crc_t modelCrc, int64_t iCommandBuffer, int64_t iInstanceCount);
	void Evaluate(int64_t iInstance, float fAnimationTime, shaders::ModelLayout& rLayout) const;
};
```

- `ModelInstances` and its `Run`:
  - `Run` gives indexed access to the layouts from the current total onward. `Commit(iCount)` adds to the total. Its destructor clears the re-entry flag, including during exception unwinding, as today's guards do.
  - A game declares one file-static `ModelInstances` per model. The constructor only stores its arguments, so the object is constant-initialized and touches no graphics state.
- `WriteModelLayout` writes `f4Position`, `f3x4Transform`, `f3x4TransformNormal` (`XMMatrixTranspose(XMMatrixInverse(nullptr, matTransform))`), a zero `f4ColorAdd`, and `uiMeshDataBase = 0`. It leaves `uiMaterialCount` unwritten, as every path does today.
- `ModelSkinning`:
  - Its constructor does Spaceships' reservation on the calling thread. When the model has animation data and `iInstanceCount > 0`, it reserves `iInstanceCount * materialCount` mesh-data slots, and when the joint total is positive it reserves the joint matrices. Otherwise it reserves nothing.
  - `Evaluate` is const, writes only instance `iInstance`'s slabs, and is safe from worker threads. It sets `uiMeshDataBase` and calls `EvaluateAnimation(0, ...)`, and does nothing when the model has no animation data.
- Game collections keep their model CRC and scale choices, the transform composition, death-shrink curves, culling, the main-menu gate, profile timers and counters, and Spaceships' `Dispatch`.
  - Players passes `BeginFrame` its main-menu-gated count total.
  - Spaceships and Missiles pass `AccumulateRenderCapacity`.
- Players moves from per-instance to whole-cell skinning reservations. `AllocateMeshData` and `AllocateJointMatrices` are bump allocators (`BufferManager.cpp:478-498`), so the offsets match exactly and the GPU sees the same data.
- Missiles keeps no skinning, as today.
- The author recommends direct includes from the three render files over an `Engine.h` entry, so the PCH does not change.

**B. Looping animation time (`Engine/Source/Graphics/AnimationData.h`).** Add `inline float AdvanceLoopingAnimationTime(float fTime, float fDeltaTime, float fDuration)`. It returns `fTime + fDeltaTime`, wrapped with `std::fmod` once the sum reaches `fDuration`. Players and Spaceships call it and keep their own guards and duration lookups.

**C. Owned-object `Remove`, and `Add` that syncs.**
- Add `static void Remove(game::Frame& __restrict rFrame, <id>& rId)` to `WindTrailsPostRender`, `AreaLightsPostRender`, `PointLightsPostRender`, `SmokeTrailsPostRender`, and `SoundsPostRender`. Each body matches `HexShieldsPostRender::Remove`.
- Switch all fourteen sites in Context item 3 to these calls, and delete the five redundant `= {}` lines in `Missiles.cpp`.
- Change `WindTrailsPostRender::Add(rFrame, rId)` to `Add(rFrame, rId, const WindTrailsInterpolate::SyncData&)`. It adds the trail, then calls `WindTrailsInterpolate::Sync(rFrame.interpolate, rId, rData)`. The three callers pass their sync data, and callers keep their own `= {}` before the call.

**D. `PointLightsInterpolate::SyncFromType(game::FrameInterpolate&, id_t, FXMVECTOR vecPosition, float fVisibleArea)`.**
- It looks up the light's row and its `puiTypeIndices` type. It writes the position and visible area, takes each of the visible intensity, lighting area, and lighting intensity from the type's wrapper when that wrapper is set and from the type's default otherwise, and sets rotation 0. Those are the exact expressions at `BlastersUpdate.cpp:128-130`.
- Both Blasters point-light syncs call it: `SynchronizeBlaster` and `BlastersInterpolate::ClientInitialize`.
- The `ClientInitialize` call now stores wrapper values in place of registration defaults. That state is client-only, and the next interpolate-phase `Update` overwrites it before render reads it (`Engine/Source/Frame/Collections/AGENTS.md` `## Core Contract`, the two-situation `Update` fan-out); the implementer confirms this.

**E. `ExplosionsInterpolate::RegisterType(int64_t& riIndex, ExplosionType type)`.**
- It hides `TypeRegistry<ExplosionType>::RegisterType`. On the client it replaces each of the six effect-index fields still at `kiInvalidControllerType`/`kiInvalidTrailType` with the matching engine index, asserting that `Register()` has set them, and then forwards.
- On the server it only forwards.
- The three game registrations drop their six designated initializers and the `BT_CLIENT` guard around them.
- With this change, a field left invalid at registration means "engine default". No caller asks to skip an effect, so the author recommends adding no opt-out.

**F. `engine::ForEachClientInitializeAll(TypeList<TS...>, game::Frame&)` (`FrameUtils.h`, under `BT_CLIENT`).**
- It calls `TS::ClientInitializeAll(rFrame)` where that hook exists, in the same shape as `ForEachRegister`.
- Rename `MissilesInterpolate::ClientInit`/`ClientInitAll` to `ClientInitialize`/`ClientInitializeAll`.
- `ClientSession::HydrateReceivedFullState` replaces its three calls with `engine::ForEachClientInitializeAll(GameInterpolateTypes {}, rReceived)`. The call order is unchanged. Players, which recreates its visuals during Update, keeps no hook.

### Stage 2 — simulation helpers (CRC-bearing)

**G. Collision cutoffs and outcome (`Engine/Source/Frame/TerrainCollisionUtils.h/.cpp`; the header gains `#include "Frame/Collision.h"`).** Recommended shape:

```cpp
// Per-row scratch for one collision layer. The owning collection keeps it in function-local thread_local
// storage: ticks run in parallel per cell, and the pointers AddLayer stores must stay valid through PostCollision.
struct CollisionLayerScratch
{
	std::vector<CollisionFlags_t> flags;
	std::vector<float> maximumTimes;
	std::vector<SegmentHit> terrainHits;
	std::vector<SegmentHit> boundaryHits;
};
void TraceCellExitCutoffs(CollisionLayerScratch& rScratch, const XMVECTOR* pVecStartPositions, const XMVECTOR* pVecEndPositions, int64_t iCount);
void TraceTerrainAndCellExitCutoffs(CollisionLayerScratch& rScratch, const CellStaticData& rStaticData, const XMVECTOR* pVecStartPositions, const XMVECTOR* pVecEndPositions, int64_t iCount);
enum class CollisionOutcome : uint8_t { kNone, kEntity, kTerrain, kCellExit };
CollisionOutcome ResolveCollisionOutcome(const CollisionLayerScratch& rScratch, int64_t iLayerIndex, int64_t i);
```

- Both trace functions resize their columns under `ScopedSuppressAllocationTracking`, so callers drop their own suppression. `AddLayer` already suppresses its one allocation (`Collision.cpp:102-106`). The functions size `flags` and leave it unfilled.
- `TraceCellExitCutoffs` (Players, Spaceships) sizes `flags` and `maximumTimes` only. It stores `boundaryHit.bHit ? boundaryHit.fTime : std::numeric_limits<float>::max()`, today's Players and Spaceships expression.
- `TraceTerrainAndCellExitCutoffs` (Blasters, Missiles) sizes all four columns and stores both hits. It computes the maximum time with today's Blasters and Missiles expression: start at `max()`, take the terrain time on a terrain hit, then `std::min` with the boundary time on a boundary hit.
- The two expressions stay separate on purpose, so no row's float can change.
- `ResolveCollisionOutcome` is valid only after `TraceTerrainAndCellExitCutoffs`. It returns `kEntity` when `Collision::GetCollisions(iLayerIndex, i)` is non-empty, else `kTerrain` when `terrainHit.bHit && (!boundaryHit.bHit || terrainHit.fTime <= boundaryHit.fTime)`, else `kCellExit` when `boundaryHit.bHit`, else `kNone`.
- **Collection sites.** Each collection keeps its layer index, the per-row `flags` values, the `CollisionLayer` category, mask, radius, damage, `bSweptTest`, and every outcome response.
  - Blasters and Missiles PostCollision switch on the outcome. Missiles reads `GetCollisions(...).front()` on `kEntity`.
  - Players and Spaceships PostCollision iterate `GetCollisions` directly. An empty span skips the loop exactly as the raw `iCount > 0` test does.
  - `Collision::sResultSpans` and `sLayerBaseOffsets` move to the private section of `Collision`. `siLayerCount` stays public for its per-tick reset (`Frame.cpp:448` today, `FrameBase.cpp` once `GameShellIntoGameBase` moves it).

**H. Transfer through `DestroySweep`.**
- The four Transfer functions call `engine::DestroySweep(rInterpolate, rPostRender, <kTransfer flag test>, <release>)`. The release lambda holds today's loop body unchanged: build the `TransferRequest`, call `PrepareTransferRequest`, LOG and `DEBUG_BREAK` on a capacity hit, call `PushTransferRequest`, release owned objects, then remove the row.
- `DestroySweep` already walks the same reverse order with the same skip test, so the author recommends no new `TransferSweep`.

## Critical files

- New: `Engine/Source/Graphics/ModelInstances.h`, `Engine/Source/Graphics/ModelInstances.cpp`
- `Engine/Source/Graphics/AnimationData.h`
- `Engine/Source/Frame/TerrainCollisionUtils.h`, `TerrainCollisionUtils.cpp`, `Collision.h`, `FrameUtils.h`
- `Engine/Source/Frame/Collections/WindTrails/WindTrails.h`, `WindTrailsUpdate.cpp`; `AreaLights/AreaLights.h`, `AreaLightsUpdate.cpp`; `PointLights/PointLights.h`, `PointLightsUpdate.cpp`; `SmokeTrails/SmokeTrails.h`, `SmokeTrailsUpdate.cpp`; `Sounds/Sounds.h`, `SoundsUpdate.cpp`; `Explosions/Explosions.h`, `Explosions.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersRender.cpp`, `Players.cpp`, `PlayersCombat.cpp`, `PlayersNavigation.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/SpaceshipsRender.cpp`, `Spaceships.cpp`, `SpaceshipsCombat.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesRender.cpp`, `Missiles.h`, `Missiles.cpp`, `MissilesUpdate.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/Blasters.cpp`, `BlastersUpdate.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp`
- Client project membership for the two new files, through `/update-vcxproj`

## In scope

Stage 1:
- **New `ModelInstances.h/.cpp`:** `ModelInstances` (with `Run`), `WriteModelLayout`, and `ModelSkinning`, per design A. Add the client project and filter entries through `/update-vcxproj`; if `Documents/Plans/Engine/SharedEngineProjectFiles.md` has landed, they go in `EngineClient.vcxitems`.
- **`PlayersRender.cpp`, `SpaceshipsRender.cpp`, `MissilesRender.cpp`:**
  - `GraphicsResources`, `BeginRender`, `Render`, and `EndRender` switch to design A.
  - Delete each file's `siRendered`, `sbRenderActive`, and `*RenderActiveGuard`.
  - `DebugRender`, `RenderCombatAim`, `RenderNavigation`, and `FindFlagshipPosition` stay unchanged.
- **`AnimationData.h`:** `AdvanceLoopingAnimationTime`. The fmod blocks at `Players.cpp:513-517` and `Spaceships.cpp:306-310` call it.
- **The five engine `PostRender` structs in Context item 3:** add `Remove`. In `WindTrailsPostRender::Add`, change the signature and add the sync.
- **Removal and `Add` call sites:**
  - the fourteen removal sites in Context item 3, with the five redundant clears;
  - the three `WindTrailsPostRender::Add` + `Sync` pairs (`Players.cpp:361-368`, `Spaceships.cpp:505-513`, `Blasters.cpp:57-64`).
- **`PointLightsInterpolate::SyncFromType`:** declaration and definition. The point-light branches of `SynchronizeBlaster` (`BlastersUpdate.cpp:120-133`) and `BlastersInterpolate::ClientInitialize` (`Blasters.cpp:67-80`) call it.
- **Explosion registration:** `ExplosionsInterpolate::RegisterType`. The six `.i*TypeIndex` initializers and their guards come out at `Players.cpp:90-97`, `Spaceships.cpp:111-118`, and `Missiles.cpp:219-226`.
- **Client hydration:**
  - `ForEachClientInitializeAll` in `FrameUtils.h`;
  - the Missiles rename (`Missiles.h:60-61`, `Missiles.cpp:137,166,175,425`);
  - `ClientSession::HydrateReceivedFullState`'s three calls (`ClientSession.cpp:338-340`).

Stage 2:
- **`TerrainCollisionUtils.h/.cpp`:** `CollisionLayerScratch`, the two trace functions, `CollisionOutcome`, and `ResolveCollisionOutcome`, per design G.
- **Collection collision code:**
  - `PreCollision` in Players (`Players.cpp:594-645`), Spaceships (`SpaceshipsCombat.cpp:22-38,80-119`), Blasters (`BlastersUpdate.cpp:24-41,194-248`), and Missiles (`MissilesUpdate.cpp:16-32,298-351`), including each file's scratch struct, accessor, and `sCollisionFlags` declaration;
  - the raw-offset tests and the `GetCollisions` loops in the four `PostCollision` bodies (`PlayersCombat.cpp:231-249`, `SpaceshipsCombat.cpp:142-166`, `BlastersUpdate.cpp:261-295`, `MissilesUpdate.cpp:376-409`).
- **`Collision.h`:** move `sResultSpans` and `sLayerBaseOffsets` to `private`.
- **Transfer:** the reverse loops of `PlayersPostRender::Transfer`, `SpaceshipsPostRender::Transfer`, `BlastersPostRender::Transfer`, and `MissilesPostRender::Transfer` become `DestroySweep` calls. Their bodies move unchanged into the release lambda.

Both stages:
- **Documentation the change makes wrong, through `/update-claude-docs`:**
  - `Projects/BrokenEngineSandbox/Source/Frame/Collections/AGENTS.md` `## Game-Specific Rules`: the collision-scratch bullet and the hydration bullet;
  - `Engine/Source/Frame/Collections/AGENTS.md` `## Identity, Serialization, and CRC`: the removal bullet;
  - `Engine/Source/Graphics/AGENTS.md`: one line placing `ModelInstances`;
  - `Engine/Source/Frame/AGENTS.md`: the cutoff bullet, to name `ResolveCollisionOutcome` as the owner of the tie rule;
  - `.agents/skills/add-collection/references/worker.md:25,115`: the `ClientInit`/`ClientInitAll` names, and the fact that `ClientInitializeAll` is picked up by `ForEachClientInitializeAll`.

## Out of scope

- **A `Spawn(SpawnInfo)` prologue helper.**
  - The prologue varies per collection: Players asserts the bounds test while the others refuse, between two and four vectors are validated, and the add is `AddElement` or `AddIndexableElement`.
  - A shared helper would not stay small.
  - The prologue's order rule stays in `Engine/Source/Frame/AGENTS.md`.
- **Generated or shared `LogDifferences`**, and the `Players.cpp:786` label fix, which `Documents/Plans/Game/SandboxSmallCleanups.md` owns.
- **`MissilesUpdate.cpp:124-134` velocity integration.** It resembles `ApplyMovement` but differs, and switching it would change the CRC.
- **Templating `PrepareTransferRequest`/`PushTransferRequest` on payload type**, which `Documents/Plans/Engine/GameShellIntoGameBase.md` owns, **or merging the capacity-hit LOG/`DEBUG_BREAK`**, which stays in each producer under every Plan. This Plan moves those lines into the sweep lambda unchanged.
- **The explosion particle multiplier wrappers** (`pParticle*Scale`), which `Documents/Plans/Engine/EngineOwnedSaveSettingsAndSwitches.md` lists.
- **Engine comments that name Sandbox gameplay**, such as `AnimationData.h:41`, which `Documents/Plans/Engine/RemoveSandboxGameplayFromEngine.md` owns.
- **Game-owned behaviour:** navigation modes, fleet following, steering, damage and health, homing, registry acquisition, status-change handling, and tuning tables. Also model CRC and scale selection, per-entity transform composition, death-shrink curves, culling, the main-menu gate, and the profile counters and timers.
- **Engine internals this Plan only calls:**
  - `ModelLayout`, shaders, `ModelPipeline`, `DynamicPipelines`, and `BufferManager`;
  - the `Collision` algorithm, `CollisionLayer`, and event ordering;
  - engine collections' own render paths.
- **Consolidating the Missiles `Fall`/`Explode`/`RemoveOwnedObjects` teardown** beyond switching their calls to `Remove`.
- **Any save, replay, wire, or collection-layout change.** No version bumps.

## Risk tier and invariants

**Tier 3.** The change spans independently owned subsystems: engine Graphics, engine Frame collision, engine Collections, the game collections, the game client session, and the add-collection skill. Stage 2 also edits the code that computes CRC'd PostRender state, a determinism/CRC surface where any changed operation alters the per-tick CRC.

Invariants:
- PostRender state and the per-tick CRC stay bit-identical. Every collision cutoff, outcome choice, flag, iteration order, and RNG draw matches today's expression for expression.
- Terrain wins an exact-time tie with the cell exit, and entity contacts beyond either cutoff stay excluded.
- Collision scratch stays thread-local, allocation-free when default-constructed, and valid from `AddLayer` through PostCollision.
- The four transfer walks stay reverse and swap-and-pop safe.
- Render calls across coordinates stay sequential on the main thread, and the re-entry tripwire still fires on overlap.
- Spaceships' skinning stays one bulk reservation per cell before worker dispatch, with disjoint per-instance slabs.
- Client-only objects keep their current visual-ID stream and order: hydration order is unchanged, and no new `Add` call is made.
- The new engine code names no game type or gameplay concept.

## Acceptance criteria

1. The Debug and Release client and server build cleanly after each stage.
2. **Bit-identical simulation (after Stage 2, and after Stage 1 if it lands alone).**
   - On a Debug server built from `f1d140ae`, record a replay with `/agent-harness`. The recording covers more than one active coordinate, blaster and missile hits, terrain impacts, and at least one cross-cell transfer.
   - The Debug server built from the change plays that recording through a completed loop (`End replay <tick>, looping`).
   - No checksum-mismatch, `LogDifferences CRC`, or `CONFIRMED DESYNC` line is newly appended.
   - The changed build also passes the replay determinism acceptance in `Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md`.
   - If the harness cannot play one build's recording on the other build, report the check `BLOCKED` with that reason rather than substituting a weaker check.
3. **No live desync.** In a live client and server session of at least two minutes, with combat and a transfer, neither log gains a `CONFIRMED DESYNC` or CRC-mismatch line.
4. **Rendering as before.**
   - Harness screenshots from the baseline and changed client show, in a comparable scene:
     - fleet ships, enemy spaceships with their animation, and missiles in flight;
     - their model shadows;
     - a dying ship shrinking;
     - lit enemy blasters.
   - `query_profile` reports non-zero `SpaceshipsRendered` and `MissilesRendered` counters.
5. **Repository-search checks.**
   - `git grep` over `Projects/` finds no `RemoveIndexableElementAndClearHandle(rFrame.interpolate.`, no `CreateModelPipeline`, no `sResultSpans`, and no `ClientInit(`.
   - `git grep -n "sResultSpans\|sLayerBaseOffsets"` matches only `Collision.h` and `Collision.cpp`.
   - None of the four Transfer functions contains `iCount - 1; i >= 0`.
   - The new engine files match no `Blaster|Spaceship|Missile|Player|bUseMissiles|Fleet` pattern.

## Notes

- `Documents/Plans/Game/ShipTypeSplit.md` replaces `Players/` and `Spaceships/` with Fighters, Omnis, and Battleships, and its decision 17 doubles the model pipelines per type. `ShipModels.md` and `CollectionTimesToChrono.md` (which converts `pfAnimationTimes` and `pfDestroyedTimes` to `std::chrono::duration<float>`) edit the same render and update files.
  - Whichever lands second rebases onto the other's edits.
  - If `CollectionTimesToChrono` lands first, `AdvanceLoopingAnimationTime` takes and returns the duration type.
  - The author recommends landing this Plan before `ShipTypeSplit`, so the three ship types instantiate `ModelInstances` and the collision helpers instead of tripling the copies.
- If `Documents/Plans/Engine/GameShellIntoGameBase.md` lands first, the design H release lambdas pass `rFrame.postRender.transferRequests` to its engine `PrepareTransferRequest`/`PushTransferRequest` templates; nothing else in design H changes, so no ordering edge is needed.
- `RemoveSandboxGameplayFromEngine` (the `globalPlayerId` rename at `PlayersNavigation.cpp:241`), `GameShellIntoGameBase` (the transfer request calls and the `siLayerCount` reset), and `SandboxSmallCleanups` overlap textually with in-scope lines. None of them needs to land first.
- The `.agents/skills/add-collection/references/worker.md` edit triggers `/external-skill-creator` validation.
