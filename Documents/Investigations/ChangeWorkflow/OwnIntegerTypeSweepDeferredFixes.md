# OwnIntegerTypeSweep: deferred fixes

## OwnIntegerTypeSweep

### Out of the fix bound

#### Common/AlignedMemory (out-of-bound)
- F1 | `Common/AlignedMemory.h:26` | Rule 13 | Leave lines 26–28 unchanged. rule edge case: converting the byte product to signed `int64_t` arithmetic introduces overflow before line 27 checks it. For `T = uint64_t` and `iCount = 1i64 << 60`, the existing unsigned product is representable and passes the assertion, but its value, 2^63, exceeds `INT64_MAX`.

#### Common/DataFile (out-of-bound)
- F2 | `Common/DataFile.h:44` | Rule 13 | Leave unchanged; defer conversion to `int64_t iOffset`: serialized ChunkLocation member with an offset assertion; changing signedness changes the stored value domain.
- F3 | `Common/DataFile.h:45` | Rule 13 | Leave unchanged; defer conversion to `int64_t iSize`: serialized ChunkLocation member with an offset assertion; changing signedness changes the stored value domain.
- F4 | `Common/DataFile.h:96` | Rule 13 | Leave unchanged; defer conversion to `int64_t iTextureCount`: widens the serialized SceneHeader layout.
- F5 | `Common/DataFile.h:97` | Rule 13 | Leave unchanged; defer conversion to `int64_t iMaterialCount`: widens the serialized SceneHeader layout.
- F6 | `Common/DataFile.h:99` | Rule 13 | Leave unchanged; defer conversion to `int64_t iPad[3] {}`: changes explicit padding and the serialized SceneHeader layout.
- F7 | `Common/DataFile.h:100` | Rule 13 | Leave unchanged; defer conversion to `int64_t iPad2[4] {}`: changes explicit padding and the serialized SceneHeader layout.
- F14 | `Common/DataFile.h:152` | Rule 13 | Leave unchanged; defer conversion to `int64_t iNodeIndex`: widens the serialized AnimationChannel layout.
- F15 | `Common/DataFile.h:153` | Rule 13 | Leave unchanged; defer conversion to `int64_t iTargetPath`: widens the serialized AnimationChannel layout.
- F16 | `Common/DataFile.h:154` | Rule 13 | Leave unchanged; defer conversion to `int64_t iInterpolation`: widens the serialized AnimationChannel layout.
- F17 | `Common/DataFile.h:155` | Rule 13 | Leave unchanged; defer conversion to `int64_t iKeyframeStart`: widens the serialized AnimationChannel layout.
- F18 | `Common/DataFile.h:156` | Rule 13 | Leave unchanged; defer conversion to `int64_t iKeyframeCount`: widens the serialized AnimationChannel layout.
- F19 | `Common/DataFile.h:165` | Rule 13 | Leave unchanged; defer conversion to `int64_t iChannelStart`: widens the serialized AnimationClip layout.
- F20 | `Common/DataFile.h:166` | Rule 13 | Leave unchanged; defer conversion to `int64_t iChannelCount`: widens the serialized AnimationClip layout.
- F21 | `Common/DataFile.h:174` | Rule 13 | Leave unchanged; defer conversion to `int64_t iPad[2] {}`: changes explicit padding and the serialized ModelNode layout.
- F22 | `Common/DataFile.h:185` | Rule 13 | Leave unchanged; defer conversion to `int64_t iNodeCount`: widens the serialized Skeleton layout.
- F23 | `Common/DataFile.h:186` | Rule 13 | Leave unchanged; defer conversion to `int64_t iSkinJointCount`: widens the serialized Skeleton layout.
- F24 | `Common/DataFile.h:204` | Rule 13 | Leave unchanged; defer conversion to `int64_t iPad[3] {}`: changes explicit padding and the serialized MaterialInfo layout.
- F25 | `Common/DataFile.h:218` | Rule 13 | Leave unchanged; defer conversion to `int64_t iJointCount`: changes the GPU MeshData layout mirrored in Engine/Data/Shaders/Model/ModelCommon.h; that shader path is also excluded.
- F26 | `Common/DataFile.h:219` | Rule 13 | Leave unchanged; defer conversion to `int64_t iJointMatrixOffset`: changes the GPU MeshData layout mirrored in Engine/Data/Shaders/Model/ModelCommon.h; that shader path is also excluded.
- F27 | `Common/DataFile.h:235` | Rule 13 | Leave unchanged; defer conversion to `int64_t iAnimationCount`: widens the serialized AnimationHeader layout.
- F28 | `Common/DataFile.h:236` | Rule 13 | Leave unchanged; defer conversion to `int64_t iChannelCount`: widens the serialized AnimationHeader layout.
- F29 | `Common/DataFile.h:237` | Rule 13 | Leave unchanged; defer conversion to `int64_t iKeyframeCount`: widens the serialized AnimationHeader layout.
- F30 | `Common/DataFile.h:238` | Rule 13 | Leave unchanged; defer conversion to `int64_t iCubicKeyframeCount`: widens the serialized AnimationHeader layout.
- F31 | `Common/DataFile.h:239` | Rule 13 | Leave unchanged; defer conversion to `int64_t iMaterialCount`: widens the serialized AnimationHeader layout.
- F32 | `Common/DataFile.h:248` | Rule 13 | Leave unchanged; defer conversion to `int64_t iColorTextureIndex`: changes the serialized MaterialShaderData layout.
- F33 | `Common/DataFile.h:249` | Rule 13 | Leave unchanged; defer conversion to `int64_t iPhysicalDescriptorTextureIndex`: changes the serialized MaterialShaderData layout.
- F34 | `Common/DataFile.h:250` | Rule 13 | Leave unchanged; defer conversion to `int64_t iNormalTextureIndex`: changes the serialized MaterialShaderData layout.
- F35 | `Common/DataFile.h:251` | Rule 13 | Leave unchanged; defer conversion to `int64_t iOcclusionTextureIndex`: changes the serialized MaterialShaderData layout.
- F36 | `Common/DataFile.h:252` | Rule 13 | Leave unchanged; defer conversion to `int64_t iEmissiveTextureIndex`: changes the serialized MaterialShaderData layout.
- F37 | `Common/DataFile.h:253` | Rule 13 | Leave unchanged; defer conversion to `int64_t iPad[3] {}`: changes explicit padding and the serialized MaterialShaderData layout.
- F38 | `Common/DataFile.h:259` | Rule 13 | Leave unchanged; defer conversion to `int64_t iColorTextureSet`: changes the serialized MaterialShaderData layout and its PbrMaterialLayout contract.
- F39 | `Common/DataFile.h:260` | Rule 13 | Leave unchanged; defer conversion to `int64_t iPhysicalDescriptorTextureSet`: changes the serialized MaterialShaderData layout and its PbrMaterialLayout contract.
- F40 | `Common/DataFile.h:261` | Rule 13 | Leave unchanged; defer conversion to `int64_t iNormalTextureSet`: changes the serialized MaterialShaderData layout and its PbrMaterialLayout contract.
- F41 | `Common/DataFile.h:262` | Rule 13 | Leave unchanged; defer conversion to `int64_t iOcclusionTextureSet`: changes the serialized MaterialShaderData layout and its PbrMaterialLayout contract.
- F42 | `Common/DataFile.h:263` | Rule 13 | Leave unchanged; defer conversion to `int64_t iEmissiveTextureSet`: changes the serialized MaterialShaderData layout and its PbrMaterialLayout contract.
- F43 | `Common/DataFile.h:287` | Rule 13 | Leave unchanged; defer conversion to `int64_t iHeightmapWidth`: widens the serialized IslandHeader layout.
- F44 | `Common/DataFile.h:288` | Rule 13 | Leave unchanged; defer conversion to `int64_t iHeightmapHeight`: widens the serialized IslandHeader layout.
- F45 | `Common/DataFile.h:298` | Rule 13 | Leave unchanged; defer conversion to `int64_t iMeshVertexCount`: widens the serialized IslandHeader layout.
- F46 | `Common/DataFile.h:299` | Rule 13 | Leave unchanged; defer conversion to `int64_t iMeshIndexCount`: widens the serialized IslandHeader layout.
- F47 | `Common/DataFile.h:300` | Rule 13 | Leave unchanged; defer conversion to `int64_t iValidAreaVertexCount`: widens the serialized IslandHeader layout.

#### Common/StringUtils (out-of-bound)
- F2 | `Common/StringUtils.h:8` | Rule 13 | rule edge case: changing `size_t N` to `int64_t` breaks deduction from `std::span<char, N>`; casting the extent makes N non-deducible. Existing callers require extent deduction. Left unchanged.

#### Common/WindowsUtils (out-of-bound)
- F8 | `Common/WindowsUtils.cpp:191` | Rule 13 | `std::make_unique<uint8_t[]>(uiAttributeListSize)` supplies byte storage reinterpreted as `LPPROC_THREAD_ATTRIBUTE_LIST` and populated by Win32. Changing its element type crosses the excluded layout boundary and changes the allocated byte count.

#### Engine/Source/Frame/Collections/AreaLights/AreaLights (out-of-bound)
- F2 | `Engine/Source/Frame/Collections/AreaLights/AreaLights.h:46` | Rule 13 | `uint8_t* __restrict puiTypeIndices = nullptr;` — Leave unchanged: this `Collection<AreaLightsInterpolate>` column participates in `Members()` at line 51 and build-local serialization; widening its elements would change serialized bytes.

#### Engine/Source/Frame/Collections/Collection (out-of-bound)
- F1 | `Engine/Source/Frame/Collections/Collection.h:121` | Rule 13 | `uint8_t& ruiIndex` | Leave unchanged: converting to `int64_t& riIndex` requires widening callers’ excluded Collection members, including `PlayersInterpolate::suiAreaLightTypeIndex`, `suiBlasterTypeIndex`, and `suiExplosionTypeIndex` at `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.h:65–67`. Proposed rename `ruiIndex -> riIndex` has in-unit references at lines 121, 123, and 125; defer the signature conversion and caller propagation.

#### Engine/Source/Frame/Collections/Explosions/Explosions (out-of-bound)
- F10 | `Engine/Source/Frame/Collections/Explosions/Explosions.h:95` | Rule 13 | Leave unchanged; rule edge case: `ExplosionsSpawn.cpp:168` adds `rSpawnInformation.uiParticleCount + rType.uiBaseParticleCount` as `uint32_t` before assigning to `int64_t`. Widening this operand removes unsigned wrap and can change the particle-loop length and shared random-engine advancement.
- F12 | `Engine/Source/Frame/Collections/Explosions/Explosions.h:186` | Rule 13 | Leave unchanged; the integer column belongs to `Collection<ExplosionsInterpolate>` and is included in `SharedMembers()` at line 208. Changing its element width changes serialized collection layout and shared CRC bytes.
- F13 | `Engine/Source/Frame/Collections/Explosions/Explosions.h:195` | Rule 13 | Leave unchanged; the integer column belongs to `Collection<ExplosionsInterpolate>` and is included in `SharedMembers()` at line 208. Changing its element width changes serialized collection layout and shared CRC bytes.
- F16 | `Engine/Source/Frame/Collections/Explosions/Explosions.h:262` | Rule 13 | Leave unchanged; rule edge case: `ExplosionsSpawn.cpp:168` adds `rSpawnInformation.uiParticleCount + rType.uiBaseParticleCount` as `uint32_t` before assigning to `int64_t`. Widening this operand removes unsigned wrap and can change the particle-loop length and shared random-engine advancement.

#### Engine/Source/Frame/Collections/HexShields/HexShields (out-of-bound)
- F1 | `Engine/Source/Frame/Collections/HexShields/HexShields.h:48` | Rule 13 | `uint8_t* __restrict puiTypeIndices = nullptr;` is Collection state included in `Members()` at line 58 and `PersistentMembers()` at line 62; changing its element width crosses the Plan's excluded Collection layout boundary.

#### Engine/Source/Frame/Alignments (out-of-bound)
- F2 | `Engine/Source/Frame/Alignments.h:9` | Rule 13 | `uint32_t uiValue = 0;` is serialized and CRC-covered state; changing its width changes serialized bytes and state layout. Carried unchanged.

#### Engine/Source/Frame/Collision (out-of-bound)
- F7 | `Engine/Source/Frame/Collision.h:114` | Rule 13 | Leave unchanged. rule edge case: widening elements and updating both `sizeof(uint32_t)` occurrences here and at Collision.cpp:624 halves the fixed 64 MiB reservation's element ceiling from 16,777,216 to 8,388,608. Collision.cpp:607 can then hit `Common/StableVector.h:63`'s reservation assertion for previously supported layer counts; retaining the old memset width instead clears only half the elements.
- F8 | `Engine/Source/Frame/Collision.h:115` | Rule 13 | Leave unchanged. rule edge case: Collision.cpp:621–625 relies on unsigned increment wrapping to zero after UINT32_MAX and clearing stored generations. Conversion to `int64_t` removes that wrap, while assignments to the unchanged uint32_t buffer at line 645 truncate and equality checks at line 641 compare against the untruncated counter, breaking pair deduplication.

#### Engine/Source/Frame/FrameBase (out-of-bound)
- F1 | `Engine/Source/Frame/FrameBase.h:100` | Rule 13 | Leave unchanged: conversion to `int64_t` requires renaming `kCollectionCount` -> `kiCollectionCount`, including declarations at lines 100/102 and both references at line 128; propagation requires editing excluded `.agents/skills/add-collection/references/worker.md:83`.
- F2 | `Engine/Source/Frame/FrameBase.h:102` | Rule 13 | Leave unchanged: same required rename and excluded instruction reference as F1.
- F3 | `Engine/Source/Frame/FrameBase.h:145` | Rule 13 | Leave unchanged: deterministic frame counter is serialized at FrameBase.cpp:115/134/153 and CRC-covered at FrameBase.cpp:80; its type and unsigned increment behavior are excluded.
- F4 | `Engine/Source/Frame/FrameBase.h:147` | Rule 13 | Leave unchanged: frame-state counter is serialized at FrameBase.cpp:117/136; frame layout and serialized state are excluded.
- F5 | `Engine/Source/Frame/FrameBase.h:148` | Rule 13 | Leave unchanged: frame-state counter is serialized at FrameBase.cpp:118/137; frame layout and serialized state are excluded.
- F6 | `Engine/Source/Frame/FrameBase.h:156` | Rule 13 | Leave unchanged: reference binds directly to the excluded frame counters at FrameBase.h:222/228 and Collections/Sounds/SoundsUpdate.cpp:39; changing it to `int64_t&` requires changing those counters and changes unsigned increment behavior at line 158.
- F7 | `Engine/Source/Frame/FrameBase.h:190` | Rule 13 | Leave unchanged: conversion to `int64_t` requires renaming `kCollectionCount` -> `kiCollectionCount`, including declarations at lines 190/192 and both references at line 207; propagation requires editing excluded `.agents/skills/add-collection/references/worker.md:83`.
- F8 | `Engine/Source/Frame/FrameBase.h:192` | Rule 13 | Leave unchanged: same required rename and excluded instruction reference as F7.

#### Engine/Source/Frame/FrameRegistry (out-of-bound)
- F4 | `Engine/Source/Frame/FrameRegistry.h:55` | Rule 13 | `std::span<uint16_t> subscriberCounts {};` belongs to byte-backed scratch storage; widening changes the excluded layout.
- F5 | `Engine/Source/Frame/FrameRegistry.cpp:130` | Rule 13 | `std::span<uint16_t>(puiCounts, static_cast<size_t>(iEligibleRows))` binds the same excluded byte-backed subscriber-count storage identified in F4.

#### Engine/Source/Frame/FrameStaticData (out-of-bound)
- F2 | `Engine/Source/Frame/FrameStaticData.cpp:25` | Rule 13 | `common::Read(rStream, iCount)` at line 26 reads serialized save/network bytes directly into this variable; changing its type changes the serialized layout.

#### Engine/Source/Frame/GridCoord (out-of-bound)
- F1 | `Engine/Source/Frame/GridCoord.h:8` | Rule 13 | Widening `iX` changes serialized grid-save bytes through `Write` at line 31 and `Read` at line 37, called by `Engine/Source/File/GridSave.cpp:41` and `:109`.
- F2 | `Engine/Source/Frame/GridCoord.h:9` | Rule 13 | Widening `iY` changes serialized grid-save bytes through `Write` at line 32 and `Read` at line 38, called by `Engine/Source/File/GridSave.cpp:41` and `:109`.

#### Engine/Source/Frame/NavBuild (out-of-bound)
- F24 | `Engine/Source/Frame/NavBuild.h:42` | Rule 13 | NavData::polygonOffsets elements are serialized at NavCellData.cpp:447–450,474–479; widening changes wire bytes.
- F25 | `Engine/Source/Frame/NavBuild.h:43` | Rule 13 | visibilityEdgeA elements are serialized at NavCellData.cpp:453–456,482–488; widening changes wire bytes.
- F26 | `Engine/Source/Frame/NavBuild.h:44` | Rule 13 | visibilityEdgeB elements are serialized at NavCellData.cpp:458–460,490–492; widening changes wire bytes.

#### Engine/Source/Frame/NavBuildInternal (out-of-bound)
- F2 | `Engine/Source/Frame/NavBuildInternal.h:13` | Rule 13 | Changing `const std::vector<int32_t>& rPolygonOffsets` requires changing caller-owned polygon-offset storage, including `NavData::polygonOffsets` (`NavBuild.h:42`), serialized as int32_t values in `NavCellData.cpp:447–450,474–479`; this crosses the serialized-layout bound.

#### Engine/Source/Frame/NavCellData (out-of-bound)
- F73 | `Engine/Source/Frame/NavCellData.cpp:448` | Rule 13 | `common::Write(rStream, iOffset)` at line 450 serializes this value using its declared type; widening changes wire bytes.
- F75 | `Engine/Source/Frame/NavCellData.cpp:454` | Rule 13 | `common::Write(rStream, iEdge)` at line 456 serializes this value using its declared type; widening changes wire bytes.
- F76 | `Engine/Source/Frame/NavCellData.cpp:458` | Rule 13 | `common::Write(rStream, iEdge)` at line 460 serializes this value using its declared type; widening changes wire bytes.
- F77 | `Engine/Source/Frame/NavCellData.cpp:466` | Rule 13 | `common::Read(rStream, iVertexCount)` at line 467 reads the serialized 32-bit count; widening changes wire consumption.
- F78 | `Engine/Source/Frame/NavCellData.cpp:474` | Rule 13 | `common::Read(rStream, iPolygonCount)` at line 475 reads the serialized 32-bit count; widening changes wire consumption.
- F79 | `Engine/Source/Frame/NavCellData.cpp:477` | Rule 13 | This reference aliases a serialized `NavData::polygonOffsets` element and is read directly at line 479; widening requires changing serialized storage and wire bytes.
- F80 | `Engine/Source/Frame/NavCellData.cpp:482` | Rule 13 | `common::Read(rStream, iEdgeCount)` at line 483 reads the serialized 32-bit count; widening changes wire consumption.
- F81 | `Engine/Source/Frame/NavCellData.cpp:486` | Rule 13 | This reference aliases a serialized `NavData::visibilityEdgeA` element and is read directly at line 488; widening requires changing serialized storage and wire bytes.
- F82 | `Engine/Source/Frame/NavCellData.cpp:490` | Rule 13 | This reference aliases a serialized `NavData::visibilityEdgeB` element and is read directly at line 492; widening requires changing serialized storage and wire bytes.

#### Engine/Source/Graphics/Managers/BufferManager (out-of-bound)
- F1 | `Engine/Source/Graphics/Managers/BufferManager.cpp:22` | Rule 13 | `uint16_t puiQuads[]` is copied into the GPU index buffer at line 35 with `VK_INDEX_TYPE_UINT16`; changing element width changes GPU bytes.
- F2 | `Engine/Source/Graphics/Managers/BufferManager.cpp:134` | Rule 13 | `uint16_t puiIndices[]` is copied into the GPU index buffer at line 151 with `VK_INDEX_TYPE_UINT16`.
- F3 | `Engine/Source/Graphics/Managers/BufferManager.cpp:161` | Rule 13 | `uint16_t puiIndices[kiCircles * kiSegments * 2]` is copied into the GPU index buffer at line 194 with `VK_INDEX_TYPE_UINT16`.
- F4 | `Engine/Source/Graphics/Managers/BufferManager.cpp:203` | Rule 13 | `uint16_t puiIndices[kiSegments * 2]` is copied into the GPU index buffer at line 227 with `VK_INDEX_TYPE_UINT16`.
- F5 | `Engine/Source/Graphics/Managers/BufferManager.cpp:235` | Rule 13 | `uint16_t puiIndices[]` is copied into the GPU index buffer at line 247 with `VK_INDEX_TYPE_UINT16`.
- F27 | `Engine/Source/Graphics/Managers/BufferManager.cpp:724` | Rule 13 | `std::vector<uint32_t>& rIndices` elements are the water mesh's GPU index bytes, uploaded at lines 785–786 with `VK_INDEX_TYPE_UINT32`; changing the element type changes the GPU layout.
- F28 | `Engine/Source/Graphics/Managers/BufferManager.cpp:770` | Rule 13 | `std::vector<uint32_t> indices;` elements are uploaded as GPU index bytes at lines 785–786 with `VK_INDEX_TYPE_UINT32`.

#### Engine/Source/Graphics/Managers/CommandBufferRecordGlobal (out-of-bound)
- F14 | `Engine/Source/Graphics/Managers/CommandBufferRecordGlobal.cpp:127` | Rule 13 | `uint32_t pWindResetCommand[3] {0, 1, 1};` is uploaded through vkCmdUpdateBuffer; widening changes GPU dispatch layout and bytes.
- F16 | `Engine/Source/Graphics/Managers/CommandBufferRecordGlobal.cpp:259` | Rule 13 | `uint32_t pResetCommand[3] {0, 1, 1};` is uploaded through vkCmdUpdateBuffer; widening changes GPU dispatch layout and bytes.

#### Engine/Source/Graphics/Managers/CommandBufferRecordMain (out-of-bound)
- F5 | `Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp:364` | Rule 13 | rule edge case: `vkCmdBindVertexBuffers` takes `&vkVertexOffset` as `const VkDeviceSize*`; replacing `VkDeviceSize` with `int64_t` makes the pointer incompatible, and a value cast cannot supply the addressable argument. The out-parameter exception does not apply to this input pointer.

#### Engine/Source/Graphics/Managers/DeviceManager (out-of-bound)
- F6 | `Engine/Source/Graphics/Managers/DeviceManager.cpp:321` | Rule 13 | Elements hold opaque Vulkan pipeline-cache bytes passed to `vkGetPipelineCacheData` at line 322 and serialized through `reinterpret_cast<const char*>` at line 329; changing element width crosses a layout boundary.
- F7 | `Engine/Source/Graphics/Managers/DeviceManager.cpp:325` | Rule 13 | The span views the serialized pipeline-cache byte buffer for CRC calculation; changing element width changes the byte-layout contract.
- F8 | `Engine/Source/Graphics/Managers/DeviceManager.cpp:378` | Rule 13 | Elements receive serialized pipeline-cache bytes at line 418, supply `memcpy` sources at lines 426, 436, and become Vulkan initial cache data at line 447.
- F9 | `Engine/Source/Graphics/Managers/DeviceManager.cpp:427` | Rule 13 | The span views serialized pipeline-cache bytes for CRC verification; changing element width changes the byte-layout contract.

#### Engine/Source/Graphics/Managers/SwapchainManager (out-of-bound)
- F3 | `Engine/Source/Graphics/Managers/SwapchainManager.cpp:289` | Rule 13 | rule edge case: `uint32_t pQueueFamilyIndices[]` feeds `VkSwapchainCreateInfoKHR::pQueueFamilyIndices`, requiring `const uint32_t*`; int64_t elements break the pointer type and Vulkan element layout.
- F7 | `Engine/Source/Graphics/Managers/SwapchainManager.cpp:578` | Rule 13 | rule edge case: `uint32_t uiCurrentFramebufferIndex` feeds `VkPresentInfoKHR::pImageIndices`, requiring `const uint32_t*`; int64_t storage breaks the pointer type and Vulkan element layout. The existing int64_t iFramebufferIndex already holds our own value.

#### Engine/Source/Graphics/Managers/TextureManager (out-of-bound)
- F6 | `Engine/Source/Graphics/Managers/TextureManager.cpp:783` | Rule 13 | `iWidth` crosses the GPU push-constant layout boundary through `std::bit_cast<float>`; widening breaks its four-byte representation. Type unchanged; initializer identifier updated under F4.
- F7 | `Engine/Source/Graphics/Managers/TextureManager.cpp:784` | Rule 13 | `iHeight` crosses the GPU push-constant layout boundary through `std::bit_cast<float>`; widening breaks its four-byte representation. Type unchanged; initializer identifier updated under F5.

#### Engine/Source/Graphics/Render/MainUniforms (out-of-bound)
- F1 | `Engine/Source/Graphics/Render/MainUniforms.cpp:572` | Rule 13 | rule edge case: persistent `++siRenderCount` has no bound or reset; widening to `int64_t` then casting into GPU field `int32_t iRenderNumber` cannot satisfy the required always-fits narrowing check after INT32_MAX renders.

#### Engine/Source/Graphics/Screenshot (out-of-bound)
- F11 | `Engine/Source/Graphics/Screenshot.cpp:365` | Rule 13 | rule edge case: changing `std::numeric_limits<uint16_t>::max()` to `int64_t` changes the UNORM16 normalization divisor and encoded pixel values.
- F15 | `Engine/Source/Graphics/Screenshot.cpp:468` | Rule 13 | `std::vector<uint8_t>` crosses the external byte-layout boundary at `stbi_write_png`; widening elements changes encoded pixels. Preserve the corresponding `uint8_t` cast.

#### Engine/Source/Network/Client/Client (out-of-bound)
- F2 | `Engine/Source/Network/Client/Client.cpp:166` | Rule 13 | Leave unchanged; rule edge case: subtraction at line 168 relies on modulo-2^32 arithmetic when ENet's cumulative received-byte counter wraps. Widening produces a negative delta.
- F3 | `Engine/Source/Network/Client/Client.cpp:167` | Rule 13 | Leave unchanged; rule edge case: subtraction at line 169 relies on modulo-2^32 arithmetic when ENet's cumulative sent-byte counter wraps. Widening produces a negative delta.
- F4 | `Engine/Source/Network/Client/Client.cpp:193` | Rule 13 | Leave unchanged; its elements are ENet packet bytes consumed by network deserialization. Widening changes the byte layout.
- F5 | `Engine/Source/Network/Client/Client.cpp:196` | Rule 13 | Leave this parameter and its declaration at Client.h:82 unchanged; the span represents serialized network bytes.
- F6 | `Engine/Source/Network/Client/Client.cpp:244` | Rule 13 | Leave unchanged; the vector stores serialized game-message payload bytes consumed by game network readers.
- F10 | `Engine/Source/Network/Client/Client.cpp:366` | Rule 13 | Leave this parameter and Client.h:126 unchanged; its elements are serialized full-state packet bytes.
- F13 | `Engine/Source/Network/Client/Client.cpp:434` | Rule 13 | Leave this parameter and Client.h:127 unchanged; its elements are serialized static-data packet bytes.
- F16 | `Engine/Source/Network/Client/Client.cpp:486` | Rule 13 | Leave this parameter and Client.h:128 unchanged; its elements are serialized update/resend packet bytes.
- F17 | `Engine/Source/Network/Client/Client.cpp:542` | Rule 13 | Leave unchanged; it views compressed serialized status-change bytes. The existing `static_cast<size_t>` is at the span-extent API boundary and stays.
- F18 | `Engine/Source/Network/Client/Client.cpp:574` | Rule 13 | Leave this parameter and Client.h:129 unchanged; its elements are serialized debug-frame packet bytes.
- F19 | `Engine/Source/Network/Client/Client.cpp:595` | Rule 13 | Leave this parameter and Client.h:130 unchanged; its elements are serialized connection-response packet bytes.
- F21 | `Engine/Source/Network/Client/Client.cpp:645` | Rule 13 | Leave this parameter and Client.h:131 unchanged; its elements are serialized subscribe-accept packet bytes.
- F24 | `Engine/Source/Network/Client/Client.cpp:733` | Rule 13 | Leave this parameter and Client.h:132 unchanged; its elements are serialized unsubscribe-acknowledgement packet bytes.
- F26 | `Engine/Source/Network/Client/Client.cpp:750` | Rule 13 | Leave this parameter and Client.h:133 unchanged; its elements are serialized load-notification packet bytes.
- F27 | `Engine/Source/Network/Client/Client.cpp:768` | Rule 13 | Leave this parameter and Client.h:134 unchanged; its elements are serialized timespeed-update packet bytes.
- F31 | `Engine/Source/Network/Client/Client.h:101` | Rule 13 | Leave the nested payload vector's element type unchanged; its bytes are passed to game network deserializers at Projects/BrokenEngineSandbox/Source/Network/PlayerEvents.cpp:21,27,51.
- F32 | `Engine/Source/Network/Client/Client.h:149` | Rule 13 | Leave unchanged; rule edge case: Client.cpp:168 requires both operands to retain unsigned 32-bit subtraction across ENet counter rollover. Widening this member changes that arithmetic.
- F33 | `Engine/Source/Network/Client/Client.h:150` | Rule 13 | Leave unchanged; rule edge case: Client.cpp:169 requires both operands to retain unsigned 32-bit subtraction across ENet counter rollover. Widening this member changes that arithmetic.

#### Engine/Source/Network/NetworkCursor (out-of-bound)
- F1 | `Engine/Source/Network/NetworkCursor.h:9` | Rule 13 | Leave unchanged: the element type defines the network byte buffer; widening changes wire bytes and cursor advancement.
- F2 | `Engine/Source/Network/NetworkCursor.h:15` | Rule 13 | Leave unchanged: the return type represents a serialized 8-bit network value.
- F3 | `Engine/Source/Network/NetworkCursor.h:20` | Rule 13 | Leave unchanged: the return type represents a serialized 16-bit network value.
- F4 | `Engine/Source/Network/NetworkCursor.h:22` | Rule 13 | Leave unchanged: line 23 reads network bytes directly into this object's representation.
- F5 | `Engine/Source/Network/NetworkCursor.h:23` | Rule 13 | Leave unchanged: this span exposes the serialized object's bytes; widening its element type changes the byte count and cursor advancement.
- F6 | `Engine/Source/Network/NetworkCursor.h:27` | Rule 13 | Leave unchanged: the return type represents a serialized 32-bit network value.
- F7 | `Engine/Source/Network/NetworkCursor.h:29` | Rule 13 | Leave unchanged: line 30 reads network bytes directly into this object's representation.
- F8 | `Engine/Source/Network/NetworkCursor.h:30` | Rule 13 | Leave unchanged: this span exposes the serialized object's bytes; widening its element type changes the byte count and cursor advancement.
- F9 | `Engine/Source/Network/NetworkCursor.h:34` | Rule 13 | Leave unchanged: the return type represents a serialized 32-bit network value.
- F10 | `Engine/Source/Network/NetworkCursor.h:36` | Rule 13 | Leave unchanged: line 37 reads network bytes directly into this object's representation.
- F11 | `Engine/Source/Network/NetworkCursor.h:37` | Rule 13 | Leave unchanged: this span exposes the serialized object's bytes; widening its element type changes the byte count and cursor advancement.
- F12 | `Engine/Source/Network/NetworkCursor.h:44` | Rule 13 | Leave unchanged: this span exposes the serialized object's bytes; widening its element type changes the byte count and cursor advancement.
- F13 | `Engine/Source/Network/NetworkCursor.h:48` | Rule 13 | Leave unchanged: the return type represents an unsigned 64-bit network value; conversion could change values above `INT64_MAX`.
- F14 | `Engine/Source/Network/NetworkCursor.h:50` | Rule 13 | Leave unchanged: line 51 reads network bytes directly into this object's representation.
- F15 | `Engine/Source/Network/NetworkCursor.h:51` | Rule 13 | Leave unchanged: this span exposes the serialized object's bytes; widening its element type changes the byte count and cursor advancement.
- F16 | `Engine/Source/Network/NetworkCursor.h:58` | Rule 13 | Leave unchanged: this span exposes the serialized object's bytes; widening its element type changes the byte count and cursor advancement.
- F17 | `Engine/Source/Network/NetworkCursor.h:65` | Rule 13 | Leave unchanged: this span exposes the serialized object's bytes; widening its element type changes the byte count and cursor advancement.
- F18 | `Engine/Source/Network/NetworkCursor.h:96` | Rule 13 | Leave unchanged: the element type defines the network byte buffer; widening changes wire bytes and cursor advancement.
- F19 | `Engine/Source/Network/NetworkCursor.h:102` | Rule 13 | Leave unchanged: line 104 writes this value directly into an 8-bit network field.
- F20 | `Engine/Source/Network/NetworkCursor.h:107` | Rule 13 | Leave unchanged: line 109 serializes this parameter's object representation.
- F21 | `Engine/Source/Network/NetworkCursor.h:109` | Rule 13 | Leave unchanged: this span exposes the serialized object's bytes; widening its element type changes the byte count and wire output.
- F22 | `Engine/Source/Network/NetworkCursor.h:112` | Rule 13 | Leave unchanged: line 114 serializes this parameter's object representation.
- F23 | `Engine/Source/Network/NetworkCursor.h:114` | Rule 13 | Leave unchanged: this span exposes the serialized object's bytes; widening its element type changes the byte count and wire output.
- F24 | `Engine/Source/Network/NetworkCursor.h:117` | Rule 13 | Leave unchanged: line 119 serializes this parameter's object representation.
- F25 | `Engine/Source/Network/NetworkCursor.h:119` | Rule 13 | Leave unchanged: this span exposes the serialized object's bytes; widening its element type changes the byte count and wire output.
- F26 | `Engine/Source/Network/NetworkCursor.h:124` | Rule 13 | Leave unchanged: this span exposes the serialized object's bytes; widening its element type changes the byte count and wire output.
- F27 | `Engine/Source/Network/NetworkCursor.h:127` | Rule 13 | Leave unchanged: line 129 serializes this parameter's object representation; its unsigned range also exceeds `int64_t`.
- F28 | `Engine/Source/Network/NetworkCursor.h:129` | Rule 13 | Leave unchanged: this span exposes the serialized object's bytes; widening its element type changes the byte count and wire output.
- F29 | `Engine/Source/Network/NetworkCursor.h:134` | Rule 13 | Leave unchanged: this span exposes the serialized object's bytes; widening its element type changes the byte count and wire output.
- F30 | `Engine/Source/Network/NetworkCursor.h:141` | Rule 13 | Leave unchanged: this span exposes the serialized object's bytes; widening its element type changes the byte count and wire output.
- F31 | `Engine/Source/Network/NetworkCursor.h:152` | Rule 13 | Leave unchanged: the template argument fixes the serialized grid coordinate's 32-bit wire width.
- F32 | `Engine/Source/Network/NetworkCursor.h:153` | Rule 13 | Leave unchanged: the template argument fixes the serialized grid coordinate's 32-bit wire width.

#### Engine/Source/Network/NetworkDiscoveryResponder (out-of-bound)
- F1 | `Engine/Source/Network/NetworkDiscoveryResponder.cpp:45` | Rule 13 | `uiMagic` receives network bytes directly; changing its width changes wire layout and accepted packet length.
- F3 | `Engine/Source/Network/NetworkDiscoveryResponder.cpp:50` | Rule 13 | `uiResponse` is sent directly as bytes; changing its width changes wire bytes.

#### Engine/Source/Network/NetworkDiscoveryScanner (out-of-bound)
- F1 | `Engine/Source/Network/NetworkDiscoveryScanner.cpp:51` | Rule 13 | Leave unchanged: lines 57 and 65 send its raw bytes using `sizeof(uiMagic)`; conversion to `int64_t` would change the discovery wire payload from 4 bytes to 8 bytes.
- F2 | `Engine/Source/Network/NetworkDiscoveryScanner.cpp:78` | Rule 13 | Leave unchanged: line 80 receives directly into its bytes using `sizeof(uiMagic)`, and line 81 validates that length; conversion to `int64_t` would change the required discovery reply length from 4 bytes to 8 bytes.

#### Engine/Source/Network/NetworkManager (out-of-bound)
- F11 | `Engine/Source/Network/NetworkManager.h:47` | Rule 13 | Both template arguments in `std::span<const uint8_t> packetSpan = rWorkbuffer.Span<uint8_t>();` describe serialized packet bytes passed to `enet_packet_create` at line 50; widening changes byte interpretation and packet extent.
- F12 | `Engine/Source/Network/NetworkManager.h:66` | Rule 13 | `rWorkbuffer.PushBack<uint8_t>(static_cast<uint8_t>(eType));` fixes the serialized packet tag width to one byte; widening changes wire bytes.

#### Engine/Source/Network/NetworkMessages (out-of-bound)
- F1 | `Engine/Source/Network/NetworkMessages.h:15` | Rule 13 | `PacketPayload::iSize` is serialized by message visitors as a 32-bit wire length.
- F2 | `Engine/Source/Network/NetworkMessages.h:29` | Rule 13 | The template argument defines the one-byte packet discriminator on the wire.
- F3 | `Engine/Source/Network/NetworkMessages.h:32` | Rule 13 | This serializer overload writes a one-byte wire field.
- F4 | `Engine/Source/Network/NetworkMessages.h:34` | Rule 13 | The template argument defines a one-byte wire field.
- F5 | `Engine/Source/Network/NetworkMessages.h:36` | Rule 13 | This serializer overload writes a two-byte wire field.
- F6 | `Engine/Source/Network/NetworkMessages.h:38` | Rule 13 | The template argument defines a two-byte wire field.
- F7 | `Engine/Source/Network/NetworkMessages.h:40` | Rule 13 | This serializer overload writes a signed four-byte wire field.
- F8 | `Engine/Source/Network/NetworkMessages.h:42` | Rule 13 | The template argument defines a signed four-byte wire field.
- F9 | `Engine/Source/Network/NetworkMessages.h:44` | Rule 13 | This serializer overload writes an unsigned four-byte wire field.
- F10 | `Engine/Source/Network/NetworkMessages.h:46` | Rule 13 | The template argument defines an unsigned four-byte wire field.
- F11 | `Engine/Source/Network/NetworkMessages.h:52` | Rule 13 | This serializer overload preserves the unsigned eight-byte wire representation.
- F12 | `Engine/Source/Network/NetworkMessages.h:54` | Rule 13 | The template argument defines the unsigned eight-byte wire representation.
- F13 | `Engine/Source/Network/NetworkMessages.h:82` | Rule 13 | This writes the one-byte string terminator required by the wire format.
- F14 | `Engine/Source/Network/NetworkMessages.h:94` | Rule 13 | This parameter represents the serialized one-byte acceptance field.
- F15 | `Engine/Source/Network/NetworkMessages.h:94` | Rule 13 | This parameter is written as a one-byte connection-response tail field.
- F16 | `Engine/Source/Network/NetworkMessages.h:118` | Rule 13 | The element type represents individual bytes of serialized network input.
- F17 | `Engine/Source/Network/NetworkMessages.h:131` | Rule 13 | This reader overload consumes a one-byte wire field.
- F18 | `Engine/Source/Network/NetworkMessages.h:135` | Rule 13 | This reader overload consumes a two-byte wire field.
- F19 | `Engine/Source/Network/NetworkMessages.h:139` | Rule 13 | This reader overload consumes a signed four-byte wire field.
- F20 | `Engine/Source/Network/NetworkMessages.h:143` | Rule 13 | This reader overload consumes an unsigned four-byte wire field.
- F21 | `Engine/Source/Network/NetworkMessages.h:151` | Rule 13 | This reader overload preserves the unsigned eight-byte wire representation.
- F22 | `Engine/Source/Network/NetworkMessages.h:227` | Rule 13 | This parameter represents the serialized one-byte acceptance field.
- F23 | `Engine/Source/Network/NetworkMessages.h:227` | Rule 13 | This parameter is read as a one-byte connection-response tail field.
- F24 | `Engine/Source/Network/NetworkMessages.h:276` | Rule 13 | The element type represents individual bytes of serialized network input.
- F25 | `Engine/Source/Network/NetworkMessages.h:294` | Rule 13 | This visitor overload skips exactly one byte of the wire layout.
- F26 | `Engine/Source/Network/NetworkMessages.h:298` | Rule 13 | This visitor overload skips exactly two bytes of the wire layout.
- F27 | `Engine/Source/Network/NetworkMessages.h:302` | Rule 13 | This visitor overload skips exactly four bytes of the wire layout.
- F28 | `Engine/Source/Network/NetworkMessages.h:306` | Rule 13 | This visitor overload skips exactly four bytes of the wire layout.
- F29 | `Engine/Source/Network/NetworkMessages.h:310` | Rule 13 | This visitor overload matches an unsigned eight-byte wire field.
- F30 | `Engine/Source/Network/NetworkMessages.h:344` | Rule 13 | The first parameter mirrors the serialized one-byte acceptance field in the visitor interface.
- F31 | `Engine/Source/Network/NetworkMessages.h:344` | Rule 13 | The second parameter mirrors the serialized one-byte debug-input field in the visitor interface.
- F32 | `Engine/Source/Network/NetworkMessages.h:398` | Rule 13 | The element type represents individual bytes supplied to network deserialization.
- F33 | `Engine/Source/Network/NetworkMessages.h:412` | Rule 13 | `AckStreamEntry::Visit` serializes this as a one-byte slot index.
- F34 | `Engine/Source/Network/NetworkMessages.h:413` | Rule 13 | `AckStreamEntry::Visit` serializes this as a two-byte epoch.
- F35 | `Engine/Source/Network/NetworkMessages.h:435` | Rule 13 | `ClientAckStreamMessage::Visit` serializes this as a one-byte count.
- F36 | `Engine/Source/Network/NetworkMessages.h:509` | Rule 13 | `ClientHelloMessage::Visit` serializes this as a four-byte protocol version.
- F37 | `Engine/Source/Network/NetworkMessages.h:534` | Rule 13 | `ServerConnectionResponseMessage::Visit` serializes this as a one-byte generation.
- F38 | `Engine/Source/Network/NetworkMessages.h:535` | Rule 13 | `ServerConnectionResponseMessage::Visit` serializes this as a one-byte acceptance field.
- F39 | `Engine/Source/Network/NetworkMessages.h:538` | Rule 13 | `ConnectionResponseTail` serializes this as a one-byte field in the accepted response.
- F40 | `Engine/Source/Network/NetworkMessages.h:558` | Rule 13 | `ClientSubscribeMessage::Visit` serializes this as a one-byte generation.
- F41 | `Engine/Source/Network/NetworkMessages.h:575` | Rule 13 | `ClientUnsubscribeMessage::Visit` serializes this as a one-byte slot index.
- F42 | `Engine/Source/Network/NetworkMessages.h:576` | Rule 13 | `ClientUnsubscribeMessage::Visit` serializes this as a two-byte epoch.
- F43 | `Engine/Source/Network/NetworkMessages.h:604` | Rule 13 | `ServerSubscribeAcceptMessage::Visit` serializes this as a one-byte generation.
- F44 | `Engine/Source/Network/NetworkMessages.h:605` | Rule 13 | `ServerSubscribeAcceptMessage::Visit` serializes this as a one-byte slot index.
- F45 | `Engine/Source/Network/NetworkMessages.h:606` | Rule 13 | `ServerSubscribeAcceptMessage::Visit` serializes this as a two-byte epoch.
- F46 | `Engine/Source/Network/NetworkMessages.h:625` | Rule 13 | `ServerUnsubscribeAckMessage::Visit` serializes this as a one-byte slot index.
- F47 | `Engine/Source/Network/NetworkMessages.h:640` | Rule 13 | `ServerLoadNotificationMessage::Visit` serializes this as a one-byte generation.
- F48 | `Engine/Source/Network/NetworkMessages.h:673` | Rule 13 | `ServerCoordFullStateMessage::Visit` serializes this as a one-byte generation.
- F49 | `Engine/Source/Network/NetworkMessages.h:674` | Rule 13 | `ServerCoordFullStateMessage::Visit` serializes this as a one-byte slot index.
- F50 | `Engine/Source/Network/NetworkMessages.h:675` | Rule 13 | `ServerCoordFullStateMessage::Visit` serializes this as a two-byte epoch.
- F51 | `Engine/Source/Network/NetworkMessages.h:678` | Rule 13 | `ServerCoordFullStateMessage::Visit` serializes this as a signed four-byte size.
- F52 | `Engine/Source/Network/NetworkMessages.h:702` | Rule 13 | `ServerCoordStaticDataMessage::Visit` serializes this as a one-byte generation.
- F53 | `Engine/Source/Network/NetworkMessages.h:703` | Rule 13 | `ServerCoordStaticDataMessage::Visit` serializes this as a one-byte slot index.
- F54 | `Engine/Source/Network/NetworkMessages.h:704` | Rule 13 | `ServerCoordStaticDataMessage::Visit` serializes this as a two-byte epoch.
- F55 | `Engine/Source/Network/NetworkMessages.h:726` | Rule 13 | `CoordUpdateFields::VisitFields` serializes this as a one-byte generation.
- F56 | `Engine/Source/Network/NetworkMessages.h:727` | Rule 13 | `CoordUpdateFields::VisitFields` serializes this as a one-byte slot index.
- F57 | `Engine/Source/Network/NetworkMessages.h:728` | Rule 13 | `CoordUpdateFields::VisitFields` serializes this as a two-byte epoch.
- F58 | `Engine/Source/Network/NetworkMessages.h:784` | Rule 13 | `ServerDebugFrameMessage::Visit` serializes this as a signed four-byte size.
- F59 | `Engine/Source/Network/NetworkMessages.h:799` | Rule 13 | The element type represents individual wire bytes parsed by the coordinate-update tick readers.

#### Engine/Source/Network/NetworkProtocol (out-of-bound)
- F4 | `Engine/Source/Network/NetworkProtocol.h:88` | Rule 13 | Leave unchanged; this GUID member crosses serialized and deterministic-state layout boundaries, is copied as bytes, and has size/alignment/offset assertions at lines 98–101. An int64_t conversion and rename require a separate layout change.
- F5 | `Engine/Source/Network/NetworkProtocol.h:89` | Rule 13 | Leave unchanged; this GUID member crosses serialized and deterministic-state layout boundaries, is copied as bytes, and has size/alignment/offset assertions at lines 98–101. An int64_t conversion and rename require a separate layout change.
- F8 | `Engine/Source/Network/NetworkProtocol.h:194` | Rule 13 | Leave unchanged; `ServerReceive.cpp:424` increments this member with uint16_t wrap, while wire messages retain uint16_t epochs and acceptance checks compare them against this member. Widening changes rollover behavior and what stale-epoch checks accept.

#### Engine/Source/Network/NetworkSerialization (out-of-bound)
- F1 | `Engine/Source/Network/NetworkSerialization.cpp:245` | Rule 13 | Serialized network byte-span; widening changes wire-buffer layout.
- F2 | `Engine/Source/Network/NetworkSerialization.cpp:261` | Rule 13 | Reads the one-byte network group type; excluded by the Plan's network-serialization rule.
- F3 | `Engine/Source/Network/NetworkSerialization.cpp:262` | Rule 13 | Reads the two-byte network group count; excluded by the Plan's network-serialization rule.
- F4 | `Engine/Source/Network/NetworkSerialization.cpp:327` | Rule 13 | Compressed network byte-span; widening changes wire-buffer layout.
- F5 | `Engine/Source/Network/NetworkSerialization.cpp:349` | Rule 13 | Value's bytes are copied into the four-byte wire prefix.
- F7 | `Engine/Source/Network/NetworkSerialization.cpp:364` | Rule 13 | Compressed network byte-span; widening changes wire-buffer layout.
- F8 | `Engine/Source/Network/NetworkSerialization.cpp:368` | Rule 13 | Four-byte wire prefix is read directly into this value's storage.
- F10 | `Engine/Source/Network/NetworkSerialization.cpp:378` | Rule 13 | Template argument describes decompressed serialized bytes; widening changes wire-buffer layout.
- F11 | `Engine/Source/Network/NetworkSerialization.h:40` | Rule 13 | Serialized network byte-span declaration corresponding to cpp line 245.
- F12 | `Engine/Source/Network/NetworkSerialization.h:44` | Rule 13 | Compressed network byte-span declaration corresponding to cpp line 327.
- F13 | `Engine/Source/Network/NetworkSerialization.h:47` | Rule 13 | Compressed network byte-span declaration corresponding to cpp line 364.

#### Engine/Source/Network/NetworkSessionContract (out-of-bound)
- F1 | `Engine/Source/Network/NetworkSessionContract.h:18` | Rule 13 | Leave unchanged: its elements are compressed network payload bytes, passed through `Projects/BrokenEngineSandbox/Source/Network/NetworkSessionContract.h:32` and buffered by `Engine/Source/Network/Server/Server.cpp:382-385`; changing the element width crosses the excluded wire-layout boundary.
- F2 | `Engine/Source/Network/NetworkSessionContract.h:19` | Rule 13 | Leave unchanged: its elements are received compressed network payload bytes, supplied by `Engine/Source/Network/Client/Client.cpp:542` through `Projects/BrokenEngineSandbox/Source/Network/NetworkSessionContract.h:36`; changing the element width crosses the excluded wire-layout boundary.

#### Engine/Source/Network/NetworkSimulation (out-of-bound)
- F1 | `Engine/Source/Network/NetworkSimulation.h:77` | Rule 13 | `std::vector<uint8_t> data;` holds serialized network payload bytes copied at line 143 and consumed by `Client::Receive` and `Server::Receive`; layout-boundary element types are excluded.

#### Engine/Source/Network/Server/Server (out-of-bound)
- F2 | `Engine/Source/Network/Server/Server.cpp:159` | Rule 13 | Leave unchanged; the element type views ENet wire bytes, and widening it changes packet layout and byte indexing.
- F3 | `Engine/Source/Network/Server/Server.cpp:164` | Rule 13 | Leave unchanged; the element type views ENet wire bytes, and widening it changes packet layout and byte indexing.
- F4 | `Engine/Source/Network/Server/Server.cpp:211` | Rule 13 | Leave this parameter and its declaration at Server.h:204 unchanged; it carries serialized network bytes into packet decoding.
- F5 | `Engine/Source/Network/Server/Server.cpp:335` | Rule 13 | Leave unchanged; this constructs the serialized game payload byte buffer consumed by byte-oriented packet readers.
- F7 | `Engine/Source/Network/Server/Server.cpp:382` | Rule 13 | Leave unchanged; the span exposes the compression destination as serialized payload bytes.
- F15 | `Engine/Source/Network/Server/Server.h:33` | Rule 13 | Leave unchanged; the elements store serialized game packet bytes consumed through `const uint8_t*` cursors in Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp.
- F16 | `Engine/Source/Network/Server/Server.h:78` | Rule 13 | Leave unchanged; rule edge case: preincrement at Server.cpp:289,618 wraps a `uint16_t` counter from 65535 to 0, whereas an `int64_t` counter becomes 65536, changing the per-type admission comparison. The FIX bound forbids changing what a trust-boundary check accepts; widening requires a separate decision about this behavior.
- F18 | `Engine/Source/Network/Server/Server.h:122` | Rule 13 | Leave unchanged; the elements are compressed wire payload bytes passed to NetworkMessages writers.
- F19 | `Engine/Source/Network/Server/Server.h:222` | Rule 13 | Leave the ClientAcknowledgementStream parameter unchanged; its elements carry serialized ACK packet bytes.
- F20 | `Engine/Source/Network/Server/Server.h:223` | Rule 13 | Leave the ClientDesynchronizationReport parameter unchanged; its elements carry serialized desynchronization-report bytes.
- F21 | `Engine/Source/Network/Server/Server.h:224` | Rule 13 | Leave the ClientDebugFrameRequest parameter unchanged; its elements carry serialized debug-frame-request bytes.
- F22 | `Engine/Source/Network/Server/Server.h:225` | Rule 13 | Leave the ClientHello parameter unchanged; its elements carry serialized handshake bytes.
- F23 | `Engine/Source/Network/Server/Server.h:227` | Rule 13 | Leave the ClientSubscribe parameter unchanged; its elements carry serialized subscription-request bytes.
- F24 | `Engine/Source/Network/Server/Server.h:228` | Rule 13 | Leave the ClientUnsubscribe parameter unchanged; its elements carry serialized unsubscription-request bytes.
- F25 | `Engine/Source/Network/Server/Server.h:229` | Rule 13 | Leave the ClientResynchronizationRequest parameter unchanged; its elements carry serialized resynchronization-request bytes.
- F28 | `Engine/Source/Network/Server/Server.h:257` | Rule 13 | Leave unchanged; LZ4 writes raw compressed bytes into this buffer through a `char*`, and its data is transmitted as serialized network payload.

#### Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenBase (out-of-bound)
- F5 | `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenBase.h:54` | Rule 13 | Leave unchanged: this is explicit padding in the verbatim-persisted `TweakSectionState`, whose size is asserted at h:59. Widening it would change serialized layout and bytes.

#### Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenLighting (out-of-bound)
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenLighting.cpp:215` | Rule 13 | Reverted to `int iOffset`. rule edge case: `RenderLightingSection`'s `std::snprintf` offset accumulator, widened to `int64_t` and passed as `sizeof(pcBuffer) - static_cast<size_t>(iOffset)` at :219 and :221, makes Client Release code analysis report C6386 (2049 bytes into the 2,048-byte buffer) at :221, which fails the build as an error. Runtime behavior does not change, because `CurveData::kiMaximumControlPoints` (16) keeps the formatted text under the buffer size.

#### Engine/Source/Ui/GameSettings (out-of-bound)
- F1 | `Engine/Source/Ui/GameSettings.cpp:16` | Rule 13 | `iLanguage` is serialized with asserted offset and size; conversion to `int64_t` would change serialized bytes.
- F2 | `Engine/Source/Ui/GameSettings.cpp:20` | Rule 13 | `uiOpaqueUi` is serialized with asserted offset and size; conversion to `int64_t` would change serialized bytes.
- F3 | `Engine/Source/Ui/GameSettings.cpp:22` | Rule 13 | `uiPad` is serialized padding with asserted offset and size; conversion to `int64_t` would change serialized bytes.

#### Engine/Source/Ui/GraphicsSettings (out-of-bound)
- F1 | `Engine/Source/Ui/GraphicsSettings.cpp:29` | Rule 13 | Serialized padding in the asserted 44-byte GraphicsSettings layout.
- F2 | `Engine/Source/Ui/GraphicsSettings.cpp:40` | Rule 13 | Serialized member at asserted offset 36; widening changes saved bytes.
- F3 | `Engine/Source/Ui/GraphicsSettings.cpp:41` | Rule 13 | Serialized member at asserted offset 37; widening changes saved bytes.
- F4 | `Engine/Source/Ui/GraphicsSettings.cpp:42` | Rule 13 | Serialized member at asserted offset 38; widening changes saved bytes.
- F5 | `Engine/Source/Ui/GraphicsSettings.cpp:43` | Rule 13 | Serialized member at asserted offset 39; widening changes saved bytes.
- F6 | `Engine/Source/Ui/GraphicsSettings.cpp:44` | Rule 13 | Serialized member at asserted offset 40; widening changes saved bytes.
- F7 | `Engine/Source/Ui/GraphicsSettings.cpp:45` | Rule 13 | Serialized padding at asserted offset 41; widening changes the asserted 44-byte saved layout.

#### Engine/Source/Ui/SoundSettings (out-of-bound)
- F1 | `Engine/Source/Ui/SoundSettings.cpp:17` | Rule 13 | `uint8_t uiMuteInBackground = 0;` is serialized by WriteVersionedFile and ReadVersionedFile; widening changes asserted layout and persisted bytes.
- F2 | `Engine/Source/Ui/SoundSettings.cpp:18` | Rule 13 | `uint8_t uiPadding[3] {};` is serialized struct padding with asserted offset 13 and total size 16; widening changes persisted bytes.

#### Engine/Source/Agent/AgentCommandsClientGeneric (out-of-bound)
- F1 | `Engine/Source/Agent/AgentCommandsClientGeneric.cpp:40` | Rule 13 | rule edge case: converting unsigned JSON to int64_t before the comparison can turn values above INT64_MAX into negative values and accept them. Preserve the unsigned range check and conversion after validation.
- F2 | `Engine/Source/Agent/AgentCommandsClientGeneric.cpp:106` | Rule 13 | rule edge case: baseline-plus-frame-count expressions currently wrap modulo 2^32; changing the baseline to int64_t removes that wrap and changes capture completion and path enumeration when the sum exceeds UINT32_MAX.
- F13 | `Engine/Source/Agent/AgentCommandsClientGeneric.cpp:1022` | Rule 13 | rule edge case: unsigned extraction checks whether JSON exceeds INT64_MAX before signed extraction. Changing the template argument to int64_t can wrap an excessive unsigned value and accept a different wheel command.

#### Engine/Source/Agent/AgentCommandServer (out-of-bound)
- F3 | `Engine/Source/Agent/AgentCommandServer.cpp:212` | Rule 13 | Its bytes are read directly as the four-byte wire length prefix at line 213; changing its width changes framing.
- F4 | `Engine/Source/Agent/AgentCommandServer.cpp:213` | Rule 13 | The span exposes the wire length prefix as individual bytes; changing its element type changes the transport layout.
- F5 | `Engine/Source/Agent/AgentCommandServer.cpp:226` | Rule 13 | The span receives UTF-8 wire payload bytes into string storage; changing its element type changes byte addressing and transfer lengths.
- F7 | `Engine/Source/Agent/AgentCommandServer.cpp:458` | Rule 13 | The element type defines byte addressing and socket receive lengths; matching declaration at header line 68 remains unchanged.
- F10 | `Engine/Source/Agent/AgentCommandServer.cpp:505` | Rule 13 | The element type defines byte addressing and socket send lengths; matching declaration at header line 69 remains unchanged.
- F13 | `Engine/Source/Agent/AgentCommandServer.cpp:557` | Rule 13 | Its bytes are sent directly as the four-byte wire length prefix at line 559; changing its width changes framing.
- F14 | `Engine/Source/Agent/AgentCommandServer.cpp:559` | Rule 13 | The span exposes the wire length prefix as individual bytes; changing its element type changes the transport layout.
- F15 | `Engine/Source/Agent/AgentCommandServer.cpp:563` | Rule 13 | The span sends UTF-8 wire payload bytes from string storage; changing its element type changes byte addressing and transfer lengths.
- F17 | `Engine/Source/Agent/AgentCommandServer.h:68` | Rule 13 | The element type defines byte addressing and socket receive lengths; matching definition at source line 458 remains unchanged.
- F18 | `Engine/Source/Agent/AgentCommandServer.h:69` | Rule 13 | The element type defines byte addressing and socket send lengths; matching definition at source line 505 remains unchanged.

#### Engine/Source/Agent/Commands/AudioStreamingFixture (out-of-bound)
- F17 | `Engine/Source/Agent/Commands/AudioStreamingFixture.cpp:485` | Rule 13 | rule edge case: `muiNextSequence.fetch_add(...) + 1` uses full-range unsigned sequence arithmetic. Signed arithmetic can overflow, and converting only the result changes reported sequence values above `INT64_MAX`.
- F31 | `Engine/Source/Agent/Commands/AudioStreamingFixture.h:88` | Rule 13 | rule edge case: full-range unsigned sequences are sorted at .cpp:644, checked with unsigned reservation arithmetic at .cpp:649, and reported by `AgentCommandsAudioStreaming.cpp:287`. Signed conversion changes ordering and reported values above `INT64_MAX`.
- F45 | `Engine/Source/Agent/Commands/AudioStreamingFixture.h:118` | Rule 13 | rule edge case: this stores a full-range unsigned sequence boundary and participates in unsigned subtraction/addition at .cpp:646,649. Signed conversion changes wrap semantics and externally reported values.
- F46 | `Engine/Source/Agent/Commands/AudioStreamingFixture.h:119` | Rule 13 | rule edge case: this stores a full-range unsigned sequence boundary and participates in unsigned subtraction at .cpp:646. Signed conversion changes wrap semantics and externally reported values.
- F47 | `Engine/Source/Agent/Commands/AudioStreamingFixture.h:120` | Rule 13 | rule edge case: `muiRetryCount` is an unbounded unsigned atomic counter, and this snapshot reports its full unsigned value at `AgentCommandsAudioStreaming.cpp:233`. Values above `INT64_MAX` cannot retain their reported value in `int64_t`.
- F65 | `Engine/Source/Agent/Commands/AudioStreamingFixture.h:258` | Rule 13 | rule edge case: .cpp:479 increments this counter without saturation. Widening changes wrap from `2^32` to `2^64`, changing when the index returns to zero and history entries are written again.
- F66 | `Engine/Source/Agent/Commands/AudioStreamingFixture.h:259` | Rule 13 | rule edge case: .cpp:482 increments without saturation. Widening removes the existing `2^32` wrap to zero, changing the reported dropped count and coherence/evidence tests.
- F68 | `Engine/Source/Agent/Commands/AudioStreamingFixture.h:268` | Rule 13 | rule edge case: .cpp:485 adds one outside `fetch_add`, relying on unsigned arithmetic. Signed conversion introduces overflow at `INT64_MAX` and changes full-range sequence reporting.
- F69 | `Engine/Source/Agent/Commands/AudioStreamingFixture.h:269` | Rule 13 | rule edge case: .cpp:816 increments this unbounded counter, whose full unsigned value is exposed through `snapshot.uiRetryCount`. Converting the counter and snapshot changes reported values above `INT64_MAX`.
- `Engine/Source/Agent/Commands/AudioStreamingFixture.cpp:52` | Rule 13 | Reverted to `uint64_t`. rule edge case: `NextHoldGeneration`, `ArmHold`, and `ReleaseHold` carry the hold generation as the bit field `uiGeneration << kuiHoldGenerationShift` packed by `PackHoldToken` into the `uint64_t` `muiHoldToken`, and `NextHoldGeneration` asserts against `UINT64_MAX >> kuiHoldGenerationShift`. As a signed `int64_t`, the shift overflows into the sign bit once the generation passes 2^59, and the exhaustion limit changes.
- F70 | `Engine/Source/Agent/Commands/AudioStreamingFixture.h:270` | Rule 13 | rule edge case: .cpp:289 copies the full unsigned sequence counter, then .cpp:530 exposes it to unsigned reservation arithmetic and external reporting. Signed conversion cannot preserve the entire value range.

#### Engine/Source/Agent/Commands/ClientNetworkFixtures (out-of-bound)
- F2 | `Engine/Source/Agent/Commands/ClientNetworkFixtures.h:34` | Rule 13 | `std::vector<uint8_t> packet;` stores exact serialized network packet bytes, assigned at `.cpp:80` and delivered through `.cpp:151–154`; changing the element type changes packet storage and delivery.
- F7 | `Engine/Source/Agent/Commands/ClientNetworkFixtures.cpp:70` | Rule 13 | `std::span<const uint8_t> packetData` and its declaration at `.h:79` describe serialized network bytes copied verbatim at `.cpp:80`; widening the element type would change the packet layout.
- F10 | `Engine/Source/Agent/Commands/ClientNetworkFixtures.cpp:151` | Rule 13 | `std::vector<uint8_t> packet` takes ownership of captured serialized bytes and passes them directly to `rClient.Receive(packet)` at `.cpp:154`; widening the element type changes packet storage and delivery.

#### Engine/Source/Audio/StreamingVoice (out-of-bound)
- F2 | `Engine/Source/Audio/StreamingVoice.h:118` | Rule 13 | `uint8_t mBuffers[kiBufferCount][kiBufferSize] {};` carries packed audio bytes through reinterpret casts and XAudio2 `.pAudioData`; changing element width changes the audio byte layout.

#### Engine/Source/File/PackChunkLoader (out-of-bound)
- F7 | `Engine/Source/File/PackChunkLoader.h:66` | Rule 13 | Leave unchanged. rule edge case: `.cpp:338` allows sequence values through `UINT64_MAX - 1`, and `.cpp:339` increments the counter. Converting the atomic and its increment to signed arithmetic overflows at `INT64_MAX`; changing the exhaustion assertion to `INT64_MAX` changes threading behavior. In-unit references: `.h:59,66`; `.cpp:232,292,335,339,344`.

#### Engine/Source/File/PackChunks (out-of-bound)
- `Engine/Source/File/PackChunks.cpp:984` | Rule 13 | Reverted to `uint64_t`. rule edge case: `LoadAudioRead`'s range guards (`uiDataFileOffset < location.uiOffset`, `uiOffset > UINT64_MAX - uiDataFileOffset`, `uiLength > UINT64_MAX - uiPrefix`, `uiAlignedOffset > UINT64_MAX - uiPhysicalSize`, `uiLength > UINT64_MAX - uiLogicalFileOffset`) are built on the unsigned on-disk `ChunkLocation::uiOffset`. Rewriting them in `int64_t` makes the guarded additions able to overflow as signed values and mixes signed with unsigned in the comparisons against `ChunkLocation::uiOffset`. The `int64_t` entry offset and length now convert to `uint64_t` once, before the guards.

#### Engine/Source/File/Replay (out-of-bound)
- F2 | `Engine/Source/File/Replay.cpp:77` | Rule 13 | Leave unchanged: serialized at line 152 and deserialized at line 505; signed conversion also changes coordinate-key ordering for keys above `INT64_MAX`.
- F3 | `Engine/Source/File/Replay.cpp:93` | Rule 13 | Leave unchanged: rule edge case: `GridCoord::ToKey()` packs negative X coordinates into keys above `INT64_MAX`; conversion to `int64_t` changes `std::to_string(uiCoordinateKey)` at lines 99–102 and therefore replay artifact filenames.
- `Engine/Source/File/Replay.cpp:120` | Rule 13 | Reverted to `.size()`. rule edge case: `AppendReplayManifestPayload` (:120–121), `ComputeReplayGenerationDigest` (:167), and `BuildExpectedReplayInventory` (:185) guard `size_t` byte totals with `x.size() > (std::numeric_limits<size_t>::max() - k) / n`. Rewriting the left side as `std::ssize(x)` compares a signed count against a `size_t` limit, which mixes signed and unsigned operands. The :167 limit, `SIZE_MAX - 4 - domain size`, is above `INT64_MAX`, so the rewritten guard is always false and cannot be expressed in `int64_t`.
- F7 | `Engine/Source/File/Replay.cpp:121` | Rule 17 | Reclassified from local: rule edge case: `std::ssize(rManifest.records) * 16` can overflow `int64_t` above `INT64_MAX / 16`; the preceding unsigned allocation bound permits larger counts. Retained both `.size()` expressions; only F5's constant rename applies here.
- F16 | `Engine/Source/File/Replay.cpp:471` | Rule 13 | Leave unchanged: `ReadReplayManifestValue` at line 472 consumes one serialized byte based on this type; widening changes replay parsing.
- F17 | `Engine/Source/File/Replay.cpp:495` | Rule 13 | Leave unchanged: `ReadReplayManifestValue` at line 497 consumes one serialized byte based on this type; widening changes replay parsing.

#### Engine/Source/GameBase (out-of-bound)
- F1 | `Engine/Source/GameBase.h:301` | Rule 13 | Leave unchanged: `Projects/BrokenEngineSandbox/Source/Game.cpp:691` increments this counter when initializing deterministic frame state (`rFrame.postRender.uiFrameIdentifier = muiNextFrameId++;`). Widening changes its unsigned wrap at 65535, an excluded deterministic arithmetic change. The destination participates in CRC and serialization (`Engine/Source/Frame/FrameBase.cpp:81,120,139,155`). Only in-unit reference: declaration at `Engine/Source/GameBase.h:301`.

#### Engine/Source/Server/ServerDisplay (out-of-bound)
- F7 | `Engine/Source/Server/ServerDisplay.cpp:208` | Rule 13 | rule edge case: widening `iMinimumX` permits padding below `INT32_MIN`; narrowing the loop coordinate into `GridCoord.iX` then wraps to another cell. Reclassified from local; unchanged.
- F8 | `Engine/Source/Server/ServerDisplay.cpp:209` | Rule 13 | rule edge case: widening `iMaximumX` permits padding above `INT32_MAX`; narrowing the loop coordinate into `GridCoord.iX` then wraps to another cell. Reclassified from local; unchanged.
- F9 | `Engine/Source/Server/ServerDisplay.cpp:210` | Rule 13 | rule edge case: widening `iMinimumY` permits padding below `INT32_MIN`; narrowing the loop coordinate into `GridCoord.iY` then wraps to another cell. Reclassified from local; unchanged.
- F10 | `Engine/Source/Server/ServerDisplay.cpp:211` | Rule 13 | rule edge case: widening `iMaximumY` permits padding above `INT32_MAX`; narrowing the loop coordinate into `GridCoord.iY` then wraps to another cell. Reclassified from local; unchanged.

#### DataPacker/Source/DiagnosticReporter (out-of-bound)
- F9 | `DataPacker/Source/DiagnosticReporter.cpp:130` | Rule 13 | Reclassified local -> out-of-bound: rule edge case: `uiAllocation + iReserve` can exceed `INT64_MAX`; storing it in `int64_t` changes the formatted required-byte value.
- F10 | `DataPacker/Source/DiagnosticReporter.cpp:145` | Rule 13 | Reclassified local -> out-of-bound: rule edge case: unchecked unsigned `uiAvailable - uiAllocation` can exceed `INT64_MAX`; conversion can produce a negative value and change the low-space comparison and formatted output.
- `DataPacker/Source/DiagnosticReporter.cpp:129` | Rule 13 | Reverted to `uint64_t`. rule edge case: `ReportMaterializationDiskSpace`'s reserve `(uiAllocation * 5 + 99) / 100`, required total `uiAllocation + uiReserve`, and projected `uiAvailable - uiAllocation` are computed from the `uint64_t` byte counts it receives. In `int64_t`, allocations above `INT64_MAX / 5` overflow the reserve product, and the sum and difference can leave the signed range. The insufficient-space and low-space decisions and the formatted byte counts then change.

#### DataPacker/Source/FileManager (out-of-bound)
- F1 | `DataPacker/Source/FileManager.cpp:10` | Rule 13 | Leave unchanged: Windows reparse-buffer layout, read through the `reinterpret_cast` at line 156.
- F2 | `DataPacker/Source/FileManager.cpp:11` | Rule 13 | Leave unchanged: Windows reparse-buffer layout, read through the `reinterpret_cast` at line 156.
- F3 | `DataPacker/Source/FileManager.cpp:12` | Rule 13 | Leave unchanged: Windows reparse-buffer layout, read through the `reinterpret_cast` at line 156.
- F4 | `DataPacker/Source/FileManager.cpp:13` | Rule 13 | Leave unchanged: Windows reparse-buffer layout; widening changes the offset field and subsequent fields consumed at line 161.
- F5 | `DataPacker/Source/FileManager.cpp:14` | Rule 13 | Leave unchanged: Windows reparse-buffer layout; widening changes the length field and subsequent fields consumed at line 161.
- F6 | `DataPacker/Source/FileManager.cpp:15` | Rule 13 | Leave unchanged: Windows reparse-buffer layout, read through the `reinterpret_cast` at line 156.
- F7 | `DataPacker/Source/FileManager.cpp:16` | Rule 13 | Leave unchanged: Windows reparse-buffer layout, read through the `reinterpret_cast` at line 156.
- F12 | `DataPacker/Source/FileManager.cpp:150` | Rule 13 | Leave unchanged: its elements are the raw Windows reparse buffer supplied to `DeviceIoControl` at line 152 and interpreted as `SymbolicLinkReparseDataBuffer` at line 156; changing element width changes the buffer layout.
- F14 | `DataPacker/Source/FileManager.cpp:176` | Rule 13 | Leave unchanged; rule edge case: conversion of this checked arithmetic chain to `int64_t` changes the supported range. `AddChecked(INT64_MAX, 1)` currently succeeds; an `int64_t` result cannot represent it. Keep the `numeric_limits<uint64_t>` check at line 178.
- F15 | `DataPacker/Source/FileManager.cpp:176` | Rule 13 | Leave unchanged; rule edge case: this parameter supports the full unsigned allocation range used by the check at line 178 and addition at line 182; conversion to `int64_t` cannot preserve values above `INT64_MAX`.
- F16 | `DataPacker/Source/FileManager.cpp:176` | Rule 13 | Leave unchanged; rule edge case: this parameter supports the full unsigned allocation range used by the check at line 178 and addition at line 182; conversion to `int64_t` cannot preserve values above `INT64_MAX`.
- F18 | `DataPacker/Source/FileManager.cpp:246` | Rule 13 | Leave unchanged; rule edge case: the accumulated allocation at line 272 uses `AddChecked`'s full unsigned range. An `int64_t` member cannot preserve totals above `INT64_MAX`, including the value passed to the diagnostic at line 567 and logged at line 579.
- F19 | `DataPacker/Source/FileManager.cpp:260` | Rule 13 | Leave unchanged; rule edge case: the multiplication explicitly supports the product of two unsigned 32-bit values; the declared operand ranges permit products above `INT64_MAX`. Replacing the widening cast and result with `int64_t` introduces signed overflow for those products and changes the rounding calculation at line 272.
- F20 | `DataPacker/Source/FileManager.cpp:271` | Rule 13 | Leave unchanged; rule edge case: the unsigned file size feeds checked addition and unsigned cluster rounding at line 272. Converting a size above `INT64_MAX` to a negative signed value changes that calculation; converting the whole chain also lowers its checked range.

#### DataPacker/Source/InputFingerprint (out-of-bound)
- F6 | `DataPacker/Source/InputFingerprint.cpp:179` | Rule 13 | Reclassified from local: rule edge case: converting a successful `uintmax_t` file size above `INT64_MAX` to `int64_t` makes it negative and bypasses the maximum-size check; the unit establishes no signed-range bound.
- F7 | `DataPacker/Source/InputFingerprint.cpp:208` | Rule 13 | This template argument defines the persisted snapshot size's unsigned read type.
- F8 | `DataPacker/Source/InputFingerprint.cpp:212` | Rule 13 | This template argument defines the persisted file identity's unsigned read type; signed conversion can change identities above `INT64_MAX`.
- F9 | `DataPacker/Source/InputFingerprint.cpp:213` | Rule 13 | This template argument defines the persisted volume identity's read width and conversion behavior.
- F12 | `DataPacker/Source/InputFingerprint.cpp:335` | Rule 13 | Reclassified from local: rule edge case: converting a successful `uintmax_t` file size above `INT64_MAX` to `int64_t` makes it negative and bypasses the maximum-size check; the unit establishes no signed-range bound.
- F13 | `DataPacker/Source/InputFingerprint.cpp:349` | Rule 13 | This template argument defines the persisted snapshot size's unsigned read type.
- F14 | `DataPacker/Source/InputFingerprint.cpp:353` | Rule 13 | This template argument defines the persisted file identity's unsigned read type; signed conversion can change identities above `INT64_MAX`.
- F15 | `DataPacker/Source/InputFingerprint.cpp:354` | Rule 13 | This template argument defines the persisted volume identity's read width and conversion behavior.
- F17 | `DataPacker/Source/InputFingerprint.h:23` | Rule 13 | Persisted snapshot member, read at `.cpp:208,349`, written at `.cpp:249,375`, and populated at `.cpp:448`; changing its signedness changes the persisted value domain.
- F18 | `DataPacker/Source/InputFingerprint.h:27` | Rule 13 | Persisted snapshot member, read at `.cpp:212,353`, written at `.cpp:253,379`, and populated at `.cpp:452`; signed conversion can change identities above `INT64_MAX`.
- F19 | `DataPacker/Source/InputFingerprint.h:28` | Rule 13 | Persisted snapshot member, read at `.cpp:213,354`, written at `.cpp:254,380`, and populated at `.cpp:453`; changing the member and associated JSON read types changes the persisted schema's conversion behavior.

#### DataPacker/Source/Main (out-of-bound)
- F13 | `DataPacker/Source/Main.cpp:304` | Rule 13 | rule edge case: line 334 can produce an aligned offset above INT64_MAX; signed accumulator arithmetic would overflow where unsigned arithmetic reaches the existing file-length rejection.
- F14 | `DataPacker/Source/Main.cpp:328` | Rule 13 | rule edge case: converting uiChunkEnd to int64_t makes line 334's addition overflow at INT64_MAX + 1; preserve its unsigned arithmetic and overflow-limit comparison.
- `DataPacker/Source/Main.cpp:202` | Rule 13 | Reverted to unsigned. rule edge case: `GetReadableFileSize` range-checks the `uintmax_t` from `std::filesystem::file_size` against `UINT64_MAX` and `std::streamoff` max before narrowing. Comparing an `int64_t` copy instead would turn sizes above `INT64_MAX` negative before the check. Only the validated return value is `int64_t`.
- `DataPacker/Source/Main.cpp:329` | Rule 13 | Reverted to unsigned. rule edge case: `ValidatePublishedPackLayout`'s alignment and padding guard (`uiChunkEnd % kuiAlignmentBytes`, `uiChunkEnd > UINT64_MAX - uiPadding`) and its range checks against `ChunkLocation::uiOffset`/`uiSize` work on the unsigned manifest fields. In `int64_t`, `uiOffset + uiSize` overflows before the guard, and the comparisons mix signed and unsigned operands.

#### DataPacker/Source/ExportJobs/ExportCubemapIbl (out-of-bound)
- F4 | `DataPacker/Source/ExportJobs/ExportCubemapIbl.cpp:331` | Rule 13 | Half-float elements cross the raw-byte serialization boundary; changing their type changes intermediate-file layout.
- F8 | `DataPacker/Source/ExportJobs/ExportCubemapIbl.cpp:398` | Rule 13 | Half-float elements cross the raw-byte serialization boundary; changing their type changes intermediate-file layout.
- F13 | `DataPacker/Source/ExportJobs/ExportCubemapIbl.cpp:617` | Rule 13 | rule edge case: `hardware_concurrency()` is not bounded to 255; existing conversion truncates modulo 256. Widening requires retaining early narrowing, while clamping changes threading.

#### DataPacker/Source/ExportJobs/ExportIsland (out-of-bound)
- F1 | `DataPacker/Source/ExportJobs/ExportIsland.cpp:20` | Rule 13 | Its elements are copied directly into the `.pack` payload at line 697; changing their width changes serialized bytes.
- F25 | `DataPacker/Source/ExportJobs/ExportIsland.cpp:265` | Rule 13 | rule edge case: `file_size()` can return a value above `INT64_MAX`; converting before the checks at lines 266, 305 loses that value and changes the validation path and diagnostic. No preceding range check proves representability.
- F26 | `DataPacker/Source/ExportJobs/ExportIsland.cpp:271` | Rule 13 | Its bytes are read directly from `MeshProcessed.bin` at line 273; widening changes the header read layout.
- F27 | `DataPacker/Source/ExportJobs/ExportIsland.cpp:272` | Rule 13 | Its bytes are read directly from `MeshProcessed.bin` at line 275; widening changes the header read layout.
- F36 | `DataPacker/Source/ExportJobs/ExportIsland.cpp:312` | Rule 13 | Its bytes are read directly from `MeshProcessed.bin` at line 317 and later written into the `.pack` payload. Applied only the constructor-size change in F29.
- F39 | `DataPacker/Source/ExportJobs/ExportIsland.cpp:368` | Rule 13 | It bounds accepted source dimensions; replacing the bound with the `int64_t` maximum changes what the trust-boundary check accepts.
- F40 | `DataPacker/Source/ExportJobs/ExportIsland.cpp:369` | Rule 13 | It bounds accepted crop width; replacing the bound with the `int64_t` maximum changes what the trust-boundary check accepts.
- F41 | `DataPacker/Source/ExportJobs/ExportIsland.cpp:370` | Rule 13 | It bounds accepted crop height; replacing the bound with the `int64_t` maximum changes what the trust-boundary check accepts.
- F43 | `DataPacker/Source/ExportJobs/ExportIsland.cpp:396` | Rule 13 | Both bounds constrain elevation width and height to the serialized header's representable range; widening them changes what the trust-boundary check accepts.
- F46 | `DataPacker/Source/ExportJobs/ExportIsland.cpp:412` | Rule 13 | rule edge case: `file_size()` can return a value above `INT64_MAX`; an unguarded conversion loses the actual file size used by the validation and diagnostic at lines 413–415. No preceding range check proves representability.
- F48 | `DataPacker/Source/ExportJobs/ExportIsland.cpp:659` | Rule 13 | The half-float words are written directly into the `.pack` payload at line 695 and supplied to the fixed half-float API at line 660. Applied only F49 to the constructor size.

#### DataPacker/Source/ExportJobs/ExportModel (out-of-bound)
- F1 | `DataPacker/Source/ExportJobs/ExportModel.cpp:17` | Rule 13 | Leave unchanged; elements are read as source bytes through `reinterpret_cast<char*>(materialIndexPositions.data())` at line 47. Changing their type changes the source layout.
- F2 | `DataPacker/Source/ExportJobs/ExportModel.cpp:18` | Rule 13 | Leave unchanged; elements are read as source bytes at line 80 and copied into packed model data at line 127. Changing their type changes source and `.pack` layouts.
- F3 | `DataPacker/Source/ExportJobs/ExportModel.cpp:19` | Rule 13 | Leave unchanged; elements are read as source bytes at line 85 and copied into packed model data at line 131. Changing their type changes source and `.pack` layouts.
- F4 | `DataPacker/Source/ExportJobs/ExportModel.cpp:22` | Rule 13 | Leave unchanged; the variable itself is read as bytes at line 33, and its size determines source offsets.
- F5 | `DataPacker/Source/ExportJobs/ExportModel.cpp:23` | Rule 13 | Leave unchanged; the variable itself is read as bytes at line 53, and its size determines the source count-block layout.
- F6 | `DataPacker/Source/ExportJobs/ExportModel.cpp:24` | Rule 13 | Leave unchanged; the variable itself is read as bytes at line 54, and its size determines the source count-block layout.
- F9 | `DataPacker/Source/ExportJobs/ExportModel.cpp:40` | Rule 13 | Leave the type and name unchanged; rule edge case: the checked unsigned multiplication can return a value above `INT64_MAX` before `RequireSourceExtent` rejects it at line 43. A signed declaration cannot represent that intermediate positive byte count.
- F10 | `DataPacker/Source/ExportJobs/ExportModel.cpp:41` | Rule 13 | Leave the type and name unchanged; rule edge case: the checked unsigned multiplication can return a value above `INT64_MAX` before extent validation at line 45. Preserve the unsigned overflow and extent checks.
- F14 | `DataPacker/Source/ExportJobs/ExportModel.cpp:67` | Rule 13 | Leave the type and name unchanged; rule edge case: an individually signed-representable index count multiplied by two or four can exceed `INT64_MAX` before extent validation at line 68. Preserve that unsigned intermediate and its validation.
- F16 | `DataPacker/Source/ExportJobs/ExportModel.cpp:70` | Rule 13 | Leave unchanged; rule edge case: the checked unsigned multiplication can return a value above `INT64_MAX` before extent validation at line 71. Preserve that unsigned intermediate and its validation.

#### DataPacker/Source/ExportJobs/ExportRaw (out-of-bound)
- F1 | `DataPacker/Source/ExportJobs/ExportRaw.cpp:24` | Rule 13 | rule edge case: converting `file_size()` to `int64_t` before the overflow check can turn sizes above `INT64_MAX` negative, bypassing rejection and passing a negative size to `AllocateHeaderAndData`.

#### DataPacker/Source/ExportJobs/ExportScene (out-of-bound)
- F28 | `DataPacker/Source/ExportJobs/ExportScene.cpp:615` | Rule 13 | The referenced mesh index elements feed meshoptimizer and the raw `.MODEL` index buffer; changing their type changes serialized layout and requires the owning `Material::indexBuffer` outside the unit to change.
- F38 | `DataPacker/Source/ExportJobs/ExportScene.cpp:709` | Rule 13 | Its elements are passed to meshoptimizer and written as raw 32-bit `.MODEL` indices at line 748.
- F39 | `DataPacker/Source/ExportJobs/ExportScene.cpp:710` | Rule 13 | The vector is written as raw material index offsets at line 738.
- F45 | `DataPacker/Source/ExportJobs/ExportScene.cpp:722` | Rule 13 | Its elements are written as raw 16-bit `.MODEL` indices at line 744.
- F49 | `DataPacker/Source/ExportJobs/ExportScene.cpp:734` | Rule 13 | The variable's bytes are written directly at line 737 as a `.MODEL` count field.
- F50 | `DataPacker/Source/ExportJobs/ExportScene.cpp:735` | Rule 13 | The variable's bytes are written directly at line 740 as a `.MODEL` count field.
- F51 | `DataPacker/Source/ExportJobs/ExportScene.cpp:736` | Rule 13 | The variable's bytes are written directly at line 741 as a `.MODEL` count field.
- F53 | `DataPacker/Source/ExportJobs/ExportScene.cpp:762` | Rule 13 | The variable receives raw `.MODEL` field bytes at line 770; its type also determines the field extent.
- F65 | `DataPacker/Source/ExportJobs/ExportScene.h:62` | Rule 13 | The span points into the packed scene's 32-bit index-start array and receives raw `.MODEL` bytes at line 849; header declaration and cpp definition remain unchanged.
- F68 | `DataPacker/Source/ExportJobs/ExportScene.cpp:839` | Rule 13 | Its bytes receive the serialized `.MODEL` count at line 840.

#### DataPacker/Source/ExportJobs/ExportShader (out-of-bound)
- F5 | `DataPacker/Source/ExportJobs/ExportShader.cpp:248` | Rule 13 | Leave unchanged: line 320 copies these elements directly into `.pack` payload bytes. Changing their width changes serialized layout; `Common/DataFile.h` computes offsets using `sizeof(uint32_t)`, and `PipelineManager.cpp` reads them as `const uint32_t*`.

#### DataPacker/Source/ExportJobs/Island/BakeIslandIntermediates (out-of-bound)
- F1 | `DataPacker/Source/ExportJobs/Island/BakeIslandIntermediates.cpp:173` | Rule 13 | rule edge case: `get<int64_t>()` followed by `static_cast<int32_t>(iSeed)` at line 270 narrows an unrestricted JSON seed; no range check proves the value always fits, as the sweep requires.

#### DataPacker/Source/ExportJobs/Island/BakeIslandIntermediatesInternal (out-of-bound)
- F3 | `DataPacker/Source/ExportJobs/Island/BakeIslandIntermediatesInternal.h:93` | Rule 13 | Leave unchanged: the elements cross a binary layout boundary. `BakeRoute.cpp:369` reads raw `AmbientOcclusion.r16` bytes into this buffer; `ProcessBakedRegion.cpp:187–190` copies and writes those elements as 16-bit pixels.
- F4 | `DataPacker/Source/ExportJobs/Island/BakeIslandIntermediatesInternal.h:100` | Rule 13 | Leave unchanged: the elements cross a binary layout boundary. `ProcessBakedRegion.cpp:349` passes this buffer to `WriteMeshProcessed`, which writes its raw 32-bit elements into `MeshProcessed.bin` at line 272.

#### DataPacker/Source/ExportJobs/Island/BakeRoute (out-of-bound)
- F4 | `DataPacker/Source/ExportJobs/Island/BakeRoute.cpp:52` | Rule 13 | Leave unchanged; rule edge case: the existing positive-dimension check permits squares above `INT64_MAX` but within `SIZE_MAX`, which callers subsequently reject through their extent checks. An `int64_t` return cannot represent that intermediate count; changing the multiplication or rejection bound changes its overflow behavior.
- `DataPacker/Source/ExportJobs/Island/BakeRoute.cpp:307` | Rule 13 | Reverted to `size_t`. rule edge case: the Gaea reads in `LoadElevationMeters` (:307–322) and `LoadAmbientOcclusion` (:349–364) check `uiPixelCount > SIZE_MAX / sizeof(element)`, then compare the byte count against `std::streamsize` max and the `uintmax_t` from `std::filesystem::file_size`. In `int64_t`, the byte product can overflow before the check and the comparison with `file_size` mixes signed and unsigned operands.
- `DataPacker/Source/ExportJobs/Island/BakeRoute.cpp:436` | Rule 13 | Reverted to `size_t`. rule edge case: `LoadMesherMesh`'s POSITION span guard `count - 1 > (byteLength - byteOffset - kuiPositionBytes) / uiStride` is one of the span checks that :418–419 says are written as `size_t` subtraction and division against the container size, over the tinygltf `size_t` fields `count`, `byteLength`, and `byteOffset`. A signed `int64_t` stride or count mixes signed operands into these `size_t` expressions, so the bound relies on an implicit conversion back to unsigned.
- F5 | `DataPacker/Source/ExportJobs/Island/BakeRoute.cpp:59` | Rule 13 | Leave unchanged; rule edge case: converting both multiplication operands at line 64 to `int64_t` permits signed overflow for positive dimensions whose square fits `size_t` but exceeds `INT64_MAX`; line 60 currently checks the unsigned multiplication bound.
- F9 | `DataPacker/Source/ExportJobs/Island/BakeRoute.cpp:344` | Rule 13 | Leave unchanged; the return element type carries the UshortRaw16 binary layout read into the vector at line 369 and written as `uint16_t` samples by `ProcessBakedRegion.cpp:180-190`.
- F11 | `DataPacker/Source/ExportJobs/Island/BakeRoute.cpp:348` | Rule 13 | Leave unchanged; line 369 reads raw UshortRaw16 bytes directly into this vector, and its elements retain that binary layout through the leaf AO writer.
- F14 | `DataPacker/Source/ExportJobs/Island/BakeRoute.cpp:382` | Rule 13 | Leave unchanged; the element type receives raw `uint32_t` bytes through `memcpy` at line 509 and reaches the binary mesh writer at `ProcessBakedRegion.cpp:272`. Changing it alters the layout.
- F25 | `DataPacker/Source/ExportJobs/Island/BakeRoute.cpp:658` | Rule 13 | Leave unchanged; this vector holds the raw UshortRaw16 samples returned by `LoadAmbientOcclusion` and passed through `BakeOutput` to the binary AO writer.
- F26 | `DataPacker/Source/ExportJobs/Island/BakeRoute.cpp:661` | Rule 13 | Leave unchanged; these elements receive raw glTF index bytes and are written as `uint32_t` elements of `MeshProcessed.bin` by `ProcessBakedRegion.cpp:272`.

#### DataPacker/Source/ExportJobs/Island/ProcessBakedRegion (out-of-bound)
- F1 | `DataPacker/Source/ExportJobs/Island/ProcessBakedRegion.cpp:180` | Rule 13 | Its element representation feeds binary AmbientOcclusion.r16 output at line 190; widening changes serialized bytes.
- F2 | `DataPacker/Source/ExportJobs/Island/ProcessBakedRegion.cpp:182` | Rule 13 | Its elements are copied as uint16_t bytes at line 187 and written directly to AmbientOcclusion.r16 at line 190.
- F3 | `DataPacker/Source/ExportJobs/Island/ProcessBakedRegion.cpp:203` | Rule 13 | This buffer feeds meshoptimizer's index representation and the binary uint32_t index payload written at line 272.
- F4 | `DataPacker/Source/ExportJobs/Island/ProcessBakedRegion.cpp:221` | Rule 13 | Moved into rMeshIndices at line 237 and ultimately written as uint32_t bytes at line 272.
- F13 | `DataPacker/Source/ExportJobs/Island/ProcessBakedRegion.cpp:264` | Rule 13 | Its uint32_t element representation is written directly to MeshProcessed.bin at line 272.
- F14 | `DataPacker/Source/ExportJobs/Island/ProcessBakedRegion.cpp:267` | Rule 13 | Its bytes are written directly as the int32_t MeshProcessed.bin vertex-count header at line 269.
- F16 | `DataPacker/Source/ExportJobs/Island/ProcessBakedRegion.cpp:268` | Rule 13 | Its bytes are written directly as the int32_t MeshProcessed.bin index-count header at line 270.
- F18 | `DataPacker/Source/ExportJobs/Island/ProcessBakedRegion.cpp:299` | Rule 13 | Passed to CropAndRepackMesh at line 348 and WriteMeshProcessed at line 349, which writes its uint32_t element bytes at line 272; widening crosses the binary layout boundary.

#### DataPacker/Source/ExportJobs/Island/SubdivideBeachBand (out-of-bound)
- F1 | `DataPacker/Source/ExportJobs/Island/SubdivideBeachBand.cpp:8` | Rule 13 | The referenced elements cross binary layout boundaries through BakeRoute.cpp:509 (memcpy) and ProcessBakedRegion.cpp:272 (raw uint32 index serialization).
- F27 | `DataPacker/Source/ExportJobs/Island/SubdivideBeachBand.cpp:182` | Rule 13 | This member references the binary uint32 mesh buffer copied at BakeRoute.cpp:509 and serialized at ProcessBakedRegion.cpp:272.
- F49 | `DataPacker/Source/ExportJobs/Island/SubdivideBeachBand.cpp:264` | Rule 13 | cpp:276 moves this vector into the binary uint32 mesh buffer serialized at ProcessBakedRegion.cpp:272; widening its elements changes the layout.
- F58 | `DataPacker/Source/ExportJobs/Island/SubdivideBeachBand.cpp:388` | Rule 13 | The public function receives the binary uint32 mesh buffer copied at BakeRoute.cpp:509 and serialized at ProcessBakedRegion.cpp:272.
- F60 | `DataPacker/Source/ExportJobs/Island/SubdivideBeachBand.h:18` | Rule 13 | The public declaration exposes the binary uint32 mesh buffer copied at BakeRoute.cpp:509 and serialized at ProcessBakedRegion.cpp:272.

#### DataPacker/Source/ExportJobs/Scene/SceneAnimationLoader (out-of-bound)
- F2 | `DataPacker/Source/ExportJobs/Scene/SceneAnimationLoader.cpp:9` | Rule 13 | rule edge case: converting `size_t uiCount` and the element-size parameter to `int64_t` makes multiplication signed; count `2^61` with element size `4` passes the existing `SIZE_MAX` check but overflows before accessor bounds rejection. Changing the overflow limit changes the existing validation error path.
- `DataPacker/Source/ExportJobs/Scene/SceneAnimationLoader.cpp:9` | Rule 13 | Reverted to `size_t`. rule edge case: when `CheckedAnimationSize` computed `iCount * iElementSize` in `int64_t`, a product above `INT64_MAX` went negative and passed the subsequent `> byteLength` byte-span checks in `AccessorFloats` (:64–90) and `LoadAnimations` (:310). The tinygltf `count`, `byteOffset`, and `byteLength` fields and `rBuffer.data.size()` are `size_t`, so the guards stay unsigned.

#### DataPacker/Source/ExportJobs/Scene/SceneSkeletonLoader (out-of-bound)
- F23 | `DataPacker/Source/ExportJobs/Scene/SceneSkeletonLoader.h:12` | Rule 13 | Leave unchanged: `std::vector<uint16_t> skinJointToNode` element bytes cross the pack layout boundary through `ExportScene.cpp:1033`'s `std::memcpy`, with section sizing at line 1006. Changing the element type to `int64_t` would change serialized bytes.
- `DataPacker/Source/ExportJobs/Scene/SceneSkeletonLoader.cpp:137` | Rule 13 | Reverted to `size_t`. rule edge case: `LoadSkeletonData`'s inverse-bind span guards (`count > SIZE_MAX / kuiMatrixSize` at :138, `byteOffset > SIZE_MAX - bufferView.byteOffset` at :161) bound products and sums of tinygltf `size_t` fields. In `int64_t`, `count * kuiMatrixSize` and the offset sum overflow as signed values for counts or offsets that the unsigned checks handle.

#### DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader (out-of-bound)
- F2 | `DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp:23` | Rule 13 | Reclassified local: rule edge case: `ByteStride()` can return `-1`; unsigned division by `sizeof(T)` followed by the existing `int` cast yields `-1` for the instantiated types, whereas the proposed `int64_t` cast yields a huge positive stride.
- F10 | `DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp:82` | Rule 17 | Reclassified local: rule edge case: replacing `static_cast<int>(rMaterials.size())` removes existing narrowing above `INT_MAX`, changing material-map values and subsequent index lookup behavior; no count bound establishes equivalence.
- F13 | `DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp:113` | Rule 17 | Reclassified local: rule edge case: widening `uiVertexStart` removes existing 32-bit truncation and unsigned wrap in `uiVertexStart + remap.at(j)`, changing vertex placement or serialized indices beyond the 32-bit range.
- F14 | `DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp:181` | Rule 13 | `std::vector<uint32_t> remap` crosses meshoptimizer’s fixed `unsigned int*` and `const unsigned int*` layout boundaries.
- F16 | `DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp:196` | Rule 13 | `std::vector<uint32_t>& rIndexBuffer` refers to fixed-width meshoptimizer buffers and the serialized model index stream.
- F18 | `DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp:208` | Rule 17 | Reclassified local: rule edge case: the accessor count is unvalidated here; signed addition of a nonempty buffer size and a count near `INT64_MAX` can overflow, replacing defined unsigned arithmetic and reserve failure with undefined behavior.
- F30 | `DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.h:18` | Rule 13 | `std::vector<uint32_t> indexBuffer` crosses meshoptimizer’s fixed-width buffer boundary and feeds the serialized model index stream.

#### DataPacker/Source/ExportJobs/SourceReadValidation (out-of-bound)
- F2 | `DataPacker/Source/ExportJobs/SourceReadValidation.h:10` | Rule 13 | rule edge case: converting `file_size()` to signed before line 11 loses its unsigned range; a successful result above `INT64_MAX` can become negative and bypass the existing maximum check.
- F3 | `DataPacker/Source/ExportJobs/SourceReadValidation.h:18` | Rule 13 | rule edge case: the helper permits products through `UINTMAX_MAX`; an `int64_t` result cannot preserve that domain or the subsequent unsigned extent validation.
- F4 | `DataPacker/Source/ExportJobs/SourceReadValidation.h:18` | Rule 13 | rule edge case: adopting signed arithmetic changes the overflow threshold at line 21; `INT64_MAX * 2` currently passes this helper without unsigned overflow.
- F5 | `DataPacker/Source/ExportJobs/SourceReadValidation.h:18` | Rule 13 | rule edge case: its full unsigned domain is preserved by line 20; signed adoption cannot represent an element size above `INT64_MAX` and changes the helper's accepted operands.
- F6 | `DataPacker/Source/ExportJobs/SourceReadValidation.h:20` | Rule 13 | rule edge case: this value controls the unsigned overflow divisor at line 21 and multiplication at line 25; signed conversion changes their range and can introduce signed overflow.
- F7 | `DataPacker/Source/ExportJobs/SourceReadValidation.h:28` | Rule 13 | rule edge case: a successful unsigned sum may exceed `INT64_MAX`; narrowing the return value changes the value supplied to subsequent extent checks.
- F8 | `DataPacker/Source/ExportJobs/SourceReadValidation.h:28` | Rule 13 | rule edge case: signed adoption changes line 30's overflow threshold; `INT64_MAX + 1` currently passes this helper without unsigned overflow.
- F9 | `DataPacker/Source/ExportJobs/SourceReadValidation.h:28` | Rule 13 | rule edge case: the subtraction at line 30 and sum at line 34 require the full unsigned domain to preserve the existing overflow predicate.
- F10 | `DataPacker/Source/ExportJobs/SourceReadValidation.h:37` | Rule 13 | rule edge case: signed adoption cannot represent its full unsigned input domain, and line 39 has no negative-value rejection.
- F11 | `DataPacker/Source/ExportJobs/SourceReadValidation.h:37` | Rule 13 | rule edge case: an oversized unsigned offset can become negative after signed conversion; converting the extent signature to signed makes line 39 accept offsets it currently rejects.
- F12 | `DataPacker/Source/ExportJobs/SourceReadValidation.h:37` | Rule 13 | rule edge case: multiplication results above `INT64_MAX` must remain unsigned for line 39 to reject them; signed conversion can turn an oversized byte count negative and bypass that check.
- F14 | `DataPacker/Source/ExportJobs/SourceReadValidation.h:64` | Rule 13 | rule edge case: converting an input above `INT64_MAX` to signed before line 66 can produce a negative value, bypass the maximum check, and request a backward seek instead of rejecting the input.

#### DataPacker/Source/ExportJobs/Texture/MigrateLegacyIntermediates (out-of-bound)
- F1 | `DataPacker/Source/ExportJobs/Texture/MigrateLegacyIntermediates.cpp:33` | Rule 13 | Returned RGBA elements cross a byte-layout boundary; widening changes texture input bytes.
- F2 | `DataPacker/Source/ExportJobs/Texture/MigrateLegacyIntermediates.cpp:38` | Rule 13 | RGBA buffer supplies the byte layout reinterpreted by `Texture`; widening changes its four-byte pixel stride.
- F4 | `DataPacker/Source/ExportJobs/Texture/MigrateLegacyIntermediates.cpp:51` | Rule 13 | `rgbcx::unpack_bc4` writes a contiguous one-byte channel layout; widening changes the decoder destination layout.
- F5 | `DataPacker/Source/ExportJobs/Texture/MigrateLegacyIntermediates.cpp:62` | Rule 13 | `rgbcx::unpack_bc5` writes a contiguous two-byte pixel layout; widening changes the decoder destination layout.
- F6 | `DataPacker/Source/ExportJobs/Texture/MigrateLegacyIntermediates.cpp:123` | Rule 13 | rule edge case: converting `file_size` to signed before checking loses values above `INT64_MAX` and changes rejection behavior; leave lines 123–128 unchanged.
- F12 | `DataPacker/Source/ExportJobs/Texture/MigrateLegacyIntermediates.cpp:245` | Rule 13 | RGBA elements are reinterpreted as bytes for `Texture`; widening changes its four-byte pixel layout.

#### DataPacker/Source/ExportJobs/Texture/Texture (out-of-bound)
- F6 | `DataPacker/Source/ExportJobs/Texture/Texture.cpp:107` | Rule 13 | rule edge case: signed dimension multiplication cannot preserve the checked-size block's accepted products above INT64_MAX.
- F7 | `DataPacker/Source/ExportJobs/Texture/Texture.cpp:108` | Rule 13 | rule edge case: signed dimension multiplication cannot preserve the checked-size block's accepted products above INT64_MAX.
- F9 | `DataPacker/Source/ExportJobs/Texture/Texture.cpp:110` | Rule 13 | Changing numeric_limits<uintmax_t> to int64_t lowers the multiplication limit and changes accepted dimensions.
- F10 | `DataPacker/Source/ExportJobs/Texture/Texture.cpp:114` | Rule 13 | rule edge case: int64_t cannot preserve permitted unsigned pixel products above INT64_MAX.
- F11 | `DataPacker/Source/ExportJobs/Texture/Texture.cpp:115` | Rule 13 | Changing numeric_limits<uintmax_t> to int64_t lowers the byte-count limit and changes accepted dimensions.
- F12 | `DataPacker/Source/ExportJobs/Texture/Texture.cpp:119` | Rule 13 | rule edge case: int64_t cannot preserve permitted byte counts above INT64_MAX or their file-length comparison.
- F13 | `DataPacker/Source/ExportJobs/Texture/Texture.cpp:129` | Rule 13 | Signed conversion changes the retained unsigned expected-byte comparison and validation.
- F14 | `DataPacker/Source/ExportJobs/Texture/Texture.cpp:141` | Rule 13 | uint16_t defines serialized sample normalization; int64_t violates the unsigned_integral constraint and changes its range.
- F30 | `DataPacker/Source/ExportJobs/Texture/Texture.cpp:405` | Rule 13 | Changing packed RGBA array element width alters encoder input layout and emitted texture bytes.
- F31 | `DataPacker/Source/ExportJobs/Texture/Texture.cpp:505` | Rule 13 | uint16_t defines serialized R16 normalization and layout; int64_t violates the unsigned_integral constraint.
- F34 | `DataPacker/Source/ExportJobs/Texture/Texture.cpp:597` | Rule 13 | Changing packed RGB buffer element width alters pixel stride, interpretation, and JPEG output.
- F43 | `DataPacker/Source/ExportJobs/Texture/Texture.cpp:690` | Rule 13 | rule edge case: int64_t wraps negative after INT64_MAX, changing the staging basename instead of retaining the unsigned sequence.

#### Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsClient (out-of-bound)
- F2 | `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsClient.cpp:33` | Rule 13 | `uint64_t uiValue = rValue.get<uint64_t>();` | Leave unchanged. rule edge case: converting the unsigned JSON value to `int64_t` before the line 34 range check loses values above INT64_MAX and can change which coordinates are rejected; the full unsigned range must survive until validation. References are lines 34 and 38.

#### Projects/BrokenEngineSandbox/Source/Agent/Commands/ClientPacketFaultFixture (out-of-bound)
- F1 | `Projects/BrokenEngineSandbox/Source/Agent/Commands/ClientPacketFaultFixture.cpp:16` | Rule 13 | Stores serialized network packet bytes, moved into `packet` at line 91 and passed to `Client::Receive(std::span<const uint8_t>)` at line 108; changing the element width changes the packet layout.
- F2 | `Projects/BrokenEngineSandbox/Source/Agent/Commands/ClientPacketFaultFixture.cpp:71` | Rule 13 | Constructs a three-byte serialized network envelope, moved into `sArmedPacketFault` at line 78 and ultimately read by `NetworkMessages::Read` through `Client::Receive`; widening elements changes the packet layout and fault behavior. Leave unchanged, including the cast.
- F3 | `Projects/BrokenEngineSandbox/Source/Agent/Commands/ClientPacketFaultFixture.cpp:91` | Rule 13 | Owns the serialized network bytes passed to `Client::Receive(std::span<const uint8_t>)` at line 108; changing the element width changes the packet layout.

#### Projects/BrokenEngineSandbox/Source/Agent/Commands/ClientSubscriptionFixtures (out-of-bound)
- F2 | `Projects/BrokenEngineSandbox/Source/Agent/Commands/ClientSubscriptionFixtures.cpp:256` | Rule 13 | rule edge case: widening `uiEpoch` and delaying narrowing would bypass the zero check after 65535 wraps, serializing epoch 0 instead of 1.
- F3 | `Projects/BrokenEngineSandbox/Source/Agent/Commands/ClientSubscriptionFixtures.cpp:274` | Rule 13 | `rWorkbuffer.Span<uint8_t>()` selects serialized network bytes passed to `Client::Receive(std::span<const uint8_t>)`; changing element width would reinterpret the packet layout.

#### Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFaultFixtures (out-of-bound)
- F1 | `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFaultFixtures.cpp:89` | Rule 13 | Leave unchanged: payload elements represent network bytes; changing their width breaks the packet layout contract.
- F2 | `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFaultFixtures.cpp:93` | Rule 13 | Leave unchanged: payload elements represent network bytes; changing their width changes the undersized fixture's byte layout.
- F3 | `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFaultFixtures.cpp:97` | Rule 13 | Leave unchanged: payload elements represent network bytes; changing their width changes the oversized fixture's byte layout.
- F4 | `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFaultFixtures.cpp:103` | Rule 13 | Leave unchanged: payload elements represent network bytes; changing their width changes the over-cap fixture's byte layout.
- F5 | `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFaultFixtures.cpp:173` | Rule 13 | Leave unchanged: `packet` contains the raw network message passed to `Server::Receive`; widening its elements changes the message layout and parser behavior.
- F9 | `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFaultFixtures.cpp:258` | Rule 13 | Leave unchanged: the span views serialized `ClientAckStreamMessage` bytes through `reinterpret_cast` and supplies the network parser's byte layout.

#### Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/Blasters (out-of-bound)
- F3 | `Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/Blasters.h:55` | Rule 13 | This Collection data column participates in SharedMembers and PersistentMembers at Blasters.h:66,81,83 and in cross-cell transfer at Blasters.cpp:187; changing its element width crosses the collection persistence, transfer, and hydration layout boundary.

#### Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles (out-of-bound)
- F2 | `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.cpp:64` | Rule 13 | rule edge case: `int64_t` cannot bind to `TypeRegistry::RegisterType(uint8_t&, const TYPE&)`; changing that signature requires widening excluded Collection members.
- F3 | `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.cpp:65` | Rule 13 | rule edge case: `int64_t` cannot bind to `TypeRegistry::RegisterType(uint8_t&, const TYPE&)`; changing that signature requires widening excluded Collection members.
- F4 | `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.cpp:67` | Rule 13 | rule edge case: `int64_t` cannot bind to `TypeRegistry::RegisterType(uint8_t&, const TYPE&)`; changing that signature requires widening excluded Collection members.
- F5 | `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/Missiles.cpp:70` | Rule 13 | rule edge case: `int64_t` cannot bind to `TypeRegistry::RegisterType(uint8_t&, const TYPE&)`; changing that signature requires widening excluded Collection members.

#### Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players (out-of-bound)
- F7 | `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.h:68` | Rule 13 | The Plan explicitly excludes members of a `Collection<T>` struct.
- F8 | `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.h:69` | Rule 13 | The Plan explicitly excludes members of a `Collection<T>` struct.
- F17 | `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.h:280` | Rule 13 | Widening the collection element type changes serialized and per-tick CRC bytes; the column participates in `SharedMembers` at Players.h:288 and `SharedCrcMembers` at line 312.
- F18 | `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.h:281` | Rule 13 | Widening the collection element type changes serialized and per-tick CRC bytes; the column participates in `SharedMembers` at Players.h:288 and `SharedCrcMembers` at line 312.

#### Projects/BrokenEngineSandbox/Source/Frame/StatusChange (out-of-bound)
- F1 | `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:60` | Rule 13 | SpawnPlayerData member crosses wire serialization in Engine/Source/Network/NetworkSerialization.cpp:148 and raw replay serialization in FrameInput.cpp:35.
- F2 | `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:80` | Rule 13 | UpdatePlayerData member crosses wire serialization in Engine/Source/Network/NetworkSerialization.cpp:174 and raw replay serialization in FrameInput.cpp:35.
- F3 | `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:89` | Rule 13 | UpdateFleetData member crosses wire serialization in Engine/Source/Network/NetworkSerialization.cpp:183 and raw replay serialization in FrameInput.cpp:35.
- F4 | `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:111` | Rule 13 | TransferData member crosses wire serialization in Engine/Source/Network/NetworkSerialization.cpp:16 and raw replay serialization in FrameInput.cpp:35.
- F5 | `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:147` | Rule 13 | TransferData member crosses wire serialization in Engine/Source/Network/NetworkSerialization.cpp:63 and raw replay serialization in FrameInput.cpp:35.
- F6 | `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:148` | Rule 13 | TransferData member crosses wire serialization in Engine/Source/Network/NetworkSerialization.cpp:64 and raw replay serialization in FrameInput.cpp:35.
- F7 | `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:151` | Rule 13 | Although omitted from network serialization and SharedMembers(), this TransferData member crosses raw replay serialization through FrameInput.cpp:35 and Common/Serialization.h:99–102.
- F8 | `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:152` | Rule 13 | Although omitted from network serialization and SharedMembers(), this TransferData member crosses raw replay serialization through FrameInput.cpp:35 and Common/Serialization.h:99–102.

#### Projects/BrokenEngineSandbox/Source/ClientSettings (out-of-bound)
- F1 | `Projects/BrokenEngineSandbox/Source/ClientSettings.cpp:18` | Rule 13 | `uiShowImGui` belongs to the asserted, serialized `TweaksSettings` layout; widening changes saved bytes and validation behavior.
- F2 | `Projects/BrokenEngineSandbox/Source/ClientSettings.cpp:19` | Rule 13 | `uiPadding[3]` belongs to the asserted, serialized `TweaksSettings` layout; widening changes saved bytes.
- F3 | `Projects/BrokenEngineSandbox/Source/ClientSettings.cpp:69` | Rule 13 | `uiPadding[4]` belongs to the serialized `ClientStateSettings` layout; widening changes saved bytes.

#### Projects/BrokenEngineSandbox/Source/Fleet (out-of-bound)
- F1 | `Projects/BrokenEngineSandbox/Source/Fleet.h:45` | Rule 13 | Leave unchanged: serialized layout boundary. `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp:43` writes this member directly and line 71 reads it directly; replacing it with `int64_t iPendingFleetWantedCoordinateTicks = 0;` would change grid-save bytes.

#### Projects/BrokenEngineSandbox/Source/Game (out-of-bound)
- F4 | `Projects/BrokenEngineSandbox/Source/Game.h:36` | Rule 13 | `uint8_t uiPadding[4] {};` is serialized `ReplayMeta` padding; changing its element width changes the persisted layout.

#### Projects/BrokenEngineSandbox/Source/Network/GameMessages (out-of-bound)
- F1 | `Projects/BrokenEngineSandbox/Source/Network/GameMessages.h:30` | Rule 13 | Leave unchanged: serialized by `Visit` at line 37; changing to `int64_t` would widen the wire field from one byte to eight.
- F2 | `Projects/BrokenEngineSandbox/Source/Network/GameMessages.h:92` | Rule 13 | Leave unchanged; rule edge case: `Count<T>()` divides the buffer byte length by `sizeof(T)`, so `Count<int64_t>()` would invalidate the expected payload byte count.
- F3 | `Projects/BrokenEngineSandbox/Source/Network/GameMessages.h:98` | Rule 13 | Leave unchanged; rule edge case: `Count<int64_t>()` would measure eight-byte elements rather than the byte offset used by the header-size assertion at line 101.
- F4 | `Projects/BrokenEngineSandbox/Source/Network/GameMessages.h:101` | Rule 13 | Leave unchanged; rule edge case: `Count<int64_t>()` would compare eight-byte element counts against header offsets and sizes measured in bytes.
- F5 | `Projects/BrokenEngineSandbox/Source/Network/GameMessages.h:105` | Rule 13 | Leave unchanged; rule edge case: `Count<int64_t>()` would measure eight-byte elements rather than the byte offset used by the member-size assertion at line 108.
- F6 | `Projects/BrokenEngineSandbox/Source/Network/GameMessages.h:108` | Rule 13 | Leave unchanged; rule edge case: `Count<int64_t>()` would compare eight-byte element counts against member offsets and sizes measured in bytes.
- F7 | `Projects/BrokenEngineSandbox/Source/Network/GameMessages.h:112` | Rule 13 | Leave unchanged; rule edge case: `Count<int64_t>()` would compare eight-byte element counts against the expected payload size measured in bytes.
- F8 | `Projects/BrokenEngineSandbox/Source/Network/GameMessages.h:117` | Rule 13 | Leave unchanged: the container holds serialized network bytes consumed by `MessageReader`; widening its element type changes the payload representation.
- F9 | `Projects/BrokenEngineSandbox/Source/Network/GameMessages.h:119` | Rule 13 | Leave unchanged: this span exposes serialized network bytes to `MessageReader`, whose constructor requires `std::span<const uint8_t>`; widening the element type changes the wire representation.

#### Projects/BrokenEngineSandbox/Source/Network/NetworkSessionContract (out-of-bound)
- F1 | `Projects/BrokenEngineSandbox/Source/Network/NetworkSessionContract.h:32` | Rule 13 | `std::span<uint8_t> destination` contains serialized network bytes; changing its element type crosses the excluded wire layout boundary.
- F2 | `Projects/BrokenEngineSandbox/Source/Network/NetworkSessionContract.h:36` | Rule 13 | `std::span<const uint8_t> source` contains serialized network bytes; changing its element type crosses the excluded wire layout boundary.

#### Projects/BrokenEngineSandbox/Source/Network/PlayerEvents (out-of-bound)
- F1 | `Projects/BrokenEngineSandbox/Source/Network/PlayerEvents.cpp:12` | Rule 13 | Leave unchanged: elements are wire payload bytes consumed by `engine::NetworkMessages::Read` at lines 21 and 27; changing their width crosses the excluded network serialization layout boundary.
- F2 | `Projects/BrokenEngineSandbox/Source/Network/PlayerEvents.cpp:34` | Rule 13 | Leave unchanged: elements are wire payload bytes consumed by `GameMessages::FleetSyncMessage::ReadPayload` at line 51; changing their width crosses the excluded network serialization layout boundary.
- F3 | `Projects/BrokenEngineSandbox/Source/Network/PlayerEvents.h:32` | Rule 13 | Leave unchanged: declaration of `ParsePlayerEvents` matches the wire payload container in `PlayerEvents.cpp:12`; changing its element width crosses the excluded network serialization layout boundary.
- F4 | `Projects/BrokenEngineSandbox/Source/Network/PlayerEvents.h:34` | Rule 13 | Leave unchanged: declaration of `ParseFleetSynchronization` matches the wire payload container in `PlayerEvents.cpp:34`; changing its element width crosses the excluded network serialization layout boundary.

#### Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization (out-of-bound)
- F1 | `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp:28` | Rule 13 | Conversion would widen the serialized network packet tag from one byte to eight bytes.
- F2 | `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp:66` | Rule 13 | Conversion would change direct save/replay consumption from four bytes to eight.
- F3 | `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp:67` | Rule 13 | Conversion would change direct save/replay consumption from four bytes to eight.
- F4 | `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp:108` | Rule 13 | Conversion would change direct save/replay consumption from four bytes to eight.
- F5 | `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp:109` | Rule 13 | Conversion would change direct save/replay consumption from four bytes to eight.
- F6 | `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp:177` | Rule 13 | Serialized GUID component assigned to an asserted unsigned layout; signed conversion cannot preserve the full unsigned value range without additional representation conversions.
- F7 | `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp:178` | Rule 13 | Serialized GUID component assigned to an asserted unsigned layout; signed conversion cannot preserve the full unsigned value range without additional representation conversions.

#### Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession (out-of-bound)
- F2 | `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:312` | Rule 13 | The template argument controls the serialized packet-type width. Changing it to `int64_t` writes eight bytes instead of one.
- F3 | `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:315` | Rule 13 | rule edge case: `Count<T>()` divides the buffer byte count by `sizeof(T)`. Substituting `int64_t` breaks the assertion against the serialized byte size; the return type already is `int64_t`.
- F4 | `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:332` | Rule 13 | The template argument controls the serialized packet-type width. Changing it to `int64_t` writes eight bytes instead of one.
- F5 | `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:335` | Rule 13 | rule edge case: `Count<T>()` divides the buffer byte count by `sizeof(T)`. Substituting `int64_t` breaks the assertion against the serialized byte size; the return type already is `int64_t`.

#### Tools/ToolCommon/CoordinationStore (out-of-bound)
- F7 | `Tools/ToolCommon/CoordinationStore.cpp:382` | Rule 13 | rule edge case: replacing unsigned JSON extraction with `get<int64_t>()` can incorrectly match values above `INT64_MAX`, changing validation acceptance.
- F8 | `Tools/ToolCommon/CoordinationStore.cpp:391` | Rule 13 | rule edge case: converting before the signed-range check loses the original unsigned range and can accept values above `INT64_MAX`.
- F9 | `Tools/ToolCommon/CoordinationStore.cpp:413` | Rule 13 | rule edge case: changing the range target to `int64_t` accepts negative PIDs and PIDs above `UINT32_MAX`.

#### Tools/WorktreeCli/BuildCommand (out-of-bound)
- F10 | `Tools/WorktreeCli/BuildCommand.cpp:225` | Rule 13 | Leave unchanged. Rule edge case: replacing `std::numeric_limits<int>` with `std::numeric_limits<int64_t>` widens the accepted diagnostic location range from `INT_MAX` to `INT64_MAX`, changing previously rejected line and column values into emitted JSON values.
- F20 | `Tools/WorktreeCli/BuildCommand.cpp:748` | Rule 13 | Leave `return static_cast<int>(*msbuildExitCode);` unchanged. Rule edge case: removing the signed 32-bit conversion changes MSBuild exit codes with the high bit set; `0x80000000` currently becomes `-2147483648`, whereas returning the widened value produces `2147483648` in the build result's top-level `exitCode`.

### Declined

#### Engine/Source/Graphics/Render/WindUniforms (declined)
- F1 | `Engine/Source/Graphics/Render/WindUniforms.cpp:49` | Rule 13 | Already uses `int64_t iWindWidth` with the required call-site cast.
- F2 | `Engine/Source/Graphics/Render/WindUniforms.cpp:50` | Rule 13 | Already uses `int64_t iWindHeight` with the required call-site cast.
- F3 | `Engine/Source/Graphics/Render/WindUniforms.cpp:51` | Rule 13 | Already uses `int64_t iWindTilesX`; all local references and GPU assignment casts comply.
- F4 | `Engine/Source/Graphics/Render/WindUniforms.cpp:52` | Rule 13 | Already uses `int64_t iWindTilesY`; all local references and GPU assignment casts comply.
