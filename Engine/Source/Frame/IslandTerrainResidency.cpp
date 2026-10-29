#if defined(BT_CLIENT)

#include "IslandTerrainResidency.h"

#include "File/PackChunks.h"
#include "Graphics/Managers/TextureManager.h"
#include "Graphics/Islands.h"

namespace engine
{

struct MeshRange
{
	int64_t iOffset = 0;
	int64_t iLength = 0;
	int64_t iIndexSize = 0;
	int64_t iVertexSize = 0;
};

static MeshRange GetMeshRange(const IslandTemplate& rTemplate)
{
	MeshRange range {};
	range.iOffset = static_cast<int64_t>(rTemplate.iHeightmapWidth) * static_cast<int64_t>(rTemplate.iHeightmapHeight) * static_cast<int64_t>(sizeof(uint16_t));
	range.iVertexSize = static_cast<int64_t>(rTemplate.iMeshVertexCount) * 2 * static_cast<int64_t>(sizeof(float));
	range.iIndexSize = static_cast<int64_t>(rTemplate.iMeshIndexCount) * static_cast<int64_t>(sizeof(uint32_t));
	range.iLength = range.iVertexSize + range.iIndexSize;
	return range;
}

static void ReleaseMeshCpuRange(common::crc_t uiIslandCrc, IslandTemplate& rTemplate, const MeshRange& rRange)
{
	gpFileManager->mpPackChunks->DecommitChunkRange(uiIslandCrc, rRange.iOffset, rRange.iLength);
	rTemplate.bMeshCpuDecommitted = true;
	gpFileManager->mpPackChunks->mLoader.ResetChunkRangeReloadState(uiIslandCrc, rRange.iOffset, rRange.iLength);
}

// A distinct transfer queue uploads elevation on the upload thread; a transfer queue aliasing graphics records it at restoration.
static bool IsElevationUploadedByTransferQueue()
{
	return gpDeviceManager->mTransferVkQueue != gpDeviceManager->mGraphicsVkQueue;
}

static bool IsTextureRestorationPending(common::crc_t uiIslandCrc, const IslandTemplate& rTemplate)
{
	if (rTemplate.bGpuResident || rTemplate.iTextureSlot < 0)
	{
		return false;
	}
	if (IsElevationUploadedByTransferQueue() && rTemplate.elevationTexture.mVkImage == VK_NULL_HANDLE)
	{
		return false;
	}
	const LazyChunk& rLazyChunk = gpFileManager->mpPackChunks->mLazyChunkMap.at(uiIslandCrc);
	common::crc_t pResidencyCrcs[4] =
	{
		rLazyChunk.header.islandHeader.colorsCrc,
		rLazyChunk.header.islandHeader.normalsCrc,
		rLazyChunk.header.islandHeader.ambientOcclusionCrc,
		rLazyChunk.header.islandHeader.masksCrc,
	};
	return std::ranges::all_of(pResidencyCrcs, [](const common::crc_t& rTextureCrc)
	{
		return gpFileManager->mpPackChunks->IsChunkReady(rTextureCrc);
	});
}

IslandTerrainResidency::IslandTerrainResidency()
: common::Singleton<IslandTerrainResidency>(gpIslandTerrainResidency, this)
{
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

	// Elevation: an R16_SFLOAT raw copy of the resident heightmap into the template-owned elevationTexture, which
	// borrows the placeholder view until its image is adopted or recorded at restoration. Boot ordering invariant:
	// WaitForElevationMaps makes the heightmap resident before any AcquireTextureSlot.
	ASSERT(rTemplate.puiHeightmapHalf != nullptr);
	rTemplate.elevationTexture.InitDeferred(TextureInfo
	{
		.name = name,
		.vkFormat = shaders::kVkFormatElevation,
		.vkExtent3D = {static_cast<uint32_t>(rTemplate.iHeightmapWidth), static_cast<uint32_t>(rTemplate.iHeightmapHeight), 1ui32},
		.iMipLevels = 1,
		.iArrayLayers = 1,
		.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
		.vkImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
		.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
		.eTextureLayout = TextureLayout::kShaderReadOnly,
	}, gpTextureManager->mIslandPlaceholderElevation.mVkImageView);
	if (IsElevationUploadedByTransferQueue())
	{
		gpTextureUploadManager->RequestUpload(uiIslandCrc, LoadPriority::kRealtime);
	}

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
		gpFileManager->mpPackChunks->mLoader.RequestChunkRangeReload(uiIslandCrc, range.iOffset, range.iLength, LoadPriority::kRealtime);
		rTemplate.eMeshResidency = IslandMeshResidency::kAsyncPending;
	}

	// Hot path still starts a mesh reload when a device recreation or earlier full teardown made
	// the mesh nonresident; texture residency itself remains unchanged.
	if (rTemplate.iTextureSlot >= 0 && rTemplate.bGpuResident)
	{
		return rTemplate.iTextureSlot;
	}

	// Color / Normals / AO / Masks ship as standalone lazy-texture chunks. Elevation lives on the template
	// (elevationTexture) and is uploaded from the island chunk's resident heightmap.
	common::crc_t pTextureCrcs[4] =
	{
		rLazyChunk.header.islandHeader.colorsCrc,
		rLazyChunk.header.islandHeader.normalsCrc,
		rLazyChunk.header.islandHeader.ambientOcclusionCrc,
		rLazyChunk.header.islandHeader.masksCrc,
	};

	if (rTemplate.iTextureSlot < 0)
	{
		return FirstMintTextureSlot(uiIslandCrc, rTemplate, pTextureCrcs, rLazyChunk.header.pcPath);
	}

	// A freshly minted slot stays in slot-0 fallback while chunks load, until RestorationSweep patches it.
	// Eviction and device-loss ResetTextureSlots set iTextureSlot negative, so the next use re-enters
	// first mint and re-uploads elevationTexture.
	gpFileManager->mpPackChunks->mLoader.RequestChunkLoad(pTextureCrcs, LoadPriority::kRealtime);
	LOG(kLoading, kVerbose, "Re-acquire islandCrc={} slot={}, requesting chunk loads", uiIslandCrc, rTemplate.iTextureSlot);
	return rTemplate.iTextureSlot;
}

bool IslandTerrainResidency::AnyEvictionPending() const
{
	if (gpGraphics == nullptr || gpTextureManager == nullptr)
	{
		return false;
	}
	return std::ranges::any_of(gpIslandTerrain->mIslands, [this](const std::pair<const common::crc_t, IslandTemplate>& rIsland)
	{
		return IsEvictionPending(rIsland.second);
	});
}

bool IslandTerrainResidency::AnyRestorationPending() const
{
	if (gpGraphics == nullptr || gpTextureManager == nullptr)
	{
		return false;
	}
	return std::ranges::any_of(gpIslandTerrain->mIslands, [this](const std::pair<const common::crc_t, IslandTemplate>& rIsland)
	{
		return IsRestorationPending(rIsland.first, rIsland.second);
	});
}

bool IslandTerrainResidency::IsEvictionPending(const IslandTemplate& rTemplate) const
{
	return rTemplate.iTextureSlot != 0 && rTemplate.bGpuResident && rTemplate.iReferenceCount == 0
	    && (gpGraphics->miFrameCounter - rTemplate.iLastUsedRenderFrame) > kiGraceRenderFrames;
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
			return gpFileManager->mpPackChunks->mLoader.GetChunkRangeReloadState(uiIslandCrc, range.iOffset, range.iLength) != ChunkRangeReloadState::kPending;
		case IslandMeshResidency::kCpuReady:
			return rTemplate.bGpuResident;
		case IslandMeshResidency::kArenaBlocked:
			return rTemplate.bGpuResident && (gpIslands->miMeshArenaCapacityGeneration != rTemplate.iMeshArenaBlockedGeneration || (HasArenaEvictionCandidate(uiIslandCrc) && !gpIslands->HasRetiredMeshRanges()));
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
	common::crc_t pEvictCrcs[4] =
	{
		rLazyChunk.header.islandHeader.colorsCrc,
		rLazyChunk.header.islandHeader.normalsCrc,
		rLazyChunk.header.islandHeader.ambientOcclusionCrc,
		rLazyChunk.header.islandHeader.masksCrc,
	};

	LOG(kGraphics, kVerbose, "Evicting islandCrc={} slot={} (refCount=0, framesSinceUse={})", uiIslandCrc, rTemplate.iTextureSlot, gpGraphics->miFrameCounter - rTemplate.iLastUsedRenderFrame);

	int64_t iSlot = rTemplate.iTextureSlot;
	// Redirect every live descriptor to placeholders and queue their per-framebuffer publication before
	// retiring any image view. Retired images and mesh ranges outlive every pending frame and stale set,
	// so the slot is reusable at once: each framebuffer republishes it from live state.
	gpTextureManager->mTextureDescriptors.EvictIslandSlot(iSlot, uiIslandCrc, pEvictCrcs);

	for (common::crc_t uiTextureCrc : pEvictCrcs)
	{
		gpTextureManager->RetireImage(gpTextureManager->mTextureMap.at(uiTextureCrc));
	}
	gpTextureManager->RetireImage(rTemplate.elevationTexture);

	MeshRange range = GetMeshRange(rTemplate);
	switch (rTemplate.eMeshResidency)
	{
		case IslandMeshResidency::kResident:
			// UpdateActiveIslands zeroes each framebuffer's indirect range before that framebuffer's next submission.
			gpIslands->RetireMeshRanges(rTemplate.meshIndexAllocation, rTemplate.meshVertexAllocation);
			rTemplate.meshIndexAllocation = VK_NULL_HANDLE;
			rTemplate.meshVertexAllocation = VK_NULL_HANDLE;
			rTemplate.iMeshIndexOffset = 0;
			rTemplate.iMeshVertexOffset = 0;
			rTemplate.fMeshMinX = 0.0f;
			rTemplate.fMeshMinY = 0.0f;
			rTemplate.fMeshMaxX = 0.0f;
			rTemplate.fMeshMaxY = 0.0f;
			rTemplate.iMeshArenaBlockedGeneration = 0;
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

	gpFileManager->mpPackChunks->ResetTextureChunkStates(pEvictCrcs);
	rTemplate.bGpuResident = false;

	LOG(kLoading, kVerbose, "Reset chunk states for evicted islandCrc={} evictCrcs=[{},{},{},{}]", uiIslandCrc, pEvictCrcs[0], pEvictCrcs[1], pEvictCrcs[2], pEvictCrcs[3]);
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
			if (!IsElevationUploadedByTransferQueue())
			{
				// Heap: the recorded create allocates its staging buffer inside RenderGlobal.
				ScopedSuppressAllocationTracking suppress;
				gpTextureManager->RetireStagingBuffer(rTemplate.elevationTexture.RecordCreate(gpTextureManager->EnsureAcquireCommandBufferBegun(), rTemplate.elevationTexture.mInfo, [&rTemplate](std::span<std::byte> data, int64_t iPosition)
				{
					std::memcpy(data.data(), reinterpret_cast<const std::byte*>(rTemplate.puiHeightmapHalf) + iPosition, data.size_bytes());
				}));
			}
			// The elevation copy or acquire sits in this or an earlier frame's acquire command buffer, so the publication
			// queued here reaches only later submissions. islandCrc is the binding key (the template-owned
			// elevationTexture has no chunk CRC).
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
			ChunkRangeReloadState eRangeState = gpFileManager->mpPackChunks->mLoader.GetChunkRangeReloadState(rCrc, range.iOffset, range.iLength);
			if (eRangeState == ChunkRangeReloadState::kPending)
			{
				continue;
			}
			if (eRangeState == ChunkRangeReloadState::kFailed)
			{
				LOG(kGraphics, kError, "Island mesh async reload failed: crc={}", rCrc);
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
			if (gpIslands->miMeshArenaCapacityGeneration == rTemplate.iMeshArenaBlockedGeneration && (!HasArenaEvictionCandidate(rCrc) || gpIslands->HasRetiredMeshRanges()))
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

		if (!gpIslands->AllocateMeshRanges(range.iIndexSize, range.iVertexSize, rTemplate.meshIndexAllocation, rTemplate.iMeshIndexOffset, rTemplate.meshVertexAllocation, rTemplate.iMeshVertexOffset))
		{
			// Evicted ranges return to the arena only once retirement frees them, which bumps the capacity generation
			// this template retries on. While ranges are retiring, wait for them instead of evicting another template.
			rTemplate.iMeshArenaBlockedGeneration = gpIslands->miMeshArenaCapacityGeneration;
			rTemplate.eMeshResidency = IslandMeshResidency::kArenaBlocked;
			if (gpIslands->HasRetiredMeshRanges())
			{
				continue;
			}

			common::crc_t uiEvictCrc = 0;
			IslandTemplate* pEvictTemplate = nullptr;
			for (auto& [rCandidateCrc, rCandidate] : rIslands)
			{
				if (rCandidateCrc != rCrc && rCandidate.eMeshResidency == IslandMeshResidency::kResident && rCandidate.bGpuResident
				 && rCandidate.iReferenceCount == 0
				 && (pEvictTemplate == nullptr || static_cast<uint64_t>(rCandidate.iLastUsedRenderFrame) < static_cast<uint64_t>(pEvictTemplate->iLastUsedRenderFrame)))
				{
					uiEvictCrc = rCandidateCrc;
					pEvictTemplate = &rCandidate;
				}
			}
			if (pEvictTemplate == nullptr)
			{
				LOG(kGraphics, kError, "Island mesh arena exhausted: crc={}. Increase kiIslandMeshArenaBytes in Islands.h", rCrc);
				DEBUG_BREAK();
				continue;
			}
			ASSERT(EvictTemplate(uiEvictCrc, *pEvictTemplate, MeshEvictionReason::kArenaExhaustion));
			continue;
		}

		ASSERT(rTemplate.pfMeshPositions != nullptr);
		ASSERT(rTemplate.puiMeshIndices != nullptr);
		ASSERT(rTemplate.iMeshVertexCount > 0);
		ASSERT(rTemplate.iMeshIndexCount > 0);
		rTemplate.fMeshMinX = rTemplate.fMeshMaxX = rTemplate.pfMeshPositions[0];
		rTemplate.fMeshMinY = rTemplate.fMeshMaxY = rTemplate.pfMeshPositions[1];
		for (int64_t i = 1; i < rTemplate.iMeshVertexCount; ++i)
		{
			rTemplate.fMeshMinX = std::min(rTemplate.fMeshMinX, rTemplate.pfMeshPositions[2 * i]);
			rTemplate.fMeshMinY = std::min(rTemplate.fMeshMinY, rTemplate.pfMeshPositions[2 * i + 1]);
			rTemplate.fMeshMaxX = std::max(rTemplate.fMeshMaxX, rTemplate.pfMeshPositions[2 * i]);
			rTemplate.fMeshMaxY = std::max(rTemplate.fMeshMaxY, rTemplate.pfMeshPositions[2 * i + 1]);
		}
		{
			// Heap: the recorded upload allocates its staging buffer inside RenderGlobal's residency sweep.
			ScopedSuppressAllocationTracking suppress;
			gpTextureManager->RetireStagingBuffer(gpIslands->UploadMesh(gpTextureManager->EnsureAcquireCommandBufferBegun(), rTemplate.iMeshIndexOffset, std::span<const std::byte>(reinterpret_cast<const std::byte*>(rTemplate.puiMeshIndices), static_cast<size_t>(range.iIndexSize)), rTemplate.iMeshVertexOffset, std::span<const std::byte>(reinterpret_cast<const std::byte*>(rTemplate.pfMeshPositions), static_cast<size_t>(range.iVertexSize))));
		}
		// The staging buffer holds the copied mesh, so the CPU range is released now; UpdateActiveIslands publishes the
		// arena range from the next frame, after the acquire command buffer holding the copy was submitted.
		ReleaseMeshCpuRange(rCrc, rTemplate, range);
		rTemplate.iMeshArenaBlockedGeneration = 0;
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
		rTemplate.iMeshIndexOffset = 0;
		rTemplate.iMeshVertexOffset = 0;
		rTemplate.fMeshMinX = 0.0f;
		rTemplate.fMeshMinY = 0.0f;
		rTemplate.fMeshMaxX = 0.0f;
		rTemplate.fMeshMaxY = 0.0f;
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
		// next AcquireTextureSlot re-uploads elevationTexture via the first-mint path.
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
			if (gpFileManager->mpPackChunks->mLoader.GetChunkRangeReloadState(rCrc, range.iOffset, range.iLength) == ChunkRangeReloadState::kFailed)
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
		rTemplate.iLastUsedRenderFrame = 0;
	}
	miNextTextureSlot = 1;
	// Device-loss resets the high-water mark to 1; stale recycled indices would collide with the
	// freshly re-minted slots against the reset descriptor arrays.
	mFreeTextureSlots.clear();
}

} // namespace engine

#endif
