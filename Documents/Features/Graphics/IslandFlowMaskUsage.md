# Island Flow Mask Usage

Revisit When: terrain art wants water-carved detail — a distinct streambed material, visible streams or waterfalls, or flow-aware snow — and the authored Flow channel is the cheapest signal that would drive it.

## Context

Gaea authors a flow/erosion map per island route (`Flow.png`), packed into the alpha channel of the island's BC7 RGBA material mask beside R=Rock, G=Sand, B=Snow. The mask encoder in `DataPacker/Source/ExportJobs/ExportIsland.cpp` writes and fingerprints it like the other three channels, and it is zeroed underwater with them.

`Engine/Data/Shaders/Terrain/Terrain.frag` samples the mask but reads only RGB, so the channel has no consumer today. It is kept deliberately: the data is already baked into tracked textures, so a later consumer costs shader work only, with no re-bake and no `.pack` version bump.

## Ideas

None of these is a decision; each is a candidate a future plan would have to design and cost on its own.

- **Streambed material.** Sample a distinct wet-rock or gravel streambed texture where flow is high and blend it over the rock/sand result, giving carved channels a material of their own instead of ordinary rock.
- **Wet tint.** Darken and desaturate albedo in flow regions, the cheapest possible use — one mix with no new texture or binding.
- **Streams and waterfalls.** Drive scrolling water normals or foam along high-flow paths, and treat high flow meeting a steep slope (from the existing island normal texture) as a waterfall site.
- **Client-only emitter mask.** Use flow as a spawn mask for water particles or ambient stream audio emitters; visual/audio only, so it stays out of the simulation and CRC.
- **Snow suppression.** Reduce snow accumulation where flow is high, so running water keeps its channels clear.

## Out of scope

- Any change to Gaea mask authoring or the DataPacker mask packing — the channel already exists and is already baked.
- Simulated or dynamic water flow; the channel is a static authored map.

## Notes

- Client-only rendering path in every idea above; no determinism or CRC exposure.
- `Documents/Features/Graphics/TerrainSnowCurvature.md` already puts the A channel out of its own scope as reserved for a future material, consistent with this document.
