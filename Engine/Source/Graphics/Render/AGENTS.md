# Render - Per-Frame GPU Data

Client-only population of mapped Global and Main layouts before their record-once command buffers submit. Each render subsystem owns its layout region; tunable-dependent staging remains separate from per-frame phase state.

Render free-runs between simulation ticks and sits outside the CRC (the per-tick checksum that keeps client and server bit-identical), so nothing computed here may feed simulation state.

## Ordering and Publication

- Main publishes water source dimensions, wave parameters, and displacement work
  even when the camera interpolate is absent; water drawing still requires it.
- Global population precedes Main. Global is submitted, possibly on a worker thread, before `RenderFrameMain` runs, so nothing in Main's call tree writes GlobalLayout: a value Main produces for a shader, such as the water band counts and active quad dimensions, is published in MainLayout. Within Global, Smoke precedes Wind because wind consumes smoke's current and previous world areas.
- Gameplay main rendering runs collection `BeginRender`, per-coord `Render`, collection `EndRender`, then debug publication, with the camera coord first. The no-renderable-coord path, including an empty active set, still runs begin/end publication so every indirect count and per-render profile counter reaches zero instead of ghost-drawing or reporting prior-frame instances.
- The terrain shadow elevation, source, blur, temporal, and history-copy chain
  shares one update interval, using the current/previous area latch
  (`TemporalAreaLatch`, `Render.h`). A refresh runs on the cadence, on
  reset or first use, or once the live visible area leaves the held shadow area
  (crop safety, no margin); it advances both areas and publishes full-texture
  indirect work from `RenderFrameGlobal`. A skipped frame republishes the held
  area pair and blend and writes zero indirect work; the elevation render-pass
  clear and every layout transition still run.
- The terrain shadow schedule and smoke layer stagger read
  `Graphics::miFrameCounter`, advanced once before `RenderFrameGlobal`.
  Terrain shadows refresh when `(miFrameCounter + 1) % shadowCadence == 0`;
  resets, first use, and crop safety force refresh outside that schedule.
- Downstream thresholds that the amplitude must cross before the effect applies may clear an upstream count only when they also skip the corresponding array write. Keep count and data publication paired.
- Write-combined mapped layouts are write-only. Compute dependent values in CPU staging state and copy each populated region once.
- State that crosses files, or that something outside this directory resets, lives as `inline` globals in `Render.h`. Single-file edge detectors and latches (the statics that remember the previous frame's value) stay function-local or file-static — do not promote them, because a promoted latch looks externally resettable and invites a second writer.
- Every latch assumes its entry point runs exactly once per frame. Hosts invoke `RenderWaterMain` in `WaterUniforms.cpp` once per frame from their water-rendering path to publish Main water data. Main wave phases use the supplied elapsed time; water's reduced-time accumulation remains on the Global publication path.
- The whole day cycle resolves on the CPU in `GlobalUniforms.cpp`: sun and moon angle and direction, the piecewise color and ambient ramps, the separate night-gate and moon envelopes, and every product of those that more than one shader reads. Shaders only read the finished uniform values, so a new day-cycle term belongs here rather than recomputed per pixel.

## Precision and History

- Water phase and camera-relative UV origins are computed in `double`, reduced with `std::fmod`, then cast to float. Integrate reduced phase from per-frame size, speed, and delta time so tunable changes do not jump. The reduction consumes the absolute camera position, rebuilt as a `double` from the camera cell and the camera's local position, so the pattern stays continuous when the camera changes cell.
- Rotate normal-map camera origins into the shader's pattern space before
  reduction, and preserve integral shader-side wrap multipliers. The stationary
  foam material layers and spawn noise each reduce camera position divided by
  their independent tile size modulo two in double precision before float
  publication, preserving the mirrored sampler's two-tile period. These CPU
  reductions are the precision owner for distant-world water sampling.
- Retained world rectangles — shadow and water foam history areas and the held visible area — are expressed in the camera cell's frame and carry it through `RetainedAreaBasis` (`Render.h`).
- Shadow snaps its footprint to its world-sized texel grid, publishes current and previous areas for temporal sampling, and reseeds history from the current area for one frame on recreation.
- Water owns foam footprint, time, controls, and reset publication. Graphics
  Water Foam activates the system only above Low water quality. Inactive foam
  skips CPU history and foam UV preparation; reactivation resets history with
  zero delta instead of advancing across the disabled interval. The Tweaks foam
  checkbox controls presentation only, so retained coverage continues updating
  while that checkbox hides it. Active foam delta uses `steady_clock` between
  published updates, including render gaps; simulation time detects rewinds
  only. First use, recreation, rewind, and a multi-cell basis jump reset history;
  ordinary zoom and LOD changes reproject it. Spawn tuning affects new emission
  without resetting retained coverage. Threshold resolution preserves a positive
  J range.
- Each publishing frame copies its camera basis, water origins, and retained
  area reports into `gPresentationContinuity` in `Render.h`; skipped rendering
  leaves it untouched. New values covered by continuity guarantees join this
  snapshot. The agent probe exposes water origins, shadow and
  smoke reports (`../../Agent/AGENTS.md`); foam history and foam texture phase
  remain internal to the snapshot.
- Smoke and wind spread remap through current and previous world areas. Their enable, disable, recreate, and occupancy paths must clear or drain stale tiles without relying on command-buffer re-recording.
- Each flight plane's smoke keeps its own held area, step count, and decay.
  With `gSmokeLayerStagger` on, a frame without a forced refresh refreshes only
  plane `miFrameCounter % gpFlightPlanes->Count()`, which steps by its frames
  since its last refresh; off, every live plane refreshes every frame. A smoke
  clear and the disabled path refresh every plane, and crop safety refreshes
  plane k early once the live visible area leaves plane k's held area (no
  margin). A skipped plane publishes its held area as both current and previous
  and zeroes its dilate dispatch, so its next refresh remaps held to current.
  Wind and wind deposits read plane 0's area pair, so wind's remap is the
  identity on frames plane 0 skips.

## Area Roles

The visible area anchors water and geometry coverage. Shadow uses a sunward-expanded, texel-snapped footprint. Grid snapping and camera-height policy belong to Graphics.

## See Also

- Water shaders (`../../../Data/Shaders/Water/AGENTS.md`) - Shader-side phase and sampling constraints
- Smoke shaders (`../../../Data/Shaders/Smoke/AGENTS.md`) and Wind shaders (`../../../Data/Shaders/Wind/AGENTS.md`) - Temporal remapping
