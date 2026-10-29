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
  and UI) is removed; graphics settings are version 20. Gameplay local diffuse is exactly zero and
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
  [LightingQuery.h](../../../Engine/Data/Shaders/Lighting/LightingQuery.h); it has no consumer yet.
- The fixed comparison fixture in
  [LightingFixture.cpp](../../../Engine/Source/Graphics/Lighting/LightingFixture.cpp): seed
  0x51A7, 1000 nested point and rectangle sources, panels, walls, overhang, colored wall, 120
  events with a moving source, occluder and skinned mesh, a cell crossing, and close and wide
  cameras. Generation is deterministic, so the commit identifies it.
- The exhaustive direct-diffuse reference
  ([LightingReference.comp](../../../Engine/Data/Shaders/Lighting/LightingReference.comp)) with 256
  or 512 Halton area samples, and its capture (see `## Decisions`), which writes position,
  shading-normal, geometric-normal, albedo and direct `.rgba32f` raws, `finished.png` and
  `capture.json` at 1920x1080.
- The candidate composition seam:
  [LightingCompose.h](../../../Engine/Data/Shaders/Lighting/LightingCompose.h) on model, terrain,
  water and fixture draws, enabled per framebuffer through
  `TextureDescriptors::SetLightingReference`.
- Controls: the Tweaks Lighting `Comparison` tab, and `query_profile.lighting` as described in
  [the harness command reference](../../../.agents/skills/agent-harness/references/command-reference.md).

## Acceptance table

This stage is complete when every row passes on one commit; the candidates start from that commit.
Row 3's baseline is the unedited tree, recorded in the session worktree's machine-local
`Temp/lighting-strip-baseline/` (replay steps `Temp/lighting-strip-baseline/temp-replay-steps.md`).

Scenario for rows 3-9 (one `/agent-harness` session): Sandbox client and server Debug x64 launched
with each process's `--log-file` and the row-3 app-data setup from `temp-replay-steps.md`
`## After-run steps`; `window_state {"minimized":false}` restores the agent client, then `resize`
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
3. Comparison disabled, client and server paused on the same save: follow `temp-replay-steps.md`
   `## After-run steps` against the stripped build (same save, `ClientGuid.bin`, fresh client app
   data, close shot at client start + 60 s at eye height 156, wide shot at + 75 s at eye height
   665.117), then compare each shot with `baseline-close.png` and `baseline-wide.png` using
   `Compare.ps1` from the same directory. Pass: `over8` is at most 2% of pixels for each shot, and
   side-by-side inspection shows the same islands, water, units, shadows and UI with no added or
   missing local glow. If `over8` exceeds 2%, retake the timed pair once; if it still exceeds 2%,
   the side-by-side inspection alone settles the row and the measured fractions are reported (the
   sun angle advances with client wall time, `temp-replay-steps.md` `## Sun angle caveat`).
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
9. Examine `git diff b56f20fa44d0424c2fdf5003157563684ed747f5 -- Engine/Source/Frame Projects/BrokenEngineSandbox/Source/Frame`
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
- `LightingClosestHit` material lookup for models, terrain and fixture solids is untested.

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

Brute force at 1000 lights is impractical: the direct reference alone evaluates about 108 billion
ray queries per 1920x1080 capture at 256 area samples (800 points plus 200 rectangles of 256
samples per pixel), and one bounce multiplies that by the hemisphere rays per pixel. Stage 2 must
choose a scene size it can capture in practical wall time — source count, sample and ray counts,
resolution — and record that choice in its plan.

Stage 2 is the first consumer of `LightingClosestHit`, so it also verifies the model, terrain and
fixture material lookup that is otherwise untested. DDGI and Split Radiance Cascades start after
stage 2; the other three can start once this stage passes its acceptance table.
