<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:50:15.139Z","dependsOn":[]} -->
# Publish the Camera Block on RenderFrameMain's No-Camera Path

## Context

Found by the architecture review phase of a deep analysis of
`Engine/Source/Graphics/Render` (the directory, not its subdirectories). Line
numbers are from commit `c675b6b2`, except those of `R/MainUniforms.cpp`,
which are from the commit that added this Plan; locate by symbol if they
moved. `R/` abbreviates `Engine/Source/Graphics/Render/`.

`RenderFrameMain` (`R/MainUniforms.cpp:234`) returns early at `:240-255` when
the active set is empty or the camera coordinate has no render interpolate.
The return comes before `:311-341`, so on that path these `MainLayout` fields
are not written: `iFrameNumber`, `iRenderNumber`, `f4x4ViewProjection`,
`f4x4ModelViewProjection`, `f4EyePosition`, `f4WaterEyePosition`,
`f4ModelEyePosition`, `f4ToEyeNormal`, `f4BillboardRight`, `f4BillboardUp`,
and every field `PopulateHexShield` writes. Main is still submitted
(`Engine/Source/Graphics/Graphics.cpp:230-235`).

What the path really is, from code reading (nothing was run live):

- Only two callers reach the engine `RenderFrameMain`, both through
  `Graphics.cpp:230`: `GameBase::Render` and the boot prerender in `Main.cpp`.
  ModelViewer defines its own `engine::RenderFrameMain`
  (`Projects/ModelViewer/Source/ModelViewerApplication.cpp:1484`).
- `SelectRenderCamera` falls back to the first renderable active coordinate
  (`Engine/Source/GameBase.cpp:739-753`), and `UpdateRenderInterpolation`
  prunes to renderable active coordinates and interpolates the camera
  coordinate whenever one exists (`GameBase.cpp:812-815`, `:843-846`). So a
  missing camera interpolate means "empty active set, or no active coordinate
  is renderable". No state has a missing camera interpolate together with
  another renderable active cell.
- The path is reached in ordinary play: on every accepted connection from the
  main menu (`ResetSession` clears `mActiveCoordinates` and returns before
  re-adding the client cell, `GameBase.cpp:1206-1211`, and
  `ComputeActiveSet` admits only terrain-prepared cells with a populated ring,
  `Engine/Source/Network/Client/ClientSessionRuntime.cpp:915`); on a rejected
  or failed connect from the main menu (`ClientSessionRuntime.cpp:352`,
  `:446`, `:451` call `Disconnect`, which resets every cell's ring,
  `Engine/Source/Network/Client/ClientSessionBase.cpp:79-82`, while the menu
  active set still names the client cell, `ClientSessionRuntime.cpp:983-984`);
  and on the server-load reset.

Two comment sentences at `R/MainUniforms.cpp:243-244` are therefore false:
"the empty-set check is defensive" at `:243` (an empty active set happens on
every accepted connection) and "A missing camera interpolate can coexist with
other renderable cells" at `:244` (no caller produces that state).
`R/AGENTS.md` `## Ordering and Publication` describes the begin/end
publication of this path but does not say the camera block and hex-shield
fields are held, not published.

Readers of the unwritten fields with nonzero work on this path: only the two
GPU particle draws
(`Engine/Data/Shaders/Particles/LongParticlesRender.vert:45,80` and
`SquareParticlesRender.vert`), whose indirect instance count is written on
the GPU from the live particle count
(`Engine/Data/Shaders/Particles/ParticlesSpawn.comp:130-131`) and is not
zeroed by Main. Water is written 0 (`R/MainUniforms.cpp:242`) and the other
draws are zeroed by begin/end publication. On the connect and server-load
entries particles are reset in the same frame, so only the failed-connect
entry leaves live particles drawing with stale fields.

Size of that defect: each framebuffer's `MainLayout` then holds the matrix of
its own last full frame, a few frames of camera motion old, for particles
only. `MainLayout` buffers are per framebuffer and recreated with the
swapchain (`Engine/Source/Graphics/Managers/BufferManager.cpp:263`), and
freshly allocated Vulkan memory has undefined contents (the Vulkan
specification: "When memory is allocated, its contents are undefined";
`VMA_DEBUG_INITIALIZE_ALLOCATIONS` is not defined in this repository), so a
swapchain rebuild followed by no-camera frames with live particles reads
undefined values. No visible defect was observed; this is derived.

Not traced: how and when the ring refills after a failed connect.

Unmet criterion: the no-renderable-coordinate comments name only states a
caller produces, and a frame on that path writes the camera block and
hex-shield fields of the `MainLayout` it submits.

## Design

Decided by the user on 2026-11-14, in answer to the authoring session's
question (this is the authoring agent's record of that answer, not the user's
words): correct the two false comment sentences and move the camera
publication ahead of the early return, so the camera fields of `MainLayout`
are written every frame. A documentation-only change that leaves the fields
held was the alternative and is not an option.

1. Comments. Replace the two false sentences at `R/MainUniforms.cpp:243-244`
   with the reachable case: an empty active set, or no renderable active
   coordinate, so no per-coordinate render runs. Reword `:245` as well, because
   after step 2 no statement below the return reads the camera interpolate.
2. Move. Move `R/MainUniforms.cpp:311-341` (the frame and render numbers, the
   shake matrix with its `static` constants and noise tables, the view
   projection, the eye and billboard vectors, and the `PopulateHexShield`
   call) to sit after `RenderLightingMain` at `:238` and before the `if` at
   `:240`.
3. Shake time. `fShakeTime` at `:322` reads the camera interpolate's `iTick`,
   which the no-camera path does not have. Author's recommendation: read
   `engine::gpCamera->miFrame` there instead, on both paths, with no branch.
   Rationale: `CameraBase::Update` copies the camera interpolate's `iTick`
   into `miFrame` (`Engine/Source/Graphics/CameraBase.cpp:171`), and both
   callers run it on that interpolate before `RenderFrameMain` in the same
   frame (`GameBase::UpdateCamera` at `Engine/Source/GameBase.cpp:874`, then
   `:1017` with the same coordinate; `Engine/Source/Main.cpp:348` before the
   boot loop, which does not rewrite the interpolate). So the value is the
   same `int64_t` whenever a camera interpolate exists, and the normal path
   does not change. On the no-camera path `UpdateCamera` is skipped
   (`GameBase.cpp:870-875`), so `miFrame` and `mfShake` hold their last values
   and the shake rotation is the last one published. `iFrameNumber` at `:311`
   already reads `miFrame`.
4. Leftover names. `rFrameInterpolate` at `:309` and `rCameraInterpolate` at
   `:257` then have no reader (`:309` is the only use of `:257`, `:322` the
   only use of `:309`); delete both declarations.
5. `R/AGENTS.md` `## Ordering and Publication`, the "Gameplay main rendering"
   bullet: add one sentence saying the camera block and hex-shield fields are
   published on every path, from the camera's held state when no coordinate
   is renderable.

Checked against the tree, so the executing session does not need to carry
anything else across the return:

- Every other operand of the moved block comes from `gpCamera`, `gpGraphics`,
  or tunables. `rMainLayout` is bound at `:236`.
- Nothing between `:240` and `:307` writes what the block reads. `mfShake` is
  written only by `CameraBase::Update` (`CameraBase.cpp:169`) and by
  `Game::UpdateCameraShake`
  (`Projects/BrokenEngineSandbox/Source/Game.cpp:112`), which the client tick
  path calls (`GameBase.cpp:1163`), not render. `Graphics::miFrameCounter`
  advances in `RenderGlobal`, before Main. The private game was not searched.
- `ResetSkinningAllocations` at `:259` and everything else between `:257` and
  `:307` stay below the return, unmoved; the block uses no skinning
  allocation.
- The camera is valid on the no-camera path: `Main.cpp:348` runs
  `UpdateCamera` once before the first `RenderFrameMain`, and Global already
  publishes from the same held camera on that path (`GameBase.cpp:1013` is
  unconditional).

If the executing session finds a reader or writer this list missed, it
surfaces it for re-planning instead of adding a branch or a second time
source.

Rider, in the same early-return block: remove the
`if constexpr (kbDebugRender)` wrapper at `R/MainUniforms.cpp:249-250,253`,
keeping the two calls. `DebugRender::BeginRender` and `EndRender` guard their
whole bodies with the same `if constexpr`
(`Engine/Source/Graphics/Debug/DebugRender.cpp:117,139`), and the normal path
calls the pair unwrapped at `R/MainUniforms.cpp:306-307`. No behavior change
in any configuration.

## Critical files

- `Engine/Source/Graphics/Render/MainUniforms.cpp`
- `Engine/Source/Graphics/Render/AGENTS.md`

## In scope

- `RenderFrameMain`: the comment at `:243-245`, the `kbDebugRender` wrapper at
  `:249-253`, the position of the block at `:311-341`, its shake-time operand
  at `:322`, and the two declarations at `:257` and `:309`.
- One sentence in `R/AGENTS.md` `## Ordering and Publication`, in the
  "Gameplay main rendering" bullet.

## Out of scope

- The active-set, connect, disconnect, and interpolation code in
  `GameBase.cpp` and `Network/Client/`, including the stale comment at
  `GameBase.cpp:810-811`.
- Particle reset or particle indirect counts.
- Initializing or clearing `MainLayout` buffers at creation.
- The Explosions counter statements in `BeginFrameInterpolateRender` and
  `EndFrameInterpolateRender`.
- ModelViewer's `RenderFrameMain`.

## Risk tier

Tier 2. Trigger: scoped behavior of one subsystem — which `MainLayout`
fields a no-camera frame writes, in one client-only publication path. No
layout field changes, so no shader repack. Render is outside the CRC; no
determinism, serialization, wire, replay, or threading exposure.

## Acceptance criteria

- In `RenderFrameMain` the camera block precedes the early return, and the
  `rCameraInterpolate` declaration and its alias are gone.
- The comment in the early-return block names only states a caller produces,
  and `R/AGENTS.md` says the camera block and hex-shield fields are published
  on that path, matching the code.
- `/compile` builds the BrokenEngineSandbox client in Debug and Release (the
  rider changes Debug code).
- Through `/agent-harness`: a gameplay screenshot after a connect shows the
  world, models, and particles placed as before the change, and a failed
  connect from the main menu leaves the menu scene drawn with no misplaced
  particles. The evidence behind this Plan records no harness setup for the
  failed connect.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

- `Documents/Plans/Engine/ExplosionsOwnsItsProfileCounter.md` edits other
  lines of the same file; no ordering is needed.
- The stale-field defect in `## Context` was derived from code, not observed,
  so the failed-connect check is a no-regression check, not a reproduction.
