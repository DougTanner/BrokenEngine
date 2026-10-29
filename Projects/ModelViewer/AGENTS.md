# ModelViewer

## Application contracts

ModelViewer is a separate client executable. Viewer policy and game contracts
belong here; shared rendering mechanisms remain engine-owned. The selected scene
draws over a seeded terrain and water background centered on a verified clear-water
footprint near island context by default. In orbit, disabling World
background switches to the original HDR gray clear color, graded by current controls
([Top-Level Shaders](../../Engine/Data/Shaders/AGENTS.md#top-level-shaders)).
World background, Seed and Reroll are viewer-owned, unsaved controls outside
presets and Q comparison. Seed edits and Reroll remain usable with the world
hidden; enabling it restores that layout. Startup uses the origin seed with
World background enabled.

SPACE toggles orbit preview and autonomous flight once per tap outside text
editing. Flight forces the world visible without changing the retained orbit
background preference; returning to orbit restores that preference and pauses
the body. Resuming flight preserves its position without a catch-up interval.
One viewer-owned body chooses random clear-water destinations inside
the original cell; it never transfers cells or enters deterministic game state.
Altitude choices are the viewer's flight-plane table (`Source/Frame/FlightPlaneTable.h`).
Planes below the highest use terrain collision at the actual height through
transitions; the highest permits terrain overflight. Altitude changes run at a
fixed rate, with descent held until the whole footprint is over clear water and
clear at the proposed lower height.
Model selection resets the body to its water anchor and the lowest plane in either mode,
retaining the active mode. Seed edits and Reroll rebuild elevation and clear-water
destinations before respawning, also retaining the mode.

Startup shares the Sandbox client's native instance mutex under
[Engine single-instance policy](../../Engine/Source/AGENTS.md#crash-reporting).
Acquire it before constructing rendering and file managers; refusal exits before
their construction, with no dialog in an agent launch.

Preserve ordinary Graphics ownership, upload acquisition, descriptor publication,
submission fences and resource retirement. Selected identity and viewer controls
must survive Graphics recreation, with resources republished before use.

Holding Q temporarily applies engine defaults to the right-hand lighting float
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

W/E/R taps load independent presets for those same sliders; edits auto-save
only the selected preset. Missing or invalid presets apply all compiled defaults,
without writing on selection. Backspace applies compiled defaults and saves them
to the selected preset, retaining selection. Startup loads ordinary graphics
settings, then selects and loads W before Graphics construction; a preset is
always selected. Selection survives Graphics recreation. The right window title
shows Preset: followed by the selected key, with a stable ImGui window ID and no
duplicate content header. Ignore slot/reset key repeats and apply taps in
received order before rendering.

Presets use separate viewer-owned versioned AppData files through the shared atomic
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

In orbit, left drags rotate the camera; right drags rotate only the model sun
about the active camera's screen axes in either mode with the same continuous
feel. Both gestures allow edge roll and cross poles
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

## Source metadata authoring

The flight panel edits XYZ scale, rotation in degrees and offset, and offers
explicit Save to the selected source's adjacent `.model.json`. The
[exporter owns the metadata format](../../DataPacker/Source/ExportJobs/AGENTS.md#model-alignment).
Geometry and animation still come from packs; alignment always comes from this
checkout's source metadata on startup and actual model selection, with no packed
alignment fallback. Selection discards unsaved edits; reselecting or restarting
reloads saved values without packing. Mode changes, Seed/Reroll and Graphics
recreation retain active edits. There is no source watcher.

The executable's containing checkout owns source reads and Save, in both main
checkouts and linked worktrees, independently of working directory and packed
Data root. Resolve the selected scene's model identity to exactly one actual
source under this checkout's Engine or Sandbox Data root; a sidecar alone does
not establish ownership. Never follow a worktree's `.git` pointer to source in another checkout.
Missing or ambiguous source ownership and unavailable checkout resolution show
errors and disable Save.

Only an absent sidecar supplies identity alignment and permits Save to create
it. An unreadable, malformed, unsupported or invalid present sidecar shows a
load error, applies no partial record and disables Save until a successful
reload. Invalid edits stay visible with a reason and disable Save while the
last valid complete alignment remains in the flight preview. Validate derived
alignment and its inverse-transpose normal matrix as finite and invertible
before accepting edits; valid scalar fields alone are insufficient.

Only explicit Save writes metadata, in the tracked sidecars' layout, so an
unedited Save leaves a shortest-form sidecar byte-identical. Replace the
destination from a completely written and closed same-directory staging file;
failure preserves the previous sidecar and shows an error. Save never runs DataPacker: games consume
the saved alignment after their next ordinary data build. A user can Save while
agents work, mutating tracked source metadata in either main or session
checkouts. Treat these changes as user-owned and preserve them; never assume
every dirty sidecar is agent-authored or silently revert it.

Authored alignment affects flight presentation only, leaving body movement,
collision, camera tracking, raw rest bounds and animation evaluation unchanged.
Compose offset, authored XYZ scale times the existing flight radius, XYZ
rotation, then flight orientation and body translation, matching Fighter order.
Save only authored scale, excluding the radius multiplier. Keep the shadow
origin at body center despite authored offset. Orbit retains raw geometry and
its fitted camera while preserving edits for flight.

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
In orbit, the viewer's world camera is fixed top-down at the default engine
height over the water anchor, with a separate model orbit pose used only for
model draws. Flight uses the engine's tracked camera and world model matrices
and eye position; retain orbit fitting and rotation across toggles. The seed
only selects the layout; its owning cell stays at the origin without a game
session. Publish one cell when the world is effectively visible and an empty
span otherwise, once per acquired framebuffer after camera update and before
RenderGlobal. Preserve the seed, mode, body, visual transforms, model selection
and animation state through Graphics recreation. The viewer supplies
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
Orbit preview isolation excludes world lighting deposits, smoke and terrain
occlusion from model shading; flight uses ordinary world shading and depth.
Flight also uses the engine's projected ground shadows with shared world sun
and existing shadow settings; right-drag model illumination does not steer
those shadows. Model and shadow draws share the final layout and one skinning
evaluation. Publish paired main/shadow counts for every catalog identity each
acquired framebuffer, zeroing unselected identities and suppressing the selected
shadow in orbit so selection and mode changes leave no stale coverage.
Shared sun and ambient controls affect the world; IBL and
BRDF controls remain model-only. HDR grading affects both world and gray modes.
Auxiliary world render targets retain their normal initialization and transitions;
the image pass retains its HDR gray and depth clears in both modes. Main uniforms
publish water when effectively visible and explicitly write zero water instances
otherwise on every acquired framebuffer. Orbit draws models after clearing
world depth; flight retains world depth. Both precede UI and HDR resolve.

Apply the retained mode's renderer policy before flight/camera updates, island
or uniform publication, and submission, including after full Graphics replacement.
Complete pending recreation there through the engine lifecycle. Rebuilds below
the swapchain tier retain the current acquisition; swapchain rebuilds reacquire
only after successful creation. Deferred creation or failed acquisition skips
publication/submission through the paced main loop and signals upload progress.
Reset display timing across mode changes, recreation and skipped frames so
movement and camera tracking share one active display interval.

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
