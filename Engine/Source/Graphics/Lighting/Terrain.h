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
	LightingTerrainCounts Prepare(int64_t iFramebuffer);
	void Write(int64_t iFramebuffer, uint32_t uiFirstInstance, uint32_t uiFirstGeometry, std::span<shaders::LightingInstance> instances, std::span<shaders::LightingGeometry> geometries, std::span<VkAccelerationStructureInstanceKHR> accelerationInstances);
	void RecordPreparation(VkCommandBuffer vkCommandBuffer, int64_t iFramebuffer);

private:
	struct State;
	std::unique_ptr<State> mpState;
};

} // namespace engine

#endif // defined(BT_CLIENT)
