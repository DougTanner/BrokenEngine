#if defined(BT_CLIENT)

#include "ModelPipeline.h"

#include "File/PackChunks.h"
#include "PipelineDescriptorWriter.h"

namespace engine
{

void ModelPipeline::Create(common::crc_t sceneCrc, const PipelineInfo& rPipelineInfo, bool bIsShadow)
{
	PipelineInfo pipelineInfo = rPipelineInfo;

	pipelineInfo.descriptorInfos.insert(pipelineInfo.descriptorInfos.end(),
	{
		{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerRepeat}, .iExplicitBinding = shaders::kiModelBindingIrradiance, .textureCrc = TextureManager::kIrradianceCrc},
		{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerRepeat}, .iExplicitBinding = shaders::kiModelBindingPrefiltered, .textureCrc = TextureManager::kPrefilteredCrc},

		// Generated at runtime, not packed, so it binds through pTexture rather than a texture CRC.
		{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerRepeat}, .iExplicitBinding = shaders::kiModelBindingBrdfLookupTable, .pTexture = &gpTextureManager->mTextureCache.mPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionLookupTableTexture},

		{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerBorder}, .iCount = static_cast<int64_t>(std::size(gpTextureManager->mRenderTargetTextures.mppLightingFinalTextures)), .iExplicitBinding = shaders::kiModelBindingLighting, .ppTextures = gpTextureManager->mRenderTargetTextures.mppLightingFinalTextures},
		{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerBorderWhite}, .iExplicitBinding = shaders::kiModelBindingShadow, .pTexture = &gpTextureManager->mRenderTargetTextures.mShadowBlurTexture},

		// The smoke sampler (kSamplerSmoke) uses LINEAR filtering for R16_SFLOAT smoke ping-pong textures. Same CLAMP_TO_BORDER + INT_TRANSPARENT_BLACK as the border sampler; aniso is a no-op at mipLevels = 1.
		{.flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerSmoke}, .iCount = shaders::kiMaxFlightPlanes, .iExplicitBinding = shaders::kiModelBindingSmoke, .ppTextures = gpTextureManager->mRenderTargetTextures.mppSmokeTexturesOne},

		{.flags = DescriptorFlags::kCombinedSamplers, .iExplicitBinding = shaders::kiModelBindingObjectShadows, .pTexture = &gpTextureManager->mRenderTargetTextures.mObjectShadowsBlurTexture},

		// Per-mesh data (matrix, normal matrix, joint count/offset)
		{.flags = DescriptorFlags::kPerCommandBufferStorageBuffers, .iExplicitBinding = shaders::kiModelBindingMeshData, .pBuffers = gpBufferManager->mMeshDataStorageBuffers.data()},

		// Joint matrices (separate dynamically-sized buffer; MeshData stays fixed-size)
		{.flags = DescriptorFlags::kPerCommandBufferStorageBuffers, .iExplicitBinding = shaders::kiModelBindingJointMatrix, .pBuffers = gpBufferManager->mJointMatrixStorageBuffers.data()},

		{.flags = DescriptorFlags::kCombinedSamplers, .iExplicitBinding = shaders::kiModelBindingTerrainElevation, .pTexture = &gpTextureManager->mRenderTargetTextures.mTerrainElevationTexture},
	});

	// mMaterialsStorageBuffer is created below, before any material pipeline writes it.
	if (!bIsShadow)
	{
		pipelineInfo.descriptorInfos.push_back({.flags = DescriptorFlags::kStorageBuffer, .iExplicitBinding = shaders::kiModelBindingMaterials, .pBuffers = &mMaterialsStorageBuffer});
	}

	mSceneCrc = sceneCrc;

	const EagerChunk& rChunk = gpFileManager->mpPackChunks->GetEagerChunkMap().at(sceneCrc);
	const common::SceneHeader& rSceneHeader = rChunk.pHeader->sceneHeader;

	// Pack counts bound per-material allocation, loops and aliased texture/index offsets. Reject invalid counts
	// before use; draw and descriptor updates require a nonzero material count, with invalid input reported as
	// std::ios_base::failure.
	if (rSceneHeader.uiTextureCount > common::SceneHeader::kiMaxTextures || rSceneHeader.uiMaterialCount == 0
	 || rSceneHeader.uiMaterialCount > common::SceneHeader::kiMaxMaterials)
	{
		throw std::ios_base::failure("ModelPipeline::Create");
	}

	miMaterialCount = rSceneHeader.uiMaterialCount;

	// Right-size the per-material arrays to the actual count (Pipeline is non-movable, so assign a fresh vector rather than resize)
	mpPipelines = std::vector<Pipeline>(miMaterialCount);
	mpiIndexCounts.resize(miMaterialCount);
	mpiFirstIndices.resize(miMaterialCount);
	mpbTransparentMaterials.resize(miMaterialCount);

	int64_t iIndexStartsOffset = common::SceneHeader::IndexStartsOffset(rSceneHeader.uiTextureCount);
	int64_t iMaterialDataOffset = common::SceneHeader::MaterialDataOffset(rSceneHeader.uiTextureCount, rSceneHeader.uiMaterialCount);

	// Index-start and material arrays must fit ChunkHeader::iSize before dereference; structural count limits
	// keep extent arithmetic within int64_t.
	if (iMaterialDataOffset + rSceneHeader.uiMaterialCount * static_cast<int64_t>(sizeof(common::MaterialShaderData)) > rChunk.pHeader->iSize)
	{
		throw std::ios_base::failure("ModelPipeline::Create");
	}

	const uint32_t* puiIndexStarts = reinterpret_cast<const uint32_t*>(rChunk.pData + iIndexStartsOffset);
	const common::MaterialShaderData* pMaterials = reinterpret_cast<const common::MaterialShaderData*>(rChunk.pData + iMaterialDataOffset);
	PipelineFlags_t originalFlags = pipelineInfo.flags;
	int64_t iPreviousIndexStart = 0;
	for (int64_t i = 0; i < miMaterialCount; ++i)
	{
		int64_t iIndexStart = puiIndexStarts[i];
		if ((i > 0 && iIndexStart < iPreviousIndexStart) || iIndexStart > pipelineInfo.pVertexBuffer->mInfo.iCount)
		{
			throw std::ios_base::failure("ModelPipeline::Create");
		}
		iPreviousIndexStart = iIndexStart;
	}

	if (!bIsShadow)
	{
		mMaterialsStorageBuffer.Create(
		{
			.name = "Materials",
			.flags = {BufferFlags::kStorage, BufferFlags::kDeviceLocal},
			.iDataSize = miMaterialCount * static_cast<int64_t>(sizeof(shaders::PbrMaterialLayout)),
		}, [&](void* pData)
		{
			const common::crc_t* pTextureCrcs = reinterpret_cast<const common::crc_t*>(rChunk.pData);
			shaders::PbrMaterialLayout* pCurrent = static_cast<shaders::PbrMaterialLayout*>(pData);
			static constexpr int64_t kiOldMaterialSize = offsetof(shaders::PbrMaterialLayout, fColorTextureIndex);
			// A present texture selector's index is a pack trust boundary; an absent selector leaves its index unused.
			auto ResolveTextureIndex = [&](int64_t iTextureSet, int64_t iTextureIndex)
			{
				if (iTextureSet < 0)
				{
					return 0.0f;
				}
				if (iTextureIndex >= rSceneHeader.uiTextureCount)
				{
					throw std::ios_base::failure("ModelPipeline::Create material");
				}
				return static_cast<float>(gpTextureManager->mTextureDescriptors.CrcToIndex(pTextureCrcs[iTextureIndex]));
			};
			for (int64_t i = 0; i < miMaterialCount; ++i)
			{
				const common::MaterialShaderData& rMaterial = pMaterials[i];
				std::memcpy(pCurrent, &rMaterial.f4BaseColorFactor, kiOldMaterialSize);
				pCurrent->fColorTextureIndex = ResolveTextureIndex(rMaterial.iColorTextureSet, rMaterial.uiColorTextureIndex);
				pCurrent->fPhysicalDescriptorTextureIndex = ResolveTextureIndex(rMaterial.iPhysicalDescriptorTextureSet, rMaterial.uiPhysicalDescriptorTextureIndex);
				pCurrent->fNormalTextureIndex = ResolveTextureIndex(rMaterial.iNormalTextureSet, rMaterial.uiNormalTextureIndex);
				pCurrent->fOcclusionTextureIndex = ResolveTextureIndex(rMaterial.iOcclusionTextureSet, rMaterial.uiOcclusionTextureIndex);
				pCurrent->fEmissiveTextureIndex = ResolveTextureIndex(rMaterial.iEmissiveTextureSet, rMaterial.uiEmissiveTextureIndex);
				pCurrent++;
			}
		});
	}

	bool bMultiSet = pipelineInfo.flags & PipelineFlags::kMultiSet;

	for (int64_t i = 0; i < miMaterialCount; ++i)
	{
		// Detect transparent materials (fAlphaMask >= 2.0 signals BLEND alpha mode from export)
		bool bTransparent = pMaterials[i].fAlphaMask >= 2.0f;
		mpbTransparentMaterials.at(i) = bTransparent;
		if (bTransparent)
		{
			mFlags.Set(ModelPipelineFlags::kHasTransparentMaterials);
		}

		if (bTransparent && !bIsShadow)
		{
			pipelineInfo.flags = originalFlags;
			pipelineInfo.flags.Set(PipelineFlags::kAlphaBlend);
			// Partly transparent materials also contain opaque regions, so blending retains the base depth/cull flags.
		}
		else
		{
			pipelineInfo.flags = originalFlags;
		}

		pipelineInfo.vkExternalDescriptorSetLayout = gpTextureManager->mTextureDescriptors.mGlobalVkDescriptorSetLayout;

		// Multi-set: Pipeline 0 owns Set 1; inner Pipelines 1..N share its layout
		pipelineInfo.vkExternalSet1DescriptorSetLayout = VK_NULL_HANDLE;
		if (bMultiSet && i > 0)
		{
			pipelineInfo.vkExternalSet1DescriptorSetLayout = mpPipelines.at(0).mVkDescriptorSetLayout;
		}

		mpPipelines.at(i).Create(pipelineInfo);

		mpiFirstIndices.at(i) = puiIndexStarts[i];
		mpiIndexCounts.at(i) = (i + 1 == rSceneHeader.uiMaterialCount ? pipelineInfo.pVertexBuffer->mInfo.iCount : puiIndexStarts[i + 1]) - mpiFirstIndices.at(i);
	}
}

void ModelPipeline::RecordDrawIndirect(int64_t iCommandBuffer, VkCommandBuffer vkCommandBuffer, const XMFLOAT4& rf4PushConstants, ModelDrawPass ePass)
{
	ASSERT(rf4PushConstants.w == 0.0f);
	XMFLOAT4 f4PushConstants = rf4PushConstants;

	// Multi-set: bind global Set 0 + shared Set 1 once before the material loop
	bool bMultiSet = mpPipelines.at(0).mInfo.flags & PipelineFlags::kMultiSet;
	if (bMultiSet)
	{
		VkDescriptorSet pVkDescriptorSets[2] = {gpTextureManager->mTextureDescriptors.mGlobalDescriptorSets.at(iCommandBuffer), mpPipelines.at(0).mVkDescriptorSets.at(iCommandBuffer)};
		vkCmdBindDescriptorSets(vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mpPipelines.at(0).mVkPipelineLayout, 0, 2, pVkDescriptorSets, 0, nullptr);
		mpPipelines.at(0).mInfo.pVertexBuffer->RecordBindVertexBuffer(vkCommandBuffer);
	}

	for (int64_t i = 0; i < miMaterialCount; ++i)
	{
		if (ePass == ModelDrawPass::kOpaque && mpbTransparentMaterials.at(i))
		{
			continue;
		}
		if (ePass == ModelDrawPass::kTransparent && !mpbTransparentMaterials.at(i))
		{
			continue;
		}

		f4PushConstants.w = static_cast<float>(i);
		if (bMultiSet)
		{
			mpPipelines.at(i).RecordDrawIndirectSet2(iCommandBuffer, vkCommandBuffer, f4PushConstants);
		}
		else
		{
			mpPipelines.at(i).RecordDrawIndirect(iCommandBuffer, vkCommandBuffer, f4PushConstants);
		}
	}
}

void ModelPipeline::RequestTextures()
{
	if (mFlags & ModelPipelineFlags::kTexturesRequested)
	{
		return;
	}

	mFlags.Set(ModelPipelineFlags::kTexturesRequested);
	const EagerChunk& rChunk = gpFileManager->mpPackChunks->GetEagerChunkMap().at(mSceneCrc);
	const common::crc_t* pTextureCrcs = reinterpret_cast<const common::crc_t*>(rChunk.pData);
	std::span<const common::crc_t> textureCrcs(pTextureCrcs, rChunk.pHeader->sceneHeader.uiTextureCount);

	// A scene's texture references come from shared header definitions built alongside it, so a missing one is a
	// data-build error, not a runtime condition.
	for (common::crc_t textureCrc : textureCrcs)
	{
		ASSERT(gpFileManager->mpPackChunks->mLazyChunkMap.contains(textureCrc));
	}

	gpFileManager->mpPackChunks->mLoader.RequestChunkLoad(textureCrcs, LoadPriority::kNormal);
}

void ModelPipeline::WriteIndirectBuffer(int64_t iCommandBuffer, int64_t iCount)
{
	if (iCount > 0)
	{
		RequestTextures();
	}

	for (int64_t i = 0; i < miMaterialCount; ++i)
	{
		mpPipelines.at(i).WriteIndirectBuffer(iCommandBuffer, iCount, mpiIndexCounts.at(i), mpiFirstIndices.at(i));
	}
}

void ModelPipeline::UpdateStorageBufferDescriptors(int64_t iFramebuffer, int64_t iBinding, const Buffer* pBuffer)
{
	if (mpPipelines.at(0).mInfo.flags & PipelineFlags::kMultiSet)
	{
		// Set 1 bindings (15, 16) are shared — only update the first pipeline's descriptor sets
		PipelineDescriptorWriter::UpdateStorageBuffer(mpPipelines.at(0), iFramebuffer, iBinding, pBuffer);
	}
	else
	{
		for (Pipeline& rPipeline : mpPipelines)
		{
			PipelineDescriptorWriter::UpdateStorageBuffer(rPipeline, iFramebuffer, iBinding, pBuffer);
		}
	}
}

} // namespace engine

#endif // defined(BT_CLIENT)
