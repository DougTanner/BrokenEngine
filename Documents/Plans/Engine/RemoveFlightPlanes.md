<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-20T21:46:45.729Z","dependsOn":["Documents/Plans/Engine/SingleLayerSmoke.md","Documents/Plans/Engine/SingleLayerWorldLighting.md","Documents/Plans/Game/SingleFlightHeightShips.md"]} -->
# Remove the Flight-Plane Table

## Context

`engine::FlightPlanes` (`Engine/Source/Frame/FlightPlanes.h:6-43`,
`FlightPlanes.cpp:8-72`) borrows each project's `game::kFlightPlanes` table
(`Projects/BrokenEngineSandbox/Source/Frame/FlightPlaneTable.h`,
`Projects/ModelViewer/Source/Frame/FlightPlaneTable.h`) and answers
`Lowest()`, `Highest()`, `Plane()`, `NearestPlaneIndex()`, and
`OccupiedPlanes()`. The user asked for the multi-layer concept to be removed
entirely, delivered as a series of Plans. Earlier stages already put every
ship on one flight height, world lighting on one layer, and smoke on one
field. What remains of the concept is plumbing: the table and its second
entry, the `kTerrainCollision` flag and the per-plane navigation bake input,
one pusher zone grid per plane, the model layout's nearest-plane index, the
object-shadow B and A channels for ships on planes 1 and 2, the plane-keyed
deposit key, and the camera and audio heights derived from the highest
plane. This stage removes all of it.

The user's height decision (their words, binding): "Do 10m to start, I will
tweak later" — BES uses 10 m for gameplay, navigation contours, and graphics.
The user did not mention ModelViewer; each project supplies its own height
header, so the code forces no shared value, and the author's recommendation
is to keep ModelViewer at its current 2 m. No backward compatibility: a
changed format bumps its owning version.

## Design

- One game-supplied height. Each project replaces `Frame/FlightPlaneTable.h`
  with `Frame/BaseHeight.h` holding
  `inline constexpr float kfBaseHeightMeters` in `namespace game`: 10.0f in
  BES, 2.0f in ModelViewer. Engine and game code read
  `game::kfBaseHeightMeters` where they read
  `gpFlightPlanes->Lowest().fHeightMeters`, including the header directly as
  `FlightPlaneTable.h` is included today; it never enters `Engine.h`.
  "Base height" is the term the shaders and `ProjectToBaseHeight` already
  use. `Engine/Source/Frame/FlightPlanes.{h,cpp}` are deleted with their
  `Engine.h` include and the `FlightPlanes` construction on the client and
  server (`Engine/Source/Main.cpp:229`, `:317`) and in ModelViewer
  (`Projects/ModelViewer/Source/Main.cpp:179`).
- Navigation bake: `NavigationGraphBake::iFlightPlane` goes, every entry
  bakes its contour (the `kTerrainCollision` gate in
  `IslandTerrain.cpp:241` goes), and the server bake loop takes its threshold
  from `game::kfBaseHeightMeters`. `BuildPlaneNavigationData` becomes
  `BuildGraphNavigationData` (author's recommendation: its name names a
  per-plane graph that no longer exists).
- Pushers: one zone grid. `kiPusherLayers`, the layer dimension of
  `spppiPushersPerZone` and `sPusherZones`, the layer argument of
  `PusherZone`, the `OccupiedPlanes` loops in `SetupZones` and `ApplyPush`,
  and the cross-layer duplicate test go; a row registers in every zone its
  radius overlaps and a query reads the zone holding its position, peers in
  row order. Because a Fighter raised between the old planes summed peers
  from two layers in layer order, peer summation order can differ from
  before, so the game's `Frame::kiVersion` base literal goes from 190 to 191
  (`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:136`).
- Camera: `TiltedFootprint` (`Engine/Source/Graphics/CameraBase.cpp:57-66`)
  spans heights 0 and `kfBaseHeightMeters` instead of 0 and the highest
  plane, and the initial position and Direct targets
  (`CameraBase.cpp:120`, `:259`) sit at the base height.
- Audio: the listener sits at a fixed `kfListenerHeight` of 77 m above the
  look-at in `Engine/Source/Audio/StaticVoices.cpp` (`:572-580`), today's BES
  value of 1.1 times the 70 m plane (author's recommendation: keeps the
  manual fade distances the user hears unchanged).
- Model layout: `ModelLayout::f4PositionLayer` becomes `f4Position`, its w
  unused; `FillModelLayout` (`Engine/Source/Graphics/Render/Render.h:54-61`)
  drops `iPlane`; the Fighter, Omni, Battleship, and Missile renderers and
  ModelViewer stop computing `NearestPlaneIndex`. `ModelCommon.h` drops
  `iOutLayer`; `Model.frag` and `ModelShadow.frag` drop `iInLayer`.
- Object shadows: B and A held coverage by ships on planes 1 and 2 and are
  dropped. `ModelShadow.frag` writes zero to B and A; `Model.frag` stops
  applying object shadow to a model's own sun and moon terms (it came only
  from B and A, through `ObjectShadowAbove`, which goes) and keeps sampling R
  for its smoke. The R/G ground-coverage split keeps BES's current height
  range as one engine constant `kfObjectShadowsBlurHeightRange` of 60 m in
  `GlobalUniforms.cpp` (the 70 m plane minus the 10 m base height), which
  ModelViewer shares (last bullet), and the B and A blur sigmas stay at Sigma
  Low. `kVkFormatObjectShadows` stays
  RGBA16F and the blur stays four-channel (author's recommendation: an RG
  storage image needs the `shaderStorageImageExtendedFormats` feature, which
  the blur does not require today).
- Deposit key: `DynamicPipelines::FlightPlaneDepositKey` is deleted, and the
  AreaLights, PointLights, HexShields, Puffs, and SmokeTrails renderers key
  their deposit `kBufferMain` buffer and their deposit pipelines (HexShields:
  its shield and shield-lighting pipelines) by the collection CRC itself. No
  key collides: that CRC otherwise keys only the AreaLights and PointLights
  `kBufferVisibleLights` buffer and `kDynamicPipelineVisibleLights` pipeline
  (`AreaLightsRender.cpp:26-27`, `PointLightsRender.cpp:25-26`), which live in
  maps separate per buffer type (`BufferManager.h:118`) and per pipeline type
  (`DynamicPipelines.h:81`), and each of the five CRCs is a distinct
  collection name (`CrcConsteval("AreaLights")`, `AreaLights.h:32`, and so
  on) no other entry uses.
- Projection file: `Engine/Source/Graphics/FlightPlaneProjection.cpp`
  becomes `BaseHeightProjection.cpp` (author's recommendation: the file now
  holds only the base-height projection).
- ModelViewer keeps its flight body behaviour, but besides its height source
  and model-layout write, two values change because it compiles the shared
  `GlobalUniforms.cpp` and `CameraBase.cpp`
  (`Projects/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj:105`,
  `:137`) and its table today is 2 m and 100 m
  (`Projects/ModelViewer/Source/Frame/FlightPlaneTable.h:8-9`): its
  object-shadow R/G height range goes from 98 m to the shared 60 m, and its
  `TiltedFootprint` ceiling from 100 m to its 2 m base height. Author's
  recommendation: accept both, because ModelViewer is a developer tool and
  one shared engine constant is a smaller design than a per-project range.

## Critical files

- `Engine/Source/Frame/FlightPlanes.{h,cpp}` (deleted),
  `Projects/{BrokenEngineSandbox,ModelViewer}/Source/Frame/BaseHeight.h`
  (new, replacing `FlightPlaneTable.h`)
- `Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp`
- `Engine/Source/Frame/IslandTerrain.{h,cpp}`, `NavCellData.cpp`,
  `Engine/Source/Main.cpp`
- `Engine/Source/Graphics/Render/GlobalUniforms.cpp`, `Render.h`,
  `Engine/Data/Shaders/Model/{Model.frag,ModelShadow.frag,ModelCommon.h}`
- Project files of the BES client and server and ModelViewer

## In scope

- New `Projects/BrokenEngineSandbox/Source/Frame/BaseHeight.h` and
  `Projects/ModelViewer/Source/Frame/BaseHeight.h`; delete both
  `Frame/FlightPlaneTable.h`.
- Delete `Engine/Source/Frame/FlightPlanes.h` and `.cpp`;
  `Engine/Source/Engine.h:76` include.
- `Engine/Source/Main.cpp`: `FlightPlanes` construction (`:229`, `:317`) and
  the bake loop (`:230-239`).
- `Projects/ModelViewer/Source/Main.cpp` (`:179`),
  `ModelViewerApplication.cpp` (`:686`, `:828-829`),
  `Graphics/Camera.cpp` (`:26`).
- `Engine/Source/Frame/IslandTerrain.h` (`:57` comment, `:129-136`),
  `IslandTerrain.cpp` (`:240-246`), `NavCellData.cpp`
  (`BuildPlaneNavigationData` and its log text, the `:353` comment).
- `Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp`: the include and
  constants (`:3-18`), `PusherZone` (`:31-34`), `SetupZones` (`:60-119`),
  `ApplyPush` (`:121-194`); `Pushers.h`: the `ApplyPush` declaration comment
  naming occupied flight-plane layers (`:58-60`).
- `Engine/Source/Graphics/CameraBase.cpp`: `TiltedFootprint` and its comment
  (`:57-66`), the constructor (`:117-121`), `ResolveTarget` (`:259`).
- `Engine/Source/Audio/StaticVoices.cpp`: the listener height (`:572-580`)
  and the fade-band comment's "highest flight plane" (`:588-593`).
- `Engine/Source/Graphics/Render/MainUniforms.cpp` (`:21`, `:59`, `:172`, and
  the underwater-hull comment's "lowest flight plane" at `:107`),
  `GlobalUniforms.cpp` (the object-shadow block `:460-472` and the base
  height `:624`), `Render.h` (`FillModelLayout` and its comment, `:54-61`).
- `Engine/Source/Graphics/BaseHeightProjection.cpp` (renamed from
  `FlightPlaneProjection.cpp`): `ProjectToBaseHeight`'s height and the
  `GlobalElevation` comment; `GraphicsUtils.h` comments naming the old file
  or flight planes.
- `Engine/Source/Graphics/Managers/RenderTargetTextures.cpp`: the
  `Frame/FlightPlaneTable.h` include and the `static_assert` on the table
  size (`:10`, `:17-18`).
- `Engine/Source/Graphics/Managers/DynamicPipelines.h` (`:87-88`) and `.cpp`
  (`:291-295`): delete `FlightPlaneDepositKey`; its callers in
  `Engine/Source/Frame/Collections/{AreaLights,PointLights,HexShields,Puffs,SmokeTrails}/*Render.cpp`
  pass the collection CRC.
- `Engine/Data/Shaders/ShaderLayoutsBase.h`: `kVkFormatObjectShadows` comment
  (`:37`), `ModelLayout::f4PositionLayer` (`:313`).
- `Engine/Data/Shaders/ShaderGlobalLayout.h`:
  `fObjectShadowsBlurHeightRangeInverse` comment (`:211`).
- `Engine/Data/Shaders/ShaderFunctions.h`: `ObjectShadowAbove` (`:226-232`).
- `Engine/Data/Shaders/Model/ModelCommon.h` (`:24`, `:103-123`),
  `Model.frag` (`:76`, `:260-270`), `ModelShadow.frag` (`:28`, `:36-42`).
- Game readers of the table: `ShipBehaviours.cpp`, `ShipNavigationUtils.cpp`,
  `Fighters.h`, `Omnis.h`, `Battleships.h`, `MissilesUpdate.cpp`,
  `FightersRender.cpp` (`:129-130`), `OmnisRender.cpp` (`:158`),
  `BattleshipsRender.cpp` (`:64`), `MissilesRender.cpp` (`:78-79`),
  `TerrainUtils.h` (`NavigationThresholdElevation`'s parameter name),
  `ServerClientManager.cpp`, `ServerSimulationHarnessRigs.cpp`,
  `Graphics/Camera.cpp`, all under `Projects/BrokenEngineSandbox/Source`:
  every `gpFlightPlanes` read and `Frame/FlightPlanes.h` include, plus any
  other reader the build reports; the `OmniPatrolDiscPoint` comment's "on the
  lowest plane" (`ShipNavigationUtils.cpp:56`).
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`: the
  `Frame::kiVersion` base literal 190 to 191 (`:136`).
- Project membership through `/update-vcxproj`: the BES client and server
  `.vcxproj` and `.vcxproj.filters` and the ModelViewer `.vcxproj` and
  `.vcxproj.filters` (remove `FlightPlanes.h`/`.cpp` and `FlightPlaneTable.h`,
  add `BaseHeight.h`, rename the projection source in the client).
- Prose this stage makes wrong:
  - `Engine/Source/Frame/AGENTS.md`: the `FlightPlanes` bullet in
    `## Architecture` (`:115-123`) becomes the base-height contract
    (`game::kfBaseHeightMeters` in `Frame/BaseHeight.h`, supplied by each
    project on both builds, read directly by engine code); the
    `NavigationGraphBake` bullet (`:272-275`) and the contour-bake bullet
    (`:310-312`) in `## Terrain and Navigation`.
  - `Engine/Source/Frame/Collections/Pushers/AGENTS.md` `## Unique Aspects`
    zone bullet (`:12-32`).
  - `Engine/Source/Graphics/AGENTS.md` `:131` ("lowest flight plane").
  - `Engine/Source/Audio/AGENTS.md` `:53`.
  - `Engine/Data/Shaders/Model/AGENTS.md` (`:51`, `:57-65`) and
    `Engine/Data/Shaders/Shadow/AGENTS.md` (`:12-15`): plane index, B and A,
    and the lowest-to-highest span.
  - The AreaLights, PointLights, HexShields, Puffs, and SmokeTrails
    `AGENTS.md` lines naming `FlightPlaneDepositKey`.
  - `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` `## Invariants`
    (`:68` "lowest plane", `:76-78` the `FlightPlaneTable.h` sentence).
  - `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/AGENTS.md`
    (the pusher sentence about an occupied flight-plane layer and any
    remaining "lowest plane") and `Missiles/AGENTS.md` (`:15`, `:19`).
  - `Projects/ModelViewer/AGENTS.md` `## Application contracts` (`:43`, the
    flight-plane table sentence).
  - `.agents/skills/agent-harness/references/command-reference.md`
    `cell_coordinate_probe` (`:86-87`, "or flight-plane mapping").
- Stale non-Plan documents: delete
  `Documents/Investigations/Engine/CollisionPerFlightPlaneZoneGrid.md`
  (a per-plane collision grid); update
  `Documents/Features/Graphics/ObjectShadowPerPixelStretchBlur.md`
  `:17-20` (B and A no longer hold planes) and
  `Documents/Features/Graphics/OceanImpactSplashes.md` `:21`, `:56`
  ("lowest flight plane" becomes the base height).

## Out of scope

- Ship behaviour, targeting, navigation graphs' contents, lighting, and smoke
  beyond the renames above (earlier stages).
- The 10 m and 2 m values; terrain avoidance and steering constants.
- The object-shadow texture format, blur passes, and tuning; ship hitboxes.
- `CollisionLayer`, frame-registry ownership layers, Vulkan layers, image
  array layers, and other uses of "layer" unrelated to flight planes.
- Historical version sentences in `Documents/Architecture/Network.md`.
- Unit tests.

## Risk triggers and invariants

Tier 3: pusher zones are simulation code (determinism and CRC), the server
navigation bake changes its input, and the change spans Frame, Graphics,
Audio, shaders, the game, ModelViewer, and project membership.

- Pushers keep their contract: full 3D falloff within the source sphere,
  each peer counted once, the coincident X correction, and the per-zone cap
  with one warning per `SetupZones`.
- The server still builds navigation before any tick reads it, and the client
  still trusts navigation data from the server.
- Game base-height reads stay identical values (10 m BES); simulation code
  still never reads render state.
- `Frame::kiVersion` composes the bumped base, so older saves and replays are
  rejected.

## Acceptance criteria

1. `/compile` builds the BrokenEngineSandbox client and server and the
   ModelViewer, including shader data; `/update-vcxproj` reports the three
   projects' membership consistent.
2. No identifier containing `FlightPlane`, and no `gpFlightPlanes`,
   `NearestPlaneIndex`, `OccupiedPlanes`, `kiMaxFlightPlanes`,
   `kTerrainCollision`, `kiPusherLayers`, `f4PositionLayer`, `iInLayer`,
   `iOutLayer`, `ObjectShadowAbove`, or `BuildPlaneNavigationData` remains
   under `Engine/`, `Projects/BrokenEngineSandbox/`, or
   `Projects/ModelViewer/`, and no `AGENTS.md` there says "flight plane".
   `Frame/BaseHeight.h` holds 10.0f in BES and 2.0f in ModelViewer.
3. `Frame::kiVersion`'s base literal is 191.
4. `/agent-harness`: `cell_coordinate_probe` with navigation returns
   polygons for every graph index in a cell with islands; `query_collection`
   Omni and Battleship rows keep z at least 10 and equal per controller.
5. The `/agent-harness` replay determinism check (`replay_record`, then
   `replay_play`) over fleet and random-wave combat reports no CRC mismatch.
6. With `pin_graphics`, a client `screenshot` shows ships' drop shadows on
   water and terrain, and a ModelViewer flight-mode screenshot shows the body
   and its shadow under the Design's ModelViewer height-range and footprint
   change, with the world textures reaching every screen edge.
