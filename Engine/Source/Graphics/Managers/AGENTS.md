# Managers - Vulkan Renderer Services

Vulkan services exposed through `gp*` globals. `Graphics` constructs its managers in dependency order and destroys or resets them in reverse; `TextureUploadManager` is the exception, owned by `Main` so its thread can span graphics recreation.

## Shared Contracts

- A manager owns its `gp<ClassName>` singleton; owned-by-value helpers are reached through their manager. Constructors publish the global and destructors clear it.
- Swapchain- and screen-dependent resources use paired destroy/create phases without replacing every manager; full destruction happens only on device loss. Full device recreation also tears down and rebuilds transfer resources around the independently owned upload manager.
- Global and Main command buffers are immutable between recreation events; ImGui records per frame. Fence and semaphore ownership belongs to the submitting manager's contract below.
- Queue synchronization depends on selected queue families: uploads may transfer ownership to graphics, and present waits for graphics when present uses a distinct queue. Do not assume all queues alias or that every transition is cross-queue.
- The shared descriptor pool serves graphics and compute pipelines. Most pipelines consume global Set 0, but legacy standalone compute pipelines retain their own layouts and do not. Pipeline-specific sets and bindless arrays must remain valid through partial recreation.
- The texture-descriptor registry owns island bindless-slot metadata and descriptor registrations. Fixed bindless-array storage addresses and slot indices are registry identity. During the post-fence bindless write epoch (a time window in which bindless descriptor writes are safe), eviction redirects all five island channels to slot-0 placeholders and retires their registrations before any view is freed or slot is reused; restoration exposes elevation only after the four lazy channels are ready.
- A pipeline rebuild clears raw pipeline registrations but preserves live island-slot metadata. Registering a rebuilt bindless consumer must recreate its live per-slot descriptors so an in-flight lazy adoption still reaches that pipeline.
- Boot fails loud when the device does not advertise `VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT` (the device's promise that it can blend into a given pixel format) for every special-format render target the MAX/ADD-blended prepasses write: elevation, lighting, smoke, and wind. Blending without it is silent undefined behavior at pipeline creation, so this device dependency is hard, unlike the sampler linear-filter probe that downgrades gracefully. Adding a blended prepass on a new format must extend that guard.
- Pack-backed shaders, models, fonts, and textures are trust boundaries. Validate declared ranges, counts, dimensions, and derived byte sizes against the resident chunk before allocation or copy. Boot consumers throw `CorruptStreamException`; the texture upload thread and per-frame adoption `ASSERT` (`../../File/AGENTS.md`).

## Manager Contracts

Renderer-wide frame and recreation ordering stays in Graphics; the contracts below own manager-specific algorithms and failure modes.

### InstanceManager

Vulkan instance, physical-device selection, and the Win32 surface. Capability validation runs against the finally selected device before logical-device creation and covers the selected required core/Vulkan 1.2 features plus format-specific optimal-tiling support; `DeviceManager` must enable every feature required here.

Under RenderDoc (`renderdoc.dll` loaded), the validation layer, its `VK_EXT_layer_settings` extension, and the `VkLayerSettingsCreateInfoEXT` `pNext` chain must be dropped together; the retry path on a machine without the Vulkan SDK drops the same `pNext` plus the portability flag to stay spec-valid. `kbRenderDoc` plus a loaded DLL exposes the in-app capture API under a `%TEMP%\RenderDoc` path template, and `--renderdoc` force-loads the DLL before instance creation at the cost of validation layers.

### DeviceManager

Logical device, queues, and GPU memory allocation. Creates the device enabling every core and Vulkan 1.2 feature `InstanceManager` validated. Initializes VMA and the single descriptor pool with both `FREE_DESCRIPTOR_SET_BIT` and `UPDATE_AFTER_BIND_BIT`. Graphics, presentation, and transfer queue handles are deduplicated. Queries `VK_KHR_maintenance9` for optional queue-family ownership transfer.

Owns the one command pool and fence shared by every `OneShotCommandBuffer`. Because they are shared, two such lifetimes must never overlap; an in-use flag asserts on overlap rather than letting the two silently corrupt each other's recording.

### SwapchainManager

Swapchain creation and recreation, framebuffers, depth and multisampling textures, and frame synchronization. Out-of-date and suboptimal swapchains defer recreation. Presentation is async through a `PersistentWorker` at time-critical priority.

Owns two render passes. The scene pass (all default pipelines, plus the F16 multisample attachment) renders into an F16 (`R16G16B16A16_SFLOAT`) HDR target; a fullscreen resolve pipeline tone-maps it into the single-sample present pass, whose only client is that resolve. That preserves HDR highlights direct-to-`UNORM` rendering would clamp, and is the one hook for whole-frame tone mapping and grading.

The HDR attachments (color, depth, MSAA) are single images reused every frame in flight, so the incoming `EXTERNAL -> 0` subpass dependency must keep depth stage and access (`EARLY|LATE_FRAGMENT_TESTS` + `DEPTH_STENCIL_ATTACHMENT_WRITE`) in its src scopes against the previous frame's depth write.

### CommandBufferManager

Per-framebuffer command buffers and the submission graph. The Global and Main record helpers own compute/world preparation and scene rendering respectively. The per-framebuffer recorded flag is an idempotence guard, not a runtime re-record mechanism.

- Global may prepend texture queue-family ownership-acquire barriers, then signals Main. Main waits on both Global completion and swapchain image availability; swapchain acquisition therefore gates Main rather than Global.
- Main signals the particle semaphore consumed by the next frame's Global submission. Preserve this cross-frame edge when changing particle compute or rendering order.
- Submission workers publish work through `PersistentWorker` wake/wait edges: Main waits for Global, and UI waits for Main. Data read by a worker must be written before its wake.
- Per-frame uniform copies need no host-write barrier of their own: `Buffer::RecordCopy` supplies barriers on both sides of the copy, and `vkQueueSubmit` implicitly makes the CPU's earlier host-visible writes device-visible. That is what lets a record-once command buffer have its uniform contents rewritten every frame; do not add a per-frame `VK_ACCESS_HOST_WRITE_BIT` barrier or conclude the scheme is unsafe.
- Main resets the framebuffer fence but submits without it. The inseparable following ImGui submission signals that fence; every path that submits Main must also submit ImGui.

### BufferManager

All GPU buffers: vertex (terrain, water, models), uniform (per-framebuffer view/projection), and storage (dynamic game objects, particles, skinning). Model buffers are indexed by CRC.

- Collections register storage buffers during `CreatePipelines()` through the CRC-keyed dynamic buffer system with three categories (main, visible lights, wind deposit). Access is templated with runtime size validation, and automatic resizing defers destruction to avoid validation errors from in-flight command buffers.
- Skinning bump-allocates mesh data and joint matrices per frame with per-command-buffer offset tracking, auto-grows by doubling, and propagates descriptor updates to model pipelines. Joint matrices use a compact 3-row format (48 bytes) since the fourth row is always identity. Multiple grows within one frame are safe: buffers are per-framebuffer, the top-of-frame per-framebuffer fence wait drains any prior submission before a grow runs, and record-once command buffers reach skinning data only through descriptor sets the grow repoints.
- Smoke and wind share one occupancy + active-tile-list pattern for hierarchical indirect dispatch. Smoke pairs one bit-packed occupancy buffer with each ping-pong texture and shares one active-tile list between both spread halves: deposits mark the first texture's occupancy; each spread reads its input occupancy, consumes its output texture's prior occupancy as a stale-storage union term, resets the output occupancy, and re-marks nonzero output. Wind keeps a separate occupancy and active-tile-list pair per ping-pong texture, with both spread variants recorded and runtime state plus indirect dispatch selecting useful work. Occupancy describes persistent texture contents, so every occupancy buffer starts at zero.
- Swapchain recreation tears down and rebuilds only per-framebuffer buffers, preserving static geometry.

### TextureManager

Loaded textures, global texture descriptors, cached generated textures, and renderer targets. Lazy textures begin on a white placeholder and enter the bounded per-frame adoption path after a load request. Non-indirect pipelines request at creation, indirect pipelines defer until their first positive instance write, and priority textures request at boot.

- Swapchain recreation selectively rebuilds screen-dependent targets and descriptor sets while preserving loaded textures.
- Explicit repeat/clamp samplers cover authored/generated deposit lookup and visible-light textures. Unflagged combined samplers, explicit offscreen linear variants, and dedicated smoke/wind variants all omit anisotropy; internal render-target reads remain non-anisotropic. Format-specific elevation, model-material, and water-normal variants remain separate.
- Light-type textures consume a fixed reservation of bindless slots for pre-blurred results. Grow that reservation if the registered lighting-texture count exceeds its capacity.
- A lighting texture's pre-blurred result is found under its own CRC salted with `kBlurSalt`, so the blur write site and every read site agree without a second lookup table. `ReblurAllLightingTextures` re-runs the blur for every ready lighting texture after a change that invalidates the results.
- Three owned-by-value units carry the detail: `TextureDescriptors` (global descriptor Set 0, the bindless texture array, per-pipeline binding tracking, deferred descriptor updates), `TextureCache` (GPU-to-CPU readback, the on-disk cache of generated textures, PBR BRDF lookup tables), and `RenderTargetTextures` (creation and sizing of effect render targets).
- The on-disk cache is versioned: a cached file is reused only when its magic number, version, format, extent, mip count, layer count, and source CRC all match the request, and the version is stamped on write. Any newly generated texture reuses this cache rather than regenerating every boot or adding a second caching path.

Water's variance-table consumption is documented in Water shaders (`../../../Data/Shaders/Water/AGENTS.md`).

### TextureUploadManager

A dedicated upload thread with a fixed staging budget, persisting large-texture progress across frames. The budget caps size, not rate: the render loop signals the thread once per frame and the thread waits for a signal before each chunk, so uploads spread across frames instead of bursting. Block-compressed partial copies support BC4, BC5, and BC7. Queue selection prefers transfer-only work but resolves to foreground adoption when transfer aliases graphics or the distinct present family, so concurrent submissions do not share that queue.

- Distinct transfer queues release image ownership for the matching graphics acquire; maintenance9-capable devices may use the simplified path. Device loss preserves CPU data for re-upload.
- `WaitIdle` is a teardown drain handshake: the upload thread acknowledges only from a no-submit-in-flight point, and every exit path publishes exit state so a waiter cannot deadlock. Frame permits remain binary and must be drained before release. Fatal upload failures are published for the main thread and survive transfer-resource recreation; device loss remains a separate recovery path.
- Pack texture headers and derived copy ranges are validated before the upload thread may hand work to foreground adoption, and again before foreground fallback allocation or copy: positive signed dimensions and mip counts, the pack ceilings and a realizable mip chain, the selected device's limits for that exact image format, square cubemaps, and enough resident bytes for every layer and mip. Upload-thread failures travel the published fatal path. Other upload failures leave active ownership intact for that path.
- The pending-adoption counter belongs to this manager because it outlives `TextureManager` during device recreation. Keep file-state rearming and adoption completion synchronized with that counter.

### PipelineManager

Loads SPIR-V from pack chunks and owns fixed engine pipelines plus CRC-keyed dynamic collection pipelines. Pipelines retain pointers into the manager's stable shader map.

- Fixed pipelines implement renderer passes and utility compute work. Lighting follows deposit, spread, combine, then temporal accumulation; separable blur pipelines pre-process registered light textures.
- Collections register dynamic pipelines by behavior and model role; these maps repopulate lazily after pipeline-tier recreation, so a new renderable collection must use the established idempotent registration path rather than requiring command-buffer re-recording per scene CRC.
- Pipeline construction registers texture consumers with the texture-descriptor registry, which owns descriptor-generation verification.

### ParticleManager

Stages CPU particle spawns for fixed-capacity GPU compute allocation and simulation. Worker spawns first cull by visible area and intensity, then append under the spawn mutex during joined frame-tick work. `RenderGlobal` runs after the worker join, copies staged spawns into the current framebuffer's mapped storage, and clears CPU staging. Bindless texture-index assignment mutates descriptor bookkeeping while spawning; its safety relies on this tick/render phase exclusion. Keep that exclusion if spawn or descriptor work moves between phases.

### ImGuiManager

Integrates Dear ImGui with a dedicated load-preserving overlay pass. It records the UI command buffer each frame, waits for Main, signals presentation, and signals the per-framebuffer fence as the frame's final graphics submission.

- Owns the engine standard menu screens plus the game HUD and TweaksScreen objects, the latter being the sanctioned engine-to-game dependency exception described by the Engine hub. Standard screens receive `GameBase&`; game screens are called through their own signatures. Screen behavior belongs to UI documentation (`../../Ui/Screens/AGENTS.md`).
- Theme geometry resets to ImGui defaults before scaling so it can be reapplied safely. Font scale composes framebuffer and user scaling. UI dimensions use a 2160-pixel reference height through `UiScale()`, a repository-wide convention: every authored pixel constant in engine and game UI code is written for a 2160-high screen and multiplied by `UiScale()` at use, under authoring rules owned by the engine screens hub (`../../Ui/Screens/AGENTS.md`).
- Opaque-region registration is rectangular, so rounded opaque windows must remain visually compatible with rectangular scene occlusion.
- The Win32 backend publishes physical cursor state during frame preparation. Synthetic agent mouse position is reissued afterward so it wins last-writer ordering. Physical-input suppression parks the cursor unless an agent pin owns it and always clears backend gamepad navigation state.
- One default font, loaded from a pack chunk at boot.
