# Plan 03 row 3 baseline drift diagnosis

## Scope and signal

Read-only source/history diagnosis at a49b6127, plus the authorized save-header patch.
No runtime commands, process inspection, harness locks, source changes, or baseline edits.
Supplied active-worker signal: loaded tick 400, resetToFresh false, counts 2/3/4/0/0;
close eye 261.853 instead of 156, wide 1130.245 instead of 665.117, near-same visible bounds;
over8 close 99.990%, wide 98.196%; historical close night/gold versus current daylight.
These current measurements and visual observations are supplied evidence, not rerun here.
Historical capture-output.txt independently records tick 400, render 413 and the old eyes.

## Root cause

Two upstream changes invalidate the old recipe: cd6d410c derives eye distance from framing
and true vertical FoV; 57c8a9d5 advances the sun by actual render-clock advance instead
of repeatedly adding the interpolation offset. Current paused time cannot reproduce old
wall-scheduled sun progression (CameraBase.cpp:363-368; GameBase.cpp:785-788).
This proves recipe drift; it does not establish that every changed pixel is upstream.

## Diagnosis evidence

- Historical base: temp-replay-steps.md names bc69eea5 and +60/+75-second screenshot times.
- At bc69eea5, CameraBase::CalculateMatricesAndVisibleArea uses gFieldOfView/aspect;
  WrapperBase.cpp sets gFieldOfView to 45. At 1920x1080 this is 25.3125 degrees vertical.
- Current CameraBase.h:78 fixes the framing reference at exactly 25.3125 degrees;
  GraphicsSettingsWrappersBase.cpp:15-16 defaults to perspective, true vertical FoV 15.
- Current CameraBase.cpp:363-368 gives eyeZ = targetZ + framing*tan(25.3125/2)/tan(15/2).
  Ratio 1.7056836249389 gives 6+150*ratio = 261.852543740835 and
  6+(665.116882324219-6)*ratio = 1130.2448731012: both supplied current eyes match.
- Framing retains the projected look-at-plane size. EyeZ alone is no longer a framing check;
  ground bounds and projected units are the meaningful scene checks. Elevated geometry
  can change projection with FoV despite identical look-at-plane framing.
- At bc69eea5, CameraBase::Update adds rFrameInterpolate.fDeltaTime*0.01 or *0.075
  every frame. GameBase::AdvanceRenderClock returns a sub-tick interpolation offset;
  FrameBase.cpp:188 stores that offset. At a fixed/starved render window it can stay
  nonzero, so repeated camera updates move the old sun even with no scene advancement.
- 57c8a9d5 replaces that increment with clamped mfRenderTime advancement. Current
  GameBase.cpp:627-635 freezes that clock on client pause; starvation also stops advance.
- CameraBase.cpp:318 resets sun to 1.8, but initial state alone cannot determine the old
  screenshot angles: all intervening render-frame offsets, resets and frame counts matter.
- Historical baseline/repeat JSON, logs and capture text contain no recorded SunAngle.
  query_profile has fps 120, not an integrated frame-by-frame history. Its publication
  counter does not recover the offset sum or piecewise day/night transitions.
- Thus the old caveat's wall-time/356-second-cycle description is incomplete. The source
  proves frame/phase dependence. Neither timestamps nor 1.8+elapsed*speed is an exact angle.
- GlobalUniforms.cpp:509,541-544 uses SunAngle for ambient/sun, shadows and water;
  the sun change therefore reaches ordinary visuals independently of local diffuse.
- cd6d410c also deliberately retunes WaterWrappersBase.cpp skybox specular defaults.
  A sun-only normalization cannot prove all remaining water differences are lighting regressions.

## Reproduction feasibility and supported controls

Current UI can set and hold a known sun: Debug F3; describe_ui; set_slider with full
label "##Sun Angle", window "Tweaks", value in [0,2*pi]; close F3. TweaksScreen.cpp:313
edits mfSunAngle directly; game Camera.cpp:81-87 returns it outside Graphics settings.
Set after loading and after client pause, before fixture capture. The slider is disabled
during receivers/tracing/composing, so it cannot disturb an ongoing reference capture.
The existing set_slider command activates and types into ImGui; no new harness is needed.
Current Sun Angle is not a registered wrapper, so get_wrapper cannot read that member.
Use an explicitly chosen recorded input; do not infer an old angle by matching pixels.
The Misc "Vertical Field of View" slider and "Orthographic Projection" toggle exist.
However the FoV wrapper clamps to [5,25] (WrapperBase.h:126-128); 25.3125 is unreachable.
At maximum FoV, minimum-framing eye is 157.937020025692, not 156. Changing zoom alone
cannot match both the old exact projection and eye; minimum framing is 150.
Comparison fixture cameras are not an ordinary-gameplay reproduction route.

## Proposed acceptance check and decision points

Keep the historical failed measurements and baseline immutable. Complete the already
approved single retake if pending, but a retake cannot restore the changed sun algorithm.
Do not declare the existing fallback passed solely from similar framing: check 3 requires
the same water, units, shadows and UI, and allows sun drift, not arbitrary appearance drift.
Smallest meaningful adaptation requires the user's approval: record a replacement control
pair from unchanged rebased upstream cd6d410c and compare the stripped candidate against it,
using the same loadable save/identity, ticks, framing, 1920x1080, perspective, FoV 15,
and an explicitly pinned sun (e.g. 1.8, chosen for the new check, not claimed historical).
Use existing UI/harness commands; record settings and scene metadata before both shots;
retain <=2% and side-by-side no-added-or-missing-local-glow requirements for the new pair.
This changes the reference, not the acceptance threshold; it is not authorized by the save patch.
Exact historical reproduction remains unavailable from supplied evidence and current UI.
No proposed source fix, guessed historical angles, new harness, or baseline waiver.

## Hypotheses ruled out and residuals

- Wrong camera framing as explanation for taller eyes: ruled out by both numerical predictions.
- Save rejection as explanation for this supplied run: ruled out by resetToFresh false/tick/counts.
- Lighting strip changed the sun accumulator or FoV defaults: diff cd6d410c..HEAD preserves them.
- All pixel differences are proved upstream: unresolved; no controlled matched-input render pair.
- Historical exact sun angle recovery: blocked by missing frame/offset history and sun recording.
- Existing row-3 literal eye recipe versus upstream camera intent: contradiction; neither waived.
- Build required: none for diagnosis. Instrumentation removed: none added. Changed files: none
  (this Temp evidence artifact only).
