# Reverse-Z Depth

Revisit When: depth fighting is visible between ships, hex shields, water, or terrain at gameplay zoom.

## Context

`CameraBase::CalculateMatricesAndVisibleArea` (`Engine/Source/Graphics/CameraBase.cpp`) builds the projection with `XMMatrixPerspectiveFovRH`. The near clip is `kfNearClip` (1 m) and the far clip is `max(kfMinimumFarClip, eye distance * kfFarClipPerEyeDistance)`, which is `max(400, eye distance * 2.667)`. Depth uses the conventional mapping: near maps to 0 and far to 1. The viewport range (`ConfigureViewportScissor` in `Engine/Source/Graphics/Objects/PipelineCreator.cpp`) and the depth clear value (`Texture::RecordBeginRenderPass` in `Engine/Source/Graphics/Objects/Texture.cpp`) both use `kfMinDepth` = 0 and `kfMaxDepth` = 1 from `Texture.h`. Every pipeline compares with `VK_COMPARE_OP_LESS` (`PipelineCreator.cpp`).

`InstanceManager::SelectDepthFormat` (`Engine/Source/Graphics/Managers/InstanceManager.cpp`) takes the first format that supports a depth attachment with optimal tiling, in this order: `D32_SFLOAT`, `D32_SFLOAT_S8_UINT`, `D24_UNORM_S8_UINT`, `D16_UNORM`, `D16_UNORM_S8_UINT`. With a float format and conventional Z, float precision is finest near 0, where the perspective projection already concentrates precision. Distant geometry therefore gets the coarsest depth steps, and at RTS altitude nearly all geometry is distant.

The depth buffer is `SwapchainManager::mDepthTexture`. It is created with only `VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT`, and the HDR render pass attaches it with `storeOp` `DONT_CARE`. No shader samples depth and nothing copies it, so the code has no depth-based position reconstruction, depth fade, or soft particles today. The change touches only code that writes depth, tests it, or interprets projected depth.

## Design

Reverse the mapping so near maps to 1 and far maps to 0, and keep a 32-bit float depth format. The float exponent then offsets the projection's uneven distribution, giving nearly uniform precision over distance (Reed).

- **Projection.** Pass the near and far clips to `XMMatrixPerspectiveFovRH` in swapped order, or build a reversed-Z matrix directly. The projection matrix itself must do the reversal. Vulkan allows `minDepth` greater than `maxDepth` within [0, 1], but flipping only the viewport range computes `1 - z` after z has already been rounded, so it gains no precision.
- **Clear and compare.** Clear depth to 0 and compare with `VK_COMPARE_OP_GREATER` or `GREATER_OR_EQUAL`. This is the standard reversed-Z convention but was not verified against a source; confirm it from the references during implementation. Today `kfMaxDepth` sets both the viewport `maxDepth` and the clear value. Afterward the viewport stays [0, 1] while the clear becomes 0, so the clear can no longer use `kfMaxDepth`.
- **Format.** `D24_UNORM_S8_UINT`, `D16_UNORM`, and `D16_UNORM_S8_UINT` have evenly spaced depth values, so reverse-Z gains nothing on them; they still render correctly. The recommendation is to require a float format (`D32_SFLOAT` or `D32_SFLOAT_S8_UINT`) and remove the UNORM fallbacks. The alternative is to keep the fallbacks and document that devices using them keep today's precision.

### Depth sites (starting list, not exhaustive)

A grep of the current code found these sites. Re-run the inventory before implementing.

- `CameraBase::CalculateMatricesAndVisibleArea` sets the projection. It also finds the visible-area corners by passing screen z 0 and 1 to `XMVector3Unproject` as the ray start and end. Under reverse-Z the two ends swap but the line stays the same, so the intersection with the Z=0 plane should not change. Verify this.
- `CameraBase::WorldToScreen` returns projected depth in Z from `XMVector3Project`, and that value reverses meaning. Its only callers, in `Projects/BrokenEngineSandbox/Source/Agent/AgentScene.cpp`, read only X and Y.
- `PipelineCreator.cpp` sets `depthCompareOp` and calls `ConfigureViewportScissor`.
- `Texture::RecordBeginRenderPass` sets the depth clear value.
- `kfDepthBiasConstantFactor` and `kfDepthBiasSlopeFactor` in `PipelineCreator.cpp` are both -3 and apply under `PipelineFlags::kDepthBias`, which only the `Water` pipeline sets (`WorldLightingShadowPipelines.cpp`). Under conventional Z a negative bias pulls water toward the camera, so the sign must flip. The magnitude also needs re-tuning: for a float format, the constant bias scales with the exponent of the primitive's depth, and that exponent changes under reverse-Z.
- `Engine/Data/Shaders/Ui/UiDepthPrepass.vert` (`kPipelineUiDepthPrepass` in `PipelineManager.cpp`) writes clip z 0 so that opaque ImGui rectangles hide the world behind them. Under reverse-Z, z 0 is the far plane, so the shader must write 1.
- In the remaining depth-tested pipelines, depth comes from `mainLayout.f4x4ViewProjection` (`MainUniforms.cpp`), so the reversed matrix flips it without other changes. These are the model pipelines (`DynamicPipelines.cpp`, `Model/ModelCommon.h`), hex shields (`Objects/HexShield.vert`), particle rendering (`PipelineManager.cpp`, `Particles/*ParticlesRender.vert`), `Terrain`, and `Water`.
- Some shaders write a constant clip z: `Log.vert`, `Quads/QuadsFullscreen.vert`, and the visible-area mode in `Model/ModelCommon.h`. For each, confirm that it never draws with a depth test against `mDepthTexture`. Any shader that does must flip.
- `BillboardsRender.cpp` projects positions on the CPU with the same matrices. Its pipeline has no depth test, so it is unaffected.

## Out of scope

- Server, simulation, and CRC state. The projection and depth buffer are client-only.
- New depth readers such as soft particles, depth fades, or position reconstruction. None exist today, and any added later should use the reversed mapping from the start.

## Notes

- Client-only rendering, so there is no determinism or CRC exposure.
- Nathan Reed, "Depth Precision Visualized": https://www.reedbeta.com/blog/depth-precision-visualized/ (NVIDIA repost: https://developer.nvidia.com/content/depth-precision-visualized).
- Vulkan specification, viewport transform (`minDepth` may exceed `maxDepth` within [0, 1]): https://docs.vulkan.org/spec/latest/chapters/vertexpostproc.html
