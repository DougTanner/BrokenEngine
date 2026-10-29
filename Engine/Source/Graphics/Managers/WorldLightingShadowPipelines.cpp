#if defined(BT_CLIENT)

#include "Graphics/Managers/WorldLightingShadowPipelines.h"

#include "Data/Shader.h"
#include "Data/Texture.h"
#include "Graphics/Managers/PipelineManager.h"

namespace engine
{

WorldLightingShadowPipelines::WorldLightingShadowPipelines(std::unordered_map<common::crc_t, Shader>& rShaders, Pipeline* pPipelines, Texture** ppWaterNormalTextures, Texture** ppWaterFoamTextures, Texture** ppWaterFoamBreakupTextures)
: mrShaders(rShaders)
, mpPipelines(pPipelines)
, mppWaterNormalTextures(ppWaterNormalTextures)
, mppWaterFoamTextures(ppWaterFoamTextures)
, mppWaterFoamBreakupTextures(ppWaterFoamBreakupTextures)
{
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
		.flags = {PipelineFlags::kRenderTarget, PipelineFlags::kPushConstants, PipelineFlags::kIndirectHostVisible, PipelineFlags::kMax, PipelineFlags::kUpdateAfterBind},
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
		.flags = {PipelineFlags::kCompute, PipelineFlags::kIndirectHostVisible},
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
		.flags = {PipelineFlags::kCompute, PipelineFlags::kIndirectHostVisible},
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
		.flags = {PipelineFlags::kCompute, PipelineFlags::kIndirectHostVisible},
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

	// Temporal accumulation: gather the reprojected last-refresh shadow and blend it in place into
	// mShadowBlurTexture. ShadowHistoryCopy then refreshes the distinct history image over the final window.
	mpPipelines[kPipelineShadowTemporal].Create(
	{
		.name = "ShadowTemporal",
		.flags = {PipelineFlags::kCompute, PipelineFlags::kIndirectHostVisible},
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
		.flags = {PipelineFlags::kCompute, PipelineFlags::kIndirectHostVisible},
		.ppShaders = {&mrShaders.at(data::kShadersShadowShadowHistoryCopycompCrc)},
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &gpTextureManager->mRenderTargetTextures.mShadowBlurTexture},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &gpTextureManager->mRenderTargetTextures.mShadowHistoryTexture},
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
	for (int64_t i = 0; i < TextureManager::kiWaterFoamTextureCount; ++i)
	{
		mppWaterFoamTextures[i] = &gpTextureManager->mTextureMap.at(TextureManager::kpWaterFoamCrcs[i]);
	}
	for (int64_t i = 0; i < TextureManager::kiWaterFoamBreakupTextureCount; ++i)
	{
		mppWaterFoamBreakupTextures[i] = &gpTextureManager->mTextureMap.at(TextureManager::kpWaterFoamBreakupCrcs[i]);
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
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerBorderWhite}, .pTexture = &gpTextureManager->mRenderTargetTextures.mShadowBlurTexture},
			{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &gpTextureManager->mRenderTargetTextures.mObjectShadowsBlurTexture},
			{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &gpTextureManager->mRenderTargetTextures.mTerrainElevationTexture},
			// Bindless per-island color / normal / AO arrays.
			// Compositing fragment shader indexes these with the per-instance `uiTextureSlot` forwarded
			// from Terrain.vert; `kSamplerClamp` matches the per-slot RegisterTextureBinding flag in
			// IslandTerrainResidency.cpp so descriptor writes line up with the sampler descriptor layout.
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerClamp, DescriptorFlags::kBindlessArrayConsumer}, .iCount = shaders::kiMaxIslands, .ppTextures = gpTextureManager->mRenderTargetTextures.mColorTextures.data()}, // set=1 binding 5 (color)
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerClamp, DescriptorFlags::kBindlessArrayConsumer}, .iCount = shaders::kiMaxIslands, .ppTextures = gpTextureManager->mRenderTargetTextures.mNormalsTextures.data()}, // set=1 binding 6 (normals)
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerClamp, DescriptorFlags::kBindlessArrayConsumer}, .iCount = shaders::kiMaxIslands, .ppTextures = gpTextureManager->mRenderTargetTextures.mAmbientOcclusionTextures.data()}, // set=1 binding 7 (ambient occlusion)
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerSmoke}, .iCount = shaders::kiMaxFlightPlanes, .ppTextures = gpTextureManager->mRenderTargetTextures.mppSmokeTexturesOne},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerRepeat}, .textureCrc = data::kTexturesTerrainBC7Rock0jpgCrc},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerRepeat}, .textureCrc = data::kTexturesTerrainBC5SandNormal0jpgCrc},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerRepeat}, .textureCrc = data::kTexturesTerrainBC5SandNormal1pngCrc},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerRepeat}, .textureCrc = data::kTexturesTerrainBC5SandNormal2pngCrc},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerRepeat}, .textureCrc = data::kTexturesTerrainBC7SandpngCrc},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerRepeat}, .textureCrc = data::kTexturesTerrainBC5RockNormal1jpgCrc},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerRepeat}, .textureCrc = data::kTexturesTerrainBC5RockNormal2jpgCrc},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerRepeat}, .textureCrc = data::kTexturesTerrainBC5RockNormal4jpgCrc},
			// AxisAlignedQuadLayout instance buffer used by Terrain.vert to transform island-local
			// mesh vertices into world space (set=1 binding=17). Mirrors kPipelineShadowElevation's
			// SSBO usage; gl_InstanceIndex is supplied per-island via firstInstance at draw time.
			{.flags = DescriptorFlags::kPerCommandBufferStorageBuffers, .pBuffers = gpIslands->mIslandsStorageBuffers.data()},
			// Bindless per-island material masks use set=1 binding=18; packed RGBA stores Rock/Sand/Snow/Flow. The masks follow the SSBO so fragment
			// bindings precede the Terrain.vert SSBO.
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerClamp, DescriptorFlags::kBindlessArrayConsumer}, .iCount = shaders::kiMaxIslands, .ppTextures = gpTextureManager->mRenderTargetTextures.mMasksTextures.data()}, // set=1 binding 18 (masks)
			// The R16_SFLOAT per-island heightmaps occupy set=1 binding=19 after masks at 18.
			// Terrain.vert samples island-local UVs and sinks vertices below the island's own zero-out threshold to the sea floor,
			// preventing an overlapping island's MAX-composite height from lifting submerged mesh. Sharing the prepasses' array
			// lets per-slot RegisterTextureBinding and eviction update this binding.
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerElevation, DescriptorFlags::kBindlessArrayConsumer}, .iCount = shaders::kiMaxIslands, .ppTextures = gpTextureManager->mRenderTargetTextures.mElevationTextures.data()}, // set=1 binding 19 (Terrain.vert own-heightmap sink)
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

	mpPipelines[kPipelineWaterFoam].Create(
	{
		.name = "WaterFoam",
		.flags = {PipelineFlags::kCompute},
		.ppShaders = {&mrShaders.at(data::kShadersWaterWaterFoamcompCrc)},
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
			{.flags = DescriptorFlags::kMainLayoutUniformBuffers},
			{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &gpTextureManager->mRenderTargetTextures.mTerrainElevationTexture},
			{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &gpTextureManager->mRenderTargetTextures.mWaterDisplacementNormalTexture},
			{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &gpTextureManager->mRenderTargetTextures.mWaterFoamHistoryTexture},
			{.flags = DescriptorFlags::kStorageImages, .pTexture = &gpTextureManager->mRenderTargetTextures.mWaterFoamCurrentTexture},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerMirroredRepeat}, .iExplicitBinding = shaders::kiWaterFoamBindingSpawnNoise, .textureCrc = data::kTexturesSmokeBC4tex_glass_0001_MKjpgCrc},
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
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerBorderWhite}, .pTexture = &gpTextureManager->mRenderTargetTextures.mShadowBlurTexture},
			{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &gpTextureManager->mRenderTargetTextures.mObjectShadowsBlurTexture},
			{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &gpTextureManager->mRenderTargetTextures.mTerrainElevationTexture},
			{.flags = DescriptorFlags::kCombinedSamplers, .textureCrc = TextureManager::kPrefilteredWaterCrc},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerRepeat}, .textureCrc = data::kTexturesWaterBC4NoisepngCrc},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerMirroredRepeatWater}, .iCount = TextureManager::kiWaterNormalCount, .ppTextures = mppWaterNormalTextures},
			{.flags = DescriptorFlags::kCombinedSamplers, .textureCrc = data::kTexturesWaterDepthLutpngCrc},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerSmoke}, .iCount = shaders::kiMaxFlightPlanes, .ppTextures = gpTextureManager->mRenderTargetTextures.mppSmokeTexturesOne},
			// Compute-pre-computed Gerstner displacement + normal sampled in Water.vert via texelFetch.
			// Explicit bindings keep the shader-side numbers at 13/14 — binding 12 is intentionally
			// unused.
			{.flags = DescriptorFlags::kCombinedSamplers, .iExplicitBinding = shaders::kiWaterBindingDisplacement, .pTexture = &gpTextureManager->mRenderTargetTextures.mWaterDisplacementTexture},
			{.flags = DescriptorFlags::kCombinedSamplers, .iExplicitBinding = shaders::kiWaterBindingDisplacementNormal, .pTexture = &gpTextureManager->mRenderTargetTextures.mWaterDisplacementNormalTexture},
			{.flags = DescriptorFlags::kCombinedSamplers, .iExplicitBinding = shaders::kiWaterBindingFoam, .pTexture = &gpTextureManager->mRenderTargetTextures.mWaterFoamHistoryTexture},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerMirroredRepeat}, .iCount = TextureManager::kiWaterFoamTextureCount, .iExplicitBinding = shaders::kiWaterBindingFoamColor, .ppTextures = mppWaterFoamTextures},
			{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerMirroredRepeat}, .iCount = TextureManager::kiWaterFoamBreakupTextureCount, .iExplicitBinding = shaders::kiWaterBindingFoamBreakup, .ppTextures = mppWaterFoamBreakupTextures},
		},
	});
}

} // namespace engine

#endif // defined(BT_CLIENT)
