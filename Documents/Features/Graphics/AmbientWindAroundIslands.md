# Ambient Wind with Gusts, Deflected by Islands

Revisit When: the scene reads as dead calm between events — smoke only moves when something pushes it — or smoke visibly passes straight through mountains and the wrongness shows at RTS altitude.

## Context

Wind is a client-only 2D velocity field (`Engine/Data/Shaders/Wind/AGENTS.md`), stepped by wall-clock time and never fed back into frame state. Its only sources are local deposits: `WindRadials` (explosions) and `WindTrails` (ships, blasters). `WindSpread` (`Engine/Data/Shaders/Wind/WindSpreadCommon.h`) adds world-anchored swirl noise and blends every behavior constant as a Low/High pair by `WindMagnitudeFactor`. The field covers the smoke area, and outside the wind passes themselves only the two smoke spread passes sample it, through `SmokeWindSample` (`Engine/Data/Shaders/Smoke/SmokeSpreadCommon.h`).

Two gaps remain:

- **No ambient wind.** `WindSpread`'s constant decay returns exact zero once deposits fade, so the world is calm and smoke drifts only where something pushed it.
- **No terrain.** The wind spread passes bind no elevation, so flow passes straight through islands. Smoke pass B (`Engine/Data/Shaders/Smoke/SmokeSpreadTwo.comp`) already binds `elevationTextureSampler`, which is `RenderTargetTextures::mTerrainElevationTexture` — the visible-area max of island heightmaps (`PipelineManager::CreateSmokeWindPipelines`). Smoke pass A (`SmokeSpreadOne.comp`) binds no elevation.

## Design

- **Ambient wind as an analytic term, not stored.** A global direction and strength (sliders), modulated by slow world-anchored noise for gusts and direction drift. Add it inside `SmokeWindSample` instead of depositing it into the field, so the wind field still decays to exact zero and its sparse dispatch stays sparse (`Wind/AGENTS.md` `## Sparse Dispatch Invariants`). Storing a field-wide ambient would make every wind tile permanently active. `SmokeWindSample` takes only a wind texcoord today, so it gains the world position both callers already compute.
- **Island deflection.** Where terrain elevation is above sea level, project out the uphill component of velocity using the elevation gradient, so flow slides along slopes and around peaks. Applied to the ambient term at the read site, it bends ambient wind around islands; on a windward slope the removed uphill component also slows the wind there, which is what lets smoke pile up.
- **Tuning.** Every new behavior constant follows the Low/High pair rule in `Wind/AGENTS.md`.

### Consequences of the read-site ambient

The analytic term keeps the wind field sparse, and smoke stays sparse because smoke occupancy is still rebuilt from nonzero smoke. It does change these existing smoke behaviors, which a plan has to accept or counter:

- Smoke's dilate radius stays 2 tiles (`Smoke/AGENTS.md` `## Frame and Remap Contract`), so ambient advection per refresh, after the `fWindToSmokeStrength`/`fWindToSmokePower` rescale and times the step count, must stay inside that halo or plume fronts are deleted.
- `SmokeSpread`'s `fHasWind` becomes 1 everywhere, so the `fWindSmokeRetention` mix applies to all smoke, and `SmokeCurlOffset` blends its curl strength by the ambient-raised magnitude in pass B.
- Pass B's extra decay over terrain (elevation above 0) kills smoke over islands quickly, which works against visible windward pile-up on the slopes themselves.

### Open choices

- Whether the stored field is also deflected — elevation bound to the wind spread passes, projecting deposit-driven wind — or only the ambient term at the read site.
- Whether pass A gets an elevation binding, or the ambient term (and its deflection) applies only in pass B.
- Whether ambient also carries stored deposits downwind (an analytic ambient added to `WindSpread`'s advection lookup only), or explosion and trail wind stays put while smoke drifts.
- Whether `fHasWind` and the curl blend read the stored field only or stored plus ambient.
- Whether to add an explicit windward slowdown beyond what the projection gives, and whether the terrain decay is relaxed so piled-up smoke stays visible.

## Out of scope

- Feeding wind into simulation (ship drift, projectile drift) — wind must stay visual-only.
- A real pressure-projection fluid solver; the slope projection is a cheap stand-in.
- Altitude-varying wind; the field stays 2D.

## Notes

- Client-only; no determinism or CRC exposure.
- Gust noise must be sampled by world position, like the existing swirl, so it stays put under camera motion.
- `Documents/Plans/Engine/SmokePerFlightPlane.md` keeps one wind field for every flight plane and guarantees island peaks lie below the highest plane. Deflection baked into the stored field would therefore deflect every plane alike; deflection at the read site can use plane-relative elevation, the way that plan measures its per-plane terrain decay, so only planes below the peaks bend.
- References:
  - Stam, "Stable Fluids" (SIGGRAPH 1999), DOI 10.1145/311535.311548 — the stock solver treats obstacles as blocked cells whose velocity is zeroed.
  - Fedkiw, Stam, Jensen, "Visual Simulation of Smoke" (SIGGRAPH 2001), https://www.graphics.stanford.edu/papers/smoke/ — smoke interacting with solid objects.
  - Bridson, "Fluid Simulation" SIGGRAPH 2007 course notes — solid-boundary handling.
  - Original note links: vector field https://www.youtube.com/watch?v=alhpH6ECFvQ, flow field https://youtu.be/n2fhtxmSD7I?t=288.
