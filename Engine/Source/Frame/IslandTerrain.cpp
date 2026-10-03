#include "IslandTerrain.h"

#include "Frame/FrameStaticData.h"
#include "Frame/IslandChainPlacement.h"

#include "Game.h"

namespace engine
{

// Footprint-AREA thresholds (engine m^2) for the IslandChainPlacement size buckets. The multi-island
// export tiles a ~400 m master into 1x1 (400x400 Huge), 2x1/3x1 strips (Large), mid tiles (Medium), and
// 4x4 (100x100 Small). Area separates them where the larger dimension cannot — a 2x1 strip (200x400) and
// the 1x1 master (400x400) share the same long edge.
constexpr float kfHugeIslandAreaMeters = 120'000.0f;   // 1x1     = 400x400 = 160k
constexpr float kfLargeIslandAreaMeters = 48'000.0f;   // 2x1/3x1 strips    = 53k-80k
constexpr float kfMediumIslandAreaMeters = 16'000.0f;  // mid tiles; 4x4 (10k) falls below -> Small

IslandTerrain::IslandTerrain()
{
	ASSERT(gpIslandTerrain == nullptr);

	gpIslandTerrain = this;

	const std::unordered_map<common::crc_t, LazyChunk>& rChunkMap = gpFileManager->GetLazyChunkMap();
	for (const auto& [rCrc, rLazyChunk] : rChunkMap)
	{
		if (!(rLazyChunk.header.flags & common::ChunkFlags::kIsland))
		{
			continue;
		}

		IslandTemplate& rTemplate = mIslands.try_emplace(rCrc).first->second;
		rTemplate.islandCrc = rCrc;
		rTemplate.fWorldFootprintXMeters = rLazyChunk.header.islandHeader.fWorldFootprintXMeters;
		rTemplate.fWorldFootprintYMeters = rLazyChunk.header.islandHeader.fWorldFootprintYMeters;
		rTemplate.fWorldElevationMeters = rLazyChunk.header.islandHeader.fWorldElevationMeters;
		ASSERT(rTemplate.fWorldFootprintXMeters > 0.0f);
		ASSERT(rTemplate.fWorldFootprintYMeters > 0.0f);
		rTemplate.fQuadFootprintX = rTemplate.fWorldFootprintXMeters;
		rTemplate.fQuadFootprintY = rTemplate.fWorldFootprintYMeters;
	}

	// Stable, deterministic iteration order for slot assignment (Phase 3).
	mIslandCrcsSorted.reserve(mIslands.size());
	for (const auto& [rCrc, rTemplate] : mIslands)
	{
		mIslandCrcsSorted.push_back(rCrc);
	}
	std::sort(mIslandCrcsSorted.begin(), mIslandCrcsSorted.end());

	for (int64_t i = 0; i < std::ssize(mIslandCrcsSorted); ++i)
	{
		mIslands.at(mIslandCrcsSorted.at(i)).iTemplateArrayIndex = i;
	}

	mIslandCrcsByArea = mIslandCrcsSorted;
	std::sort(mIslandCrcsByArea.begin(), mIslandCrcsByArea.end(), [this](common::crc_t crcA, common::crc_t crcB)
		{
			const IslandTemplate& rTemplateA = mIslands.at(crcA);
			const IslandTemplate& rTemplateB = mIslands.at(crcB);
			float fAreaA = rTemplateA.fWorldFootprintXMeters * rTemplateA.fWorldFootprintYMeters;
			float fAreaB = rTemplateB.fWorldFootprintXMeters * rTemplateB.fWorldFootprintYMeters;
			if (fAreaA != fAreaB)
			{
				return fAreaA > fAreaB;
			}
			return crcA < crcB;
		});

	for (common::crc_t islandCrc : mIslandCrcsSorted)
	{
		const IslandTemplate& rTemplate = mIslands.at(islandCrc);
		float fAreaMeters = rTemplate.fWorldFootprintXMeters * rTemplate.fWorldFootprintYMeters;

		if (fAreaMeters >= kfHugeIslandAreaMeters)
		{
			mHugeCrcs.push_back(islandCrc);
		}
		else if (fAreaMeters >= kfLargeIslandAreaMeters)
		{
			mLargeCrcs.push_back(islandCrc);
		}
		else if (fAreaMeters >= kfMediumIslandAreaMeters)
		{
			mMediumCrcs.push_back(islandCrc);
		}
		else
		{
			mSmallCrcs.push_back(islandCrc);
		}
	}

#if defined(BT_CLIENT)
	// Color/normals/AO/masks chunk CRCs must be unique across templates; DataPacker derives them from each
	// island's separate path, and boot asserts uniqueness. Eviction uses no refcounts: TextureDescriptors
	// unregisters all five slot channels before IslandTerrain frees channel textures. Sharing a channel
	// leaves another template sampling freed storage after its generation-verifier record is removed.
	{
		std::vector<common::crc_t> channelCrcs;
		channelCrcs.reserve(mIslandCrcsSorted.size() * 4);
		for (common::crc_t islandCrc : mIslandCrcsSorted)
		{
			const common::IslandHeader& rIslandHeader = rChunkMap.at(islandCrc).header.islandHeader;
			channelCrcs.push_back(rIslandHeader.colorsCrc);
			channelCrcs.push_back(rIslandHeader.normalsCrc);
			channelCrcs.push_back(rIslandHeader.ambientOcclusionCrc);
			channelCrcs.push_back(rIslandHeader.masksCrc);
		}
		std::sort(channelCrcs.begin(), channelCrcs.end());
		ASSERT(std::adjacent_find(channelCrcs.begin(), channelCrcs.end()) == channelCrcs.end());
	}
#endif

	// Downstream (IslandChainPlacement, TextureManager slot-0 anchor) requires at least one island.
	ASSERT(!mIslandCrcsSorted.empty());


	gpFileManager->RequestChunkLoad(mIslandCrcsSorted, LoadPriority::kRealtime);
}

IslandTerrain::~IslandTerrain()
{
	if (gpIslandTerrain == this)
	{
		gpIslandTerrain = nullptr;
	}
}

#if defined(BT_SERVER)
void IslandTerrain::WaitForElevationMaps(float fNavigationThreshold, float fNavigationClearanceMeters)
#else
void IslandTerrain::WaitForElevationMaps()
#endif
{
	gpFileManager->WaitForChunks(mIslandCrcsSorted);

	const std::unordered_map<common::crc_t, LazyChunk>& rChunkMap = gpFileManager->GetLazyChunkMap();
	for (auto& [rCrc, rTemplate] : mIslands)
	{
		const LazyChunk& rLazyChunk = rChunkMap.at(rCrc);
		const common::IslandHeader& rIslandHeader = rLazyChunk.header.islandHeader;

		// Chunk payload layout (set by ExportIsland::Export): [heightmap R16 halfs][float2 mesh positions][uint32 mesh indices][float2 valid-area hull verts].
		// Validate every count from the pack header before forming payload pointers or reclaiming a range.
		// iSize excludes the lazy-pool's alignment pad, so it must describe the layout exactly and fit the
		// actual resident extent.
		if (common::IsCompressed(rLazyChunk.header.flags) || rLazyChunk.pData == nullptr || rLazyChunk.iDataSize <= 0
		 || rLazyChunk.header.iSize <= 0 || rLazyChunk.header.iSize > rLazyChunk.iDataSize || rIslandHeader.iHeightmapWidth <= 0
		 || rIslandHeader.iHeightmapHeight <= 0 || rIslandHeader.iMeshVertexCount <= 0 || rIslandHeader.iMeshIndexCount <= 0
		 || rIslandHeader.iValidAreaVertexCount < 0)
		{
			throw std::ios_base::failure("IslandTerrain::WaitForElevationMaps");
		}

		int64_t iBytesRemaining = rLazyChunk.header.iSize;
		if (rIslandHeader.iHeightmapWidth > iBytesRemaining / static_cast<int64_t>(sizeof(uint16_t)) / rIslandHeader.iHeightmapHeight)
		{
			throw std::ios_base::failure("IslandTerrain::WaitForElevationMaps");
		}
		int64_t iHeightmapBytes = static_cast<int64_t>(rIslandHeader.iHeightmapWidth) * rIslandHeader.iHeightmapHeight * static_cast<int64_t>(sizeof(uint16_t));
		iBytesRemaining -= iHeightmapBytes;

		auto ConsumeSection = [&iBytesRemaining](int64_t iElementCount, int64_t iElementBytes)
		{
			if (iElementCount > iBytesRemaining / iElementBytes)
			{
				throw std::ios_base::failure("IslandTerrain::WaitForElevationMaps");
			}
			int64_t iSectionBytes = iElementCount * iElementBytes;
			iBytesRemaining -= iSectionBytes;
			return iSectionBytes;
		};

		[[maybe_unused]] int64_t iMeshPositionBytes = ConsumeSection(rIslandHeader.iMeshVertexCount, 2 * static_cast<int64_t>(sizeof(float)));
		ConsumeSection(rIslandHeader.iMeshIndexCount, static_cast<int64_t>(sizeof(uint32_t)));
		int64_t iValidAreaBytes = ConsumeSection(rIslandHeader.iValidAreaVertexCount, static_cast<int64_t>(sizeof(XMFLOAT2)));
		if (iBytesRemaining != 0)
		{
			throw std::ios_base::failure("IslandTerrain::WaitForElevationMaps");
		}
		int64_t iMeshBytes = rLazyChunk.header.iSize - iHeightmapBytes - iValidAreaBytes;

		rTemplate.puiHeightmapHalf = reinterpret_cast<const uint16_t*>(rLazyChunk.pData);
		rTemplate.iHeightmapWidth = rIslandHeader.iHeightmapWidth;
		rTemplate.iHeightmapHeight = rIslandHeader.iHeightmapHeight;
		rTemplate.iMeshVertexCount = rIslandHeader.iMeshVertexCount;
		rTemplate.iMeshIndexCount = rIslandHeader.iMeshIndexCount;
		rTemplate.iValidAreaVertexCount = rIslandHeader.iValidAreaVertexCount;
		const std::byte* pAfterHeightmap = rLazyChunk.pData + iHeightmapBytes;
		// Not validated here: DataPacker's VerifyHullCcwConvex (ExportIsland.cpp) already fails the bake on a
		// non-CCW or non-convex hull, which is what ConvexHullsOverlap's SAT requires of these vertices.
		rTemplate.pf2ValidAreaVertices = reinterpret_cast<const XMFLOAT2*>(pAfterHeightmap + iMeshBytes);

#if defined(BT_CLIENT)
		// Mesh CPU pointers are client-only. The lazy-pool slice is reclaimed immediately and
		// asynchronously restored only when this template gains a render slot.
		rTemplate.pfMeshPositions = reinterpret_cast<const float*>(pAfterHeightmap);
		rTemplate.puiMeshIndices = reinterpret_cast<const uint32_t*>(pAfterHeightmap + iMeshPositionBytes);
		gpFileManager->DecommitChunkRange(rCrc, static_cast<uint64_t>(iHeightmapBytes), static_cast<uint64_t>(iMeshBytes));
		rTemplate.bMeshCpuDecommitted = true;
#endif

#if defined(BT_SERVER)
		// The server never reads the mesh CPU slice (no GPU upload, no device loss), so reclaim it immediately after
		// load: decommit the [positions][indices] sub-range of the kIsland chunk. Heightmap (before, offset 0) and hull
		// (after) stay resident — the server reads the heightmap for NavContour below and the hull for placement/nav.
		gpFileManager->DecommitChunkRange(rCrc, static_cast<uint64_t>(iHeightmapBytes), static_cast<uint64_t>(iMeshBytes));
#endif
	}

#if defined(BT_SERVER)
	// Phase 4: build NavContour for every template so multi-template cells produce correct nav data.
	LOG(kLoading, kInfo, "Building NavContour for {} island templates", mIslands.size());
	ScopedBootTimer scopedTimer(kBootTimerIslands);
	for (auto& [rCrc, rTemplate] : mIslands)
	{
		if (rTemplate.puiHeightmapHalf != nullptr)
		{
			// BuildNavContour consumes full-precision floats; dequantize the R16 heightmap into a transient
			// boot buffer (one template at a time, freed each iteration).
			int64_t iHeightmapTexels = static_cast<int64_t>(rTemplate.iHeightmapWidth) * static_cast<int64_t>(rTemplate.iHeightmapHeight);
			std::vector<float> heightmapFloats(static_cast<size_t>(iHeightmapTexels));
			DirectX::PackedVector::XMConvertHalfToFloatStream(heightmapFloats.data(), sizeof(float), rTemplate.puiHeightmapHalf, sizeof(uint16_t), static_cast<size_t>(iHeightmapTexels));
			BuildNavContour(rTemplate.navContour, heightmapFloats, rTemplate.iHeightmapWidth, rTemplate.iHeightmapHeight, fNavigationThreshold, fNavigationClearanceMeters, rTemplate.fQuadFootprintX, rTemplate.fQuadFootprintY);
		}
	}
#endif
}

// Re-home a cell-local position that has stepped outside its cell onto the neighbouring cell that contains
// it, rewriting both the coord and the local XY. GlobalElevation's callers hand in a position local to the
// basis coord, and GlobalNormal's taps can cross the edge. Returns false when the neighbour would leave the
// signed-int32 identity range, so an edge cell reports sea floor instead of wrapping to the far side.
static bool ResolveLocalPosition(GridCoord& rCoord, XMFLOAT4A& rf4Local)
{

	int32_t iStepX = static_cast<int32_t>(std::floor((rf4Local.x - kfBaseAreaMinimumX) / kfCellWidth));
	int32_t iStepY = static_cast<int32_t>(std::floor((rf4Local.y - kfBaseAreaMinimumY) / kfCellHeight));
	if (iStepX == 0 && iStepY == 0)
	{
		return true;
	}

	if (!TryAddGridCoordinate(rCoord, iStepX, iStepY, rCoord))
	{
		return false;
	}

	rf4Local.x -= static_cast<float>(iStepX) * kfCellWidth;
	rf4Local.y -= static_cast<float>(iStepY) * kfCellHeight;
	return true;
}

template <bool MULTIPLY_UV, bool HOISTED_HEIGHTMAP_MAXIMUM>
static bool SamplePlacementHeightmap(float fDeltaX, float fDeltaY, float fCosine, float fSine, float fFootprintX, float fFootprintY, float fHalfX, float fHalfY, const uint16_t* puiHeightmapHalf, int64_t iHeightmapWidth, int64_t iHeightmapHeight, float fInverseFootprintX, float fInverseFootprintY, float fHeightmapMaximumU, float fHeightmapMaximumV, float& rfSample)
{
	float fLocalX = fDeltaX * fCosine - fDeltaY * fSine;
	float fLocalY = fDeltaX * fSine + fDeltaY * fCosine;

	if constexpr (MULTIPLY_UV)
	{
		if (std::abs(fLocalX) > fHalfX || std::abs(fLocalY) > fHalfY)
		{
			return false;
		}
	}
	else if (std::abs(fLocalX) > 0.5f * fFootprintX || std::abs(fLocalY) > 0.5f * fFootprintY)
	{
		return false;
	}

	// UV from local frame; V axis is world-Y inverted.
	float fU = 0.0f;
	float fV = 0.0f;
	if constexpr (MULTIPLY_UV)
	{
		fU = fLocalX * fInverseFootprintX + 0.5f;
		fV = 0.5f - fLocalY * fInverseFootprintY;
	}
	else
	{
		fU = fLocalX / fFootprintX + 0.5f;
		fV = 0.5f - fLocalY / fFootprintY;
	}

	int64_t iX = 0;
	int64_t iY = 0;
	if constexpr (HOISTED_HEIGHTMAP_MAXIMUM)
	{
		iX = static_cast<int64_t>(fU * fHeightmapMaximumU);
		iY = static_cast<int64_t>(fV * fHeightmapMaximumV);
	}
	else
	{
		iX = static_cast<int64_t>(fU * static_cast<float>(iHeightmapWidth - 1));
		iY = static_cast<int64_t>(fV * static_cast<float>(iHeightmapHeight - 1));
	}
	iX = std::clamp(iX, static_cast<int64_t>(0), static_cast<int64_t>(iHeightmapWidth - 1));
	iY = std::clamp(iY, static_cast<int64_t>(0), static_cast<int64_t>(iHeightmapHeight - 1));

	rfSample = DirectX::PackedVector::XMConvertHalfToFloat(puiHeightmapHalf[iY * iHeightmapWidth + iX]);
	return true;
}

template <typename ELEVATION_CALLABLE>
static XMVECTOR NormalFromElevation(FXMVECTOR vecPosition, float fDistance, ELEVATION_CALLABLE&& rElevation)
{
	auto vecTopLeft = XMVectorAdd(vecPosition, XMVectorSet(-fDistance, fDistance, 0.0f, 0.0f));
	vecTopLeft = XMVectorSetZ(vecTopLeft, rElevation(vecTopLeft));
	auto vecTopRight = XMVectorAdd(vecPosition, XMVectorSet(fDistance, fDistance, 0.0f, 0.0f));
	vecTopRight = XMVectorSetZ(vecTopRight, rElevation(vecTopRight));
	auto vecBottomLeft = XMVectorAdd(vecPosition, XMVectorSet(-fDistance, -fDistance, 0.0f, 0.0f));
	vecBottomLeft = XMVectorSetZ(vecBottomLeft, rElevation(vecBottomLeft));
	auto vecBottomRight = XMVectorAdd(vecPosition, XMVectorSet(fDistance, -fDistance, 0.0f, 0.0f));
	vecBottomRight = XMVectorSetZ(vecBottomRight, rElevation(vecBottomRight));

	return XMVector3Normalize(XMVector3Cross(XMVectorSubtract(vecTopRight, vecBottomLeft), XMVectorSubtract(vecTopLeft, vecBottomRight)));
}

// CellElevation shares MAX elevation over a resolved cell between GlobalElevation and GlobalNormal; the
// latter reuses cell resolution across four taps. Hull-based placement permits overlapping footprint
// rectangles, so the highest terrain wins, matching the GPU prepass and remaining order-independent and
// deterministic. BuildRenderPlacementCache stores inverse rotation, footprint, and heightmap data in
// island-parallel queries, avoiding per-island hashes and trig. Placement edits clear the cache, which
// can lag RunFrameTick's rebuild by one tick; the server never builds it. Until available, reconstruct
// each query from placement/template with inline trig and one mIslands.at lookup.
static float CellElevation(const IslandTerrain& rTerrain, const FrameStaticData& rStaticData, const XMFLOAT4A& rf4Position)
{
	const std::vector<IslandPlacement>& rIslands = rStaticData.islands;
	const std::vector<IslandRenderQuery>& rQueries = rStaticData.islandRenderQueries;
	bool bHaveQueryCache = rQueries.size() == rIslands.size();

	// Reused across the fallback path's iterations (every field overwritten before use each time), so the
	// common cache-hit path constructs nothing per island.
	IslandRenderQuery fallbackQuery;
	float fMaximumElevation = rTerrain.mfSeaFloorElevation;
	for (int64_t i = 0; i < std::ssize(rIslands); ++i)
	{
		const IslandRenderQuery* pQuery = nullptr;
		if (bHaveQueryCache)
		{
			pQuery = &rQueries.at(i);
		}
		else
		{
			const IslandPlacement& rPlacement = rIslands.at(i);
			const IslandTemplate& rTemplate = rTerrain.mIslands.at(rPlacement.islandCrc);
			fallbackQuery.fCosine = std::cos(-rPlacement.fRotation);
			fallbackQuery.fSine = std::sin(-rPlacement.fRotation);
			fallbackQuery.f2WorldPosition = rPlacement.f2WorldPosition;
			fallbackQuery.fFootprintX = rTemplate.fQuadFootprintX;
			fallbackQuery.fFootprintY = rTemplate.fQuadFootprintY;
			fallbackQuery.puiHeightmapHalf = rTemplate.puiHeightmapHalf;
			fallbackQuery.iHeightmapWidth = rTemplate.iHeightmapWidth;
			fallbackQuery.iHeightmapHeight = rTemplate.iHeightmapHeight;
			pQuery = &fallbackQuery;
		}
		const IslandRenderQuery& rQuery = *pQuery;

		float fDeltaX = rf4Position.x - rQuery.f2WorldPosition.x;
		float fDeltaY = rf4Position.y - rQuery.f2WorldPosition.y;
		float fSample = 0.0f;
		if (!SamplePlacementHeightmap<false, false>(fDeltaX, fDeltaY, rQuery.fCosine, rQuery.fSine, rQuery.fFootprintX, rQuery.fFootprintY, 0.0f, 0.0f, rQuery.puiHeightmapHalf, rQuery.iHeightmapWidth, rQuery.iHeightmapHeight, 0.0f, 0.0f, 0.0f, 0.0f, fSample))
		{
			continue;
		}

		fMaximumElevation = std::max(fMaximumElevation, fSample);
	}

	return fMaximumElevation;
}

float XM_CALLCONV IslandTerrain::GlobalElevation(GridCoord coord, FXMVECTOR vecLocalPosition) const
{
	// Frame Purity Constraint (IslandTerrain.h): GlobalElevation/GlobalNormal walk mCoordFrames with
	// libm trig and must never run from frame-tick code — the sim hot path uses FrameElevationSampler::Sample/FrameNormal.
	ASSERT(common::gpThreadLocal != nullptr && !common::gpThreadLocal->mbInFrameTick);

	XMFLOAT4A f4Local {};
	XMStoreFloat4A(&f4Local, vecLocalPosition);

	if (!ResolveLocalPosition(coord, f4Local))
	{
		return mfSeaFloorElevation;
	}

	// Look up per-cell placements. Cells outside the simulated set fall through to sea floor.
	auto it = game::gpGame->mCoordFrames.find(coord);
	if (it == game::gpGame->mCoordFrames.end())
	{
		return mfSeaFloorElevation;
	}

	return CellElevation(*this, it->second.staticData, f4Local);
}

// Splat one island placement's heightmap into the per-cell elevation grid (max-blend over the rotated
// footprint). Pure deterministic computation — the grid feeds FrameElevationSampler::Sample, which steers CRC'd sim
// positions, so client and server must build a bit-identical grid. Every position is centered cell-local
// meters, so grid texel 0,0 sits at the cell's south-west corner, the constant (kfBaseAreaMinimumX, kfBaseAreaMinimumY).
static void BlendPlacementIntoGrid(const IslandPlacement& rPlacement, const IslandTemplate& rTemplate, std::vector<float>& rOutGrid)
{
	static constexpr int64_t kiGridDimension = kiElevationGridDimension;
	static constexpr float kfGridPitchX = kfCellWidth / static_cast<float>(kiGridDimension);
	static constexpr float kfGridPitchY = kfCellHeight / static_cast<float>(kiGridDimension);

	float fFootprintX = rTemplate.fQuadFootprintX;
	float fFootprintY = rTemplate.fQuadFootprintY;
	float fHalfX = 0.5f * fFootprintX;
	float fHalfY = 0.5f * fFootprintY;

	// Negated rotation maps cell-local positions into the island's local frame, matching GlobalElevation.
	common::SinCos rotation = common::DeterministicSinCos(-rPlacement.fRotation);
	float fCosine = rotation.fCos;
	float fSine = rotation.fSin;

	// Cell-local AABB of the rotated quad: the 4 corners of the rotated footprint, projected onto X/Y.
	float fAbsoluteCosine = std::abs(fCosine);
	float fAbsoluteSine = std::abs(fSine);
	float fAxisAlignedBoundingBoxHalfX = fAbsoluteCosine * fHalfX + fAbsoluteSine * fHalfY;
	float fAxisAlignedBoundingBoxHalfY = fAbsoluteSine * fHalfX + fAbsoluteCosine * fHalfY;
	float fAxisAlignedBoundingBoxMinimumX = rPlacement.f2WorldPosition.x - fAxisAlignedBoundingBoxHalfX;
	float fAxisAlignedBoundingBoxMaximumX = rPlacement.f2WorldPosition.x + fAxisAlignedBoundingBoxHalfX;
	float fAxisAlignedBoundingBoxMinimumY = rPlacement.f2WorldPosition.y - fAxisAlignedBoundingBoxHalfY;
	float fAxisAlignedBoundingBoxMaximumY = rPlacement.f2WorldPosition.y + fAxisAlignedBoundingBoxHalfY;

	// Clamp AABB to this cell's grid index range. Texel center at (ix + 0.5) * pitch.
	int64_t iMinimumGridX = static_cast<int64_t>(std::floor((fAxisAlignedBoundingBoxMinimumX - kfBaseAreaMinimumX) / kfGridPitchX - 0.5f));
	int64_t iMaximumGridX = static_cast<int64_t>(std::floor((fAxisAlignedBoundingBoxMaximumX - kfBaseAreaMinimumX) / kfGridPitchX - 0.5f));
	int64_t iMinimumGridY = static_cast<int64_t>(std::floor((fAxisAlignedBoundingBoxMinimumY - kfBaseAreaMinimumY) / kfGridPitchY - 0.5f));
	int64_t iMaximumGridY = static_cast<int64_t>(std::floor((fAxisAlignedBoundingBoxMaximumY - kfBaseAreaMinimumY) / kfGridPitchY - 0.5f));
	iMinimumGridX = std::clamp(iMinimumGridX, static_cast<int64_t>(0), kiGridDimension - 1);
	iMaximumGridX = std::clamp(iMaximumGridX, static_cast<int64_t>(0), kiGridDimension - 1);
	iMinimumGridY = std::clamp(iMinimumGridY, static_cast<int64_t>(0), kiGridDimension - 1);
	iMaximumGridY = std::clamp(iMaximumGridY, static_cast<int64_t>(0), kiGridDimension - 1);

	float fHeightmapMaximumU = static_cast<float>(rTemplate.iHeightmapWidth - 1);
	float fHeightmapMaximumV = static_cast<float>(rTemplate.iHeightmapHeight - 1);
	float fInverseFootprintX = 1.0f / fFootprintX;
	float fInverseFootprintY = 1.0f / fFootprintY;

	for (int64_t j = iMinimumGridY; j <= iMaximumGridY; ++j)
	{
		float fTexelY = kfBaseAreaMinimumY + (static_cast<float>(j) + 0.5f) * kfGridPitchY;
		float fDeltaY = fTexelY - rPlacement.f2WorldPosition.y;
		for (int64_t i = iMinimumGridX; i <= iMaximumGridX; ++i)
		{
			float fTexelX = kfBaseAreaMinimumX + (static_cast<float>(i) + 0.5f) * kfGridPitchX;
			float fDeltaX = fTexelX - rPlacement.f2WorldPosition.x;
			float fSample = 0.0f;
			if (!SamplePlacementHeightmap<true, true>(fDeltaX, fDeltaY, fCosine, fSine, fFootprintX, fFootprintY, fHalfX, fHalfY, rTemplate.puiHeightmapHalf, rTemplate.iHeightmapWidth, rTemplate.iHeightmapHeight, fInverseFootprintX, fInverseFootprintY, fHeightmapMaximumU, fHeightmapMaximumV, fSample))
			{
				continue;
			}
			float& rfCell = rOutGrid.at(static_cast<size_t>(j * kiGridDimension + i));
			rfCell = std::max(rfCell, fSample);
		}
	}
}

void XM_CALLCONV IslandTerrain::BuildElevationGrid(const std::vector<IslandPlacement>& rPlacements, std::vector<float>& rOutGrid) const
{
	static constexpr int64_t kiGridDimension = kiElevationGridDimension;

	// Sea floor everywhere first; each placement then max-blends its footprint over the top
	// (commutative max → splat order doesn't matter, matches GlobalElevation's per-point semantics).
	rOutGrid.assign(static_cast<size_t>(kiGridDimension * kiGridDimension), mfSeaFloorElevation);

	for (const IslandPlacement& rPlacement : rPlacements)
	{
		BlendPlacementIntoGrid(rPlacement, mIslands.at(rPlacement.islandCrc), rOutGrid);
	}
}

FrameElevationSampler XM_CALLCONV IslandTerrain::MakeFrameElevationSampler(const FrameStaticData& rStaticData) const
{
	FrameElevationSampler sampler;
	sampler.fSeaFloor = mfSeaFloorElevation;
	if (rStaticData.elevationGrid.empty())
	{
		// Leave pGrid null so Sample returns fSeaFloor for an empty elevation grid.
		return sampler;
	}

	sampler.pGrid = &rStaticData.elevationGrid;
	return sampler;
}

float XM_CALLCONV FrameElevationSampler::Sample(FXMVECTOR vecLocalPosition) const
{
	if (pGrid == nullptr)
	{
		return fSeaFloor;
	}

	static constexpr int64_t kiGridDimension = kiElevationGridDimension;
	static constexpr float kfGridPitchX = kfCellWidth / static_cast<float>(kiGridDimension);
	static constexpr float kfGridPitchY = kfCellHeight / static_cast<float>(kiGridDimension);

	XMFLOAT4A f4Position {};
	XMStoreFloat4A(&f4Position, vecLocalPosition);

	float fGridX = f4Position.x - kfBaseAreaMinimumX;
	float fGridY = f4Position.y - kfBaseAreaMinimumY;
	int64_t iGridX = static_cast<int64_t>(std::floor(fGridX / kfGridPitchX));
	int64_t iGridY = static_cast<int64_t>(std::floor(fGridY / kfGridPitchY));
	if (iGridX < 0 || iGridX >= kiGridDimension || iGridY < 0 || iGridY >= kiGridDimension)
	{
		return fSeaFloor;
	}

	return pGrid->at(static_cast<size_t>(iGridY * kiGridDimension + iGridX));
}

XMVECTOR XM_CALLCONV IslandTerrain::FrameNormal(const FrameStaticData& rStaticData, FXMVECTOR vecLocalPosition) const
{
	// 4-tap finite-difference over FrameElevationSampler::Sample. Same baseline as GlobalNormal so contour-following
	// AI behaves identically — only the elevation source changes.
	float fDistance = 2.0f;

	return NormalFromElevation(vecLocalPosition, fDistance, [&](FXMVECTOR vecTap)
	{
		return MakeFrameElevationSampler(rStaticData).Sample(vecTap);
	});
}

XMVECTOR XM_CALLCONV IslandTerrain::GlobalNormal(GridCoord coord, FXMVECTOR vecLocalPosition) const
{
	// Frame Purity Constraint (see GlobalElevation): must never run from frame-tick code. GlobalNormal
	// resolves cells itself (batching the 4 taps' lookups below) rather than routing each tap through
	// GlobalElevation, so it carries its own guard.
	ASSERT(common::gpThreadLocal != nullptr && !common::gpThreadLocal->mbInFrameTick);

	// 4-tap finite-difference over the terrain elevation. Each tap resolves its own cell/island list, so a
	// single fixed baseline works across multiple islands at different scales. The 2-unit cross-tap baseline
	// (1 meter per half-step, since islands use 1 m = 1 engine unit) is fine-grained enough to capture
	// normals without falling below per-pixel heightmap noise.
	float fDistance = 2.0f;

	// The 4 taps sit ±fDistance around the center, so they almost always land in the same cell. Resolve the
	// cell (hash lookup + island/query list) once and cache it, re-resolving only when a tap crosses into a
	// neighbor cell — collapsing 4 hash lookups + 4 list walks to ~1 in the common case. Same per-tap
	// arithmetic and cell mapping as GlobalElevation, so results are identical.
	bool bHaveCachedCoord = false;
	GridCoord cachedCoord {};
	const FrameStaticData* pCachedStaticData = nullptr;

	auto SampleElevation = [&](FXMVECTOR vecTap) -> float
	{
		XMFLOAT4A f4Tap {};
		XMStoreFloat4A(&f4Tap, vecTap);
		GridCoord tapCoord = coord;
		if (!ResolveLocalPosition(tapCoord, f4Tap))
		{
			return mfSeaFloorElevation;
		}
		if (!bHaveCachedCoord || tapCoord != cachedCoord)
		{
			auto it = game::gpGame->mCoordFrames.find(tapCoord);
			pCachedStaticData = it == game::gpGame->mCoordFrames.end() ? nullptr : &it->second.staticData;
			cachedCoord = tapCoord;
			bHaveCachedCoord = true;
		}
		return pCachedStaticData == nullptr ? mfSeaFloorElevation : CellElevation(*this, *pCachedStaticData, f4Tap);
	};

	return NormalFromElevation(vecLocalPosition, fDistance, SampleElevation);
}

SegmentHit XM_CALLCONV TracePointAgainstTerrain(const FrameStaticData& rStaticData, FXMVECTOR vecStartPosition, FXMVECTOR vecEndPosition, float fStartTime, float fEndTime)
{
	static constexpr int64_t kiGridDimension = kiElevationGridDimension;
	static constexpr float kfGridPitchX = kfCellWidth / static_cast<float>(kiGridDimension);
	static constexpr float kfGridPitchY = kfCellHeight / static_cast<float>(kiGridDimension);
	static constexpr float kfCellOriginX = kfBaseAreaMinimumX;
	static constexpr float kfCellOriginY = kfBaseAreaMinimumY;

	FrameElevationSampler sampler = gpIslandTerrain->MakeFrameElevationSampler(rStaticData);
	XMFLOAT4A f4Start {};
	XMFLOAT4A f4End {};
	XMStoreFloat4A(&f4Start, vecStartPosition);
	XMStoreFloat4A(&f4End, vecEndPosition);

	float fDeltaX = f4End.x - f4Start.x;
	float fDeltaY = f4End.y - f4Start.y;
	float fDeltaZ = f4End.z - f4Start.z;
	int64_t iGridX = static_cast<int64_t>(std::floor((f4Start.x - kfCellOriginX) / kfGridPitchX));
	int64_t iGridY = static_cast<int64_t>(std::floor((f4Start.y - kfCellOriginY) / kfGridPitchY));
	int64_t iStepX = fDeltaX > 0.0f ? 1 : fDeltaX < 0.0f ? -1 : 0;
	int64_t iStepY = fDeltaY > 0.0f ? 1 : fDeltaY < 0.0f ? -1 : 0;
	int64_t iNextBoundaryX = iGridX + (iStepX > 0 ? 1 : 0);
	int64_t iNextBoundaryY = iGridY + (iStepY > 0 ? 1 : 0);
	float fAbsoluteDeltaX = std::abs(fDeltaX);
	float fAbsoluteDeltaY = std::abs(fDeltaY);

	float fCurrentPercent = 0.0f;
	for (;;)
	{
		XMVECTOR vecCurrentPosition = XMVectorLerp(vecStartPosition, vecEndPosition, fCurrentPercent);
		float fPointElevation = sampler.Sample(vecCurrentPosition);
		float fCurrentZ = f4Start.z + fDeltaZ * fCurrentPercent;
		if (fCurrentZ <= fPointElevation)
		{
			float fTime = fStartTime + fCurrentPercent * (fEndTime - fStartTime);
			return
			{
				.bHit = true,
				.fTime = fTime,
				.vecPosition = XMVectorSetZ(vecCurrentPosition, fPointElevation),
			};
		}

		float fDistanceX = std::numeric_limits<float>::max();
		float fDistanceY = std::numeric_limits<float>::max();
		if (iStepX != 0)
		{
			float fBoundaryX = kfCellOriginX + static_cast<float>(iNextBoundaryX) * kfGridPitchX;
			fDistanceX = std::abs(fBoundaryX - f4Start.x);
		}
		if (iStepY != 0)
		{
			float fBoundaryY = kfCellOriginY + static_cast<float>(iNextBoundaryY) * kfGridPitchY;
			fDistanceY = std::abs(fBoundaryY - f4Start.y);
		}

		bool bAdvanceX = false;
		bool bAdvanceY = false;
		if (iStepX == 0 && iStepY != 0)
		{
			bAdvanceY = true;
		}
		else if (iStepY == 0 && iStepX != 0)
		{
			bAdvanceX = true;
		}
		else if (iStepX != 0 && iStepY != 0)
		{
			// Compare the two rational crossing times by cross multiplication. A double exactly holds
			// the product of two floats, so mathematical corner ties advance both axes without epsilon.
			double fScaledDistanceX = static_cast<double>(fDistanceX) * static_cast<double>(fAbsoluteDeltaY);
			double fScaledDistanceY = static_cast<double>(fDistanceY) * static_cast<double>(fAbsoluteDeltaX);
			bAdvanceX = fScaledDistanceX <= fScaledDistanceY;
			bAdvanceY = fScaledDistanceY <= fScaledDistanceX;
		}

		float fNextPercent = 1.0f;
		if (bAdvanceX || bAdvanceY)
		{
			fNextPercent = bAdvanceX ? fDistanceX / fAbsoluteDeltaX : fDistanceY / fAbsoluteDeltaY;
			fNextPercent = std::min(1.0f, fNextPercent);
		}
		float fElevation = sampler.fSeaFloor;
		if (sampler.pGrid != nullptr && iGridX >= 0 && iGridX < kiGridDimension && iGridY >= 0 && iGridY < kiGridDimension)
		{
			fElevation = sampler.pGrid->at(static_cast<size_t>(iGridY * kiGridDimension + iGridX));
		}
		// The traversed cell owns the open interval after fCurrentPercent. If a height step rises immediately
		// across a boundary, collision begins at the boundary even when the exact point belongs to the
		// outgoing cell under floor ownership.
		if (fCurrentZ <= fElevation)
		{
			float fTime = fStartTime + fCurrentPercent * (fEndTime - fStartTime);
			XMVECTOR vecPosition = XMVectorSetZ(vecCurrentPosition, fElevation);
			return
			{
				.bHit = true,
				.fTime = fTime,
				.vecPosition = vecPosition,
			};
		}

		float fNextZ = f4Start.z + fDeltaZ * fNextPercent;
		// The next boundary is excluded from this cell. Equality is resolved below by Sample(), whose
		// floor lookup defines ownership for positive crossings and final endpoints.
		if (fNextZ < fElevation)
		{
			float fHitPercent = (fElevation - f4Start.z) / fDeltaZ;
			float fTime = fStartTime + fHitPercent * (fEndTime - fStartTime);
			XMVECTOR vecPosition = XMVectorSetZ(XMVectorLerp(vecStartPosition, vecEndPosition, fHitPercent), fElevation);
			return
			{
				.bHit = true,
				.fTime = fTime,
				.vecPosition = vecPosition,
			};
		}

		if (fNextPercent >= 1.0f)
		{
			float fEndElevation = sampler.Sample(vecEndPosition);
			if (f4End.z <= fEndElevation)
			{
				return
				{
					.bHit = true,
					.fTime = fEndTime,
					.vecPosition = XMVectorSetZ(vecEndPosition, fEndElevation),
				};
			}
			return {};
		}

		if (bAdvanceX)
		{
			iGridX += iStepX;
			iNextBoundaryX += iStepX;
		}
		if (bAdvanceY)
		{
			iGridY += iStepY;
			iNextBoundaryY += iStepY;
		}
		fCurrentPercent = fNextPercent;
	}
}

} // namespace engine
