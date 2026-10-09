<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-09T00:01:51.106Z","dependsOn":["Documents/Plans/Engine/FlightPlaneData.md","Documents/Plans/Engine/SmokePerFlightPlane.md"]} -->
# Dim each light's 2D lighting deposit by its height above the surface under it

Tier 2: client-only render behaviour of the lighting deposit stage (three CPU depositor sites plus one sprite shader); nothing CRC'd, serialized, networked, or threaded changes. Line numbers cite baseline `c1420821`; after `FlightPlaneData` lands, the `gBaseHeight.mfCurrent` read at `GraphicsUtils.cpp:109` is already `engine::gpFlightPlanes->Lowest().fHeightMeters` and the `fBaseHeight` uniform names are kept (its decision 8); `SmokePerFlightPlane` then generalizes that projection per plane (Context). Where a statement here and the code disagree, the code wins; report the contradiction instead of matching one side to the other. Flight-plane series.

## Context

Lighting spread is 2D: every dynamic light deposits into the three RGBA16F EWNS targets at the point where its eye line meets the surface under it, then `LightingSpread.frag` spreads that texture (`Engine/Data/Shaders/Lighting/AGENTS.md`). A light keeps its full 3D position (`PointLights.h:52`, `AreaLights` corner positions), but its height above the ground plays no part in what it deposits, so a light on the high flight plane (100 m) would light the terrain and the ships on the low plane exactly as a light 6 m above them. The user chose the cheap option: dim each light at deposit time, on the CPU, by its height above the surface under it, with zero GPU cost, accepting that a ship on the same high plane as the light is dimmed too and that per-pixel 3D lighting of ship decks is deferred.

- Projection, as `Documents/Plans/Engine/SmokePerFlightPlane.md` (prerequisite, its decision 7) leaves it: `ProjectToFlightPlane(vecLocalPosition, rBasis, iPlane)` (`Engine/Source/Graphics/GraphicsUtils.cpp`, today's `ProjectToBaseHeight` body at `:104-110`) queries `gpIslandTerrain->GlobalElevation` in the light's own cell and intersects the eye line with the plane `max(elevation, Plane(iPlane).fHeightMeters)` through `common::ToBaseHeight` (`Common/Math/MathUtils.cpp:6-11`), and `ProjectToBaseHeight` is `ProjectToFlightPlane(.., 0)`. `ProjectToBaseHeight` callers: `PointLightsRender.cpp:90`, `AreaLightsRender.cpp:129-132`, plus the non-light depositors `WindRadialsRender.cpp:79`, `WindTrailsRender.cpp:100-101`; the smoke depositors (`PuffsRender.cpp:73`, `SmokeTrailsRender.cpp:82-83`) call `ProjectToFlightPlane` per plane.
- Deposit intensity: point lights write `f4Parameters.y = fLightingIntensity` (`PointLightsRender.cpp:94-98`), area lights `f4Parameters.y = fLightingIntensity * fIntensityMultiplier` (`AreaLightsRender.cpp:144-150`); both reach the fragment shaders as `f4InParams.y` (`PointLight.frag:86`, `AreaLight.frag` deposit path). Hex shields write `rLayout.fLightingIntensity` (`HexShieldsRender.cpp:103`) and project per vertex in `HexShield.vert:89-95` onto the lowest plane, not the terrain.
- Deposit values are Reinhard-compressed at spread pass 0 (`LightingSpread.frag:39-51,144-149`, `gLightingDepositCompress` 4.0), so a multiplier applied before the deposit is not the same as the same multiplier at the receiver; accepted.
- `VisibleLight.frag:34-41` fades a light's sprite as the terrain under it rises above `fBaseHeight` over a fixed `0.25 * fBaseHeight` (1.5 m) band. A light 100 m up over a 60 m peak is therefore invisible today, and so is a missile exhaust light arcing over a hill (missiles climb: `kfMissilePitchMinimum` 0.75 rad, `Missiles.h:29-30`).
- `LightingSpread.frag:77-82` reduces spread distance and decay on receiving terrain above the lowest plane by at most `gSpreadHeightMultiplier` (0.125): a 12.5 % reduction, independent of any light's height. The research note that it "would kill a high light's spread over high terrain" is contradicted by the code; the code is trusted and this fade is left alone.
- Tuning convention: `Wrapper(value, min, max)` globals in `Engine/Source/Ui/LightingWrappersBase.cpp/.h`, grouped by stage (`// Write - Deposit` at `:11-14`), each registered by label in `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenLighting.cpp` (`"Deposit Compress"` at `:19`).
- Today nothing places a ship above the low plane, but blasters are not re-snapped to it (`BlastersUpdate.cpp:160-170` integrates the previous position), and a blaster carries a point or area light synced every tick (`BlastersUpdate.cpp:116-142`, `:174`). `edit_frame` writes a blaster row's `pVecPositions` as `[x,y,z]` (`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`), which is the acceptance rig for a light on a higher plane.

## Decisions

Each decision is recorded as made; the alternatives are not open.

1. **Falloff curve:** linear. `fHeight = z_light - SurfaceHeight`, `multiplier = clamp(1 - fHeight / gLightingDepositHeightFalloffDistance, 0, 1)`. Every light at or below its surface (all low-plane ships at 6 m over water or low terrain, crater lights on the terrain, lights under a peak) gets exactly `1.0f`, so today's content changes only for lights already above the plane (missile exhausts in their arc). One tuning value, no start offset, no exponential: the simplest curve that reaches zero and is readable on a slider.
2. **Tuning value:** `engine::gLightingDepositHeightFalloffDistance` = `Wrapper(150.0f, 1.0f, 400.0f)`, declared in `LightingWrappersBase.h` and defined in `LightingWrappersBase.cpp` directly after `gLightingDepositCompress` in `// Write - Deposit`; Tweaks label `"Deposit Height Falloff Distance"` directly after `"Deposit Compress"`. At the default the three planes deposit 1.0, 0.71, and 0.37 over water and a high-plane light over a 60 m peak deposits 0.73. The minimum of 1.0f is load-bearing: it keeps the divide finite (comment it as `gSpreadRingCount` does).
3. **Surface height:** `SurfaceHeight(vecLocalPosition, rBasis) = std::max(gpIslandTerrain->GlobalElevation(rBasis.coordinate, vecLocalPosition), gpFlightPlanes->Lowest().fHeightMeters)`, the plane-0 surface extracted from `ProjectToFlightPlane`'s clamp. `ProjectToFlightPlane` now projects onto `std::max(SurfaceHeight(vecLocalPosition, rBasis), Plane(iPlane).fHeightMeters)`, which equals its `max(elevation, Plane(iPlane).fHeightMeters)` exactly because plane heights ascend, so projection and falloff read one definition and `ProjectToBaseHeight` stays `ProjectToFlightPlane(.., 0)`.
4. **Where the multiplier applies:** the three depositors of the lighting deposit stage, each after its own cull so a culled light pays no elevation query: point lights at their position, area lights at the quad centre (`vecCenter`, one query per light, not four), hex shields at the shield position. Hex shields take the falloff because they are a depositor into the same targets and a shield on a higher plane would otherwise light the ground at full strength; their per-vertex `HexShield.vert` projection onto the lowest plane is left as is (see Out of scope).
5. **Lighting area is not widened with height.** A wider, dimmer pool would cost deposit fill, and spread already widens the pool; nothing in the request asks for it.
6. **`VisibleLight.frag` sprite fade re-bases to the light itself:** the comparison height becomes `max(f3InPosition.z, globalLayout.fBaseHeight)`; the 1.5 m band stays `0.25f * globalLayout.fBaseHeight`. A sprite at or below the lowest plane behaves exactly as today; a sprite above it fades only when the terrain under it rises above the light. This also makes crater sprites on terrain above 7.5 m and missile exhaust sprites over hills visible, which today's base-height comparison hides; both are the same correction, not separate features.
7. **No new uniform, shader layout, or GPU work.** The multiplier travels in the existing `f4Parameters.y` and `fLightingIntensity` fields.

## Design

`Engine/Source/Graphics/GraphicsUtils.h`, next to `ProjectToFlightPlane`:

```cpp
// The surface a light over vecLocalPosition deposits onto: the terrain there or the lowest flight plane, whichever is higher.
float SurfaceHeight(XMVECTOR vecLocalPosition, const RenderBasis& rBasis);
// Deposit multiplier for a light's height above SurfaceHeight: 1 at or below it, falling linearly to 0 at
// gLightingDepositHeightFalloffDistance above it. Lighting spread is 2D, so this is the only height cue a deposit carries.
float LightingDepositHeightFalloff(XMVECTOR vecLocalPosition, const RenderBasis& rBasis);
```

`GraphicsUtils.cpp`: `ProjectToFlightPlane` becomes `return common::ToBaseHeight(Rebase(rBasis, vecLocalPosition), engine::gpCamera->mVecEyePosition, std::max(SurfaceHeight(vecLocalPosition, rBasis), engine::gpFlightPlanes->Plane(iPlane).fHeightMeters));`; `ProjectToBaseHeight` is unchanged. `LightingDepositHeightFalloff` is `std::clamp(1.0f - (XMVectorGetZ(vecLocalPosition) - SurfaceHeight(vecLocalPosition, rBasis)) / gLightingDepositHeightFalloffDistance.mfCurrent, 0.0f, 1.0f)`; the file gains `#include "Ui/LightingWrappersBase.h"`.

Depositor sites:

- `PointLightsInterpolate::Render` (`PointLightsRender.cpp:96`): `f4Parameters.y = fLightingIntensity * LightingDepositHeightFalloff(vecLocalPosition, rBasis);`
- `AreaLightsInterpolate::Render` (`AreaLightsRender.cpp:146`): after the AABB cull, `float fHeightFalloff = LightingDepositHeightFalloff(vecCenter, rBasis);` then `f4Parameters.y = fLightingIntensity * fIntensityMultiplier * fHeightFalloff;`
- `HexShieldsInterpolate::Render` (`HexShieldsRender.cpp:103`): `rLayout.fLightingIntensity = rCurrent.pfLightingIntensities[i] * LightingDepositHeightFalloff(vecLocalPosition, rBasis);`

`Engine/Data/Shaders/Lighting/VisibleLight.frag:35-41`:

```glsl
// Fade only when the terrain under the sprite rises above the light itself; a sprite at or below the lowest flight
// plane fades against the plane, as before.
const float fLightHeight = max(f3InPosition.z, globalLayout.fBaseHeight);
const float fFalloff = 0.25f * globalLayout.fBaseHeight;
float fHeightPercent = 1.0f;
if (fTerrainElevation > fLightHeight)
{
	fHeightPercent = 1.0f - clamp((fTerrainElevation - fLightHeight) / fFalloff, 0.0f, 1.0f);
}
```

Documentation: the deposit contract line in `Engine/Data/Shaders/Lighting/AGENTS.md` `## Pipeline Contracts` gains the height dimming; the `PointLights/AGENTS.md` culling invariant ("The deposit projects to terrain or ocean base height") and the `AreaLights/AGENTS.md` orientation bullet state that the deposit intensity is dimmed by the light's height above that surface; `/update-claude-docs` finds any other stale line.

## Critical files

- `Engine/Source/Graphics/GraphicsUtils.h`, `GraphicsUtils.cpp`
- `Engine/Source/Frame/Collections/PointLights/PointLightsRender.cpp`
- `Engine/Source/Frame/Collections/AreaLights/AreaLightsRender.cpp`
- `Engine/Source/Frame/Collections/HexShields/HexShieldsRender.cpp`
- `Engine/Source/Ui/LightingWrappersBase.h`, `LightingWrappersBase.cpp`
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenLighting.cpp`
- `Engine/Data/Shaders/Lighting/VisibleLight.frag`

## In scope

- `SurfaceHeight` and `LightingDepositHeightFalloff` (new) in `GraphicsUtils.h/.cpp`; `ProjectToFlightPlane`'s projection-plane expression (decision 3); the include the new function needs.
- `gLightingDepositHeightFalloffDistance` (new) in `LightingWrappersBase.h/.cpp` and its `TweaksScreenLighting.cpp` entry (decision 2).
- The `f4Parameters.y` assignment in `PointLightsInterpolate::Render`, the `f4Parameters.y` assignment and the one added local in `AreaLightsInterpolate::Render`, and the `rLayout.fLightingIntensity` assignment in `HexShieldsInterpolate::Render` (decision 4).
- `VisibleLight.frag` `main`: the height comparison at `:35-41` (decision 6).
- The `AGENTS.md` lines named under Design that the change makes stale.

## Out of scope

- Per-receiver-height falloff textures, per-pixel 3D lighting of ship decks, any new uniform, shader layout member, render target, or GPU pass.
- `HexShield.vert:89-95`: its per-vertex projection onto `fBaseHeight` rather than the terrain, so a shield on a higher plane over a peak deposits where the eye line meets the lowest plane; a plane-aware ship Plan owns it.
- `LightingSpread.frag:77-82` (the receiver-terrain height fade) and its three `gSpreadHeight*` wrappers; the deposit and output compressors; `gSpreadDistanceEnd`'s camera-height lerp (`LightingWrappersBase.cpp:41-47`).
- Widening `fLightingArea` or `fLightingSize` with height (decision 5).
- `ProjectToFlightPlane`'s and `ProjectToBaseHeight`'s signatures, `ProjectToBaseHeight`'s body, and their non-light callers (puffs, smoke trails, wind radials, wind trails): they project but do not deposit light.
- Placing ships, missiles, or players on the medium or high plane; the camera, shadow, water, audio, and smoke height work; `common::ToBaseHeight`; `FlightPlaneTable.h` (`game::kFlightPlanes`).
- Uniform, function, and wrapper renames (`fBaseHeight`, `ProjectToBaseHeight`).

## Risk triggers and invariants

- Client-only render: every changed site is under `BT_CLIENT` or a shader; `LightingWrappersBase.cpp` compiles on both builds and gains only a `Wrapper`. No `Frame` member, CRC'd value, save, replay, or wire layout changes; `Frame::kiVersion` and `kiProtocolVersion` stay.
- Low-plane invariance: for every light whose `z <= SurfaceHeight` the multiplier is exactly `1.0f` and the sprite comparison height is exactly `fBaseHeight`, so the deposit bytes and sprite alpha at every current ship-light site are unchanged.
- Zero GPU cost: no shader other than `VisibleLight.frag` changes, and that change replaces one uniform read with one `max`; no new draw, dispatch, or texture.
- Thread safety: `GlobalElevation` is already called from these render loops through `ProjectToBaseHeight`; the new call runs in the same loops on the same thread, never from frame-tick code (`IslandTerrain.h:150`). `gpFlightPlanes` is immutable after boot.
- Wrapper minimum `1.0f` keeps the divide finite; the clamp bounds the multiplier to `[0, 1]`.
- `SurfaceHeight` keeps `ProjectToFlightPlane`'s projection plane bit-identical for every plane: `max(max(fElevation, lowest), Plane(iPlane))` equals `max(fElevation, Plane(iPlane))` because plane heights ascend.

## Acceptance

| Criterion | Expected observation | Method |
|---|---|---|
| Builds | Client and server BrokenEngineSandbox build; the shader pack rebuilds `VisibleLight.frag` without diagnostics | `/compile` client and server |
| Tuning value is live | With the F3 Tweaks overlay open, `describe_ui` lists `Deposit Height Falloff Distance` with value `150` | `/agent-harness` client `describe_ui` |
| Low plane unchanged | Two injected fleet ships (Players before ShipTypeSplit, Omnis after) exchanging fire at the default spawn: the lit ground under blaster, impact, and explosion lights in a screenshot of the change matches the same state on the baseline build up to the lighting pass's own jitter and temporal noise; no light is dimmer | `/agent-harness`: `reset`, `inject_payload` two `SpawnPlayer` entries if ShipTypeSplit has not landed, `SpawnShip` after, per `verification.md` chosen placement, `screenshot` on baseline and on the change |
| High-plane light over water is dimmer on the ground | `pause`, `edit_frame` one `blasters` row's `pVecPositions` to `[x,y,100]` over open water, `pause false` for about 0.2 s, `pause true`, `screenshot`: that blaster's ground pool is visibly dimmer (about 0.37 of a low-plane blaster of the same type in the same screenshot); its sprite is unchanged | `/agent-harness` server `edit_frame` recipe (`commands-server.md`), client `screenshot` |
| High-plane light over terrain is dimmer and its sprite stays visible | The same recipe with `[x,y]` over an island from `describe_scene` `islands`: the pool on the terrain is dimmer than a low-plane blaster's, and the sprite is visible where the baseline build hides it over terrain above 7.5 m | `/agent-harness` as above, baseline and change |
| Missile exhaust dims in its arc | A missile fired with `{"bUseMissiles":true}` through `UpdatePlayer` if ShipTypeSplit has not landed, `UpdateShip` after, shows a brighter ground pool at launch than near its apex | `/agent-harness` screenshots at two ticks of one flight |
| No GPU timer regression | `query_profile` `gpuTimers` rows `Lighting Deposit`, `Lighting Spread`, and `VisibleLights` `averageUs` are within run-to-run noise of the baseline build at the same scene | `/agent-harness` client `query_profile`, baseline then change |
