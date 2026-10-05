#if defined(BT_CLIENT)

#include "Graphics/Managers/DynamicPipelines.h"

#include "Data/Model.h"
#include "Data/Shader.h"
#include "Data/Texture.h"
#include "File/PackChunks.h"

namespace engine
{

DynamicPipelines::DynamicPipelines(std::unordered_map<common::crc_t, Shader>& rShaders)
: mrShaders(rShaders)
{
}

// Resolves a model scene CRC to its model buffer + animation-aware vertex shader CRC (shared by the model + model-shadow pipeline creators).
static void ResolveModelChunkShaders(common::crc_t sceneCrc, Buffer*& rpModelBuffer, common::crc_t& rVertexShaderCrc)
{
	const std::unordered_map<common::crc_t, EagerChunk>& rChunkMap = gpFileManager->mpPackChunks->GetEagerChunkMap();
	auto sceneIt = rChunkMap.find(sceneCrc);
	if (sceneIt == rChunkMap.end())
	{
		throw std::ios_base::failure("DynamicPipelines scene");
	}

	if (!(sceneIt->second.pHeader->flags & common::ChunkFlags::kScene))
	{
		throw std::ios_base::failure("DynamicPipelines scene");
	}

	const common::SceneHeader& rSceneHeader = sceneIt->second.pHeader->sceneHeader;
	auto modelIt = gpBufferManager->mModelMap.find(rSceneHeader.modelCrc);
	if (modelIt == gpBufferManager->mModelMap.end())
	{
		throw std::ios_base::failure("DynamicPipelines scene");
	}

	rpModelBuffer = &modelIt->second;
	rVertexShaderCrc = rSceneHeader.bHasAnimation ? data::kShadersModelModelSkinnedvertCrc : data::kShadersModelModelStaticvertCrc;
}

void DynamicPipelines::AddPipeline(DynamicPipelineType eType, common::crc_t crc, const PipelineInfo& rPipelineInfo)
{
	Pipeline* pPipeline = mPipelines.emplace_back(std::make_unique<Pipeline>()).get();
	pPipeline->Create(rPipelineInfo);
	mPipelineMaps[eType].insert_or_assign(crc, pPipeline);
}

ModelPipeline* DynamicPipelines::CreateModelPipeline(const ModelPipelineSpec& rModelPipelineSpec)
{
	std::unique_ptr<ModelPipeline> pModelPipeline = std::make_unique<ModelPipeline>();
	pModelPipeline->Create(rModelPipelineSpec.sceneCrc, rModelPipelineSpec.pipelineInfo, rModelPipelineSpec.bIsPipelineShadow);

	ModelPipeline* pResult = pModelPipeline.get();
	mModelPipelines.push_back(std::move(pModelPipeline));

	return pResult;
}

void DynamicPipelines::CreateModelPipeline(common::crc_t crc, std::string_view name, common::crc_t sceneCrc, Buffer* pStorageBuffers)
{
	if (mModelPipelineMaps[kDynamicModelPipelineModel].contains(crc))
	{
		return;
	}

	// Trust boundary: ResolveModelChunkShaders validates the pack-derived scene kind and model reference before
	// PipelineInfo consumes them; ModelPipeline::Create validates scene-header counts and material ranges. This
	// boot-required model pipeline logs kError and propagates to ProcessMain's try/catch (HandleException — crash
	// report + exit), matching the boot hard-fail tier.
	try
	{
		Buffer* pModelBuffer = nullptr;
		common::crc_t vertexShaderCrc = 0;
		ResolveModelChunkShaders(sceneCrc, pModelBuffer, vertexShaderCrc);

		ModelPipeline* pPipeline = CreateModelPipeline(
		{
			.sceneCrc = sceneCrc,
			.pipelineInfo =
			{
				.name = name,
				.flags = {PipelineFlags::kIndirectHostVisible, PipelineFlags::kPushConstants, PipelineFlags::kDepthTest, PipelineFlags::kDepthWrite, PipelineFlags::kCullBack, PipelineFlags::kSampleShading, PipelineFlags::kUpdateAfterBind, PipelineFlags::kMultiSet},
				.ppShaders = {&mrShaders.at(vertexShaderCrc), &mrShaders.at(data::kShadersModelModelfragCrc)},
				.pVertexBuffer = pModelBuffer,
				.descriptorInfos =
				{
					{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
					{.flags = DescriptorFlags::kMainLayoutUniformBuffers},
					{.flags = DescriptorFlags::kPerCommandBufferStorageBuffers, .pBuffers = pStorageBuffers},
				},
			},
			.bIsPipelineShadow = false,
		});

		mModelPipelineMaps[kDynamicModelPipelineModel].insert_or_assign(crc, pPipeline);
	}
	catch (const std::ios_base::failure& rException)
	{
		char pcHex[20] {};
		LOG(kLoading, kError, "Corrupt scene chunk for {}model pipeline \"{}\" (scene CRC {}): {}", "", name, common::ToHex(std::span(pcHex), sceneCrc), rException.what());
		throw;
	}
}

void DynamicPipelines::CreateModelPipelineShadow(common::crc_t crc, std::string_view name, common::crc_t sceneCrc, Buffer* pStorageBuffers)
{
	if (mModelPipelineMaps[kDynamicModelPipelineModelShadow].contains(crc))
	{
		return;
	}

	// Trust boundary: ResolveModelChunkShaders validates the pack-derived scene kind and model reference before
	// PipelineInfo consumes them; ModelPipeline::Create validates scene-header counts and material ranges. This
	// boot-required model pipeline logs kError and propagates to ProcessMain's try/catch (HandleException — crash
	// report + exit), matching the boot hard-fail tier.
	std::string_view pipelineName = name;
	try
	{
		Buffer* pModelBuffer = nullptr;
		common::crc_t vertexShaderCrc = 0;
		ResolveModelChunkShaders(sceneCrc, pModelBuffer, vertexShaderCrc);

		// Create shadow variant of pipeline name (stored in map to outlive this function)
		std::string& rShadowName = mShadowPipelineNames.insert_or_assign(crc, std::string(name) + "Shadow").first->second;
		pipelineName = rShadowName;

		ModelPipeline* pPipelineShadow = CreateModelPipeline(
		{
			.sceneCrc = sceneCrc,
			.pipelineInfo =
			{
				.name = rShadowName,
				.flags = {PipelineFlags::kRenderTarget, PipelineFlags::kIndirectHostVisible, PipelineFlags::kPushConstants, PipelineFlags::kUpdateAfterBind},
				.ppShaders = {&mrShaders.at(vertexShaderCrc), &mrShaders.at(data::kShadersModelModelShadowfragCrc)},
				.pVertexBuffer = pModelBuffer,
				.vkTargetRenderPass = gpTextureManager->mRenderTargetTextures.mObjectShadowsTexture.mVkRenderPass,
				.vkExtent3D = gpTextureManager->mRenderTargetTextures.mObjectShadowsTexture.mInfo.vkExtent3D,
				.descriptorInfos =
				{
					{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
					{.flags = DescriptorFlags::kMainLayoutUniformBuffers},
					{.flags = DescriptorFlags::kPerCommandBufferStorageBuffers, .pBuffers = pStorageBuffers},
				},
			},
			.bIsPipelineShadow = true,
		});

		mModelPipelineMaps[kDynamicModelPipelineModelShadow].insert_or_assign(crc, pPipelineShadow);
	}
	catch (const std::ios_base::failure& rException)
	{
		char pcHex[20] {};
		LOG(kLoading, kError, "Corrupt scene chunk for {}model pipeline \"{}\" (scene CRC {}): {}", "shadow ", pipelineName, common::ToHex(std::span(pcHex), sceneCrc), rException.what());
		throw;
	}
}

void DynamicPipelines::CreateAreaLightingPipeline(DynamicPipelineType eType, common::crc_t crc, std::string_view name, int64_t iBufferSize, common::crc_t vertexShaderCrc, common::crc_t fragmentShaderCrc, DescriptorFlags eSamplerFlag)
{
	if (mPipelineMaps[eType].contains(crc))
	{
		return;
	}

	gpBufferManager->CreateDynamicBuffer(crc, kBufferMain, name, iBufferSize);

	AddPipeline(eType, crc,
	{
		.name = name,
		.flags = {PipelineFlags::kRenderTarget, PipelineFlags::kPushConstants, PipelineFlags::kIndirectHostVisible, PipelineFlags::kMax, PipelineFlags::kUpdateAfterBind},
		.ppShaders = {&mrShaders.at(vertexShaderCrc), &mrShaders.at(fragmentShaderCrc)},
		.pVertexBuffer = &gpBufferManager->mQuadsVertexBuffer,
		.vkTargetRenderPass = gpTextureManager->mRenderTargetTextures.mLightingVkRenderPass,
		.vkExtent3D = gpTextureManager->mRenderTargetTextures.mpLightingTextures[0].mInfo.vkExtent3D,
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
			{.flags = DescriptorFlags::kPerCommandBufferStorageBuffers, .pBuffers = gpBufferManager->mDynamicStorageBuffers[kBufferMain].at(crc).data()},
			{.flags = eSamplerFlag},
			{.flags = DescriptorFlags::kTextures},
		},
	});
}

void DynamicPipelines::CreatePipelineVisibleLights(common::crc_t crc, std::string_view name, Buffer* pStorageBuffers)
{
	if (mPipelineMaps[kDynamicPipelineVisibleLights].contains(crc))
	{
		return;
	}

	AddPipeline(kDynamicPipelineVisibleLights, crc,
	{
		.name = name,
		.flags = {PipelineFlags::kIndirectHostVisible, PipelineFlags::kAddAlpha, PipelineFlags::kSampleShading, PipelineFlags::kUpdateAfterBind},
		.ppShaders = {&mrShaders.at(data::kShadersLightingVisibleLightvertCrc), &mrShaders.at(data::kShadersLightingVisibleLightfragCrc)},
		.pVertexBuffer = &gpBufferManager->mQuadsVertexBuffer,
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
			{.flags = DescriptorFlags::kMainLayoutUniformBuffers},
			{.flags = DescriptorFlags::kPerCommandBufferStorageBuffers, .pBuffers = pStorageBuffers},
			{.flags = DescriptorFlags::kSamplerRepeat},
			{.flags = DescriptorFlags::kTextures},
			{.flags = DescriptorFlags::kCombinedSamplers, .pTexture = &gpTextureManager->mRenderTargetTextures.mTerrainElevationTexture},
		},
	});
}

void DynamicPipelines::CreatePipelineBillboards(common::crc_t crc, std::string_view name, int64_t iBufferSize)
{
	if (mPipelineMaps[kDynamicPipelineBillboards].contains(crc))
	{
		return;
	}

	gpBufferManager->CreateDynamicBuffer(crc, kBufferMain, name, iBufferSize);

	AddPipeline(kDynamicPipelineBillboards, crc,
	{
		.name = name,
		.flags = {PipelineFlags::kIndirectHostVisible, PipelineFlags::kSampleShading, PipelineFlags::kAlphaBlend, PipelineFlags::kUpdateAfterBind},
		.ppShaders = {&mrShaders.at(data::kShadersParticlesBillboardsvertCrc), &mrShaders.at(data::kShadersParticlesBillboardsfragCrc)},
		.pVertexBuffer = &gpBufferManager->mQuadsVertexBuffer,
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
			{.flags = DescriptorFlags::kMainLayoutUniformBuffers},
			{.flags = DescriptorFlags::kPerCommandBufferStorageBuffers, .pBuffers = gpBufferManager->mDynamicStorageBuffers[kBufferMain].at(crc).data()},
			{.flags = DescriptorFlags::kSamplerClamp},
			{.flags = DescriptorFlags::kTextures},
		},
	});
}

void DynamicPipelines::CreateDepositPipeline(DynamicPipelineType eType, common::crc_t crc, std::string_view name, common::crc_t vertexShaderCrc, common::crc_t fragmentShaderCrc, const Texture& rTargetTexture, const DescriptorInfo& rTextureDescriptor, VkBuffer* pVkOccupancyBuffer, int64_t iBufferSize)
{
	if (mPipelineMaps[eType].contains(crc))
	{
		return;
	}

	if (iBufferSize > 0)
	{
		gpBufferManager->CreateDynamicBuffer(crc, kBufferMain, name, iBufferSize);
	}

	AddPipeline(eType, crc,
	{
		.name = name,
		.flags = {PipelineFlags::kRenderTarget, PipelineFlags::kPushConstants, PipelineFlags::kIndirectHostVisible, PipelineFlags::kAdd, PipelineFlags::kUpdateAfterBind},
		.ppShaders = {&mrShaders.at(vertexShaderCrc), &mrShaders.at(fragmentShaderCrc)},
		.pVertexBuffer = &gpBufferManager->mQuadsVertexBuffer,
		.vkTargetRenderPass = rTargetTexture.mVkRenderPass,
		.vkExtent3D = rTargetTexture.mInfo.vkExtent3D,
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
			{.flags = DescriptorFlags::kPerCommandBufferStorageBuffers, .pBuffers = gpBufferManager->mDynamicStorageBuffers[kBufferMain].at(crc).data()},
			rTextureDescriptor,
			{.flags = DescriptorFlags::kStorageBuffer, .pVkBuffers = pVkOccupancyBuffer},
		},
	});
}

void DynamicPipelines::CreatePipelineHexShields(common::crc_t crc, std::string_view name, int64_t iBufferSize)
{
	if (mPipelineMaps[kDynamicPipelineHexShields].contains(crc))
	{
		return;
	}

	gpBufferManager->CreateDynamicBuffer(crc, kBufferMain, name, iBufferSize);

	AddPipeline(kDynamicPipelineHexShields, crc,
	{
		.name = name,
		.flags = {PipelineFlags::kIndirectHostVisible, PipelineFlags::kPushConstants, PipelineFlags::kAlphaBlend, PipelineFlags::kDepthTest, PipelineFlags::kCullBack, PipelineFlags::kUpdateAfterBind},
		.ppShaders = {&mrShaders.at(data::kShadersObjectsHexShieldvertCrc), &mrShaders.at(data::kShadersObjectsHexShieldfragCrc)},
		.pVertexBuffer = &gpBufferManager->mModelMap.at(data::kModelsDualGeodesicIcosahedronDualGeodesicIcosahedrongltfMODELCrc),
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
			{.flags = DescriptorFlags::kMainLayoutUniformBuffers},
			{.flags = DescriptorFlags::kPerCommandBufferStorageBuffers, .pBuffers = gpBufferManager->mDynamicStorageBuffers[kBufferMain].at(crc).data()},
			{.flags = DescriptorFlags::kCombinedSamplers, .textureCrc = data::kTexturesCSkyboxCrc},
		},
	});
}

void DynamicPipelines::CreatePipelineHexShieldsLighting(common::crc_t crc, std::string_view name)
{
	if (mPipelineMaps[kDynamicPipelineHexShieldsLighting].contains(crc))
	{
		return;
	}

	// Shares the main HexShields pipeline's storage buffer.
	AddPipeline(kDynamicPipelineHexShieldsLighting, crc,
	{
		.name = name,
		.flags = {PipelineFlags::kRenderTarget, PipelineFlags::kPushConstants, PipelineFlags::kMax, PipelineFlags::kIndirectHostVisible, PipelineFlags::kUpdateAfterBind},
		.ppShaders = {&mrShaders.at(data::kShadersObjectsHexShieldvertCrc), &mrShaders.at(data::kShadersObjectsHexShieldLightingfragCrc)},
		.pVertexBuffer = &gpBufferManager->mModelMap.at(data::kModelsDualGeodesicIcosahedronDualGeodesicIcosahedrongltfMODELCrc),
		.vkTargetRenderPass = gpTextureManager->mRenderTargetTextures.mLightingVkRenderPass,
		.vkExtent3D = gpTextureManager->mRenderTargetTextures.mpLightingTextures[0].mInfo.vkExtent3D,
		.descriptorInfos =
		{
			{.flags = DescriptorFlags::kGlobalLayoutUniformBuffers},
			{.flags = DescriptorFlags::kMainLayoutUniformBuffers},
			{.flags = DescriptorFlags::kPerCommandBufferStorageBuffers, .pBuffers = gpBufferManager->mDynamicStorageBuffers[kBufferMain].at(crc).data()},
		},
	});
}

void DynamicPipelines::UpdateAllModelPipelineDescriptors(int64_t iCommandBuffer, int64_t iBinding, Buffer* pBuffer)
{
	for (const auto& [rCrc, rpPipeline] : mModelPipelineMaps[kDynamicModelPipelineModel])
	{
		rpPipeline->UpdateStorageBufferDescriptors(iCommandBuffer, iBinding, pBuffer);
	}
	for (const auto& [rCrc, rpPipeline] : mModelPipelineMaps[kDynamicModelPipelineModelShadow])
	{
		rpPipeline->UpdateStorageBufferDescriptors(iCommandBuffer, iBinding, pBuffer);
	}
}

} // namespace engine

#endif // defined(BT_CLIENT)
