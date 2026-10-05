#pragma once

#include "Frame/FrameUtils.h"
#include "Frame/GridCoord.h"
#include "Frame/NavBuild.h"
#if defined(BT_CLIENT)
#include "Graphics/Objects/Buffer.h"
#include "Graphics/Objects/Texture.h"
#endif

namespace engine
{

struct FrameStaticData;

#if defined(BT_CLIENT)
enum class IslandMeshResidency : uint8_t
{
	kNonresident,
	kAsyncPending,
	kCpuReady,
	kArenaBlocked,
	kFailed,
	kResident,
};
#endif

// One entry per kIsland chunk in the manifest. Heightmap pointer fills in
// WaitForElevationMaps once chunk data is resident; NavContour is built for
// every template server-side so multi-template cells get correct nav data.
struct IslandTemplate
{
	common::crc_t islandCrc = 0;

	// Heightmap samples use engine meters relative to beach: zero is sea level, negative is submerged,
	// positive is above. At export DataPacker shifts Gaea's normalized output by Sea.Level *
	// elevationMeters (Level fallback 0.1), giving sea-floor depth -(Level * elevationMeters) without
	// runtime conversion. The anisotropic land bounds above 1 m expand to multiples of 4 *
	// kiElevationDivisor on both axes for BC/downsample alignment. puiHeightmapHalf aliases quantized IEEE
	// R16 half-floats at kIsland payload offset zero; readers use
	// DirectX::PackedVector::XMConvertHalfToFloat.
	const uint16_t* puiHeightmapHalf = nullptr;
	int64_t iHeightmapWidth = 0;
	int64_t iHeightmapHeight = 0;

	float fWorldFootprintXMeters = 0.0f;
	float fWorldFootprintYMeters = 0.0f;
	float fWorldElevationMeters = 0.0f;

	// Anisotropic quad footprint in engine units (islands use 1 m = 1 engine unit), so a
	// direct copy of fWorldFootprint{X,Y}Meters; set in ctor.
	float fQuadFootprintX = 0.0f;
	float fQuadFootprintY = 0.0f;

	NavContour navContour;

	int64_t iTextureSlot = -1;

	// Stable template-group index matching IslandTerrain::mIslandCrcsSorted, assigned in ctor right after
	// the sort. Identifies the template's grouping and indirect-command slot in Islands; never changes
	// after boot.
	// Decoupled from iTextureSlot (which mints lazily on first visit).
	int64_t iTemplateArrayIndex = -1;

	// Mesh vertex/index counts and CPU mesh data pointers (pfMeshPositions / puiMeshIndices) are
	// populated by WaitForElevationMaps after it validates the resident kIsland chunk payload.
	int64_t iMeshVertexCount = 0;
	int64_t iMeshIndexCount = 0;

	// Per-island valid-area convex hull (CCW) in island-local meters, centered. Slices the kIsland
	// chunk payload after the mesh indices (set by WaitForElevationMaps). Shared: the server packs
	// island placements against the rotated hull (IslandChainPlacement) and the client also debug-
	// renders it (MainUniforms DebugRenderIslandValidArea). A count < 3 (or null pointer) means no
	// usable polygon.
	const XMFLOAT2* pf2ValidAreaVertices = nullptr;
	int64_t iValidAreaVertexCount = 0;

#if defined(BT_CLIENT)
	// Phase 5 LRU eviction state. bGpuResident means "slot points at this template's real
	// Texture*s AND those Textures have live GPU resources". False while in slot-0 fallback
	// (the neutral placeholder textures) — covers both first-mint-pre-adopt and
	// post-eviction-pre-restore. iReferenceCount is recomputed from scratch each frame in
	// Islands::UpdateActiveIslands.
	int64_t iReferenceCount = 0;
	int64_t iLastUsedRenderFrame = 0;
	bool bGpuResident = false;

	// Gaea Mesher-baked terrain mesh in island-local meters (XY centered). CPU pointers slice into
	// the kIsland chunk payload after the heightmap halfs (set by WaitForElevationMaps). The
	// persistent arena is addressed through these virtual allocations; Z is not stored because
	// Terrain.vert re-derives it from the elevation sampler.
	const float* pfMeshPositions = nullptr;   // interleaved XY pairs (2 floats per vertex)
	const uint32_t* puiMeshIndices = nullptr;
	VmaVirtualAllocation meshIndexAllocation = VK_NULL_HANDLE;
	VmaVirtualAllocation meshVertexAllocation = VK_NULL_HANDLE;
	int64_t iMeshIndexOffset = 0;
	int64_t iMeshVertexOffset = 0;
	IslandMeshResidency eMeshResidency = IslandMeshResidency::kNonresident;
	int64_t iMeshArenaBlockedGeneration = 0;
	// True once the [positions][indices] CPU slice has been decommitted from the lazy pool.
	bool bMeshCpuDecommitted = false;

	// Elevation R16_SFLOAT is raw-copied from puiHeightmapHalf at first mint, freed by EvictionSweep with
	// color/normals/AO/masks, and recreated on the next AcquireTextureSlot first mint. The template owns it
	// outside TextureManager::mTextureMap because elevation lives in the kIsland heightmap payload,
	// without a standalone chunk.
	Texture elevationTexture;
#endif
};

// Batch samples reuse one cell's elevation grid and sea-floor value through the /fp:strict simulation path.
// A null pGrid returns fSeaFloor.
struct FrameElevationSampler
{
	const std::vector<float>* pGrid = nullptr;
	float fSeaFloor = 0.0f;

	[[nodiscard]] float XM_CALLCONV Sample(FXMVECTOR vecLocalPosition) const;
};

class IslandTerrain
{
public:

	IslandTerrain();
	~IslandTerrain();

#if defined(BT_SERVER)
	void WaitForElevationMaps(float fNavigationThreshold, float fNavigationClearanceMeters);
#else
	void WaitForElevationMaps();
#endif

	// Sim path (Frame-tick callers). Cell-local O(1) nearest-texel lookup into the cell's
	// precomputed FrameStaticData::elevationGrid, taking the position in that cell's own centered local
	// meters. Out-of-cell positions return mfSeaFloorElevation. Honors the Frame Purity Constraint: the
	// caller hands its own static data in, so this never touches gpGame->mCoordinateFrames and never reads a
	// neighbor cell. Builds happen at the top of RunFrameTick (see FrameBase.cpp), before any sim phase
	// that would query.
	[[nodiscard]] XMVECTOR XM_CALLCONV FrameNormal(const FrameStaticData& rStaticData, FXMVECTOR vecLocalPosition) const;

	[[nodiscard]] FrameElevationSampler XM_CALLCONV MakeFrameElevationSampler(const FrameStaticData& rStaticData) const;

	// Centered cell-local grids cover [-450,+450] meters; overlapping island footprints max-blend, matching GlobalElevation.
	void XM_CALLCONV BuildElevationGrid(const std::vector<IslandPlacement>& rPlacements, std::vector<float>& rOutGrid) const;

	// Render path (engine client — ProjectToBaseHeight). Takes the cell the position is local to plus that
	// local position, and iterates mCoordinateFrames' immutable islands list; a position (or a GlobalNormal tap)
	// past the cell edge resolves onto the neighbouring cell. Never touches the per-cell grid, so it never
	// races the tick-time build. MUST NOT be called from Frame-tick code; use FrameElevationSampler::Sample/FrameNormal
	// from a Frame-tick context.
	[[nodiscard]] float XM_CALLCONV GlobalElevation(GridCoord coord, FXMVECTOR vecLocalPosition) const;
	[[nodiscard]] XMVECTOR XM_CALLCONV GlobalNormal(GridCoord coord, FXMVECTOR vecLocalPosition) const;

	std::unordered_map<common::crc_t, IslandTemplate> mIslands;
	std::vector<common::crc_t> mIslandCrcsSorted;

	// Same CRCs as mIslandCrcsSorted, ordered by footprint area (fWorldFootprintXMeters *
	// fWorldFootprintYMeters) descending, CRC ascending as a stable tiebreak. Drives only the
	// debug main-menu island browser (Game::BuildMenuIslandPlacement) so it cycles largest-first.
	// Kept separate from mIslandCrcsSorted, whose CRC order is load-bearing (template slot
	// assignment + world-gen placement RNG) and must not change.
	std::vector<common::crc_t> mIslandCrcsByArea;

	// IslandChainPlacement selects roles from size buckets in the same CRC order on client and server.
	// Empty buckets fall back through related buckets, then mIslandCrcsSorted.
	std::vector<common::crc_t> mHugeCrcs;      // area >= kfHugeIslandAreaMeters   (1x1 full tiles, ~400x400)
	std::vector<common::crc_t> mLargeCrcs;     // area >= kfLargeIslandAreaMeters  (2x1 / 3x1 strips)
	std::vector<common::crc_t> mMediumCrcs;    // area >= kfMediumIslandAreaMeters (mid tiles)
	std::vector<common::crc_t> mSmallCrcs;     // smaller                          (4x4 tiles, ~100x100)

	// Open-ocean samples share the elevation render target's sea-floor depth to avoid blends at island edges.
	// Island heightmaps already store negative depths in engine meters.
	float mfSeaFloorElevation = common::kfSeaBottomMeters;
};

inline IslandTerrain* gpIslandTerrain = nullptr;

SegmentHit XM_CALLCONV TracePointAgainstTerrain(const FrameStaticData& rStaticData, FXMVECTOR vecStartPosition, FXMVECTOR vecEndPosition, float fStartTime, float fEndTime);

} // namespace engine
