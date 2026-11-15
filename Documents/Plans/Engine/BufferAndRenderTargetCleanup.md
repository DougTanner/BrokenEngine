<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T23:21:05.442Z","dependsOn":[]} -->
# Clean up buffer and render-target creation in Graphics/Managers

## Context

A path without a directory is under `Engine/Source/Graphics/Managers/`;
`Objects/` is `Engine/Source/Graphics/Objects/`. Lines are those at commit
`401e4b75`, except that the `CommandBufferRecordGlobal.cpp` and
`BufferManager.cpp` lines in `### Whole-buffer barriers` reflect a later
authoring session's edits to those files (so the same smoke barrier is cited
there as `BufferManager.cpp:604-615` and below as the neighbor of `:623`).
Every line moves once an earlier step edits its file; re-find each site by
symbol. Searches covered `Engine`, `Projects`, `Tools`, `DataPacker`, and
`Common`.

Originating gap: the architecture phase (cohesion lens) and the in-function
phase of an analysis of `Engine/Source/Graphics/Managers` at that commit.

### Whole-buffer barriers

Fifteen `VkBufferMemoryBarrier` initializers are written field by field:
thirteen in `CommandBufferRecordGlobal.cpp` (`:118-139` two, `:152-195` four,
`:205-226` two, `:249-260` one, `:270-291` two, `:307-318` one, `:333-343`
one), one at `BufferManager.cpp:604-615`, and one at
`RenderTargetTextures.cpp:356-367`. Every one sets the same nine fields and
varies only `srcAccessMask`, `dstAccessMask`, and `buffer`.

`Buffer::RecordBarriers` (`Objects/Buffer.cpp:115-160`) cannot serve them: it
fixes the source access and the source stage, offers three destinations, and
issues its own `vkCmdPipelineBarrier`, while these sites group barriers with
different source accesses under one call with call-site stage masks. So this
is uncovered duplication, not a bypassed helper.

Unmet criterion: no whole-buffer barrier in the three files is written field
by field.

### BufferManager dead members and duplicates

- Three public members are written, used on the next lines, and zeroed, with
  no other reader: `miSmokeActiveTileBufferSize` (`BufferManager.h:70`;
  `BufferManager.cpp:623`, `:624`, `:648`), `miWindActiveTileBufferSize`
  (`BufferManager.h:80`; `BufferManager.cpp:667`, `:673`, `:695`), and
  `InstanceManager::mMaxMultisampleCountVkSampleCountFlagBits`
  (`InstanceManager.h:49`; `InstanceManager.cpp:492-494`).
  `.agents/references/cpp-conventions.md` (derived members) applies. The two
  occupancy sizes are read elsewhere
  (`CommandBufferRecordGlobal.cpp:206-207`, `RenderTargetTextures.cpp:355`)
  and are not part of this.
- The `BufferManager` constructor (`BufferManager.cpp:91-94`) and
  `CreateSwapchainDependentBuffers` (`:296-299`) hold the same three
  statements: read the framebuffer count, assert it, call
  `InitializePerCommandBufferBuffers`. That function has exactly those two
  callers. `CreateSwapchainDependentBuffers` has one outside caller
  (`Engine/Source/Graphics/Graphics.cpp:425`), on the branch where the
  manager already exists, so the two paths never both run.
- Five copies of one 14-line block pack 16-bit indices and then vertices
  into one buffer: `BufferManager.cpp:137-150`, `:180-193`, `:213-226`,
  `:233-246` (debug meshes, identical except the member and the name) and
  `:21-34` (`Quads`, same shape with stride `sizeof(float) * 2` and the
  literal count 6, which equals the index array size).

Unmet criteria: none of the three names remains a member; the framebuffer
count is read and asserted in one place; the index-then-vertex packing for
16-bit meshes is written once.

### UI prepass indirect buffer

`ImGuiManager::CreateUiPrepassIndirectBuffer` (`ImGuiManager.cpp:313-338`)
fills the buffer and allocation create infos by hand (`:316-331`).
`Buffer::CreateBuffer` (`Objects/Buffer.cpp:10-54`) with usage
`INDIRECT_BUFFER` and `HOST_VISIBLE | HOST_COHERENT` produces the same create
info: `VMA_MEMORY_USAGE_AUTO`, `requiredFlags` equal to the memory flags, the
`HOST_ACCESS_SEQUENTIAL_WRITE | MAPPED` allocation flags (`Buffer.cpp:41`,
because the usage has no `TRANSFER_DST`), exclusive sharing, `VkName`, and the
mapped pointer through the optional `VmaAllocationInfo`. The one difference:
the helper rounds the size up to `nonCoherentAtomSize` (`Buffer.cpp:12`).

Sibling host-visible buffers already use the helper
(`Engine/Source/Graphics/Islands.cpp:60`, `Objects/PipelineCreator.cpp:90`).
This is the one host-visible allocation in the Managers directory that
encodes the flag contract of `Engine/Source/Graphics/Objects/AGENTS.md`
`## Resource Contracts` outside its owner.

Unmet criterion: the buffer is created through `Buffer::CreateBuffer`.

### Render-target texture creation

- Repeated `TextureInfo` initializers in `RenderTargetTextures.cpp`:
  `WindTwo` (`:314-331`) equals `windTextureInfo` (`:294-311`) except
  `.name`; `ShadowBlur` (`:152-166`), `ShadowBlurIntermediate` (`:167-181`),
  and `ShadowHistory` (`:182-196`) are identical except `.name`, and
  `Shadow` (`:137-151`) differs from them in `.name` and `.eTextureLayout`;
  `WaterDisplacementNormal` (`:57-71`) equals `WaterDisplacement`
  (`:42-56`) except `.name`; `ObjectShadowsBlurIntermediate` (`:414-428`)
  equals `ObjectShadowsBlur` (`:399-413`) except `.name`. The same file
  already reuses one `TextureInfo` for such pairs (`CreateWaterFoamTextures`
  `:77-96`, `CreateSmokeTextures` `:242-270`), and the comment at `:260-261`
  says reuse is what keeps a pair compatible.
- Dangling names: in `RenderTargetTexturesLighting.cpp`, `spreadName`
  (`:196`) and `spreadOnlyName` (`:212`) are `std::string` locals of the
  inner loop body. `TextureInfo::name` is a `std::string_view`, and
  `Texture::Create` stores the info (`mInfo = rInfo` in `CreateImage`). The
  strings die at `:228`, so `mInfo.name` of every spread and spread-only
  texture dangles. The file's own comment at `:11` and
  `RenderTargetTextures.cpp:20` state the rule, and the combine, history,
  and smoke names follow it. Nothing reads the dangling name today: every
  repository read of a texture's name for these textures happens inside
  `Create`, while the string is alive. This is a latent hazard, not observed
  misbehavior.
- Three identical zero clears: `RenderTargetTextures.cpp:277-289`,
  `:343-351`, and `RenderTargetTexturesLighting.cpp:384-396` each declare a
  zero `VkClearColorValue` and the same single-mip subresource range, then
  loop transition to transfer destination, `vkCmdClearColorImage`,
  transition back. `Texture` has no clear member (`Objects/Texture.h:99-111`).
- `RenderTargetTexturesLighting.cpp:67` checks at run time a plane count
  that is a compile-time value: `FlightPlanes::mPlanes` is bound once to
  `game::kFlightPlanes`, a `std::array` in both programs. A game table with
  3 planes passes the `static_assert` at `RenderTargetTextures.cpp:18` and
  fails only at boot. `RenderTargetTextures.cpp:240` asserts a bound that
  `:15`, `:18`, and `Engine/Source/Frame/FlightPlanes.cpp:13` already
  enforce.

Unmet criteria: each pair or triple of identical targets is created from one
`TextureInfo`; every `TextureInfo::name` a texture keeps outlives the
texture; the three image clears call one `Texture` member.

### Smoke trail gradient

The `SmokeTrailGradient` texture is filled by a loop at
`RenderTargetTextures.cpp:221-235`. For each texel of the 128 by 128 image it
computes `fDistance`, the distance from the centre divided by the half size
(`:231`), and then an intensity from `std::pow(fDistance, fAlpha)` and
`std::pow(1.0f - fDistance, fAlpha)` (`:232`), which it converts to `int64_t`
(`:234`) and narrows to `uint16_t` (`:235`).

The formula is only meaningful for `fDistance` in [0, 1], but the loop
evaluates it over the whole square:

- `fDistance` ranges from 0.01105 at the four centre texels to 1.40317 at
  the four corners. 3,492 of 16,384 texels (21.31%) have `fDistance` above
  1; no texel has exactly 1.
- For those texels `1.0f - fDistance` is negative, between -0.00104 and
  -0.40317.
- At `gSmokeTrailAlpha` 1.0 (the wrapper's maximum; its range is 0 to 1) the
  formula reduces to `1 - fDistance`, a negative intensity. The `int64_t` is
  negative and `:235` narrows it modulo 65,536: the texel just outside the
  circle gets 65,469 (full intensity) and the corner gets 39,115, so the
  outside region is a bright ring fading toward the corners instead of zero.
  This needs no external fact.
- At a fractional alpha, including the default, `:232` raises a negative
  base to a non-integer power. Converting a NaN `float` to an integer type
  is undefined behavior in C++ (verified against the ISO C++ draft,
  [conv.fpint] paragraph 1: the truncated value cannot be represented in
  the destination type). Whether MSVC's `std::pow` returns NaN for that
  input was not settled, so this Plan does not state what the default alpha
  writes today.
- The outside texels are sampled: `Engine/Data/Shaders/Smoke/Smoke.frag:34`
  reads the gradient at the quad texcoord rotated about the centre, with a
  clamp sampler
  (`Engine/Source/Frame/Collections/SmokeTrails/SmokeTrailsRender.cpp:26`),
  so quad corners and clamped edges land on them.
- An alpha change recreates the texture
  (`Engine/Source/Graphics/Graphics.cpp:650-658`).

Unmet criterion: every texel outside the inscribed circle holds 0 for alpha
above 0.

## Design

Recommended, in this order:

1. Whole-buffer barriers: one static function beside `Buffer::RecordBarriers`
   that returns one whole-buffer `VkBufferMemoryBarrier` from (source access,
   destination access, buffer), and the fifteen sites call it. Rationale:
   three files use it, so it belongs with the buffer helpers; three
   parameters replace eleven lines per use. Stage masks, the grouping of
   barriers under each `vkCmdPipelineBarrier` call, and the order of calls
   stay at the call sites, so the recorded commands are unchanged.
2. BufferManager dead members and duplicates:
   - Turn each of the three members into a local at its write site and
     delete the zeroing stores. The `ASSERT` at `InstanceManager.cpp:494`
     keeps working on the local.
   - The constructor calls `CreateSwapchainDependentBuffers` at the position
     of its three statements, between the model loop and the particle
     buffers, so no ordering changes.
   - One file-local function with five parameters (buffer, name, indices,
     vertices, stride) replaces the five packing blocks.
3. UI prepass indirect buffer: replace `ImGuiManager.cpp:316-331` with one
   `Buffer::CreateBuffer` call that passes a `VmaAllocationInfo` and takes the
   mapped pointer from it. The destroy at `ImGuiManager.cpp:294-296` is
   unchanged. A buffer rounded up to the atom size is harmless here: the
   extra bytes are never read.
4. Render-target texture creation:
   - Create each identical pair or triple from one `TextureInfo`, reassigning
     `.name` (and `.eTextureLayout` for `Shadow`) between `Create` calls, as
     the file already does elsewhere. No helper.
   - Give the spread and spread-only names static storage at file scope,
     assigned before `Create`, as the file's other names have. This
     allocates no more than the `std::format` calls already do.
   - Add one `Texture` member that takes only the command buffer and records
     the transition, zero clear, and transition back; the three clear sites
     call it. The one-shot command buffer each site uses stays where it is.
   - Turn `RenderTargetTexturesLighting.cpp:67` into a namespace-scope
     `static_assert` on `game::kFlightPlanes`, with the
     `Frame/FlightPlaneTable.h` include, and delete
     `RenderTargetTextures.cpp:240`. These two edits are cosmetic alone and
     are included only because this Plan already edits both files; the
     `static_assert` conversion is the part with value. Naming
     `game::kFlightPlanes` in engine code is allowed
     (`Engine/Source/AGENTS.md` `## Hub Conventions`), and
     `RenderTargetTextures.cpp:18` already does it.
5. Smoke trail gradient: clamp `fDistance` to at most 1 at
   `RenderTargetTextures.cpp:231`. With `fDistance` 1 the intensity is 0 for
   every alpha above 0, and every texel inside the circle is unchanged. This
   is a domain bound on a computed value, not the file or packet bad-value
   rule. Take the pre-change `SmokeTrailGradient` dump acceptance criterion 3
   needs before making this edit.

## Critical files

- `Engine/Source/Graphics/Objects/Buffer.h`
- `Engine/Source/Graphics/Objects/Buffer.cpp`
- `Engine/Source/Graphics/Objects/Texture.h`
- `Engine/Source/Graphics/Objects/Texture.cpp`
- `Engine/Source/Graphics/Managers/BufferManager.cpp`
- `Engine/Source/Graphics/Managers/BufferManager.h`
- `Engine/Source/Graphics/Managers/InstanceManager.cpp`
- `Engine/Source/Graphics/Managers/InstanceManager.h`
- `Engine/Source/Graphics/Managers/CommandBufferRecordGlobal.cpp`
- `Engine/Source/Graphics/Managers/ImGuiManager.cpp`
- `Engine/Source/Graphics/Managers/RenderTargetTextures.cpp`
- `Engine/Source/Graphics/Managers/RenderTargetTexturesLighting.cpp`

## In scope

- `Buffer`: one new static function, and the fifteen barrier initializers
  listed in `### Whole-buffer barriers`.
- `BufferManager` and `InstanceManager`: the three members and their writes
  listed in `### BufferManager dead members and duplicates`;
  `BufferManager.cpp:91-94` and `:294-302`, and the declaration at
  `BufferManager.h:123` as far as the merge needs it; the five packing blocks
  and one new file-local function.
- `ImGuiManager`: the creation statements in
  `ImGuiManager::CreateUiPrepassIndirectBuffer`.
- `Texture` and the render-target files: four functions in
  `RenderTargetTextures.cpp` (`:37-72`, `:137-196`, `:292-331`, `:398-428`),
  the repeated initializers; `RenderTargetTexturesLighting.cpp:188-228` and
  file-scope name storage; one new `Texture` member and the three clear
  sites; the two plane-count ASSERTs.
- The `fDistance` computation in the `SmokeTrailGradient` fill loop.

## Out of scope

- `Buffer::RecordBarriers` and `BarrierInfo`: both stay as they are.
- Image barriers and barriers that cover a sub-range.
- Stage masks, barrier grouping, and call order at every barrier site.
- A shared occupancy zero-fill function for smoke and wind: the two copies
  differ in array shape, size member, and enclosing one-shot, and wind
  occupancy being zeroed with the wind textures is a recorded design
  (`Engine/Source/Graphics/Managers/AGENTS.md` `### BufferManager`).
- The water mesh (`BufferManager.cpp:783-796`): it uses 32-bit indices from
  vectors.
- The two skinning bump allocators and their grow paths
  (`BufferManager.cpp:487-571`), and the creation-time identity fills
  (`:359-407`).
- `ASSERT(bInserted)` at `BufferManager.cpp:88`.
- `miSmokeOccupancyBufferSize` and `miWindOccupancyBufferSize`.
- The UI prepass indirect buffer's destroy path, its contents, and its
  consumers; `Buffer::CreateBuffer` itself.
- `ShadowElevation` and `ObjectShadows`: their infos differ.
- Shadow texture sizing in `CreateShadowTextures`.
- Formats, extents, usages, and creation order of every render target.
- The smoke gradient intensity formula for texels inside the circle, the
  texture size and format, and `gSmokeTrailAlpha`'s range.
- Alpha 0, where every `pow` term is `pow(x, 0)` and the gradient texture is
  0.5 everywhere, inside and outside alike.
- `Smoke.frag` and the sampler.

## Risk tier

Tier 2 — scoped behavior. Triggers: it removes three public members from
`BufferManager` and `InstanceManager`, adds a public static function to
`Buffer`, and adds a public member to `Texture`, all in headers `Engine.h`
aggregates, which Tier 1's "no public signature" excludes; the allocation
request of the UI prepass indirect buffer changes (its size may round up); and
the contents of the `SmokeTrailGradient` texture change, which is rendering
behavior of one subsystem.

Invariants: no threading, determinism, layout, or wire exposure. Every
removed member is unread and both BufferManager merges keep their statements
and order; the recorded barriers are field-for-field the same; the created
images and recorded clears are the same. The gradient texture is client
render state outside the simulation CRC.

## Acceptance criteria

1. The criteria in `## Context` hold in the source:
   - None of the fifteen barrier sites sets the barrier fields one by one,
     and `RecordBarriers` and `BarrierInfo` are unchanged.
   - None of the three names remains a member; the framebuffer count is read
     and asserted in one place; the index-then-vertex packing for 16-bit
     meshes is written once.
   - `CreateUiPrepassIndirectBuffer` calls `Buffer::CreateBuffer` and sets no
     create-info field itself.
   - Each pair or triple of identical targets is created from one
     `TextureInfo`; every `TextureInfo::name` a texture keeps outlives the
     texture; the three image clears call one `Texture` member; a plane count
     the lighting layers cannot serve fails the build.
   - `fDistance` never exceeds 1 when the intensity is computed.
2. The BrokenEngineSandbox client and ModelViewer build through `/compile`.
3. Through `/agent-harness`, one client run with validation layers on boots,
   renders a world frame with lighting, shadows, wind, smoke, and water, with
   the UI shown over it, and survives one window resize (the swapchain
   recreation and swapchain-dependent buffer path), the UI still shown after
   it, with no synchronization or other validation error. In that run a
   render-target dump of `SmokeTrailGradient` (the texture is created with
   transfer-source usage for that readback) has 0 in every texel outside the
   inscribed circle, at the default alpha and at alpha 1.0, and the
   in-circle texels equal a dump taken before the change. The supplied
   evidence records no harness setup.

## Notes

- The executing session confirms no private-game use of the three removed
  members where that source is available, and reports it as unconfirmed
  otherwise.
- Whether `vkSetDebugUtilsObjectNameEXT` and the tools behind it copy the
  name they are given was asked and not settled; the dangling-name item is
  recorded as latent on repository evidence only.
