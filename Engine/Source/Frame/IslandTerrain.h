#pragma once

#include "Frame/GridCoord.h"
#include "Frame/NavBuild.h"
#if defined(BT_CLIENT)
#include "Graphics/Objects/Buffer.h"
#include "Graphics/Objects/Texture.h"
#endif

namespace engine
{

struct CellStaticData;

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
// WaitForElevationMaps once chunk data is resident; navContours are built for
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

	std::vector<NavContour> navContours;   // one per flight plane, index-parallel with gpFlightPlanes; empty without kTerrainCollision

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
	// chunk payload after the mesh indices. Shared: the server packs island placements against the
	// rotated hull (IslandChainPlacement) and the client also debug-renders it (MainUniforms
	// DebugRenderIslandValidArea). WaitForElevationMaps sets a non-null pointer and a count of at
	// least 3 for every template.
	const XMFLOAT2* pf2ValidAreaVertices = nullptr;
	int64_t iValidAreaVertexCount = 0;

#if defined(BT_CLIENT)
	// LRU eviction state. bGpuResident means "slot points at this template's real
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
	float fMeshMinX = 0.0f;
	float fMeshMinY = 0.0f;
	float fMeshMaxX = 0.0f;
	float fMeshMaxY = 0.0f;
	IslandMeshResidency eMeshResidency = IslandMeshResidency::kNonresident;
	int64_t iMeshArenaBlockedGeneration = 0;
	// True once the [positions][indices] CPU slice has been decommitted from the lazy pool.
	bool bMeshCpuDecommitted = false;

	// Elevation R16_SFLOAT is raw-copied from the resident heightmap after first mint (by the upload thread on a
	// distinct transfer queue, otherwise recorded at restoration), retired by EvictionSweep with
	// color/normals/AO/masks, and re-uploaded after the next AcquireTextureSlot first mint. The template owns it
	// outside TextureManager::mTextureMap because elevation lives in the kIsland heightmap payload,
	// without a standalone chunk.
	Texture elevationTexture;
#endif
};

// Batch samples reuse one cell's elevation grid and sea-floor value through the /fp:strict simulation path.
// A null pGrid returns fSeaFloor.
struct CellElevationSampler
{
	const std::vector<float>* pGrid = nullptr;
	float fSeaFloor = 0.0f;

	[[nodiscard]] float XM_CALLCONV Sample(FXMVECTOR vecLocalPosition) const;
};

class IslandTerrain : public common::Singleton<IslandTerrain>
{
public:

	IslandTerrain();

#if defined(BT_SERVER)
	void WaitForElevationMaps(std::span<const float> navigationThresholds, float fNavigationClearanceMeters); // size == gpFlightPlanes->Count()
#else
	void WaitForElevationMaps();
#endif

	// Sim path (Frame-tick callers). Cell-local surface normal from four nearest-texel samples of the cell's
	// precomputed CellStaticData::elevationGrid, taking the position in that cell's own centered local
	// meters. A tap outside the cell samples mfSeaFloorElevation. Honors the Frame Purity Constraint: the
	// caller hands its own static data in, so this never touches gpGame->mCells and never reads a
	// neighbor cell. Builds happen at the top of RunFrameTick (see FrameBase.cpp), before any sim phase
	// that would query.
	[[nodiscard]] XMVECTOR XM_CALLCONV CellNormal(const CellStaticData& rStaticData, FXMVECTOR vecLocalPosition) const;

	// Render path. Highest elevation of the cell's island placements at rf4Position, local to that cell; sea floor
	// where no placement covers it.
	[[nodiscard]] float CellElevation(const CellStaticData& rStaticData, const XMFLOAT4A& rf4Position) const;

	[[nodiscard]] CellElevationSampler XM_CALLCONV MakeCellElevationSampler(const CellStaticData& rStaticData) const;

	// Centered cell-local grids cover the cell's bounding rectangle, with sea floor outside its hexagon; overlapping island
	// footprints max-blend, matching GlobalElevation.
	void XM_CALLCONV BuildElevationGrid(const std::vector<IslandPlacement>& rPlacements, std::vector<float>& rOutGrid) const;

	std::unordered_map<common::crc_t, IslandTemplate> mIslands;
	std::vector<common::crc_t> mIslandCrcsSorted;

	// Same CRCs as mIslandCrcsSorted, ordered by footprint area (fWorldFootprintXMeters *
	// fWorldFootprintYMeters) descending, CRC ascending as a stable tiebreak. Drives only the
	// debug main-menu island browser (GameBase::BuildMenuIslandPlacement) so it cycles largest-first.
	// Kept separate from mIslandCrcsSorted, whose CRC order is load-bearing (template slot
	// assignment + world-gen placement RNG) and must not change.
	std::vector<common::crc_t> mIslandCrcsByArea;

	// IslandChainPlacement selects roles from size buckets in the same CRC order on client and server.
	std::vector<common::crc_t> mHugeCrcs;      // area >= kfHugeIslandAreaMeters   (1x1 full tiles, ~400x400)
	std::vector<common::crc_t> mLargeCrcs;     // area >= kfLargeIslandAreaMeters  (2x1 / 3x1 strips)
	std::vector<common::crc_t> mMediumCrcs;    // area >= kfMediumIslandAreaMeters (mid tiles)
	std::vector<common::crc_t> mSmallCrcs;     // smaller                          (4x4 tiles, ~100x100)

	// Open-ocean samples share the elevation render target's sea-floor depth to avoid blends at island edges.
	// Island heightmaps already store negative depths in engine meters.
	float mfSeaFloorElevation = common::kfSeaBottomMeters;
};

inline IslandTerrain* gpIslandTerrain = nullptr;

} // namespace engine
