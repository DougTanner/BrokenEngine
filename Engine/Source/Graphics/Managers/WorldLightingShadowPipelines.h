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

	// The combine, temporal, and history-copy arrays hold one pipeline per lighting layer.
	WorldLightingShadowPipelines(std::unordered_map<common::crc_t, Shader>& rShaders, Pipeline* pPipelines, Pipeline* pSpreadPipelines, std::string* pSpreadPipelineNames, Pipeline* pCombinePipelines, Pipeline* pLightingTemporalPipelines, Pipeline* pLightingHistoryCopyPipelines, Texture** ppWaterNormalTextures, Texture** ppWaterFoamTextures, Texture** ppWaterFoamBreakupTextures);

	void CreateLightingPipelines();
	void CreatePipelineShadows();
	void CreateLightingBlurPipelines();
	void CreateLightingShadowDependentPipelines();

	std::unordered_map<common::crc_t, Shader>& mrShaders;
	Pipeline* mpPipelines = nullptr;
	Pipeline* mpSpreadPipelines = nullptr;
	std::string* mpSpreadPipelineNames = nullptr;
	Pipeline* mpCombinePipelines = nullptr;
	Pipeline* mpLightingTemporalPipelines = nullptr;
	Pipeline* mpLightingHistoryCopyPipelines = nullptr;
	Texture** mppWaterNormalTextures = nullptr;
	Texture** mppWaterFoamTextures = nullptr;
	Texture** mppWaterFoamBreakupTextures = nullptr;
};

} // namespace engine

#endif // defined(BT_CLIENT)
