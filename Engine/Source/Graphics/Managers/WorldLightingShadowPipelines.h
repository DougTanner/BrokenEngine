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

	WorldLightingShadowPipelines(std::unordered_map<common::crc_t, Shader>& rShaders, Pipeline* pPipelines, Texture** ppWaterNormalTextures, Texture** ppWaterFoamTextures, Texture** ppWaterFoamBreakupTextures);

	void CreatePipelineShadows();
	void CreateLightingShadowDependentPipelines();

	std::unordered_map<common::crc_t, Shader>& mrShaders;
	Pipeline* mpPipelines = nullptr;
	Texture** mppWaterNormalTextures = nullptr;
	Texture** mppWaterFoamTextures = nullptr;
	Texture** mppWaterFoamBreakupTextures = nullptr;
};

} // namespace engine

#endif // defined(BT_CLIENT)
