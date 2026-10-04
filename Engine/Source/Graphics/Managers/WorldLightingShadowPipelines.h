#pragma once

#if defined(BT_CLIENT)

namespace engine
{

class Pipeline;
class Shader;
class Texture;

class WorldLightingShadowPipelines
{
public:

	WorldLightingShadowPipelines(std::unordered_map<common::crc_t, Shader>& rShaders, Pipeline* pPipelines, Pipeline* pSpreadPipelines, std::string* pSpreadPipelineNames, Pipeline& rCombinePipeline, Pipeline& rLightingTemporalPipeline, Pipeline& rLightingHistoryCopyPipeline, Texture** ppWaterNormalTextures);

	void CreateLightingPipelines();
	void CreatePipelineShadows();
	void CreateLightingBlurPipelines();
	void CreateLightingShadowDependentPipelines();

	std::unordered_map<common::crc_t, Shader>& mrShaders;
	Pipeline* mpPipelines = nullptr;
	Pipeline* mpSpreadPipelines = nullptr;
	std::string* mpSpreadPipelineNames = nullptr;
	Pipeline& mrCombinePipeline;
	Pipeline& mrLightingTemporalPipeline;
	Pipeline& mrLightingHistoryCopyPipeline;
	Texture** mppWaterNormalTextures = nullptr;
};

} // namespace engine

#endif // defined(BT_CLIENT)
