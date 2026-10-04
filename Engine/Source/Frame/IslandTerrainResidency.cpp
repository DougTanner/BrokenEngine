#if defined(BT_CLIENT)

#include "IslandTerrainResidency.h"

#include "File/PackChunks.h"
#include "Graphics/Managers/TextureManager.h"
#include "Graphics/Islands.h"

namespace engine
{

struct MeshRange
{
	uint64_t uiOffset = 0;
	uint64_t uiLength = 0;
	VkDeviceSize vkIndexSize = 0;
	VkDeviceSize vkVertexSize = 0;
};

static MeshRange GetMeshRange(const IslandTemplate& rTemplate)
{
	MeshRange range {};
	range.uiOffset = static_cast<uint64_t>(rTemplate.iHeightmapWidth) * static_cast<uint64_t>(rTemplate.iHeightmapHeight) * sizeof(uint16_t);
	range.vkVertexSize = static_cast<VkDeviceSize>(rTemplate.iMeshVertexCount) * 2 * sizeof(float);
	range.vkIndexSize = static_cast<VkDeviceSize>(rTemplate.iMeshIndexCount) * sizeof(uint32_t);
	range.uiLength = static_cast<uint64_t>(range.vkVertexSize + range.vkIndexSize);
	return range;
}

static void ReleaseMeshCpuRange(common::crc_t uiIslandCrc, IslandTemplate& rTemplate, const MeshRange& rRange)
{
	gpFileManager->mpPackChunks->DecommitChunkRange(uiIslandCrc, rRange.uiOffset, rRange.uiLength);
	rTemplate.bMeshCpuDecommitted = true;
	gpFileManager->mpPackChunks->mLoader.ResetChunkRangeReloadState(uiIslandCrc, rRange.uiOffset, rRange.uiLength);
}

static bool IsTextureRestorationPending(common::crc_t uiIslandCrc, const IslandTemplate& rTemplate)
{
	if (rTemplate.bGpuResident || rTemplate.iTextureSlot < 0)
	{
		return false;
	}
	const LazyChunk& rLazyChunk = gpFileManager->mpPackChunks->mLazyChunkMap.at(uiIslandCrc);
	common::crc_t residencyCrcs[4] =
	{
		rLazyChunk.header.islandHeader.colorsCrc,
		rLazyChunk.header.islandHeader.normalsCrc,
		rLazyChunk.header.islandHeader.ambientOcclusionCrc,
		rLazyChunk.header.islandHeader.masksCrc,
	};
	for (common::crc_t uiTextureCrc : residencyCrcs)
	{
		if (!gpFileManager->mpPackChunks->IsChunkReady(uiTextureCrc))
		{
			return false;
		}
	}
	return true;
}

// Upload an island's heightmap into its template-owned elevationTexture as an R16_SFLOAT image (raw
// byte-copy — the resident heightmap is already R16 half-float, matching the image's texel size).
// Reused at first-mint and on device-loss re-Create. Descriptor patching is deferred to
// RestorationSweep so it lands inside RenderGlobal's post-fence-wait descriptor-patch window.
static void CreateElevationTextureFromHeightmap(IslandTemplate& rTemplate, std::string_view name)
{
	// Boot ordering invariant: WaitForElevationMaps (called once at startup) is the only writer of
	// puiHeightmapHalf. AcquireTextureSlot must never run before it.
	ASSERT(rTemplate.puiHeightmapHalf != nullptr);
	// Heap: Texture::Create allocates GPU resources and uses a OneShotCommandBuffer.
	ScopedSuppressAllocationTracking suppress;
	rTemplate.elevationTexture.Create(TextureInfo
	{
		.name = name,
		.vkFormat = shaders::kVkFormatElevation,
		.vkExtent3D = {static_cast<uint32_t>(rTemplate.iHeightmapWidth), static_cast<uint32_t>(rTemplate.iHeightmapHeight), 1u},
		.uiMipLevels = 1u,
		.uiArrayLayers = 1u,
		.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
		.vkImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
		.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
		.eTextureLayout = TextureLayout::kShaderReadOnly,
	}, [&rTemplate](std::span<std::byte> data, int64_t iPosition)
	{
		std::memcpy(data.data(), reinterpret_cast<const std::byte*>(rTemplate.puiHeightmapHalf) + iPosition, static_cast<size_t>(static_cast<int64_t>(data.size_bytes())));
	});
}

IslandTerrainResidency::IslandTerrainResidency()
{
	ASSERT(gpIslandTerrainResidency == nullptr);

	gpIslandTerrainResidency = this;
}

IslandTerrainResidency::~IslandTerrainResidency()
{
	if (gpIslandTerrainResidency == this)
	{
		gpIslandTerrainResidency = nullptr;
	}
}

int64_t IslandTerrainResidency::FirstMintTextureSlot(common::crc_t uiIslandCrc, IslandTemplate& rTemplate, const common::crc_t (&rTextureCrcs)[4], std::string_view name)
{
	// First-mint. Slot 0 stays the neutral placeholder anchor (no real island ever maps there).
	// Reuse a slot reclaimed by a prior eviction before extending the high-water mark, so churn
	// (e.g. the menu island browser cycling repeatedly) reuses indices rather than exhausting the
	// fixed kiMaxIslands-sized descriptor arrays.
	int64_t iSlot = 0;
	if (mFreeTextureSlots.empty())
	{
		iSlot = miNextTextureSlot++;
	}
	else
	{
		iSlot = mFreeTextureSlots.back();
		mFreeTextureSlots.pop_back();
	}
	ASSERT(iSlot >= 1 && iSlot < shaders::kiMaxIslands);
	rTemplate.iTextureSlot = iSlot;

	// Elevation: uploaded directly from the in-memory heightmap into the template-owned
	// elevationTexture. Descriptor patching deferred to RestorationSweep (safety window).
	CreateElevationTextureFromHeightmap(rTemplate, name);

	gpTextureManager->mTextureDescriptors.MintIslandSlot(iSlot, uiIslandCrc, rTemplate.elevationTexture, rTextureCrcs);

	rTemplate.bGpuResident = false;
	gpFileManager->mpPackChunks->mLoader.RequestChunkLoad(rTextureCrcs, LoadPriority::kRealtime);
	LOG(kGraphics, kVerbose, "First-mint slot={} islandCrc={}", iSlot, uiIslandCrc);

	return iSlot;
}

int64_t IslandTerrainResidency::AcquireTextureSlot(common::crc_t uiIslandCrc)
{
	IslandTemplate& rTemplate = gpIslandTerrain->mIslands.at(uiIslandCrc);

	const LazyChunk& rLazyChunk = gpFileManager->mpPackChunks->mLazyChunkMap.at(uiIslandCrc);
	if (rTemplate.eMeshResidency == IslandMeshResidency::kNonresident)
	{
		MeshRange range = GetMeshRange(rTemplate);
		gpFileManager->mpPackChunks->mLoader.RequestChunkRangeReload(uiIslandCrc, range.uiOffset, range.uiLength, LoadPriority::kRealtime);
		rTemplate.eMeshResidency = IslandMeshResidency::kAsyncPending;
	}

	// Hot path still starts a mesh reload when a device recreation or earlier full teardown made
	// the mesh nonresident; texture residency itself remains unchanged.
	if (rTemplate.iTextureSlot >= 0 && rTemplate.bGpuResident)
	{
		return rTemplate.iTextureSlot;
	}

	// Color / Normals / AO / Masks ship as standalone lazy-texture chunks. Elevation lives on the template
	// (elevationTexture) and is uploaded directly from the in-memory heightmap — no chunk, no CRC.
	common::crc_t textureCrcs[4] =
	{
		rLazyChunk.header.islandHeader.colorsCrc,
		rLazyChunk.header.islandHeader.normalsCrc,
		rLazyChunk.header.islandHeader.ambientOcclusionCrc,
		rLazyChunk.header.islandHeader.masksCrc,
	};

	if (rTemplate.iTextureSlot < 0)
	{
		return FirstMintTextureSlot(uiIslandCrc, rTemplate, textureCrcs, rLazyChunk.header.pcPath);
	}

	// A freshly minted slot stays in slot-0 fallback while chunks load, until RestorationSweep patches it.
	// Eviction and device-loss ResetTextureSlots set iTextureSlot negative, so the next use re-enters
	// first mint and recreates elevationTexture.
	gpFileManager->mpPackChunks->mLoader.RequestChunkLoad(textureCrcs, LoadPriority::kRealtime);
	LOG(kLoading, kVerbose, "Re-acquire islandCrc={} slot={}, requesting chunk loads", uiIslandCrc, rTemplate.iTextureSlot);
	return rTemplate.iTextureSlot;
}

bool IslandTerrainResidency::AnyEvictionPending() const
{
	if (gpGraphics == nullptr || gpTextureManager == nullptr)
	{
		return false;
	}
	for (const auto& [rCrc, rTemplate] : gpIslandTerrain->mIslands)
	{
		if (IsEvictionPending(rTemplate))
		{
			return true;
		}
	}
	return false;
}

bool IslandTerrainResidency::AnyRestorationPending() const
{
	if (gpGraphics == nullptr || gpTextureManager == nullptr)
	{
		return false;
	}
	for (const auto& [rCrc, rTemplate] : gpIslandTerrain->mIslands)
	{
		if (IsRestorationPending(rCrc, rTemplate))
		{
			return true;
		}
	}
	return false;
}

bool IslandTerrainResidency::IsEvictionPending(const IslandTemplate& rTemplate) const
{
	return rTemplate.iTextureSlot != 0 && rTemplate.bGpuResident && rTemplate.iReferenceCount == 0
	    && (gpGraphics->muiFrameCounter - rTemplate.uiLastUsedRenderFrame) > kuiGraceRenderFrames;
}

bool IslandTerrainResidency::IsRestorationPending(common::crc_t uiIslandCrc, const IslandTemplate& rTemplate) const
{
	if (IsTextureRestorationPending(uiIslandCrc, rTemplate))
	{
		return true;
	}

	MeshRange range = GetMeshRange(rTemplate);
	switch (rTemplate.eMeshResidency)
	{
		case IslandMeshResidency::kAsyncPending:
			return gpFileManager->mpPackChunks->mLoader.GetChunkRangeReloadState(uiIslandCrc, range.uiOffset, range.uiLength) != ChunkRangeReloadState::kPending;
		case IslandMeshResidency::kCpuReady:
			return rTemplate.bGpuResident;
		case IslandMeshResidency::kArenaBlocked:
			return rTemplate.bGpuResident && (gpIslands->muiMeshArenaCapacityGeneration != rTemplate.uiMeshArenaBlockedGeneration || HasArenaEvictionCandidate(uiIslandCrc));
		case IslandMeshResidency::kNonresident:
		case IslandMeshResidency::kFailed:
		case IslandMeshResidency::kResident:
			return false;
	}
	return false;
}

bool IslandTerrainResidency::HasArenaEvictionCandidate(common::crc_t uiExcludedCrc) const
{
	for (const auto& [rCrc, rTemplate] : gpIslandTerrain->mIslands)
	{
		if (rCrc != uiExcludedCrc && rTemplate.eMeshResidency == IslandMeshResidency::kResident && rTemplate.bGpuResident && rTemplate.iReferenceCount == 0)
		{
			return true;
		}
	}
	return false;
}

bool IslandTerrainResidency::EvictTemplate(common::crc_t uiIslandCrc, IslandTemplate& rTemplate, MeshEvictionReason eReason)
{
	bool bEligible = eReason == MeshEvictionReason::kGrace ? IsEvictionPending(rTemplate) : rTemplate.iTextureSlot != 0 && rTemplate.bGpuResident && rTemplate.eMeshResidency == IslandMeshResidency::kResident && rTemplate.iReferenceCount == 0;
	if (!bEligible)
	{
		return false;
	}

	const LazyChunk& rLazyChunk = gpFileManager->mpPackChunks->mLazyChunkMap.at(uiIslandCrc);
	// The 4 chunk-backed channels (color/normals/AO/masks). Elevation is template-owned (no chunk
	// CRC) and is evicted separately below, via the template's own image rather than the chunk pool.
	common::crc_t evictCrcs[4] =
	{
		rLazyChunk.header.islandHeader.colorsCrc,
		rLazyChunk.header.islandHeader.normalsCrc,
		rLazyChunk.header.islandHeader.ambientOcclusionCrc,
		rLazyChunk.header.islandHeader.masksCrc,
	};

	LOG(kGraphics, kVerbose, "Evicting islandCrc={} slot={} (refCount=0, framesSinceUse={})", uiIslandCrc, rTemplate.iTextureSlot, gpGraphics->muiFrameCounter - rTemplate.uiLastUsedRenderFrame);

	int64_t iSlot = rTemplate.iTextureSlot;
	// Redirect every live descriptor to placeholders and retire its five generation records before
	// freeing any image view. A recycled slot cannot observe a destroyed prior occupant this way.
	gpTextureManager->mTextureDescriptors.EvictIslandSlot(iSlot, uiIslandCrc, evictCrcs);

	for (common::crc_t uiTextureCrc : evictCrcs)
	{
		gpTextureManager->mTextureMap.at(uiTextureCrc).Destroy();
	}
	rTemplate.elevationTexture.Destroy();

	MeshRange range = GetMeshRange(rTemplate);
	switch (rTemplate.eMeshResidency)
	{
		case IslandMeshResidency::kResident:
			// Indirect count must be zero in every framebuffer before the virtual ranges can be reused.
			gpIslands->WriteMeshIndirect(rTemplate.iTemplateArrayIndex, 0, 0, 0);
			gpIslands->FreeMeshRanges(rTemplate.meshIndexAllocation, rTemplate.meshVertexAllocation);
			rTemplate.meshIndexAllocation = VK_NULL_HANDLE;
			rTemplate.meshVertexAllocation = VK_NULL_HANDLE;
			rTemplate.vkMeshIndexOffset = 0;
			rTemplate.vkMeshVertexOffset = 0;
			rTemplate.uiMeshArenaBlockedGeneration = 0;
			rTemplate.eMeshResidency = IslandMeshResidency::kNonresident;
			break;
		case IslandMeshResidency::kCpuReady:
		case IslandMeshResidency::kArenaBlocked:
			ReleaseMeshCpuRange(uiIslandCrc, rTemplate, range);
			rTemplate.eMeshResidency = IslandMeshResidency::kNonresident;
			break;
		case IslandMeshResidency::kFailed:
			ReleaseMeshCpuRange(uiIslandCrc, rTemplate, range);
			rTemplate.eMeshResidency = IslandMeshResidency::kNonresident;
			break;
		case IslandMeshResidency::kAsyncPending:
		case IslandMeshResidency::kNonresident:
			break;
	}

	{
		// Heap: free-list growth runs inside RenderGlobal (EvictionSweep).
		ScopedSuppressAllocationTracking suppress;
		mFreeTextureSlots.push_back(iSlot);
	}
	rTemplate.iTextureSlot = -1;

	gpFileManager->mpPackChunks->ResetTextureChunkStates(evictCrcs);
	rTemplate.bGpuResident = false;

	LOG(kLoading, kVerbose, "Reset chunk states for evicted islandCrc={} evictCrcs=[{},{},{},{}]", uiIslandCrc, evictCrcs[0], evictCrcs[1], evictCrcs[2], evictCrcs[3]);
	return true;
}

void IslandTerrainResidency::EvictionSweep()
{
	if (gpGraphics == nullptr || gpTextureManager == nullptr)
	{
		return;
	}

	for (auto& [rCrc, rTemplate] : gpIslandTerrain->mIslands)
	{
		EvictTemplate(rCrc, rTemplate);
	}
}

void IslandTerrainResidency::RestorationSweep()
{
	if (gpGraphics == nullptr || gpTextureManager == nullptr)
	{
		return;
	}

	std::unordered_map<common::crc_t, IslandTemplate>& rIslands = gpIslandTerrain->mIslands;

	// First-mint assigns the four chunk-backed pointers, while elevation remains at the slot-0
	// placeholder until this all-four-ready transition. Chunk descriptor writes flow through
	// ProcessPendingTextures as each channel reaches kReady; RestoreIslandSlot switches elevation.
	for (auto& [rCrc, rTemplate] : rIslands)
	{
		if (IsTextureRestorationPending(rCrc, rTemplate))
		{
			// Patch the elevation array binding inside the safety window (RestorationSweep runs in
			// RenderGlobal post-fence-wait). The Texture's real VkImageView was created at first-mint
			// but the per-pipeline array descriptor still points at the slot-0 placeholder snapshot
			// taken at RegisterTextureBinding time. islandCrc was used as the binding key (the
			// template-owned elevationTexture has no chunk CRC).
			gpTextureManager->mTextureDescriptors.RestoreIslandSlot(rCrc);
			rTemplate.bGpuResident = true;
			LOG(kGraphics, kVerbose, "Island resident islandCrc={} slot={}", rCrc, rTemplate.iTextureSlot);
		}
	}

	for (auto& [rCrc, rTemplate] : rIslands)
	{
		MeshRange range = GetMeshRange(rTemplate);
		if (rTemplate.eMeshResidency == IslandMeshResidency::kAsyncPending)
		{
			ChunkRangeReloadState eRangeState = gpFileManager->mpPackChunks->mLoader.GetChunkRangeReloadState(rCrc, range.uiOffset, range.uiLength);
			if (eRangeState == ChunkRangeReloadState::kPending)
			{
				continue;
			}
			if (eRangeState == ChunkRangeReloadState::kFailed)
			{
				LOG(kGraphics, kError, "Island mesh async reload failed: crc={}", rCrc);
				gpIslands->WriteMeshIndirect(rTemplate.iTemplateArrayIndex, 0, 0, 0);
				ReleaseMeshCpuRange(rCrc, rTemplate, range);
				if (rTemplate.iTextureSlot < 0)
				{
					rTemplate.eMeshResidency = IslandMeshResidency::kNonresident;
				}
				else
				{
					rTemplate.eMeshResidency = IslandMeshResidency::kFailed;
				}
				continue;
			}
			ASSERT(eRangeState == ChunkRangeReloadState::kReady);
			rTemplate.bMeshCpuDecommitted = false;
			rTemplate.eMeshResidency = IslandMeshResidency::kCpuReady;
			if (rTemplate.iTextureSlot < 0)
			{
				ReleaseMeshCpuRange(rCrc, rTemplate, range);
				rTemplate.eMeshResidency = IslandMeshResidency::kNonresident;
				continue;
			}
		}

		if (rTemplate.eMeshResidency == IslandMeshResidency::kArenaBlocked)
		{
			if (rTemplate.iTextureSlot < 0)
			{
				continue;
			}
			if (gpIslands->muiMeshArenaCapacityGeneration == rTemplate.uiMeshArenaBlockedGeneration && !HasArenaEvictionCandidate(rCrc))
			{
				continue;
			}
			rTemplate.eMeshResidency = IslandMeshResidency::kCpuReady;
		}

		if (rTemplate.eMeshResidency != IslandMeshResidency::kCpuReady)
		{
			continue;
		}
		if (rTemplate.iTextureSlot < 0)
		{
			continue;
		}
		if (!rTemplate.bGpuResident)
		{
			continue;
		}

		while (!gpIslands->AllocateMeshRanges(range.vkIndexSize, range.vkVertexSize, rTemplate.meshIndexAllocation, rTemplate.vkMeshIndexOffset, rTemplate.meshVertexAllocation, rTemplate.vkMeshVertexOffset))
		{
			common::crc_t uiEvictCrc = 0;
			IslandTemplate* pEvictTemplate = nullptr;
			for (auto& [rCandidateCrc, rCandidate] : rIslands)
			{
				if (rCandidateCrc != rCrc && rCandidate.eMeshResidency == IslandMeshResidency::kResident && rCandidate.bGpuResident
				 && rCandidate.iReferenceCount == 0
				 && (pEvictTemplate == nullptr || rCandidate.uiLastUsedRenderFrame < pEvictTemplate->uiLastUsedRenderFrame))
				{
					uiEvictCrc = rCandidateCrc;
					pEvictTemplate = &rCandidate;
				}
			}
			if (pEvictTemplate == nullptr)
			{
				rTemplate.uiMeshArenaBlockedGeneration = gpIslands->muiMeshArenaCapacityGeneration;
				rTemplate.eMeshResidency = IslandMeshResidency::kArenaBlocked;
				gpIslands->WriteMeshIndirect(rTemplate.iTemplateArrayIndex, 0, 0, 0);
				LOG(kGraphics, kWarning, "Island mesh arena exhausted: crc={}", rCrc);
				break;
			}
			ASSERT(EvictTemplate(uiEvictCrc, *pEvictTemplate, MeshEvictionReason::kArenaExhaustion));
		}
		if (rTemplate.eMeshResidency != IslandMeshResidency::kCpuReady)
		{
			continue;
		}

		ASSERT(rTemplate.pfMeshPositions != nullptr);
		ASSERT(rTemplate.puiMeshIndices != nullptr);
		ASSERT(rTemplate.iMeshVertexCount > 0);
		ASSERT(rTemplate.iMeshIndexCount > 0);
		{
			// Heap: UploadMesh creates transient VMA staging allocations in the RenderGlobal residency sweep.
			ScopedSuppressAllocationTracking suppress;
			gpIslands->UploadMesh(rTemplate.vkMeshIndexOffset, std::span<const std::byte>(reinterpret_cast<const std::byte*>(rTemplate.puiMeshIndices), static_cast<size_t>(range.vkIndexSize)), rTemplate.vkMeshVertexOffset, std::span<const std::byte>(reinterpret_cast<const std::byte*>(rTemplate.pfMeshPositions), static_cast<size_t>(range.vkVertexSize)));
		}
		ReleaseMeshCpuRange(rCrc, rTemplate, range);
		gpIslands->WriteMeshIndirect(rTemplate.iTemplateArrayIndex, rTemplate.vkMeshIndexOffset, rTemplate.vkMeshVertexOffset, static_cast<uint32_t>(rTemplate.iMeshIndexCount));
		rTemplate.uiMeshArenaBlockedGeneration = 0;
		rTemplate.eMeshResidency = IslandMeshResidency::kResident;
		LOG(kGraphics, kDebug, "Restored island mesh: crc={} vertices={} indices={}", rCrc, rTemplate.iMeshVertexCount, rTemplate.iMeshIndexCount);
	}
}

void IslandTerrainResidency::ReleaseGpuResources()
{
	for (auto& [rCrc, rTemplate] : gpIslandTerrain->mIslands)
	{
		rTemplate.meshIndexAllocation = VK_NULL_HANDLE;
		rTemplate.meshVertexAllocation = VK_NULL_HANDLE;
		rTemplate.vkMeshIndexOffset = 0;
		rTemplate.vkMeshVertexOffset = 0;
		if (rTemplate.eMeshResidency == IslandMeshResidency::kResident)
		{
			rTemplate.eMeshResidency = IslandMeshResidency::kNonresident;
		}
		else if (rTemplate.eMeshResidency == IslandMeshResidency::kArenaBlocked)
		{
			rTemplate.eMeshResidency = IslandMeshResidency::kCpuReady;
		}
		// elevationTexture is template-owned (no mTextureMap entry), so TextureManager's wholesale
		// destroy doesn't touch it — release here. On device-loss recovery the
		// TextureManager ctor's ResetTextureSlots forces iTextureSlot < 0 for every template, so the
		// next AcquireTextureSlot re-Creates elevationTexture via the first-mint path.
		rTemplate.elevationTexture.Destroy();
		rTemplate.bGpuResident = false;
	}
}

void IslandTerrainResidency::ResetTextureSlots()
{
	for (auto& [rCrc, rTemplate] : gpIslandTerrain->mIslands)
	{
		if (rTemplate.eMeshResidency == IslandMeshResidency::kAsyncPending)
		{
			MeshRange range = GetMeshRange(rTemplate);
			if (gpFileManager->mpPackChunks->mLoader.GetChunkRangeReloadState(rCrc, range.uiOffset, range.uiLength) == ChunkRangeReloadState::kFailed)
			{
				// Teardown can run after the File-owned async request failed but before RestorationSweep
				// promoted this template state. Consume that destroyed-lifecycle failure so re-mint can retry.
				ReleaseMeshCpuRange(rCrc, rTemplate, range);
				rTemplate.eMeshResidency = IslandMeshResidency::kNonresident;
			}
		}
		else if (rTemplate.eMeshResidency == IslandMeshResidency::kFailed)
		{
			rTemplate.eMeshResidency = IslandMeshResidency::kNonresident;
		}
		rTemplate.iTextureSlot = -1;
		rTemplate.bGpuResident = false;
		rTemplate.iReferenceCount = 0;
		rTemplate.uiLastUsedRenderFrame = 0;
	}
	miNextTextureSlot = 1;
	// Device-loss resets the high-water mark to 1; stale recycled indices would collide with the
	// freshly re-minted slots against the reset descriptor arrays.
	mFreeTextureSlots.clear();
}

} // namespace engine

#endif
