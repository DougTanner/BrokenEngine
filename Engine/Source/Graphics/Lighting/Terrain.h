#pragma once

#if defined(BT_CLIENT)

#include "../../../Data/Shaders/Lighting/LightingTerrainCommon.h"
#include "Lighting.h"

namespace engine
{

struct LightingTerrainCounts
{
	int64_t iPlacements = 0;
	int64_t iVertices = 0;
	int64_t iTriangles = 0;
	int64_t iTiles = 0;
	int64_t iWorkgroups = 0;
};

struct LightingTerrainOperations
{
	int64_t iRenderPasses = 0;
	int64_t iDraws = 0;
	int64_t iDispatches = 0;
	int64_t iBuilds = 0;
	int64_t iBarriers = 0;
	int64_t iCopies = 0;
	int64_t iFills = 0;
	int64_t iUpdates = 0;
	int64_t iTimestamps = 0;
	int64_t iQueryResets = 0;
};

struct LightingTerrainCompletion
{
	shaders::LightingTerrainValidation validation {};
	std::span<const shaders::LightingTerrainTileResult> tiles;
	// TEMP START bounded terrain evidence
	const shaders::LightingTerrainEvidence* pEvidence = nullptr;
	// TEMP END bounded terrain evidence
	LightingTerrainCounts counts;
	LightingTerrainOperations productionOperations;
	LightingTerrainOperations validationOperations;
	GridCoord basis {};
	XMFLOAT4A f4VisibleArea {};
	VkExtent3D vkVisibleExtent {};
	uint64_t uiPublication = 0;
	uint64_t uiResourceGeneration = 0;
	std::chrono::duration<double, std::milli> durationPreparation = 0ms;
	std::chrono::duration<double, std::milli> durationWorstTile = 0ms;
	std::chrono::duration<double, std::milli> durationValidation = 0ms;
};

struct LightingTerrainBytes
{
	LightingAllocation scratch;
	LightingAllocation xyz;
	LightingAllocation work;
	LightingAllocation materials;
	LightingAllocation blas;
	LightingAllocation accelerationScratch;
	LightingAllocation validation;
};

struct LightingTerrainPlacementIdentity
{
	common::crc_t templateCrc = 0;
	uint32_t uiPlacement = 0;
	uint32_t uiInstance = 0;
	uint32_t uiGeometry = 0;
	uint32_t uiVertices = 0;
	uint32_t uiTriangles = 0;
	VkDeviceAddress vkXY = 0;
	VkDeviceAddress vkXYZ = 0;
	VkDeviceAddress vkIndices = 0;
	VkAccelerationStructureKHR vkBLAS = VK_NULL_HANDLE;
	VkDeviceAddress vkBLASAddress = 0;
	int32_t iMinTileX = 0;
	int32_t iMinTileY = 0;
	int32_t iMaxTileX = 0;
	int32_t iMaxTileY = 0;
};

class LightingTerrain
{
public:
	LightingTerrain();
	~LightingTerrain();
	void CreateResources();
	void DestroyResources();
	void CreatePipelines();
	void DestroyPipelines();
	LightingTerrainCounts Prepare(int64_t iFramebuffer, uint64_t uiValidationSerial = 0, uint64_t uiPublication = 0);
	void Write(int64_t iFramebuffer, uint32_t uiFirstInstance, uint32_t uiFirstGeometry, std::span<shaders::LightingInstance> instances, std::span<shaders::LightingGeometry> geometries, std::span<VkAccelerationStructureInstanceKHR> accelerationInstances, VkDeviceAddress vkGeometryAddress);
	void RecordPreparation(VkCommandBuffer vkCommandBuffer, int64_t iFramebuffer);
	uint64_t GetResourceBytes() const;
	LightingTerrainBytes GetResourceBytesDetail() const;
	const LightingTerrainCompletion& Complete(int64_t iFramebuffer);
	void WritePlacementIdentities(int64_t iFramebuffer, std::span<LightingTerrainPlacementIdentity> identities) const;

private:
	struct State;
	std::unique_ptr<State> mpState;
};

} // namespace engine

#endif // defined(BT_CLIENT)
