<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-01T21:52:33.175Z","dependsOn":[]} -->
# Object shadow cadence, shadow frame alternation, and blur setting

## Context

The user asked whether the object-shadow textures have an update cadence and
frame alternation, asked that one be added if they do not, and asked for a
Low/High graphics-menu setting for the object-shadow "Blur Multiplier"
(Low 0.5, High 1.0), all filed as one follow-up Plan. The user then added that,
as well as the cadence, the two shadow textures should have a checkbox to
alternate frames. This Plan records that as a graphics-menu checkbox that keeps
the terrain shadow texture and the object shadow texture from refreshing on the
same frame, parallel to the existing "Stagger Smoke Layer Updates" flag.

Answer from the current source: the object-shadow chain has no cadence and no
frame alternation. It runs in full on every frame:

- `CommandBufferRecordMain::Record` records `RecordObjectShadows` and
  `RecordObjectShadowsBlur` unconditionally
  (`Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp:51-52`). The
  Main command buffer is recorded once, not per frame
  (`Engine/Source/Graphics/Managers/AGENTS.md` `### CommandBufferManager`).
- The raster pass draws every `kDynamicModelPipelineModelShadow` pipeline
  indirectly (`CommandBufferRecordMain.cpp:288-299`). Its draw count is
  written by `DynamicPipelines::WriteModelPipelineIndirectBuffers`
  (`Engine/Source/Graphics/Managers/DynamicPipelines.cpp:74-78`), which writes
  the same count to the main model pipeline and the shadow pipeline. The game
  collections' render code and the ModelViewer call it every frame.
- The two blur passes are direct dispatches sized from the texture extent
  (`CommandBufferRecordMain.cpp:301-314`, `RecordCompute` with `TileCount`).
  Their pipelines are created without `PipelineFlags::kIndirectHostVisible`
  (`Engine/Source/Graphics/Managers/WorldLightingShadowPipelines.cpp:190-214`),
  so nothing can skip them at run time.
- The raster projects into the current frame's visible area
  (`ShadowStretchProjection`, `Engine/Data/Shaders/ShaderFunctions.h:35-43`,
  called from `Engine/Data/Shaders/Model/ModelCommon.h:119`). Every consumer
  samples the blurred texture through that same per-frame area,
  `globalLayout.f4VisibleArea` (published at
  `Engine/Source/Graphics/Render/GlobalUniforms.cpp:569`): `Model.frag:262`
  and `:269`, `Terrain.frag:144` and `:157`, `Water.frag:160` and `:233`. No
  held object-shadow area exists, so a frame that skipped the chain would
  leave the texture mapped to the wrong place as the camera moves.

The terrain shadow chain, by contrast, has a cadence through
`gShadowUpdateCadence` (graphics-menu "Terrain Shadow Update Cadence",
`Engine/Source/Ui/Screens/GraphicsMenuScreen.cpp:164`). It refreshes on
`(miFrameCounter + 1) % cadence == 0`, against lighting's phase 0, on reset or
first use, or when the visible area leaves the held area. A skipped frame
publishes the held area and writes zero indirect work
(`GlobalUniforms.cpp:322-358`;
`Engine/Source/Graphics/Render/AGENTS.md` `## Ordering and Publication`).
The smoke planes stagger on `miFrameCounter % planeCount` while
`gSmokeLayerStagger` is on (`Engine/Source/Ui/GraphicsSettingsWrappersBase.cpp:26`,
default on; graphics-menu toggle at `GraphicsMenuScreen.cpp:186`). No
equivalent keeps the terrain and object shadow refreshes apart.

Persisted Boolean settings: all eight `GraphicsSettingsFlags` bits are taken,
the last by `kSmokeLayerStagger` (`Engine/Source/Ui/GraphicsSettings.cpp:14-24`).
Water Foam and Orthographic Projection are instead stored as validated Boolean
bytes (`GraphicsSettings.cpp:47-48`, checked at `:181-188`).

Blur multiplier: `gObjectShadowsBlurMultiplier`
(`Engine/Source/Ui/ShadowWrappersBase.cpp:23`, default 0.5, range
0.125-1.0) scales the blur and blur-intermediate texture size
(`Engine/Source/Graphics/Managers/RenderTargetTextures.cpp:395`). A change
recreates the object-shadow textures through `DestroyFlags::kObjectShadows`
(`Engine/Source/Graphics/Graphics.cpp:609-613`, `:648-650`). The wrapper is
reachable only from Tweaks ("Blur Multiplier",
`Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenShadow.cpp:28`); no
graphics-menu control or persisted level drives it.

Originating gap: a user-directed follow-up; the requesting session's change did
not include it.

## Design

Three parts, one stage: all add persisted fields to `GraphicsSettings.bin` and
rows to the graphics menu, so they share one format-version bump. The frame
alternation also depends on the object-shadow schedule the first part adds.

### Object shadow update cadence

The author recommends mirroring the terrain shadow schedule:

- Add a wrapper `gObjectShadowUpdateCadence` beside `gShadowUpdateCadence`
  with the same integer shape (step 1, range 1-4). The author recommends
  default 2, so the chain alternates frames by default. A ship moving relative
  to the ground shows its shadow up to `cadence - 1` frames behind; the live
  check below judges whether 2 is acceptable, and the user can choose
  default 1 at plan review.
- Decide the refresh in Global next to the terrain decision
  (`GlobalUniforms.cpp`), at phase 2:
  `(miFrameCounter + 2) % gObjectShadowUpdateCadence == 0`. Phase 2 avoids
  lighting (phase 0) and terrain shadow (phase 1) whenever the cadences allow;
  `### Shadow texture frame alternation` covers the terrain pairs it cannot
  avoid. Forced refreshes stay outside the schedule: first use, object-shadow
  texture recreation (new texture contents are undefined), a camera basis reset
  (a multi-cell jump), and crop safety.
- Hold one object-shadow area, set to `mf4RenderVisibleArea` on each refresh.
  Express it in the camera cell's frame and shift it with a
  `RetainedAreaBasis` on a camera cell change, as the other retained areas do
  (`Engine/Source/Graphics/Render/AGENTS.md` `## Precision and History`). It
  has no previous area (the chain has no temporal pass), so it needs no
  `TemporalAreaLatch`. Crop safety refreshes once the live visible area leaves
  the held area (`IsVisibleAreaInsideHeldArea`, `Render.h:80`), with no
  margin. A panning camera therefore refreshes every frame, and the saving
  applies while the view is still or zooming in. The author recommends this
  over adding headroom to the object-shadow area, which would trade texel
  density for fewer forced refreshes.
- Publish the held area as a new `GlobalLayout` field (`f4ObjectShadowsArea`)
  in `Engine/Data/Shaders/ShaderGlobalLayout.h`. `ShadowStretchProjection` and
  every consumer sample listed in `## Context` map through it instead of
  `f4VisibleArea`. Nothing else in those shaders changes mapping, including
  the shadow-mode elevation lookup at `ModelCommon.h:120`.
- A skipped frame draws and dispatches nothing for the chain:
  - Give both object-shadow blur pipelines `kIndirectHostVisible`, record them
    with `RecordComputeIndirect`, and write their group counts every frame
    from Global with Z = 0 on a skip, as the terrain chain does.
  - Zero the shadow pipeline's draw count on a skip inside
    `DynamicPipelines::WriteModelPipelineIndirectBuffers`, so its game and
    ModelViewer callers stay unchanged; the main model draw keeps its count.
  - The raster render pass keeps its load-op clear; only the blur passes read
    the raster.
- Add the held area to `gPresentationContinuity` (`Render.h:146`), as the
  `## Precision and History` rule requires for a new retained value.

### Shadow texture frame alternation

- Add a Boolean wrapper beside `gSmokeLayerStagger`
  (`GraphicsSettingsWrappersBase.cpp` and `.h`); the author recommends the name
  `gShadowUpdateAlternation` and default on, like the smoke stagger.
- Phases alone cannot keep the two chains apart. With terrain cadence T and
  object cadence O, terrain refreshes on schedule when
  `miFrameCounter` is congruent to -1 modulo T and objects when it is
  congruent to -2 modulo O. Both hold on one frame exactly when T and O share
  no factor above 1: never for pairs drawn from {2, 4} or for 3 with 3, but
  periodically when either cadence is 1 or a 3 meets a 2 or 4. Forced
  refreshes ignore the schedule. So the checkbox chooses by a yield rule, not
  by moving phases; the terrain phase 1 and object phase 2 stay as they are.
- The rule, the author's recommendation: with the checkbox on, once both
  chains' refresh reasons for the frame are known on the Global path, a frame
  on which both would refresh runs only one of them, unless both reasons are
  forced:
  - a forced refresh wins over a scheduled one;
  - between two scheduled refreshes, terrain goes first;
  - the chain that yields carries its refresh to the next frame (one
    file-static carried flag per chain), where the carried refresh ranks as
    forced, so a yield delays a chain by exactly one frame and never starves
    it;
  - two forced refreshes on one frame both run: a skip would leave that chain
    sampling outside its held area or an undefined texture.
- Consequences to accept at plan review: with both cadences 1, the chains take
  turns, each effectively at cadence 2. At the defaults (terrain 4, object 2)
  the schedules never meet, so the checkbox parts only a scheduled refresh
  from the other chain's forced one. While the camera pans, the object chain
  crop-refreshes every frame, so a carried terrain refresh shares the next
  frame with it.
- A yielding terrain frame takes today's skipped-frame path
  (`GlobalUniforms.cpp:332-354`): held area pair and blend republished, zero
  indirect work. A yielding object frame takes the skipped-frame path the
  cadence part adds. This is the only change to the terrain shadow schedule,
  and only while the checkbox is on.
- With the checkbox off, each chain follows its own schedule and forced
  refreshes independently, sharing frames wherever the arithmetic above
  allows.
- Where the two decisions meet in `GlobalUniforms.cpp` (the terrain decision
  is inside `PopulateShadowArea`, `:292`) is the implementer's choice; both
  stay on the Global path, before `RenderFrameMain`.
- Graphics menu: a `WrapperToggle` ending the new object-shadow block. The
  author recommends the label "Alternate Shadow Texture Updates", after the
  user's "alternate frames" and the "Stagger Smoke Layer Updates" pattern; the
  label is an open naming choice for plan review.

### Blur multiplier setting

- Add a two-level wrapper `gObjectShadowsBlurLevel` (values 0 and 1, default
  0 = Low) in `Engine/Source/Ui/GraphicsQualityWrappersBase.cpp`, with
  `ApplyObjectShadowsBlurLevel()` writing `gObjectShadowsBlurMultiplier` through
  `Set`: Low 0.5, High 1.0. Call it from `ApplyAllGraphicsQualityLevels` and
  from the menu row's change, as the existing levels are applied
  (`Engine/Source/Ui/AGENTS.md` `## Graphics Quality Levels`). Low equals
  today's 0.5 default, so default visuals do not change.
- Graphics menu (`GraphicsMenuScreen.cpp`): the author recommends a new
  object-shadow block after the Terrain Shadows block. It holds a `RadioRow`
  with Low/High, a `ColumnSlider` "Object Shadow Update Cadence", and the
  alternation toggle above. The row label is an open naming choice for plan
  review: the user's label is "Blur Multiplier"; the author recommends
  "Object Shadow Blur Multiplier" because the menu also has terrain-shadow
  rows. This Plan writes the recommended label below.
- Add "Object Shadow Update Cadence" to the Tweaks object-shadow section
  (`TweaksScreenShadow.cpp`), next to its terrain counterpart
  "Shadow Update Cadence". "Blur Multiplier" stays in Tweaks as the derived
  wrapper's direct control, like "Render Multiplier".

### Persistence

- `GraphicsSettings` (`Engine/Source/Ui/GraphicsSettings.cpp:26-73`) gains a
  `uint8_t` blur level (the author recommends taking the
  `puiProjectionPadding` byte at offset 43), a `float` object-shadow cadence,
  and a `uint8_t` Boolean byte for the alternation checkbox, since no flag bit
  is free. The author recommends appending the float and then the byte after
  `fFieldOfView`, with explicit trailing padding. Bump `kiVersion` from 21 to
  22 and update the offset and `sizeof` static_asserts, so version 21 files
  fail the current-format gate and load defaults (root `AGENTS.md`: no
  backward compatibility).
- Validate all three fields before any is applied
  (`FindInvalidGraphicsSetting`): the cadence through `IsInRange`, like
  `fShadowUpdateCadence`. The blur level must be 0 or 1;
  `IsValidGraphicsQualityLevel` (`GraphicsSettings.cpp:113`) accepts the
  three-level range, so it needs its own two-level check. The alternation byte
  must be 0 or 1, like `uiWaterFoamEnabled`. Load applies all three;
  `ResetGraphicsSettings` restores their defaults.

Change Workflow tier: Tier 3. Triggers: serialization and data layout. The
change alters the `GraphicsSettings.bin` layout and version, and the
CPU/GPU-shared `GlobalLayout` uniform layout
(`.agents/references/risk-tiers.md` `### Risk tiers`).

Exposure:

- Determinism/CRC, wire, replay, grid save, `.pack` format version: none. The
  object-shadow chain, `GlobalLayout`, and `GraphicsSettings.bin` are client
  presentation state outside the PostRender CRC.
- Shader: the `GlobalLayout` change and the consumer edits need DataPacker to
  regenerate the SPIR-V with the client build.
- Serialization: `GraphicsSettings.bin` version 22; older files fall back to
  defaults.
- Affinity: all edits are client-only (`BT_CLIENT`) apart from the shared
  shader header, whose layout only client code writes.
- Threading: the refresh decision is made on the Global path, which runs before
  `RenderFrameMain`. Main reads it only to zero the shadow draw count and never
  writes `GlobalLayout` (`Engine/Source/Graphics/Render/AGENTS.md`
  `## Ordering and Publication`). Indirect slots are written per command
  buffer, every frame, because pipeline recreation zeroes them.
- Build: BrokenEngineSandbox client and ModelViewer (both reach
  `WriteModelPipelineIndirectBuffers`).

## Critical files

- `Engine/Source/Graphics/Render/GlobalUniforms.cpp`
- `Engine/Source/Graphics/Render/Render.h`
- `Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp`
- `Engine/Source/Graphics/Managers/DynamicPipelines.cpp`
- `Engine/Source/Graphics/Managers/WorldLightingShadowPipelines.cpp`
- `Engine/Data/Shaders/ShaderGlobalLayout.h`
- `Engine/Data/Shaders/ShaderFunctions.h`
- `Engine/Data/Shaders/Model/Model.frag`
- `Engine/Data/Shaders/Terrain/Terrain.frag`
- `Engine/Data/Shaders/Water/Water.frag`
- `Engine/Source/Ui/ShadowWrappersBase.cpp` and `.h`
- `Engine/Source/Ui/GraphicsQualityWrappersBase.cpp` and `.h`
- `Engine/Source/Ui/GraphicsSettingsWrappersBase.cpp` and `.h`
- `Engine/Source/Ui/GraphicsSettings.cpp`
- `Engine/Source/Ui/Screens/GraphicsMenuScreen.cpp`
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenShadow.cpp`

## In scope

- The object-shadow refresh decision, held area, and indirect count writes in
  `GlobalUniforms.cpp` (next to the terrain shadow refresh), plus a
  `Render.h` global carrying the decision to Main and the
  `PresentationContinuitySnapshot` entry.
- The frame-alternation yield between the terrain refresh decision in
  `PopulateShadowArea` and the object-shadow decision, with its two carried
  flags, in `GlobalUniforms.cpp`.
- `CommandBufferRecordMain::RecordObjectShadowsBlur` (indirect blur
  dispatches) and the `kPipelineObjectShadowsBlurH`/`V` creation flags.
- `DynamicPipelines::WriteModelPipelineIndirectBuffers`: the shadow count on a
  skipped frame.
- `GlobalLayout::f4ObjectShadowsArea`; `ShadowStretchProjection`; the
  object-shadow sample coordinates at `Model.frag:269`, `Terrain.frag:144` and
  `:157`, and `Water.frag:160` and `:233`.
- New wrappers `gObjectShadowUpdateCadence`, `gObjectShadowsBlurLevel`, and
  the alternation Boolean, `ApplyObjectShadowsBlurLevel`, and its call in
  `ApplyAllGraphicsQualityLevels`.
- `GraphicsSettings` layout, version, save, validation, load, and reset for the
  three new fields.
- The new graphics-menu rows, including the alternation toggle, and the Tweaks
  cadence entry.
- The documentation the change makes stale: `Engine/Data/Shaders/Shadow/AGENTS.md`
  (object chain), `Engine/Source/Graphics/Render/AGENTS.md`
  `## Ordering and Publication` (schedule phases and the alternation yield),
  and `Engine/Source/Ui/AGENTS.md` `## Settings Persistence` (version 22
  fields).

## Out of scope

- Lighting and smoke schedules and their phases; the terrain shadow cadence
  and phase, which change only through the alternation yield while the
  checkbox is on.
- Alternation with lighting or smoke, and any Tweaks entry for the
  alternation checkbox.
- Object-shadow blur kernels, sigmas, radii, channel layout, and the
  shadow-mode elevation lookup.
- `gObjectShadowsLevel` (persisted Low/Medium/High render multiplier with no
  graphics-menu row) and any change to `gObjectShadowsBlurMultiplier`'s range
  or default.
- Headroom around the object-shadow area, and temporal smoothing of object
  shadows.
- Game collection render code (`Projects/`).

## Acceptance criteria

- The graphics menu shows "Object Shadow Blur Multiplier" (Low/High),
  "Object Shadow Update Cadence", and "Alternate Shadow Texture Updates"
  (default on). Selecting Low sets `gObjectShadowsBlurMultiplier` to 0.5 and
  High to 1.0. The `ObjectShadowsBlur` texture extent follows the selection.
- All three settings survive Back and a client restart. A version 21
  `GraphicsSettings.bin` is rejected, and every setting loads its default. An
  out-of-range cadence, a blur level above 1, or an alternation byte above 1
  rejects the file.
- With a still camera, the checkbox off, and cadence N, the object-shadow
  raster and both blur passes do GPU work only on frames where
  `(miFrameCounter + 2) % N == 0`. The `kGpuTimerObjectShadows` and
  `kGpuTimerObjectShadowsBlur` timers read near zero on the other frames. At
  cadence 1 every frame refreshes.
- With a still camera, the checkbox on, and both shadow cadences 1, the
  terrain chain (`kGpuTimerShadow`) and the object chain do GPU work on
  alternating frames, never on the same frame. With the checkbox off, both
  refresh every frame.
- A forced refresh happens on first use, after changing the blur level (texture
  recreation), and on every frame the visible area leaves the held area.
- While the camera pans and zooms, ship shadows on terrain, water, and lower
  ships stay registered under their ships, with no sliding or blank texture.
  A `/agent-harness` screenshot of the scene and of the `ObjectShadowsBlur`
  texture shows this; the supplied evidence records no reproduction setup.
- Cadence 1 with blur Low looks the same as before the change.
- Client and ModelViewer build; a replay determinism check stays clean.

## Notes

- `Engine/Source/Ui/ShadowWrappersBase.cpp` had uncommitted edits in the primary
  checkout when this Plan was written (other object-shadow defaults, not the
  blur multiplier). Read that file's current defaults at claim time.
- `Documents/Plans/Engine/RemoveUnreadSmokeGlobalLayoutFields.md` also edits
  `ShaderGlobalLayout.h` and `GlobalUniforms.cpp`; they are independent, and
  whichever lands second rebases.
