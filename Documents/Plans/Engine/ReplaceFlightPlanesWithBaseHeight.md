<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-25T23:52:00.910Z","dependsOn":["Documents/Plans/Engine/CollapsePusherZonesAndDecoupleAudioNav.md"]} -->
# Replace FlightPlanes and FlightPlaneTable with Base Height in Frame.h

## Context

`engine::FlightPlanes` (`Engine/Source/Frame/FlightPlanes.h:6-43`,
`FlightPlanes.cpp:8-72`) borrows each project's `game::kFlightPlanes` table
(`Projects/BrokenEngineSandbox/Source/Frame/FlightPlaneTable.h`,
`Projects/ModelViewer/Source/Frame/FlightPlaneTable.h`). Earlier stages already
put every ship on one flight height, world lighting on one layer, smoke on one
field, object shadows on a single-channel uniform blur, pushers on a single XY
zone grid, and audio listener height on a fixed constant. Only the table itself,
its `FlightPlanes` wrapper and startup constructions, its remaining base-height
readers across Engine, BrokenEngineSandbox, and ModelViewer, and the
`FlightPlaneProjection.cpp` filename remain.

The user's height decision (binding): BrokenEngineSandbox uses `10.0f` m for
gameplay, navigation contours, and graphics ("Do 10m to start, I will tweak
later"). The user did not specify ModelViewer's value; each project supplies its
own constant, and the author's recommendation is to keep ModelViewer at `2.0f`
m. The user also selected defining `game::kfBaseHeightMeters` directly in each
project's PCH-included `Frame/Frame.h` with a positive-height `static_assert` in
`Engine/Source/Engine.h` rather than introducing new `BaseHeight.h` headers.

Originating gap: final stage of the user-approved flight-plane removal split,
removing the remaining `FlightPlanes` and `FlightPlaneTable.h` plumbing once
pushers, audio, and navigation naming no longer depend on the table.

## Design

- **One game-supplied base height in PCH-included `Frame/Frame.h`**: define
  `inline constexpr float kfBaseHeightMeters` in `namespace game` directly in
  `Projects/BrokenEngineSandbox/Source/Frame/Frame.h` (`10.0f`, `:12-17`) and
  `Projects/ModelViewer/Source/Frame/Frame.h` (`2.0f`, `:5-18`), without adding
  `BaseHeight.h` headers. `Engine/Platforms/VisualStudio2026/Engine.props:56-58`
  force-includes `Pch.h`, and both
  `Projects/BrokenEngineSandbox/Source/Pch.h:24-25` and
  `Projects/ModelViewer/Source/Pch.h:14-15` include `Frame/Frame.h` ahead of
  `Engine.h` (no other project includes `Engine.h`). Replace
  `#include "Frame/FlightPlanes.h"` in `Engine/Source/Engine.h:76` with
  `static_assert(game::kfBaseHeightMeters > 0.0f);`.
- **Delete table and runtime wrapper**: delete
  `Engine/Source/Frame/FlightPlanes.h`, `Engine/Source/Frame/FlightPlanes.cpp`,
  `Projects/BrokenEngineSandbox/Source/Frame/FlightPlaneTable.h`, and
  `Projects/ModelViewer/Source/Frame/FlightPlaneTable.h`. Remove `FlightPlanes`
  startup constructions in `Engine/Source/Main.cpp:229,307` and
  `Projects/ModelViewer/Source/Main.cpp:179`.
- **Projection file rename**: rename
  `Engine/Source/Graphics/FlightPlaneProjection.cpp` to
  `Engine/Source/Graphics/BaseHeightProjection.cpp` (author's recommendation:
  the file holds only base-height projection), reading `game::kfBaseHeightMeters`
  while keeping `ProjectToBaseHeight`'s terrain maximum and updating its
  `GlobalElevation` comment.
- **Camera and renderers**: update `Engine/Source/Graphics/CameraBase.cpp`
  (`:57-66` `TiltedFootprint` spans `{0.0f, game::kfBaseHeightMeters}` instead
  of 0 and the highest plane; `:117-121` constructor and `:259` `ResolveTarget`
  Direct targets sit at `game::kfBaseHeightMeters`),
  `Engine/Source/Graphics/Render/MainUniforms.cpp` (`:21`, `:59`, `:107`),
  `Engine/Source/Graphics/Render/GlobalUniforms.cpp` (`:624`),
  `Projects/ModelViewer/Source/ModelViewerApplication.cpp` (`:686`), and
  `Projects/ModelViewer/Source/Graphics/Camera.cpp` (`:26`). ModelViewer shares
  `CameraBase.cpp`, so its `TiltedFootprint` ceiling moves from `100.0f` to its
  `2.0f` base height (author's recommendation: accept for the developer tool).
- **Remaining BES readers and navigation bake**: update every remaining
  `gpFlightPlanes`, `kFlightPlanes`, and `FlightPlanes.h` reader under
  `Projects/BrokenEngineSandbox/Source/` (`ShipBehaviours.cpp`,
  `ShipNavigationUtils.cpp` including `:56` `OmniPatrolDiscPoint` comment,
  `Fighters.h`, `Omnis.h`, `Battleships.h`, `MissilesUpdate.cpp`,
  `TerrainUtils.h:80` `kNavigationGraphSpecs` threshold height and debug draw Z,
  `ServerClientManager.cpp`, `ServerSimulationHarnessRigs.cpp` including `:110`,
  `Graphics/Camera.cpp`, `Frame.cpp:302`, plus any other reader the build
  reports) to read `game::kfBaseHeightMeters`.
- **Project membership**: reconcile the BES client and server
  `.vcxproj`/`.vcxproj.filters` and ModelViewer `.vcxproj`/`.vcxproj.filters`
  via `/update-vcxproj` (removing `FlightPlanes.{h,cpp}` and
  `FlightPlaneTable.h`, renaming `FlightPlaneProjection.cpp` to
  `BaseHeightProjection.cpp` in the client).
- **Documentation and comments**: update remaining `AGENTS.md`, shader comment,
  harness command reference, and feature docs naming flight planes or the old
  table/projection file (listed in `## In scope`).

## Critical files

- `Engine/Source/Engine.h`
- `Engine/Source/Frame/FlightPlanes.{h,cpp}` (deleted)
- `Projects/{BrokenEngineSandbox,ModelViewer}/Source/Frame/FlightPlaneTable.h`
  (deleted)
- `Projects/{BrokenEngineSandbox,ModelViewer}/Source/Frame/Frame.h`
- `Engine/Source/Main.cpp`, `Projects/ModelViewer/Source/Main.cpp`
- `Engine/Source/Graphics/CameraBase.cpp`,
  `Engine/Source/Graphics/BaseHeightProjection.cpp` (renamed from
  `FlightPlaneProjection.cpp`)
- `Engine/Source/Graphics/Render/{MainUniforms.cpp,GlobalUniforms.cpp}`
- `Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.h`
- `.vcxproj` and `.vcxproj.filters` of BES client, BES server, and ModelViewer

## In scope

- Define `inline constexpr float kfBaseHeightMeters` in
  `Projects/BrokenEngineSandbox/Source/Frame/Frame.h` (`10.0f`, `:12-17`) and
  `Projects/ModelViewer/Source/Frame/Frame.h` (`2.0f`, `:5-18`).
- Delete `Engine/Source/Frame/FlightPlanes.h`,
  `Engine/Source/Frame/FlightPlanes.cpp`,
  `Projects/BrokenEngineSandbox/Source/Frame/FlightPlaneTable.h`, and
  `Projects/ModelViewer/Source/Frame/FlightPlaneTable.h`.
- `Engine/Source/Engine.h` (`:76`): replace `#include "Frame/FlightPlanes.h"`
  with `static_assert(game::kfBaseHeightMeters > 0.0f);`.
- `Engine/Source/Main.cpp` (`:229`, `:307`) and
  `Projects/ModelViewer/Source/Main.cpp` (`:179`): remove `FlightPlanes`
  startup constructions.
- Rename `Engine/Source/Graphics/FlightPlaneProjection.cpp` to
  `Engine/Source/Graphics/BaseHeightProjection.cpp`: read
  `game::kfBaseHeightMeters` in `ProjectToBaseHeight` (keeping its terrain
  maximum) and update its `GlobalElevation` comment.
- `Engine/Source/Graphics/CameraBase.cpp` (`:57-66`, `:117-121`, `:259`),
  `Engine/Source/Graphics/Render/MainUniforms.cpp` (`:21`, `:59`, `:107`),
  `Engine/Source/Graphics/Render/GlobalUniforms.cpp` (`:624`),
  `Projects/ModelViewer/Source/ModelViewerApplication.cpp` (`:686`), and
  `Projects/ModelViewer/Source/Graphics/Camera.cpp` (`:26`).
- Remaining BES readers under `Projects/BrokenEngineSandbox/Source/`:
  `ShipBehaviours.cpp`, `ShipNavigationUtils.cpp` (including `:56`),
  `Fighters.h`, `Omnis.h`, `Battleships.h`, `MissilesUpdate.cpp`,
  `TerrainUtils.h:80` (`kNavigationGraphSpecs`), `ServerClientManager.cpp`,
  `ServerSimulationHarnessRigs.cpp` (including `:110`), `Graphics/Camera.cpp`,
  `Frame.cpp:302`, plus any other `gpFlightPlanes`/`kFlightPlanes`/
  `FlightPlanes.h` reader the build reports.
- Reconcile BES client and server `.vcxproj`/`.vcxproj.filters` and ModelViewer
  `.vcxproj`/`.vcxproj.filters` through `/update-vcxproj`.
- Docs and comments:
  - `Engine/Source/Frame/AGENTS.md` (`:115-123` `FlightPlanes` bullet becomes
    the base-height contract: `game::kfBaseHeightMeters` in PCH-included
    `Frame/Frame.h` validated by `Engine.h`)
  - `Engine/Source/Graphics/AGENTS.md` (`:131`)
  - `Engine/Source/Graphics/Render/AGENTS.md` (`:36`)
  - `Engine/Data/Shaders/Smoke/AGENTS.md` (`:3`)
  - `Engine/Data/Shaders/Objects/AGENTS.md` (`:19`)
  - `Engine/Data/Shaders/Objects/HexShield.vert` (`:91`)
  - `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md` (`:69`, `:75-77`)
  - `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/AGENTS.md`
    (remaining "lowest plane")
  - `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/AGENTS.md`
    (`:15`, `:19`)
  - `Projects/ModelViewer/AGENTS.md` (`:43`)
  - `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`
    (`:230`)
  - `.agents/skills/agent-harness/references/command-reference.md` (`:86-87`)
  - `Documents/Features/Graphics/OceanImpactSplashes.md` (`:21`, `:56`)
  - `Documents/Features/Graphics/PlanarHolographicRadianceCascades.md`
    (`:36`, `:46`, `:226`, `:229`)

## Out of scope

- Ship behaviour, targeting, navigation graph contents, lighting, smoke, object
  shadows, pushers, and audio listener height (handled by earlier stages).
- Changing the `10.0f` m or `2.0f` m values, or terrain avoidance and steering
  constants.
- `CollisionLayer`, frame-registry ownership layers, Vulkan layers, image array
  layers, and other uses of "layer" unrelated to flight planes.
- Historical version sentences in `Documents/Architecture/Network.md`.
- Unit tests.

## Risk triggers and invariants

- Change Workflow Tier 3: changes the server navigation bake input
  (`TerrainUtils.h:80`, `NavCellData.cpp:281-358`), spans independently owned
  subsystems (Frame, Graphics, BrokenEngineSandbox, ModelViewer, Shaders, and
  `.agents/skills/`), and updates project membership across three projects.
- The server still builds navigation before any tick reads it, and the client
  still trusts server navigation data.
- Game base-height reads keep identical values (`10.0f` BES, `2.0f` ModelViewer)
  from PCH-included `Frame/Frame.h`, validated by
  `static_assert(game::kfBaseHeightMeters > 0.0f)` in `Engine/Source/Engine.h`;
  simulation code never reads render state.

## Acceptance criteria

1. `builder /compile` builds `BrokenEngineSandbox` (client),
   `BrokenEngineSandboxServer`, and `ModelViewer` (Local data mode triggered by
   editing `Engine/Data/Shaders/Objects/HexShield.vert`);
   `mechanic /update-vcxproj` reports all three projects' membership consistent;
   `/external-skill-creator` validates the
   `.agents/skills/agent-harness/references/command-reference.md` edit.
2. Static grep across `Engine/`, `Projects/BrokenEngineSandbox/`, and
   `Projects/ModelViewer/` finds zero matches for `FlightPlane`,
   `gpFlightPlanes`, `NearestPlaneIndex`, `OccupiedPlanes`,
   `kiMaxFlightPlanes`, and `kTerrainCollision`, and zero matches for
   `flight plane` in any `AGENTS.md` under those trees; code read confirms
   `Frame/Frame.h` defines `game::kfBaseHeightMeters` as `10.0f` in BES and
   `2.0f` in ModelViewer, `Engine/Source/Engine.h` carries the positive-height
   `static_assert`, `CameraBase.cpp` (`TiltedFootprint`
   `{0.0f, game::kfBaseHeightMeters}`, constructor/Direct targets) and
   `TerrainUtils.h` (`kNavigationGraphSpecs`) read `game::kfBaseHeightMeters`,
   and `BaseHeightProjection.cpp` keeps `ProjectToBaseHeight`'s terrain maximum.
3. Pre-edit baseline check (`before the first edit`, Local `no-effect`;
   `builder /compile` BES client and server, then `runner /agent-harness`):
   record baseline `cell_coordinate_probe` (`navigation`) vertices, offsets, and
   reachable edges at a fixed island cell coordinate, capture a `save` of that
   state, `load` that same save file with `pauseAfterLoad=true`, apply any fixed
   inputs on matching ticks, pause at a chosen completed tick with
   `pause_at_tick`, and record baseline nonempty `query_collection` `Omnis` and
   `Battleships` row counts and per-row `z` values grouped by collection and
   controller; after edits, rerun `cell_coordinate_probe` at the same
   coordinate and verify identical vertices, offsets, and reachable edges for
   every graph index (`0, 1, 2`), and `load` the same save file with
   `pauseAfterLoad=true`, apply the identical inputs on matching ticks, pause at
   the same completed tick with `pause_at_tick`, and verify matching nonempty
   row counts and per-row `z` values (each `z >= 10`) per collection and
   controller, plus code-read trace of server navigation bake/send through
   client adoption.
4. `runner /agent-harness`: replay determinism check (`replay_record`, then
   `replay_play`) over fleet and random-wave combat reports no CRC mismatch.
5. `runner /agent-harness`: a ModelViewer flight-mode screenshot via
   `Invoke-ModelViewerCapture.ps1 -Flight` shows the body and its shadow with
   world textures reaching every screen edge; code read confirms `2.0f`
   `game::kfBaseHeightMeters` feeds `CameraBase.cpp` `TiltedFootprint`.

## Notes

- Required roles include `mechanic /update-vcxproj` for file additions,
  removals, and renames, and `/external-skill-creator` for
  `.agents/skills/agent-harness/references/command-reference.md`.
