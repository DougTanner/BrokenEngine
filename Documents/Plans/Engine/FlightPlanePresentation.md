<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-09T00:18:59.221Z","dependsOn":["Documents/Plans/Engine/LightingHeightFalloff.md","Documents/Plans/Engine/NavigationPerFlightPlane.md","Documents/Plans/Engine/SmokePerFlightPlane.md","Documents/Plans/Game/FlightPlaneClimbing.md"]} -->
# Present ships on higher flight planes correctly: camera, shadows, lighting samples, audio panning, debug markers

Tier 3 (`.agents/references/risk-tiers.md`): client-only render and audio behaviour, nothing CRC'd, serialized, networked, or threaded changes, but the change spans independently owned subsystems (engine Graphics shaders and uniforms, engine Audio, the engine HexShields collection, the game Camera, and the three game ship render files). Line numbers cite baseline `c1420821`; the prerequisite Plans rename the ship files and replace `gBaseHeight`, so post-Plan names are used throughout and each citation names the current-tree site it stands for. Where a statement here and the code disagree, the code wins; report the contradiction instead of matching one side to the other. Flight-plane series; the term is "flight plane". `Frame/` paths mean `Projects/BrokenEngineSandbox/Source/Frame/`; `Engine/` paths are repository-relative.

## Context

After `Documents/Plans/Game/FlightPlaneClimbing.md` a ship's position Z is a plane height (6, 50, or 100 m) or strictly between the lowest and highest, and Fighters tilt while climbing. Every presentation path still assumes the lowest plane:

- Camera: `PullTarget` returns the tracked ship's full position (`Projects/BrokenEngineSandbox/Source/Graphics/Camera.cpp:50,58`); the engine adds the eye height along +Z to that look-at (`Engine/Source/Graphics/CameraBase.cpp:188-192`, `EngineCamera.cpp` at `c1420821`), so a climb of 94 m would move the look-at and eye up with the ship, zooming the view out and shifting every eye-height-keyed value: chase blend (`:274`), far clip (`:379-382`), shadow and lighting texel references (`:185-186`), water LOD (`:454`), and the audio fade band (`Engine/Source/Audio/StaticVoices.cpp:592-596`). The visible area is the frustum's intersection with Z = 0 (`CameraBase.cpp:388-431`), and every renderer culls against it with the XY of its position; a point above Z = 0 inside that footprint projects no further from the screen centre than its Z = 0 foot under a straight-down eye, so the footprint over-includes and never drops a high ship. The reconciliation visual offset added to the look-at is a 3D position error (`Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.cpp:123-133`).
- Object shadows: models draw into the top-down object-shadow raster through `ModelVertexOutput` mode 2 (`Engine/Data/Shaders/Model/ModelCommon.h:108-117`), whose `ShadowStretchProjection` (`Engine/Data/Shaders/ShaderFunctions.h:35-48`) adds one uniform XY translation `f2ShadowStretchTranslation` = `-(sunrise + sunset cubes) * f4SunMoonNormal.xy` (`Engine/Source/Graphics/Render/GlobalUniforms.cpp:274-284`) plus a position-dependent horizon stretch; the object's Z plays no part. Terrain and water read that raster at their own XY (`Terrain.frag:144`, `Water.frag:150`). Models sample the terrain shadow at their own XY (`Model.frag:261-262`), so a ship 100 m above a shaded valley is drawn in that valley's shadow. The model vertex shaders (`ModelStatic.vert`, `ModelSkinned.vert`) share `ModelCommon.h`; only `Model.frag` and `ModelShadow.frag` pair with them (`Engine/Source/Graphics/Managers/DynamicPipelines.cpp:86,137`). Object shadows apply to sun and moon alike (`Engine/Data/Shaders/Shadow/AGENTS.md`).
- Model lighting: `Model.frag:306-309` reads the lighting textures at the raw fragment XY ("very close to base-height already"), while a ship's own lights deposit where the eye line through the light meets `max(terrain, lowest plane)` (`LightingHeightFalloff` decision 3, `ProjectToBaseHeight`), which lies outward from the look-at by `height * tan(view angle)`; a high ship near the screen edge therefore misses its own deposit. `BaseHeightPosition` (`ShaderFunctions.h:50-56`) is clamped: it is a no-op for a fragment above the plane, so it cannot serve. The specular loop (`:352`) and the smoke blend (`:377`) reuse the same `pf4Lighting`.
- Hex shields deposit lighting per vertex through `HexShield.vert:89-95`, an unclamped eye-line intersection with `globalLayout.fBaseHeight`, never the terrain; `LightingHeightFalloff` dims the deposit by height above `SurfaceHeight` but explicitly left this projection to this Plan. `HexShieldsRender.cpp:61-109` fills `HexShieldLayout` on the main thread and, after `LightingHeightFalloff`, already calls `LightingDepositHeightFalloff(vecLocalPosition, rBasis)` there.
- Water reflections: `ProjectWaterReflection` (`Engine/Data/Shaders/Water/WaterReflectionProjection.h:13-51`) reflects the eye ray about the wave normal and intersects it with `fBaseHeight` to read the one 2D lighting field. That field has no height: a high ship's deposit already sits on the lowest plane at its eye-line projection, so the reflection shows the deposit where the surface shows it. A true mirror image of a 100 m light would sit further from the ship; making that visible would need a per-height lighting field, which nothing else in the series adds.
- Audio: the manual fade and culling measure 3D distance to the camera eye (`StaticVoices.cpp:113,326,701`); X3DAudio pan and Doppler use a listener on the lowest plane facing -Z (`:573-584`) and the emitter's full 3D position (`StaticVoices.cpp:664-679`). A ship 94 m above that plane is "behind" the listener's -Z front and its azimuth collapses toward centre.
- Debug: the post-split home of `RenderNavigation` (today `Frame/Collections/Players/PlayersRender.cpp:136-175`) flattens the waypoint line end, the flagship circle, and the island destination circle to the lowest plane (`:155,165,172`), so a high ship's markers float 94 m below it. The cell-edge and island-boundary debug lines in `Engine/Source/Graphics/Render/MainUniforms.cpp:23,62` are world geometry, not ship markers, and `Documents/Plans/Engine/NavigationPerFlightPlane.md` (prerequisite, its decision 8) draws the nav-polygon debug lines (`:160`) per plane at each plane's height.
- Tuning convention: `Wrapper(value, min, max)` globals in `Engine/Source/Ui/ShadowWrappersBase.cpp/.h` (`gObjectShadowsGrow` at `:28`/`:30`), each registered by label in `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenShadow.cpp` (`"Object Shadow Grow"` at `:33`). Uniform-only products are folded CPU-side (`Engine/Data/Shaders/AGENTS.md`).
- Binding decisions from the user: the camera height stays player-controlled (zoom) and never follows the tracked ship's climb; everything here is client-only render and audio outside the CRC; no backward compatibility.

## Decisions

Each decision is recorded as made; the alternatives are not open.

1. **Camera look-at Z is the lowest plane; the visual error offset loses its Z.** `PullTarget` returns `CameraTarget::Tracked(XMVectorSetZ(vecShipPosition, engine::gpFlightPlanes->Lowest().fHeightMeters), XMVectorSetZ(gpGame->mVecVisualErrorOffset, 0.0f))`. The engine camera is untouched: the eye stays `mfCameraEyeHeight` above a look-at that no longer moves in Z, so a climb changes no eye-height-keyed value (chase blend, far clip, texel references, water LOD, audio fade band) and does not zoom. A ship on the high plane is 94 m nearer the eye and renders proportionally larger; that is the correct perspective and is kept. The extrapolation cache (`mVecLastKnownPlayerPosition`) is fed the flattened position, so a lost-target extrapolation stays on the plane too.
2. **Object shadow offset grows with height above the lowest plane, computed in the shader.** `GlobalLayout` gains `vec2 f2ShadowHeightOffsetPerMeter` = `-f4SunMoonNormal.xy / max(f4SunMoonNormal.z, kfObjectShadowsHeightOffsetMinimumSunZ) * gObjectShadowsHeightOffset`, folded in `PopulateShadowStretch` with `static constexpr float kfObjectShadowsHeightOffsetMinimumSunZ = 0.5f` (caps the offset at about 1.7 m per metre of height near the horizon, where the existing horizon stretch already lengthens shadows). Mode 2 of `ModelVertexOutput` computes `vec2 f2HeightOffset = max(model.f4Position.z - globalLayout.fBaseHeight, 0.0f) * globalLayout.f2ShadowHeightOffsetPerMeter;` and adds it to both the grown vertex position and the object position it passes to `ShadowStretchProjection`, so the stretch term (a function of their difference) is unchanged and the whole shadow translates. Height is measured from the lowest plane rather than the terrain under the ship: no CPU elevation query per model (the Fighter render loop runs on worker threads, `SpaceshipsRender.cpp:156-201`), no `ModelLayout` change, and every model pipeline gets it, including Missiles, whose arcs now cast offset shadows. Over terrain a high ship's shadow lands too far out by `terrain height * offset per metre`; accepted. A ship at or below the lowest plane adds exactly `0 * vec2` = `vec2(0)`, so the low plane is bit-identical.
3. **Terrain shadow on models fades out with height above the lowest plane.** `ModelCommon.h` adds vertex output `layout (location = 8) out float fOutHeightAboveBase;` written as `max(model.f4Position.z - globalLayout.fBaseHeight, 0.0f)` for every rendering mode; `Model.frag` declares the matching input and, directly after `:262`, applies `fShadow = mix(fShadow, 1.0f, clamp(fInHeightAboveBase * globalLayout.fObjectShadowsTerrainShadowFadeInverse, 0.0f, 1.0f));`. `ModelShadow.frag` is unchanged: a vertex output no fragment stage consumes is legal. The model centre, not the vertex, supplies the height so a low-plane hull whose upper vertices sit above 6 m still gets `mix(fShadow, 1, 0)` = `fShadow` exactly. A hard skip at any positive height was rejected because a climbing ship would pop out of shadow.
4. **Two tuning values** in `ShadowWrappersBase.h/.cpp` directly after `gObjectShadowsGrow`, labels in `TweaksScreenShadow.cpp` directly after `"Object Shadow Grow"`: `gObjectShadowsHeightOffset` = `Wrapper(1.0f, 0.0f, 4.0f)`, label `"Object Shadow Height Offset"` (a scale on the geometric offset; 1 is the sun's true projection); `gObjectShadowsTerrainShadowFade` = `Wrapper(30.0f, 1.0f, 200.0f)`, label `"Terrain Shadow Height Fade"` (metres above the lowest plane at which a model receives no terrain shadow; the minimum `1.0f` keeps the folded reciprocal finite, comment it as `gSpreadRingCount` does). `GlobalLayout` gains `float fObjectShadowsTerrainShadowFadeInverse` = `1.0f / gObjectShadowsTerrainShadowFade.mfCurrent`, filled beside `fObjectShadowsGrow` (`GlobalUniforms.cpp:400`).
5. **Model lighting sample projects along the eye line by the model's height.** `Model.frag:306-307` becomes an unclamped eye-line projection that mirrors where the ship's own lights deposit: `vec3 f3ToEye = mainLayout.f4EyePosition.xyz - f3InWorldPosition; vec2 f2LightingPosition = f3InWorldPosition.xy - (fInHeightAboveBase / f3ToEye.z) * f3ToEye.xy;` then `WorldToVisibleArea(vec3(f2LightingPosition, 0.0f), globalLayout.f4LightingArea)`. `f3ToEye.z` is always positive: the eye is at least `kfMinimumEyeHeight` (150 m) above the look-at on the lowest plane, above every plane. The height comes from the decision 3 varying, so the low plane subtracts exactly `0 * vec2` and samples its raw XY as today. The specular loop and the smoke blend keep reading the same `pf4Lighting`, so a high ship's specular and smoke-lit terms follow its deposit as well. Deposits land on `max(terrain, lowest)` while this projects to the lowest plane; over terrain the sample misses the deposit by `(terrain - lowest) * tan(view angle)`, the same error `HexShield.vert` carries today and every surface fragment accepts; kept. The projection stays inline in `Model.frag` rather than a `ShaderFunctions.h` helper: `HexShield.vert` projects to a per-instance surface height by a different input and the two bodies would not be one function.
6. **Hex shield deposit projects onto the surface under the shield.** `HexShieldLayout` gains `float fSurfaceHeight` after `fMinimumIntensity`; `HexShieldsInterpolate::Render` writes `rLayout.fSurfaceHeight = SurfaceHeight(vecLocalPosition, rBasis);` (the `LightingHeightFalloff` helper), and `HexShield.vert:92` reads `pHexShields[i].fSurfaceHeight` in place of `globalLayout.fBaseHeight`. A shield on a higher plane over a peak now deposits where its owner's lights do, so the glow sits under the shield on screen instead of outward of it. Over water or terrain below the lowest plane `SurfaceHeight` equals `fBaseHeight`, so the low plane is bit-identical. The second terrain query per visible shield (beside the one inside `LightingDepositHeightFalloff`) is accepted: shields are few and the loop is already on the main thread.
7. **Water reflections are left as they are** (Context): the one lighting field has no height, so the reflection already shows a high ship's deposit where the water shows it; the missing mirror displacement is the accepted error and no shader changes.
8. **Audio panning drops the emitter's Z to the pan listener's plane.** `ApplyThreeDimensionalVolume` stores `XMVectorSetZ(vecPosition, mX3dAudioListener.Position.z)` into the X3DAudio emitter position; the manual fade, one-shot cull, and priority distances keep the full 3D distance to the camera eye. An emitter on the lowest plane already has that Z, so its mix is unchanged; missile and explosion emitters above the plane now pan by their XY alone, which is the same correction. Listener orientation, velocity, and the height-lerped curves are untouched.
9. **Ship debug markers draw at the ship's own height.** In the post-split `RenderNavigation`: the waypoint line end and its circle (`:155`) and the island destination circle (`:172`) use `XMVectorSetZ(..., XMVectorGetZ(vecPosition))`, the ship's interpolated Z; the flagship destination circle (`:165`) drops its `XMVectorSetZ` and uses the flagship's own interpolated position. The cell and island debug lines in `MainUniforms.cpp` stay on the lowest plane: they outline cells and islands, not ships; the nav-polygon debug lines are left as `NavigationPerFlightPlane` draws them, per plane.
10. **Culling is unchanged.** The Z = 0 footprint over-includes everything above zero under the straight-down eye (Context), so no renderer's `InVisibleArea` test moves.
11. **No format or version changes.** `GlobalLayout` and `HexShieldLayout` are per-frame GPU layouts rebuilt every frame; the shader pack is re-exported by the build; no `Frame::kiVersion`, `kiProtocolVersion`, save, or replay bytes change.

## Design

`Engine/Data/Shaders/ShaderGlobalLayout.h`, directly after `f2ShadowStretchTranslation` (`:193`):

```cpp
vec2 f2ShadowHeightOffsetPerMeter INIT;             // -f4SunMoonNormal.xy / max(z, 0.5) * gObjectShadowsHeightOffset: object-shadow XY shift per metre above fBaseHeight (ModelCommon.h mode 2)
float fObjectShadowsTerrainShadowFadeInverse INIT;  // 1 / gObjectShadowsTerrainShadowFade (Model.frag terrain-shadow fade by height above fBaseHeight)
```

`Engine/Data/Shaders/ShaderLayoutsBase.h` `HexShieldLayout`, after `fMinimumIntensity`:

```cpp
float fSurfaceHeight INIT;  // max(terrain, lowest flight plane) under the shield; HexShield.vert projects the lighting deposit onto it
```

`Engine/Data/Shaders/Model/ModelCommon.h`: `layout (location = 8) out float fOutHeightAboveBase;` beside the other outputs; in `ModelVertexOutput`, `fOutHeightAboveBase = max(model.f4Position.z - globalLayout.fBaseHeight, 0.0f);` beside `f4OutColorAdd`, and mode 2:

```glsl
// Shift the whole footprint along the sun's ground projection by the object's height above the lowest flight
// plane; both positions move so the horizon stretch (a function of their difference) is unchanged.
vec2 f2HeightOffset = fOutHeightAboveBase * globalLayout.f2ShadowHeightOffsetPerMeter;
f3GrownWorldPosition.xy += f2HeightOffset;
gl_Position = ShadowStretchProjection(globalLayout, f3GrownWorldPosition, vec3(model.f4Position.xy + f2HeightOffset, model.f4Position.z));
```

`Engine/Data/Shaders/Model/Model.frag`: `layout (location = 8) in float fInHeightAboveBase;`; after `:262` the decision 3 `mix`; `:305-309` becomes:

```glsl
// Sample lighting where this ship's own lights deposit: the eye line through the ship, continued down to the
// lowest flight plane. Zero height keeps the raw XY, so a ship on that plane samples exactly as before.
vec3 f3ToEye = mainLayout.f4EyePosition.xyz - f3InWorldPosition;
vec2 f2LightingPosition = f3InWorldPosition.xy - (fInHeightAboveBase / f3ToEye.z) * f3ToEye.xy;
vec2 f2LightingTexcoord = WorldToVisibleArea(vec3(f2LightingPosition, 0.0f), globalLayout.f4LightingArea);
```

`Engine/Data/Shaders/Objects/HexShield.vert:90-92`: `float fT = (pHexShields[i].fSurfaceHeight - f3OutPosition.z) / f3ToEye.z;` with the comment naming the surface under the shield.

`Engine/Source/Graphics/Render/GlobalUniforms.cpp`: `PopulateShadowStretch` folds `f2ShadowHeightOffsetPerMeter` after `:284`; the `fObjectShadowsGrow` fill (`:400`) is followed by `rGlobalLayout.fObjectShadowsTerrainShadowFadeInverse = 1.0f / gObjectShadowsTerrainShadowFade.mfCurrent;`.

`Engine/Source/Ui/ShadowWrappersBase.h/.cpp`, `TweaksScreenShadow.cpp`: decision 4.

`Engine/Source/Frame/Collections/HexShields/HexShieldsRender.cpp`: `rLayout.fSurfaceHeight = SurfaceHeight(vecLocalPosition, rBasis);` after the `fLightingIntensity` line (`:103`).

`Engine/Source/Audio/StaticVoices.cpp`: `ApplyThreeDimensionalVolume` (`:666-667`) stores `XMVectorSetZ(vecPosition, mX3dAudioListener.Position.z)`; `:701` keeps `vecPosition`. The listener comment (`:573-576`) says emitters are flattened to the pan plane in `ApplyThreeDimensionalVolume`.

`Projects/BrokenEngineSandbox/Source/Graphics/Camera.cpp` `PullTarget`: decision 1 on the `Tracked` return.

The post-split `RenderNavigation` in the ship render files: decision 9.

## Critical files

- `Engine/Data/Shaders/ShaderGlobalLayout.h`, `ShaderLayoutsBase.h`
- `Engine/Data/Shaders/Model/ModelCommon.h`, `Model.frag`
- `Engine/Data/Shaders/Objects/HexShield.vert`
- `Engine/Source/Graphics/Render/GlobalUniforms.cpp`
- `Engine/Source/Ui/ShadowWrappersBase.h`, `ShadowWrappersBase.cpp`, `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenShadow.cpp`
- `Engine/Source/Frame/Collections/HexShields/HexShieldsRender.cpp`
- `Engine/Source/Audio/StaticVoices.cpp`
- `Projects/BrokenEngineSandbox/Source/Graphics/Camera.cpp`
- The post-split ship render file(s) holding `RenderNavigation` (`Frame/Collections/Omnis/OmnisRender.cpp` and any counterpart `ShipTypeSplit` leaves for Fighters and Battleships)

## In scope

- `Camera::PullTarget`: the `Tracked` return's position Z and visual offset Z (decision 1).
- `ShaderGlobalLayout.h`: the two new members; `ShaderLayoutsBase.h`: `HexShieldLayout::fSurfaceHeight`.
- `ModelCommon.h`: the location-8 output, its write in `ModelVertexOutput`, and the mode 2 offset. `Model.frag`: the location-8 input, the terrain-shadow `mix` after `:262`, and the lighting texcoord at `:305-309`. `HexShield.vert`: the plane height at `:90-92`.
- `GlobalUniforms.cpp`: `PopulateShadowStretch` (the folded offset) and the `fObjectShadowsTerrainShadowFadeInverse` fill.
- `ShadowWrappersBase.h/.cpp` and `TweaksScreenShadow.cpp`: the two wrappers and labels (decision 4).
- `HexShieldsInterpolate::Render`: the `fSurfaceHeight` write.
- `StaticVoices::ApplyThreeDimensionalVolume`: the emitter position Z; the listener comment in `UpdateListenerPosition`.
- `RenderNavigation`'s three marker heights (decision 9).
- The `AGENTS.md` lines `/update-claude-docs` finds stale, at least `Projects/BrokenEngineSandbox/Source/Graphics/AGENTS.md` (`PullTarget` tracks the ship's XY on the lowest plane), `Engine/Source/Audio/AGENTS.md` (emitters flattened to the pan plane), `Engine/Data/Shaders/Model/AGENTS.md` (shadow offset and terrain-shadow fade by height, lighting sample projection), `Engine/Data/Shaders/Objects/AGENTS.md` (deposit projects onto the shield's surface height), `Engine/Source/Frame/Collections/HexShields/AGENTS.md` (the per-element surface height).

## Out of scope

- `engine::CameraBase` (`CameraBase.h/.cpp`): eye height, zoom, LOD, far clip, texel references, visible area, culling (decision 10).
- Water reflections (`WaterReflectionProjection.h`, `Water.frag`) and a per-height lighting field (decision 7); ambient and surface lighting fetches in `Terrain.frag` and `Water.frag`.
- Terrain under the ship for the shadow offset or the terrain-shadow fade (decision 2 and 3 use the lowest plane); object-on-object shadows; the shadow compute chain, `ObjectShadows*` blur, `gObjectShadowsGrow`, and the horizon stretch.
- `LightingHeightFalloff`'s deposit dimming, `SurfaceHeight`'s definition, `ProjectToBaseHeight`, `ToBaseHeight`, `BaseHeightPosition`, and the smoke, particle, wind-trail, and smoke-trail projections (`SmokePerFlightPlane` owns the smoke ones).
- `VisibleLight.frag`, point- and area-light sprites, explosion and hit-flash effects at height.
- Audio fade, cull, priority, Doppler, listener orientation, and the height-lerped curves; a 3D listener.
- `MainUniforms.cpp` debug lines (cell edges, island boundaries, nav polygons); the Fighter climb tilt (`FlightPlaneClimbing` decision 13).
- The HUD, `ScreenToWorld` picking heights, screenshots or captures of a plane, and any new harness command.
- Renaming `fBaseHeight` or any uniform; `Frame::kiVersion`, `kiProtocolVersion`, save, replay, and wire formats.

## Risk triggers and invariants

- Client-only and outside the CRC: every changed file is `BT_CLIENT` or GLSL; no `Frame` member, simulation read, save, replay, or wire byte changes. `PullTarget` reads the interpolate frame only, as today.
- Low-plane invariance, bit-exact: for a ship at or below the lowest plane `fOutHeightAboveBase` is exactly `0.0f`, so the shadow offset is `vec2(0)`, the terrain-shadow `mix` returns `fShadow`, and the lighting texcoord is the raw XY; a shield over water or low terrain has `fSurfaceHeight == fBaseHeight`; an emitter on the plane keeps its Z; a tracked ship on the plane keeps its look-at. Every current screenshot and mix is therefore unchanged apart from missile arcs (decisions 2, 3, 8), which are the same correction.
- Eye-line projections divide by `f3ToEye.z`, which is positive for every plane because the eye is at least `kfMinimumEyeHeight` above the lowest plane and the highest plane is below that (100 < 150 + 6); `HexShield.vert` keeps today's unclamped form. `f2ShadowHeightOffsetPerMeter` is finite by the `0.5f` floor on `f4SunMoonNormal.z`; the fade reciprocal is finite by the wrapper minimum.
- Zero GPU cost beyond one varying and a handful of ALU ops per model vertex and fragment; no new draw, dispatch, texture, or binding. `GlobalLayout` and `HexShieldLayout` keep scalar layout: the new members are 4-byte-aligned and appended, so CPU `sizeof` and GLSL stride agree.
- Thread safety: `SurfaceHeight` runs in the main-thread hex-shield loop where `LightingDepositHeightFalloff` already queries elevation; no model render loop calls it (decision 2), so the worker-dispatched Fighter loop stays query-free. `gpFlightPlanes` is immutable after boot.
- The visual error offset loses only its Z at the camera; `ClientReconciler` and the stored offset are untouched.
- Audio listener roles stay separate: eye distance drives fade, the pan-plane look-at drives pan and Doppler (`Engine/Source/Audio/AGENTS.md`); the pan listener's Z is the one value emitters are flattened to.

## Acceptance

| Criterion | Expected observation | Method |
|---|---|---|
| Builds | Client and server BrokenEngineSandbox build; the shader pack rebuilds `ModelStatic.vert`, `ModelSkinned.vert`, `Model.frag`, `HexShield.vert`, and `HexShieldLighting.frag` without diagnostics | `/compile` client and server |
| Tuning values are live | With the F3 Tweaks overlay open, `describe_ui` lists `Object Shadow Height Offset` at `1` and `Terrain Shadow Height Fade` at `30` | `/agent-harness` client `describe_ui` |
| Camera framing unchanged by a climb | Tracking a fleet Fighter, `set_fleet_flight_plane {type: Fighter, plane: 2}`: screenshots before, mid-climb, and at 100 m show the same water-grid scale and visible-area extent (`query_camera` or the continuity probe reports the same eye height and visible area), the Fighter growing on screen as it climbs; `plane: 0` returns it | `/agent-harness`: launch, `set_fleet_flight_plane`, timed `screenshot`, camera/continuity query |
| Shadow offset grows with height | Two Fighters in one screenshot with the sun away from the zenith, one on plane 0 and one on plane 2: the high one's object shadow lies further from its hull along the sun's ground projection; a mid-climb screenshot shows an intermediate offset | `/agent-harness` `screenshot`; `edit_frame` on a `fighters` row's `pVecPositions` Z for a paused still if the climb is too fast |
| High ship leaves terrain shadow | A Fighter ordered to plane 2 over a terrain-shadowed slope (morning or evening sun) is drawn lit while a plane-0 Fighter in the same shadow is drawn dark | `/agent-harness` `screenshot`, `edit_frame` placement |
| Model lighting follows its deposit | A Fighter on plane 2 near the screen edge, firing: its hull picks up its own blaster light (the lit side faces the deposit), where the baseline build leaves the hull unlit because the deposit lies outward of the raw XY | `/agent-harness` `screenshot` baseline then change, `UpdateShip` fire via `inject_payload` or a random-spawn fight |
| Hex shield glow centred under a high Omni | An Omni ordered to its highest plane (plane 1, 50 m) over an island slope near the screen edge, shields up: the shield's ground glow sits directly under the shield on screen; on the baseline build it is displaced outward | `/agent-harness` `set_fleet_flight_plane {type: Omni, plane: 1}`, `screenshot` baseline then change |
| Debug markers at ship height | With `kbDebugRender`, a navigating Fighter on plane 2 shows its waypoint line and destination circle at its own height (no vertical drop from the hull to the markers in a screenshot near the screen edge) | `/agent-harness` `screenshot` |
| Audio pans by XY | With a Fighter on plane 2 firing left of screen centre, `query_audio` (or the audio harness rig's per-voice matrix) reports a left-weighted output matrix equal to the same ship firing on plane 0 at the same XY | `/agent-harness` audio rig, or code reading of `ApplyThreeDimensionalVolume` if no rig exposes the matrix |
| Low plane unchanged | With every ship on plane 0, screenshots over water and over terrain and the Tweaks-default audio mix match a baseline session at `c1420821` plus the prerequisites, up to the lighting and shadow passes' own temporal noise | `/agent-harness` `verification.md` smoke run, `screenshot`, baseline then change |
| No determinism change | No `LogDifferences CRC` or `CONFIRMED DESYNC` over a multi-minute connected session containing climbs | `get_logs` diff |
