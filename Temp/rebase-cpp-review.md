## Result

PASS: no reachable correctness defect found in the focused rebase integration review. Immutable
source boundary d170b18b843a3a96af227cef467b824b4aa214c5..1c3608533ba58d1a1327b51928ed4dced5426c53;
incoming main cd6d410c064a5a5f905bf68efaf28493ab6fc33a; old base
b56f20fa44d0424c2fdf5003157563684ed747f5. All 37 supplied targets covered for preservation and
integration. Existing foundation behavior and unrelated incoming-main design were not redesigned.
Concurrent documentation and three comment-only source corrections are outside this immutable
review.

## Conflict contracts

CameraBase::CalculateMatricesAndVisibleArea derives the ordinary eye from framing and true vertical
FOV, latches orthographic state, and supplies the framing water eye. Its comparison branch replaces
eye/target/view/projection with the explicit rebased pose, forces perspective, publishes that eye
for water, and retains the shadow-reference floor. LightingFixture::MakeCamera supplies XM_PIDIV4
and explicit near/far clips. GetRenderFraming retains ordinary framing and the foundation comparison
eye-height tuning; terrain detail, normal weights, UV speed, and wave fade use it. Ordinary framing
persistence, interpolation, LOD buckets and projection-sensitive visible-area latch remain main's
implementation. ShiftToRenderedCell moves the water eye alongside other cached positions.

PopulateMainCamera publishes both matrices, world/model eyes, water eye and projection flag. Its
normal and missing-camera-interpolate paths both invoke the helper; comparison shake remains zero.
GameBase::Render refreshes comparison cameras without a renderable camera frame. ModelViewer's own
publication writes water eye and projection flag, retains separate orbit model matrices, and uses
the shared projection in flight. BufferManager allocates sizeof(shaders::MainLayout);
TextureDescriptors binds that buffer with VK_WHOLE_SIZE. Shared ShaderMainLayout inclusion and
ShaderFunctions::ToEyeLine agree on the added water vec4 and direction W flag.

ComputeWorldSizedTexelArea uses main's framing reference angle and shadow reference. PopulateShadowArea retains cadence, history, coordinate shifts and visible-footprint crop refresh. Comparison keeps its previous floor rather than following ordinary FOV controls. Legacy texture publication and obsolete reflection path stay removed.

GraphicsSettings version21 is the single current format: size48; orthographic41, padding42, FOV44.
Save, whole-file validation, load, reset and wrapper defaults agree; FOV15 in [5,25], orthographic
false. ReadVersionedFile receives the type's current version before application. No obsolete
lighting setting was restored. Tweaks sun slider edits gpCamera->mfSunAngle directly and remains
inside the receivers/tracing/composing disabled block. Game Camera::SunAngle retains only the
Graphics-screen override. Sun delta plumbing matches the render clock, boot passes zero, and
ModelViewer flight passes zero to retain manual sun.

## Incoming main preservation and coverage

An independent object comparison of every main-only path (main delta minus old foundation delta,
both based on old base) found 51 paths and zero mismatches. Among the supplied 37 C++ targets, 21
are byte-identical to pinned main; the 16 combined paths are listed below. Main-only preservation
was complemented by reading changed functions and shared consumers: audio listener/fade framing;
billboard projection; collection tuple lookup; pusher per-layer registration/query and FrameBase
setup plus ShipBehaviours consumer; GameBase header/boot sun argument; flight-plane projection;
profile framing display; wrapper relocation and declarations; height lerp; menu hover helper and
both HUD callers; chat drawing; Tweaks projection registration; water defaults; client framing
persistence; game SunAngle; ModelViewer initial streaming after successful Render. These paths
retain their incoming implementation and bind to the merged names/contracts.

Combined paths: Engine/Data/Shaders/ShaderMainLayout.h; Engine/Source/GameBase.cpp;
Engine/Source/Graphics/CameraBase.cpp; Engine/Source/Graphics/CameraBase.h;
Engine/Source/Graphics/Managers/BufferManager.h; Engine/Source/Graphics/Render/GlobalUniforms.cpp;
Engine/Source/Graphics/Render/LightingUniforms.cpp; Engine/Source/Graphics/Render/MainUniforms.cpp;
Engine/Source/Graphics/Render/Render.h; Engine/Source/Graphics/Render/WaterUniforms.cpp;
Engine/Source/Ui/GraphicsSettings.cpp; Engine/Source/Ui/GraphicsSettingsWrappersBase.cpp;
Engine/Source/Ui/GraphicsSettingsWrappersBase.h;
Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.cpp;
Projects/ModelViewer/Source/Graphics/Camera.cpp;
Projects/ModelViewer/Source/ModelViewerApplication.cpp.

No C++ file addition/deletion or whole-file affinity change occurs in the supplied rebase target delta, so no new update-vcxproj trigger. Project XML was deliberately not read by this review. Serialized project/configuration verification and the supplied successful four-build envelope belong to manager evidence; source callers carry no retired configuration names.

## Decisive checks

- Get-Content Temp/rebase-review-targets.json: expected schemaVersion plus paths, 37 paths.
- git object comparisons across all main-only changed paths: 51 examined, zero mismatches.
- git diff --check d170b18b843a3a96af227cef467b824b4aa214c5 1c3608533ba58d1a1327b51928ed4dced5426c53: exit0.
- Source searches for retired eye-height/mMatPerspective/legacy texel identifiers and configuration names: no matches in engine and application source.
- All changed .cpp targets measured in one documented Measure-Tokens.ps1 -Path array -Json invocation: success. No size observation is raised: the focused integration delta does not justify splitting unrelated existing code.
- Independent source traces above confirm sibling shader review's camera-publication claims without treating its conclusions as proof.

## Scope and limits

Scope authorization, minimum sufficient change and KISS: PASS against the explicit preserve-both-intents instruction and fixed decisions. No API verdict request. No source edits, builds or runtime launches performed. This is integration source review, not fresh runtime acceptance; the unrelated saved-baseline incompatibility remains manager-owned and is not waived by this result.
