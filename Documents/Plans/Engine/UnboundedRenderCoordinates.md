<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-30T16:41:17.810Z","dependsOn":["Documents/Plans/Engine/FrameAreaValidation.md"]} -->
# Make simulation and presentation geometry cell-local on the unbounded grid

## Context

The resolved finding CAI/shard-0014/005 identified a representation failure at
the boundary between the unbounded sparse-grid contract and
Engine/Source/Frame/FrameUtils.h. GridCoord is a signed int32 identity, but
ComputeFrameArea converts that identity to a large float offset and stores
the resulting absolute edges in FrameStaticData::vecArea. At
x = 16,777,216, the float edges for a 900 m cell collapse; at
x = 1,000,000, the rounded width is 896 m. The active-frame path accepts
these coordinates, so collision zones, strict frame-boundary transfer
decisions, terrain origins, and render/world feeds can all consume a zero or
distorted cell.

The engine contract is still an unbounded sparse grid of independently
simulated cells. The integer cell identity must therefore remain intact while
all geometry that is used by a cell stays in a centered local representation.
The same representation must be used by deterministic server and client
simulation, by static cell data, and by the client presentation boundary.
The current client agent command also imposes an arbitrary absolute-coordinate
limit, which prevents probing the legal int32 domain.

This Plan is the resolved implementation decision. It replaces the historical
investigation and the superseded Frame-relative positions note; those documents
are retired by the preparation stage that authors this Plan. The future source
change is intentionally an integration change rather than a render-only
optimization.

## Design

### Coordinate and geometry contract

- Keep signed-int32 GridCoord as the only cell identity and keep the
  unbounded sparse-grid model. Coordinates are used for identity, lookup,
  deterministic placement seeding, and checked integer neighbor arithmetic.
- Make deterministic simulation geometry and every per-cell static geometry
  centered cell-local. A cell is exactly 900 m wide and high, and local X/Y
  values use the existing centered convention [-450,+450]; the canonical
  local area is therefore exactly 900 m by 900 m at every coordinate.
- Introduce only small shared typed coordinate/local/basis values at
  boundaries that need both pieces. A position crossing a boundary is
  represented as its GridCoord plus its local position; a presentation basis
  identifies the selected camera cell and the local translation needed for
  nearby copies. These values are passed in narrow batches, not through a
  god module or a broad FrameCellContext/RenderFrameSet owner.
- Every interpolate collection exposes an auditable, fixed range of its
  position arrays. One linear helper walks those ranges when making a
  presentation copy. Do not add a new Collection and do not add a
  per-entity coordinate SOA member; collection ownership and persistence stay
  unchanged.
- Retain the raw float/vector widths and member order of retained per-entity
  and static position fields. Their meaning becomes cell-local, including the
  existing island-placement position field, without adding compatibility
  aliases or a second representation.

### Frame, static data, terrain, and navigation

- Remove FrameStaticData::vecArea and ComputeFrameArea. Replace all
  simulation bounds, collision-area, frame-center, and transfer-area reads
  with canonical local bounds/area derived from the 900 m constants.
  FrameStaticData::coord remains the cell identity.
- Generate island placement positions in centered local meters. The
  coordinate still seeds the independent deterministic placement streams and
  owns identity; it is never converted to a large float to form geometry.
- Build and sample the elevation grid in local cell coordinates. The sampler,
  FrameElevation, and FrameNormal consume local positions and local
  placement data. BuildElevationGrid, BlendPlacementIntoGrid, and
  MakeFrameElevationSampler must not form a large coordinate-derived origin.
  IslandRenderQuery values are local too and carry the cell identity
  separately rather than storing a large absolute float.
- Store navigation vertices, contours, and acceleration data in the same
  cell-local meters. Keep the shared build/query predicates and existing
  topology/order rules. Increment kiNavDataVersion because the derived data
  representation changes.
- Keep same-cell movement, targeting, collision, and integration local. At
  the existing transfer preparation helper, calculate destination-local
  position as sourceLocal - step * 900 for each transferred cell axis.
  Forward that value unchanged through TransferRequest, network transfer,
  spawn, and replay consumption; do not repeat the conversion downstream.
- Add checked GridCoord addition for every neighbor/transfer destination.
  Numeric-edge visible neighbors are skipped. An outward transfer whose
  destination cannot be represented fails before any partial transfer state,
  publication, or queue commit is visible.

### Render, camera, and static presentation

- Build the selected camera-source render interpolate in local coordinates,
  update the split-position camera from that coordinate-tagged snapshot,
  snapshot the camera's current cell as the presentation basis, and then translate
  only nearby presentation copies and static uploads. The render-copy rebase
  is one linear pass over the fixed position ranges, after interpolation and
  before collection render callbacks; renderers do not each invent a rebase.
- Use fixed stack coordinate storage and std::span<const GridCoord> through
  render callbacks. The selected render snapshot supplies the camera target's
  coordinate and local position, while the basis follows the camera's current
  split position after interpolation; it never comes from a live
  mClientGridCoord read that can disagree with a delayed snapshot. Cells too
  distant to rebase into the presentation window show ocean until they become
  near.
- Represent camera points as GridCoord plus local position. Preserve the
  existing two-second far-focus smooth flight with client-only double
  relative-delta math. On an adjacent camera-cell change, rebase wind-trail
  position caches and smoke/lighting/shadow current/previous area descriptors
  while retaining GPU history. A jump of more than one cell in a render
  resets history.
- Preserve water phase continuity by reconstructing a client-only double
  world camera position before CPU phase reduction. No GLSL change is planned;
  no double or fixed-point value enters deterministic simulation or the CRC.
- Convert every remaining client world-space feed at the presentation
  boundary, including island/static uploads, camera matrices, visibility
  rectangles, elevation projection, and water snapping. No feed may create a
  large-magnitude float from GridCoord merely
  to draw geometry.

### Audio and agent observability

- One-shot audio events and persistent voices carry emitting/listener
  GridCoord plus local position. Convert one-shots against the already
  published listener basis at their existing immediate cull/mix point, and
  convert persistent voices while mixing; do not add an audio queue or member
  SOA column.
- Make all agent position schemas explicit {coord,local,worldMeters} and
  remove legacy position aliases. Add the camera basis to the camera schema.
  Add a read-only, both-endpoint cell_coordinate_probe that exercises the
  same local bounds, terrain, placement, and CRC evidence on server and
  client. Remove the arbitrary client agent grid limit so legal full-int32
  probes can run after checked neighbor arithmetic is in place.

### Versions and compatibility

This is an intentional one-generation compatibility break with no shims.
Bump kiNavDataVersion, the base term of Frame::kiVersion,
FrameInput::kiVersion, and kuiProtocolVersion. Keep the existing raw
per-entity/static position widths and member order for fields that remain;
the removed derived area and the changed local meaning are rejected by the
version gates rather than decoded through a compatibility path. Preserve
CRC composition/order and the existing client/server fixed-tick phase order.
All deterministic local conversions run under the existing strict floating
point environment and must produce identical placement, elevation, and frame
CRCs on both endpoints. Presentation-only rebases remain outside the CRC.

## Critical files

- Engine/Source/Frame/GridCoord.h,
  Engine/Source/Frame/FrameUtils.h, Engine/Source/Frame/FrameStaticData.h,
  and Engine/Source/Frame/FrameStaticData.cpp — cell identity, canonical
  local bounds, removal of vecArea, and static serialization, including
  ComputeFrameBounds, TracePointToFrameExit, IsOutOfBounds, and
  ComputeTransferDelta.
- Projects/BrokenEngineSandbox/Source/Frame/Frame.h and Frame.cpp —
  Frame::kiVersion, FramePostRender::Transfer, PrepareTransferRequest,
  frame-center/spawn rasterization, local bounds, and transfer preparation.
- Engine/Source/Frame/IslandChainPlacement.cpp,
  Engine/Source/Frame/IslandTerrain.cpp, NavBuild.cpp,
  NavCellData.cpp, NavQuery.cpp, and their headers — local island
  placement, BuildElevationGrid, BlendPlacementIntoGrid,
  MakeFrameElevationSampler, FrameElevation, FrameNormal, BuildCellNavData,
  NavQueryDirection, navigation data, and versioning.
- Engine/Source/Frame/Collision.cpp and the frame/collection update paths —
  Collision::SetupZones, collision zones, frame exit, ownership checks, and
  local positions.
- Projects/BrokenEngineSandbox/Source/Frame/Collections/Players,
  .../Blasters, .../Missiles, and .../Spaceships — every deterministic
  position field, bounds/center calculation, island destination, and the four
  transfer builders.
- Engine/Source/Frame/Collections/Explosions,
  Pushers, and the client collections AreaLights, Billboards, HexShields,
  PointLights, Puffs, Sounds, SmokeTrails, WindRadials, and WindTrails —
  fixed interpolate position ranges and current/previous presentation caches.
- Engine/Source/Network/Server/ServerTransferManager.cpp,
  Projects/BrokenEngineSandbox/Source/SpawnTransfer.cpp, and
  Engine/Source/Network/Client/ReconcileReplayTick.cpp — checked
  destination coordinates and unchanged destination-local transfer payloads.
- Engine/Source/GameBase.h and GameBase.cpp, plus
  Projects/BrokenEngineSandbox/Source/Game.cpp — GameBase::CreateFrameAtCoord,
  GameBase::UpdateRenderInterpolation, GameBase::SelectRenderCamera,
  ServerSessionRuntime::SyncActiveFrames, Game::SetClientGridCoord,
  active-cell neighbor arithmetic, render interpolation/rebase ordering,
  camera-cell selection, and subscription conversion.
- Engine/Source/Graphics/Render/MainUniforms.cpp,
  Engine/Source/Graphics/GraphicsUtils.cpp,
  Engine/Source/Graphics/EngineCamera.cpp, Engine/Source/Graphics/Islands.cpp,
  and Projects/BrokenEngineSandbox/Source/Graphics/Camera.cpp — local
  static uploads, FrameInterpolate::Update, FrameInterpolate::BeginRender,
  RenderFrameMain, ProjectToBaseHeight,
  Camera::CalculateMatricesAndVisibleArea, camera basis/matrices, visibility,
  elevation projection, water phase/snap, and far-focus continuity.
- Engine/Source/Audio/AudioManager.cpp, AudioManager.h,
  StaticVoices.cpp, StaticVoices.h, and the game one-shot call sites —
  PlayOneShot3d, AudioManager::Update, StaticVoices, and coordinate-aware
  event/voice conversion against the listener basis.
- Engine/Source/Network/NetworkProtocol.h,
  Projects/BrokenEngineSandbox/Source/Frame/FrameInput.*,
  and Documents/Architecture/Network.md — generation gates and documented
  compatibility break.
- Engine/Source/Agent/AgentCommandsShared.*,
  Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsClient.cpp,
  AgentCommandsServer.cpp, AgentCommandsServerQueries.cpp, and the
  client/server AgentHarness command and cross-cell documents — explicit
  position schemas, camera basis, CommandSetClientGridCoord,
  ExecuteAgentCommandClient/Server, full-domain coordinate parsing, and the
  read-only cell_coordinate_probe.

## In scope

- Replace the world-area float construction and every simulation consumer of
  FrameStaticData::vecArea with canonical centered local bounds/area, then
  remove the member and ComputeFrameArea while preserving retained raw field
  widths/order and version-gated serialization.
- Convert deterministic entity positions, island placement positions, nav
  vertices/acceleration, elevation-grid construction/sampling, island
  destinations, frame centers, collision zones, and strict frame-boundary
  checks to the local [-450,+450] convention.
- Add the typed coord/local/basis values, fixed per-collection interpolate
  position ranges, one linear render-copy rebase, stack coordinate storage,
  and span-based render callbacks. Update every affected engine/game
  collection without introducing a collection or per-entity coord column.
- Implement destination-local transfer preparation in the existing helper,
  forward it unchanged through all four entity types and replay/network
  paths, and add checked coordinate addition to active-neighbor and transfer
  publication paths.
- Rebase client camera/render/static/audio presentation data against the
  snapshot camera basis, including adjacent wind/smoke/lighting/shadow cache
  handling, multi-cell history reset, double relative camera flight math, and
  continuous CPU water phase reduction.
- Update every remaining non-CRC world-space consumer proven by the
  presentation-boundary audit.
- Update agent position/camera schemas, add the read-only both-endpoint
  cell_coordinate_probe, remove only the arbitrary client coordinate limit,
  and document the new probe/output contract without legacy aliases.
- Bump all four specified generation/version gates, retain the existing CRC
  and phase ordering, and update the architecture and AgentHarness documents
  needed to describe the current contract.

## Out of scope

- Any finite supported coordinate range, weakening of signed-int32 identity,
  fixed-point or double-precision simulation, or a finite-world workaround.
- Any per-entity coordinate SOA member, new Collection, broad
  FrameCellContext/RenderFrameSet ownership, speculative compatibility
  shim, legacy schema alias, or second absolute simulation representation.
- Pack formats, packed assets, shader source, GLSL bindings, or a shader-side
  water-phase change. The client-only CPU phase reconstruction is the complete
  water fix.
- Changes to RNG stream identities/draw counts, fixed-tick phase order, CRC
  ordering, placement count limits, or the shared navigation
  segment/point predicates.
- Unrelated frame-area work owned by
  Documents/Plans/Engine/FrameAreaValidation.md; that Plan is not edited,
  rejected, or deleted by this work.
- New unit tests. Acceptance uses the existing Debug builds, AgentHarness
  commands/probes, transfer fixtures, replay checks, and live render scenarios.
- Cosmetic conversion of unrelated debug text that is not a position schema or
  an observable contract.

## Risk tier and invariants

Expected Change Workflow Tier 3. The change alters deterministic geometry and
shared CRC inputs, static and navigation serialization, transfer semantics,
client/server wire/version gates, render interpolation and camera continuity,
audio boundaries, and agent observability across independently owned
subsystems.

Preserve these invariants:

- Every legal signed-int32 coordinate identifies a usable 900 m cell. Local
  simulation and static geometry remain centered in [-450,+450]; no
  coordinate-to-large-float construction can collapse or distort the area.
- An entity is simulated only in its owning cell. Strict local bounds identify
  the neighboring transfer, and a transfer payload is already destination
  local from the preparation helper.
- Checked coordinate addition never wraps. Numeric-edge neighbors are omitted,
  and an unrepresentable transfer fails before queue/publication mutation or
  partial state publication.
- Placement, elevation, navigation, frame, replay, and shared CRC results are
  bit-identical between Debug client and server for the same cell and tick.
  Presentation rebases, camera flight, and audio conversion do not enter
  deterministic state or CRC.
- Retained raw per-entity/static widths and member order remain stable within
  the new generation; old-generation data is rejected by the bumped gates
  without compatibility shims.
- The rendered snapshot supplies an explicit coord+local camera target; the
  render basis follows the camera's interpolated split-position cell rather
  than live client selection state. Adjacent basis changes preserve visual
  continuity and GPU history; a multi-cell render jump resets history, and no
  900 m one-frame pop occurs.
- Camera far-focus flight remains monotonic over its two-second duration, and
  CPU water phase remains continuous after reconstructing the client-only
  double world camera position.
- Agent reports expose only the explicit coord/local/worldMeters position
  shape and camera basis. cell_coordinate_probe is read-only and exercises
  both endpoints over the full requested coordinate cases.

## Acceptance criteria

- A source/contract audit finds no deterministic or static geometry path that
  forms a large float from a cell coordinate, no FrameStaticData::vecArea,
  and no ComputeFrameArea; every remaining coordinate-to-float use is
  identity, RNG, checked integer-neighbor, or nearby presentation-basis
  work.
- Debug client and Debug server both compile successfully through the
  repository build driver, including the versioned static/nav/network and
  agent changes.
- Run the read-only cell_coordinate_probe at both endpoints for
  0, ±9321, ±37283, ±9544372, ±9544373, and 16777216. Each result has exact
  900 m area width/height, 1024 distinct local terrain-axis positions,
  finite terrain samples, and identical placement and elevation CRCs between
  endpoints. The probe accepts full signed-int32 coordinates and reports no
  legacy position aliases.
- Exercise local strict-boundary and numeric-edge transfer probes. Confirm
  that checked outward neighbors are skipped, unrepresentable outward
  transfers fail before partial publication, and representable transfers
  preserve destination-local positions.
- Exercise natural and fixture transfers for players, spaceships, blasters,
  and missiles in both directions where supported. Confirm destination
  ownership, local positions, and no partial state publication.
- Record and replay a deterministic scenario through the existing replay
  checks. Confirm CRC equality and no client/server desync across local
  movement, terrain/nav use, and transfers; confirm an old-generation stream
  is rejected by the intentional version break.
- Render a high-coordinate client terrain cell and confirm finite, stable
  terrain/static presentation with distant target cells showing ocean until
  near; no large-coordinate float collapse is visible.
- Run a two-fleet far-camera scenario that crosses an adjacent cell boundary.
  Sample the focus over the transition and confirm a monotonic two-second
  flight, continuous water phase, preserved adjacent wind/smoke/lighting/
  shadow history, and no 900 m presentation pop. Confirm a multi-cell
  render jump resets history as specified.
- Verify coordinate-aware one-shot and persistent audio positioning against
  the listener basis.

## Coordination

Documents/Plans/Engine/FrameAreaValidation.md is the sole prerequisite for this
Plan. It may land first against the current vecArea. WorktreeCli prerequisite
completion deletes that Plan and removes this child edge; scheduler completion
and edge removal are the evidence that it ran first. This Plan never edits,
rejects, or deletes the prerequisite itself. The exact dependency and
prerequisite hash checks from this authoring stage remain landing criteria
outside the future Plan's acceptance checks. The later cell-local Plan may then
remove vecArea and its validation as part of its approved source scope.

Implementation should keep the shared coord/local/basis contract and version
decisions together, then make disjoint simulation/static and client
render/camera/audio slices before the agent/probe and documentation slice.
The four transfer builders must share the existing preparation helper, and
the render callbacks must share the one fixed-range rebase; parallel work must
not create duplicate conversions. Run the required propagation and
cross-subsystem reviews after all slices are present because the changed
representation reaches CRC, serialization, wire, render, audio, and agent
boundaries.

The current preparation stage retires
Documents/Investigations/Engine/UnboundedRenderCoordinates.md and
Documents/Features/Frame/FrameRelativePositions.md. No C++, GLSL, script,
pack, claim, build, or primary-branch change is part of that retirement.
