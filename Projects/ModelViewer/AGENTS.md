# ModelViewer

## Application contracts

ModelViewer is a separate client executable; shared rendering mechanisms stay
engine-owned. The selected scene draws over a seeded terrain and water
background centered on a verified clear-water footprint near island context.
With World background off in orbit, the HDR clear shows exactly the Background
gray value, white to black, ungraded
([Top-Level Shaders](../../Engine/Data/Shaders/AGENTS.md#top-level-shaders));
the slider is disabled while the world is visible. World background, Background
gray and Reroll are unsaved viewer controls outside presets and Q comparison;
Reroll works with the world hidden, and its layout shows when the world is
enabled again. Startup enables World background and picks the template with the
tallest stored peak (`IslandHeader`). Startup steps the seed forward from the
origin, and Reroll from a random seed, until the cell's `CentredIslands` hold
that template and another placement and every one of their peaks passes the
flight sweep's inset, because the body could never reach a peak past it. The
elevation grid keeps the full list, so the viewer's cell shows the land the game
does.

TAB, Left/Right, SPACE, F3, Q and W/E/R/Backspace act only outside text
editing. TAB and Left/Right act once per non-repeat press and are then withheld
from ImGui, whose tabbing would start a slider's text entry; during text editing
they reach ImGui as usual. SPACE, F3 (Debug and Profile builds) and the
W/E/R/Backspace taps act once per non-repeat press after ImGui has seen them;
taps apply in received order before rendering. Q acts on every press, repeats
included.

TAB toggles orbit preview and autonomous flight. Flight adds a Flight tab to the
right window, selected when TAB enters flight with the panels shown and removed
on return to orbit; every exact entry in it has a paired slider. Flight forces
the world visible without changing the retained orbit background preference;
returning to orbit restores it and pauses the body. In flight the wheel drives
the engine camera's framing zoom between its limits, starting at the engine's
initial framing on every entry; resuming flight keeps the body's position
without a catch-up interval. One viewer-owned body loops between peaks: the
tallest template's, then a random other centred placement's, then the tallest
again. It stays in the original cell, never transfers cells, and never enters
deterministic game state. Peaks are horizontal destinations only: the body flies
at the exact, unbounded Height (m) entry, which accepts only finite values and
is reached at a fixed rate, with no terrain collision, island avoidance or
descent hold. Flight planes are not a viewer control; the table stays supplied
for engine files ([Engine integration](#engine-integration)). Model selection
retains the body, destination, Height (m) entry, pause and mode. Reroll
rebuilds elevation, then resets the body to its water anchor and the Height (m)
entry to its default, retaining the mode.

Startup shares the Sandbox client's native instance mutex under
[Engine single-instance policy](../../Engine/Source/AGENTS.md#crash-reporting);
refusal exits before the rendering and file managers are constructed, with no
dialog in an agent launch.

Preserve ordinary Graphics ownership, upload acquisition, descriptor
publication, submission fences and resource retirement. Selected identity, the
seed, mode, body, visual transforms, animation state and viewer controls survive
Graphics recreation, with resources republished before use.

Holding Q applies engine defaults to the right-hand lighting float sliders and
disables their edits; the current tweaks are restored on Q release, focus loss
or shutdown, and survive Graphics recreation meanwhile. A preset selection or
reset while Q is held updates what will be restored; comparison never saves a
preset or changes its selection. Sun angle and the left Render quality controls
stay outside comparison, presets and Backspace reset. The viewer never loads or
saves graphics settings: every run starts from the compiled graphics defaults.
Model diffuse IBL multiplies the shared ambient color directly; sun/moon IBL
specular stays independent. One ordered descriptor table owns both the lighting
UI and preset/comparison membership.

W/E/R load independent presets for those sliders, and a preset is always
selected: startup selects and loads W before Graphics construction, and the
selection survives Graphics recreation. Only viewer-panel slider edits
(auto-saving the selected preset) and Backspace (applying compiled defaults and
saving them to the selected preset, retaining selection) write preset files. A
missing, invalid or unsupported-version preset applies all compiled defaults
without writing on selection, with no migration. Presets are separate
viewer-owned versioned AppData files written through the shared atomic file
helpers; validate the complete payload before applying any value, and bump the
version when participant order changes. The right window title shows Preset:
and the selected key, with a stable ImGui window ID and no duplicate content
header.

SPACE toggles a viewer-owned pause freezing clip playback time, the flight body
and the shader elapsed time; resuming continues without a catch-up interval.
The camera, sun controls, mode toggle and panels stay usable, and the engine
debug text area shows PAUSED.

F3 toggles the engine Tweaks screen; while it is open the viewer hides only its
panel windows, and orbit, sun drag and wheel zoom keep working. Tweaks edits of
the shared Pbr, SunMoon and color-grading wrappers apply live, are saved to the
preset only by the next viewer-panel slider edit (which saves every current
value), are replaced by the next W/E/R load or Backspace reset, and are
discarded on Q release when made while Q is held.

Bare F4 uses the ordinary window-close path, restoring comparison and releasing
owners. Modified F4 adds no viewer close binding; Alt-F4 keeps native window
handling.

F9 permits one outstanding PNG capture, logged as successful only from the
matching engine capture result after the PNG write; queued requests survive
Graphics replacement. Once per process, after a completed render, the viewer
logs `ModelViewer initial streaming done` when no texture chunk is between load
request and adoption and every referenced island template and its mesh are GPU
resident.

The catalog holds every packed scene with materials, sorted by packed path.
Left/Right selects the adjacent model, stopping at the catalog endpoints; mouse
selection uses the same transition. The viewer owns clip selection, playback
time and play/pause state; selected clips use the engine's indexed pose
evaluation. Rest pose uses cached authored node matrices and skinning, never an
animation's first key. Camera fitting transforms the rest-bounds center through
accepted alignment and scales its radius by the largest 3D linear-row length,
enclosing positive anisotropic scales. Model changes reset to Rest pose; they
and accepted alignment edits refit the camera, retaining orbit camera rotation;
clip changes restart playback without refitting. UI mouse capture takes
precedence over camera, sun and picking gestures.

In orbit, left drags rotate the camera, except that while picking mode is on and
active a left click picks a hardpoint of the selected kind and never orbits
([Source metadata authoring](#source-metadata-authoring)); right drags rotate
only the model sun about the active camera's screen axes in either mode with the
same continuous feel. Both gestures allow edge roll and cross poles without
clamping, and orientation accumulates along the drag path, so returning the
pointer need not restore it. Pointer coordinates use the viewport's shorter
dimension so non-square viewports keep circular rotation. The initiating button
owns the gesture until release or cancellation; another button, or dragging
after UI cancellation, needs a fresh click.

With `--agent-port`, the viewer stays hidden, suppresses startup error dialogs
and starts no generic agent transport. Optional `--scene-path` selects an exact
packed catalog path before the first render; an empty, missing or unknown
identity fails startup, and omission keeps the first sorted entry. Programmatic
pointer positions and left/right-button transitions enter ImGui's public input
queue through viewer-private messages without physical mouse-leave tracking or
native mouse capture; ordinary native input forwarding stays.

Agent testing shares the PC with an active human: automated verification must
not activate or raise the viewer, steal focus or mouse capture, move the
physical pointer, or inject global desktop keys or clicks. Send inputs to the
viewer programmatically; automated pointer testing requires a verified
nonactivating launch and noncapturing input route. Human-requested launches
stay maximized and foreground.

## Source metadata authoring

The right window's Flight tab edits XYZ scale, rotation in degrees and offset,
the Edit Hardpoints tab edits hardpoints, and the Collision tab edits the
outline inset; each offers the same explicit Save to the selected source's
adjacent `.model.json`, whose format and alignment matrix
[the exporter owns](../../DataPacker/Source/ExportJobs/AGENTS.md#model-alignment).
No control edits the hitbox setting or the navigation obstacle tag, so Save
writes back the loaded values. Geometry and animation come from packs;
alignment, outline inset and hardpoints always come from this checkout's source
metadata on startup and actual model selection, with no packed fallback.
Selection discards unsaved edits; reselecting or restarting reloads saved values
without packing. Mode changes, Reroll and Graphics recreation keep active edits.
There is no source watcher.

One ordered hardpoint list is edited through per-kind tabs; X deletes the
original entry and Save preserves list order. Pick appends the selected kind at
the nearest front-facing (counter-clockwise) rest-geometry triangle hit under
the pointer, with its hit position and its unflipped unit winding normal as the
direction; a miss adds nothing. Picking and the hardpoint markers are active
only while Edit Hardpoints is selected in the shown right window, in orbit with
Rest pose; there a left click outside the panels always picks and never orbits.
In flight or with a clip selected the Pick setting is kept but inactive and its
checkbox disabled. The markers draw both kinds as dots at their positions with
short lines along their directions, through one shared projection applying the
accepted alignment, over the scene and under the panels with no depth test.

The Collision tab, present in both modes, previews the inset on a model whose
pack holds the `outline` hitbox setting; on any other model it says a data build
applies a hitbox change, its Inset (m) slider stays disabled and nothing draws.
The slider and overlay are active only while Collision is selected in the shown
right window, in orbit with Rest pose. Two closed lines draw at the outline's
mid height, over the scene and under the panels with no depth test: magenta, the
packed outline the game holds, and on top green, that outline moved by the
difference between its baked inset and the slider. The viewer reads no mesh and
builds no silhouette, so green only approximates the next bake, and magenta
changes only after a data build and viewer restart. An edit failing the bake's
vertex-count or origin check keeps the last valid green line, shows the reason
and disables Save.

The executable's containing checkout owns source reads and Save, in main
checkouts and linked worktrees alike, independently of working directory and
packed Data root. Resolve the selected scene's model identity to exactly one
actual source under this checkout's Engine or Sandbox Data root; a sidecar alone
does not establish ownership. Never follow a worktree's `.git` pointer to
another checkout. Missing or ambiguous source ownership and unavailable checkout
resolution show errors and disable Save.

Only an absent sidecar supplies identity alignment and lets Save create it. An
unreadable, malformed, unsupported or invalid present sidecar shows a load
error, applies no partial record and disables Save until a successful reload.
Validate derived alignment and its inverse-transpose normal matrix as finite and
invertible, and derived preview bounds as finite with positive radius, before
accepting an edit; valid scalar fields alone are insufficient. An invalid edit
stays visible with a reason and disables Save, while the last valid alignment
keeps the draw, both previews and the camera fit.

Only explicit Save writes metadata, in the tracked sidecars' layout, so an
unedited Save leaves a shortest-form sidecar byte-identical. Replace the
destination from a completely written and closed same-directory staging file;
failure keeps the previous sidecar and shows an error. Save never runs
DataPacker: games consume the saved alignment after their next ordinary data
build. A user can Save while agents work, in main or session checkouts; treat
those sidecar changes as user-owned, never assume a dirty sidecar is
agent-authored, and never silently revert one.

Both modes, orbit picking and the hardpoint markers use the same accepted
alignment; hardpoint values stay in pre-alignment rest coordinates. Flight
appends its orientation and body translation. Keep the shadow origin at body
center despite authored offset. Body movement, camera tracking, raw rest bounds
and animation evaluation stay independent of alignment. The orbit camera starts
looking straight down with the aligned front (+x after the authored rotation) to
screen right and +y to screen up.

## Engine integration

The viewer compiles only the engine files it needs and supplies only the game
names those files use (the project membership, `Source/` and `Source/Pch.h`
switches hold the current set), defining unneeded hooks empty. Never satisfy an
unused gameplay dependency with dummy cells, flags, sessions or reporting
functions; the viewer constructs no `GameBase` or network session. It registers
only the engine Tweaks sections, adding no Tweaks content. Keep ordinary engine
profiling collection, Graphics lifecycle and local crash reporting without
network upload.

In orbit, the world camera follows the
[shared camera contract](../../Engine/Source/Graphics/AGENTS.md#world-and-camera-contracts)
at the default engine framing over the water anchor, with a separate model orbit
pose used only for model draws. Flight uses the engine's tracked camera and
world model matrices and eye position; orbit fitting and rotation survive
toggles. The seed only selects the layout; its owning cell stays at the origin
without a game session. Pass `RenderGlobal` one cell when the world is
effectively visible and an empty span otherwise, once per acquired framebuffer
after camera update.

The viewer supplies a presentation frame and sun input and writes the camera's
model sun rotation (`mModelSunRotation`), whose reach the
[camera contract](../../Engine/Source/Graphics/AGENTS.md#world-and-camera-contracts)
owns. Model illumination uses the game's shared sun/day cycle, ambient and model
skybox environment for equal sun input and shared defaults. Right-drag sun
rotation persists in world space across Sun angle edits and Graphics
recreation; model changes reset it. Sun angle still drives shared day-cycle
colors and brightness and the base direction for world and model. Orbit preview
isolation excludes world lighting deposits, smoke and terrain occlusion from
model shading; flight uses ordinary world shading and depth, and the engine's
projected ground shadows with the shared world sun and existing shadow
settings. Shared sun and ambient controls affect the world; IBL and BRDF
controls stay model-only. HDR grading affects fully covered drawn pixels in
both modes.

Model and shadow draws share the final layout and one skinning evaluation.
Publish paired main/shadow counts for every catalog identity each acquired
framebuffer, zeroing unselected identities and suppressing the selected shadow
in orbit, so selection and mode changes leave no stale coverage. Auxiliary world
render targets keep their normal initialization and transitions; the image pass
clears to the Background gray and clears depth in both modes. Main uniforms
publish water when effectively visible and explicitly write zero water
instances otherwise, every acquired framebuffer. Orbit draws models after
clearing world depth; flight keeps world depth. Both precede UI and HDR resolve.

Apply the retained mode's renderer policy and the Background gray clear before
flight/camera updates, island or uniform publication, and submission, including
after full Graphics replacement, and complete pending recreation there through
the engine lifecycle. Rebuilds below the swapchain tier keep the current
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

Source and header membership is explicit in the project and filters. A file is
a project member exactly when a source the project already compiles, or that
the same `/update-vcxproj` run adds to it, needs the file: the source calls a
symbol the `.cpp` defines or includes the header. Generated headers, including
the default-root header, use the DataFiles filter.

The project directly compiles `Engine/Source/Memory/Mimalloc.cpp` under the
ThirdParty warning policy of
`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md`
`## Build Configuration`. Its per-source `MI_DEFAULT_RESERVE_OS_MEMORY`
definition, in KiB, selects 10 GiB in every configuration.
