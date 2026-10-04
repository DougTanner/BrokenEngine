#if defined(BT_CLIENT)

#include "ModelPipeline.h"

#include "File/PackChunks.h"
#include "PipelineDescriptorWriter.h"

namespace engine
{

void ModelPipeline::Create(common::crc_t sceneCrc, const PipelineInfo& rPipelineInfo, bool bIsShadow)
{
	PipelineInfo pipelineInfo = rPipelineInfo;

	static constexpr int64_t kiModelAdditionalDescriptors = 5; // +1 lighting, +2 shadow, +3 smoke, +4 mesh data, +5 joint matrices

	// Grow by the model descriptor plus its five appended slots before the scan, so the slots the loop
	// writes in place exist. Grown entries default to kEmpty, so the scan still stops at the first of them.
	int64_t iDescriptorInfoCount = std::ssize(pipelineInfo.descriptorInfos) + kiModelAdditionalDescriptors + 1;
	pipelineInfo.descriptorInfos.resize(iDescriptorInfoCount);

	for (int64_t i = 0; i < iDescriptorInfoCount; ++i)
	{
		DescriptorInfo& rDescriptorInfo = pipelineInfo.descriptorInfos.at(i);
		if (rDescriptorInfo.flags & DescriptorFlags::kEmpty)
		{
			rDescriptorInfo.flags = DescriptorFlags::kModel;
			rDescriptorInfo.crc = sceneCrc;

			ASSERT(pipelineInfo.descriptorInfos.at(i + 1).flags == DescriptorFlags::kEmpty);

			pipelineInfo.descriptorInfos.at(i + 1).flags = DescriptorFlags::kCombinedSamplers;
			pipelineInfo.descriptorInfos.at(i + 1).iCount = static_cast<int64_t>(std::size(gpTextureManager->mRenderTargetTextures.mppLightingFinalTextures));
			pipelineInfo.descriptorInfos.at(i + 1).ppTextures = gpTextureManager->mRenderTargetTextures.mppLightingFinalTextures;

			pipelineInfo.descriptorInfos.at(i + 2).flags = DescriptorFlags::kCombinedSamplers;
			pipelineInfo.descriptorInfos.at(i + 2).pTexture = &gpTextureManager->mRenderTargetTextures.mShadowBlurTexture;

			// The smoke sampler (kSamplerSmoke) uses LINEAR filtering for R16_SFLOAT smoke ping-pong textures. Same CLAMP_TO_BORDER + INT_TRANSPARENT_BLACK as the border sampler; aniso/lodbias are no-ops at mipLevels = 1.
			pipelineInfo.descriptorInfos.at(i + 3).flags = {DescriptorFlags::kCombinedSamplers, DescriptorFlags::kSamplerSmoke};
			pipelineInfo.descriptorInfos.at(i + 3).pTexture = &gpTextureManager->mRenderTargetTextures.mSmokeTextureOne;

			// Binding 15: Per-mesh data (matrix, normal matrix, joint count/offset)
			pipelineInfo.descriptorInfos.at(i + 4).flags = DescriptorFlags::kPerCommandBufferStorageBuffers;
			pipelineInfo.descriptorInfos.at(i + 4).iExplicitBinding = shaders::kiModelBindingMeshData;
			pipelineInfo.descriptorInfos.at(i + 4).pBuffers = gpBufferManager->mMeshDataStorageBuffers.data();

			// Binding 16: Joint matrices (separate dynamically-sized buffer; MeshData stays fixed-size)
			pipelineInfo.descriptorInfos.at(i + 5).flags = DescriptorFlags::kPerCommandBufferStorageBuffers;
			pipelineInfo.descriptorInfos.at(i + 5).iExplicitBinding = shaders::kiModelBindingJointMatrix;
			pipelineInfo.descriptorInfos.at(i + 5).pBuffers = gpBufferManager->mJointMatrixStorageBuffers.data();
			break;
		}
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
	uint32_t uiPreviousIndexStart = 0;
	for (int64_t i = 0; i < miMaterialCount; ++i)
	{
		uint32_t uiIndexStart = puiIndexStarts[i];
		if ((i > 0 && uiIndexStart < uiPreviousIndexStart) || static_cast<int64_t>(uiIndexStart) > pipelineInfo.pVertexBuffer->mInfo.iCount)
		{
			throw std::ios_base::failure("ModelPipeline::Create");
		}
		uiPreviousIndexStart = uiIndexStart;
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

void ModelPipeline::WriteIndirectBuffer(int64_t iCommandBuffer, int64_t iCount)
{
	if (iCount > 0 && !(mFlags & ModelPipelineFlags::kTexturesRequested))
	{
		mFlags.Set(ModelPipelineFlags::kTexturesRequested);
		const EagerChunk& rChunk = gpFileManager->mpPackChunks->GetEagerChunkMap().at(mSceneCrc);
		const common::crc_t* pTextureCrcs = reinterpret_cast<const common::crc_t*>(rChunk.pData);
		std::span<const common::crc_t> textureCrcs(pTextureCrcs, rChunk.pHeader->sceneHeader.uiTextureCount);

		// A scene's texture references come from shared header definitions built alongside it, so a missing one is a
		// data-build error, not a runtime condition. Island channel references are the soft-failing case instead.
		for (common::crc_t textureCrc : textureCrcs)
		{
			ASSERT(gpFileManager->mpPackChunks->mLazyChunkMap.contains(textureCrc));
		}

		gpFileManager->mpPackChunks->mLoader.RequestChunkLoad(textureCrcs, LoadPriority::kNormal);
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
