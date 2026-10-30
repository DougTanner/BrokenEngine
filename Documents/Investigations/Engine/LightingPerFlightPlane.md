# Lighting Per Flight Plane

Smoke keeps one 2D field per flight plane and refreshes the planes on interleaved frames. Dynamic lighting keeps one 2D field on the lowest plane. The question is whether three lighting planes, with interleaved rendering like smoke's, are affordable on the GPU and look better. For now the user keeps lighting as it is. Whether to build per-plane lighting waits on the measurements below, which is why this is an Investigation and not a Plan.

## Lighting today

- Three depositors draw into one deposit render pass: area lights, point lights, and hex shields (`Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp:65-95`). Its three targets, `mpLightingTextures[3]`, are one per color channel, each holding EWNS (`RenderTargetTextures.h:36`); the three are not planes. The pass clears through its load operation every frame (`Engine/Data/Shaders/Lighting/AGENTS.md` `## Pipeline Contracts`).
- Every deposit lands at the lowest plane's height; point and area lights land on the terrain instead where it rises higher.
  Point and area lights project through `ProjectToBaseHeight` (`Engine/Source/Frame/Collections/PointLights/PointLightsRender.cpp:100`, `AreaLights/AreaLightsRender.cpp:129-132`), which is `ProjectToFlightPlane` onto plane 0, or onto the terrain where it rises above plane 0 (`Engine/Source/Graphics/GraphicsUtils.cpp:123-134`). `HexShield.vert:90-95` intersects the eye line with `fBaseHeight`. No depositor weighs the light's height.
- Each frame then runs `gSpreadPassCount` spread passes, combine, temporal, and history copy (`CommandBufferRecordMain.cpp:108-209`). `Model.frag`, `Terrain.frag`, and `Water.frag` read the combine targets once through `ReadLighting` (`Engine/Data/Shaders/ShaderFunctions.h:114-119`), whatever the fragment's height. `AddSmoke` lights every smoke plane from that same sample (`ShaderFunctions.h:262`).
- Ship-borne lights fly and fire on all three planes. Fighters, omnis and battleships climb toward their ordered plane through `ClimbTowardsFlightPlane(..., GetTargetFlightPlane(...), ...)` (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.cpp:255`, `Omnis/Omnis.cpp:330`, `Battleships/Battleships.cpp:250`), up to planes 2, 1 and 0 respectively (`kiFighterHighestFlightPlane`, `kiOmniHighestFlightPlane`, `kiBattleshipHighestFlightPlane`).
  The planes are 6, 50, and 100 m (`Projects/BrokenEngineSandbox/Source/Frame/FlightPlaneTable.h`).

## How smoke renders per flight plane

- Each plane has its own texture pair (`mSmokeTexturesOne`/`mSmokeTexturesTwo`, `RenderTargetTextures.h:29-30`), occupancy buffers, and pipelines. `RecordSmokeSpreadPipeline` loops over the planes under one `kGpuTimerSmokeSpread` (`Engine/Source/Graphics/Managers/CommandBufferRecordGlobal.cpp:307-368`).
- `ComputeSmokePlaneWeights` (`GraphicsUtils.cpp:104-121`) splits a source between the two planes around its height. The source deposits into each plane with that weight, projected to that plane (`Engine/Source/Frame/Collections/Puffs/PuffsRender.cpp:87-104`, `SmokeTrails/SmokeTrailsRender.cpp:101`).
- Interleaving: with `gSmokeLayerStagger` on (the default; the Graphics menu's "Stagger Smoke Layer Updates"), one frame refreshes only plane `miFrameCounter % gpFlightPlanes->Count()`. That plane steps by the frames since its last refresh.
  A skipped plane publishes its held area and zeroes its dilate dispatch, and a plane refreshes early once the visible area leaves its held area (`Engine/Source/Graphics/Render/SmokeUniforms.cpp:147-229`; `Engine/Source/Graphics/Render/AGENTS.md` `## Precision and History`). Deposits keep landing in skipped planes.
- Readers sample every plane, lowest first. `SmokeShadowFaded` fades each plane by the fragment's height above it (`ShaderFunctions.h:245-260`).

Smoke and lighting differ in one way that matters here. Smoke is a persistent simulation, so skipping a frame only delays its advance. Lighting is rebuilt each frame from a cleared deposit, so a skipped lighting plane would show its last refreshed result. That result would trail moving lights by up to `Count() - 1` frames.

## What three lighting planes would mean

These are options, not a decision.

Assigning lights to planes. Each depositor projects with `ProjectToFlightPlane(..., iPlane)` instead of `ProjectToBaseHeight`, and `HexShield.vert` takes the plane's height instead of `fBaseHeight`.
- (1) Split like smoke: use `ComputeSmokePlaneWeights`, so a light between planes deposits into both.
- (2) Nearest plane only: one deposit per light.

What multiplies:
- (a) Per-plane deposit and spread with one combine. The deposit targets and the spread chain triple, and combine sums the three planes. Each plane can have its own spread settings, such as a wider and dimmer spread higher up. Consumers are unchanged. A high light lights the surface differently, but receivers do not see height.
- (b) A full stack per plane: deposit, spread, combine, temporal, history, and the ambient target all triple. Consumers sample every plane and weight it by the fragment's height, as `SmokeShadowFaded` does, and `AddSmoke` can light each smoke plane from its own lighting plane. Receivers see height: an object at 100 m is lit by 100 m lights. This option costs the most memory and the most consumer work, with three times the lighting samples in `Model.frag`, `Terrain.frag`, and `Water.frag`.

Interleaving:
- (i) None: every plane refreshes every frame. This is the upper bound on cost.
- (ii) Stagger like smoke: deposit and spread only the scheduled plane each frame, and keep the other planes' last output and area. Under (a), combine still runs every frame over the kept outputs; under (b), each plane's temporal runs only on its refresh. This needs a held area per plane and an early crop-safety refresh, as smoke has.
- (iii) Stagger only the upper planes and keep plane 0 every frame.

## What to measure

Performance:
- Read the GPU timer rows `kGpuTimerLightingDeposit`, `kGpuTimerLightingSpread`, `kGpuTimerLightingCombine`, `kGpuTimerLightingTemporal`, and the consumers `kGpuTimerObjects`, `kGpuTimerTerrain`, and `kGpuTimerWater` (`Engine/Source/Profile/ProfileManager.h:158-192`) through `query_profile`.
- The `query_profile` caveat (`.agents/skills/agent-harness/references/command-reference.md` `## Durable caveats`) requires both sides in one process lifetime. The experiment build therefore needs a runtime toggle between today's single-plane path and the per-plane path, plus a stagger toggle like `gSmokeLayerStagger`, with cohorts alternated.
- Test two scenes: today's battle content, which has lights on all three planes, and harness-placed lights spread across 6, 50, and 100 m. Measure each zoomed in and fully zoomed out.
- Also record the memory the extra targets add.

Quality, judged from screenshots of the same saved scene with the toggle flipped:
- Does a high light read as higher on water and terrain, by footprint and brightness, than a 6 m light?
- Under (b), are objects on the upper planes lit by their own plane's lights and not by lights far below?
- Under staggering, do fast lights such as blasters and explosions lag or step, and do seams appear while the camera pans or zooms?
- Does smoke lighting still look right?

It becomes a Plan when the user judges the per-plane look worth having and the chosen option's measured GPU cost over today's fits a budget the user sets. The Plan then fixes the assignment rule, the option for what multiplies, and the interleaving mode.

## Fallback

The set-aside height-falloff approach dims each light's deposit by its height above the surface under it. It adds no targets or passes, and it remains the cheaper fallback if per-plane lighting costs too much or adds too little.
