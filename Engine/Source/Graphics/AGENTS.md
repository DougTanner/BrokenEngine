# Graphics - Vulkan Rendering System

Client-only multi-pass Vulkan renderer. `gpGraphics` owns renderer-wide creation, frame sequencing, recreation, world-area projection, and capture; Managers (`Managers/AGENTS.md`) own shared Vulkan resource and synchronization contracts, while Objects (`Objects/AGENTS.md`) own individual RAII handles.

## Frame and Resource Lifecycle

- Frames submit Global, preparation, Main, and ImGui work before present. The frame boundary is between Main submission and the next acquire.
- Global and Main command buffers are recorded once and rebuilt only by the recreation pipeline. Per-frame variation belongs in host-visible buffers, descriptors, and indirect commands; resources discovered within existing capacity must not depend on a re-record.
- Model depth ordering is runtime host policy in `Graphics::mFlags`, initialized
  from the host's `kbDrawModelsOverWorld` default. Change it only on the owning
  main thread and request at least command-buffer recreation; complete that
  recreation before publishing or submitting a frame for the new policy. Never
  mutate the policy concurrently with command-buffer recording.
- `RenderGlobal` waits only its own framebuffer fence, then pre-scans
  descriptor churn. When island eviction, lazy adoption, or restoration is
  pending, or that framebuffer has unapplied descriptor publications, it opens
  the bindless write epoch (a time window in which bindless descriptor writes
  are safe) for that framebuffer alone, performs those mutations, ends the
  acquire command buffer, and publishes into that framebuffer's sets; a
  completion that races the scan waits for the next frame. Descriptor writes
  reach only framebuffers whose fence has signaled; only drained contexts (the
  texture waits, and `Graphics::Create` over a surviving texture manager) write
  every framebuffer's sets. Every frame `RenderGlobal` also publishes to, and
  marks waited, each other framebuffer whose fence already reads signaled,
  without blocking.
- Staging buffers, evicted images, and evicted mesh ranges that recorded work or descriptors may still use are retired, not destroyed. Every frame, outside the churn gate, each waited framebuffer leaves every retirement entry, and after that frame's publications an entry no framebuffer holds is destroyed; `Graphics::Destroy` destroys every entry after its device drain, at every tier.
- The acquire command buffer, prepended to Global, carries queue-family acquire barriers and recorded uploads; its work precedes any descriptor or indirect record exposing the result. It is single-publication work: clear prior publication before the churn scan, end it at most once per frame after the last recording, and let only fresh recording republish it. An idle frame must not replay prior barriers.
- `CheckVkFailed` handles every failed `CHECK_VK` result: out-of-date and
  suboptimal swapchain results escalate to the swapchain tier, surface loss to
  the surface tier, and `VK_ERROR_DEVICE_LOST` throws `std::system_error` with
  `VkErrorCode` carrying the raw Vulkan result. `SwapchainManager`'s acquire and
  present paths must inspect their results directly, so they apply the same
  swapchain-tier escalation themselves and pass every other result to
  `CHECK_VK`. The main loop and upload thread recover only when both category
  and value exactly match device loss; every other system error stays fatal.
  Only the fatal unexpected-result path breaks into the debugger; the
  recoverable results deliberately do not, so a routine window resize does not
  halt every debug session. New Vulkan call sites go through `CHECK_VK` instead
  of repeating or bypassing that escalation.
- Destroy tiers are monotonic within a frame. Selective texture, sampler,
  command-buffer, pipeline, swapchain, and surface rebuilds preserve lower-tier
  resources; full teardown drains the pack loaders, then waits for uploads and
  device work, before destroying dependencies and resetting texture chunk state;
  that order is required (`../File/AGENTS.md`). `Graphics::Destroy` only tears
  down, at every tier. Destroying `Graphics` tears down at the surface tier.
  For a surviving tier, `Graphics::Create` rebuilds flagged texture and buffer
  resources and, at the sampler tier and above, the texture manager's samplers;
  destroy flags stay set across both swapchain deferrals.
- `Graphics::Create` constructs the texture manager before the pipeline manager, then initializes boot textures after both managers exist.
- A zero-area swapchain extent defers swapchain-tier recreation. The render loop
  skips submission, retries creation at sim-tick cadence, and acquires again
  only after recreation succeeds; surface-loss recovery remains a full recreate.
  The simulation keeps ticking while rendering is skipped, so client state the
  render path normally drains or paces is drained or paced on every skipped
  iteration in `GameBase::Render`'s skip path (staged particle spawns, the
  texture-upload frame signal), and render-frame-paced accumulators reset once
  in `GameBase::HandleDeferredSwapchain`'s resume branch (profiler frame
  samples, `mRenderFrameTimer`).
- Every source in this subtree is whole-file `BT_CLIENT`-guarded and client-project-only. Shared simulation code that touches client graphics state needs its own `BT_CLIENT` guard.

## World and Camera Contracts

- `engine::CameraBase` (`CameraBase.h`) owns the whole generic camera: matrices,
  world/screen projection, visible-area LOD, grid-snapped render extents, and
  the display-rate update — target resolution, jump/chase, Hermite zoom,
  texel-framing references, shake, and sun angle. `CameraBase` also holds the
  world-space model sun rotation, identity unless the host writes it. It affects
  only the model direction after Normal Tilt; world lighting and shadow products
  retain the ordinary direction. It registers and clears the single
  `engine::gpCamera` global.
  Game code derives one final `game::Camera` from it, the only camera ever
  constructed, and supplies only policy — `PullTarget` and `SunAngle` — as
  non-virtual members the engine calls by name through a `static_cast` to
  `game::Camera`. `CameraBase::Update` names no game type: the gameplay host's
  `GameBase::UpdateCamera` reads the main menu from `GameBase::mGameFlags`,
  pulls the target, and passes both with the interpolate frame, the
  sim-scaled render delta, and the render clock's advance for the sun. The
  camera also owns the reconciliation visual-error offset, client render state
  outside the CRC: `AddVisualErrorOffset`
  accumulates it, and the update decays it after `PullTarget` has read it. A
  tracked target's raw position updates the tracking velocity and last-known
  position; the offset is added only to the resolved target. The engine places
  the initial position and every resolved target on the
  lowest flight plane, so the look-at always lies on it and a target's height
  never moves the eye or the visible area; `PullTarget` supplies no height
  policy.
- Camera position, easing, shake, and visual timers advance from the sim-scaled render delta the caller passes to `CameraBase::Update` (wall delta multiplied by the active time ratio; equal to wall time at ratio 1.0). Sun angle advances from the render clock's per-frame advance the caller passes separately, so it stops whenever on-screen game time stops, and pauses in the main menu. Debug free-camera motion is main-menu-only and drives the pose directly, ignoring the pulled target.
- Long-distance target changes use a bounded smoothstep jump that may re-anchor from the current position; normal tracking tightens with framing. Mouse-wheel input updates a persisted, clamped framing target, with framing and velocity re-anchored while scrolling so repeated input stays smooth; the gameplay zoom ceiling is not a texel-grid boundary. Damage-driven shake decays here; after the update, `GameBase::UpdateCamera` sets controller vibration from the decayed shake.
- The camera works entirely in the local frame of the cell it is rendering,
  building its presentation basis with `MakeRenderBasis` (`CameraBase.h`). The
  rebasing rule every client conversion point follows is in Frame
  (`../Frame/AGENTS.md`). A rendered-cell change to an edge neighbour
  (`GridDistance` at most 1) shifts every cached camera position by the
  whole-cell centre offset (`CellCenterOffset`); a larger change discards the
  tracking caches. A new cached position must join
  that shift or it will describe a place in the previous cell.
- Visible-area edges snap to the water quad grid. Zoom and LOD use hysteretic buckets so quad size remains stable across adjacent frames; terrain, water, and shadow areas must preserve this shared grid contract.
- The zoom value is the framing (`mfCameraFraming`): the eye height above the
  look-at that would show the current view at `kfFramingFieldOfView`, so every
  field of view and projection shows the same look-at-plane extent.
  `CalculateMatricesAndVisibleArea` derives the eye distance from framing and
  the true vertical field of view; orthographic projection
  (`mbOrthographicProjection`, latched there once per update) uses a virtual
  eye at the default field of view. Readers that measure zoom — LOD, zoom
  buckets, texel references, height-lerped tunables, audio blends — read
  framing, never the eye distance. Field of view and projection mode join the
  visible-area latch key because they move the ground area at a fixed framing
  while the zoom bucket does not.
- Comparison pose/projection is render-only and updates even without an
  interpolate frame. `GetRenderFraming` returns ordinary framing or the fixed
  comparison camera's eye height; Main publishes comparison matrices with zero
  shake, including empty-frame paths. Comparison uses its explicit perspective
  projection and eye for water shading.
- Shadow texel size tracks a framing reference that expands immediately with
  outward zoom and contracts at its existing rate inward. The reference never
  falls below live framing. Shadow history rejects reprojected texels outside
  the previous footprint and follows the terrain shadow cadence unless crop
  safety requires an earlier refresh; recreation re-seeds current data.
- Terrain rendering uses boot-fixed island templates, a stable 128 MiB mesh
  arena, and a per-framebuffer placement arena derived from the active-cell
  subscription slots plus one local unconfirmed cell and the 107-placement cell
  contract. The record-once terrain command buffer binds the mesh arena once;
  each acquired framebuffer's indirect records, rewritten every frame from
  residency state, provide each template's mesh subrange (zero unless
  resident) plus the current per-template run offset and mesh-visible count,
  with each run contiguous and mesh-visible instances first, while the
  elevation and shadow prepasses consume the full fixed arena. Clear the stale
  placement tail before publishing a shorter run. A restored mesh is copied
  through the acquire command buffer, and an evicted template's subranges are
  retired, so a pending frame's stale indirect record never draws from a
  reused range. Residency never replaces the mesh-buffer handle or re-records.
  Arena teardown invalidates every suballocation, while File-owned
  asynchronous range state survives.
- The island least-recently-used grace period counts rendered frames, not sim ticks or wall time. Both the frame counter and each template's last-used stamp advance only inside the render path, so a frame that skips rendering ages nothing and cannot evict an island early.
- Render-target sizes derive from a fixed 3840-pixel reference width, not the
  live window. Smoke simulation resolution scales against that reference, and
  the water vertex grid anchors to a fixed 3840x2160: Gerstner wave frequencies
  are fixed, so letting the water grid follow the live extent (the obvious
  cleanup, since the general detail size does) would change wave appearance per
  monitor. Shadow sizes snap to the shared detail block and hold their width at
  two thirds of the device's `maxImageDimension2D` (the largest 2D image the
  device allows) so the 1.5x-wide derived elevation texture still fits; raising
  shadow quality or adding a wider derived variant without that cap fails image
  creation only on lower-limit devices.
- Terrain collision belongs to shared Frame island terrain (`../Frame/AGENTS.md`), not this client renderer.

## Lighting Publication

- Graphics owns client-only Lighting state outside Frame and CRC. Begin/end
  publication runs for every acquired framebuffer, including ModelViewer and
  empty gameplay frames. Gameplay Frame arguments are borrowed synchronously;
  do not retain Frame/collection pointers, capture them in workers, or copy CPU
  geometry and matrix snapshots. Gameplay source publication is a zero stub.
- Model publication records owner identities and ranges after ordinary render
  writes, including off-screen opaque instances after the visible prefix.
  Resolve current GPU handles after capacity growth and reuse final mesh/joint
  ranges. Resident terrain/model geometry, material tables, and acceleration
  structures remain tied to the framebuffer's publication and resource lifetime.
- The terrain producer shares one compute pipeline across framebuffers, with
  descriptor sets indexed by framebuffer; its buffers and acceleration
  structures remain per framebuffer. Each frame refreshes only the current
  framebuffer's work-buffer binding, after growth and before use. Destroy the
  shared pipeline before its backing resources in both explicit teardown and
  member destruction.
- Preparation completes lighting and environmental updates before receivers or Main.
  A recorded publication is not completed GPU work: `BeginRender` latches the
  published counts only after that framebuffer's covering fence completes.
  Submission ownership belongs to
  [CommandBufferManager](Managers/AGENTS.md#commandbuffermanager).
- Lighting owns comparison capture. Settings changes apply only after draining
  render, present and upload workers and device work, before the next CPU
  publication. A capture requires visible Tweaks and advances inside ordinary
  frames while the client is paused. Hiding Tweaks fails an active capture
  before publication; queued requests consumed while hidden are rejected and
  require a fresh request after Tweaks becomes visible. During receivers,
  tracing and composing, Tweaks disables Sun Angle
  and non-Lighting section contents to preserve receiver inputs; Lighting
  controls and navigation remain available. One frame renders the receivers,
  each later preparation traces a fixed number of reference tiles,
  and only the frame that traces the last tiles enables the reference for its
  own Main composition. Each tracing frame must publish the receivers frame's
  counts. Unpause, a settings change, a changed publication or Graphics
  recreation sets capture status `failed` with a reason, and recreation never
  resets that status.
- The fixed client fixture supplies immutable sources, samples, geometry and
  events without consuming simulation IDs or RNG. Physical emission is authored
  independently of visible sprites, shield colors, and smoke appearance.

## Assets and Capture

Comparison export runs after the composing frame's UI submission and waits that
framebuffer's fence before reading back the raw receivers and reference and
writing the finished PNG. It then disables the reference, so later frames
compose zero local diffuse. `capture.json` records only the capture settings,
wall time and device; the source commit identifies the fixture and code.

- Animation aliases initialize before Graphics creation so record-once fixture draws can resolve them. They alias eagerly loaded pack memory and therefore cannot outlive `FileManager`. Validate all pack-derived ranges and indices before constructing aliases; temporary transforms use the workbuffer.
- DirectXMath matrices are uploaded without an extra transpose. Animation nodes remain parent-before-child, enabling a single forward world-transform pass.
- Screenshot and render-target capture run after the ImGui submission is enqueued and before present. The fence signal must already be pending before readback waits, or capture deadlocks. Requests enter through `Graphics` mailboxes; asynchronous encoders own separate thread-local state.

## See Also

- Managers (`Managers/AGENTS.md`) - Cross-manager lifecycle, synchronization, and resources
- Objects (`Objects/AGENTS.md`) - Vulkan object ownership
- Render (`Render/AGENTS.md`) - Per-frame buffer publication and pass ordering
- Debug (`Debug/AGENTS.md`) - Debug rendering
