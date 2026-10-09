#if defined(BT_CLIENT)

#include "Islands.h"

#include "Frame/CellStaticData.h"
#include "Frame/IslandChainPlacement.h"

namespace engine
{

Islands::Islands()
: common::Singleton<Islands>(gpIslands)
{
	// The record-once terrain CB binds this arena once. Template mesh residency changes only its
	// sub-allocation offsets in indirect commands, never this VkBuffer handle.
	mIslandMeshArena.Create(
	{
		.name = "IslandMeshArena",
		.flags = {BufferFlags::kIndexVertex, BufferFlags::kDeviceLocal},
		.vkIndexType = VK_INDEX_TYPE_UINT32,
		.iVertexStride = static_cast<int64_t>(2 * sizeof(float)),
		.iDataSize = kiIslandMeshArenaBytes,
	});
	VmaVirtualBlockCreateInfo vmaVirtualBlockCreateInfo
	{
		.size = static_cast<VkDeviceSize>(kiIslandMeshArenaBytes),
	};
	CHECK_VK(vmaCreateVirtualBlock(&vmaVirtualBlockCreateInfo, &mIslandMeshVirtualBlock));

	miTemplateCount = std::ssize(gpIslandTerrain->mIslandCrcsSorted);
	ASSERT(miTemplateCount > 0);
	// Slot 0 is the reserved neutral placeholder (miNextTextureSlot starts at 1), so the usable budget
	// is kiMaxIslands - 1 real templates.
	ASSERT(miTemplateCount < shaders::kiMaxIslands);

	// SSBO + indirect buffers are buffered per framebuffer: one instance per framebuffer index (kiMaxFramebuffers),
	// all created once here and indexed by gpSwapchainManager->miFramebufferIndex thereafter. This keeps the
	// per-frame host rewrite in UpdateActiveIslands off the memory an in-flight frame is still GPU-reading.
	// All kiMaxFramebuffers instances are allocated regardless of the live framebuffer count so any index
	// stays valid across a swapchain recreation that changes the count (mpIslands is not rebuilt then).
	int64_t iStorageBufferEntryCount = kiMaxActivePlacements;
	int64_t iIndirectSize = miTemplateCount * static_cast<int64_t>(sizeof(VkDrawIndexedIndirectCommand));

	for (int64_t i = 0; i < kiMaxFramebuffers; ++i)
	{
		// SSBO: one shared kiMaxActivePlacements-entry arena. Zero-initialized — every slot is a zero-width
		// quad until UpdateActiveIslands writes a real placement, which produces a degenerate triangle the
		// vertex shader culls.
		mIslandsStorageBuffers.at(i).Create(
		{
			.name = "Islands",
			.flags = {BufferFlags::kStorage, BufferFlags::kHostVisible},
			.iCount = iStorageBufferEntryCount,
			.iVertexStride = sizeof(shaders::AxisAlignedQuadLayout),
			.iDataSize = iStorageBufferEntryCount * static_cast<int64_t>(sizeof(shaders::AxisAlignedQuadLayout)),
		});
		std::memset(mIslandsStorageBuffers.at(i).mpMappedMemory, 0, static_cast<size_t>(iStorageBufferEntryCount) * sizeof(shaders::AxisAlignedQuadLayout));

		// Per-template VkDrawIndexedIndirectCommand buffer. Residency writes indexCount / firstIndex /
		// vertexOffset in the drained churn window; firstInstance and instanceCount are rewritten per frame by
		// UpdateActiveIslands.
		VmaAllocationInfo vmaAllocationInfo {};
		Buffer::CreateBuffer("IslandsIndirect", iIndirectSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, mIslandsIndirectVkBuffers.at(i), mIslandsIndirectVmaAllocations.at(i), &vmaAllocationInfo);
		mppIslandsIndirectMappedVkDrawIndexedIndirectCommands.at(i) = static_cast<VkDrawIndexedIndirectCommand*>(vmaAllocationInfo.pMappedData);

		for (int64_t j = 0; j < miTemplateCount; ++j)
		{
			mppIslandsIndirectMappedVkDrawIndexedIndirectCommands.at(i)[j] = VkDrawIndexedIndirectCommand
			{
				.indexCount = 0,
				.instanceCount = 0,
				.firstIndex = 0,
				.vertexOffset = 0,
			};
		}
	}
}

Islands::~Islands()
{
	vmaClearVirtualBlock(mIslandMeshVirtualBlock);
	vmaDestroyVirtualBlock(mIslandMeshVirtualBlock);
	mIslandMeshArena.Destroy();

	// SSBO buffers (std::array<Buffer>) free via RAII; the manually-allocated indirect buffers do not.
	for (int64_t i = 0; i < kiMaxFramebuffers; ++i)
	{
		vmaDestroyBuffer(gpDeviceManager->mpAllocator, mIslandsIndirectVkBuffers.at(i), mIslandsIndirectVmaAllocations.at(i));
	}
}

bool Islands::AllocateMeshRanges(int64_t iIndexSize, int64_t iVertexSize, VmaVirtualAllocation& rIndexAllocation, int64_t& riIndexOffset, VmaVirtualAllocation& rVertexAllocation, int64_t& riVertexOffset)
{
	ASSERT(mIslandMeshVirtualBlock != VK_NULL_HANDLE);

	VmaVirtualAllocation vmaIndexAllocation = VK_NULL_HANDLE;
	VkDeviceSize vkIndexOffset = 0;
	VmaVirtualAllocationCreateInfo indexAllocationCreateInfo
	{
		.size = static_cast<VkDeviceSize>(iIndexSize),
		.alignment = sizeof(uint32_t),
	};
	if (vmaVirtualAllocate(mIslandMeshVirtualBlock, &indexAllocationCreateInfo, &vmaIndexAllocation, &vkIndexOffset) != VK_SUCCESS)
	{
		return false;
	}

	VmaVirtualAllocation vmaVertexAllocation = VK_NULL_HANDLE;
	VkDeviceSize vkVertexOffset = 0;
	VmaVirtualAllocationCreateInfo vertexAllocationCreateInfo
	{
		.size = static_cast<VkDeviceSize>(iVertexSize),
		.alignment = 2 * sizeof(float),
	};
	if (vmaVirtualAllocate(mIslandMeshVirtualBlock, &vertexAllocationCreateInfo, &vmaVertexAllocation, &vkVertexOffset) != VK_SUCCESS)
	{
		vmaVirtualFree(mIslandMeshVirtualBlock, vmaIndexAllocation);
		return false;
	}

	rIndexAllocation = vmaIndexAllocation;
	riIndexOffset = static_cast<int64_t>(vkIndexOffset);
	rVertexAllocation = vmaVertexAllocation;
	riVertexOffset = static_cast<int64_t>(vkVertexOffset);
	return true;
}

void Islands::FreeMeshRanges(VmaVirtualAllocation vmaIndexAllocation, VmaVirtualAllocation vmaVertexAllocation)
{
	ASSERT(mIslandMeshVirtualBlock != VK_NULL_HANDLE);
	ASSERT(vmaIndexAllocation != VK_NULL_HANDLE);
	ASSERT(vmaVertexAllocation != VK_NULL_HANDLE);
	vmaVirtualFree(mIslandMeshVirtualBlock, vmaIndexAllocation);
	vmaVirtualFree(mIslandMeshVirtualBlock, vmaVertexAllocation);
	++miMeshArenaCapacityGeneration;
}

void Islands::UploadMesh(int64_t iIndexOffset, std::span<const std::byte> indexData, int64_t iVertexOffset, std::span<const std::byte> vertexData)
{
	ASSERT(iIndexOffset >= 0 && iIndexOffset <= kiIslandMeshArenaBytes && std::ssize(indexData) <= kiIslandMeshArenaBytes - iIndexOffset);
	ASSERT(iVertexOffset >= 0 && iVertexOffset <= kiIslandMeshArenaBytes && std::ssize(vertexData) <= kiIslandMeshArenaBytes - iVertexOffset);
	DeviceLocalBufferUpload uploads[]
	{
		{.pData = indexData.data(), .iDestinationOffset = iIndexOffset, .iSize = std::ssize(indexData)},
		{.pData = vertexData.data(), .iDestinationOffset = iVertexOffset, .iSize = std::ssize(vertexData)},
	};
	Buffer::UploadToDeviceLocal(mIslandMeshArena.mDeviceLocalVkBuffer, uploads);
}

void Islands::WriteMeshIndirect(int64_t iTemplate, int64_t iIndexOffset, int64_t iVertexOffset, int64_t iIndexCount)
{
	ASSERT(iTemplate >= 0 && iTemplate < miTemplateCount);
	ASSERT(iIndexOffset % static_cast<int64_t>(sizeof(uint32_t)) == 0);
	ASSERT(iVertexOffset % static_cast<int64_t>(2 * sizeof(float)) == 0);
	for (VkDrawIndexedIndirectCommand* pVkIndirect : mppIslandsIndirectMappedVkDrawIndexedIndirectCommands)
	{
		VkDrawIndexedIndirectCommand& rVkIndirect = pVkIndirect[iTemplate];
		rVkIndirect.indexCount = static_cast<uint32_t>(iIndexCount);
		rVkIndirect.firstIndex = static_cast<uint32_t>(iIndexOffset / static_cast<int64_t>(sizeof(uint32_t)));
		rVkIndirect.vertexOffset = static_cast<int32_t>(iVertexOffset / static_cast<int64_t>(2 * sizeof(float)));
	}
}

void Islands::UpdateActiveIslands(const std::unordered_map<GridCoord, Cell>& rCells, std::span<const GridCoord> activeCoordinates)
{
	// Write only the framebuffer instance the current frame will consume. miFramebufferIndex was set by the
	// trailing AcquireNextImage of the prior render (Graphics.cpp); it is the index RenderGlobal reads
	// and the record-once CB for that framebuffer binds, and is stable until this frame's
	// submission. Re-acquiring this image index implies the prior frame that used it has presented, so its
	// GPU read of this instance has finished; this frame's render is not yet submitted — hence no host/GPU race.
	int64_t iFramebuffer = gpSwapchainManager->miFramebufferIndex;
	Buffer& rStorageBuffer = mIslandsStorageBuffers.at(iFramebuffer);
	VkDrawIndexedIndirectCommand* pVkIndirect = mppIslandsIndirectMappedVkDrawIndexedIndirectCommands.at(iFramebuffer);

	// Per-template reference counts reset each frame. Templates with reference count zero for
	// kiGraceRenderFrames become eviction candidates in RenderGlobal's next post-fence EvictionSweep.
	for (auto& [rCrc, rTemplate] : gpIslandTerrain->mIslands)
	{
		rTemplate.iReferenceCount = 0;
	}

	auto pStorageBufferQuads = reinterpret_cast<shaders::AxisAlignedQuadLayout*>(rStorageBuffer.mpMappedMemory);
	int64_t iPreviousWrittenTotal = mLastWrittenCounts.at(iFramebuffer);

	// Counting, prefix, and emission cursors are per-frame scratch in one contiguous thread-workbuffer
	// reservation. Keeping them in one reservation ensures a possible workbuffer grow happens before any
	// derived pointer is taken; the first two counts then stay immutable through emission so the indirect
	// record and final stale-tail clear use the same totals that established each run's base.
	int64_t iTemplateArrayBytes = miTemplateCount * static_cast<int64_t>(sizeof(uint32_t));
	auto perTemplateScratch = common::gpThreadLocal->mWorkbuffer.PushBuffer<uint32_t*>(4 * iTemplateArrayBytes);
	uint32_t* puiPerTemplateMeshVisibleCount = perTemplateScratch.mpData;
	uint32_t* puiPerTemplateTotalCount = perTemplateScratch.mpData + miTemplateCount;
	uint32_t* puiPerTemplateBase = puiPerTemplateTotalCount + miTemplateCount;
	uint32_t* puiPerTemplateEmitCount = puiPerTemplateBase + miTemplateCount;
	std::memset(puiPerTemplateMeshVisibleCount, 0, static_cast<size_t>(iTemplateArrayBytes));
	std::memset(puiPerTemplateTotalCount, 0, static_cast<size_t>(iTemplateArrayBytes));
	std::memset(puiPerTemplateBase, 0, static_cast<size_t>(iTemplateArrayBytes));
	std::memset(puiPerTemplateEmitCount, 0, static_cast<size_t>(iTemplateArrayBytes));

	// mf4RenderVisibleArea is the straight-down frustum footprint at Z=0. The lowest terrain vertices
	// are sunk to mfSeaFloorElevation, whose perspective footprint is the widest; expand analytically
	// about the camera XY so every higher vertex lies within this conservative area.
	XMFLOAT4A f4EyePosition {};
	XMStoreFloat4A(&f4EyePosition, engine::gpCamera->mVecEyePosition);
	float fSeaFloorScale = (f4EyePosition.z - gpIslandTerrain->mfSeaFloorElevation) / f4EyePosition.z;
	XMFLOAT4 f4MeshVisibleArea = engine::gpCamera->mf4RenderVisibleArea;
	f4MeshVisibleArea.x = f4EyePosition.x + (f4MeshVisibleArea.x - f4EyePosition.x) * fSeaFloorScale;
	f4MeshVisibleArea.y = f4EyePosition.y + (f4MeshVisibleArea.y - f4EyePosition.y) * fSeaFloorScale;
	f4MeshVisibleArea.z = f4EyePosition.x + (f4MeshVisibleArea.z - f4EyePosition.x) * fSeaFloorScale;
	f4MeshVisibleArea.w = f4EyePosition.y + (f4MeshVisibleArea.w - f4EyePosition.y) * fSeaFloorScale;

	// Placement positions are local to the cell that owns them, so every pass below resolves that cell's offset from
	// the camera cell once and hands it to the visibility test and the emission that consume the position.
	auto IsMeshVisible = [&](const IslandPlacement& rPlacement, const IslandTemplate& rTemplate, XMFLOAT2 f2Offset)
	{
		float fRadius = 0.5f * std::hypot(rTemplate.fQuadFootprintX, rTemplate.fQuadFootprintY);
		XMFLOAT4 f4Position {rPlacement.f2WorldPosition.x + f2Offset.x, rPlacement.f2WorldPosition.y + f2Offset.y, 0.0f, 1.0f};
		return engine::gpCamera->InVisibleArea(f4MeshVisibleArea, f4Position, fRadius, fRadius, fRadius, fRadius);
	};

	// Count each active placement with the frame and visibility predicates used by both emission passes.
	// Acquire residency and texture slots once per placement; write placements after all per-template bases are known.
	for (const GridCoord& rCoordinate : activeCoordinates)
	{
		auto it = rCells.find(rCoordinate);
		if (it == rCells.end() || it->second.iSnapshotCount == 0)
		{
			continue;
		}

		const CellStaticData& rStaticData = it->second.staticData;
		XMFLOAT2 f2Offset = MakeRenderBasis(rCoordinate, engine::gpCamera->mBasisCoordinate).f2Offset;
		for (const IslandPlacement& rPlacement : rStaticData.islands)
		{
			IslandTemplate& rTemplate = gpIslandTerrain->mIslands.at(rPlacement.islandCrc);
			int64_t iTemplate = rTemplate.iTemplateArrayIndex;
			ASSERT(iTemplate >= 0 && iTemplate < miTemplateCount);
			++rTemplate.iReferenceCount;
			rTemplate.iLastUsedRenderFrame = gpGraphics->miFrameCounter;
			std::ignore = gpIslandTerrainResidency->AcquireTextureSlot(rPlacement.islandCrc);
			++puiPerTemplateTotalCount[iTemplate];
			if (IsMeshVisible(rPlacement, rTemplate, f2Offset))
			{
				++puiPerTemplateMeshVisibleCount[iTemplate];
			}
		}
	}

	// Check the aggregate before publishing any indirect command. A malformed placement set must not expose
	// a firstInstance/instanceCount pair that addresses beyond the shared SSBO arena.
	int64_t iCurrentWrittenTotal = 0;
	for (int64_t i = 0; i < miTemplateCount; ++i)
	{
		iCurrentWrittenTotal += puiPerTemplateTotalCount[i];
	}
	ASSERT(iCurrentWrittenTotal <= kiMaxActivePlacements);

	// Exclusive prefix sum of per-template totals establishes each contiguous run in the shared arena.
	// firstInstance and the mesh-visible instanceCount are refreshed for every template, including zero-count
	// templates, in the acquired framebuffer's indirect buffer only.
	int64_t iCurrentWrittenOffset = 0;
	for (int64_t i = 0; i < miTemplateCount; ++i)
	{
		puiPerTemplateBase[i] = static_cast<uint32_t>(iCurrentWrittenOffset);
		pVkIndirect[i].firstInstance = puiPerTemplateBase[i];
		pVkIndirect[i].instanceCount = puiPerTemplateMeshVisibleCount[i];
		iCurrentWrittenOffset += puiPerTemplateTotalCount[i];
	}

	auto EmitPlacement = [&](const IslandPlacement& rPlacement, const IslandTemplate& rTemplate, int64_t iTextureSlot, XMFLOAT2 f2Offset)
	{
		int64_t iTemplate = rTemplate.iTemplateArrayIndex;
		ASSERT(iTemplate >= 0 && iTemplate < miTemplateCount);
		int64_t iSlotInTemplate = puiPerTemplateEmitCount[iTemplate];
		int64_t iStorageBufferIndex = static_cast<int64_t>(puiPerTemplateBase[iTemplate]) + iSlotInTemplate;
		if (iStorageBufferIndex >= kiMaxActivePlacements)
		{
			// Placement indices must stay below kiMaxActivePlacements to keep writes within the shared arena.
			ASSERT(false);
			std::unreachable();
		}

		shaders::AxisAlignedQuadLayout& rQuad = pStorageBufferQuads[iStorageBufferIndex];

		rQuad.f4VertexRect.x = rPlacement.f2WorldPosition.x + f2Offset.x - 0.5f * rTemplate.fQuadFootprintX;
		rQuad.f4VertexRect.y = rPlacement.f2WorldPosition.y + f2Offset.y + 0.5f * rTemplate.fQuadFootprintY;
		rQuad.f4VertexRect.z = rTemplate.fQuadFootprintX;
		rQuad.f4VertexRect.w = -rTemplate.fQuadFootprintY;

		rQuad.f4TextureRect.x = 0.0f;
		rQuad.f4TextureRect.z = 1.0f;
		rQuad.f4TextureRect.y = 0.0f;
		rQuad.f4TextureRect.w = 1.0f;

		rQuad.f4Parameters.x = 0.0f;
		rQuad.fRotation = rPlacement.fRotation;
		rQuad.uiTextureSlot = static_cast<uint32_t>(iTextureSlot);

		++puiPerTemplateEmitCount[iTemplate];
	};

	// First emission pass packs the mesh-visible prefix for every template. Visible placements receive the
	// beginning of each template's run so the indirect terrain draw consumes exactly this prefix.
	for (const GridCoord& rCoordinate : activeCoordinates)
	{
		auto it = rCells.find(rCoordinate);
		if (it == rCells.end() || it->second.iSnapshotCount == 0)
		{
			continue;
		}

		const CellStaticData& rStaticData = it->second.staticData;
		XMFLOAT2 f2Offset = MakeRenderBasis(rCoordinate, engine::gpCamera->mBasisCoordinate).f2Offset;
		for (const IslandPlacement& rPlacement : rStaticData.islands)
		{
			const IslandTemplate& rTemplate = gpIslandTerrain->mIslands.at(rPlacement.islandCrc);
			if (IsMeshVisible(rPlacement, rTemplate, f2Offset))
			{
				EmitPlacement(rPlacement, rTemplate, rTemplate.iTextureSlot, f2Offset);
			}
		}
	}

	// Second pass appends every offscreen placement. Texture slots were acquired in the first pass; reuse the
	// template-owned slot without touching residency. Mesh-visible placements always remain in the prefix so
	// TerrainElevation covers every terrain mesh instance, while the full subscribed set remains available to
	// ShadowElevation.
	for (const GridCoord& rCoordinate : activeCoordinates)
	{
		auto it = rCells.find(rCoordinate);
		if (it == rCells.end() || it->second.iSnapshotCount == 0)
		{
			continue;
		}

		const CellStaticData& rStaticData = it->second.staticData;
		XMFLOAT2 f2Offset = MakeRenderBasis(rCoordinate, engine::gpCamera->mBasisCoordinate).f2Offset;
		for (const IslandPlacement& rPlacement : rStaticData.islands)
		{
			const IslandTemplate& rTemplate = gpIslandTerrain->mIslands.at(rPlacement.islandCrc);
			if (!IsMeshVisible(rPlacement, rTemplate, f2Offset))
			{
				EmitPlacement(rPlacement, rTemplate, rTemplate.iTextureSlot, f2Offset);
			}
		}
	}

	for (int64_t i = 0; i < miTemplateCount; ++i)
	{
		ASSERT(puiPerTemplateEmitCount[i] == puiPerTemplateTotalCount[i]);
	}

	if (iCurrentWrittenTotal < iPreviousWrittenTotal)
	{
		// The current run overwrote [0, iCurrentWrittenTotal). Clear only the stale tail from this frame's
		// total through the previous total; the prepasses draw the entire fixed arena and would otherwise see
		// placements left by the prior population of this framebuffer instance.
		std::memset(&pStorageBufferQuads[static_cast<size_t>(iCurrentWrittenTotal)], 0, static_cast<size_t>(iPreviousWrittenTotal - iCurrentWrittenTotal) * sizeof(shaders::AxisAlignedQuadLayout));
	}
	mLastWrittenCounts.at(iFramebuffer) = iCurrentWrittenTotal;
}

} // namespace engine

#endif // BT_CLIENT
