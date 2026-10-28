#ifndef LIGHTING_TERRAIN_COMMON_H
#define LIGHTING_TERRAIN_COMMON_H

#if defined(BT_ENGINE)
namespace shaders
{
#endif

CONSTEXPR int kiLightingTerrainTile = 256;
CONSTEXPR int kiLightingTerrainHalo = 4;
CONSTEXPR int kiLightingTerrainScratch = 264;
CONSTEXPR int kiLightingTerrainGroup = 64;

struct LightingTerrainWork
{
	uint64_t uiXYAddress;
	uint64_t uiXYZAddress;
	uint32_t uiVertexCount;
	uint32_t uiPadding;
	uint64_t uiWritesAddress;
	uint64_t uiOriginalAddress;
	uint64_t uiSummaryAddress;
	uint64_t uiBaseTileAddress;
	uint64_t uiIndexAddress;
	uint64_t uiTileResultsAddress;
	uint64_t uiTemplate;
	uint64_t uiSerial;
	uint32_t uiPrimitiveCount;
	uint32_t uiValidation;
	uint64_t uiBaseZAddress;
	uint64_t uiHitGeometryAddress;
	int32_t iFootprintMinTileX;
	int32_t iFootprintMinTileY;
	int32_t iFootprintMaxTileX;
	int32_t iFootprintMaxTileY;
	uint32_t uiBuildPrimitiveCount;
	uint32_t uiBuildVertexCount;
	// TEMP START bounded terrain evidence
	uint64_t uiEvidenceAddress;
	// TEMP END bounded terrain evidence
};

struct LightingTerrainOriginal
{
	vec3 f3Position;
	float fOwn;
	uint32_t uiWritten;
	uint32_t uiCutoff;
	// TEMP START bounded terrain evidence
	uint32_t uiInvocations;
	vec2 f2UV;
	vec2 f2OwnUV;
	float fSampledZ;
	vec2 f2ProductionUV;
	vec2 f2ProductionOwnUV;
	vec2 f2ProductionPixel;
	float fProductionBefore;
	float fProductionOwn;
	// TEMP END bounded terrain evidence
};

// TEMP START bounded terrain evidence
CONSTEXPR int kiTerrainEvidenceClasses = 18;
CONSTEXPR int kiTerrainEvidenceCapacity = 136;
struct LightingTerrainEvidenceRecord
{
	uint32_t puiWords[128];
};
struct LightingTerrainEvidence
{
	uint32_t puiClaims[18];
	uint32_t puiCounts[18];
	uint32_t uiListCount;
	uint32_t uiOverflow;
	LightingTerrainEvidenceRecord pClasses[18];
	LightingTerrainEvidenceRecord pVertices[136];
};
struct LightingTerrainBaseSample
{
	vec2 f2UV;
	float fBefore;
	float fAfter;
};
// TEMP END bounded terrain evidence

struct LightingTerrainTileResult
{
	int32_t iOriginX;
	int32_t iOriginY;
	uint32_t uiPlacement;
	uint32_t uiVertices;
	uint32_t puiComparisons[4];
	uint32_t puiRawMismatches[4];
	uint32_t puiXYZMismatches[4];
	uint32_t puiXYZMaxBits[4];
};

struct LightingTerrainValidation
{
	uint64_t uiRequestedSerial;
	uint64_t uiCompletedSerial;
	uint32_t uiOnce;
	uint32_t uiUntouched;
	uint32_t uiDuplicate;
	uint32_t uiIndices;
	uint32_t uiInvalidIndices;
	uint32_t uiTriangles;
	uint32_t uiNonfinite;
	uint32_t uiOverhang;
	uint32_t uiOverhangBeyondFootprint;
	uint32_t uiSeam;
	uint32_t uiCutoffBelow;
	uint32_t uiCutoffEqual;
	uint32_t uiOriginalComparisons;
	uint32_t uiOriginalMissing;
	uint32_t uiOriginalMismatches;
	uint32_t uiOriginalMaxBits;
	uint32_t uiGlobalComparisons;
	uint32_t uiGlobalMismatches;
	uint32_t uiBaseComparisons;
	uint32_t uiBaseMismatches;
	uint32_t uiBaseXYZMismatches;
	uint32_t uiTiles;
	uint32_t uiEmptyTiles;
	uint32_t uiShifts;
	uint32_t uiExcludedTaps;
	uint32_t uiAdjacentRenders;
	uint32_t uiAdjacentComparisons;
	uint32_t uiAdjacentMismatches;
	uint32_t uiMappingMismatches;
	uint32_t uiFirstClaim;
	uint32_t uiFirstPlacement;
	uint32_t uiFirstVertex;
	uint32_t uiFirstIndex;
	uint64_t uiFirstTemplate;
	vec2 f2FirstLocal;
	vec3 f3FirstPosition;
	float fFirstOwn;
	vec2 f2FirstPixel;
	int32_t iFirstOriginX;
	int32_t iFirstOriginY;
	int32_t iFirstShift;
	int32_t iFirstTapX;
	int32_t iFirstTapY;
	uint32_t uiFirstBaseBits;
	uint32_t uiFirstOtherBits;
	float fFirstBaseZ;
	float fFirstOtherZ;
	float fFirstCutoff;
};

struct LightingTerrainValidatePush
{
	int32_t iOriginX;
	int32_t iOriginY;
	uint32_t uiPlacement;
	uint32_t uiBase;
	uint32_t uiMode;
	uint32_t uiShift;
	uint32_t uiTile;
	uint32_t uiPadding;
};

struct LightingTerrainMaterial
{
	uint32_t uiPlacement;
	uint32_t uiTextureSlot;
	vec2 f2Footprint;
	float fRotation;
	uint32_t uiPadding;
};

#if defined(BT_ENGINE)
// TEMP START bounded terrain evidence
static_assert(std::cmp_equal(sizeof(LightingTerrainWork), 144));
// TEMP END bounded terrain evidence
static_assert(std::cmp_equal(offsetof(LightingTerrainWork, uiVertexCount), 16));
static_assert(std::cmp_equal(sizeof(LightingTerrainMaterial), 24));
// TEMP START bounded terrain evidence
static_assert(sizeof(LightingTerrainOriginal) == 80);
static_assert(sizeof(LightingTerrainEvidenceRecord) == 512);
static_assert(offsetof(LightingTerrainEvidence, pClasses) == 152);
static_assert(sizeof(LightingTerrainEvidence) == 79000);
static_assert(sizeof(LightingTerrainBaseSample) == 16);
// TEMP END bounded terrain evidence
static_assert(sizeof(LightingTerrainTileResult) == 80);
static_assert(sizeof(LightingTerrainValidatePush) == 32);
static_assert(sizeof(LightingTerrainValidation) == 232);
static_assert(offsetof(LightingTerrainValidation, uiFirstTemplate) == 152);
static_assert(offsetof(LightingTerrainValidation, f2FirstLocal) == 160);
} // namespace shaders
#endif

#endif
