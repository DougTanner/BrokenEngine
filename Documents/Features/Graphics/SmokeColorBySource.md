# Smoke Color by Source

Revisit When: smoke from different sources needs to read as different things from altitude — clean missile exhaust against sooty explosion and wreck smoke — or a new smoke source (burning wrecks, engine damage) wants its own look.

## Context

Smoke is a client-only 2D density field (`Engine/Data/Shaders/Smoke/AGENTS.md`): one `R16_SFLOAT` channel per texel (`shaders::kVkFormatSmoke`), deposited additively through `Smoke.frag` by the `SmokeTrails` and `Puffs` collections, advected and diffused by the two spread passes, and composited in `Engine/Data/Shaders/ShaderFunctions.h` by `AddSmoke` (`Model.frag`) and `BlendSmokePrecomputed` (`Terrain.frag`, `Water.frag`). `BlendSmoke` has no caller, and `SmokeShadow` reads density only.

The only color variation today is by density: `fSmokeColorMin + fSmokeColorMultiplier * density` scales the lit smoke color, capped at 1.25, so thin smoke reads darker than thick smoke. Every source makes the same grey smoke.

Sources do not map one-to-one onto collections. Missiles (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.cpp`) and explosions (`Engine/Source/Frame/Collections/Explosions/ExplosionsSpawn.cpp`) both deposit through `SmokeTrails`, each under its own registered trail type; explosions, blaster terrain hits (`BlastersUpdate.cpp`), and player impacts (`PlayersCombat.cpp`) deposit through `Puffs`. A per-type color already exists but never reaches the field: `SmokeTrailsType::iColor` and `PuffsType::uiColor`, all registered as `0xFFFFFFFF`, are copied into each instance's `uiColor` (`QuadLayout`, `AxisAlignedQuadLayout`), but the deposit vertex shaders do not forward it and `Smoke.frag` writes density only.

## Design

- **Shade channel.** Store `density * shade` beside density (`R16G16_SFLOAT` or a second texture), where shade runs from 0 (black soot) to 1 (white). Deposits add both channels; the composite reads `shade = G / max(R, epsilon)`. Deposit, advection, and diffusion are linear, so overlapping smoke mixes to the density-weighted average shade with no extra logic. Pass B's terrain and edge decays scale only density today; they must scale the shade channel by the same factor, or the ratio drifts.
- **Shade per source.** Open choice for the plan:
  - Per deposit pipeline: one push constant per collection, since deposit pipelines are keyed by collection CRC (`DynamicPipelines::CreateDepositPipeline`). Cheapest, but it separates only trails from puffs; missile exhaust and explosion trails both use `SmokeTrails`, so it cannot make exhaust white and explosion smoke dark on its own.
  - Per type: derive shade from the existing type color by forwarding the instance `uiColor` to `Smoke.frag`. Separates missile trails from explosion trails with no new collection member.
  - Per instance: a new collection member, for sources that share one type (a missile trail versus a burning-wreck trail).
- **Composite.** Every compositing reader of the field (`AddSmoke`, and the `Terrain.frag` and `Water.frag` samples that feed `BlendSmokePrecomputed`) also reads the shade channel, mixes the lit smoke color between a dark soot tint and white by shade, and keeps the existing density tint on top so thin smoke still darkens.
- **Exact-zero rule.** Pass B's step-down walk that zeros faint density must zero the shade channel with it (`Smoke/AGENTS.md` `## Sparse Dispatch Invariants`). Occupancy is rebuilt from density, so shade left behind in a tile that drops out of occupancy is never revisited and tints the next smoke deposited there.
- **Optional look extras.** Composite-side only: slow noise-driven wobble of the smoke sample position, and 3D-noise erosion of plume edges so plumes read less blobby. The spread passes already swirl the field itself (`SmokeSpread` noise, `SmokeCurlOffset`); these extras change only how it is read.

## Out of scope

- Colored (non-grey) smoke.
- Any change to the wind field or to smoke spread physics beyond carrying the shade channel through the spread passes.

## Notes

- Client-only rendering; smoke is not CRC'd, so no determinism exposure.
- A second channel doubles the smoke textures' memory and the spread passes' texture bandwidth.
- `Documents/Plans/Engine/SmokePerFlightPlane.md` turns the smoke textures into one field per flight plane; whichever lands second carries the shade channel across every plane's texture.
- References:
  - Fedkiw, Stam, Jensen, "Visual Simulation of Smoke" (SIGGRAPH 2001), https://www.graphics.stanford.edu/papers/smoke/ — vorticity confinement restores small rolling detail lost on coarse grids.
  - Forney, Smokeview/FDS (2011), https://files.thunderheadeng.com/femtc/2011_d1-07-forney-presentation.pdf — color and opacity driven from separate soot and temperature quantities, the closest published analogue to a shade channel.
  - Playdead, "Low Complexity, High Fidelity: INSIDE Rendering" (GDC Europe 2016), https://gdcvault.com/play/1023783/Low-Complexity-High-Fidelity-INSIDE — dithering against banding; the original note pointed at its smoke section (https://youtu.be/RdN06E6Xn9E?t=2968).
