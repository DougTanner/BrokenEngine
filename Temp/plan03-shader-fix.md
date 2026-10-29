# Plan03 shader validation fix

## Scope and attribution
Session baseline: cd6d410c064a5a5f905bf68efaf28493ab6fc33a; pre-fix HEAD a49b6127.
Accepted HARNESS-F-002; classification conformance + non_structural; no plan delta.
Owned changes: exactly the three shader regions below; concurrent Temp/lighting-plans and save edits untouched.
No builds, runtime/harness, staging, commits, primary-checkout edits, or new tests performed.

## Exact changed regions and snapshots
Engine/Data/Shaders/Water/Water.vert:18: remove unused scalar set 1 binding 7 noiseTextureSampler declaration.
Engine/Data/Shaders/Lighting/LightingModelReceiver.frag:21-22: add location 7 vec4 f4InColorAdd and location 8 float fInHeightAboveBase.
Engine/Data/Shaders/Lighting/LightingNeutralModelReceiver.frag:5-11: add locations 2-6 vec2 UV inputs, 7 vec4 color, 8 float height.
Temp/plan03-shader-prefixed/Water.vert is the exact pre-fix byte snapshot.
Temp/plan03-shader-prefixed/LightingModelReceiver.frag is the exact pre-fix byte snapshot.
Temp/plan03-shader-prefixed/LightingNeutralModelReceiver.frag is the exact pre-fix byte snapshot.
Temp/plan03-shader-before.patch is empty: all three matched HEAD before this worker edited them.
Temp/plan03-shader-fix.patch is the complete fix patch against that HEAD: 9 insertions, 1 deletion.

## Root cause confirmation
Accepted proof: Temp/plan03-validation-diagnosis.md ## Root causes and trace.
Close inspection confirms Water.vert never samples noiseTextureSampler; ordinary Water.frag:41 owns binding 7 as a 17-sampler array.
LightingWaterReceiver.frag has no binding 7; LightingComparison.cpp:247-250 clones water and replaces only its fragment stage.
WorldLightingShadowPipelines.cpp:253 still supplies all 17 normal samplers, as ordinary water requires.
PipelineCreator.cpp:116-150 merges reflected bindings; PipelineDescriptorWriter.cpp:321-342 filters by binding presence.
Removing the vertex declaration lets the existing filter omit the receiver's unused array write without changing ordinary water.
ModelCommon.h:14-22 emits locations 0-8; both ModelStatic.vert and ModelSkinned.vert include it.
LightingComparison.cpp:18,268 pairs those vertices with the two edited receivers.
Existing ModelForward.h:57-65 and ModelShadow.frag:13-21 mirror the complete interface, including unused inputs.
Receiver fragments now follow that established pattern; material and normal expressions remain byte-identical.

## Decisive checks and self-audit
Temp/plan03-shader-checks.txt records six PASS results from the focused source check command.
Both receiver location/type/name sequences equal all nine ModelCommon outputs after Out-to-In name normalization.
Water.vert contains neither noiseTextureSampler nor binding 7; ordinary fragment binding 7 remains unchanged.
BOM presence, LF/CRLF convention, and final-newline presence match all pre-fix byte snapshots.
Scoped git diff --check passed; final diff contains only the accepted declaration changes.
Claim: no extra shader dependency needs an edit -> tracked shader/pipeline searches -> confirmed below.
Claim: shading behavior remains intact -> snapshot/diff inspection -> function bodies and shader includes unchanged.

## Propagation searches
Tracked git grep of kShadersWaterWatervertCrc, receiver CRCs, and ModelCommon.h traced ordinary/receiver pairings and both vertex producers.
Tracked git grep of binding 7, pWaterNormalSamplers, and noiseTextureSampler covered both water fragments and the shared vertex.
Location declaration search covered all Lighting/Model .frag and .h siblings; ordinary and shadow model fragments already match.
ReceiverWater locations 0-3 match Water.vert outputs unchanged; fixture/terrain families use separate producers and are unaffected.
No CPU layout, descriptor writer, shader identity, serialization, client/server guard, or material propagation change is necessary.
Trigger outcomes: 2/2 settled (water binding and model interface); no unresolved code site.

## Documentation and membership
/update-claude-docs discovery returned pass; root, Engine/Data, shader hub, Water and Lighting governing documents read.
All guidance remains correct: no AGENTS.md edit required for these ordinary interface repairs.
Water AGENTS.md has pre-existing over-target advisory; not changed or trimmed.
Temp/plan03-shader-inventory.json records the documented baseline inventory run with status pass.
Owned membership candidates: both model receiver fragments, reason membership, because they were added before this fix versus session baseline.
/update-vcxproj may consume those two candidate paths if the parent has not already settled plan-wide membership; no project XML opened here.
This fix modifies existing files only and creates no additional project membership trigger versus pre-fix HEAD.

## Required independent verification
Build via /compile: BrokenEngineSandbox client Debug|x64, server Debug|x64, ModelViewer Debug|x64, with Local data regeneration.
No DataPacker C++ changed, so no separate DataPacker rebuild requested solely by this fix.
Manager dispatches /glsl-review and /comment-review; no comment text changed.
Manager dispatches /agent-harness: fixture toggle, point and wide captures, including ordinary and neutral animated receivers.
Require original plan03 rows 8/9 and zero callbacks/DEBUG_BREAK across complete client/server logs as prescribed by diagnosis.
No runtime-green claim is made by the focused source checks.
