<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-21T19:13:59.529Z","dependsOn":[]} -->
# Build the packed-model viewer as an engine game

## Context and design

Build a separate Visual Studio ModelViewer project that reads exported packs, uses engine materials/shaders,
and provides scene selection, drag rotation, zoom and an ImGui sun-angle slider. Use ordinary engine startup
and viewer-owned game definitions and UI. Existing world graphics resource initialization is accepted;
zero world-resource startup is no longer an acceptance requirement. The visible scene must still contain
only the selected model on a fixed dark neutral background. This stage delivers true rest-pose display;
animation controls and their sample remain a later stage.

Create ModelViewer/Platforms/VisualStudio2026/ModelViewer.sln and its project/filters, Debug/Release x64,
with a dedicated PCH, BT_CLIENT and existing third-party dependencies. Compile the required existing Engine
implementations directly into this project. Supply viewer-owned Game.h identity/version, Profile/ProfileRows.h,
game::Camera and the game/frame/render/UI contracts those implementations actually consume. Follow the
existing contiguous profile-row layout and required kCpuTimerFrameUpdate/kGameCpuCounterFirst anchors.
Identity declarations alone do not satisfy the game/frame/network references in profiling screens or startup;
provide real viewer host contracts for enabled paths, without fake sandbox objects or empty reporting stubs.

Reuse Graphics, gpGraphics, gVkWantedFramebufferExtent2D and the existing singleton resource managers through
their ordinary construction, recreation and shutdown lifecycle. Keep required frame types and scene-system
initialization as engine integration. Do not instantiate or copy the sandbox Game: its sessions, menus,
music and gameplay are not viewer startup. Reuse the existing engine startup functions and required host
orchestration with viewer implementations, rather than assuming Engine/Source/Main.cpp is game-independent.
Do not start a client/server connection, sandbox gameplay or combat.

## Display and controls

Enumerate renderable Scene chunks, not raw Model chunks, exactly once; sort by full packed source path and
initially select the first. Retain scene CRC and source path for selection. On selection, wait for dependent
GPU work before replacing selected resources, evaluate actual bind/rest transforms, and compute center/radius
from packed geometry with evaluated node/skinning transforms. Empty catalogs report no selectable model and
leave controls safe.

Use existing model rendering, static/skinned shaders, material settings, environment maps, BRDF lookup and HDR
resolve. Evaluate bind-local matrices in parent-before-child order with the existing material/joint output
conventions; rest pose must not mean clip zero at time zero. Add only a demonstrated missing rest-pose
calculation if the existing path cannot supply it. Preserve indexed engine animation behavior. Reuse engine
uniform calculations and viewer camera sun input. Keep visible skybox, ground, terrain, water, shadows, smoke,
flight planes and local dynamic lighting out of the selected-model display through viewer render hooks and
the existing rendering path. Supply valid descriptors and neutral world contributions where that path
requires them; accepting world allocations does not satisfy selected-model presentation by itself.

Left drag orbits about the bounds center; wheel zooms. Respect ImGui mouse capture, clamp pitch/zoom, refit on
model change and update projection on resize. Provide a sun slider from 0 to 2*pi, initially 1.15 radians,
using existing sun direction/color/brightness calculations. Viewer UI uses the existing ImGui backend and
owns its controls; sandbox menus and HUD do not appear. Retain engine ordering for upload/acquisition,
descriptor publication, model draw, UI submission and presentation, including resize/minimize/recovery.

## Critical files

New ModelViewer/Source host, game definitions, frame/render/UI hooks, camera, profiling rows and project/PCH
files; existing Engine/Source/Main.cpp, Graphics/Graphics.cpp, Graphics/Managers/ImGuiManager.cpp,
Graphics/Render/GlobalUniforms.cpp and Graphics/AnimationData.{h,cpp} as integration references; compile
target/data-root routing below. Shared engine edits are conditional on a demonstrated missing hook or
calculation required by the display and lifecycle acceptance criteria.

## In scope

- New ModelViewer/Source/Main.cpp and ModelViewerApplication.{h,cpp}: required ordinary engine startup/window
  orchestration, scene catalog/selection, selected-resource lifecycle, rest-pose display, camera and sun UI.
  Supply viewer-owned Pch.{h,cpp}, ExternalHeaders.h, Game.h/version, Profile/ProfileRows.h, game::Camera,
  and the game/frame/render/UI hook implementations required by the selected existing engine paths.
- New viewer solution/project/filters: Debug/Release x64 settings, BT_CLIENT, third-party linkage, shared
  Engine source membership, viewer header resolution, generated data headers and pack-output validation.
- Existing Engine startup, Graphics creation/recreation/render submission, ImGuiManager screen construction/
  Prepare, frame graphics/render hooks, model uniform writers and AnimationData material/joint evaluation:
  only a small demonstrated missing integration hook or calculation needed for the stated viewer behavior.
  First try viewer-owned contracts and existing paths; record the concrete mismatch before any shared edit.
  Preserve ordinary engine callers, manager ownership and lifecycle. No pre-authorized extraction framework.
- .agents/skills/compile/scripts/Invoke-CompileBuild.ps1: ModelViewer target routing/selective mapping,
  BrokenEngineSandbox Shared/Local resolution and an untracked resolved-default-data-root header;
  Resolve-CompileContext.ps1: viewer target recognition/recommendation using BrokenEngineSandbox data identity.
  Reuse existing provisioning/export mechanisms.
- .agents/skills/compile/SKILL.md and references/runtime-data-mode.md and references/worker.md: viewer target/
  configuration/data behavior only; .gitignore: viewer output/intermediate/generated-header entries only
  when existing patterns do not cover them.
- New ModelViewer/AGENTS.md: viewer game integration, display and lifecycle contracts. Engine aggregation,
  affected project/filter entries and ownership documentation only when an actual shared edit requires them.

Named regions and mechanically required declarations/includes are the target and ceiling. Prioritize the
fewest engine changes and minimum viewer host code that fulfills the existing contracts.

## Out of scope

Animation widgets/sample, asset import/export UI, pack picker, sandbox menus/gameplay, active networking,
new simulation features, application-neutral manager/context interfaces, separate renderer ownership,
resource/world separation, ImGui backend extraction, new shading algorithms, shader/descriptor layout changes,
dynamic lights/shadow features, new harness commands, compatibility paths, generated pack commits and unrelated
build-tool refactors. Do not replace existing manager dependencies with explicit application input tables or
require a game-free source closure.

## Build and data contract

Reuse BrokenEngineSandbox generated data headers, pack filename tables and Shared/Local data resolution.
The viewer's game name/version labels its Vulkan application, AppData and temporary directories; it does not
rename pack files or change manifest/pack identity. /compile uses existing generation/resolution and writes
the resolved absolute root to an untracked build header. Set existing LaunchOptions dataDirectory from that
root before constructing FileManager. Normal client startup expects the complete existing client pack set;
this Plan does not promise a models-only footprint.

Direct launch and VS F5 consume the same baked default root and generated headers as the configured build.
Validate existing outputs and fail clearly through normal startup/build handling when missing. Do not add
path-configuration UI, another exporter, pack format changes or generated pack commits.

## Risk and invariants

Tier 3 for future implementation: game-host/graphics/UI/build integration and GPU synchronization. Preserve
existing shader/descriptor contracts, skinning allocation/offsets, upload acquisition/publication and fence/
retirement/destruction order, Shared/Local data identity and loader trust validation. Viewer state is
client-only; engine simulation/CRC, network/save/replay and pack versions remain unchanged. No unit tests.

## Implementation checkpoints

Complete these checkpoints in order within this single delivered stage; they are not separate landing or
approval stages. A failed or unobserved gate leaves that checkpoint open and stops work that depends on it.
Record each technical check and user-quality observation separately with its evidence and pass/open result;
use screenshots and diagnostics where required by Acceptance criteria below. Passing a technical check does
not close an unobserved user-quality gate. All final acceptance obligations remain required.

1. **Project and data setup.** Add project/PCH membership, target routing and generated-header/data-root
   integration from Build and data contract.
   - Technical gate: inspect Debug/Release x64 settings and source/header membership; build DataPacker
     Release through /compile and verify existing Shared/Local resolution and generated inputs. A linked
     viewer is not required until its host exists in checkpoint 2; launch/F5 root verification closes there.
   - User-quality observations begin at checkpoint 2, when the host can run.
2. **Runnable host.** Supply real viewer game/frame/render/UI contracts and ordinary engine lifecycle,
   with the neutral display and viewer UI.
   - Technical gate: build the viewer in Debug/Release through /compile and launch it; verify startup,
     direct-launch/F5 roots, resize/minimize/restore and shutdown under the stated lifecycle contracts.
   - User-quality gate: observe a readable viewer UI against the dark neutral display, with no sandbox
     menus/HUD or visible world effects; window and UI interactions remain responsive through resizing
     and restore. Missing-data startup failures clearly identify the missing input.
3. **True rest rendering.** Render one static scene and one skinned scene through existing engine paths
   before expanding selection to the complete catalog.
   - Technical gate: verify evaluated bind/rest transforms, material/joint output and geometry-derived
     bounds under Display and controls; preserve indexed animation behavior.
   - User-quality gate: observe correct visible geometry and materials, an undeformed static model and
     the skinned model's actual rest pose; initial framing shows the complete model at a useful size.
4. **Catalog and selection.** Implement the complete ordered catalog, switching, bounds/refit and empty
   catalog behavior.
   - Technical gate: verify enumeration identity/order, dependent GPU work retirement and safe empty
     selection; exercise every exported scene and repeated switching as Acceptance criteria requires.
   - User-quality gate: selection identifies the displayed scene, each switch visibly refits the complete
     model without stale geometry or corruption, and the empty state clearly reports no selectable model
     while controls remain safe.
5. **Camera and sun controls.** Complete orbit/zoom, ImGui capture, resize projection and sun input.
   - Technical gate: check the input, clamp, refit and sun contracts in Display and controls.
   - User-quality gate: orbit/zoom respond predictably about the model, interacting with UI does not move
     the camera, and sun adjustments visibly produce the required daylight/dawn/night appearances. Controls
     and selected state remain understandable and readable while inspecting and resizing the model.
6. **Complete integration verification.** Close every item in Acceptance criteria, including remaining
   configurations/data modes, engine regression checks and required reviews.
   - Technical gate: collect the required build, root, lifecycle and Vulkan evidence, including repeated
     switching/resizing, recovery and shutdown; all required reviews and checks pass.
   - User-quality gate: repeat the complete selection/camera/sun interaction sequence through resize and
     minimize/restore; models remain usefully framed, controls responsive and presentation free of visual
     corruption. Any direct GUI observation without evidence remains open.

## Acceptance criteria

- Build viewer Debug/Release x64, DataPacker Release and affected client/server targets through /compile.
  Inspect project/header membership and enabled startup/reporting paths for real viewer game contracts,
  ordinary Graphics/manager lifecycle and absence of sandbox Game construction or client/server connections.
- Verify Shared and Local builds, direct launch and VS F5 consume the same resolved pack/header roots;
  observe dataDirectory set before FileManager. Generated packs/headers stay untracked and Local generation
  uses only the existing pipeline. Viewer identity must not rename packs or change their format.
- Direct GUI: enumerate every exported renderable scene exactly once in path order; switch and fit every
  model; verify materials, static/skinned true rest pose, orbit/zoom, ImGui capture and sun daylight/dawn/night.
  Confirm only the selected model appears against the neutral background, without sandbox menus/HUD/gameplay
  or visible world effects even though normal world graphics resources initialize.
- Exercise resize, minimize/restore, recovery, shutdown and repeated switching/resizing. Record screenshots
  and Vulkan diagnostics and verify selected-resource lifetime and presentation remain correct. Verify
  zero-scene startup reports no selectable model and leaves controls safe.
- Through /agent-harness, verify the existing engine still starts/renders after any shared edits. Run static
  checks, required C++/artifact reviews, project/docs/skill validation and Tier-3 adversarial review. Direct
  GUI criteria that cannot be observed remain open; do not substitute new harness commands.

## Coordination

This is future viewer work, not evidence that viewer integration or shared APIs already exist. Keep target,
host and display changes together so the stage ends in a runnable rest-pose tool. A concrete missing engine
hook may justify a scoped engine edit; do not prepare a separate engine abstraction first. All application
acceptance closes in this stage rather than being postponed to animation.
