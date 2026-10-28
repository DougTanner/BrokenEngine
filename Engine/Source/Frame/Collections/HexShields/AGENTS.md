# HexShields - Geodesic Shield Meshes

Client-only geodesic shield meshes with directional damage visualization. Fully owner-driven — no autonomous simulation; a game-layer collection owns lifetime, per-frame `Sync` data, and damage-channel writes (including the slot-recycling policy).

## Unique Aspects

- Interpolate `Update` is not a stub: unlike a typical owner-driven collection (all PostRender hooks empty per the hub Sync pattern), the Interpolate-phase `Update` here carries transforms and directional intensities forward — see asymmetric propagation below.
- Directional damage channels: pointer-array count matches the shader-side direction constant — keep header in sync. Independent vertex-stage and fragment-stage intensities per direction.
- Asymmetric propagation: `PersistentMembers()` carries owner-driven scalar fields (position, type index, lighting/size/color-mix) through `AllocateAndCopyMembers()`; transforms and directional intensities instead carry forward via the Interpolate `Update`'s previous-frame copy (no lerp) then get overwritten by `Sync`. Last values persist on frames where sync has not yet run.
- The visible pipeline owns one packed dynamic buffer and rebinds it on resize; `EndRender` always publishes its current count. Retained lighting scalars do not publish physical sources.
- Culled-packed GPU buffer: per-element visibility test with margin; culled elements do not advance the write cursor, keeping the buffer tightly packed.
- Type-registry colors: `TypeRegistry<HexShieldsType>` holds the per-type-index visible color (packed ABGR uint32) and minimum intensity; the visible color decodes to float4 inside the render loop.
