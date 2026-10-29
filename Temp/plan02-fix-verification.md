# Plan 02 focused fix verification

## Result

Status: NEEDS_ACTION
Findings: CXX005 Critical Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.cpp:409 — ADV001 remains reachable through Graphics menu Water quality; hiding Tweaks and switching High to Low changes receiver geometry while command-buffer-only recreation retains the running capture; reject that scene mutation or fail the active capture at the existing UI/recreation boundary.
Changed files: none
Decisive checks: All 11 supplied target paths independently matched the immutable patch and were traced in fixed-function and affected-caller context; CXX001-004 and naming resolved, ADV001 incomplete.
Decisive checks: Final recorded DataPacker Release|x64, Sandbox client/server Debug|x64 and ModelViewer Debug|x64 envelopes each report success and exitCode 0; no build was run by this review.
Decisive checks: Focused git diff --check passed; eight changed cpp files measured together with Measure-Tokens; no new concrete cohesive split exposed by these fixes.
Build required: none
Evidence: Temp/plan02-fix-verification.md ## Per-item verdicts — accepted-finding resolution and source proof.
Evidence: Temp/plan02-fix-verification.md ## Remaining ADV001 path — reachable counterexample, lifecycle refutations and smallest correction.
Evidence: Temp/plan02-fix-verification.md ## Coverage and checks — all target paths, evidence identity and limits.
Scope: PASS
API verification requests: none
Project membership trigger: none
Residuals: none

## Per-item verdicts

- CXX001 RESOLVED. Lighting.h directly exposes requestedComparisonSettings and diagnostics. Removed SetComparison/GetDiagnostics have no
  remaining Engine/Projects references. TweaksScreenLighting.cpp reads/writes those members directly. Lighting::BeginRender updates only
  completed counts, leaving capture status/error intact. LightingComparison.cpp uses the same diagnostics object for request validation,
  capture cancellation and teardown failure; LightingCapture.cpp uses it for receivers/tracing/composing/export and profile output.
  Graphics.cpp:450-460 preserves the Lighting object during resource/pipeline recreation. Moving diagnostics from State to Lighting
  therefore preserves initialization, counts and capture status lifetime.
- CXX002 RESOLVED. Four trivial fixture getters are deleted. Lighting.cpp uses mAnimation by const reference, mAnchor by value, and
  mEvents.at(iEvent).camera/mCameras.at(static_cast<int64_t>(eCamera)); LightingComparison.cpp reads mAnimation.sceneCrc directly. Bounds
  checking and enum conversion remain equivalent. Fixture constructor initializes mAnchor before MakeCamera uses it, then fills
  cameras/events; InitializeAnimation and WriteAnimation retain semantic work. No serialization or GPU-record layout consumes the reordered
  fixture object itself.
- CXX003 RESOLVED. State keeps settings as its sole scenario discriminator. CreateFixtureResources, WriteFixtureSolids, source count selection and WriteSources read settings.eScenario; ApplyComparisonSettings copies requested settings once. No remaining pState->eScenario exists.
- CXX004 RESOLVED. InstanceManager's unconditional required-feature row and DeviceManager's unconditional vertexPipelineStoresAndAtomics assignment are removed. The existing optional kbGraphicsProcessingUnitAssistedValidation || kbDebugPrintf feature enable remains at DeviceManager.cpp:219-223. Required lighting address/AS/query and unrelated feature configuration remain present.
- Naming RESOLVED. Lighting owns pState and requestedComparisonSettings with all focused callers updated. Retaining struct Lighting, private coupled State and the three implementation files is the explicit accepted style decision; no contrary split finding is reintroduced.
- ADV001 NOT RESOLVED. The original Water Height Tweaks control is correctly blocked, including the disabled-aware agent set_slider route. However, GraphicsMenu's Water quality remains reachable while paused and changes water receiver positions/normals without canceling the capture. See Remaining ADV001 path.

## Remaining ADV001 path

CXX005 is the remaining accepted ADV001 failure, not a new independent foundation audit.

Reachable UI sequence: before capture, use the existing Water > Low Waves controls to set Low Camera Fade End to 10000
(TweaksScreenWater.cpp:77,228; wrapper allows 0..10000 at WaterWrappersBase.cpp:80), leaving nonzero low-wave amplitude and High Water
quality. Use the close fixture camera (eye height about 900), pause and start reference capture. During tracing press F3 to hide Tweaks,
Escape to open Pause, open Graphics and choose Low in the Water row. Let tracing finish. The fade precondition ensures a real change:
default fade ends at 600, above which these fixture cameras would already have flat water.

- Input.cpp:83,90 maps Escape/F3 to kPauseMenu/kMenuTweaks. GameBase.cpp:90-101 changes only meUiState for menu navigation; :125-128 changes only mbShowImGui. Neither changes GameFlags::kPaused. The pause simulation toggle is a separate action at :154-157.
- UiScreens.cpp:55-69 renders GraphicsMenu whenever Tweaks is hidden. PauseMenuScreen.cpp:54-57 enters kGraphicsSettings. GraphicsMenuScreen.cpp:152 calls the ordinary enabled RadioRow for gWaterLevel, outside the new Tweaks disabled scopes. The same enabled item is writable through agent UI activation.
- Graphics.cpp:560 polls gWaterLevel at DestroyType::kCommandBuffers. Graphics.h:28-36 orders kCommandBuffers below kPipelines. Graphics::Destroy resets command buffers at :685-688, but calls Lighting::DestroyTerrainPipelines/DestroyComparisonResources only at kPipelines or higher (:690-704). Thus the scene-affecting quality change does not run the existing capture-failure branch. CommandBufferManager construction/destruction has no capture invalidation.
- WaterUniforms.cpp:495-502 zeros both wave counts for Low; :560-569 zeros displacement dispatch and invokes that producer. At the stated fade precondition, High had nonzero wave counts/amplitude. Water.vert:57-65 bypasses displacement and emits flat normals when those counts become zero. Positions/normals in the final water surface therefore change.
- LightingComparison.cpp:247-250 creates water receivers from the ordinary Water vertex pipeline, changing only the fragment shader; LightingWaterReceiver.frag:42 saves its position/normal/albedo. LightingCapture.cpp:29-35 renders those receivers only once. They remain from High quality after the menu change.
- Water quality is absent from LightingComparisonSettings and LightingPublication. ApplyComparisonSettings checks only pause/settings (:109-116), and PrepareComparisonPipelines compares only publication counts (:198-214). Water is not a BLAS publication. Both checks therefore admit this mutation, and capture can reach composing/complete with old receivers and the new water surface.

Smallest correction: reuse an existing mutation boundary to block scene-affecting Graphics-menu controls while capture runs, or fail the active capture when their setting changes. A command-buffer-only water-quality change must not preserve a running capture. No scene hashes, geometry copies, new capture state, runtime expansion or new guards for impossible cases are needed.

The new Tweaks BeginDisabled/EndDisabled pairs themselves are balanced, with no intervening return. Toggle buttons and section window
controls are outside those scopes. The Lighting section remains enabled; its requests are applied at the beginning of the next
GameBase::Render, before publication. A request queued before a capture starts cannot leave an intervening mutable tracing UI frame:
ApplyComparisonSettings first establishes receivers, then that frame's UI sees receivers/tracing. Settings edits and unpause still fail on
the next ApplyComparisonSettings before that frame's preparation. Composing is completed/exported synchronously before the following UI
frame. Complete/failed statuses remove the disabled condition automatically. Audit-only pRender calls are noninteractive and WrapperSlider
returns before mutation in audit mode.

## Coverage and checks

Session baseline b56f20fa44d0424c2fdf5003157563684ed747f5; focused origin b01f2286e10144f36a8031f4daad51c060015151. All shell commands used
C:/Users/dougt/.claude/worktrees/BrokenEngine/0cb8b7b1-9695-4b87-ab78-8bf06eceacad. Targets file schemaVersion is
broken-engine-code-quality-targets/v1 with paths only. Per-file git diff comparisons against Temp/plan02-fix-review.patch matched all eleven
selected paths. No source, project, branch, build or runtime mutation was performed; this evidence file is the only output.

Traced: Lighting.cpp, Lighting.h, LightingCapture.cpp, LightingComparison.cpp, LightingFixture.cpp, LightingFixture.h, LightingState.h,
Managers/DeviceManager.cpp, Managers/InstanceManager.cpp, TweaksScreen/TweaksScreen.cpp and TweaksScreenLighting.cpp. Relevant lifecycle
callers, wrapper producers, UI/agent activation, camera/fixture initialization and direct shader receiver consumers were inspected to settle
the focused fixes. No new file or whole-file build-affinity change triggers membership reconciliation.

The four JSON envelopes under Temp/AgentBuildEnvelopes/compile-20261029T150823266Z-41536.md ## Build envelopes report status success/exitCode 0 for DataPacker Release at 15:08:36Z, Sandbox client Debug at 15:09:26Z, Sandbox server Debug at 15:10:41Z and ModelViewer Debug at 15:11:33Z. Temp/plan02-static-final.json reports pass for four triggered checks. Those checks do not refute the reachable UI defect.

Measure-Tokens results (bt-token-v1): Lighting.cpp 6276, LightingCapture.cpp 1417, LightingComparison.cpp 5257, LightingFixture.cpp 2254, DeviceManager.cpp 5601, InstanceManager.cpp 9184, TweaksScreen.cpp 4294, TweaksScreenLighting.cpp 818. No size follow-up is prescribed because the focused changed regions expose no new concrete cohesive split.

Scope authorization/minimality/KISS: PASS against the accepted CXX001-004/naming and ADV001 corrections in the dispatch and plan02 authorization. No pending external claim, pre-existing defect or incomplete target trace is required for this verdict. Runtime proof remains plan03; this result is independent static correctness verification and does not claim live observation.
