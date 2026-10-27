# ModelViewer

## Application contracts

ModelViewer is a separate client executable. Viewer policy and game contracts
belong here; shared rendering mechanisms remain engine-owned. The selected scene
draws over a seeded terrain and water background by default. Disabling World
background switches to the original HDR gray clear color, graded by current controls
([Top-Level Shaders](../../Engine/Data/Shaders/AGENTS.md#top-level-shaders)).
World background, Seed and Reroll are viewer-owned, unsaved controls outside
presets and Q comparison. Seed edits and Reroll remain usable with the world
hidden; enabling it restores that layout. Startup uses the origin seed with
World background enabled.

Startup shares the Sandbox client's native instance mutex under
[Engine single-instance policy](../../Engine/Source/AGENTS.md#crash-reporting).
Acquire it before constructing rendering and file managers; refusal exits before
their construction, with no dialog in an agent launch.

Preserve ordinary Graphics ownership, upload acquisition, descriptor publication,
submission fences and resource retirement. Selected identity and viewer controls
must survive Graphics recreation, with resources republished before use.

Holding Q temporarily applies engine defaults to the 20 right-hand float
sliders and disables their edits. Preserve the current tweaks across Graphics
recreation and restore them on Q release, focus loss, or shutdown before saving
settings. Preset selection or reset while Q is held updates what will be restored;
comparison never saves a preset or changes its selection. Sun angle and the left
Render quality controls remain outside comparison, presets and Backspace reset;
quality persists through ordinary graphics settings. Model Data Mip Bias remains
a lighting participant. Model diffuse IBL multiplies the shared ambient color
directly; sun/moon IBL specular remains independent. One ordered descriptor table
owns both lighting UI and
preset/comparison membership.

W/E/R taps load independent presets for those same 20 sliders; edits auto-save
only the selected preset. Missing or invalid presets apply all compiled defaults,
without writing on selection. Backspace applies compiled defaults and saves them
to the selected preset, retaining selection. Startup loads ordinary graphics
settings, then selects and loads W before Graphics construction; a preset is
always selected. Selection survives Graphics recreation. The right window title
shows Preset: followed by the selected key, with a stable ImGui window ID and no
duplicate content header. Ignore slot/reset key repeats and apply taps in
received order before rendering.

Presets use separate viewer-owned v3 AppData files through the shared atomic
file helpers. Unsupported versions are rejected without migration and use the
same compiled-default fallback. Validate the complete payload before applying
any value; participant order is part of the format and changes require its
version bump.
Only slider edits and explicit selected-preset resets write preset files.

Bare F4 uses the ordinary window-close path, restoring comparison before saving
graphics settings and releasing owners. Modified F4 adds no viewer close binding;
Alt-F4 retains native window handling.

F9 permits one outstanding PNG capture. Successful completion is logged only
from the matching engine capture result after the PNG write; queued requests
survive Graphics replacement.

The catalog includes every packed scene with materials, sorted by packed path.
The viewer owns clip selection, playback time and play/pause state; selected
clips use the engine's indexed pose evaluation. Rest pose uses cached authored
node matrices and skinning; an animation's first key is not a rest-pose
substitute. Camera fitting uses bounds from that rest geometry. Model changes
reset to Rest pose and refit the camera; clip changes restart playback without
refitting. UI mouse capture takes precedence over camera and sun gestures.

Left drags rotate the camera; right drags rotate only the model sun about the
current camera's screen axes with the same continuous feel. Both allow edge roll and cross poles
without clamping. Orientation accumulates along the drag path; returning
the pointer to its starting position need not restore the starting orientation.
Pointer coordinates use the viewport's shorter dimension so non-square viewports
preserve circular rotation. The initiating button owns the gesture until release
or cancellation; a held secondary button cannot take over without a fresh click.
UI cancellation also requires a fresh click before dragging resumes.

With `--agent-port`, the viewer remains hidden and suppresses startup error
dialogs; it does not start generic agent transport.
Optional `--scene-path` selects an exact packed catalog path before the first
render; an empty, missing or unknown identity fails startup. Omission retains
the first entry in the sorted catalog.
Programmatic pointer positions and left/right-button transitions enter ImGui's
public input queue through viewer-private messages without physical mouse-leave
tracking or native mouse capture. Preserve ordinary native input forwarding.

Agent testing shares the PC with an active human. Automated verification must
not activate or raise the viewer, steal focus or mouse capture, move the physical
pointer, or inject global desktop keys or clicks. Send inputs programmatically
to the viewer; automated pointer testing requires a verified nonactivating launch
and noncapturing input route. Explicit human-requested launches remain maximized
and foreground.

## Engine integration

The viewer compiles only engine files whose game names it supplies: game name
and version, HDR clear color and UI reference height in `Game.h`, the
flight-plane table, profile rows, `game::FrameInterpolate`, and
`game::Camera::SunAngle`. Never satisfy an unused gameplay dependency with
dummy cells, flags, sessions or reporting functions; the viewer constructs no
`GameBase` or network session. By project membership it defines
`engine::RenderFrameMain` for Main uniforms and the three UI functions
`ImGuiManager` calls: `RenderUiScreens` draws the viewer panel, and
`CreateUiScreens` and `DestroyUiScreens` are empty. The viewer `Pch.h` sets
`kbAudioStreamingHarnessRig` and `kbProfileScreens` to `false` and
`kbDrawModelsOverWorld` to `true`. Keep ordinary
engine profiling collection, Graphics lifecycle and local crash reporting
without network upload.
The viewer's `CameraBase` state is a fixed top-down world camera at the default
engine height, centered on the generated Huge anchor. The seed only selects the
layout; its owning cell stays at the origin without a game session. Publish one
cell when World background is enabled and an empty span when disabled, once per
acquired framebuffer after camera update and before RenderGlobal. Preserve the
seed, mode and model state through Graphics recreation. The viewer owns a separate
model orbit pose that reaches only model draws; sun dragging uses that orbit
view's screen axes. The viewer supplies
a presentation frame and sun input, and writes `mModelSunRotation` inherited
from `engine::CameraBase`. Model
illumination uses the game's shared sun/day cycle, ambient and model skybox
environment for equal sun input and shared defaults. Right-drag
sun rotation changes only the model direction and persists in world space across
Sun angle edits, model changes and Graphics recreation. Sun angle still drives
shared day-cycle colors and brightness and the base direction for world and model.
The viewer publishes no native water source grid, so startup selects Low water
quality after graphics settings load, before Graphics construction; preserve
that policy across recreation.
Preview isolation excludes world lighting deposits, smoke and terrain occlusion
from model shading. Shared sun and ambient controls affect the world; IBL and
BRDF controls remain model-only. HDR grading affects both world and gray modes.
Auxiliary world render targets retain their normal initialization and transitions;
the image pass retains its HDR gray and depth clears in both modes. Main uniforms
publish water when enabled and explicitly write zero water instances when
disabled on every acquired framebuffer. Model draws retain their separate
matrices and draw after clearing world depth, before UI and HDR resolve.

## Build and data ownership

BrokenEngineSandbox owns the viewer's generated data and packed identity.
Sandbox data headers are permitted; sandbox Source headers are not viewer host
contracts. Viewer Source has include precedence over engine headers.

The project imports ignored resolved data properties written by `/compile` and
generates its default-root header from the effective build properties. The host
must assign that root to LaunchOptions before FileManager construction. Direct
launch and configured Visual Studio builds use the same root. Output, intermediate
and generated-root files remain under ignored Output/Build directories.

Source and header membership is explicit in the project and filters. Viewer-owned
items use the Viewer filter; generated headers use DataFiles. Engine/Common items
follow their directory structure, as in the sandbox project.

The project directly compiles
`Engine/Source/Memory/Mimalloc.cpp`.
That source disables PCH and forced includes and uses the vendor-wrapper warning
policy documented in `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md`
`## Build Configuration`; its other compiler settings follow the viewer project.
Its per-source `MI_DEFAULT_RESERVE_OS_MEMORY` definition selects 10 GiB in KiB in every
configuration. The allocator startup contract is in
`Engine/Source/Memory/AGENTS.md`.
