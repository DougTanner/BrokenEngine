# Island Export Pipeline

Gaea route baking, archetype patching, region splitting, and island intermediate generation. An archetype is a reusable Gaea terrain template that an island export starts from. The parent ExportJobs hub owns generic cache and chunk rules.

## Cache Lifecycle

Each delivered route owns `BakedIsland.json`, a versioned completion declaration binding the current
island configuration, effective patched terrain, stage versions and complete accepted leaf set to
seven delivered-file hashes per leaf. The three source geometry inputs (`BakedDimensions.json`,
`Elevation.r32`, `MeshProcessed.bin`) sit beside the four BC textures and are canonical for
discovery and chunk export. A validated publication needs neither Gaea nor raw cache files. Invalid
publications fail ordinary runs rather than silently regenerating.

Without a declaration, an ordinary run may adopt the known-good original author cache only when bake
provenance, normalized patched terrain, complete split outputs and existing texture-input freshness
markers all agree. The legacy texture marker has no historical BC output hashes: initial adoption
trusts those known-good bytes and records the seven hashes for later mismatch detection. Geometry
copies use staged sibling renames; the declaration is published last. Normal adoption never rewrites
BC files or invokes Git.

Raw Gaea and split outputs remain under
`%LOCALAPPDATA%/BrokenEngine/DataPackerCache/<project>/Gaea/Islands/`. `BakeVersion.meta` and
`SplitVersion.meta` independently own raw and split completion. `BakedDimensions.json` is written
last in accepted cache leaves; rejected leaves retain no completion marker. Explicit authoring
invalidates a route publication before changing delivered inputs, prunes stale numeric leaves, and
publishes a new declaration only after all island jobs succeed. Source routes for removed island
configurations are pruned by the prepass.

Splitting also densifies the mesh: after the raw Gaea Mesher output is loaded,
`SubdivideBeachBand` recursively splits triangles whose height range overlaps
the beach band — the narrow depth window straddling engine Z 0, tuned by the
`kfBeachSubdivision*` constants in `BakeRoute.cpp` — until their longest
horizontal edge falls under the target. Neighbors outside the band take the
smallest split that absorbs an inherited midpoint without creating new ones, so
the cascade stops one ring outside the band. It runs post-bake on the full mesh
and the chunk crops reuse the result, so a change here bumps the split version,
not the bake version.

Each accepted leaf's downsampled elevation is edge-tapered before it is written:
a band in from every leaf border — interior split cut lines included — ramps
water at or below halfway depth down to the per-island sea floor, so every leaf
blends with the engine's constant open-ocean elevation clear, while shallower
water is preserved almost unchanged so the taper cannot amputate a leaf's
visible sand apron. It is a split-stage step in `ProcessBakedRegion`, so a
change here bumps the split version.

Both forbid guards still stop a dirty Gaea export. Published BC intermediates pass through packing under the expensive-export guard because they require no encoding.

Published packing, cache adoption and authorized split-only work succeed with Gaea uninstalled. An authorized dirty raw bake resolves Gaea before its dirty-stage work begins.

## Island Outputs

Each complete leaf produces a `kIsland` chunk plus independently routed BC texture intermediates for color, normals, ambient occlusion, and the RGBA material mask. The mask channels are rock, sand, snow, and flow. Flow is packed and fingerprinted like the others; `Documents/Features/Graphics/IslandFlowMaskUsage.md` owns why it is kept. Texture filenames and formats are producer/consumer contracts; update the runtime shader and upload expectations with any change.

Masking occurs before mip generation. Underwater texels use format-specific flat
values so constant regions survive through mipmaps and compression, while
shoreline above `common::kfUnderwaterMaskThresholdMeters` remains available to
rendering and placement. That one shared constant is the whole cut line:
DataPacker bakes both the texture mask and the valid-area hull from it, and the
engine publishes the same value to the shaders and to the debug hull draw. Never
introduce a second local threshold — nothing would catch baked textures drifting
out of step with the shader.

The island payload stores the quantized elevation field, XY mesh data, indices, and valid-area hull. Runtime terrain reconstructs Z from elevation. The hull must have at least three vertices and remain convex and counter-clockwise because deterministic client/server island placement consumes it through `common::ConvexHullsOverlap` with no substitute shape; producer checks enforce those properties before serialization, so a leaf whose hull has fewer than three vertices fails its export.

Large shared route inputs use persistent fingerprints rather than repeated content reads. JPEG diagnostics belong only in the parallel `%LOCALAPPDATA%/BrokenEngine/DataPackerCache/.../Gaea/Diagnostics/` tree, never the source checkout.

## Versioning

The publication schema has its own version, independent of the four calculated stages; bump only the
owner of changed bytes or contracts. The bake version covers raw Gaea output; the split version
covers post-bake crop, split, leaf-output, or completion rules. `ExportIsland::kiTextureVersion`
covers the BC texture encode alone, and `ExportIsland::kiVersion` covers the chunk payload —
quantized elevation, mesh, indices, hull, and `IslandHeader` layout — which may also require the
shared data-format version.

The BC outputs are tracked in the checkout, which shapes the texture stage: its version stays a plain `int64_t` outside `ExportJob::Version(...)`, so a chunk-header layout change cannot rewrite hundreds of megabytes of tracked textures for byte-identical output. Its marker fingerprints only the encode's own inputs; the processed mesh is deliberately excluded so a mesh-only rebake re-packs the chunk without touching the textures.

## See Also

- `../Texture/AGENTS.md` - texture intermediate encoding
- `../../../../Common/Math/AGENTS.md` - deterministic convex-hull contract
- `../../../../Engine/Source/Frame/AGENTS.md` - runtime island placement ownership
