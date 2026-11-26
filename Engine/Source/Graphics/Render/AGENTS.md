# Render - Per-Frame GPU Data

Client-only population of mapped Global and Main layouts before their
record-once command buffers submit. Each render subsystem owns its layout
region; tunable-dependent staging remains separate from per-frame phase state.

Render free-runs between simulation ticks and sits outside the CRC (the per-tick
checksum that keeps client and server bit-identical), so nothing computed here
may feed simulation state.

## Ordering and Publication

- Main publishes water source dimensions, wave parameters, and displacement work
  and all Main lighting data even when the camera interpolate is absent; water
  drawing still requires it. Main lighting publication runs every rendered
  frame independently of lighting and smoke refresh cadence.
- Global population precedes Main. Global is submitted, possibly on a worker
  thread, before `RenderFrameMain` runs, so nothing in Main's call tree writes
  GlobalLayout: a value Main produces for a shader, such as the water band
  counts and active quad dimensions, is published in MainLayout. Within Global,
  Smoke precedes Wind because wind consumes smoke's current and previous world
  areas.
- Gameplay main rendering runs collection `BeginRender`, per-coord `Render`,
  collection `EndRender`, lighting-spread gate publication, then debug
  publication, with the camera coord first, on one path every frame. With no
  renderable coord, including an empty active set, no per-coord `Render` runs,
  but begin/end publication still zeroes every indirect count and per-render
  profile counter instead of ghost-drawing or reporting prior-frame instances,
  and the `MainLayout` camera block and hex-shield fields are written from the
  camera's held state.
- `CommandBufferRecordMain::Record` includes the lighting deposit and spread
  passes only when `gLightingEnabled` is on; those recorded passes clear their
  respective attachments by load op on each world frame with `gLightingEnabled`
  on (including frames that skip a lighting refresh), so no clear pass is
  recorded.
- World lighting has one layer at the base flight plane height, keeping its
  combine outputs, history, area latch, and reset flag; one
  deposit-to-history-copy chain refreshes it on scheduled or forced frames
  (`gbLightingRefreshFrame`, `Render.h`). A refresh advances its areas and
  publishes full-texture work; a skipped enabled frame keeps its held areas and
  publishes zero work (load clears above still run). Published UVs stay finite
  across reset-pending and disabled frames (`PopulateLightingParameters` in
  `LightingUniforms.cpp` owns snapped-area fallback and pending-reset handling).
  Lighting (`../../../Data/Shaders/Lighting/AGENTS.md` `## Pipeline Contracts`)
  owns the reset and history-copy sequence.
- The terrain shadow elevation, source, blur, temporal, and history-copy chain
  shares one update interval, using the same current/previous area latch as
  lighting (`TemporalAreaLatch`, `Render.h`). A refresh runs on the cadence, on
  reset or first use, or on a teleport; it advances both areas and publishes
  full-texture indirect work from `RenderFrameGlobal`. A skipped frame
  republishes the held area pair and blend and writes zero indirect work; the
  elevation render-pass clear and every layout transition still run.
- The object shadow raster and its two blur passes share one update interval,
  `gObjectShadowCadence`, and one held area, with no temporal pass. A
  refresh runs on the cadence, on first use, object-shadow texture recreation,
  or the world's return from hiding (`gbObjectShadowsReset`, `Render.h`), or on
  a teleport; it sets the held area to the live visible area scaled about its
  center by `gExtraArea`. Global publishes the held area's mapping as
  `objectShadowsAreaToUv` and the decision as `gbObjectShadowsRefreshFrame`
  (`Render.h`), which Main's model shadow draw counts read. A skipped frame
  writes zero shadow draw and blur dispatch work; the raster pass still clears
  the unblurred texture, and the blurred texture consumers read keeps its last
  refresh.
- The lighting, terrain shadow, object shadow, and smoke schedules read
  `Graphics::miFrameCounter`, which the residency step
  (`Graphics::UpdateFrameResidency`) advances once per frame, before
  `RenderFrameGlobal` on a world frame. `PopulateLightingParameters`
  (`LightingUniforms.cpp`) computes the scheduled lighting refresh from
  `miFrameCounter` and `gLightingUpdateCadence`, and `RenderSmokeGlobal`
  (`SmokeUniforms.cpp`) computes the scheduled smoke refresh from
  `miFrameCounter` and `gSmokeUpdateCadence`. Terrain shadows refresh when
  `(miFrameCounter + 1) % shadowCadence == 0` and object shadows when
  `miFrameCounter % objectShadowCadence` equals `1 % objectShadowCadence`.
  Forced refreshes — resets, first use, and a teleport — stay outside the
  schedule. A teleport is the live visible area's center lying outside the area
  a chain's retained texture was last written for, tested after the
  `RetainedAreaBasis` shift; a view edge that only pans past that area samples
  outside it until the next refresh.
- `Graphics::Refresh` is the sole caller of `gLightingEnabled.Changed<bool>()`.
  With Lighting off, `CommandBufferRecordMain::Record` omits lighting passes
  (visible sprites and shields still render) and frames clear
  `gbLightingRefreshFrame`. `RenderTargetTextures::ClearLightingTextures()`
  (`../Managers/RenderTargetTextures.h`), called from `CreateLightingTextures()`
  and `Graphics::Create()` on a surviving-texture disable, owns target clear
  coverage, values, and `gbLightingTemporalReset` rearming.
- Downstream thresholds that the amplitude must cross before the effect applies
  may clear an upstream count only when they also skip the corresponding array
  write. Keep count and data publication paired.
- Write-combined mapped layouts are write-only. Compute dependent values in CPU
  staging state and copy each populated region once.
- State that crosses files, or that something outside this directory resets,
  lives as `inline` globals in `Render.h`. Single-file edge detectors and
  latches (the statics that remember the previous frame's value) stay
  function-local or file-static — do not promote them, because a promoted latch
  looks externally resettable and invites a second writer.
- Every latch assumes its entry point runs exactly once per frame. Hosts invoke
  `RenderWaterMain` in `WaterUniforms.cpp` once per frame from their
  water-rendering path to publish Main water data. Main wave phases use the
  supplied elapsed time; water's reduced-time accumulation remains on the Global
  publication path.
- The whole day cycle resolves on the CPU in `GlobalUniforms.cpp`: sun and moon
  angle and direction, the piecewise color and ambient ramps, the separate
  night-gate and moon envelopes, and every product of those that more than one
  shader reads. Shaders only read the finished uniform values, so a new
  day-cycle term belongs here rather than recomputed per pixel.

- Water skybox components blend continuously from their daytime inputs to their
  night inputs using the moon envelope. Component 1's daytime endpoint includes
  its daytime boost.

## Precision and History

- Water phase and camera-relative UV origins are computed in `double`, reduced
  with `std::fmod`, then cast to float. The normal-map scroll integrates its
  reduced phase from per-frame size, speed, and delta time so tunable changes
  do not jump; while `gbGraphicsPinned` (`Render.h`) is set,
  `PopulateWaterReducedUv` holds that phase at zero and publishes a foam
  texture strength of zero, hiding the foam.
  Gerstner wave phase is not integrated, so a tunable change may jump it. The
  reductions consume the absolute camera position, rebuilt as a `double` from
  the camera cell and the camera's local position, so the pattern stays
  continuous when the camera changes cell.
- Rotate normal-map camera origins into the shader's pattern space before
  reduction, and preserve integral shader-side wrap multipliers. The stationary
  foam material layers and spawn noise each reduce camera position divided by
  their independent tile size modulo two in double precision before float
  publication, preserving the mirrored sampler's two-tile period. These CPU
  reductions are the precision owner for distant-world water sampling. Water
  Scale is applied on the CPU, so each scaled size, frequency, or tile size must
  be the identical float in its origin reduction and its published uniform, or
  the pattern drifts as the camera moves.
- Every retained world rectangle is expressed in the camera cell's frame and
  carries it through `RetainedAreaBasis` (`Render.h`), whose comment lists
  them.
- Lighting snaps its footprint to the light-deposit grid; shadow snaps to its
  own world-sized texel grid. Both publish current and previous areas for
  temporal sampling, and recreation reseeds history from the current area for
  one frame.
- Water owns foam footprint, time, controls, and reset publication. Graphics
  Water Foam activates the system only above Low water quality. Inactive foam
  skips CPU history and foam UV preparation; reactivation resets history with
  zero delta instead of advancing across the disabled interval. The Tweaks foam
  checkbox controls presentation only, so retained coverage continues updating
  while that checkbox hides it. Active foam delta uses `steady_clock` between
  published updates, including render gaps, and is zero while gameplay is
  paused, as wind's is, so foam reset or newly exposed while paused stays
  empty until the resume; simulation time detects rewinds only. First use,
  recreation, and rewind reset history;
  ordinary zoom and LOD changes reproject it. Spawn tuning affects new emission
  without resetting retained coverage. Threshold resolution preserves a positive
  J range.
- Each frame that publishes also copies the values
  `PresentationContinuitySnapshot` (`Render.h`) lists into the
  `gPresentationContinuity` snapshot, which only the agent probe reads
  (`../../Agent/AGENTS.md`). A new retained or reduced value that a continuity
  guarantee covers joins it; a frame that skips rendering leaves it untouched.
- Smoke and wind spread remap through current and previous world areas. Their
  enable, disable, recreate, and occupancy paths must clear or drain stale tiles
  without relying on command-buffer re-recording.
- GameBase publishes gameplay pause before Global. Paused smoke skips cadence
  and teleport refreshes while retaining its pending steps; its held area still
  follows camera-cell basis shifts. Clear, disable, and recreation paths retain
  precedence. Wind publishes zero delta and holds its accumulated time, but
  keeps ping-pong and occupancy rebuilding active. All smoke and wind producers
  publish zero deposit counts. Resume excludes paused frames and time.
- Smoke keeps one held area, step count, and decay for its single field. A
  scheduled frame (`## Ordering and Publication`) or teleport refreshes the
  field, stepping by the unpaused frames since its last refresh; a smoke clear
  and the disabled path also refresh it. A skipped frame publishes the held
  area as both current and previous and zeroes the dilate dispatch, so the next
  refresh remaps held to current (`RenderSmokeGlobal` in `SmokeUniforms.cpp`;
  see `../../../Data/Shaders/Wind/AGENTS.md` for wind's use of the pair).

## Area Roles

The visible area anchors water and geometry coverage. Shadow uses a
sunward-expanded, texel-snapped footprint; lighting uses the light-deposit grid
shared by spread, combine, and temporal passes. Grid snapping and camera-height
policy belong to Graphics.

## See Also

- Water shaders (`../../../Data/Shaders/Water/AGENTS.md`) - Shader-side phase
  and sampling constraints
- Smoke shaders (`../../../Data/Shaders/Smoke/AGENTS.md`) and Wind shaders
  (`../../../Data/Shaders/Wind/AGENTS.md`) - Temporal remapping
