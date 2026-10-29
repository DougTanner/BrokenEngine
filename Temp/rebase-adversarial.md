# Rebase adversarial integration review

## Scope and checks

Reviewed the supplied pass/nontruncated 83-entry inventory against old tip d170b18b843a3a96af227cef467b824b4aa214c5 and integration against pinned main cd6d410c064a5a5f905bf68efaf28493ab6fc33a. Read-only source review; no runtime, builds, primary changes, or source edits. This evidence file is the sole authored artifact.

Independently compared every path changed only by incoming main (main delta minus old foundation
delta) with pinned main using git diff --quiet: 51 paths, zero mismatches. This covers the
incoming script, history data/SVG, plans and nonoverlapping documentation/C++ changes without
attributing their existing behavior to this integration. Inspected overlapping camera, shared
shader, render publication, settings, capture, ModelViewer and project metadata paths and their
dependent consumers. Recorded four-build success remains build evidence supplied by the manager,
not proof against the runtime defect below.

## Findings

ADV001 Critical Engine/Source/Graphics/CameraBase.cpp:495 — Fixture camera changes no longer
invalidate the visible-area quad-size latch, causing clipped water/composite coverage. The close
and wide fixture poses are (0,-600,900) and (0,-4000,6000), with the same 45-degree projection
(LightingFixture.cpp:55-56,126). ApplyComparisonSettings assigns the camera selection without
resetting CameraBase state (LightingComparison.cpp:103-140); UpdateComparisonCamera/ordinary
Update both call CalculateMatricesAndVisibleArea. The merge changed the zoom bucket and LOD
inputs from actual eye distance to mfCameraFraming (:470,495), which fixture selection never
updates. Its latch key (:508) contains the ordinary user FOV, projection flag, mesh dimensions
and framebuffer dimensions, with no fixture selection or effective fixture extent. Starting with
close fixture mode, waiting for ordinary framing to settle, then selecting wide leaves every
latch condition unchanged (:510). The code retains the close quad size and reconstructs the
final area with that smaller fixed span (:519-523), although the actual projected footprint is
roughly 6.7 times larger. RenderFrameGlobal publishes the truncated area
(GlobalUniforms.cpp:535), and Water.vert:41 builds all mesh vertices inside it; the receiver
water pass copies that same water draw. Entering a fixture directly from ordinary perspective
can likewise reuse an unrelated small span. Pre-rebase CameraBase computed fEyeDistance after
applying the explicit fixture eye, and used it for the bucket, so close-to-wide re-latched.
Smallest correction: preserve ordinary framing behavior while deriving fixture LOD/bucket from
its effective camera extent and explicitly invalidating/re-latching on fixture camera/mode
changes; ensure entering/exiting fixtures and event poses cannot reuse an incompatible span.
Retest close-to-wide and ordinary-to-fixture transitions at a settled framing and unchanged user
FOV/framebuffer.

## Traced clean

- User orthographic/FOV settings override the fixed comparison projection — refuted: CameraBase.cpp:388-398 applies the explicit fixture eye/up/clips/FOV after ordinary projection, forces perspective, and sets water eye to explicit eye.
- Empty render frames publish stale new shared fields — refuted: MainUniforms.cpp:249-258 writes matrix, real/water/model eyes and projection flag; both the empty branch (:286) and active branch (:336) call PopulateMainCamera. ModelViewerApplication.cpp:1143-1146 independently publishes its shared world-camera fields.
- Pausing leaves a wall-time-driven sun advancing during captures — refuted: GameBase.cpp:770-788 initializes sun delta to zero and derives it only from bounded render-clock advancement; CameraBase.cpp:135-139 consumes that delta. ModelViewer Camera.cpp:44 passes zero. The Tweaks live sun slider is disabled for receivers/tracing/composing at TweaksScreen.cpp:310-313, and ApplyComparisonSettings rejects capture-setting/paused-visibility changes at LightingComparison.cpp:109-112.
- Incompatible version20 graphics settings are silently consumed — refuted: GraphicsSettings.cpp:26 sets version21; :66-69 assert orthographic offset41, FOV44 and size48; save/load/reset retain both controls. FileManager.h:283 requires exact version equality and valid size before settings are used.
- Incoming project configuration collapse loses foundation compilation membership — refuted by project diffs against pinned main: incoming configurations remain, while foundation lighting members are added and retired lighting members removed; supplied successful client/server/ModelViewer/DataPacker builds cover build reachability.
- Incoming main-only scripts/plans/history/docs were inadvertently dropped — refuted by independent 51-path equality comparison. The capture script waits for the same initial-streaming log emitted by ModelViewer Main.cpp:299-302; the new schema is v2 and its removed stable-capture interface remains removed.

## API verification requests

None. ADV001 follows repository-owned latch arithmetic, immutable camera poses and direct consumers; no disputed external API proposition is required.

## Residuals

Runtime rendering and capture output were outside this read-only assignment. No runtime evidence is claimed. The manager's existing baseline-save rejection and unfinished plan03 acceptance remain outside this integration review; this review does not revise acceptance or authorize compatibility.
