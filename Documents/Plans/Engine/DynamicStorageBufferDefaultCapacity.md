<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-19T17:33:30.524Z","dependsOn":[]} -->
# Give dynamic storage buffers a higher default capacity and break when one grows

## Context

The CRC-keyed per-framebuffer dynamic storage buffers that collection render
paths fill each frame start at one element and grow to exactly the size the
current frame needs. Every rise of the required size above a buffer's current
size reallocates that framebuffer's buffer on its turn, which costs a
`vmaCreateBuffer` (a `vkAllocateMemory` whenever VMA has to open a new block),
the release of the previous buffer, and a descriptor update, all on the render
path. Found while preparing the texture-streaming stall fix in this session and
left out of it because the root cause and boundary differ.

Verified behavior, at baseline `8fbdd7910`:

- `BufferManager::CreateDynamicBuffer`
  (`Engine/Source/Graphics/Managers/BufferManager.cpp:417`) creates every
  framebuffer's buffer at one element (`.iDataSize = iElementSize`, line 435),
  so the first frame in which a collection draws anything reallocates it on
  each framebuffer in turn. A repeat call for an existing CRC returns the
  existing buffers (line 422), so the `DynamicPipelines` creation functions'
  second call for the same CRC (`DynamicPipelines.cpp:186`, `:238`, `:266`,
  `:300`) creates nothing.
- `BufferManager::ResizeDynamicBufferIfNeeded` (`BufferManager.cpp:466`)
  computes `iRequiredSize = iLayoutSize * iCapacity` and, when the buffer is
  smaller, calls `ResizeDynamicBuffer` (`BufferManager.cpp:443`), which
  recreates only that framebuffer's buffer at exactly `iRequiredSize` (line
  461). Each framebuffer's buffer is recreated only on its own turn, after that
  framebuffer's fence wait (comment at lines 445-448), so one rise of the
  required size reallocates up to one buffer per framebuffer, on successive
  frames. `ResizeDynamicBuffer` has no other caller.
- Collection `BeginRender` hooks pass `AccumulateRenderCapacity`
  (`Engine/Source/Frame/Collections/Collection.h:384`), the sum of that
  collection's `iCapacity` over every active coord; per-cell capacity grows as
  `2 * capacity + 1` (`CollectionLifecycle.h:44`). A coord joining the active
  set therefore adds its whole capacity. Callers: `BattleshipsRender.cpp:77`,
  `FightersRender.cpp:76`, `OmnisRender.cpp:119`, `MissilesRender.cpp:59`
  (game) and the engine Billboards, WindTrails, WindRadials, PointLights,
  AreaLights, HexShields, SmokeTrails, and Puffs render files.
- The debug path `DebugRender.cpp:126` passes the live instance count, not a
  capacity, so with debug rendering on it reallocates on every new peak count.
- `DestroySwapchainDependentBuffers` clears every dynamic storage buffer
  (`BufferManager.cpp:270-273`); they are recreated through
  `CreateDynamicBuffer`, so after a swapchain recreation every buffer starts
  again from its initial size.
- The buffer size is read back only as the capacity
  `GetDynamicStorageBuffer` (`BufferManager.h:45`) returns, and every caller
  uses that only in an `ASSERT` that the rows it writes fit. No shader or
  indirect count reads the buffer length, so extra capacity is never drawn.
- At baseline there are at most 37 such buffers: seven `ModelLayout` model
  buffers, one per flight plane for SmokeTrails (`QuadLayout`) and Puffs
  (`AxisAlignedQuadLayout`) with up to `kiMaxFlightPlanes` = 8 planes, four
  `DebugRenderLayout` buffers (`PipelineManager.cpp:459`), and one or two for
  each other renderable collection. Their element sizes
  (`Engine/Data/Shaders/ShaderLayoutsBase.h:189-292`) sum to about 4.5 KB, so
  each element of initial capacity costs about 13 KB of host-visible memory
  across all buffers with three framebuffers.
- `DEBUG_BREAK()` (`Common/ErrorUtils.h:19`) logs one `kWarning` naming its
  call site, then breaks only when a debugger is attached; release builds and
  runs without a debugger continue. The workbuffer already uses it as its
  under-sizing signal (`Common/AGENTS.md` `## Allocation-Free Scratch`).

## Design

The author recommends one shared initial element count for every dynamic
storage buffer, a file-scope constant in `BufferManager.cpp` (for example
`kiDynamicBufferInitialCapacity`), which `CreateDynamicBuffer` multiplies by
`iElementSize` for each framebuffer's `.iDataSize`. One constant keeps the
change inside `BufferManager` and leaves every `CreateDynamicBuffer` caller and
the `DynamicPipelines` creation signatures untouched; a per-buffer initial
capacity would thread a new argument through about fifteen callers and four
`DynamicPipelines` functions whose repeat call is a no-op.

Growth still happens, so release builds and runs without a debugger stay
correct, but it doubles instead of growing to exactly the required size.
`ResizeDynamicBufferIfNeeded` still returns `nullptr` with no allocation when
this framebuffer's buffer holds `iRequiredSize` bytes. Otherwise the author
recommends one rule that serves both the growth size and the break check: take
the largest `iDataSize` among every framebuffer's buffer for that CRC and type,
double it until it is at least `iRequiredSize`, and call `ResizeDynamicBuffer`
with the result. When that largest size was already at least `iRequiredSize`,
no doubling happens: the framebuffer grows silently to the size a sibling
already holds. Only when the largest size was smaller, so the buffer must grow
past every framebuffer's current size, does it log one `kGraphics` `kWarning`
naming the buffer, the required element count (`iCapacity`), the new element
count, and the initial-capacity constant to raise, then call `DEBUG_BREAK()`,
before the resize.

Starting from the largest sibling size rather than this framebuffer's own size
keeps every framebuffer's buffer for a CRC and type at the same doubled size
once each has grown on its own turn, even when the required size has dropped by
the time a lagging framebuffer catches up, and makes the break fire exactly
once per new largest size, not once per framebuffer and not on a rise that a
sibling's doubled size already covers. The warning carries the buffer name and
counts because `DEBUG_BREAK()`'s own log names only its call site.

Choosing the value: the author recommends starting at 1024 elements (about
13 MiB host-visible across all buffers with three framebuffers), then running
the typical-play harness session in `## Acceptance criteria` and reading the
client log for the new warning. If any fires, raise the constant to the next
power of two at or above the largest logged element count and repeat until a
run logs none. Stop and return to the user if typical play needs more than
4096 elements (about 53 MiB): beyond that a per-buffer initial capacity
becomes the better trade, and that is a decision for the user.

Exposure: client-only render path outside the CRC; no determinism, wire, save,
replay, `.pack`, shader, threading, or affinity change. It moves the dynamic
storage allocations from the first frames and cell entries to buffer creation
at startup and swapchain recreation, raises host-visible memory by the amount
above, and lets a buffer that does grow hold up to twice its required size.

Change Workflow tier: Tier 2. Trigger: scoped behavior of one subsystem's
runtime (the Graphics `BufferManager` dynamic-buffer sizing policy), with none
of the Tier 3 surfaces in `.agents/references/risk-tiers.md`.

## Critical files

- `Engine/Source/Graphics/Managers/BufferManager.cpp`
- `Engine/Source/Graphics/Managers/AGENTS.md` (`### BufferManager`, if its
  dynamic-buffer bullet needs to state the default and the break)

## In scope

- A file-scope initial-capacity constant in `BufferManager.cpp`.
- `BufferManager::CreateDynamicBuffer`: the `.iDataSize` it creates each
  framebuffer's buffer with.
- `BufferManager::ResizeDynamicBufferIfNeeded`: doubling growth from the
  largest framebuffer size for that CRC and type, the new-largest-size check,
  the warning log, and the `DEBUG_BREAK()` before the resize.
- The `### BufferManager` dynamic-buffer bullet in
  `Engine/Source/Graphics/Managers/AGENTS.md`, only as `/update-claude-docs`
  requires.

## Out of scope

- Any growth policy other than doubling; `ResizeDynamicBuffer` and its
  release and stash behavior; shrinking buffers.
- A per-buffer or per-collection initial capacity, and any change to
  `CreateDynamicBuffer`'s signature or callers, including the
  `DynamicPipelines` creation functions.
- Collection `BeginRender` callers, `DebugRender.cpp`,
  `AccumulateRenderCapacity`, and collection `iCapacity` growth.
- Swapchain-recreation handling of dynamic storage buffers.
- Skinning, particle, uniform, and other `BufferManager` buffers.
- The ImGui backend's vertex and index buffers.
- VMA allocator configuration and block sizes.

## Acceptance criteria

- `CreateDynamicBuffer` creates every framebuffer's buffer at the
  initial-capacity constant times `iElementSize`.
- `ResizeDynamicBufferIfNeeded` returns `nullptr` with no allocation when the
  buffer holds `iLayoutSize * iCapacity` bytes; otherwise it resizes to the
  largest framebuffer size for that CRC and type, doubled until it covers
  `iLayoutSize * iCapacity`, and it logs the warning and calls `DEBUG_BREAK()`
  only when that largest size did not already cover it (diff-decisive).
- Client builds compile in Debug and Release.
- A client and server run through `/agent-harness` in which fleets spawn and
  fight, and the camera crosses into neighboring cells, logs no
  dynamic-buffer growth warning, no `ASSERT`, and no Vulkan validation error,
  and collection models, lights, smoke, and wind trails still draw.
- The chosen constant value and the run that justified it are reported with
  the change.

## Notes

- The user directed a higher shared default plus a break on growth, then
  directed that growth after the break double rather than grow to exactly the
  required size.
- After a swapchain recreation every buffer restarts at the initial size, so a
  constant tuned below typical play breaks again after each recreation; the
  tuning procedure removes that case.
- `ProfileNvidiaFramePacing.md` measures frame pacing; it does not own this
  sizing policy and is not a dependency.
