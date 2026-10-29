#pragma once

#if defined(BT_CLIENT)

#include "Lighting.h"

namespace engine
{

class Shader;
struct LightingModelRange;

struct LightingModelCounts
{
	uint32_t uiInstances = 0;
	uint32_t uiGeometries = 0;
	uint32_t uiTriangles = 0;
	uint32_t uiMaterials = 0;
	uint32_t uiTraceOnlyInstances = 0;
};

class LightingModels
{
public:
	LightingModels();
	~LightingModels();
	void Create(const Shader& rDeformationShader);
	void DestroyPipeline();
	// Prepare and Write run after every producer has joined and completed all buffer growth.
	LightingModelCounts Prepare(int64_t iFramebuffer, std::span<const LightingModelRange> ranges);
	void Write(int64_t iFramebuffer, std::span<const LightingModelRange> ranges, uint32_t uiFirstInstance, uint32_t uiFirstGeometry, std::span<shaders::LightingInstance> instances, std::span<shaders::LightingGeometry> geometries, std::span<VkAccelerationStructureInstanceKHR> accelerationInstances);
	void RecordPreparation(VkCommandBuffer vkCommandBuffer, int64_t iFramebuffer, std::span<const LightingModelRange> ranges);

private:
	struct State;
	std::unique_ptr<State> mpState;
};

} // namespace engine

#endif // defined(BT_CLIENT)
