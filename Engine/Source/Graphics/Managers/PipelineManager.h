#pragma once

#if defined(BT_CLIENT)

#include "DynamicPipelines.h"
#include "WorldLightingShadowPipelines.h"

namespace engine
{

// Per-plane smoke pipelines; PipelineManager::SmokePipeline maps (plane, kind) into the Pipelines smoke range.
enum SmokePipelineKind : int64_t
{
	kSmokeOccupancyDilate,
	kSmokeOccupancyDilateRemap,
	kSmokeSpreadComputeB,
	kSmokeSpreadComputeA,

	kSmokePipelineKindCount,
};

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
	kPipelineSmokeFirst,
	kPipelineSmokeLast = kPipelineSmokeFirst + kSmokePipelineKindCount * shaders::kiMaxFlightPlanes - 1,

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

	Pipeline& SmokePipeline(int64_t iPlane, SmokePipelineKind eKind);

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

	// Temporal-accumulation pass after combine: reprojects + EMA-blends the 4 combine outputs in place against history
	Pipeline mLightingTemporalPipeline;

	// Publishes the temporal-blended combine outputs into the distinct persistent history images.
	Pipeline mLightingHistoryCopyPipeline;

	DynamicPipelines mDynamicPipelines = DynamicPipelines(mShaders);

	WorldLightingShadowPipelines mWorldLightingShadowPipelines = WorldLightingShadowPipelines(mShaders, mpPipelines, mpSpreadPipelines, mpSpreadPipelineNames, mCombinePipeline, mLightingTemporalPipeline, mLightingHistoryCopyPipeline, mppWaterNormalTextures, mppWaterFoamTextures, mppWaterFoamBreakupTextures);
};

inline PipelineManager* gpPipelineManager = nullptr;

} // namespace engine

#endif // defined(BT_CLIENT)
