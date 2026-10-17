<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-17T18:42:43.504Z","dependsOn":[]} -->
# Remove the smoke deposit tile scale, which is always exactly 1.0

Tier 2 (`.agents/references/risk-tiers.md`): a behavior-preserving change inside
the client Graphics subsystem that removes one field from the shared CPU/GLSL
`GlobalLayout` uniform contract. The uniform is rebuilt every frame, never
persisted, serialized, networked, or CRC'd, and the removed multiply is by 1.0,
so no rendered value changes. Line numbers cite baseline `259772c0`. Where a
statement here and the code disagree, the code wins; report the contradiction
instead of matching one side to the other.

## Context

The `/over-engineering-review` finder raised this as design suggestion DS-1
during the SmokePerFlightPlane change (landed at `259772c0`); it was left
unfixed there because it predates that change, and the user accepted filing it.

`GlobalLayout::fSmokeDepositTileScale`
(`Engine/Data/Shaders/ShaderGlobalLayout.h:86`) rescales a deposit fragment's
coordinate before the occupancy-tile lookup in `Smoke.frag`
(`Engine/Data/Shaders/Smoke/Smoke.frag:42`), to correct for SmokeOne and
SmokeTwo having different extents. `RenderSmokeGlobal`
(`Engine/Source/Graphics/Render/SmokeUniforms.cpp:73-74`) takes the larger of
plane 0's SmokeOne and SmokeTwo extents, and `:80` publishes
`iMaxWidth / iTextureOneWidth`.

The extents never differ. `RenderTargetTextures::CreateSmokeTextures`
(`Engine/Source/Graphics/Managers/RenderTargetTextures.cpp:211-239`) builds
`smokeTextureTwoInfo` as a copy of `smokeTextureInfo` with only
`vkRenderPassInitialImageLayout` changed, and inside the per-plane loop changes
only each `name`; both copies carry the same `vkExtent3D`. So `iMaxWidth`
equals `iTextureOneWidth`, `iMaxHeight` equals `iTextureOneHeight`, and the
published scale is exactly 1.0 for every plane on every frame.
`BufferManager::CreateSmokeHierarchicalBuffers`
(`Engine/Source/Graphics/Managers/BufferManager.cpp:568-572`) already sizes the
smoke occupancy from plane 0's SmokeOne extent alone.

## Design

- Delete `float fSmokeDepositTileScale INIT;` from `GlobalLayout`. The header
  is single-sourced for C++ and GLSL, so both sides change together.
- In `Smoke.frag`, compute the tile coordinate as
  `ivec2(gl_FragCoord.xy) / kiComputeTileSize`, dropping the multiply.
- In `RenderSmokeGlobal`, delete the `fSmokeDepositTileScale` store (`:80`)
  and the two max-extent locals (`:73-74`), and pass `iTextureOneWidth` and
  `iTextureOneHeight` to the `TileCount` calls (`:75-76`). The max-extent locals
  are still read by those tile counts, so the author
  recommends replacing them with the SmokeOne extent (equal by the construction
  above, and matching `BufferManager.cpp:569-572`) rather than leaving a
  `std::max` over two equal values. The `:70` comment stays accurate.

## Critical files

- `Engine/Data/Shaders/ShaderGlobalLayout.h`
- `Engine/Data/Shaders/Smoke/Smoke.frag`
- `Engine/Source/Graphics/Render/SmokeUniforms.cpp`

## In scope

- `GlobalLayout::fSmokeDepositTileScale` (the one field declaration).
- `Smoke.frag` `main`: the tile-coordinate line only.
- `SmokeUniforms.cpp` `RenderSmokeGlobal`: the `iMaxWidth`/`iMaxHeight`
  locals, the two `TileCount` arguments, and the `fSmokeDepositTileScale`
  store.

## Out of scope

- `RenderTargetTextures::CreateSmokeTextures` and the smoke texture extents.
- Storing a single smoke extent or otherwise restructuring how smoke sizes are
  read (`BufferManager.cpp`, `SmokeUniforms.cpp` `iTextureOneWidth`/`Height`
  uses at `:99-100`); the finder's companion suggestion to do so was dropped.
- Wind tile counts and any other `GlobalLayout` field.
- `SmokeUniforms.cpp` cadence counter work owned by
  `Documents/Plans/Engine/CadencePhaseBalancing.md`.

## Risk triggers and invariants

- CPU/GLSL layout: the field is removed from the one shared header, so CPU and
  GLSL offsets of every later `GlobalLayout` field shift together; no other
  site reads the field (`rg -n fSmokeDepositTileScale` finds only the three
  sites above at baseline).
- Shader build: the `Smoke.frag` and `ShaderGlobalLayout.h` edits change
  compiled SPIR-V, so the build regenerates Local data through `/compile`.
- Determinism: client-only render state, outside the CRC; no simulation,
  wire, save, or replay surface is touched.

## Acceptance criteria

1. `rg -n fSmokeDepositTileScale Engine Projects` prints nothing.
2. `/compile` builds the client and the server in Debug and Release with no new
   warnings, and the shader data regenerates without error.
3. A live `/agent-harness` client session with smoke trails or puffs on screen
   shows smoke depositing and spreading as before, with no Vulkan validation
   errors in the client log.

## Notes

- The finder's DS-2 (store one smoke extent) is deliberately not recorded; see
  `## Out of scope`.
