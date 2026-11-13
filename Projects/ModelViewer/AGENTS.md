# ModelViewer

## Application contracts

ModelViewer is a separate client executable. Viewer policy and game contracts
belong here; shared rendering mechanisms remain engine-owned. The selected scene
draws over a seeded terrain and water background centered on a verified
clear-water footprint near island context by default. In orbit, disabling World
background shows the HDR clear at exactly the Background gray slider's value,
white to black, independent of exposure and grading
([Top-Level Shaders](../../Engine/Data/Shaders/AGENTS.md#top-level-shaders));
the slider is disabled while the world is visible. World background, Background
gray and Reroll are viewer-owned, unsaved controls outside presets and Q
comparison. Reroll remains usable with the world hidden; enabling World
background restores that layout. At load the viewer picks the template with the
tallest stored peak (`IslandHeader`). Startup steps the seed forward from the
origin, and Reroll from a random seed, until the cell's `CentredIslands` hold
that template and another placement and every one of their peaks passes the
flight sweep's inset, because the body could never arrive at a peak past it. The
elevation grid keeps the full list, so the viewer's cell shows the land the game
does. Startup enables World background.

TAB toggles orbit preview and autonomous flight once per tap outside text
editing and is then withheld from ImGui, whose tabbing would start a slider's
text entry; during text editing TAB reaches ImGui as usual. In flight the right
window adds a Flight tab, removed on return to orbit and selected when TAB
enters flight while the panels are shown; every exact entry in it has a paired
slider. Flight forces the world visible without changing the retained orbit
background preference; returning to orbit restores that preference and pauses
the body. In flight the wheel drives the engine camera's framing zoom between
its minimum and maximum, starting at the engine's initial framing on every
flight entry. Resuming flight preserves its position without a catch-up
interval. One viewer-owned body loops between peaks: the tallest template's
peak, then a random other centred placement's peak, then the tallest again. The
body stays inside the original cell, never transfers cells, and never enters
deterministic game state. Peaks are horizontal destinations only: the body flies
at the exact, unbounded Height (m) entry, which has a 0-100 m slider, accepts
only finite values and is reached at a fixed rate, with no terrain collision,
island avoidance or descent hold. Flight planes are not a viewer control; the
table stays supplied for engine files
([Engine integration](#engine-integration)). Model selection retains the body,
destination, Height (m) entry, pause and active mode. Reroll rebuilds elevation
before resetting the body to its water anchor and the Height (m) entry to its
default height, also retaining the mode.

Startup shares the Sandbox client's native instance mutex under
[Engine single-instance policy](../../Engine/Source/AGENTS.md#crash-reporting).
Acquire it before constructing rendering and file managers; refusal exits before
their construction, with no dialog in an agent launch.

Preserve ordinary Graphics ownership, upload acquisition, descriptor
publication, submission fences and resource retirement. Selected identity and
viewer controls must survive Graphics recreation, with resources republished
before use.

Holding Q temporarily applies engine defaults to the right-hand lighting float
sliders and disables their edits. Preserve the current tweaks across Graphics
recreation and restore them on Q release, focus loss, or shutdown. Preset
selection or reset while Q is held updates what will be restored; comparison
never saves a preset or changes its selection. Sun angle and the left Render
quality controls remain outside comparison, presets and Backspace reset. The
viewer never loads or saves graphics settings: every run starts from the
compiled graphics defaults, and edits last only until exit. Model diffuse IBL
multiplies the shared ambient color directly; sun/moon IBL specular remains
independent. One ordered descriptor table owns both lighting UI and
preset/comparison membership.

W/E/R taps load independent presets for those same sliders; edits auto-save only
the selected preset. Missing or invalid presets apply all compiled defaults,
without writing on selection. Backspace applies compiled defaults and saves them
to the selected preset, retaining selection. Startup selects and loads W before
Graphics construction; a preset is always selected. Selection survives Graphics
recreation. The right window title shows Preset: followed by the selected key,
with a stable ImGui window ID and no duplicate content header. Ignore slot/reset
key repeats and apply taps in received order before rendering.

SPACE toggles a viewer-owned pause once per tap outside text editing. It
freezes clip playback time, the flight body and the shader elapsed time;
resuming continues from the frozen values without a catch-up interval. The
camera, sun controls, mode toggle and panels stay usable, and the engine debug
text area shows PAUSED.

In Debug builds a non-repeat F3 toggles the engine Tweaks screen. F3, Q press
and W/E/R/Backspace taps act only outside text editing, as SPACE does. While
Tweaks is open the viewer hides only its panel windows; orbit, sun drag and
wheel zoom keep working. Tweaks edits of the shared Pbr, SunMoon and
color-grading wrappers apply live, are not saved to the preset until the next
viewer-panel slider edit (which saves every current value, Tweaks-edited
participants included), are replaced by the next W/E/R preset load or Backspace
reset, and an edit made while Q is held is discarded on Q release.

Presets use separate viewer-owned versioned AppData files through the shared
atomic file helpers. Unsupported versions are rejected without migration and use
the same compiled-default fallback. Validate the complete payload before
applying any value; participant order is part of the format and changes require
its version bump. Only viewer-panel slider edits and explicit selected-preset
resets write preset files.

Bare F4 uses the ordinary window-close path, restoring comparison and releasing
owners. Modified F4 adds no viewer close binding;
Alt-F4 retains native window handling.

F9 permits one outstanding PNG capture. Successful completion is logged only
from the matching engine capture result after the PNG write; queued requests
survive Graphics replacement. Once per process, after a completed render, the
viewer logs `ModelViewer initial streaming done` when no texture chunk is
between load request and adoption and every referenced island template is GPU
resident with its mesh resident.

The catalog includes every packed scene with materials, sorted by packed path.
Left/Right selects the adjacent model once per tap outside text editing and
stops at the catalog endpoints. These keys are withheld from ImGui outside text
editing; mouse selection uses the same transition. The viewer owns clip
selection, playback time and play/pause state; selected clips use the engine's
indexed pose evaluation. Rest pose uses cached authored node matrices and
skinning; an animation's first key is not a rest-pose substitute. Camera fitting
transforms the rest-bounds center through accepted alignment and scales its
radius by the largest 3D linear-row length, enclosing positive anisotropic
scales. Model changes reset to Rest pose, refit the camera, retaining orbit
camera rotation. Accepted alignment edits also refit with the same retained
rotation; clip changes restart playback without refitting. UI mouse capture
takes precedence over camera, sun and picking gestures.

In orbit, left drags rotate the camera, except that while picking mode is on,
the Edit Hardpoints tab is selected and Rest pose is shown a left click picks a
hardpoint of the selected kind and never orbits
([Source metadata authoring](#source-metadata-authoring)); right drags rotate
only the model sun about the active camera's screen axes in either mode with the
same continuous feel. Both gestures allow edge roll and cross poles without
clamping. Orientation accumulates along the drag path; returning the pointer to
its starting position need not restore the starting orientation. Pointer
coordinates use the viewport's shorter dimension so non-square viewports
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
not activate or raise the viewer, steal focus or mouse capture, move the
physical pointer, or inject global desktop keys or clicks. Send inputs
programmatically to the viewer; automated pointer testing requires a verified
nonactivating launch and noncapturing input route. Explicit human-requested
launches remain maximized and foreground.

## Source metadata authoring

The right window's Flight tab edits XYZ scale, rotation in degrees and offset,
the Edit Hardpoints tab edits hardpoints, and the Collision tab edits the
outline inset; all three tabs offer the same explicit Save to the selected
source's adjacent `.model.json`. No control edits the hitbox setting or the
navigation obstacle tag, so Save writes back the loaded values. The
[exporter owns the metadata format](../../DataPacker/Source/ExportJobs/AGENTS.md#model-alignment).
Geometry and animation still come from packs; alignment, outline inset and
hardpoints always come from this checkout's source metadata on startup and
actual model selection, with no packed fallback. Selection discards unsaved
alignment, inset and hardpoint edits; reselecting or restarting reloads saved
values without packing. Mode changes, Reroll and Graphics recreation retain
active edits. There is no source watcher.

One ordered hardpoint list is edited through per-kind tabs; X deletes the
original entry and Save preserves list order. Pick appends the selected kind at
the nearest front-facing (counter-clockwise) rest-geometry triangle hit under
the pointer: its hit position and its unflipped unit winding normal as the
direction; a miss adds nothing. Picking is active only while Edit Hardpoints is
selected in the shown right window, in orbit with Rest pose; there a left click
outside the panels always picks and never orbits. In flight or with a clip
selected the Pick setting is kept but inactive and its checkbox disabled, and
picking resumes on return to orbit with Rest pose. Only while Edit Hardpoints is
selected in orbit with Rest pose, both kinds draw together as dots at their
positions with short lines along their directions. One shared projection applies
the accepted alignment, over the scene and under the panels with no depth test.

The Collision tab, present in both modes, previews the inset on a model whose
pack holds the `outline` hitbox setting; on any other model it says a data build
applies a hitbox change, its Inset (m) slider stays disabled and nothing draws.
The slider and overlay are active only while Collision is selected in the shown
right window, in orbit with Rest pose. Two closed lines draw at the outline's
mid height, over the scene and under the panels with no depth test: magenta, the
packed outline the game holds, and on top of it green, that outline moved by
the difference between its baked inset and the slider. The viewer reads no mesh
and builds no silhouette, so green only approximates the next bake, and magenta
changes only after a data build and viewer restart. An edit that fails the
bake's vertex-count or origin check keeps the last valid green line, shows the
reason and disables Save.

The executable's containing checkout owns source reads and Save, in both main
checkouts and linked worktrees, independently of working directory and packed
Data root. Resolve the selected scene's model identity to exactly one actual
source under this checkout's Engine or Sandbox Data root; a sidecar alone does
not establish ownership. Never follow a worktree's `.git` pointer to source in
another checkout. Missing or ambiguous source ownership and unavailable checkout
resolution show errors and disable Save.

Only an absent sidecar supplies identity alignment and permits Save to create
it. An unreadable, malformed, unsupported or invalid present sidecar shows a
load error, applies no partial record and disables Save until a successful
reload. Invalid edits stay visible with a reason and disable Save while the
last valid complete alignment remains in both previews. Validate derived
alignment and its inverse-transpose normal matrix as finite and invertible,
and derived preview bounds as finite with positive radius, before accepting
edits; invalid edits retain the accepted draw and camera fit. Valid scalar
fields alone are insufficient.

Only explicit Save writes metadata, in the tracked sidecars' layout, so an
unedited Save leaves a shortest-form sidecar byte-identical. Replace the
destination from a completely written and closed same-directory staging file;
failure preserves the previous sidecar and shows an error. Save never runs
DataPacker: games consume the saved alignment after their next ordinary data
build. A user can Save while agents work, mutating tracked source metadata in
either main or session checkouts. Treat these changes as user-owned and preserve
them; never assume every dirty sidecar is agent-authored or silently revert it.

Both modes draw through the complete shared alignment under the
[exporter's contract](../../DataPacker/Source/ExportJobs/AGENTS.md#model-alignment).
Orbit picking and hardpoint markers use that same accepted matrix; hardpoint
values stay in pre-alignment rest coordinates. Flight appends its orientation
and body translation. Keep the shadow origin at body center despite authored
offset. Body movement, camera tracking, raw rest bounds and animation evaluation
remain independent of alignment. The orbit camera starts looking straight down
with the aligned front (+x after the authored rotation) to screen right and +y
to screen up.

## Engine integration

The viewer compiles only engine files whose game names it supplies: game name
and version, HDR clear color and UI reference height in `Game.h`, the
flight-plane table, profile rows, `game::FrameInterpolate`,
`game::Camera::SunAngle`, and the four `game::Render*TweakExtensions` functions,
defined empty. Never satisfy an unused gameplay dependency with dummy cells,
flags, sessions or reporting functions; the viewer constructs no `GameBase` or
network session. By project membership it defines `engine::RenderFrameMain` for
Main uniforms and the three UI functions `ImGuiManager` calls: `RenderUiScreens`
draws the viewer panel and the engine text areas, and `CreateUiScreens` and
`DestroyUiScreens` are empty. It compiles the engine Tweaks screen and
`TweaksSettings` and registers only the engine Tweaks sections, adding no Tweaks
content. The viewer `Pch.h` sets `kbAudioStreamingHarnessRig` and
`kbProfileScreens` to `false` and `kbDrawModelsOverWorld` to `true`. Keep
ordinary engine profiling collection, Graphics lifecycle and local crash
reporting without network upload. In orbit, the viewer's world camera follows
the
[shared camera contract](../../Engine/Source/Graphics/AGENTS.md#world-and-camera-contracts)
at the default engine framing over the water anchor, with a separate model orbit
pose used only for model draws. Flight uses the engine's tracked camera and
world model matrices and eye position; retain orbit fitting and rotation across
toggles. The seed only selects the layout; its owning cell stays at the origin
without a game session. Publish one cell when the world is effectively visible
and an empty span otherwise, once per acquired framebuffer after camera update
and before RenderGlobal. Preserve the seed, mode, body, visual transforms, model
selection and animation state through Graphics recreation. The viewer supplies a
presentation frame and sun input, and writes `mModelSunRotation` inherited from
`engine::CameraBase`. Model illumination uses the game's shared sun/day cycle,
ambient and model skybox environment for equal sun input and shared defaults.
Right-drag sun rotation changes only the model direction and persists in world
space across Sun angle edits and Graphics recreation; model changes reset it.
Sun angle still drives shared day-cycle colors and brightness and the base
direction for world and model. Orbit preview isolation excludes world lighting
deposits, smoke and terrain occlusion from model shading; flight uses ordinary
world shading and depth. Flight also uses the engine's projected ground shadows
with shared world sun and existing shadow settings; right-drag model
illumination does not steer those shadows. Model and shadow draws share the
final layout and one skinning evaluation. Publish paired main/shadow counts for
every catalog identity each acquired framebuffer, zeroing unselected identities
and suppressing the selected shadow in orbit so selection and mode changes leave
no stale coverage. Shared sun and ambient controls affect the world; IBL and
BRDF controls remain model-only. HDR grading affects fully covered drawn pixels
in both modes. Auxiliary world render targets retain their normal initialization
and transitions; the image pass clears to the Background gray and clears depth
in both modes. Main uniforms publish water when effectively visible and
explicitly write zero water instances otherwise on every acquired framebuffer.
Orbit draws models after clearing world depth; flight retains world depth. Both
precede UI and HDR resolve.

Apply the retained mode's renderer policy and the Background gray clear before
flight/camera updates, island or uniform publication, and submission, including
after full Graphics replacement. Complete pending recreation there through the
engine lifecycle. Rebuilds below the swapchain tier retain the current
acquisition; swapchain rebuilds reacquire only after successful creation.
Deferred creation or failed acquisition skips publication/submission through the
paced main loop and signals upload progress. Reset display timing across mode
changes, recreation and skipped frames so movement and camera tracking share one
active display interval.

## Build and data ownership

BrokenEngineSandbox owns the viewer's generated data and packed identity.
Sandbox data headers are permitted; sandbox Source headers are not viewer host
contracts. Viewer Source has include precedence over engine headers.

In linked worktrees, the project imports ignored resolved data properties
written by `/compile`. Ordinary primary-checkout builds automatically pack
Sandbox data. The project generates its default-root header from the effective
build properties. The host must assign that root to LaunchOptions before
FileManager construction. Direct launch and configured Visual Studio builds use
the same root. Output, intermediate and generated-root files remain under
ignored Output/Build directories.

Source and header membership is explicit in the project and filters.
Viewer-owned items use the Viewer filter; generated headers use DataFiles.
Engine/Common items follow their directory structure, as in the sandbox project.

The project directly compiles `Engine/Source/Memory/Mimalloc.cpp`. That source
disables PCH and forced includes and uses the vendor-wrapper warning policy
documented in
`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md`
`## Build Configuration`; its other compiler settings follow the viewer project.
Its per-source `MI_DEFAULT_RESERVE_OS_MEMORY` definition selects 10 GiB in KiB
in every configuration. The allocator startup contract is in
`Engine/Source/Memory/AGENTS.md`.
