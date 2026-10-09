<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-08T23:52:30.832Z","dependsOn":["Documents/Plans/Engine/SplitIslandTerrainCollision.md","Documents/Plans/Game/SpaceshipZDrift.md"]} -->
# Flight plane table handed to the engine at startup, replacing gBaseHeight

Tier 3: the change spans independently owned subsystems (Engine Frame, Graphics, Audio, Ui, Main; game Frame, Graphics, Agent) and adds an engine/game startup contract; sim readers of the replaced value are CRC'd, though the value itself does not change. No pack, wire, handshake, save, or replay format is touched. Line numbers cite baseline `c1420821` unless marked `90fa6c85`. Where a statement here and the code disagree, the code wins; report the contradiction instead of matching one side to the other. First Plan of the flight-plane series; the series term is "flight plane" (never "layer", which CollisionLayer and smoke already use).

## Context

Every ship today flies at one height, `gBaseHeight`: `Wrapper gBaseHeight(6.0f, 0.0f, 20.0f)` (`Engine/Source/Ui/WrapperBase.cpp:9`, declared `WrapperBase.h:245`), an internal-only wrapper bound to no UI. Simulation and render on both builds read it, and nothing makes the client and server agree on its value.

- Sim readers (CRC'd PostRender state): `Players.cpp:261,478,671`, `PlayersNavigation.cpp:356,468` and `:481` (`90fa6c85`), `Frame.cpp:264,297,387`, `Spaceships.cpp:589`, `SpaceshipsNavigation.cpp:140` (`90fa6c85`) (all under `Projects/BrokenEngineSandbox/Source/Frame/`), `Engine/Source/Frame/NavQuery.cpp:574-575,608-610`, `Engine/Source/Frame/IslandTerrain.cpp:943` (`90fa6c85`), and the server nav bake threshold `Engine/Source/Main.cpp:342` through `game::NavigationThresholdElevation` (`Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.h:34-37`).
- Harness and camera: `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.cpp:353,441,538`, `Projects/BrokenEngineSandbox/Source/Graphics/Camera.cpp:15,32,37`.
- Render and audio (client only): `Engine/Source/Graphics/GraphicsUtils.cpp:104-110` (`ProjectToBaseHeight`), `Engine/Source/Graphics/Render/GlobalUniforms.cpp:484-486` (`fBaseHeight`, `fBaseHeightInverse` uniforms), `Engine/Source/Graphics/Render/MainUniforms.cpp:23,62,160`, `Engine/Source/Audio/StaticVoices.cpp:578`, `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersRender.cpp:155,165,172`.
- Shaders read only the uniform `globalLayout.fBaseHeight` / `fBaseHeightInverse` (`Engine/Data/Shaders/ShaderGlobalLayout.h:11-12`; `ShaderFunctions.h` `BaseHeightPosition`, `Water.frag`, `WaterReflectionProjection.h`, `Terrain.frag`, `Model.frag`, `VisibleLight.frag`, `LightingSpread.frag`, `ParticlesRender.frag`, `HexShield.vert`). They never read the wrapper.
- Comments naming the wrapper: `WrapperBase.h:12`, `SpaceshipsNavigation.cpp:76`, `PlayersNavigation.cpp:168` and `:480` (`90fa6c85`), `StaticVoices.cpp:574`, `MainUniforms.cpp:110`.
- Swept terrain contact is the engine function `engine::ResolveDiscAgainstTerrain(..., float fThresholdHeight, bool bSlide)` (`Engine/Source/Frame/IslandTerrain.h:203`, `90fa6c85`). Both callers pass `gBaseHeight.mfCurrent` as `fThresholdHeight`: `PlayersPostRender::ResolveTerrainContact` (`PlayersNavigation.cpp:478-493`, `90fa6c85`) and `SpaceshipsPostRender::ApplyTerrainBounce` (`SpaceshipsNavigation.cpp:138-157`, `90fa6c85`). The function's overlapped-start snap reads `gBaseHeight` itself (`IslandTerrain.cpp:943`, `90fa6c85`), and `Documents/Plans/Engine/SplitIslandTerrainCollision.md` (prerequisite) moves that body unchanged into `Engine/Source/Frame/TerrainCollisionUtils.cpp`. `Documents/Plans/Game/SpaceshipZDrift.md` (prerequisite) adds one more read: the Z pin after the position integration in `SpaceshipsInterpolate::Update` (`Spaceships.cpp:279`). This Plan replaces all of them.
- Engine/game hook precedent: the game already hands the engine its navigation inputs at startup as compile-time values. `Engine/Source/Main.cpp:12` includes the game header `Frame/TerrainUtils.h` and `:342` calls `gpIslandTerrain->WaitForElevationMaps(game::NavigationThresholdElevation(gBaseHeight.mfCurrent), game::NavigationClearanceMeters())`, both `constexpr` functions over game constants (`TerrainUtils.h:26-39`). `Engine/Source/AGENTS.md` `## Hub Conventions` allows engine code to consume game compile-time symbols the game is required to provide; `Engine.h` must not include game headers, so the hook stays in `Main.cpp`.
- Nothing about flight planes is baked offline: navigation contours are built at server startup from the threshold above, and terrain collision samples elevation at runtime. DataPacker never needs the plane heights, so no data file or pack chunk is involved; client and server agree because both compile the same table, as for every other gameplay constant.
- Include-path collision to avoid: the game PCH includes `Frame/Frame.h` from `Projects/BrokenEngineSandbox/Source/` and `Engine.h` includes `Frame/IslandTerrain.h` from `Engine/Source/`, so both `Frame/` directories are on the include path and the game header must not share the engine header's file name.
- Terrain elevation range: every island's `Island.json` (`Engine/Data/Islands/01|02|03`) has `elevationMeters: 100.0`; the beach is engine Z 0 and the sea floor is `common::kfSeaBottomMeters` = -10 (`Common/DataFile.h:15-21`, `DataPacker/Source/AGENTS.md` elevation paragraph), so no heightmap texel exceeds `IslandHeader::fWorldElevationMeters + kfSeaBottomMeters` = 90 m. That per-island ceiling is in every `IslandTemplate` at `IslandTerrain` construction (`Engine/Source/Frame/IslandTerrain.cpp:26-38`), read from the lazy chunk headers before any chunk data is resident.
- Boot order: client constructs `IslandTerrain` (`Main.cpp:272`), `IslandTerrainResidency` (`:273`), then `HandleEagerLoadCompletion` (`:278`), `WaitForElevationMaps` (`:282`), then `Camera` and `Game` (`:292-293`); server constructs `IslandTerrain` (`:337`), `HandleEagerLoadCompletion` (`:338`), `WaitForElevationMaps` (`:342`), then `Game` (`:344`).

## Decisions

Each decision is recorded as made; the alternatives are not open.

1. **Planes for BrokenEngineSandbox:** three, ascending: low 6.0 m with terrain collision (today's `gBaseHeight` default, so behaviour is unchanged), medium 50.0 m with terrain collision, high 100.0 m without terrain collision. 100 m clears the 90 m ceiling of every current island by 10 m; 50 m is the midpoint of the colliding range. Only the low plane is consumed by this Plan.
2. **Table in game code:** `game::kFlightPlanes`, an `inline constexpr std::array<engine::FlightPlane, 3>` in the new game header `Projects/BrokenEngineSandbox/Source/Frame/FlightPlaneTable.h` (both builds, no guard), following the `inline constexpr` gameplay-constant convention of `Players.h:20-40`. The file name differs from the engine's `Frame/FlightPlanes.h` because both `Frame/` directories are on the include path (Context).
3. **Compile-time validation in the game header:** `static_assert`s that the table has at least one plane, at most `engine::kiMaxFlightPlanes`, and strictly ascending positive heights (a `constexpr` helper over the array). Flag bits are closed by the enum, so no flag check is needed. No runtime validation of the table: it is code, not input (`.agents/references/cpp-conventions.md`, error handling at trust boundaries only).
4. **Engine types and loader:** `engine::FlightPlane`, `engine::FlightPlaneFlags`, `engine::kiMaxFlightPlanes = 8`, and `engine::FlightPlanes` in `Engine/Source/Frame/FlightPlanes.h/.cpp`, both builds. `FlightPlanes` is constructed once on both builds in `Main.cpp` from `game::kFlightPlanes`, published through `gpFlightPlanes` (manager convention, `.agents/references/cpp-conventions.md`), and immutable afterwards so frame-tick worker threads read it without synchronization. It stores a `std::span<const FlightPlane>` over the game's table, which has static storage duration. The engine names the game symbol only in `Main.cpp`, exactly as it names `game::NavigationThresholdElevation` today; the engine header names no game concept.
5. **Boot terrain check:** in the `FlightPlanes` constructor, for every `IslandTemplate` in `gpIslandTerrain->mIslands` the ceiling `fWorldElevationMeters + common::kfSeaBottomMeters` must be strictly below `Highest().fHeightMeters`. The ceiling bounds every heightmap texel (Context), so no heightmap scan is needed and the check runs before island chunk data is resident. A violation logs `kError` naming the island CRC, its ceiling, and the plane height, then `ASSERT`s: the table and the island data disagree and nothing is clamped.
6. **Construction point:** directly after `IslandTerrain` on both builds, client `Main.cpp:272` (before `IslandTerrainResidency`) and server `:337`, so the ceiling check has `gpIslandTerrain->mIslands` and `WaitForElevationMaps(:342)` can pass `Lowest()`. It precedes `Camera`, `Game`, and every reader.
7. **Client/server agreement needs no handshake change.** Both executables compile `game::kFlightPlanes`; a mismatch is a mismatched build, the same situation as any other differing gameplay constant. `kiProtocolVersion`, `mPackIntegrityToken`, and `Frame::kiVersion` are untouched: no CRC'd value changes.
8. **Shader names stay.** `fBaseHeight`, `fBaseHeightInverse`, `BaseHeightPosition`, `ToBaseHeight`, and `ProjectToBaseHeight` keep their names and now mean the lowest flight plane's height; `GlobalUniforms.cpp` feeds them from `Lowest()`. Renaming is not needed for correctness and would touch every shader, so it is left out of this Plan.
9. **`gBaseHeight` is deleted**, not kept beside the table.

## Design

Header-style declarations; names are exact.

`Engine/Source/Frame/FlightPlanes.h` (both builds, no guard; aggregated in `Engine.h` directly after `Frame/TerrainCollisionUtils.h`, which SplitIslandTerrainCollision adds directly after `Frame/IslandTerrain.h` (`Engine.h:74`)), `FlightPlanes.cpp`:

```cpp
namespace engine
{

enum class FlightPlaneFlags : uint8_t
{
	kTerrainCollision = 0x01,
};
using FlightPlaneFlags_t = common::Flags<FlightPlaneFlags>;

struct FlightPlane
{
	float fHeightMeters = 0.0f;
	FlightPlaneFlags_t flags;
};

inline constexpr int64_t kiMaxFlightPlanes = 8;

// The game's flight-plane table, handed over once at boot on both builds and immutable afterwards, so tick code on
// worker threads reads it without synchronization. Index 0 is the lowest plane; heights ascend.
class FlightPlanes
{
public:

	explicit FlightPlanes(std::span<const FlightPlane> planes);
	~FlightPlanes();

	[[nodiscard]] int64_t Count() const;
	[[nodiscard]] const FlightPlane& Plane(int64_t iIndex) const;
	[[nodiscard]] const FlightPlane& Lowest() const;   // Plane(0)
	[[nodiscard]] const FlightPlane& Highest() const;  // Plane(Count() - 1)

private:

	std::span<const FlightPlane> mPlanes;
};

inline FlightPlanes* gpFlightPlanes = nullptr;

} // namespace engine
```

The constructor asserts `gpFlightPlanes == nullptr`, assigns it, stores the span, and runs decision 5. The destructor nulls the global.

`Projects/BrokenEngineSandbox/Source/Frame/FlightPlaneTable.h` (both builds, no guard; included only by `Engine/Source/Main.cpp`, beside `Frame/TerrainUtils.h` at `Main.cpp:12`):

```cpp
namespace game
{

inline constexpr std::array<engine::FlightPlane, 3> kFlightPlanes {{
	{.fHeightMeters = 6.0f, .flags = engine::FlightPlaneFlags::kTerrainCollision},
	{.fHeightMeters = 50.0f, .flags = engine::FlightPlaneFlags::kTerrainCollision},
	{.fHeightMeters = 100.0f},
}};

constexpr bool FlightPlanesAscend(std::span<const engine::FlightPlane> planes);  // every height > 0 and > its predecessor

static_assert(kFlightPlanes.size() >= 1);
static_assert(kFlightPlanes.size() <= engine::kiMaxFlightPlanes);
static_assert(FlightPlanesAscend(kFlightPlanes));

} // namespace game
```

**Replacement rule.** Every `gBaseHeight.mfCurrent` read becomes `engine::gpFlightPlanes->Lowest().fHeightMeters` (the semantically right plane at every current site: everything lives on the lowest plane). `Main.cpp:342` passes `gpFlightPlanes->Lowest().fHeightMeters` to `game::NavigationThresholdElevation`, whose signature is unchanged. `GlobalUniforms.cpp:484-486` reads `Lowest()` into the unchanged uniforms. Comments that name `gBaseHeight` are reworded to "the lowest flight plane"; `WrapperBase.h:12` drops the `gBaseHeight in NavQuery.cpp` example and keeps the rule.

**Main.cpp construction.** Client: `auto pFlightPlanes = std::make_unique<FlightPlanes>(game::kFlightPlanes);` directly after `:272`. Server: the same directly after `:337`. `Main.cpp` gains `#include "Frame/FlightPlaneTable.h"` beside `:12`.

## Critical files

- `Engine/Source/Frame/FlightPlanes.h`, `FlightPlanes.cpp` (new), `Engine/Source/Engine.h`, `Engine/Source/Main.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/FlightPlaneTable.h` (new)
- `Engine/Source/Ui/WrapperBase.h`, `WrapperBase.cpp`
- The reader files listed under `## In scope`

## In scope

- `engine::FlightPlaneFlags`, `FlightPlane`, `kiMaxFlightPlanes`, `FlightPlanes`, and `gpFlightPlanes` (new, `Engine/Source/Frame/FlightPlanes.h/.cpp`), its `Engine.h` include, and client/server vcxproj membership through `/update-vcxproj`.
- `game::kFlightPlanes`, `FlightPlanesAscend`, and the three `static_assert`s (new, `Projects/BrokenEngineSandbox/Source/Frame/FlightPlaneTable.h`), and client/server vcxproj membership through `/update-vcxproj`.
- `Engine/Source/Main.cpp`: the `FlightPlaneTable.h` include, the two `FlightPlanes` constructions, and the `WaitForElevationMaps` argument (`:342`).
- Deletion of `gBaseHeight` (`WrapperBase.cpp:9`, `WrapperBase.h:245`) and the `WrapperBase.h:12` comment.
- Replacement reads, each in its enclosing function: `ProcessSpawnStatusChanges` (`Players.cpp:261`), `PlayersInterpolate::Update` (`:478`), `PlayersPostRender::Update` (`:671`), `PlayersPostRender::ComputeNavigation` (`PlayersNavigation.cpp:356`), `PlayersPostRender::ApplyTerrainPush` (`:468`), `PlayersPostRender::ResolveTerrainContact` (`:481`, `90fa6c85`), `SpawnSpaceshipGroup` (`Frame.cpp:264,297,387`), `SpaceshipsPostRender::Update` (`Spaceships.cpp:589`), `SpaceshipsInterpolate::Update` (the SpaceshipZDrift Z pin), `SpaceshipsPostRender::ApplyTerrainBounce` (`SpaceshipsNavigation.cpp:140`, `90fa6c85`), `engine::ResolveDiscAgainstTerrain`'s overlapped-start snap (`IslandTerrain.cpp:943` at `90fa6c85`, in `TerrainCollisionUtils.cpp` once SplitIslandTerrainCollision lands; that file's `Ui/WrapperBase.h` include, present only for `gBaseHeight` (SplitIslandTerrainCollision Design step 2), becomes `Frame/FlightPlanes.h`), `RenderNavigation` (`PlayersRender.cpp:155,165,172`), `ParseInjectedField` and `BuildInjectedEntry` (`ServerSimulationHarnessRigs.cpp:353,441,538`), `Camera::Camera` and `Camera::PullTarget` (`Camera.cpp:15,32,37`), `NavQuerySnapToNavigable` and `NavQueryDirection` (`NavQuery.cpp:574,608`), `ProjectToBaseHeight` (`GraphicsUtils.cpp:109`), `RenderGlobal`'s uniform fill (`GlobalUniforms.cpp:484`), `DebugRenderCellEdges`, `DebugRenderIslandBoundaries`, `DebugRenderNavigationData` (`MainUniforms.cpp:23,62,160`), `StaticVoices::UpdateListenerPosition` (`StaticVoices.cpp:578`).
- Comment rewording at `SpaceshipsNavigation.cpp:76`, `PlayersNavigation.cpp:168,480` (`:480` at `90fa6c85`), `StaticVoices.cpp:574`, `MainUniforms.cpp:110`.
- The `AGENTS.md` lines `/update-claude-docs` finds stale, at least: `Engine/Source/Frame/AGENTS.md` (the flight-plane table consumer and the boot ceiling check), `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` (the navigation-inputs bullet now also covers the flight-plane table handed to engine startup), `Projects/BrokenEngineSandbox/Source/Frame/Collections/AGENTS.md` (the terrain-contact bullet names `gBaseHeight`).

## Out of scope

- Per-ship current/target plane columns, climbing, per-plane navigation, cross-plane weapons, per-plane smoke, lighting falloff, camera/shadow/water/audio height work, and the ship-type rename: later Plans.
- Consuming `medium`, `high`, or `kTerrainCollision` anywhere; this Plan declares them only.
- Any data file, DataPacker job, pack chunk, `DataHeader::kiVersion`, or `mPackIntegrityToken` change for flight planes: the table is code.
- Renaming shader uniforms, `BaseHeightPosition`, `ToBaseHeight`, or `ProjectToBaseHeight` (decision 8).
- `NavigationThresholdElevation`'s signature, the nav bake, `kiNavDataVersion`, `Frame::kiVersion`, `kiProtocolVersion`, wire layouts, save and replay formats.
- A heightmap scan for true per-island peaks (decision 5 uses the header ceiling).
- Any tweak/UI binding or runtime editing of plane heights.
- Changes to `Engine.h`'s rule against game headers: the game table is included by `Main.cpp` only.

## Risk triggers and invariants

- No CRC'd value changes: every replaced read yields 6.0f exactly as before (the wrapper snapped `6.0f` with step 0 to `6.0f`), so `Frame::kiVersion` stays and replay CRCs match the baseline.
- No pack, wire, handshake, save, or replay format changes; DataPacker output is unchanged.
- Thread safety: `gpFlightPlanes` is written only by its constructor before `Game` exists and never afterwards; the span targets a `constexpr` table with static storage duration.
- Engine/game contract: the engine header names no game concept; `Main.cpp` is the only engine TU that names `game::kFlightPlanes`, matching the `game::NavigationThresholdElevation` hook (`Engine/Source/AGENTS.md` `## Hub Conventions`). `Engine.h` includes no game header.
- Table validity is compile-time (decision 3); the island ceiling check halts on a table that disagrees with the island data (decision 5). Nothing clamps or substitutes.
- Client-only render reads stay out of the CRC; `ProjectToBaseHeight` and the uniforms are unchanged in behaviour.
- `Common` gains nothing.

## Acceptance

| Criterion | Expected observation | Method |
|---|---|---|
| Client and server builds | Both succeed with `gBaseHeight` absent from the tree | `/compile` BrokenEngineSandbox client and server; `rg gBaseHeight` returns nothing |
| Table validation is compile-time | With the second plane's height temporarily lowered below the first (or a ninth plane added, or the array emptied), the build fails at the `static_assert` in `FlightPlaneTable.h`; table restored afterwards | `/compile` on the temporarily edited header |
| Behaviour unchanged | Ships spawn and fly at Z 6.0; `query_players` / `query_collection` positions report Z 6.0; terrain push, spawn, and navigation screenshots match a baseline session | Harness smoke run per `verification.md` with `query_players`, `query_collection`, screenshots |
| Client and server agree | A client connects, and `get_logs` shows no `LogDifferences CRC Client` and no `CONFIRMED DESYNC` over a multi-minute session | Harness connected session, `get_logs` baseline then diff |
| Island ceiling check | With the high plane temporarily set to 80.0 in `FlightPlaneTable.h`, the client and server halt at boot logging the island CRC, ceiling 90, and plane height; table restored afterwards | `/compile` on the edited header, harness launch, `get_logs` |
| Repeated-run replay determinism | No `LogDifferences CRC` mismatch across repeated playback loops | `/agent-harness` replay (`replay.md`) |
