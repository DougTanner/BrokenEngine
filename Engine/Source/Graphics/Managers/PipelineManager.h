#pragma once

#if defined(BT_CLIENT)

#include "DynamicPipelines.h"

namespace engine
{

enum Pipelines
{
	kPipelineLog,

	kPipelineObjectShadowsBlurH,
	kPipelineObjectShadowsBlurV,
	kPipelineShadow,
	kPipelineShadowBlurH,
	kPipelineShadowBlurV,
	kPipelineShadowTemporal,
	kPipelineShadowHistoryCopy,
	kPipelineShadowElevation,

	kPipelineTerrainElevation,
	kPipelineTerrain,

	kPipelineWater,
	kPipelineWaterDisplacement,
	kPipelineWaterFoam,

	kPipelineSmokeClearA,
	kPipelineSmokeClearB,
	kPipelineSmokeOccupancyDilate,
	kPipelineSmokeOccupancyDilateRemap,
	kPipelineSmokeSpreadComputeB,
	kPipelineSmokeSpreadComputeA,

	kPipelineWindOccupancyDilateA,
	kPipelineWindOccupancyDilateB,
	kPipelineWindSpreadComputeA,
	kPipelineWindSpreadComputeB,

	kPipelineLongParticlesSpawn,
	kPipelineLongParticlesUpdate,
	kPipelineLongParticlesRender,
	kPipelineSquareParticlesSpawn,
	kPipelineSquareParticlesUpdate,
	kPipelineSquareParticlesRender,

	kPipelineLightingBlurH,
	kPipelineLightingBlurV,

	kPipelineDebugTexture,

	kPipelineUiDepthPrepass,

	kPipelineDebugBox,
	kPipelineDebugSphere,
	kPipelineDebugCircle,
	kPipelineDebugLine,

	kPipelineHdrResolve,

	kPipelineCount,
};

class PipelineManager : public common::Singleton<PipelineManager>
{
public:

	PipelineManager();

	std::unordered_map<common::crc_t, Shader> mShaders;

	void CreateTerrainDataPipelines();
	void CreateSmokeWindPipelines();
	void CreateParticlePipelines();
	void CreateDebugRenderPipelines();

	Pipeline mpPipelines[kPipelineCount];

	// Texture* array for the kPipelineWater normal map atlas binding (sized sampler array).
	// Populated by WorldLightingShadowPipelines::CreateLightingShadowDependentPipelines() before kPipelineWater is built.
	Texture* mppWaterNormalTextures[shaders::kiWaterNormalCount] {};
	Texture* mppWaterFoamTextures[shaders::kiWaterFoamTextureCount] {};
	Texture* mppWaterFoamBreakupTextures[shaders::kiWaterFoamBreakupTextureCount] {};

	// Spread pipelines [pass]: radial directional spread, fragment shader with MRT
	Pipeline mpSpreadPipelines[shaders::kiMaxSpreadPasses];
	// Owns the spread pipeline name strings (PipelineInfo::name is a string_view that must outlive the Create call;
	// mirrors mShadowPipelineNames in DynamicPipelines)
	std::string mpSpreadPipelineNames[shaders::kiMaxSpreadPasses];

	// Combine pipeline (tone map accumulate → UNORM, all 3 colors in one dispatch)
	Pipeline mCombinePipeline;

	// Temporal-accumulation pass after combine: reprojects + EMA-blends the 3 directional combine outputs in place against history
	Pipeline mLightingTemporalPipeline;

	// Publishes temporal-blended directional combine outputs into distinct persistent history images and derives the ambient combine target.
	Pipeline mLightingHistoryCopyPipeline;

	DynamicPipelines mDynamicPipelines = DynamicPipelines(mShaders);
};

inline PipelineManager* gpPipelineManager = nullptr;

} // namespace engine

#endif // defined(BT_CLIENT)
