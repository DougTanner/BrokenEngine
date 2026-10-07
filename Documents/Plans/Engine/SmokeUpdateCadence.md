<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-07T12:02:28.364Z","dependsOn":[]} -->
# Smoke Update Cadence

## Context

Lighting already refreshes its spread chain every N render frames through `gLightingUpdateCadence` (`Engine/Source/Ui/LightingWrappersBase.cpp:61`). The mechanism lives in `Engine/Source/Graphics/Render/LightingUniforms.cpp`:
- The refresh predicate (`:139-142`) decides which frames refresh.
- A skipped frame writes zero indirect counts (`:393-415`).
- The current/previous world areas are held between refreshes (`:76-111`).
- An early refresh is forced when the camera leaves the held area (`IsVisibleAreaInsideHeldCombineCrop`, `:15-21, 95-96`).
- Deposits still run every frame.

This Plan adds the same optional cadence to the smoke simulation. Smoke deposits still land every frame. Each spread refresh advances the field by the number of frames since the previous refresh, so smoke drifts and fades at about the same rate as cadence 1.

Smoke is a plausible GPU cost:
- The simulation texture is 8192x4608 R16F at 4K Medium (`Engine/Source/Graphics/Graphics.cpp:27,71`).
- Each frame runs two spread halves, each an occupancy dilate plus a sparse indirect spread (`Engine/Source/Graphics/Managers/CommandBufferRecordGlobal.cpp:257-374`).
- The work is timed by `kGpuTimerSmokeSpread` (`Engine/Source/Profile/ProfileManagerBase.h:183`).

Smoke cannot copy the lighting mechanism directly, for three reasons:
1. **The spread count comes from the GPU.** The dilate writes the active-tile list. The dilate itself is a direct dispatch (`CommandBufferRecordGlobal.cpp:278`), so the CPU has no count to zero.
2. **The occupancy wipe is unconditional.** `vkCmdFillBuffer` clears each half's output occupancy every frame (`:309`). If only the spread were skipped, pass A's fill would erase the record of which tiles hold smoke in `SmokeTextureOne`, and that smoke would stop being processed.
3. **There is no time step.** Decay and the sampling offsets are fixed amounts per step (`Engine/Data/Shaders/Smoke/SmokeSpreadCommon.h:15-36`, `SmokeSpreadTwo.comp:74,79`).

Wind shares smoke's published area pair (`Engine/Source/Graphics/Render/WindUniforms.cpp:61-62`). Holding smoke's area on skipped frames therefore also holds wind's. Wind keeps spreading every frame inside the held area with no remap. On the next smoke refresh, both remap from the held area to the current one. This stays consistent and needs no wind change.

## Design

The author recommends this design because it reuses the lighting cadence shape (`Wrapper` slider, render-frame counter, host-visible indirect gating, held area pair with crop safety). The only new GPU mechanism is moving the occupancy reset into a gated pass.

1. **Setting.**
   - Add `Wrapper gSmokeUpdateCadence(1.0f, 1.0f, 4.0f, 1.0f)`. The user decided on default 1: the feature is off and today's look is unchanged.
   - It is persisted in `GraphicsSettings.bin`, with a format version bump so older files are rejected.
   - It is shown in the player Graphics menu's Smoke group and in the Smoke tweaks screen, mirroring how Lighting Update Cadence is exposed.
2. **Refresh predicate** (`RenderSmokeGlobal`). Keep a function-local render-frame counter and frames-since-refresh count, mirroring `LightingUniforms.cpp:139-142`. In the enabled, non-clear path, a frame refreshes when any of these holds:
   - the counter is a multiple of the cadence;
   - the live `gpCamera->mf4RenderVisibleArea` is no longer fully inside the held smoke area (crop safety).

   The clear path (`gbSmokeClear`) and the disabled path keep today's behavior: they always refresh with a step count of 1, so stale occupancy still drains. Both also reset the frames-since-refresh count. Re-enabling smoke always passes through a clear frame, because the enable edge sets `gbSmokeClear` (`SmokeUniforms.cpp:65-69`). The frame after a clear or disabled span therefore already starts from a consistent held area, and needs no extra forced refresh.
3. **Refresh frame.**
   - Publish `f4SmokeArea` = current and `f4PreviousSmokeArea` = held, then advance the held area. This is today's normal path, where `sf4PreviousSmokeArea` is the held area.
   - Publish step count `k` = frames since the last refresh (1..N).
   - Publish `fSmokeDecay = pow(gSmokeDecay, k)`.
   - Write dilate dispatch `(groups, 1, 1)` for both dilate pipelines.
4. **Skip frame.**
   - Publish both `f4SmokeArea` and `f4PreviousSmokeArea` as the held area, so deposits, consumers, and wind all project into the area the texture was last written in.
   - Do not advance the held area.
   - Write dilate dispatch `(groups, 1, 0)`. The active-tile list then stays at its `{0,1,1}` reset, so both indirect spreads dispatch nothing.
   - The existing one-cell `RetainedAreaBasis` shift of `sf4PreviousSmokeArea` keeps the held area correct across skips.
5. **Gated occupancy reset.**
   - Make `kPipelineSmokeOccupancyDilate` and `kPipelineSmokeOccupancyDilateRemap` host-visible indirect, and record them with `RecordComputeIndirect`. This is the same path lighting combine uses with `WriteIndirectComputeBuffer`.
   - Remove the `vkCmdFillBuffer` of the output occupancy, together with its transfer barriers.
   - Each dilate invocation reads and clears its own tile bit in the output occupancy with `atomicAnd(ownOccupancy[w], ~bit)`. The returned prior bit is today's stale-storage union term.
   - Other invocations only read their own bits of that buffer, so clearing one bit does not disturb them.
   - The spread still re-marks nonzero tiles with `atomicOr`. Every tile whose prior bit was set is active, so after the spread its bit is correct.
   - On a skip frame nothing runs, so occupancy is preserved.
   - The barrier after the dilate changes from shader-read→transfer-write to shader-write→shader-read/write (compute to compute) for the output occupancy.
6. **Step scaling in shaders.**
   - Add `float fSmokeStepCount` to `GlobalLayout` (`Engine/Data/Shaders/ShaderGlobalLayout.h`), written only by `RenderSmokeGlobal`.
   - In `SmokeSpread`, multiply the per-texel sampling offset `f2Noise + f2WindAdvection` and `f2WindDisplacement` by `fSmokeStepCount`. These products vary per texel, so they stay in the shader.
   - **Dilate radius.** Scale the dilate's neighborhood to match: both dilate shaders' ±2-tile (Chebyshev-2) loops become ±`2 * int(globalLayout.fSmokeStepCount)` tiles (`SmokeOccupancyDilate.comp:48-61`, `SmokeOccupancyDilateRemap.comp:61-74`).
     - The ±2 radius is the coverage bound for one step's sampling offset.
     - Offsets up to k times larger need a radius k times larger. Otherwise, a tile that smoke reaches in one refresh is never made active, and the moving front of the plume is dropped.
     - The extra work is tile-level lookups only (81 instead of 25 at k = 2, 289 at k = 4), and it is skipped entirely on skip frames.
   - Decay compensation is the CPU-side `pow` from item 3. That one uniform covers both the shared decay multiply and pass B's terrain decay exponent.
   - Pass B's step-down branch reads the compensated decay. Its exact-zero progress proof is stated per applied decay, so it stays valid.
   - In `SmokeSpreadTwo.comp`, raise the edge-decay factor to the power `fSmokeStepCount`.
   - The step-down walk stays one step per refresh. Very faint smoke drains up to N times more slowly, but still reaches exact zero.
   - At `k = 1` every expression equals today's math.

## Critical files

- `Engine/Source/Ui/SmokeWrappersBase.h`, `.cpp`
- `Engine/Source/Ui/GraphicsSettings.cpp`
- `Engine/Source/Ui/Screens/GraphicsMenuScreen.cpp`
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenSmoke.cpp`
- `Engine/Source/Graphics/Render/SmokeUniforms.cpp`
- `Engine/Source/Graphics/Managers/CommandBufferRecordGlobal.cpp`, `.h`
- `Engine/Source/Graphics/Managers/PipelineManager.cpp`
- `Engine/Data/Shaders/ShaderGlobalLayout.h`
- `Engine/Data/Shaders/Smoke/SmokeOccupancyDilate.comp`, `SmokeOccupancyDilateRemap.comp`, `SmokeSpreadCommon.h`, `SmokeSpreadTwo.comp`

## In scope

- **`SmokeWrappersBase.h/.cpp`:** declaration and definition of `gSmokeUpdateCadence`.
- **`GraphicsSettings.cpp`:**
  - the `GraphicsSettings` struct field `fSmokeUpdateCadence`;
  - its offset and `sizeof` `static_assert`s;
  - `kiVersion` 15 → 16;
  - the save initializer, the load-validation `IsInRange` check, the load `Set`, and the Defaults reset, each mirroring the `fLightingUpdateCadence` sites.
- **`GraphicsMenuScreen.cpp`:** one `ColumnSlider("Smoke Update Cadence", &gSmokeUpdateCadence, "%.0f")` inside the existing Smoke group (`:174-180`), after the Smoke Area slider.
- **`TweaksScreenSmoke.cpp`:** one slider-map entry and one `WrapperSlider` row for the cadence, in wrapper declaration order.
- **`SmokeUniforms.cpp` `RenderSmokeGlobal`:**
  - the refresh predicate and crop test, as a file-static helper mirroring `IsVisibleAreaInsideHeldCombineCrop` but against the held smoke area with no texel margin;
  - the held-area publication on skip frames;
  - the `fSmokeDecay` and `fSmokeStepCount` publication;
  - the dilate `WriteIndirectComputeBuffer` writes in the clear, disabled, refresh, and skip paths.
- **`PipelineManager.cpp`:** the shared `.flags` (`:249`) of the loop that creates exactly the two smoke occupancy dilate pipelines (`:241-242`) gains `kIndirectHostVisible`.
- **`CommandBufferRecordGlobal.cpp` and `CommandBufferRecordGlobal.h`:**
  - In `RecordSmokeSpreadHalf`:
    - replace the dilate `RecordCompute` with `RecordComputeIndirect`, and drop its now-unused `iDilateGroups` parameter;
    - remove the output-occupancy `vkCmdFillBuffer` and its fill barrier;
    - change the post-dilate output-occupancy barrier to compute→compute.
  - In `RecordSmokeSpreadPipeline`, drop the `iTotalTiles` and `iDilateGroups` locals and the `iSmokeTilesX`/`iSmokeTilesY` parameters once unused.
  - In the caller in `Record` (`:47-50`), drop the smoke tile-count locals and arguments once unused.
  - `RenderSmokeGlobal` now writes the X group count each frame, using today's record-time formula (`CommandBufferRecordGlobal.cpp:334-335`) over `uiSmokeTilesX * uiSmokeTilesY`.
- **`ShaderGlobalLayout.h`:** the new `fSmokeStepCount` field, placed so the scalar layout stays valid.
- **`SmokeOccupancyDilate.comp` and `SmokeOccupancyDilateRemap.comp`:** the `ownOccupancyBuffer` becomes writable, the own-bit read becomes an `atomicAnd` read-and-clear, and the neighborhood loop radius becomes `2 * int(globalLayout.fSmokeStepCount)`.
- **`SmokeSpreadCommon.h` `SmokeSpread`:** offset scaling by `fSmokeStepCount`.
- **`SmokeSpreadTwo.comp`:** edge-decay power by `fSmokeStepCount`.
- **Documentation:**
  - `Engine/Data/Shaders/Smoke/AGENTS.md` `## Frame and Remap Contract`: per refresh, skip frames hold the area, and the step count.
  - `Engine/Source/Graphics/Managers/AGENTS.md` `### BufferManager` smoke bullet: the dilate read-and-clear replaces the output-occupancy reset.
  - `Engine/Source/Graphics/Render/AGENTS.md`: the smoke/wind area bullet gains the held area on skip and crop safety.
  - `Engine/Source/Ui/AGENTS.md`: the `GraphicsSettings.bin` version line.
  - `Documents/UserInterfaceDesign.txt`: the smoke group description (`:326-328`), and the slider-precision sentence (`:319-321`) naming Smoke Update Cadence beside Lighting Update Cadence as whole-frame with no decimals.

## Out of scope

- Wind: wind uniforms, shaders, and recording, its ping-pong parity, and any wind cadence.
- Lighting deposit gating on skipped lighting frames.
- Shadow-chain cadence.
- Any other renderer pass.
- Smoke deposit strength (`Smoke.frag` `kfSmokeDepositNormPerFrame`) and any frame-rate independence for smoke.
- The pass-B step-down walk and zero threshold.
- Any new harness command, profile counter, or GPU timer.
- Refactoring the lighting cadence to share code with smoke.

## Risk tier and invariants

**Tier 3.** It changes the `GraphicsSettings.bin` layout and version (serialization) and a CPU/GPU layout field, and it spans Ui, Graphics, and shaders.

Invariants:
- Smoke and wind areas are published once per frame, smoke before wind.
- On a skip frame the published current area equals the published previous area, and equals the area the smoke texture was last written in.
- Occupancy describes persistent texture contents across skip frames.
- Smoke output still converges to exact zero.
- Command buffers stay record-once; all per-frame variation goes through uniforms and indirect buffers.
- At cadence 1, behavior is identical to today.
- The client render path stays outside the CRC.

## Acceptance criteria

1. **Baseline first (go/no-go), before any code change.**
   - Setup: a Debug client (`set_slider` is Debug-only, and the GPU timers measure GPU time only), fullscreen 3840x2160, Immediate present mode, and a heavy-smoke scene.
   - Capture `query_profile` cohorts as `commands-client.md` `query_profile` requires: discard a warm-up cohort, then take equal-size cohorts in one process lifetime.
   - Record `averageUs` for Global+Main+Image, Smoke Spread, and Wind Spread.
   - If Smoke Spread is below 5% of Global+Main+Image, stop and report the numbers instead of implementing.
2. The client and server build cleanly.
3. **Cadence 1.** A screenshot of the same heavy-smoke scene shows no visible difference from the pre-change baseline screenshot.
4. **Cadence 2 and 4.**
   - Screenshots while panning and zooming show no gaps at the smoke-area edges and no frozen smoke.
   - Smoke drifts and fades at roughly the cadence-1 rate.
   - `presentation_continuity_probe` shows equal smoke current and previous areas on skip frames.
5. **Performance.**
   - At cadence 2 and 4, Smoke Spread `averageUs` falls to roughly 1/2 and 1/4 of cadence 1, measured with the criterion 1 protocol.
   - Report GPU frame time (Global+Main+Image) and overlay fps at cadence 1, 2, and 4.
6. A version-15 `GraphicsSettings.bin` fails the current-format gate the same way version 14 does today (`Engine/Source/Ui/AGENTS.md`).
