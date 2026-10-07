<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-07T12:28:15.491Z","dependsOn":["Documents/Plans/Engine/SmokeUpdateCadence.md"]} -->
# Terrain Shadow Update Cadence

## Context

Lighting already refreshes its spread, combine, and temporal chain every N render frames through `gLightingUpdateCadence` (`Engine/Source/Ui/LightingWrappersBase.cpp:61`). The mechanism is in `Engine/Source/Graphics/Render/LightingUniforms.cpp`:
- A refresh predicate (`:139-142`, `:95-96`) decides which frames refresh.
- Skipped frames write zero counts to host-visible indirect buffers (`:393-415`).
- A latch holds the current/previous area pair (`LightingTemporalAreaLatch`, `:23-56`, used at `:97-109`).

The sibling Plan `Documents/Plans/Engine/SmokeUpdateCadence.md` adds the same option for smoke. This Plan adds it for the terrain-shadow chain. The user decided that each cadence is its own Plan, that the slider is optional, and that the smoke slider defaults to 1 (off). This Plan uses the same default.

The terrain-shadow chain is a good fit:
- **What it depends on.** Its result depends only on terrain elevation, the sun, and the shadow area (`Engine/Data/Shaders/Shadow/AGENTS.md`). No unit or moving-object data feeds it, because object shadows are a separate Main-buffer chain.
- **How slowly that changes.** The sun moves slowly, and island placements change rarely.
- **Existing history.** It already has temporal history with per-texel rejection.

All six passes are in the Global command buffer, in `RecordShadowPasses`, `Engine/Source/Graphics/Managers/CommandBufferRecordGlobal.cpp:75-104`, timed by `kGpuTimerShadow`:
1. the `ShadowElevation` render pass, with a clear and one draw of `kiMaxActivePlacements` instances;
2. `Shadow` (ray-march);
3. `ShadowBlurH`;
4. `ShadowBlurV`;
5. `ShadowTemporal`, which works in place on `ShadowBlur`;
6. `ShadowHistoryCopy`.

Consumers (Terrain, Water, Model) sample only `mShadowBlurTexture` through `f4ShadowArea` (`Terrain.frag:145`, `Water.frag:151`, `Model.frag:261-262`).

Why the shadow chain cannot simply be skipped today:
- **Fixed counts.** All six record calls use direct draws or dispatches with counts fixed at record time (`RecordDraw` and `RecordCompute`). No pipeline uses `kIndirectHostVisible` (`WorldLightingShadowPipelines.cpp:135,152,165,178,219,232`).
- **The area moves every frame.** `PopulateShadowArea` (`Engine/Source/Graphics/Render/GlobalUniforms.cpp:287-322`) publishes the live area every frame through `TemporalAreaLatch` (`Engine/Source/Graphics/Render/Render.h:86-111`). That latch moves `previous` to the last call's current every call, so it cannot hold a pair. On a skipped frame, consumers would sample the old texture through a moved area, and shadows would slide with the camera.

## Design

The author recommends this design because it copies the lighting cadence exactly and reuses its existing latch. The only difference is where the indirect counts are written: shadow is in the Global command buffer, so they are written from `RenderFrameGlobal`'s call tree before Global submission.

1. **One shared latch.**
   - Move `LightingTemporalAreaLatch` out of `LightingUniforms.cpp` into `Render.h`, replacing the current `TemporalAreaLatch` with the same name `TemporalAreaLatch`. The old struct's only user is shadow.
   - Lighting keeps identical behavior.
   - For shadow at cadence 1, the held-pair latch called every frame publishes exactly what the old latch did: previous equals the last call's current, and blend is 1 on reset or first use.
2. **Setting.**
   - Add `Wrapper gShadowUpdateCadence(1.0f, 1.0f, 4.0f, 1.0f)`. Default 1 means off.
   - Persist it in `GraphicsSettings.bin`, bumping the format version by one from its value when this Plan executes. That is 16 → 17 if `SmokeUpdateCadence` has landed, which the `dependsOn` edge orders first.
   - In the player Graphics menu, add a `"Terrain Shadow Update Cadence"` slider directly under the `"Terrain Shadows"` row.
   - In the Shadow tweaks screen, add an `"Update Cadence"` row in the Quality / Perf group, after Temporal Blend.
3. **Refresh predicate** (`PopulateShadowArea`).
   - Keep a function-local render-frame counter.
   - A refresh happens when any of these holds:
     - `gbShadowTemporalReset` (recreate, multi-cell jump);
     - the latch is uninitialized;
     - `counter % cadence == 0`;
     - crop safety: the live `gpCamera->mf4RenderVisibleArea` is not fully inside the held `f4ShadowArea`.
   - The crop test uses the held shadow area itself, not lighting's held-visible-area-plus-one-texel test. The whole shadow texture covers its area (1.5x headroom), and consumers sample all of it, so any visible area inside the held area is fully covered.
   - Zooming out grows the live area and quickly fails the test. Zooming in keeps the larger held area, at lower density, until the next scheduled refresh, which is at most N-1 frames later.
4. **Publication.**
   - On a refresh frame, call `sTemporalAreaLatch.Update(...)` and publish its current area, previous area, and blend.
   - On a skip frame, republish the latch's held `f4CurrentArea`, `f4PreviousArea`, and `fBlend` into `f4ShadowArea`, `f4ShadowAreaPrevious`, and `fShadowTemporalBlend`.
   - `f4ShadowAreaExtra` already derives from the published `f4ShadowArea` (`GlobalUniforms.cpp:329`), so it follows automatically.
   - On a one-cell basis step, shift both latch areas, as lighting does at `LightingUniforms.cpp:85-86`. Today only `previous` is shifted (`GlobalUniforms.cpp:305`).
   - The `gPresentationContinuity.shadow` areas report the latch pair.
   - The temporal blend is not compensated for the number of skipped frames. This mirrors lighting: each refresh blends once.
5. **Indirect gating.**
   - Add `PipelineFlags::kIndirectHostVisible` to all six shadow pipelines. ShadowElevation's flag set becomes the existing `DynamicPipelines.cpp:173` combination.
   - Switch the six record calls to `RecordDrawIndirect(iCommandBuffer, vkCommandBuffer, {1,0,0,0})` and `RecordComputeIndirect`.
   - Each frame, `PopulateShadowArea` writes slot `iCommandBuffer`:
     - `WriteIndirectBuffer(iCommandBuffer, refresh ? kiMaxActivePlacements : 0)` for ShadowElevation;
     - `WriteIndirectComputeBuffer(iCommandBuffer, iTilesX, iTilesY, refresh ? 1 : 0)` for the five computes, with tile counts from `TileCount` of the `mShadowTexture` extent.
   - The render-pass clear and all layout transitions stay unconditional. The cleared `ShadowElevation` texture is read only by `Shadow.comp`, which is skipped on the same frames. This is the same as lighting's unconditional spread clears.
   - All five computes gate on one decision, because BlurV, Temporal, and HistoryCopy write `ShadowBlur` and the history texture in sequence.
6. **Accepted delays.** Island placement changes, heightmap residency, and sun-angle changes (including the Graphics-screen sun override slider) reach the shadow at most N-1 frames late.

## Critical files

- `Engine/Source/Graphics/Render/Render.h`
- `Engine/Source/Graphics/Render/LightingUniforms.cpp`
- `Engine/Source/Graphics/Render/GlobalUniforms.cpp`
- `Engine/Source/Graphics/Managers/CommandBufferRecordGlobal.cpp`
- `Engine/Source/Graphics/Managers/WorldLightingShadowPipelines.cpp`
- `Engine/Source/Ui/ShadowWrappersBase.h`, `.cpp`
- `Engine/Source/Ui/GraphicsSettings.cpp`
- `Engine/Source/Ui/Screens/GraphicsMenuScreen.cpp`
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenShadow.cpp`

## In scope

- **`Render.h`:** replace `struct TemporalAreaLatch` (`:86-111`) with the held-pair latch body moved from `LightingUniforms.cpp:23-56`.
- **`LightingUniforms.cpp`:** delete `LightingTemporalAreaLatch` and change the `sTemporalAreaLatch` declaration (`:77`) to the moved type. Nothing else changes.
- **`GlobalUniforms.cpp`:**
  - `PopulateShadowArea`: refresh counter and predicate, crop test, held-pair publication, two-area basis shift, and the six indirect writes.
  - `PopulateShadowParameters` and its call in `RenderFrameGlobal`: only to pass `iCommandBuffer` through to `PopulateShadowArea`, if it is not already reachable.
- **`WorldLightingShadowPipelines.cpp` `CreatePipelineShadows`:** the `flags` of the six shadow pipelines.
- **`CommandBufferRecordGlobal.cpp` `RecordShadowPasses`:** the six record calls (`:83, 85, 88, 91, 96, 99`), plus removing the record-time tile count locals if they become unused.
- **`ShadowWrappersBase.h/.cpp`:** declaration and definition of `gShadowUpdateCadence`.
- **`GraphicsSettings.cpp`:**
  - the field `fShadowUpdateCadence` and its offset and `sizeof` `static_assert`s;
  - the `kiVersion` bump by one;
  - the save, `IsInRange` load validation, load `Set`, and Defaults sites, mirroring `fLightingUpdateCadence`.
- **`GraphicsMenuScreen.cpp`:** one `ColumnSlider("Terrain Shadow Update Cadence", &gShadowUpdateCadence, "%.0f")` directly after the `"Terrain Shadows"` `RadioRow` (`:156-159`).
- **`TweaksScreenShadow.cpp`:** one registrar entry and one `WrapperSlider("Update Cadence", ...)` row in the Quality / Perf group, both in wrapper declaration order.
- **Documentation:**
  - `Engine/Data/Shaders/Shadow/AGENTS.md`: the full-texture and record-once lines at `:10-11`. Group counts are now written per frame to indirect buffers, and the chain refreshes on the shadow cadence.
  - `Engine/Source/Graphics/Render/AGENTS.md` `## Ordering and Publication`: add the shadow chain's shared update interval beside the lighting bullet.
  - `Engine/Source/Graphics/AGENTS.md` `## World and Camera Contracts`: the "Shadow uses the same per-texel rejection" sentence gains the shadow cadence and its crop test.
  - `Engine/Source/Ui/AGENTS.md`: the `GraphicsSettings.bin` version line.
  - `Documents/UserInterfaceDesign.txt`:
    - `:325-328`: the Terrain Shadows row gains its cadence slider;
    - the slider-precision sentence (`:319-321`) adds Terrain Shadow Update Cadence to the whole-frame, no-decimals cadence sliders.

## Out of scope

- The `TerrainElevation` pass (`CommandBufferRecordGlobal.cpp:113-117`). It is not part of the shadow chain.
- Object shadows (Main command buffer).
- Smoke, wind, and lighting cadence behavior. Lighting changes only by the latch type move.
- Compensating the shadow temporal blend for skipped frames.
- Phase offsets or balancing between cadences, which is a separate Plan.
- The `Engine/Source/Graphics/Objects/ModelPipeline.cpp:36-37` shadow sampler lacking `kSamplerBorderWhite`. It is a pre-existing deviation and is not changed here.
- Any new harness command, profile counter, or GPU timer.

## Risk tier and invariants

**Tier 3.** It changes the `GraphicsSettings.bin` layout and version (serialization), touches a latch shared with lighting, and spans Ui and Graphics.

Invariants:
- At cadence 1, shadow and lighting output is identical to today.
- On a skip frame, the published `f4ShadowArea` and `f4ShadowAreaPrevious` equal the area pair the shadow texture and history were last written with.
- The six shadow passes refresh or skip together.
- Command buffers stay record-once; per-frame variation goes only through uniforms and indirect buffers. Indirect slots are written every frame, because pipeline recreation zeroes them.
- Retained areas follow `RetainedAreaBasis`.
- Render state stays outside the CRC.

## Acceptance criteria

1. **Baseline first (go/no-go), before any code change.**
   - Setup: a Debug client, fullscreen 3840x2160, Immediate present mode, terrain shadows at the default and at High.
   - Capture `query_profile` cohorts as the `commands-client.md` `query_profile` entry requires: discard a warm-up cohort, then take equal-size cohorts in one process lifetime.
   - Record `averageUs` for Shadow and for Global+Main+Image.
   - If Shadow is below 5% of Global+Main+Image at the default quality, stop and report the numbers instead of implementing.
2. The client and server build cleanly.
3. **Cadence 1.** Screenshots of a fixed scene show no visible difference from the pre-change baseline, for both shadows and lighting.
4. **Cadence 2 and 4.**
   - Screenshots while panning, zooming in and out, and moving the sun override slider show no shadow sliding with the camera, no missing shadow at the screen edges, and no seams.
   - `presentation_continuity_probe` shows the shadow area pair unchanged across skip frames.
5. **Performance.**
   - At cadence 2 and 4, the Shadow row's `averageUs` falls to roughly 1/2 and 1/4 of cadence 1, measured with the criterion 1 protocol. `shadowSample.currentUs` alternates between the full-chain cost and near zero.
   - Report GPU frame time (Global+Main+Image) and overlay fps at cadence 1, 2, and 4.
6. A `GraphicsSettings.bin` from the previous version fails the current-format gate.

