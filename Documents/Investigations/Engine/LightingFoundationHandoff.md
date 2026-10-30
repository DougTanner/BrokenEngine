# Lighting foundation

## Purpose

Five possibly discarded lighting prototypes — [clustered direct
lighting](LightingPrototypeClustered.md), [DDGI](LightingPrototypeDDGI.md), [directional
cache](LightingPrototypeDirectionalCache.md), [ReGIR/ReSTIR](LightingPrototypeReGIRReSTIR.md) and
[Split Radiance Cascades](LightingPrototypeSplitRadianceCascades.md) — each branch from one commit
of the `lighting-foundation` branch, which sits next to main and never lands on it. The foundation
carries only what all five share, so each candidate compares against identical inputs without
rebuilding them.

## What the foundation provides

- The legacy projected local-light system (deposit, spread, combine, history, blur, their settings
  and UI) is removed; graphics settings are version 21. Gameplay local diffuse is exactly zero and
  gameplay light collections publish no physical sources. Sprites, shields, sun/IBL, shadows, smoke
  and water keep their ordinary paths.
- A Vulkan ray-query backend: capability enablement in `DeviceManager`/`InstanceManager` and
  BLAS/TLAS objects in
  [LightingAccelerationStructure.cpp](../../../Engine/Source/Graphics/Lighting/LightingAccelerationStructure.cpp).
- A per-framebuffer traced scene built in the preparation command buffer: drawn terrain placements
  ([Terrain.cpp](../../../Engine/Source/Graphics/Lighting/Terrain.cpp)), rigid and skinned models
  including trace-only off-screen instances
  ([LightingModels.cpp](../../../Engine/Source/Graphics/Lighting/LightingModels.cpp)), and the
  fixture solids and skinned mesh. Terrain preparation GPU time is the `Lighting Terrain` profile
  timer.
- Hit material lookup, `LightingClosestHit` in
  [LightingQuery.h](../../../Engine/Data/Shaders/Lighting/LightingQuery.h); the one-bounce indirect
  reference (`## Stage 2: one-bounce indirect reference`) is its consumer.
- The fixed comparison fixture in
  [LightingFixture.cpp](../../../Engine/Source/Graphics/Lighting/LightingFixture.cpp): seed
  0x51A7, 1000 nested point and rectangle sources, panels, walls, overhang, colored wall, 120
  events with a moving source, occluder and skinned mesh, a cell crossing, and Lowest zoom and
  Default zoom cameras using gameplay minimum/default framing (150/300), default vertical FoV,
  straight-down orientation, lowest-flight-plane look-at and gameplay clip math.
  Capture camera indices 0/1 select those presets; 2 selects the event path between them.
  Generation is deterministic, so the commit identifies it.
- The exhaustive direct-diffuse reference
  ([LightingReference.comp](../../../Engine/Data/Shaders/Lighting/LightingReference.comp)) with 256
  or 512 Halton area samples, and its capture (see `## Decisions`), which writes position,
  shading-normal, geometric-normal, albedo and direct `.rgba32f` raws, `finished.png` and
  `capture.json` at 1920x1080. With the `Indirect` option checked it also traces the one-bounce
  indirect reference and writes `indirect.rgba32f` (`## Stage 2: one-bounce indirect reference`).
- The candidate composition seam:
  [LightingCompose.h](../../../Engine/Data/Shaders/Lighting/LightingCompose.h) on model, terrain,
  water and fixture draws, enabled per framebuffer through
  `TextureDescriptors::SetLightingReference`.
- Controls: the Tweaks Lighting `Comparison` tab, and `query_profile.lighting` as described in
  [the harness command reference](../../../.agents/skills/agent-harness/references/command-reference.md).

## Acceptance table

This stage is complete when every row passes on one commit; the candidates start from that commit.
Row 3's user-approved replacement baseline is exact rebased upstream
`cd6d410c064a5a5f905bf68efaf28493ab6fc33a`, captured in the existing session worktree.
Recipe and preserved inputs: `Temp/lighting-matched-reference/reference-recipe.md`.
Historical `Temp/lighting-strip-baseline/` captures remain unchanged as historical evidence.

The user superseded the excessive wide fixture and its benchmark requirement with gameplay
minimum/default zoom. Current verification is a build and short preset smoke check at 1920x1080,
including the patched save load and readiness. The exhaustive exports, radiometry and former
15-minute test remain incomplete; the historical recipe below does not authorize rerunning them.
Plan03 remains unchecked, and row 3 retains its ordinary-gameplay visual fallback result.

Historical scenario for rows 3-9 (superseded fixture cameras): Sandbox client and server Debug x64 launched
with each process's `--log-file` and fresh app-data setup from the matched-reference recipe;
`window_state {"minimized":false}` restores the agent client, then `resize`
1920x1080 must return `{"width":1920,"height":1080}` (otherwise stop: rows 4-7 cannot run); server
`load` of `lighting-strip-baseline-t400` with `pauseAfterLoad`; after row 3, client pause with `key`
Space when `describe_scene` `gameFlags` lacks it; Tweaks Lighting `Comparison` tab controls; Capture
button; poll `query_profile` until `lighting.capture.status` is `failed`, or `complete` with a
`directory` different from the previous capture's.

| # | Criterion | Pass |
| --- | --- | --- |
| 1 | Builds | DataPacker Release x64 with Local shader data, Sandbox client and server Debug x64 and ModelViewer Debug x64 build through `/compile`. |
| 2 | Diagnostics and legacy gone | Both searches in check 2 return nothing. |
| 3 | Gameplay zero diffuse, ordinary visuals preserved | Each shot's `over8` is at most 2% (or check 3's retake rule applies) and side-by-side inspection shows no added or missing local glow. |
| 4 | Ray visibility on the fixture | Panel shadow exactly zero and moving; at least 10 floor pixels near the skinned mesh differ by more than 1%; `traceOnlyInstances` > 0. |
| 5 | Terrain agrees within the 5 cm offset | Lit island terrain in the wide 1000-source `finished.png` shows no self-shadow speckle. |
| 6 | Radiometry sanity | Three floor pixels within 0.1% of the hand-computed point value. |
| 7 | Reference exports | The wide 1000-source 256-sample capture completes within 15 minutes; every capture writes all seven files. |
| 8 | No Vulkan validation errors | No validation, `CheckVk` failure, device loss or debug break in the client log. |
| 9 | Simulation/CRC unaffected | Merge-aware static examination confirms no simulation or CRC changes; neither log contains desync or CRC failures. |

Checks:

1. `/compile`: DataPacker Release x64 with Local shader data, BrokenEngineSandbox client and server
   Debug x64, ModelViewer Debug x64 all succeed.
2. `rg -n "TEMP START|TEMP END|DESCRIPTOR_CENSUS|DESCRIPTOR_OWNER|MEMORY CREATE_|LightingTerrainValidat|LightingTerrainWitness|LightingTerrainTile|kiLightingTerrainScratch|kPointList|LightingSceneDiagnostic|NativeCapture|ServiceCaptureTransport|DrainLightingCaptureCommands|HoldPublication|uiValidationRequest|GetInputHash" Engine Projects/*/Source`
   and
   `rg -n "LightingSpread|LightCombine|LightingBlur|LightingDeposit|LightingWrappersBase|CurveWidget|HexShieldLighting\.frag" Engine Projects/*/Source`
   return nothing.
3. Comparison disabled, client and server paused on the same patched version256 save and original
   `ClientGuid.bin`: follow `Temp/lighting-matched-reference/reference-recipe.md` against exact
   rebased upstream and foundation. Main GraphicsMenu Lighting must read checked:false, matching
   foundation's zero gameplay local diffuse; preserve ordinary sprites/shields. Use fresh client
   app data independently on each side, FoV15
   perspective, explicitly pinned sun1.8, 1920x1080, matching close/wide projected framing and
   client prediction. Record settings, pose, ticks and subscribed-cell readiness before shots;
   require all subscribed cells confirmed/rendered. Exact historical eye heights and timed sun
   schedule are superseded. Compare each matched shot using the preserved `Compare.ps1`.
   Pass: `over8` is at most 2% for each shot and side-by-side inspection shows the same islands,
   water, units, shadows and UI with no added or missing local glow. If `over8` exceeds 2%, retake
   the matched pair once; if it still exceeds 2%, side-by-side inspection alone settles the row
   and the measured fractions are reported. Preserve historical captures and failed comparisons.
4. Isolated point scenario, close camera, 256 samples, events 0 and 119: in `direct.rgba32f`, floor
   pixels inside the moving panel's geometric shadow are exactly 0, nearby unshadowed floor pixels
   are > 0, and the zero region moves between the two events. Skinned mesh: density 100, close
   camera, events 0 and 119; compare `direct.r` only at floor pixels (`position.z` within 0.01 m of
   0, geometric-normal z > 0.99) within 60 m horizontally of the mesh anchor (-160, 160)
   (`LightingFixture.h:50`). Each such pixel is at least 201 m from source 0 at (-205.3, -97.5) and
   at least 179 m from the moving panel and blocker that follow it within 22 m, beyond the 150 m
   source range, so the mesh animation is the only fixture change that reaches them; at least 10
   such pixels differ by more than 1% between the events. Off-screen: with fixture mode active and
   at least one opaque unit in a rendered cell outside the comparison camera's view (for example
   pin the client with `set_client_grid_coord` to a cell next to [0,0] before enabling the fixture,
   with [0,0]'s units still rendered), `query_profile.lighting.traceOnlyInstances` > 0.
5. Inspect row 7's wide 1000-source `finished.png`: island terrain lit by local sources shows no
   self-shadow speckle (isolated dark pixels or dark streaks inside lit terrain). An island must be
   in view; if none is, `set_client_grid_coord` to a cell with islands before enabling the fixture
   and recapture.
6. Isolated point, close camera, event 0: for 3 unshadowed floor pixels 10-100 m from (-200,-180),
   a PowerShell read of the `position`, `shading-normal` and `direct` raws gives `direct.r` within
   0.1% of `(0.5/pi) * (1000/(4*pi)) * cos(theta) / d^2`, computed in double from the source at
   (-200,-180,6).
7. Density scenario, wide camera, 1000 sources, event 0, 256 samples reaches `complete` within 15
   minutes wall; it and every capture rows 4 and 6 take produce five 33,177,600-byte `.rgba32f`
   files, a 1920x1080 `finished.png` with visible local diffuse, and a `capture.json` carrying
   request, sourceCount, samples, event, camera, scenario, width, height, wallMs and device.
8. Search the client's `--log-file` over the whole scenario (restore, resize, mode switches, all
   captures) for `DebugUtilsCallback`, `CheckVk failed`, `VK_ERROR_DEVICE_LOST` and
   `DEBUG_BREAK at`: no match.
9. Examine `git diff cd6d410c064a5a5f905bf68efaf28493ab6fc33a -- Engine/Source/Frame Projects/BrokenEngineSandbox/Source/Frame`
   against the session baseline, which includes the merged main changes. Only lighting-related
   render, client-only residency and documentation changes are allowed; confirm no changes to
   simulation behavior, shared state or CRC participation. The client's and server's `--log-file`s
   over the scenario contain no `SendDesyncReport`,
   `RecoverFromDesync`, `LogDifferences CRC` or `DEBUG_BREAK at` line.

## Known omissions

- Off-screen terrain is not traced. The terrain producer samples the visible elevation texture, so
  vertices outside the visible area sit at the sea floor and terrain beyond the drawn area does not
  occlude rays. Off-screen models are traced.
- MASK geometry is opaque. BLEND, water, smoke, sprites and shields do not occlude; water still
  receives light.
- Traced terrain matches drawn terrain only within the 0.05 m ray offset.
- `LightingClosestHit` material lookup was checked once in stage 2, by a temporary probe comparing
  the albedo it returns for each receiver's own surface with the raster albedo (Debug client,
  Density 100, Default zoom, event 0, with a battleship spawned into view). It matches
  on terrain (median max-channel difference 0.0057 over 200,965 pixels), on fixture solids (exact
  over 406,358 pixels where the raster shows a fixture surface; the z = 0 water, coplanar with the
  floor, hides about half the floor from the raster receivers) and on the skinned fixture mesh
  (exactly 0.5 over 23,183 pixels). Rigid gameplay models are only weakly checked: the battleship's
  albedo is at most 0.026, so its median difference of 0.0011 over 751 pixels cannot tell a correct
  lookup from a black one; the probe does track that albedo (correlation 0.913, median relative
  difference 0.16 over the 360 pixels with albedo above 0.01). Textured models with bright albedo
  and MASK alpha on hits are not verified.

## Decisions

Taken on 2026-10-29, when the foundation was cut down from its original L1-L14 completion list:

- Terrain accuracy: the ray-origin offset and emitter-segment shortening are one shared 0.05 m
  constant, `kfLightingRayOffset`, used by the reference and every candidate. No exact-equivalence
  proof, raw-tap equality, shifted re-render or witness draw is required, and the off-screen
  terrain tile pass was removed.
- Diagnostics stripped: terrain validation, scene diagnostics, capture manifests, buffer, pack and
  executable hashing, fixture blobs and SHA-256 identities, byte and operation accounting, native
  RenderDoc capture support and all temporary instrumentation. The commit and `capture.json` are
  the reproducibility record.
- Capture runs progressively inside ordinary frames while the client is paused: one frame renders
  the receivers, later frames trace reference tiles, and the last frame composes the reference
  once before export. Final export blocks the main thread while `FinishCapture` waits for the
  framebuffer fence, reads back the images and writes the files. Unpausing or changing the scene
  fails the capture.
- The 512-sample convergence pair is not required: with 4x4 m emitters at least 2 m from every
  receiver plane, 256 Halton samples leave integration error far below the differences the
  comparison judges. The 512 control stays for a candidate that suspects area-sampling noise. The
  100- and 300-source reference exports are not required either; candidates generate their own.
- The ordinary-RAM buffer placement and one-retry out-of-memory recovery in
  [Buffer.cpp](../../../Engine/Source/Graphics/Objects/Buffer.cpp) stay, as the user selected. The
  matched memory-cost experiment stays reverted; its open question is
  [LightingTerrainPreparationMemoryCost.md](LightingTerrainPreparationMemoryCost.md).

## Stage 2: one-bounce indirect reference

DDGI and Split Radiance Cascades compare their indirect lighting against a one-diffuse-bounce
reference; the other three candidates need only the direct reference. Stage 2 adds that reference
to the foundation, so neither candidate builds its own.

The Tweaks Lighting `Comparison` tab's `Indirect` option adds a second tile phase to the capture.
After every direct tile,
[LightingIndirect.comp](../../../Engine/Data/Shaders/Lighting/LightingIndirect.comp) traces 256
cosine-weighted bounce rays per receiver pixel through `LightingClosestHit` and sums, at each hit,
the light from every source, with 16 area samples per rectangle (`kiLightingBounceRays`,
`kiLightingBounceAreaSamples`). Only local sources light a hit; a miss contributes zero. The result
is in the same units as `direct.rgba32f`, so direct + indirect is the one-bounce total. A checked
capture writes it to `indirect.rgba32f`, its `finished.png` shows direct + indirect, and its
`capture.json` records `bounceRays` 256 and `bounceAreaSamples` 16. An unchecked capture writes the
five raws as before and records both as 0. The commit fixes the sampling: the Halton point set
shifted by a per-pixel offset seeded from the pixel index, plus a separate hit-side offset per
bounce. Repeatability across processes was not tested.

Reference scene: Density scenario, 100 sources, Default zoom, event 0, 256 area samples, 256 bounce
rays, 16 hit-side area samples. At 100 sources each receiver pixel costs about 103,000 ray queries
(256 bounces, each one closest hit plus 80 point and 20 x 16 rectangle queries); at 1000 sources it
would cost about ten times as many, so 100 sources was chosen for a capture estimated at about 7
minutes. The Default view contains the red colored wall, so the scene shows colored bounce. A
candidate captures this scene with `Indirect` checked and compares its indirect lighting alone
against `indirect.rgba32f`.

The measured captures below were made this way. The Sandbox client and server ran Debug x64, the
client window at 1920x1080, on one fresh app-data root that held only `ClientGuid.bin` and the save
`lighting-strip-baseline-t400` (2,547 bytes, SHA-256
`42DD5DA00A924D39B95D985ACD09D9F104972BBD0946DBEF2FFCB1154E0AAF55`). Byte-identical copies of both
are tracked in this commit under `Temp/lighting-strip-baseline/AppData/`, in the same per-process
folders. The server `load`ed the save with `pauseAfterLoad` and stayed paused at tick 400, and no
units were spawned. The Tweaks Lighting `Comparison` tab was set to `Fixture reference`,
`Default zoom`, `Density`, 100 sources, 256 samples and event 0, with `Indirect` checked, or
unchecked for the comparison capture, and then `Capture` was clicked.

Measured on the Debug client with an AMD Radeon RX 9070 XT at 1920x1080:

- Wall time: the checked capture took 422,140 ms (direct phase about 166 s at about 780 tiles/s,
  bounce phase about 256 s at about 520 tiles/s); the unchecked capture took 165,802 ms. On the
  foundation before stage 2, direct-only captures at Default/Lowest zoom took 156/154 s (Point),
  160/167 s (Density 100) and 201/185 s (Density 1000). About 142 ms per frame of `Lighting Terrain`
  caps the Debug client at about 7 fps, so each 1080-frame phase takes at least about 155 s; the
  bounce phase is slower than that floor.
- Colored bounce, checked on the floor only (z = 0, facing up, gray 0.5 albedo; positions in
  camera-cell metres). Beside the red wall (x 100..118, y 42..74, 4,373 pixels) the mean indirect is
  r 3.786e-3, g 9.736e-4, b 9.507e-4 (r/g 3.89, r/b 3.98); the mirrored region (x -118..-100, 7,605
  pixels) has r/g 0.9996. Over the red-wall region, the mean `finished.png` red is 160.5 checked and
  159.5 unchecked.
- Per-pixel noise, std((a - b) / sqrt 2) / mean((a + b) / 2) of `indirect.r` over horizontally
  adjacent pixel pairs: 0.41 beside the red wall, 0.64 in the mirrored region. Region means are
  stable; single pixels are noisy. The error from 16 hit-side area samples is not measured
  separately; it is part of this noise.
- Surfaces that see the red wall: the wall faces -x in the plane x 119 (y 46.7..68, z 0.4..9 as
  captured). Three 16 m floor cells beside the wall (x 96..119, y 48..68) have r/g 3.77..4.05; no
  other group of 300 pixels or more apart from the wall reaches r/g 1.3 (smaller groups were not
  reported). Gray vertical walls show no tint, and the up-facing plate at z 100.5 receives zero
  indirect.
- Not attributed: a mild warm tint, r/g 1.05..1.25, over much of the central and southern floor
  away from the wall, including the floor south of the wall (x 80..128, y -80..-32) at r/g
  1.05..1.14. It is not traced to a source; terrain or model bounce is plausible but unproven.

Stage 2 is the first consumer of `LightingClosestHit`; `## Known omissions` records what of its
material lookup was verified. DDGI and Split Radiance Cascades start after stage 2; the other three
can start once this stage passes its acceptance table.
