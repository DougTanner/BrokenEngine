<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-17T19:39:27.764Z","dependsOn":[]} -->
# Stagger smoke layer refreshes and offset terrain shadows from lighting

## Context

Three client render chains refresh on a schedule instead of every frame. Each
keeps its own private frame counter:

- **Lighting:** `RenderLightingGlobal`
  (`Engine/Source/Graphics/Render/LightingUniforms.cpp`) counts
  `siLightingRefreshFrame` and refreshes when it is a multiple of
  `gLightingUpdateCadence` (1 to 4, default 2).
- **Terrain shadows:** `PopulateShadowArea` (`GlobalUniforms.cpp`) counts
  `siShadowRenderFrame` against `gShadowUpdateCadence` (1 to 4, default 4).
- **Smoke:** `RenderSmokeGlobal` (`SmokeUniforms.cpp`) counts
  `siSmokeRenderFrame` against `gSmokeUpdateCadence` (1 to 4, default 2).

All three counters start at 0 and advance once per `RenderFrameGlobal`, so they
stay aligned, and chains with even cadences refresh on the same frames. At the
defaults, lighting (2) and terrain shadows (4) both refresh on every fourth
frame, and the GPU frame time spikes there.

Since commit `259772c0`, smoke runs one field per flight plane (up to
`kiMaxFlightPlanes`, which is 8; the game uses 3). Every plane shares one area
pair (`GlobalLayout::f4SmokeArea`, `f4PreviousSmokeArea`,
`f2PreviousSmokeAreaSizeInverse`) and one step count and decay
(`fSmokeStepCount`, `fSmokeDecay`), so a smoke refresh spreads every plane at
once and the refresh frame pays for all of them.

This Plan replaces the smoke cadence with a per-plane stagger, and replaces the
three private counters with the renderer's existing frame counter,
`Graphics::miFrameCounter`, offsetting the terrain shadow schedule from
lighting's when an offset can help. It supersedes the retired
`CadencePhaseBalancing` Plan, which proposed a measured, weighted phase search.
The user approved the overall design: a checkbox replaces the smoke cadence
slider; off refreshes every layer every frame; on refreshes one layer per frame;
the checkbox defaults on; and lighting/shadow phase balancing uses no weights.
The remaining details are the author's choices.

## Design

### Part A: smoke layer stagger

1. **Setting.** Delete `gSmokeUpdateCadence`. Add a bool wrapper
   `gSmokeLayerStagger` beside `gSmokeEnabled` in
   `Ui/GraphicsSettingsWrappersBase.h`/`.cpp`, where the persisted Graphics menu
   bools live, declared the same way (`Wrapper gSmokeLayerStagger(true);`). The
   Graphics menu shows it with
   `WrapperToggle("Stagger Smoke Layer Updates", &gSmokeLayerStagger)` in place
   of the "Smoke Update Cadence" slider, inside the existing Smoke disabled
   block. The user chose to default it on, the cost-saving mode; today's
   default cadence of 2 is already the cost-saving choice.
2. **Schedule.**
   - Off: every live plane refreshes every frame, which is today's cadence-1
     behavior.
   - On: on a frame with no forced refresh, exactly one plane refreshes:
     plane `gpGraphics->miFrameCounter % gpFlightPlanes->Count()`. Each plane therefore
     refreshes every plane-count frames and advances by its own frames since its
     last refresh. The largest step count is the plane count (3 in the game);
     the old cadence slider topped out at 4.
3. **Forced refreshes** refresh every plane they affect, on the frame they fire:
   - a smoke clear (`gbSmokeClear`, set for example by the enable/disable edge,
     texture recreation, or a camera jump of more than one cell) and the
     disabled path refresh every plane, as today;
   - crop safety refreshes plane k early once the live visible area leaves
     plane k's held area (`IsVisibleAreaInsideHeldArea` against that plane's
     held area).

   A one-cell camera step shifts every live plane's held area with `ShiftArea`,
   as it shifts the single held area today.
4. **Per-plane CPU state** in `RenderSmokeGlobal`:
   - The held area `sf4PreviousSmokeArea` and the counter
     `siSmokeFramesSinceRefresh` become arrays sized `shaders::kiMaxFlightPlanes`;
     only the first `gpFlightPlanes->Count()` slots are used. First use sets
     every live plane's held area to the current area.
   - `siSmokeRenderFrame` and the cadence read are deleted.
   - Each publishing exit (clear, disabled, skip, refresh) runs per plane. It
     writes that plane's slot of each array in item 5, that plane's step count
     and decay through `PublishSmokeStep`, and that plane's dilate dispatch: Z 1
     to run, Z 0 to skip.
   - The clear and disabled exits keep their writes to the two shared clear
     pipelines.
5. **GlobalLayout** (`Engine/Data/Shaders/ShaderGlobalLayout.h`, one header for
   C++ and GLSL, scalar layout). Replace the five shared fields with per-plane
   arrays sized `kiMaxFlightPlanes`, named like the existing
   `pfFlightPlaneHeights`:
   - `vec4 pf4SmokeAreas[kiMaxFlightPlanes]` replaces `f4SmokeArea`;
   - `vec4 pf4PreviousSmokeAreas[kiMaxFlightPlanes]` replaces
     `f4PreviousSmokeArea`;
   - `vec2 pf2PreviousSmokeAreaSizeInverses[kiMaxFlightPlanes]` replaces
     `f2PreviousSmokeAreaSizeInverse`;
   - `float pfSmokeStepCounts[kiMaxFlightPlanes]` replaces `fSmokeStepCount`;
   - `float pfSmokeDecays[kiMaxFlightPlanes]` replaces `fSmokeDecay`.

   `PopulatePreviousSmokeAreaSizeInverse` and `PublishSmokeStep` take the plane
   index. `f2SmokeCurlOffsetScale` stays one shared value: its existing
   reasoning (the area size cancels, and every published area shares the same
   construction) holds for each plane's area; only its comment changes to name
   the per-plane areas.
6. **How each shader learns its plane.** Every case uses the existing push
   constant `PushConstantsLayout::f4Pipeline`:
   - Pass B (`SmokeSpreadTwo.comp`) already reads its plane from
     `f4Pipeline.x`. It now also indexes its areas, step count, and decay by it.
   - Pass A (`SmokeSpreadOne.comp`) and the remap dilate
     (`SmokeOccupancyDilateRemap.comp`) gain the push-constant block and read
     their plane from `f4Pipeline.x`. In
     `PipelineManager::CreateSmokeWindPipelines`, `kSmokeSpreadComputeA` gains
     `PipelineFlags::kPushConstants`, and so does the one shared `Create` call
     that builds both dilate kinds. The plain dilate
     (`SmokeOccupancyDilate.comp`) reads no area, declares no push-constant
     block, and ignores the pushed values.
   - `CommandBufferRecordGlobal::RecordSmokeSpreadHalf` passes its push
     constants to the dilate record as well as the spread record; rename the
     parameter `rf4SpreadPushConstants` to `rf4PushConstants`.
     `RecordSmokeSpreadPipeline` passes `{plane, 0, 0, 0}` to both halves.
   - Deposits: the per-plane smoke deposit loop in
     `CommandBufferRecordMain.cpp` records each plane's deposit pipelines with
     `{2, plane, 0, 0}` instead of `{2, 0, 0, 0}`. `QuadsVisibleArea.vert` and
     `QuadsAxisAlignedVisibleArea.vert` read
     `pf4SmokeAreas[int(f4Pipeline.y)]` for selector 2. Wind deposits keep
     `{2, 0, 0, 0}`, so they project through plane 0's area, which is the wind
     area.
   - World sampling already loops over planes. `SmokeShadow` and
     `SmokeShadowFaded` (`ShaderFunctions.h`) move the texcoord computation into
     their loops and read `pf4SmokeAreas[iPlane]`; `Terrain.frag` and
     `Water.frag` read `pf4SmokeAreas[iPlane]` in their loops. `AddSmoke` gains
     an `int iPlane` parameter, which `Model.frag` passes. Particles sample
     through `SmokeShadowFaded`.
   - `SmokeSpread` (`SmokeSpreadCommon.h`) gains an `int iPlane` parameter for
     the step count and decay; both spread passes pass their plane.
7. **Per-plane dispatch counts.** Each plane already owns its two dilate
   pipelines, which use per-framebuffer host-visible indirect commands, and a Z
   count of 0 skips both dilates and so both spreads for that plane.
   `WriteSmokeDilateDispatch` takes the plane index and writes only that plane's
   two dilates; each exit calls it once per live plane.
8. **Wind** stays one shared field and reads plane 0's area pair:
   `WindOccupancyDilate.comp`, `WindSpreadOne.comp`, and `WindSpreadTwo.comp`
   read index 0. Smoke spreads sample wind through that wind area, not their
   own plane's area:
   - pass B computes its wind texcoord with `pf4SmokeAreas[0]`;
   - pass A today samples wind at its own output UV, which equals the wind UV
     only while both share one area. It now converts its world position with
     `WorldToSmokeTexcoord(globalLayout.pf4SmokeAreas[0], f2WorldPosition)`.

   The wind sampler clamps, so a plane area that reaches past the wind area
   reads the wind edge.
9. **Continuity probe.** The author chose: `gPresentationContinuity.smoke`,
   written by `PublishSmokeContinuity`, reports plane 0's published area pair,
   the pair wind reads. `iHistoryResets` and the probe's JSON shape are
   unchanged.
10. **Persisted settings** (`Ui/GraphicsSettings.cpp`). `GraphicsSettings`
    drops `fSmokeUpdateCadence` and stores the bool as a new flag bit,
    `GraphicsSettingsFlags::kSmokeLayerStagger = 1 << 7`, the same way `kSmoke`,
    `kWind`, and `kLighting` are stored.
    - The fields after it move up 4 bytes and the struct shrinks from 52 to 48
      bytes; update the offset and size `static_assert`s.
    - Bump `kiVersion` from 17 to 18, so an older file fails the version gate in
      `ReadVersionedFile` and the defaults load. No migration.
    - `SaveGraphicsSettings`, `FindInvalidGraphicsSetting` (its cadence check is
      deleted), `LoadGraphicsSettings`, and `ResetGraphicsSettings` follow the
      existing flag pattern.

### Part B: frame counter and lighting/shadow phase

1. **Counter.** The lighting, terrain shadow, and smoke stagger predicates read
   `gpGraphics->miFrameCounter` (`Graphics.h`), replacing
   `siLightingRefreshFrame`, `siShadowRenderFrame`, and `siSmokeRenderFrame`.
   No new counter is added. `Graphics::RenderGlobal` advances it once, just
   before it calls `RenderFrameGlobal`, and it already serves as the island LRU
   grace clock and `MainLayout.iRenderNumber`. Each old counter also started at
   0 and was incremented once per frame before use, so the lighting schedule
   does not change.
2. **Phase.** Lighting's phase is 0. The terrain shadow phase is 1 when
   `std::gcd(lightingCadence, shadowCadence) > 1`, and 0 otherwise. A chain
   refreshes on schedule when
   `(gpGraphics->miFrameCounter + phase) % cadence == 0`.
   `PopulateShadowArea` computes the shadow phase every frame from the current
   values of `gLightingUpdateCadence` and `gShadowUpdateCadence`; nothing is
   cached.
3. **Why this rule works.** Two every-N schedules offset by d frames coincide
   only if their gcd divides d. When the gcd is above 1, an offset of 1 never
   coincides. When the gcd is 1, coinciding once every lcm frames cannot be
   avoided by any offset. So the rule needs no cost weights, search, or
   profiling constants. Smoke is left out because with the stagger it is a
   steady per-frame load either way.
4. **Forced refreshes are unchanged and stay outside the schedule:** the
   lighting reset (`gbLightingTemporalReset`), the shadow reset
   (`gbShadowTemporalReset`), first use, and crop safety. Changing a cadence
   slider can move the shadow's next refresh by up to cadence-1 frames once, as a
   cadence change already does today.

## Critical files

- `Engine/Source/Graphics/Render/GlobalUniforms.cpp`
- `Engine/Source/Graphics/Render/LightingUniforms.cpp`
- `Engine/Source/Graphics/Render/SmokeUniforms.cpp`
- `Engine/Data/Shaders/ShaderGlobalLayout.h`
- `Engine/Data/Shaders/ShaderFunctions.h`
- `Engine/Data/Shaders/Smoke/` spread, dilate, and common shaders
- `Engine/Source/Graphics/Managers/PipelineManager.cpp`,
  `CommandBufferRecordGlobal.cpp`/`.h`, `CommandBufferRecordMain.cpp`
- `Engine/Source/Ui/GraphicsSettings.cpp`

## In scope

- **`GlobalUniforms.cpp` `PopulateShadowArea`:** the `siShadowRenderFrame`
  counter and its modulo in `bShadowRefreshFrame`, replaced by the phase rule.
- **`LightingUniforms.cpp` `RenderLightingGlobal`:** the
  `siLightingRefreshFrame` counter and its modulo in
  `bScheduledLightingRefresh`.
- **`SmokeUniforms.cpp`:** `PopulatePreviousSmokeAreaSizeInverse`,
  `PublishSmokeContinuity` (plane 0), `PublishSmokeStep`,
  `WriteSmokeDilateDispatch`, and in `RenderSmokeGlobal` everything from the
  `sbPreviousAreaInitialized` block to the end of the function. The
  `f2SmokeCurlOffsetScale` comment is reworded only.
- **`WindUniforms.cpp`:** the closing comment, reworded to say wind reads plane
  0's area pair.
- **`ShaderGlobalLayout.h`:** the five fields replaced by the five arrays in
  Design A5, with their comments.
- **Shaders:**
  - `ShaderFunctions.h`: `SmokeShadow`, `SmokeShadowFaded`, `AddSmoke`;
  - `Smoke/SmokeSpreadCommon.h`: `SmokeSpread`;
  - `Smoke/SmokeSpreadOne.comp`, `Smoke/SmokeSpreadTwo.comp`,
    `Smoke/SmokeOccupancyDilateRemap.comp`: push constants, plane indexing, and
    the wind texcoord;
  - `Smoke/SmokeOccupancyDilate.comp` and `Smoke/SmokeOccupancyDilateRemap.comp`:
    the "at every Smoke Update Cadence" comment wording;
  - `Wind/WindOccupancyDilate.comp`, `Wind/WindSpreadOne.comp`,
    `Wind/WindSpreadTwo.comp`: index 0;
  - `Quads/QuadsVisibleArea.vert`, `Quads/QuadsAxisAlignedVisibleArea.vert`:
    the selector 2 branch;
  - `Terrain/Terrain.frag`, `Water/Water.frag`: the per-plane smoke loop;
    `Model/Model.frag`: the `AddSmoke` call.
- **`PipelineManager.cpp` `CreateSmokeWindPipelines`:** the `flags` of the
  shared smoke dilate `Create` call and of `kSmokeSpreadComputeA`.
- **`CommandBufferRecordGlobal.cpp`/`.h`:** `RecordSmokeSpreadHalf` (parameter
  and dilate record) and `RecordSmokeSpreadPipeline` (pass A push constants).
- **`CommandBufferRecordMain.cpp`:** the two smoke deposit `RecordDrawIndirect`
  push constants in the per-plane smoke deposit loop.
- **UI and settings:**
  - `Ui/SmokeWrappersBase.h`/`.cpp`: delete `gSmokeUpdateCadence`; reword the
    `gSmokeCurlStrengthLow` comment, which names Smoke Update Cadence 4, in
    terms of the step count;
  - `Ui/GraphicsSettingsWrappersBase.h`/`.cpp`: add `gSmokeLayerStagger`;
  - `Ui/Screens/GraphicsMenuScreen.cpp`: the slider line replaced by the toggle;
  - `Ui/Screens/TweaksScreen/TweaksScreenSmoke.cpp`: the registrar entry and
    the `WrapperSlider` row for "Smoke Update Cadence", both deleted; the bool
    gets no Tweaks entry;
  - `Ui/GraphicsSettings.cpp`: the flag enum, struct, `static_assert`s,
    `kiVersion`, save, validation, load, and reset.
- **Documentation:**
  - `Engine/Source/Graphics/Render/AGENTS.md` `## Ordering and Publication`:
    one bullet stating that cadenced chains read `Graphics::miFrameCounter`; the
    shadow phase rule and its gcd reason; and that forced refreshes stay outside
    the schedule;
  - the same file's `## Precision and History` smoke-cadence bullet, rewritten
    for the per-plane stagger, per-plane held areas, and wind reading plane 0;
  - `Engine/Data/Shaders/Smoke/AGENTS.md`: the opening paragraph ("refreshes on
    the same cadence") and `## Frame and Remap Contract` (Smoke Update Cadence
    1, "at every cadence", "higher cadence");
  - `Engine/Source/Ui/AGENTS.md`: the `GraphicsSettings.bin` version bullet,
    now version 18;
  - `Documents/UserInterfaceDesign.txt`: the display-precision line that names
    Smoke Update Cadence, and the Graphics layout sentence whose Smoke group
    ends with its "update-cadence slider", which becomes the Stagger Smoke Layer
    Updates toggle after the area slider, directly above Wind.

## Out of scope

- The lighting and terrain shadow cadence sliders: their ranges, defaults, and
  persistence.
- What any refresh does, the crop-safety test itself, and the lighting and
  shadow forced-refresh triggers.
- Cost weights, phase searches, profiling-derived constants, and any smoke
  phase in the lighting/shadow balance.
- Per-plane wind, wind's own refresh, and wind deposit areas beyond keeping
  them on plane 0.
- Smoke texture extents, tile counts, the shared active-tile list, spread math,
  the dilate radius, and how deposits split between planes.
- `f2SmokeCurlOffsetScale` as a single value (only its comment changes).
- A Tweaks screen entry for `gSmokeLayerStagger`.
- New harness commands, GPU timers, or profile counters.
- `Engine/Source/Graphics/Render/AGENTS.md`'s lighting and terrain shadow
  update-interval bullets, which stay as they are.

## Risk tier and invariants

**Tier 3** (`.agents/references/risk-tiers.md`): the change alters the shared
CPU/GLSL `GlobalLayout` uniform layout and the persisted `GraphicsSettings.bin`
format.

Invariants:

- `Graphics::miFrameCounter` advances exactly once per `RenderFrameGlobal`,
  before any schedule check reads it.
- Lighting and terrain shadows each still refresh once per cadence on their
  schedule, plus forced refreshes. At cadence 1 a chain refreshes every frame
  whatever its phase.
- With the stagger on and no forced refresh, exactly one live plane writes a
  dilate Z count of 1 each frame. With it off, every live plane does.
- Plane k's held area, frames-since-refresh count, published area pair, step
  count, and decay live only in slot k. Plane k's dilate-remap, spreads,
  deposits, and world sampling read slot k. Wind, wind deposits, and both smoke
  spreads' wind lookups read slot 0.
- A skipped plane publishes its held area as both current and previous, so its
  deposits and readers keep using the area its texture was written with.
- `GraphicsSettings` keeps a fixed layout with `static_assert`s, and
  `kiVersion` changes with it.
- Render state stays outside the CRC; no simulation, wire, save, or replay
  surface changes.

## Acceptance criteria

1. `/compile` builds the client and the server in Debug and Release with no new
   warnings, and the shader data regenerates without error.
2. `rg -n "gSmokeUpdateCadence|fSmokeUpdateCadence|Smoke Update Cadence|siLightingRefreshFrame|siShadowRenderFrame|siSmokeRenderFrame" Engine Projects Documents/UserInterfaceDesign.txt`
   prints nothing,
   `rg -n -U "Smoke group[^.]*cadence" Documents/UserInterfaceDesign.txt`
   prints nothing, and
   `rg -n "[lL]ayout\.(f4SmokeArea|f4PreviousSmokeArea|f2PreviousSmokeAreaSizeInverse|fSmokeStepCount|fSmokeDecay)\b" Engine`
   prints nothing.
3. A code reading of `PopulateShadowArea` and `RenderLightingGlobal` confirms
   these (lighting, shadow) cadence pairs:
   - (2,2): shadow phase 1. Lighting refreshes on even frames, shadows on odd.
   - (2,4): shadow phase 1. Lighting refreshes on even frames, shadows on
     frames 3 mod 4.
   - (4,4): shadow phase 1. Lighting refreshes on frames 0 mod 4, shadows on 3
     mod 4.
   - (3,4): shadow phase 0. Both refresh together on frames 0 mod 12, which no
     offset avoids.
4. A Debug client in a scene with smoke on screen and a static camera, driven
   only with existing `/agent-harness` commands, with the toggle switched
   through the Graphics menu with `click`:
   - with the stagger on, repeated `query_profile` samples show the Smoke
     Spread `averageUs` at most half its stagger-off value (one plane of three
     per frame predicts about one third);
   - a code reading of `RenderSmokeGlobal` confirms that, on a frame without a
     forced refresh, only plane `gpGraphics->miFrameCounter % 3` writes a dilate
     Z of 1.
5. With the stagger off, screenshots in the same scene match the pre-change
   build at Smoke Update Cadence 1, apart from run-to-run noise.
6. `GraphicsSettings::kiVersion` is 18, so a settings file written before the
   change fails the version gate in `ReadVersionedFile` and
   `LoadGraphicsSettings` falls back to defaults (diff and code reading).
7. The client log shows no Vulkan validation errors with the stagger on and
   with it off.
