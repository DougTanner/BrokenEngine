# ModelViewer

## Application contracts

ModelViewer is a separate client executable. Viewer policy and game contracts
belong here; shared rendering mechanisms remain engine-owned. Do not instantiate
the sandbox Game or start a network session. The selected scene is the only
visible geometry, against a gray background that is the viewer's HDR clear
color, displayed about RGB 0.5 at default grading
([Top-Level Shaders](../../Engine/Data/Shaders/AGENTS.md#top-level-shaders)).
World allocations required by existing graphics managers do not authorize
visible world passes.

Preserve ordinary Graphics ownership, upload acquisition, descriptor publication,
submission fences and resource retirement. Selected identity and viewer controls
must survive Graphics recreation, with resources republished before use.

Holding Q temporarily applies engine defaults to the 21 right-hand float
sliders and disables their edits. Preserve the current tweaks across Graphics
recreation and restore them on Q release, focus loss, or shutdown before saving
settings. Preset selection or reset while Q is held updates what will be restored;
comparison never saves a preset or changes its selection. Sun angle and the left
Render quality controls remain outside comparison, presets and Backspace reset;
quality persists through ordinary graphics settings. Model Data Mip Bias remains
a lighting participant. One ordered descriptor table owns both lighting UI and
preset/comparison membership.

W/E/R taps load independent presets for those same 21 sliders; edits auto-save
only the selected preset. Missing or invalid presets apply all compiled defaults,
without writing on selection. Backspace applies compiled defaults and saves them
to the selected preset, retaining selection. Startup loads ordinary graphics
settings, then selects and loads W before Graphics construction; a preset is
always selected. Selection survives Graphics recreation. The right window title
shows Preset: followed by the selected key, with a stable ImGui window ID and no
duplicate content header. Ignore slot/reset key repeats and apply taps in
received order before rendering.

Presets use separate viewer-owned v2 AppData files through the shared atomic
file helpers. Unsupported versions are rejected without migration and use the
same compiled-default fallback. Validate the complete payload before applying
any value; participant order is part of the format and changes require its
version bump.
Only slider edits and explicit selected-preset resets write preset files.

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

Left drags rotate the camera; right drags rotate the sun about the current camera's
screen axes with the same continuous feel. Both allow edge roll and cross poles
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

`BT_MODEL_VIEWER` is confined to this project. It excludes gameplay host and
generic agent aggregation, gameplay camera updates and terrain cell queries,
collection pipeline registration, sandbox screen ownership, network crash uploads
and profile-screen presentation. Keep local crash reporting, ordinary engine
profiling collection and Graphics lifecycle.
The viewer supplies a presentation frame, camera matrices and sun input,
`engine::RenderFrameMain` by project membership for Main uniforms,
`game::Camera::SunAngle`, its HDR clear color and UI reference height in
`Game.h`, and `RenderViewerUi` inside the ordinary ImGui backend, and writes
the sun rotation its camera inherits from `engine::CameraBase`. Model
illumination uses the game's shared sun/day cycle, ambient and model skybox
environment for equal sun input and shared defaults. Right-drag
sun rotation changes direction only and persists in world space across Sun angle edits,
model changes and Graphics recreation. Sun angle still drives the shared day-cycle
colors and brightness and moves the base direction through the rotated orbit.
Preview isolation excludes world lighting deposits, smoke and terrain occlusion
from model shading.
Auxiliary world render targets retain their normal initialization
and transitions; the image pass clears HDR color to the viewer's gray, its world
draws record zero instances because the viewer writes no world indirect records,
and only model draws and UI depth
produce pixels before HDR resolve. Do not satisfy an unused gameplay dependency
with dummy cells, flags, sessions or reporting functions.

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
