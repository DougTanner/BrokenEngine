#pragma once

#if defined(BT_CLIENT)

namespace engine
{

struct WorldLightingShadowPipelines
{
	static void CreateLightingPipelines();
	static void CreatePipelineShadows();
	static void CreateLightingBlurPipelines();
	static void CreateLightingShadowDependentPipelines();
};

} // namespace engine

#endif // defined(BT_CLIENT)
