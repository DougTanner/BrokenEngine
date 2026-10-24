# ModelViewer

## Application contracts

ModelViewer is a separate client executable. Viewer policy and game contracts
belong here; shared rendering mechanisms remain engine-owned. Do not instantiate
the sandbox Game or start a network session. The selected scene is the only
visible geometry, against a fixed displayed RGB (0.5, 0.5, 0.5) background.
Scene grading does not alter that background. World allocations required by
existing graphics managers do not authorize visible world passes.

Preserve ordinary Graphics ownership, upload acquisition, descriptor publication,
submission fences and resource retirement. Selected identity and viewer controls
must survive Graphics recreation, with resources republished before use.

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

With `--agent-port`, the viewer remains hidden, suppresses startup error dialogs
and disables backend cursor changes; it does not start generic agent transport.
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
`RenderViewerFrame` for Main uniforms and `RenderViewerUi` inside the ordinary
ImGui backend. Model illumination uses the game's shared sun/day cycle, ambient
and model skybox environment for equal sun input and shared defaults. Right-drag
sun rotation changes direction only and persists in world space across Sun angle edits,
model changes and Graphics recreation. Sun angle still drives the shared day-cycle
colors and brightness and moves the base direction through the rotated orbit.
Preview isolation excludes world lighting deposits, smoke and terrain occlusion
from model shading.
Auxiliary world render targets retain their normal initialization
and transitions; the image pass clears uncovered HDR pixels and records model
draws and UI depth only before HDR resolve. Coverage composition belongs to
[HDR resolve](../Engine/Data/Shaders/AGENTS.md#top-level-shaders). Do not satisfy
an unused gameplay dependency with dummy cells, flags, sessions or reporting
functions.

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
