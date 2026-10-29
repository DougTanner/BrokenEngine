# Plan 02 capture diagnosis

## Root cause

PASS: supported Graphics UI mutations change saved receiver inputs outside LightingComparisonSettings/publication-count validation, while Graphics::Destroy only invalidates capture at kPipelines and above (Engine/Source/Graphics/Graphics.cpp:560,685-704; Engine/Source/Graphics/Lighting/LightingComparison.cpp:109-116,198-214). The prior Tweaks-only restriction does not own this other UI mutation boundary.

Scope: read-only source diagnosis in lighting-foundation-work worktree, fixed baseline b56f20fa44d0424c2fdf5003157563684ed747f5. No source edits, builds, runtime, instrumentation, commits, branch changes, or delegation. This evidence file is the explicitly requested durable handoff artifact. The failed retest is Temp/plan02-fix-verification.md ## Remaining ADV001 path; all source conclusions below were independently inspected.

## Reproducing signal

Static close inspection is the authorized Plan 02 reproducing signal. Red sequence: before capture set Water > Low Waves > Low Camera Fade End to 10000 through its existing slider; retain Start=300, nonzero amplitude, positive low-wave count, High Water quality and Close comparison camera. Pause, request reference capture, wait for tracing, hide Tweaks with F3, Escape to Pause, open Graphics, choose Water Low. Preserve client pause and comparison settings through tracing completion.

The precondition is permitted: TweaksScreenWater.cpp:77,228 exposes the value; WaterWrappersBase.cpp:71,79-80 gives amplitude 0.05,
Start300, default End600 and allowed maximum10000. LightingFixture.cpp:55 has Close eye Z900. Thus WaterUniforms.cpp:517-519 produces low
amplitude scale (10000-900)/(10000-300), approximately0.938, rather than zero. PopulateGerstnerLowWaves at :411-448 publishes nonzero wave
count/amplitude. Low quality instead sets both counts zero at :497-502 and zero displacement dispatch at :560-569. Water.vert:57-65 changes
displaced geometry/normals to flat values. The old receiver is therefore distinguishable from the final water surface; the default End600
alone would not prove this.

GameBase.cpp:90-101,125-128 changes UI state/Tweaks visibility without changing pause (the pause toggle is separate at :154-157).
UiScreens.cpp:55-69 calls GraphicsMenu when Tweaks is hidden; PauseMenuScreen.cpp:54-57 enters Graphics; GraphicsMenuScreen.cpp:152 writes
enabled gWaterLevel. Refresh at Graphics.cpp:560 selects kCommandBuffers; Graphics.h:28-36 orders this below kPipelines. Destroy at :685-688
resets only command buffers at this tier; Lighting::DestroyTerrainPipelines at :690-695 calls DestroyComparisonResources
(Lighting.cpp:203-207), which is therefore skipped.

LightingComparison.cpp:247-250 builds receivers using ordinary Water vertex shader; LightingWaterReceiver.frag:42 stores the receiver.
LightingCapture.cpp:29-35 renders receivers once; later frames only trace their saved images and finally enable reference composition at
:37-51. LightingComparisonSettings (Lighting.h:31-42) excludes water quality and publication checks contain only counts. Those remain
identical, so neither failure condition fires. FinishCapture at LightingCapture.cpp:54-96 can export the old receivers with newly flat final
water and set complete. No runtime claim is made.

## Alternate mutation routes

| Existing route | Classification and evidence |
|---|---|
| Graphics Defaults | Same supported bypass, not a separate hypothetical input. GraphicsMenuScreen.cpp:100-103 calls ResetGraphicsSettings. GraphicsSettings.cpp:225-250 resets gWaterLevel, foam, sampling and other settings. Arrange nondefault water quality with otherwise-default recreation settings so reset does not coincidentally escalate to kPipelines. A Water-row-only restriction leaves this writer available. |
| Medium versus High Water | Same geometry class when the medium band is nonzero: WaterUniforms.cpp:526-532 drops medium waves only for Medium. Preconfigure the allowed medium camera fade before capture to keep that band nonzero. No separate mechanism is needed. |
| Water Foam | Receiver albedo rather than geometric position: WaterUniforms.cpp:41-43,186-188 toggles iWaterFoamEnabled; WaterMaterial.h:32-43 changes material mix and LightingWaterReceiver.frag:38-42 stores that mix. Refresh:559 selects kCommandBuffers, so invalidation is missed when foam coverage is nonzero. |
| Anisotropy / Max Anisotropy / Mip Lod Bias | Material/normal sampling class, not water mesh geometry. Refresh:550-554 selects kSamplers,
below kPipelines. Graphics.cpp:407-418 rebuilds samplers and rewrites descriptors while capture survives. TextureManager.cpp:359-374 applies
bias/anisotropy to render samplers; receiver shaders call shared TerrainMaterial and ModelMaterial sampling. A scene with affected textured
receivers can change albedo/normals without count changes. No runtime magnitude claimed. |
| Time of Day | Receiver color/normal mutation without any Refresh polling: GraphicsMenuScreen.cpp:119 writes gSunAngleOverride.
Camera.cpp:82-93 returns override in Graphics/Tweaks; GlobalUniforms.cpp:509,541-544 feeds its angle to terrain/water uniforms.
WaterUniforms.cpp:13-24 changes sunset material fade; WaterMaterial.h:30 uses it in the receiver's prelighting color. TerrainMaterial.h:85
uses sun-derived snow normal; GlobalUniforms.cpp:60 produces it. This is an equivalent saved-input problem, although not a
geometry-displacement change. |
| Hide/show Tweaks or enter/leave Graphics | Implicit receiver-input mutation when retained override differs from mfSunAngle: Projects/BrokenEngineSandbox/Source/Graphics/Camera.cpp:82-93 selects a different sun-angle source based on these UI states. No slider write or Graphics recreation is necessary. A menu-controls-only fix does not settle this branch. |
| Multisampling / count / presentation / fullscreen extent / sample shading | Existing pipeline/swapchain-or-higher invalidation settles these recreation routes: Refresh:517-552 escalates; Destroy:690-704 reaches capture failure. They need no new identities. |
| Terrain Shadows / Smoke Detail / Smoke Area | Quality helpers set shadow multiplier/smoke pixels (GraphicsQualityWrappersBase.cpp:30-43); Refresh:564,583-589 escalates actual changes to kPipelines. Existing invalidation applies. |
| Minimum Ambient / shadow cadence / Smoke enable / stagger / Wind | Not proven geometric receiver mutations in this bounded trace. Minimum ambient and shadow cadence alter ordinary lighting; Smoke/Wind alter excluded visual systems (Lighting shader AGENTS Query Contracts excludes smoke as occluder). No new defect asserted for those controls. A common menu policy may cover them without per-control machinery. |

## Minimal correction evidence

The smallest existing graphics-resource boundary covering Water row, Water Foam, sampler edits and Defaults mutations is the
kCommandBuffers-or-higher branch in Graphics::Destroy (Graphics.cpp:685), already after worker/device waits at :647-673. Make an active
capture fail at every actual graphics recreation, rather than only indirectly when comparison resources are destroyed at kPipelines.
Preserve the existing status/error lifetime and normal tier resource preservation; no reason exists to promote water quality to expensive
pipeline recreation. This matches Graphics/AGENTS.md's current promise that Graphics recreation fails capture. Reusing that boundary alone
does not settle Time of Day or UI sun-source switching.

The existing common pre-publication capture boundary is GameBase::Render at :942 -> Lighting::ApplyComparisonSettings, before
SelectRenderCamera/UpdateRenderInterpolation and RenderGlobal. The smallest policy-level closure for the entire exposed Graphics-menu class
is to fail an active capture when navigation leaves its protected Tweaks context, before that next publication, including the implicit
sun-source change. Navigation remains usable; capture fails with a reason. This avoids item-by-item guards, hashes, snapshots or new
identities. It changes when capture may survive navigation and therefore is returned to main as a behavior choice, not implemented here. An
alternative retaining capture during navigation must hold the effective sun source stable and guard both Defaults and all Graphics mutable
contents; blocking only Water is insufficient. If main chooses disablement, reconcile Engine/Source/Ui/Screens/AGENTS.md ## Screen-Specific
Contracts, which currently says Time of Day stays enabled while Graphics is open. The binding handoff Decisions promise scene changes fail
capture; that promise is trusted over present controls.

## Acceptance trace

1. Reinspect the exact red sequence above with nonzero wave precondition. Prove either the Water action is rejected (including existing disabled-aware agent activation) or status becomes failed before any frame publishes the changed water and can trace/compose against the saved receivers. Trace the selected boundary's ordering through GameBase::Render and Graphics::RenderMainPresentAcquire; do not accept a late check after FinishCapture.
2. Inspect Defaults with all unrelated settings already default and Water nondefault; prove the same boundary rejects/cancels it without relying on an incidental pipeline rebuild.
3. Inspect each listed lower-tier mutation (command-buffer and sampler), plus Time of Day and hide-Tweaks/Graphics sun-source transitions. A claimed common fix must settle them explicitly. For a policy that fails when leaving Tweaks, prove the first pre-publication capture check occurs before GlobalUniforms reads the changed sun source, and new requests cannot bypass that check.
4. Preserve unchanged paused capture progression, settings-edit/unpause failure, completed/failed status lifetime, and re-enabled controls after capture. On the final composing frame UI edits occur after uniform publication; establish whether they affect only the following frame before labeling that final already-rendered image stale.
5. Plan 02 uses this static signal and appropriate builds after a fix; Plan 03 owns live harness acceptance. This diagnosis requests no build because source was not changed.

## Hypotheses ruled out

| Hypothesis | Killing check |
|---|---|
| Menu entry unpauses and triggers existing failure | GameBase navigation changes meUiState/mbShowImGui; GameFlags pause toggle is separate. |
| Any Graphics recreation already fails capture | Graphics Destroy only calls Lighting teardown at kPipelines; Water is kCommandBuffers and samplers also fall below the threshold. |
| Existing comparison-settings/count checks include Water | Lighting.h settings and LightingCommon.h publication contain no water quality; both checks admit unchanged records. |
| Fixture water remains flat regardless of configuration | Permitted End10000 gives positive fade at Close Z900; nonzero wave count and amplitude reach displacement. |
| Extending one Graphics disabled row settles the class | Defaults writes the same wrapper; sampler and sun-source routes remain outside that row. |

## Diagnosis limits

Root cause is proven statically; runtime observations were neither authorized nor performed. No instrumentation added. No source changed. Main must choose navigation-fails-capture versus preserving capture across navigation with stable receiver inputs; the latter is a behavior policy decision with a larger correction surface. This is not a broader repository audit.

## Visibility boundary proof

Main's proposed smallest cause removal is supported by current entry points. Whole-tree `rg -n
'uiCaptureRequest|requestedComparisonSettings' Engine/Source Projects` finds the only capture request increment at
TweaksScreenLighting.cpp:62 and its only external assignment at :68. The UI section is rendered only via visible Tweaks
(UiScreens.cpp:65-67). AgentCommandsClientGeneric.cpp:1017 exposes WriteProfile diagnostics; no direct Lighting request command or hidden
wrapper writer was found. Agent synthetic activation requires a rendered target and does not turn a hidden Graphics menu into an active
control surface.

Main.cpp:420-430 drains agent commands and advances synthetic input; :436 calls ProcessInput, which toggles mbShowImGui at
GameBase.cpp:125-128; :446 then calls ClientUpdate and :454 Render. GameBase::Render:942 calls ApplyComparisonSettings before camera
selection/update and RenderGlobal:962, so that call is an existing boundary at which to reject hidden-Tweaks capture and fail an active
capture before any frame publishes the changed effective sun or water inputs. All ordinary Graphics writes require Tweaks hidden because
UiScreens gates the screens exclusively. A visibility condition there therefore closes Water, Defaults, sampler, Time of Day and implicit
sun-source switching without per-control disablement, graphics tier promotion or receiver tracking. Navigation controls may remain
available; leaving visible Tweaks makes capture fail with an accurate reason. No new scene hash/identity is needed.

Crucial admission race: pressing Capture changes requestedComparisonSettings during ImGui Prepare (Graphics.cpp:213) after that frame's
ApplyComparisonSettings has already run. Service/admission occurs in the next Render at LightingComparison.cpp:140-169. F3 can hide Tweaks
during ProcessInput before that admission. A cancellation only on the F3 branch that tests active status would miss this pending request:
status can still be not-run/complete/failed, and existing admission would start receivers while hidden. The correction must check visibility
both for active-capture invalidation and for new-request admission at the existing ApplyComparisonSettings boundary (or equivalently retire
the pending request). A check only after the equality early-return is also insufficient for active requests because settings normally remain
unchanged while tracing.

Additional acceptance: queue Capture on one UI frame, hide Tweaks before the next Render, and prove the serviced request remains failed with
a visibility reason rather than entering receivers; then prove ordinary visible-Tweaks paused admission still works. For a live tracing
request, F3-off must fail before RenderGlobal reads SunAngle. Turning Tweaks back on may permit a fresh explicit request and must not
silently resurrect the failed one. The current static code order supports these checks; no runtime was run.
