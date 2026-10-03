<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:57:18.353Z","dependsOn":[]} -->
# Remove unused 16-bit Vulkan feature plumbing

## Context

At baseline `270007c8dde36b36a7b46583d5c2670c89bb5138`, `DeviceManager::DeviceManager` (`Engine/Source/Graphics/Managers/DeviceManager.cpp:140-152`) requests both `storageBuffer16BitAccess` and `uniformAndStorageBuffer16BitAccess`. The latter has no demonstrated shader consumer. `InstanceManager.h:27-35` also queries all four 16-bit storage features, but the only readers are the disabled assertions in `InstanceManager::ValidatePhysicalDeviceCapabilities` (`InstanceManager.cpp:556-560`). `Engine/Data/Shaders/ShaderLayoutsBase.h:330` unconditionally defines `ENABLE_32_BIT_BOOL`, disabling these assertions and the matching `shaderInt16` request at `DeviceManager.cpp:210-212`.

The accepted cleanup removes the unused uniform-buffer permission and its dead query/assertion plumbing. It reduces misleading feature dependencies without changing particle representation or claiming a performance improvement. Removing a proven unnecessary request may broaden device eligibility; existing supported devices remain supported.

Compiled evidence collected during review covered 128 raw/optimized cached SPIR-V files and all 61 modules in the existing primary `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/Output/Data/Shader.pack`. Its SHA-256 was `759C3D42527337B3A16EA4AAE7E906ED428704B5F22F03B426E5F58933553D62`. The only capabilities were Shader, ImageQuery, StorageImageExtendedFormats, ShaderNonUniform, RuntimeDescriptorArray, and SampledImageArrayNonUniformIndexing (IDs 1, 49, 50, 5301, 5302, 5307). Neither Int16 nor a 16-bit storage capability appeared. This records an existing generated artifact, not a fresh export of the Git baseline. `DataPacker/Source/ExportJobs/ExportShader.cpp`, `ExportShader::Export`, owns preprocessing, Vulkan 1.2 compilation, optimization and chunk publication; `Common/DataFile.h`, `ShaderHeader::SpirvOffset`, owns the packed SPIR-V offset.

The retained storage-buffer request has a tooling consumer: [Vulkan-ValidationLayers at vulkan-sdk-1.4.341, gpuav_features.cpp](https://raw.githubusercontent.com/KhronosGroup/Vulkan-ValidationLayers/vulkan-sdk-1.4.341/layers/gpuav/core/gpuav_features.cpp), `Instance::AddFeatures:280-313`, enables supported `storageBuffer16BitAccess`; lines 372-374 report feature adjustments. This instrumentation setup also handles DebugPrintf. It does not request `uniformAndStorageBuffer16BitAccess`. Keeping the existing storage-buffer request avoids introducing that adjustment while preserving the supported tooling configuration. See also the [Khronos feature definitions](https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDevice16BitStorageFeatures.html); image-format width alone does not establish a buffer-member feature requirement.

## Design

Recommend this three-file cleanup because compiled shader evidence supports removing the uniform-buffer request, while validation tooling supplies a concrete reason to retain the storage-buffer request:

1. Delete `InstanceManager::mVkPhysicalDevice16BitStorageFeatures`. Set `mVkPhysicalDeviceVulkan12Features.pNext` directly to `kbShaderRealtimeClock ? &mVkPhysicalDeviceShaderClockFeaturesKHR : nullptr`, preserving the same optional tail and `mVkPhysicalDeviceFeatures2` head.
2. Delete the complete `#if !defined(ENABLE_32_BIT_BOOL)` assertion block in `ValidatePhysicalDeviceCapabilities`, including the `shaderInt16` assertion. Delete the matching conditional `shaderInt16` initializer in `DeviceManager::DeviceManager`.
3. Delete only `.uniformAndStorageBuffer16BitAccess = VK_TRUE` from the logical-device 16-bit feature initializer; aggregate zero initialization leaves it false. Preserve the structure, `.storageBuffer16BitAccess = VK_TRUE`, both explicit false bits, its `pFeatureChainTail`, and the Vulkan 1.2 link to it.

## Critical files

| File | Role |
|---|---|
| `Engine/Source/Graphics/Managers/InstanceManager.h` | Unused query member and Vulkan 1.2 query tail. |
| `Engine/Source/Graphics/Managers/InstanceManager.cpp` | Disabled feature assertions. |
| `Engine/Source/Graphics/Managers/DeviceManager.cpp` | Unused request and disabled core feature initializer. |
| `Engine/Data/Shaders/ShaderLayoutsBase.h` and particle shaders | Read-only active layout and shader-consumer evidence. |
| `DataPacker/Source/ExportJobs/ExportShader.cpp` and `Common/DataFile.h` | Read-only compiled/packed module provenance and extraction layout. |
| `Engine/Source/Graphics/Managers/AGENTS.md` | Read-only feature coherence contract. |

## In scope

Only the query member/tail, disabled assertion block, disabled `shaderInt16` initializer, and uniform-buffer initializer named in Design. Production changes are limited to those three manager files.

## Out of scope

Particle allocation/layout or inactive shader-branch cleanup; shader source, bytes, packs, CRC headers or export/version changes; removing or conditionally enabling `storageBuffer16BitAccess`; Vulkan 1.1 aggregate insertion; other feature requests, device selection, API version, toggles, logging, helpers, instrumentation, performance benchmarks, project membership, and unit tests.

## Acceptance criteria

| Criterion | Evidence |
|---|---|
| The unused query layer is gone | Repository search finds no `mVkPhysicalDevice16BitStorageFeatures`; diff removes the complete disabled assertion and request blocks. |
| Optional query tail remains correct | Static chain trace for both shader-clock flag values: Features2 -> Vulkan12 -> shader clock or null. |
| Tooling request is preserved | Logical-device 16-bit structure, storage-buffer true bit, false push/I/O bits, and optional tail remain unchanged; uniform-buffer bit is zero initialized. |
| Shader execution and storage remain unchanged | Shader/layout/export diff is empty; final accepted pack hash and capability evidence establish absence of a consumer of the removed request. |
| Supported-device startup and rendering work | Client build and validation-enabled existing-scene smoke run succeed with no related feature, shader, or chain validation errors. |
| Boundaries remain intact | Three-file production diff; Vulkan 1.2 minimum, record-once behavior, all other enabled features and client affinity unchanged. No simulation, CRC, wire, save/replay, threading or data-format changes. |

## Verification

Before editing, repeat the consumer searches and identify the exact pack used by the client. Compare its SHA-256 with the recorded pack above. If different, reconfirm capabilities for every module in that final baseline pack before removing the request. Use `Shader.manifest` chunk locations and the asserted `DataHeader`/`ChunkHeader` layout in `Common/DataFile.h`, validate chunk and SPIR-V magic, and locate each module with `ShaderHeader::SpirvOffset`. Disassemble extracted modules with the installed SDK's `spirv-dis.exe` and inspect every `OpCapability`; do not substitute a stale cache scan for the runtime pack. Scratch extraction is verification only, with no permanent scanner. If the changed pack requires the removed feature, stop and return the conflicting evidence rather than widening this plan into shader changes.

Build the client through `/compile`, retaining the existing runtime data. Hash the accepted pack before and after the cleanup and confirm no shader/layout or generated-data edits. Through `/agent-harness`, launch with ordinary validation enabled (without RenderDoc), render an existing scene, and inspect logs for related errors. No GPU-AV/DebugPrintf execution is claimed by the source-based preservation check. Hardware lacking the removed feature need not be available; record that limitation rather than claiming it was tested. No benchmark is necessary for a request/query cleanup with unchanged shader bytes.

Run the normal scoped affected-code, correctness, comment, style and documentation checks for the future C++ implementation. Preserve designated-initializer ordering and existing formatting. No explanatory comment is needed for obvious deletion. `/update-claude-docs` should confirm the manager contracts remain accurate; no AGENTS.md, architecture, project/filter or version edit is planned.

## Notes

- Future implementation is Change Workflow Tier 2: scoped Graphics device-feature enablement changes. The query-only deletions are mechanical, but the removed logical-device request determines the highest tier.
- Duplicate search across live Plans by feature names, query member, manager paths and feature-chain terms found no existing owner. `VulkanHostQueryResetInitialization.md` changes another feature in adjacent manager code; `SmokeSubgroupCompaction.md` adds a separate property query. Their implementation regions and outcomes are independent. No directional prerequisites or mandatory reciprocal Coordination constraints are needed; preserve their changes if already present.
- The source and compiled-artifact evidence above make this plan self-contained. Implementation should refresh baseline-sensitive evidence instead of relying on an investigation document or temporary review report.
