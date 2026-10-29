# Plan03 focused shader review

## Scope and verdict
PASS: exact three shader changes against a49b61276f424515ff80f746c58ac89f4e73f520.
Inventory status pass, truncated false, glsl count 3; 9 insertions and 1 deletion.
Reviewed HARNESS-F-002 repairs only; no source edits or new build initiated.
Authorization, minimality and KISS pass: declarations directly repair the accepted two causes.
Function bodies and includes remain unchanged; no new arithmetic or descriptor roles.
No correctness or performance finding; no external claim request; no sibling C++ review required.

## Descriptor trace
Water.vert removes its unused scalar set 1 binding 7 and has no remaining use.
Water.frag:41 retains pWaterNormalSamplers[kiWaterNormalCount]; ShaderLayoutsBase.h:149 fixes count 17.
WorldLightingShadowPipelines.cpp:253 supplies that full normal array for ordinary water.
LightingComparison.cpp:247-250 clones ordinary water and replaces only its fragment stage.
LightingWaterReceiver.frag declares no binding 7 and does not require normal-array samples.
PipelineCreator.cpp:116-150 merges declared stage bindings using max descriptor count.
PipelineDescriptorWriter.cpp:321-342 filters writes by either stage having the binding.
After deletion, receiver binding 7 has no declaration, so its inherited array write is filtered.
Ordinary water still reflects its fragment array count 17 and retains the matching write.

## Preserved optimized interfaces
ExportShader.cpp:187-198 compiles with glslangValidator -V --target-env vulkan1.2.
ExportShader.cpp:211-218 runs spirv-opt -O --preserve-interface --target-env=vulkan1.2 --scalar-block-layout.
ExportShader.cpp:89-90 reflects the optimized result; unused interfaces are intentionally retained.
Inspected existing optimized SPIR-V using C:/SDK/VulkanSDK/1.4.363.0/Bin/spirv-dis.exe.
Cache root: C:/Users/dougt/AppData/Local/BrokenEngine/DataPackerCache/BrokenEngineSandbox/Shaders.
Receiver optimized modules and Water.vert optimized module timestamp: 2026-10-29 16:00:35 EDT.
Receiver preprocessed source #line points to this session worktree and contains the corrected declarations.
ModelStatic.vert and ModelSkinned.vert optimized outputs have identical locations and types:
0 vec3, 1 vec3, 2 vec2, 3 vec2, 4 vec2, 5 vec2, 6 vec2, 7 vec4, 8 float.
LightingModelReceiver.frag and LightingNeutralModelReceiver.frag optimized inputs match that full sequence.
The two receivers retain locations 7/8 in OpEntryPoint and OpDecorate after optimization.
Water.vert optimized module has no Binding 7 decoration.
Water.frag optimized module has Binding 7 and OpTypeArray with uint_17 length.
LightingWaterReceiver.frag optimized module has no Binding 7 decoration.
ModelForward.h:57-65 and ModelShadow.frag:13-21 already use the same complete interface pattern.
Direct declaration repair is sufficient for both model producers under the actual compilation flags.

## Other checks and limits
Scoped git diff --check a49b6127 passed.
Measure-Tokens.ps1: Water.vert 893; LightingModelReceiver.frag 363; neutral receiver 129 bt-token-v1.
No size residual; no changed shared header or project-membership trigger against pre-fix HEAD.
Reviewed current shader bodies, producer interfaces, material helpers and receiver helper for this focused change.
Runtime retest and complete-log zero-callback acceptance belong to the manager's separate harness pass.
