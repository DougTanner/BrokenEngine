# Plan03 validation diagnosis

## Scope and reproducing signal

Read-only source/log diagnosis at a49b6127 on lighting-foundation-work; no runtime launched.
Captured signal: Temp/plan03-patched-runtime/client-agent.log:117-237.
Exact inspection: Select-String -Path Temp/plan03-patched-runtime/client-agent.log -Pattern 'DebugUtilsCallback|DEBUG_BREAK'.
Red result: 23 callbacks and 23 breaks: 10 descriptorCount-10776, 3 dstArrayElement-00321, 10 OutputNotConsumed.
The first and third families reach duplicate_message_limit=10; counts are capped, not total defect instances.
No source, instrumentation, settings, process, or build changes made; this evidence file alone was created.

## Root causes and trace

Descriptor cause: Water.vert:18 declares an unused scalar sampler at set 1 binding 7.
Ordinary Water.frag:41 declares 17 normal samplers at that binding; the receiver fragment has no binding 7.
LightingComparison.cpp:247-250 clones ordinary water PipelineInfo and replaces only its fragment shader.
WorldLightingShadowPipelines.cpp:253 supplies the 17-normal-texture descriptor entry at binding 7.
ShaderLayoutsBase.h:149 fixes kiWaterNormalCount=17; this is not a runtime texture count accident.
PipelineCreator.cpp:116-150 merges reflected bindings, taking max count and union of shader stages.
For the receiver, binding 7 therefore has count 1 and VERTEX only; binding 8 has count 1 and FRAGMENT only.
PipelineDescriptorWriter.cpp:477-487 resolves binding 7 from the descriptor index; :237-275 writes all 17 entries.
Its :319-342 filter tests binding existence, not descriptor count, so the scalar vertex declaration retains the write.
Set routing drops global writes 0/1, making this the reported pDescriptorWrites[2] after elevation and noise writes.
Pipeline.cpp:126-130 and writer :388-389 allocate/write per framebuffer; the three 00321 reports fit that loop.
Log :117-176 directly confirms count 17, binding 7 VERTEX, and spill into FRAGMENT bindings 8/15/16/17.
Both descriptor VUID families are consequences of this one malformed write, not independent lifetime errors.
The 00321 diagnostic's internal count-1 wording does not supersede the count-17 messages or host writer.
DataPacker ExportShader.cpp:211-215 deliberately passes --preserve-interface, preserving unused reflected interfaces.

Shader-output cause: ModelCommon.h:22 emits location 8 float fOutHeightAboveBase, assigned at :97.
LightingModelReceiver.frag:14-20 accepts locations 0-6 only; LightingNeutralModelReceiver.frag:3-4 accepts 0-1 only.
LightingComparison.cpp:268 pairs those receivers with ordinary ModelStatic/ModelSkinned vertices through :18.
The receiver fragments omit location 8, unlike ordinary ModelForward.h:65 and ModelShadow.frag:21.
Log :177-237 names precisely that missing location-8 fragment input; this is separate from descriptor updates.
InstanceManager.cpp:80-86 logs and breaks on both warning and error callbacks, explaining all 23 DEBUG_BREAKs.
The captured output messages are warnings, not proof of invalid execution or a device loss.
Other unused receiver varyings may be hidden by the warning cap; matching the existing full interface avoids them.

## Trigger and provenance

PrepareComparisonPipelines:210 returns unless capture status is receivers; :247-250 creates receiver water once.
Model receiver creation follows at :261-271 for each published model range absent from the comparison cache.
Thus requesting capture triggers these pipeline creations; fixture enable alone is not required for water failure.
Log :110 records Graphics::Destroy(4) before the failures, but recreation is not required to derive either mismatch.
git ls-tree cd6d410c -- Engine/Data/Shaders/Lighting shows neither receiver shader existed in the new base.
git diff cd6d410c HEAD -- Water.vert changes only elevation binding 5 to 4; unused noise binding 7 predates this work.
git diff cd6d410c HEAD -- ModelCommon.h is empty; the vertex output also predates this work.
Both new receiver shaders and the cloned-water creation path are already in lighting-foundation-handoff-20261029.
git diff that snapshot HEAD over all three receiver fragments is empty: no rebase/camera regression at these sites.
Classification: new lighting receiver pairings expose pre-existing declarations; neither mismatch existed as a receiver path in cd6d410c.

## Hypotheses ruled out

Wrong runtime array population: count 17 is the authored constant and host list, directly matching the log.
Reflection randomly assigns stages: reflected layout merge and shader declarations deterministically produce the reported stages/count.
Stale resource handles or synchronization: failure depends on immutable count/stage metadata before draws, regardless of handles.
One descriptor defect explains every callback: output warnings arise from vkCreateGraphicsPipelines and a distinct missing shader input.
Camera rebase introduced mismatch: unchanged receiver fragments versus October29 snapshot and unchanged ModelCommon disprove it.

## Minimum fix and proposed acceptance check

Remove Water.vert's unused noiseTextureSampler declaration; it has no references anywhere else in that file.
This removes receiver binding 7, so the existing filter drops the unnecessary normal-array write; ordinary fragment binding 7 remains.
Complete both model receiver fragment input declarations to mirror ModelCommon's locations 0-8 and exact types.
Keep receiver material/normal expressions unchanged; do not enable maintenance4 or suppress callbacks.
Build through /compile: BrokenEngineSandbox client Debug|x64 with Local data regeneration; ModelViewer Debug|x64 consumes the same data.
Server has no affected code; rebuild only if the manager's harness build envelope requires both game targets.
Retest through /agent-harness with validation enabled, clean logs, same restored save, fixture toggle and point capture, then wide capture.
Require capture to enter receivers (and complete for rows8/9), including ordinary and neutral animated model receivers.
Scan complete explicit client/server logs for every callback and DEBUG_BREAK, not only device loss or the capped VUID counts.
Zero occurrences of all three families and zero DEBUG_BREAK are the matching acceptance signal; retain original rows8/9 criteria.
No non-obvious external API assumption is needed: exact violated rules and official Vulkan specification URLs are in log :120/:138.
Residual: actual post-fix green runtime evidence remains manager-owned; warning cap prevents assigning each warning to one named pipeline.
