# Row 3 projected-light reference diagnosis

## Scope and result

Read-only source diagnosis in the assigned existing worktree, candidate HEAD c1aa4847,
reference cd6d410c064a5a5f905bf68efaf28493ab6fc33a, historical baseline bc69eea5.
No runtime commands, branch switches, source/plan/data edits, builds, or process access.
This evidence file is the sole authored artifact.

PASS for the source-level reference-configuration root cause. Row 3 itself remains
unsettled. No image was supplied to this worker or inspected, so attribution of every
reported red/pink pixel and the 53.884% close / 43.028% wide over8 values is not claimed.
Those measurements and matched gates are dispatch-supplied observations.

## Root cause and reproducing signal

The replacement recipe omitted the zero-projected-light precondition: fresh reference
cd6d410c defaults Graphics Lighting to enabled, whereas historical bc69eea5 and the
candidate have already removed the projected-light pipeline and return zero gameplay
local diffuse. Thus comparing their default gameplay shots compares different lighting.

The deterministic static signal is this three-way inspection:

- `git show cd6d410c:Engine/Source/Ui/GraphicsSettingsWrappersBase.cpp`, line 28:
  gLightingEnabled defaults true. The recipe explicitly uses fresh settings, and its
  Commands and readiness / Matched shots sections never set this checkbox false.
- `git show cd6d410c:Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp`,
  lines 47-48: RecordLightingDeposit and RecordLightingSpreadPipeline precede the
  image render pass. Its deposit draws include point/area and hex-shield lighting.
- `git show bc69eea5:Engine/Data/Shaders/Lighting/LightingCompose.h` and the same
  current file, lines 7-10: disabled reference returns vec3(0). Both historical and
  current TextureDescriptors initialize SetLightingReference(..., nullptr, false).
  Historical CommandBufferRecordMain has no legacy deposit/spread calls, and
  bc69eea5 has no Engine/Source/Ui/LightingWrappersBase.cpp.

This signal is red without any dependence on perceived glow color. It proves the
reference-config mismatch, not that all measured pixel differences have this cause.

## Authority and intended behavior

Temp/lighting-plans/00-context.md ## Goal and authority quotes the user: the old
lighting system is completely replaced and should be stripped, leaving stubs.
Documents/Investigations/Engine/LightingFoundationHandoff.md ## What the foundation
provides explicitly requires legacy removal, gameplay diffuse exactly zero, and
ordinary sprites/shields/sun/IBL/shadows/smoke/water paths retained.
Its ## Acceptance table row 3/check 3 requires ordinary visuals and local glows
preserved with Comparison disabled. Temp/lighting-foundation-strip-plan.md
## Keep / simplify / strip table keeps legacy removal as a user requirement.

The user-approved current-main reference replaces commit identity and camera/sun
setup; it does not authorize restoring old lighting or changing candidate zero diffuse.
There is a contradiction between the zero-diffuse criterion and the recipe's fresh
main default Lighting=true. The user/source intent above is trusted; the setup must
be corrected explicitly, with existing captures retained as failed/history evidence.

## Existing control that isolates the legacy contribution

No reference source adaptation is required. On exact cd6d410c use the existing main
UI Graphics menu Lighting checkbox, checked false. This is the checkbox, not its
Low/Medium/High Lighting radio row, and not the Tweaks Comparison mode.

- Engine/Source/Ui/Screens/GraphicsMenuScreen.cpp:68 names window GraphicsMenu;
  line 166 calls WrapperToggle("Lighting", &gLightingEnabled).
- Engine/Source/Ui/MenuUtils.cpp WrapperToggle uses ImGui::Checkbox and Set(bool).
- Engine/Source/Frame/Collections/PointLights/PointLightsRender.cpp:167-168 gates
  only projected deposit indirect count; visible sprite indirect count stays siRendered.
- Engine/Source/Frame/Collections/AreaLights/AreaLightsRender.cpp:169 gates deposits.
- Engine/Source/Frame/Collections/HexShields/HexShieldsRender.cpp:123-124 retains
  ordinary shield indirect count and gates only the shield-light deposit count.
- Engine/Source/Graphics/Render/LightingUniforms.cpp:98-105 detects toggle change,
  sets gbLightingTemporalReset, and schedules the refresh even when disabled.
  Lines 356-379 disable spread draws but dispatch combine/temporal/history on that
  refresh. CommandBufferRecordMain clears spread attachments unconditionally.
- Engine/Source/Graphics/Render/Render.h:94-107 resolves the reset blend to 1;
  Engine/Data/Shaders/Lighting/LightingTemporal.comp:36-38 skips old-history mix.
  LightCombine.comp accumulates the zero spread textures and publishes RGB and
  ambient zero: default gCombineBlackTightness is 0 (LightingWrappersBase.cpp:70).
  This is a black publication, not a slow history decay requiring an arbitrary wait.
- VisibleLight.frag computes its output from sprite color/texture/intensity, with no
  projected-light sampler. HexShield.frag uses skybox/color/alpha independently;
  its diff from reference to candidate is empty. HexShield.vert's change removes
  the old alternate deposit projection; its ordinary f4Pipeline.x=0 path remains
  the same view-projection transform.

Runtime action for the manager's subsequent authorized reference run: open pause
menu via Escape, use describe_ui to click its Graphics entry, then describe_ui
in GraphicsMenu and click the full Lighting checkbox label if checked true.
The anticipated request is click {label:"Lighting",window:"GraphicsMenu",
describeUiAfter:true}; actual full labels and checked:false must come from that UI
reply. Follow the existing harness skill rather than guessing untranslated menu IDs.
Keep paused state, return to identical hidden ordinary UI, allow normal completed
render frames (click itself settles two), and retain before/after checked state,
frames/profile/scene gates and fresh close/wide images. This worker executed none.

## Why the Tweaks alternative is incomplete

Tweaks Lighting > Read has Day Final Multiplier and Night Final Multiplier,
both min 0, and GlobalUniforms.cpp:237-241 combines them into
fLightingTimeOfDayMultiplier and fSmokeLightingCombinedMultiplier. Setting both 0
does zero terrain, model diffuse, water projected direct/ambient, and projected
smoke terms without zeroing sun/IBL.

However Model.frag:337-360 enables ENABLE_SPECULAR_LIGHTING and accumulates raw
EWNS samples without fLightingTimeOfDayMultiplier. Thus those two sliders alone
are not equivalent to the stripped baseline. The existing Graphics Lighting=false
control zeros the sampled textures too, covering this local specular path. Likewise
setting just Water Intensity=0 misses water ambient and smoke. Do not use either
partial recipe as the proposed reference.

## Competing hypotheses checked

| Hypothesis / prediction | Check and verdict |
| --- | --- |
| Default main projected diffuse accounts for reference-only broad local glows; disabling its deposits removes that component. | Proven source mismatch through defaults, deposit chain, terrain/water/model consumers, and zero candidate seam. A controlled reference Lighting on/off pair must settle exact pixel attribution. |
| Candidate removed visible sprites or ordinary shield shading. | VisibleLight.frag/vert and HexShield.frag have empty cd6d410c..HEAD diffs; source retains separate sprite/shield draws. Their absence in any specific screenshot would need a separate runtime explanation; it is not proved by color alone. |
| Water skybox defaults diverged after rebase. | git diff cd6d410c HEAD -- Engine/Source/Ui/WaterWrappersBase.cpp is empty. Shared upstream retuning cannot explain this matched pair as a default mismatch. Water shader/material extraction still needs the ordinary-visual comparison; no blanket equivalence asserted. |
| Main intensity sliders alone produce a complete zero-local-light baseline. | Falsified by enabled model local specular path reading raw EWNS samples at Model.frag:337-360. |
| New foundation strip introduced the missing legacy glows relative to original bc69eea5. | Falsified as historical premise: bc69eea5 already has no legacy pipeline and disabled gameplay compose returns zero. The user replacement reference reintroduced the default-on legacy path. |

## Proposed acceptance and residual uncertainty

Retain existing current-main Lighting=true captures and failures. Explicitly add
Graphics Lighting=false as the main reference precondition; preserve exact commit,
same save/identity/ticks/FOV/projection/sun and all existing readiness gates.
Capture main close/wide with that control and compare to candidate Comparison Disabled
under the existing row 3 thresholds and retake rule. For causal confirmation preserve
a main same-pose Lighting on/off pair: broad projected terrain/water glow should vanish
while sprite/shield geometry remains. Do not waive a threshold or declare ordinary
visual preservation from this static diagnosis.

If zero-projected-light main still differs from foundation, inspect residual pixels
against unchanged ordinary paths/material extraction and treat it as an unresolved
in-scope acceptance failure. No restore of legacy candidate lighting is justified.

The manager may need to approve/update reference setup wording explicitly under its
existing scope authority; this worker does not change the plan or infer that approval.
No source change, build, added instrumentation, or unit test is proposed. A future
reference branch switch/build/run belongs to the manager and its harness/build workers.

Missing evidence: exact candidate screenshot paths were requested from the manager;
no active Temp directory was probed. Pixels and reference control behavior have not
been independently runtime-tested here. This limits image attribution, not the
deterministic proof that the existing reference config mismatches zero local diffuse.
