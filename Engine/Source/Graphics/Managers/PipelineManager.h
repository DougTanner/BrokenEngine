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

	DynamicPipelines mDynamicPipelines = DynamicPipelines(mShaders);

	WorldLightingShadowPipelines mWorldLightingShadowPipelines = WorldLightingShadowPipelines(mShaders, mpPipelines, mppWaterNormalTextures, mppWaterFoamTextures, mppWaterFoamBreakupTextures);
};

inline PipelineManager* gpPipelineManager = nullptr;

} // namespace engine

#endif // defined(BT_CLIENT)
