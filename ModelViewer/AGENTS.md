# ModelViewer

## Application contracts

ModelViewer is a separate client executable. Viewer policy and game contracts
belong here; shared rendering mechanisms remain engine-owned. Do not instantiate
the sandbox Game or start a network session. The selected scene is the only
visible geometry, against a dark neutral background; world allocations required
by existing graphics managers do not authorize visible world passes.

Preserve ordinary Graphics ownership, upload acquisition, descriptor publication,
submission fences and resource retirement. Selected identity and viewer controls
must survive Graphics recreation, with resources republished before use.

F9 permits one outstanding PNG capture. Successful completion is logged only
from the matching engine capture result after the PNG write; queued requests
survive Graphics replacement.

The catalog includes every packed scene with materials, sorted by packed path.
Preview geometry stays in its authored rest pose, including node matrices and
skinning; an animation's first key is not a rest-pose substitute. Camera fitting
uses bounds from the same transformed, drawn geometry. UI mouse capture takes
precedence over camera gestures.

Controlled background pointer positions enter ImGui's public input queue through
a viewer-private message without arming physical mouse-leave tracking. Preserve
ordinary native input forwarding.

## Engine integration

`BT_MODEL_VIEWER` is confined to this project. It excludes gameplay host and
generic agent aggregation, gameplay camera updates and terrain cell queries,
collection pipeline registration, sandbox screen ownership, network crash uploads
and profile-screen presentation. Keep local crash reporting, ordinary engine
profiling collection and Graphics lifecycle.
The viewer supplies a presentation frame, camera matrices and sun input,
`RenderViewerFrame` for Main uniforms and `RenderViewerUi` inside the ordinary
ImGui backend. Model illumination uses the game's shared sun/day cycle, ambient
and model skybox environment for equal sun input and shared defaults. Preview
isolation excludes world lighting deposits, smoke and terrain occlusion from
model shading.
Auxiliary world render targets retain their normal initialization
and transitions; the image pass clears a neutral background and records model
draws and UI depth only before HDR resolve. Do not satisfy an unused gameplay
dependency with dummy cells, flags, sessions or reporting functions.

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
