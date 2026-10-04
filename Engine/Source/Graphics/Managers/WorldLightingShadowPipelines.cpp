#if defined(BT_CLIENT)

#include "Graphics/Managers/WorldLightingShadowPipelines.h"

#include "Data/Shader.h"
#include "Data/Texture.h"
#include "Graphics/Managers/PipelineManager.h"

namespace engine
{

WorldLightingShadowPipelines::WorldLightingShadowPipelines(std::unordered_map<common::crc_t, Shader>& rShaders, Pipeline* pPipelines, Pipeline* pSpreadPipelines, std::string* pSpreadPipelineNames, Pipeline& rCombinePipeline, Pipeline& rLightingTemporalPipeline, Pipeline& rLightingHistoryCopyPipeline, Texture** ppWaterNormalTextures)
: mrShaders(rShaders)
, mpPipelines(pPipelines)
, mpSpreadPipelines(pSpreadPipelines)
, mpSpreadPipelineNames(pSpreadPipelineNames)
, mrCombinePipeline(rCombinePipeline)
, mrLightingTemporalPipeline(rLightingTemporalPipeline)
, mrLightingHistoryCopyPipeline(rLightingHistoryCopyPipeline)
, mppWaterNormalTextures(ppWaterNormalTextures)
{
}

void WorldLightingShadowPipelines::CreateLightingPipelines()
{
	RenderTargetTextures& rTextures = gpTextureManager->mRenderTargetTextures;

	// Spread pipelines (radial directional spread, fragment shader with MRT)
	// Pass 0 reads deposit textures, passes 1+ read previous pass spread textures
	// kIndirectHostVisible gives each pass a per-framebuffer VkDrawIndexedIndirectCommand slot so the refresh
	// predicate can suppress the chain without re-recording the Main CB. The flag also defers the Pipeline::Create
	// texture request to the first WriteIndirectBuffer with instances, which is a no-op here: these pipelines bind
	// only render-target textures, never disk-loaded chunks.
	for (int64_t i = 0; i < shaders::kiMaxSpreadPasses; ++i)
	{
		mpSpreadPipelineNames[i] = std::format("LightingSpread{}", i);
		mpSpreadPipelines[i].Create(
		{
			.name = mpSpreadPipelineNames[i],
			.flags = {PipelineFlags::kRenderTarget, PipelineFlags::kPushConstants, PipelineFlags::kIndirectHostVisible},
			.ppShaders = {&mrShaders.at(data::kShadersQuadsQuadsFullscreenvertCrc), &mrShaders.at(data::kShadersLightingLightingSpreadfragCrc)},
			.pVertexBuffer = &gpBufferManager->mQuadsVertexBuffer,
			.vkTargetRenderPass = rTextures.mSpreadVkRenderPass,
			.vkExtent3D = rTextures.mpSpreadTextures[i][0].mInfo.vkExtent3D,
			.iColorAttachmentCount = 6,
			.descriptorInfos =
			{
				{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
				{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = i == 0 ? &rTextures.mpLightingTextures[0] : &rTextures.mpSpreadTextures[i - 1][0]},
				{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = i == 0 ? &rTextures.mpLightingTextures[1] : &rTextures.mpSpreadTextures[i - 1][1]},
				{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = i == 0 ? &rTextures.mpLightingTextures[2] : &rTextures.mpSpreadTextures[i - 1][2]},
				{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &rTextures.mTerrainElevationTexture},
			},
		});
	}

	// Combine pipeline (tone map spread float16 → UNORM, all 3 colors)
	Texture* ppSpreadRed[shaders::kiMaxSpreadPasses] {};
	Texture* ppSpreadGreen[shaders::kiMaxSpreadPasses] {};
	Texture* ppSpreadBlue[shaders::kiMaxSpreadPasses] {};
	for (int64_t i = 0; i < shaders::kiMaxSpreadPasses; ++i)
	{
		ppSpreadRed[i] = &rTextures.mpSpreadOnlyTextures[i][0];
		ppSpreadGreen[i] = &rTextures.mpSpreadOnlyTextures[i][1];
		ppSpreadBlue[i] = &rTextures.mpSpreadOnlyTextures[i][2];
	}
	mrCombinePipeline.Create(
	{
		.name = "LightCombine",
		.flags = {PipelineFlags::kCompute, PipelineFlags::kIndirectHostVisible},
		.ppShaders = {&mrShaders.at(data::kShadersLightingLightCombinecompCrc)},
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
			{.flags = DescriptorFlags::kCombinedSamplers, .iCount = shaders::kiMaxSpreadPasses, .ppTextures = ppSpreadRed},
			{.flags = DescriptorFlags::kCombinedSamplers, .iCount = shaders::kiMaxSpreadPasses, .ppTextures = ppSpreadGreen},
			{.flags = DescriptorFlags::kCombinedSamplers, .iCount = shaders::kiMaxSpreadPasses, .ppTextures = ppSpreadBlue},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &rTextures.mpCombineTextures[0]},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &rTextures.mpCombineTextures[1]},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &rTextures.mpCombineTextures[2]},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &rTextures.mAmbientCombineTexture},
		},
	});

	// Temporal pass: 4 history samplers + the 4 combine outputs (read-write storage images), reprojected and
	// EMA-blended in place. No push constants — the shader reads the combine extent via imageSize().
	mrLightingTemporalPipeline.Create(
	{
		.name = "LightingTemporal",
		.flags = {PipelineFlags::kCompute, PipelineFlags::kIndirectHostVisible},
		.ppShaders = {&mrShaders.at(data::kShadersLightingLightingTemporalcompCrc)},
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
			{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &rTextures.mpLightingHistoryTextures[0]},
			{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &rTextures.mpLightingHistoryTextures[1]},
			{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &rTextures.mpLightingHistoryTextures[2]},
			{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &rTextures.mAmbientHistoryTexture},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &rTextures.mpCombineTextures[0]},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &rTextures.mpCombineTextures[1]},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &rTextures.mpCombineTextures[2]},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &rTextures.mAmbientCombineTexture},
		},
	});

	mrLightingHistoryCopyPipeline.Create(
	{
		.name = "LightingHistoryCopy",
		.flags = {PipelineFlags::kCompute, PipelineFlags::kIndirectHostVisible},
		.ppShaders = {&mrShaders.at(data::kShadersLightingLightingHistoryCopycompCrc)},
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &rTextures.mpCombineTextures[0]},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &rTextures.mpCombineTextures[1]},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &rTextures.mpCombineTextures[2]},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &rTextures.mAmbientCombineTexture},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &rTextures.mpLightingHistoryTextures[0]},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &rTextures.mpLightingHistoryTextures[1]},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &rTextures.mpLightingHistoryTextures[2]},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &rTextures.mAmbientHistoryTexture},
		},
	});
}

void WorldLightingShadowPipelines::CreatePipelineShadows()
{
	mpPipelines[kPipelineShadowElevation].Create(
	{
		.name = "ShadowElevation",
		// kMax: islands' bounding rectangles may overlap (chain packs by hull); MAX-blend the per-island
		// heightmaps so the tallest terrain wins per pixel instead of last-draw-wins. RTT clears to
		// mfSeaFloorElevation (the shared ocean floor, the lowest any heightmap reaches), so single-island
		// pixels are unchanged (max(floor, v) == v).
		.flags = {PipelineFlags::kRenderTarget, PipelineFlags::kPushConstants, PipelineFlags::kMax, PipelineFlags::kUpdateAfterBind},
		.ppShaders = {&mrShaders.at(data::kShadersQuadsQuadsAxisAlignedVisibleAreavertCrc), &mrShaders.at(data::kShadersTerrainTerrainElevationfragCrc)},
		.pVertexBuffer = &gpBufferManager->mQuadsVertexBuffer,
		.vkTargetRenderPass = gpTextureManager->mRenderTargetTextures.mShadowElevationTexture.mVkRenderPass,
		.vkExtent3D = gpTextureManager->mRenderTargetTextures.mShadowElevationTexture.mInfo.vkExtent3D,
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
			{.flags = DescriptorFlags::kPerCommandBufferStorageBuffers, .pBuffers = gpIslands->mIslandsStorageBuffers.data()},
			// The unflagged default keeps this offscreen shadow prepass linear without player-facing anisotropy.
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kBindlessArrayConsumer}, .iCount = shaders::kiMaxIslands, .ppTextures = gpTextureManager->mRenderTargetTextures.mElevationTextures.data()}, // set=1 binding 2 (elevation)
		},
	});

	mpPipelines[kPipelineShadow].Create(
	{
		.name = "Shadow",
		.flags = {PipelineFlags::kCompute},
		.ppShaders = {&mrShaders.at(data::kShadersShadowShadowcompCrc)},
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
			{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &gpTextureManager->mRenderTargetTextures.mShadowElevationTexture},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &gpTextureManager->mRenderTargetTextures.mShadowTexture},
		},
	});

	mpPipelines[kPipelineShadowBlurH].Create(
	{
		.name = "ShadowBlurH",
		.flags = {PipelineFlags::kCompute},
		.ppShaders = {&mrShaders.at(data::kShadersShadowShadowBlurHcompCrc)},
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
			{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &gpTextureManager->mRenderTargetTextures.mShadowTexture},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &gpTextureManager->mRenderTargetTextures.mShadowBlurIntermediateTexture},
		},
	});

	mpPipelines[kPipelineShadowBlurV].Create(
	{
		.name = "ShadowBlurV",
		.flags = {PipelineFlags::kCompute},
		.ppShaders = {&mrShaders.at(data::kShadersShadowShadowBlurVcompCrc)},
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
			{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &gpTextureManager->mRenderTargetTextures.mShadowBlurIntermediateTexture},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &gpTextureManager->mRenderTargetTextures.mShadowBlurTexture},
		},
	});

	mpPipelines[kPipelineObjectShadowsBlurH].Create(
	{
		.name = "ObjectShadowsBlurH",
		.flags = {PipelineFlags::kCompute},
		.ppShaders = {&mrShaders.at(data::kShadersShadowObjectShadowsBlurHcompCrc)},
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
			{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &gpTextureManager->mRenderTargetTextures.mObjectShadowsTexture},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &gpTextureManager->mRenderTargetTextures.mObjectShadowsBlurIntermediateTexture},
		},
	});

	mpPipelines[kPipelineObjectShadowsBlurV].Create(
	{
		.name = "ObjectShadowsBlurV",
		.flags = {PipelineFlags::kCompute},
		.ppShaders = {&mrShaders.at(data::kShadersShadowObjectShadowsBlurVcompCrc)},
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
			{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &gpTextureManager->mRenderTargetTextures.mObjectShadowsBlurIntermediateTexture},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &gpTextureManager->mRenderTargetTextures.mObjectShadowsBlurTexture},
		},
	});

	// Temporal accumulation: gather the reprojected previous-frame shadow and blend it in place into
	// mShadowBlurTexture. ShadowHistoryCopy then refreshes the distinct history image over the final window.
	mpPipelines[kPipelineShadowTemporal].Create(
	{
		.name = "ShadowTemporal",
		.flags = {PipelineFlags::kCompute},
		.ppShaders = {&mrShaders.at(data::kShadersShadowShadowTemporalcompCrc)},
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
			{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &gpTextureManager->mRenderTargetTextures.mShadowHistoryTexture},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &gpTextureManager->mRenderTargetTextures.mShadowBlurTexture},
		},
	});

	mpPipelines[kPipelineShadowHistoryCopy].Create(
	{
		.name = "ShadowHistoryCopy",
		.flags = {PipelineFlags::kCompute},
		.ppShaders = {&mrShaders.at(data::kShadersShadowShadowHistoryCopycompCrc)},
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &gpTextureManager->mRenderTargetTextures.mShadowBlurTexture},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &gpTextureManager->mRenderTargetTextures.mShadowHistoryTexture},
		},
	});
}

void WorldLightingShadowPipelines::CreateLightingBlurPipelines()
{
	// Lighting texture pre-blur pipelines (descriptors rebound per-texture at blur time)
	mpPipelines[kPipelineLightingBlurH].Create(
	{
		.name = "LightingBlurH",
		.flags = {PipelineFlags::kCompute, PipelineFlags::kPushConstants},
		.ppShaders = {&mrShaders.at(data::kShadersLightingLightingBlurHcompCrc)},
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &gpTextureManager->mWhiteTexture},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &gpTextureManager->mRenderTargetTextures.mpCombineTextures[0]},
		},
	});

	mpPipelines[kPipelineLightingBlurV].Create(
	{
		.name = "LightingBlurV",
		.flags = {PipelineFlags::kCompute, PipelineFlags::kPushConstants},
		.ppShaders = {&mrShaders.at(data::kShadersLightingLightingBlurVcompCrc)},
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &gpTextureManager->mWhiteTexture},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &gpTextureManager->mRenderTargetTextures.mpCombineTextures[0]},
		},
	});
}

void WorldLightingShadowPipelines::CreateLightingShadowDependentPipelines()
{
	// Resolve the water normal atlas Texture* pointers from kpWaterNormalCrcs. mTextureMap (unordered_map) is pointer-stable.
	for (int64_t i = 0; i < TextureManager::kiWaterNormalCount; ++i)
	{
		mppWaterNormalTextures[i] = &gpTextureManager->mTextureMap.at(TextureManager::kpWaterNormalCrcs[i]);
	}

	mpPipelines[kPipelineTerrain].Create(
	{
		.name = "Terrain",
		// Terrain records one vkCmdDrawIndexedIndirect per island template in CommandBufferRecordMain from Islands' per-template indirect buffers.
		.flags = {PipelineFlags::kDepthTest, PipelineFlags::kDepthWrite, PipelineFlags::kCullBack, PipelineFlags::kUpdateAfterBind},
		.ppShaders = {&mrShaders.at(data::kShadersTerrainTerrainvertCrc), &mrShaders.at(data::kShadersTerrainTerrainfragCrc)},
		// pVertexBuffer is null: vertex buffer is per-island and bound at draw time. Vertex input
		// stride and attribute layout come from shader reflection (Terrain.vert declares vec2 f2InPosition).
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
			{.flags = DescriptorFlags::kMainLayoutUniformBuffers},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerBorder}, .iCount = static_cast<int64_t>(std::size(gpTextureManager->mRenderTargetTextures.mppLightingFinalTextures)), .ppTextures = gpTextureManager->mRenderTargetTextures.mppLightingFinalTextures},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerBorderWhite}, .pTexture = &gpTextureManager->mRenderTargetTextures.mShadowBlurTexture},
			{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &gpTextureManager->mRenderTargetTextures.mObjectShadowsBlurTexture},
			{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &gpTextureManager->mRenderTargetTextures.mTerrainElevationTexture},
			// Bindless per-island color / normal / AO arrays.
			// Compositing fragment shader indexes these with the per-instance `uiTextureSlot` forwarded
			// from Terrain.vert; `kSamplerClamp` matches the per-slot RegisterTextureBinding flag in
			// IslandTerrainResidency.cpp so descriptor writes line up with the sampler descriptor layout.
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerClamp, DescriptorFlags::kBindlessArrayConsumer}, .iCount = shaders::kiMaxIslands, .ppTextures = gpTextureManager->mRenderTargetTextures.mColorTextures.data()}, // set=1 binding 6 (color)
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerClamp, DescriptorFlags::kBindlessArrayConsumer}, .iCount = shaders::kiMaxIslands, .ppTextures = gpTextureManager->mRenderTargetTextures.mNormalsTextures.data()}, // set=1 binding 7 (normals)
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerClamp, DescriptorFlags::kBindlessArrayConsumer}, .iCount = shaders::kiMaxIslands, .ppTextures = gpTextureManager->mRenderTargetTextures.mAmbientOcclusionTextures.data()}, // set=1 binding 8 (ambient occlusion)
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerSmoke}, .pTexture = &gpTextureManager->mRenderTargetTextures.mSmokeTextureOne},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerRepeat}, .textureCrc = data::kTexturesTerrainBC7Rock0jpgCrc},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerRepeat}, .textureCrc = data::kTexturesTerrainBC5SandNormal0jpgCrc},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerRepeat}, .textureCrc = data::kTexturesTerrainBC5SandNormal1pngCrc},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerRepeat}, .textureCrc = data::kTexturesTerrainBC5SandNormal2pngCrc},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerRepeat}, .textureCrc = data::kTexturesTerrainBC7SandpngCrc},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerRepeat}, .textureCrc = data::kTexturesTerrainBC5RockNormal1jpgCrc},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerRepeat}, .textureCrc = data::kTexturesTerrainBC5RockNormal2jpgCrc},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerRepeat}, .textureCrc = data::kTexturesTerrainBC5RockNormal4jpgCrc},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerBorder}, .pTexture = &gpTextureManager->mRenderTargetTextures.mAmbientCombineTexture},
			// AxisAlignedQuadLayout instance buffer used by Terrain.vert to transform island-local
			// mesh vertices into world space (set=1 binding=19). Mirrors kPipelineShadowElevation's
			// SSBO usage; gl_InstanceIndex is supplied per-island via firstInstance at draw time.
			{.flags = DescriptorFlags::kPerCommandBufferStorageBuffers, .pBuffers = gpIslands->mIslandsStorageBuffers.data()},
			// Bindless per-island material masks use set=1 binding=20; packed RGBA stores Rock/Sand/Snow/Flow. The masks follow the SSBO so fragment
			// bindings 9..18 and the Terrain.vert SSBO at 19 stay fixed.
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerClamp, DescriptorFlags::kBindlessArrayConsumer}, .iCount = shaders::kiMaxIslands, .ppTextures = gpTextureManager->mRenderTargetTextures.mMasksTextures.data()}, // set=1 binding 20 (masks)
			// The R16_SFLOAT per-island heightmaps occupy set=1 binding=21 after masks at 20, preserving bindings 0..20.
			// Terrain.vert samples island-local UVs and sinks vertices below the island's own zero-out threshold to the sea floor,
			// preventing an overlapping island's MAX-composite height from lifting submerged mesh. Sharing the prepasses' array
			// lets per-slot RegisterTextureBinding and eviction update this binding.
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerElevation, DescriptorFlags::kBindlessArrayConsumer}, .iCount = shaders::kiMaxIslands, .ppTextures = gpTextureManager->mRenderTargetTextures.mElevationTextures.data()}, // set=1 binding 21 (Terrain.vert own-heightmap sink)
		},
	});

	// Compute Gerstner displacement and Jacobian normal once per frame into two RGBA16F textures sampled by
	// Water.vert (kPipelineWater) via texelFetch per vertex. MainUniforms writes the indirect dispatch dimensions
	// each frame through WriteIndirectComputeBuffer to cover the active LOD region; the shader also bounds-checks
	// each thread against iWaterActiveQuad*.
	mpPipelines[kPipelineWaterDisplacement].Create(
	{
		.name = "WaterDisplacement",
		.flags = {PipelineFlags::kCompute, PipelineFlags::kIndirectHostVisible},
		.ppShaders = {&mrShaders.at(data::kShadersWaterWaterDisplacementcompCrc)},
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
			{.flags = DescriptorFlags::kMainLayoutUniformBuffers},
			{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &gpTextureManager->mRenderTargetTextures.mTerrainElevationTexture},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &gpTextureManager->mRenderTargetTextures.mWaterDisplacementTexture},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &gpTextureManager->mRenderTargetTextures.mWaterDisplacementNormalTexture},
		},
	});

	// Water. All three skybox-specular lobes are evaluated inline with analytic specular AA
	// (WATER_SPEC_AA_MODE in Water.frag) instead of MSAA sample shading — kSampleShading stays off.
	mpPipelines[kPipelineWater].Create(
	{
		.name = "Water",
		.flags = {PipelineFlags::kAlphaBlend, PipelineFlags::kCullBack, PipelineFlags::kDepthTest, PipelineFlags::kDepthWrite, PipelineFlags::kDepthBias, PipelineFlags::kUpdateAfterBind, PipelineFlags::kIndirectHostVisible},
		.ppShaders = {&mrShaders.at(data::kShadersWaterWatervertCrc), &mrShaders.at(data::kShadersWaterWaterfragCrc)},
		.pVertexBuffer = &gpBufferManager->mWaterMeshBuffer,
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
			{.flags = DescriptorFlags::kMainLayoutUniformBuffers},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerBorder}, .iCount = static_cast<int64_t>(std::size(gpTextureManager->mRenderTargetTextures.mppLightingFinalTextures)), .ppTextures = gpTextureManager->mRenderTargetTextures.mppLightingFinalTextures},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerBorderWhite}, .pTexture = &gpTextureManager->mRenderTargetTextures.mShadowBlurTexture},
			{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &gpTextureManager->mRenderTargetTextures.mObjectShadowsBlurTexture},
			{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &gpTextureManager->mRenderTargetTextures.mTerrainElevationTexture},
			{.flags = DescriptorFlags::kCombinedSamplers, .textureCrc = TextureManager::kPrefilteredWaterCrc},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerRepeat}, .textureCrc = data::kTexturesWaterBC4NoisepngCrc},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerMirroredRepeatWater}, .iCount = TextureManager::kiWaterNormalCount, .ppTextures = mppWaterNormalTextures},
			{.flags = DescriptorFlags::kCombinedSamplers, .textureCrc = data::kTexturesWaterDepthLutpngCrc},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerSmoke}, .pTexture = &gpTextureManager->mRenderTargetTextures.mSmokeTextureOne},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerBorder}, .pTexture = &gpTextureManager->mRenderTargetTextures.mAmbientCombineTexture},
			// Compute-pre-computed Gerstner displacement + normal sampled in Water.vert via texelFetch.
			// Explicit bindings keep the shader-side numbers at 13/14 — binding 12 is intentionally
			// unused.
			{.flags = DescriptorFlags::kCombinedSamplers, .iExplicitBinding = shaders::kiWaterBindingDisplacement, .pTexture = &gpTextureManager->mRenderTargetTextures.mWaterDisplacementTexture},
			{.flags = DescriptorFlags::kCombinedSamplers, .iExplicitBinding = shaders::kiWaterBindingDisplacementNormal, .pTexture = &gpTextureManager->mRenderTargetTextures.mWaterDisplacementNormalTexture},
		},
	});
}

} // namespace engine

#endif // defined(BT_CLIENT)
