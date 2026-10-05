#if defined(BT_CLIENT)

#include "PipelineDescriptorWriter.h"

#include "File/PackChunks.h"
#include "Pipeline.h"

namespace engine
{

// Check if a binding belongs to global Set 0 (must not register for per-pipeline descriptor updates)
static bool BindingIsInSet0(const Pipeline& rPipeline, int64_t iBinding)
{
	if (rPipeline.mExternalVkDescriptorSetLayout == VK_NULL_HANDLE)
	{
		return false;
	}
	// Callers gate every BindingIsInSet0 with BindingExistsInShaderLayout (short-circuit), so the binding
	// is always declared by some shader here — ResolveBindingSetIndex's default-0 fallback never decides.
	return Pipeline::ResolveBindingSetIndex(rPipeline.mInfo, iBinding) == 0;
}

// WriteModelDescriptor pushes this many single-texture image-infos (sampler, irradiance, prefiltered,
// lutbrdf); its bindless array reads gpTextureManager->mTextureDescriptors.mImageInfos.data() directly
// and does not touch the per-Write() image-info buffer. Used as the per-descriptor floor when sizing
// that buffer in Write() (see the iMaxImageInfos pre-scan).
constexpr int64_t kiModelDescriptorImageInfos = 4;

// Non-owning pointers: image infos live in the workbuffer; descriptor writes and buffer infos live on Write()'s stack.
struct DescriptorWriteCursor
{
	VkWriteDescriptorSet* pVkWriteDescriptorSets = nullptr;
	int64_t iDescriptorCount = 0;
	VkDescriptorImageInfo* pVkImageInfos = nullptr;
	int64_t iImageInfoCount = 0;
	int64_t iMaxImageInfos = 0;
	VkDescriptorBufferInfo* pVkBufferInfos = nullptr;
	int64_t iBufferInfoCount = 0;
};

// The framebuffer-0-only "this binding takes a deferred per-pipeline registration" gate, repeated at
// every register site. The extra kCombinedSamplers conjunct at the combined-sampler site stays at that
// call site (specific to that branch, not part of the general gate).
static bool ShouldRegisterBinding(const Pipeline& rPipeline, int64_t iFramebuffer, int64_t iBinding)
{
	return iFramebuffer == 0 && PipelineDescriptorWriter::BindingExistsInShaderLayout(rPipeline, iBinding) && !BindingIsInSet0(rPipeline, iBinding);
}

// Emits one IBL combined-image-sampler write (irradiance / prefiltered / lutBRDF) — the three differ only
// in (registerCrc, source texture). Pushes the image-info + write into the cursor and registers the
// binding for deferred updates on framebuffer 0.
static void PushCombinedImageSamplerWrite(Pipeline& rPipeline, int64_t iFramebuffer, VkWriteDescriptorSet& rVkWriteDescriptorSet, DescriptorWriteCursor& rCursor, common::crc_t registerCrc, Texture& rTexture)
{
	VkDescriptorImageInfo& rVkDescriptorImageInfo = rCursor.pVkImageInfos[rCursor.iImageInfoCount++];
	ASSERT(rCursor.iImageInfoCount <= rCursor.iMaxImageInfos);
	rVkDescriptorImageInfo.sampler = gpTextureManager->GetSampler(DescriptorFlags::kSamplerRepeat);
	rVkDescriptorImageInfo.imageView = rTexture.mVkImageView;
	rVkDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	rVkWriteDescriptorSet.dstBinding = static_cast<uint32_t>(rCursor.iDescriptorCount);
	rVkWriteDescriptorSet.descriptorCount = 1;
	rVkWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	rVkWriteDescriptorSet.pImageInfo = &rVkDescriptorImageInfo;
	rVkWriteDescriptorSet.pBufferInfo = nullptr;

	rCursor.pVkWriteDescriptorSets[rCursor.iDescriptorCount++] = rVkWriteDescriptorSet;
	ASSERT(rCursor.iDescriptorCount < common::ShaderHeader::kiMaxDescriptorSetLayoutBindings);

	if (ShouldRegisterBinding(rPipeline, iFramebuffer, rCursor.iDescriptorCount - 1))
	{
		gpTextureManager->mTextureDescriptors.RegisterTextureBinding({.crc = registerCrc, .pPipeline = &rPipeline, .iBinding = rCursor.iDescriptorCount - 1, .samplerFlags = DescriptorFlags::kSamplerRepeat, .pTexture = &rTexture});
	}
}

static void WriteModelDescriptor(Pipeline& rPipeline, const DescriptorInfo& rDescriptorInfo, int64_t iFramebuffer, VkWriteDescriptorSet& rVkWriteDescriptorSet, DescriptorWriteCursor& rCursor)
{
	const EagerChunk& rChunk = gpFileManager->mpPackChunks->GetEagerChunkMap().at(rDescriptorInfo.crc);

	// Sampler for bindless texture array
	{
		VkDescriptorImageInfo& rVkDescriptorImageInfo = rCursor.pVkImageInfos[rCursor.iImageInfoCount++];
		ASSERT(rCursor.iImageInfoCount <= rCursor.iMaxImageInfos);
		rVkDescriptorImageInfo.sampler = gpTextureManager->GetSampler(DescriptorFlags::kSamplerRepeat);
		rVkDescriptorImageInfo.imageView = nullptr;
		rVkDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		rVkWriteDescriptorSet.dstBinding = static_cast<uint32_t>(rCursor.iDescriptorCount);
		rVkWriteDescriptorSet.descriptorCount = 1;
		rVkWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		rVkWriteDescriptorSet.pImageInfo = &rVkDescriptorImageInfo;
		rVkWriteDescriptorSet.pBufferInfo = nullptr;

		rCursor.pVkWriteDescriptorSets[rCursor.iDescriptorCount++] = rVkWriteDescriptorSet;
		ASSERT(rCursor.iDescriptorCount < common::ShaderHeader::kiMaxDescriptorSetLayoutBindings);

		if (ShouldRegisterBinding(rPipeline, iFramebuffer, rCursor.iDescriptorCount - 1))
		{
			gpTextureManager->mTextureDescriptors.RegisterStandaloneSamplerBinding(&rPipeline, rCursor.iDescriptorCount - 1, DescriptorFlags::kSamplerRepeat);
		}
	}

	// Bindless texture array
	{
		rVkWriteDescriptorSet.dstBinding = static_cast<uint32_t>(rCursor.iDescriptorCount);
		rVkWriteDescriptorSet.descriptorCount = static_cast<uint32_t>(std::ssize(gpTextureManager->mTextureDescriptors.mImageInfos));
		rVkWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		rVkWriteDescriptorSet.pImageInfo = gpTextureManager->mTextureDescriptors.mImageInfos.data();
		rVkWriteDescriptorSet.pBufferInfo = nullptr;

		rCursor.pVkWriteDescriptorSets[rCursor.iDescriptorCount++] = rVkWriteDescriptorSet;
		ASSERT(rCursor.iDescriptorCount < common::ShaderHeader::kiMaxDescriptorSetLayoutBindings);
	}

	PushCombinedImageSamplerWrite(rPipeline, iFramebuffer, rVkWriteDescriptorSet, rCursor, TextureManager::kIrradianceCrc, gpTextureManager->mTextureMap.at(TextureManager::kIrradianceCrc));
	PushCombinedImageSamplerWrite(rPipeline, iFramebuffer, rVkWriteDescriptorSet, rCursor, TextureManager::kPrefilteredCrc, gpTextureManager->mTextureMap.at(TextureManager::kPrefilteredCrc));
	PushCombinedImageSamplerWrite(rPipeline, iFramebuffer, rVkWriteDescriptorSet, rCursor, 0, gpTextureManager->mTextureCache.mPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionLookupTableTexture);

	if (rPipeline.mModelMaterialsStorageBuffer.mDeviceLocalVkBuffer == VK_NULL_HANDLE)
	{
		rPipeline.mModelMaterialsStorageBuffer.Create(
		{
			.name = "Materials",
			.flags = {BufferFlags::kStorage, BufferFlags::kDeviceLocal},
			.iDataSize = static_cast<int64_t>(rChunk.pHeader->sceneHeader.uiMaterialCount) * static_cast<int64_t>(sizeof(shaders::PbrMaterialLayout)),
		}, [&](void* pData)
		{
			const common::SceneHeader& rSceneHeader = rChunk.pHeader->sceneHeader;
			int64_t iArraysSize = common::SceneHeader::MaterialDataOffset(rSceneHeader.uiTextureCount, rSceneHeader.uiMaterialCount);
			const common::crc_t* pTextureCrcs = reinterpret_cast<const common::crc_t*>(rChunk.pData);
			const common::MaterialShaderData* pMaterialShaderData = reinterpret_cast<const common::MaterialShaderData*>(rChunk.pData + iArraysSize);
			shaders::PbrMaterialLayout* pCurrent = static_cast<shaders::PbrMaterialLayout*>(pData);
			static constexpr int64_t kiOldMaterialSize = offsetof(shaders::PbrMaterialLayout, fColorTextureIndex);
			auto ResolveTextureIndex = [&](int64_t iTextureSet, int64_t iTextureIndex)
			{
				if (iTextureSet < 0)
				{
					return 0.0f;
				}
				if (iTextureIndex >= rSceneHeader.uiTextureCount)
				{
					throw std::ios_base::failure("PipelineDescriptorWriter material");
				}
				return static_cast<float>(gpTextureManager->mTextureDescriptors.CrcToIndex(pTextureCrcs[iTextureIndex]));
			};
			for (int64_t j = 0; j < rSceneHeader.uiMaterialCount; ++j)
			{
				const common::MaterialShaderData& rMaterial = pMaterialShaderData[j];
				std::memcpy(pCurrent, &pMaterialShaderData[j].f4BaseColorFactor, kiOldMaterialSize);
				pCurrent->fColorTextureIndex = ResolveTextureIndex(rMaterial.iColorTextureSet, rMaterial.uiColorTextureIndex);
				pCurrent->fPhysicalDescriptorTextureIndex = ResolveTextureIndex(rMaterial.iPhysicalDescriptorTextureSet, rMaterial.uiPhysicalDescriptorTextureIndex);
				pCurrent->fNormalTextureIndex = ResolveTextureIndex(rMaterial.iNormalTextureSet, rMaterial.uiNormalTextureIndex);
				pCurrent->fOcclusionTextureIndex = ResolveTextureIndex(rMaterial.iOcclusionTextureSet, rMaterial.uiOcclusionTextureIndex);
				pCurrent->fEmissiveTextureIndex = ResolveTextureIndex(rMaterial.iEmissiveTextureSet, rMaterial.uiEmissiveTextureIndex);
				pCurrent++;
			}
		});
	}

	VkDescriptorBufferInfo& rVkDescriptorBufferInfo = rCursor.pVkBufferInfos[rCursor.iBufferInfoCount++];
	ASSERT(rCursor.iBufferInfoCount < common::ShaderHeader::kiMaxDescriptorSetLayoutBindings);
	rVkDescriptorBufferInfo.buffer = rPipeline.mModelMaterialsStorageBuffer.mDeviceLocalVkBuffer;
	rVkDescriptorBufferInfo.offset = 0;
	rVkDescriptorBufferInfo.range = VK_WHOLE_SIZE;

	rVkWriteDescriptorSet.dstBinding = static_cast<uint32_t>(rCursor.iDescriptorCount);
	rVkWriteDescriptorSet.descriptorCount = 1;
	rVkWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	rVkWriteDescriptorSet.pImageInfo = nullptr;
	rVkWriteDescriptorSet.pBufferInfo = &rVkDescriptorBufferInfo;

	rCursor.pVkWriteDescriptorSets[rCursor.iDescriptorCount++] = rVkWriteDescriptorSet;
	ASSERT(rCursor.iDescriptorCount < common::ShaderHeader::kiMaxDescriptorSetLayoutBindings);
}

static void WriteBufferDescriptor(const DescriptorInfo& rDescriptorInfo, int64_t iFramebuffer, VkWriteDescriptorSet& rVkWriteDescriptorSet, DescriptorWriteCursor& rCursor)
{
	VkBuffer vkBuffer = VK_NULL_HANDLE;
	if (rDescriptorInfo.flags & DescriptorFlags::kGlobalLayoutUniformBuffers)
	{
		vkBuffer = gpBufferManager->mGlobalLayoutUniformBuffers.at(static_cast<size_t>(iFramebuffer)).GetBuffer();
	}
	else if (rDescriptorInfo.flags & DescriptorFlags::kMainLayoutUniformBuffers)
	{
		vkBuffer = gpBufferManager->mMainLayoutUniformBuffers.at(static_cast<size_t>(iFramebuffer)).GetBuffer();
	}
	else if (rDescriptorInfo.flags & DescriptorFlags::kUniformBuffer || rDescriptorInfo.flags & DescriptorFlags::kStorageBuffer)
	{
		vkBuffer = rDescriptorInfo.pBuffers != nullptr ? rDescriptorInfo.pBuffers->GetBuffer() : *rDescriptorInfo.pVkBuffers;
	}
	else if (rDescriptorInfo.flags & DescriptorFlags::kPerCommandBufferUniformBuffers || rDescriptorInfo.flags & DescriptorFlags::kPerCommandBufferStorageBuffers)
	{
		vkBuffer = rDescriptorInfo.pBuffers[iFramebuffer].GetBuffer();
	}
	ASSERT(vkBuffer != VK_NULL_HANDLE);

	VkDescriptorBufferInfo& rVkDescriptorBufferInfo = rCursor.pVkBufferInfos[rCursor.iBufferInfoCount++];
	ASSERT(rCursor.iBufferInfoCount < common::ShaderHeader::kiMaxDescriptorSetLayoutBindings);
	rVkDescriptorBufferInfo.buffer = vkBuffer;
	rVkDescriptorBufferInfo.offset = 0;
	rVkDescriptorBufferInfo.range = VK_WHOLE_SIZE;

	rVkWriteDescriptorSet.descriptorCount = 1;
	rVkWriteDescriptorSet.descriptorType = rDescriptorInfo.flags & DescriptorFlags::kStorageBuffer || rDescriptorInfo.flags & DescriptorFlags::kPerCommandBufferStorageBuffers ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	rVkWriteDescriptorSet.pImageInfo = nullptr;
	rVkWriteDescriptorSet.pBufferInfo = &rVkDescriptorBufferInfo;

	rCursor.pVkWriteDescriptorSets[rCursor.iDescriptorCount++] = rVkWriteDescriptorSet;
	ASSERT(rCursor.iDescriptorCount < common::ShaderHeader::kiMaxDescriptorSetLayoutBindings);
}

static void WriteStandaloneSampler(Pipeline& rPipeline, const DescriptorInfo& rDescriptorInfo, int64_t iFramebuffer, int64_t iBinding, int64_t iRegisterBinding, VkWriteDescriptorSet& rVkWriteDescriptorSet, DescriptorWriteCursor& rCursor)
{
	VkDescriptorImageInfo& rVkDescriptorImageInfo = rCursor.pVkImageInfos[rCursor.iImageInfoCount++];
	ASSERT(rCursor.iImageInfoCount <= rCursor.iMaxImageInfos);
	rVkDescriptorImageInfo.sampler = gpTextureManager->GetSampler(rDescriptorInfo.flags);
	rVkDescriptorImageInfo.imageView = nullptr;
	rVkDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	rVkWriteDescriptorSet.descriptorCount = 1;
	rVkWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	rVkWriteDescriptorSet.pImageInfo = &rVkDescriptorImageInfo;
	rVkWriteDescriptorSet.pBufferInfo = nullptr;

	if (ShouldRegisterBinding(rPipeline, iFramebuffer, iBinding))
	{
		gpTextureManager->mTextureDescriptors.RegisterStandaloneSamplerBinding(&rPipeline, iRegisterBinding, rDescriptorInfo.flags);
	}

	rCursor.pVkWriteDescriptorSets[rCursor.iDescriptorCount++] = rVkWriteDescriptorSet;
	ASSERT(rCursor.iDescriptorCount < common::ShaderHeader::kiMaxDescriptorSetLayoutBindings);
}

// Fills the per-element image infos for a combined-image-sampler / storage-image array and sets the
// write's type/count/pImageInfo. Registration of the bindings (combined samplers only) is a separate
// concern handled by RegisterCombinedSamplerBindings at the call site.
static void WriteCombinedSamplers(const DescriptorInfo& rDescriptorInfo, VkWriteDescriptorSet& rVkWriteDescriptorSet, DescriptorWriteCursor& rCursor)
{
	VkDescriptorImageInfo* pVkImageInfoStart = &rCursor.pVkImageInfos[rCursor.iImageInfoCount];

	for (int64_t k = 0; k < rDescriptorInfo.iCount; ++k)
	{
		VkDescriptorImageInfo& rVkDescriptorImageInfo = rCursor.pVkImageInfos[rCursor.iImageInfoCount++];
		ASSERT(rCursor.iImageInfoCount <= rCursor.iMaxImageInfos);
		rVkDescriptorImageInfo.sampler = rDescriptorInfo.flags & DescriptorFlags::kCombinedSamplers ? gpTextureManager->GetSampler(rDescriptorInfo.flags) : nullptr;

		if (rDescriptorInfo.textureCrc != 0)
		{
			rVkDescriptorImageInfo.imageView = gpTextureManager->mTextureMap.at(rDescriptorInfo.textureCrc).mVkImageView;
		}
		else if (rDescriptorInfo.iCount == 1 && rDescriptorInfo.pTexture != nullptr)
		{
			// Runtime-only texture (e.g. render targets, generated textures). Data-packed textures must use
			// the textureCrc path instead so they get deferred descriptor updates when lazy-loaded.
			ASSERT(rDescriptorInfo.pTexture->mInfo.uiCrc == 0);
			rVkDescriptorImageInfo.imageView = rDescriptorInfo.pTexture->mVkImageView;
		}
		else
		{
			// Array of texture pointers. A slot mid-reload after eviction points at an mTextureMap
			// entry whose view Destroy destroyed and AdoptTransferredImage has not yet
			// re-attached (post-eviction re-mint window) — a pipeline rebuild (settings/fullscreen
			// recreate) snapshotting that null view trips VUID-02997. Fall back to the array's
			// slot-0 placeholder; the adoption path patches the real view in afterward.
			VkImageView vkImageView = rDescriptorInfo.ppTextures[k]->mVkImageView;
			rVkDescriptorImageInfo.imageView = vkImageView != VK_NULL_HANDLE ? vkImageView : rDescriptorInfo.ppTextures[0]->mVkImageView;
		}

		rVkDescriptorImageInfo.imageLayout = rDescriptorInfo.flags & DescriptorFlags::kCombinedSamplers ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
	}

	rVkWriteDescriptorSet.descriptorCount = static_cast<uint32_t>(rDescriptorInfo.iCount);
	rVkWriteDescriptorSet.descriptorType = rDescriptorInfo.flags & DescriptorFlags::kCombinedSamplers ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	rVkWriteDescriptorSet.pImageInfo = pVkImageInfoStart;
	rVkWriteDescriptorSet.pBufferInfo = nullptr;

	rCursor.pVkWriteDescriptorSets[rCursor.iDescriptorCount++] = rVkWriteDescriptorSet;
	ASSERT(rCursor.iDescriptorCount < common::ShaderHeader::kiMaxDescriptorSetLayoutBindings);
}

// Registrations retain Pipeline pointers until Pipeline::Destroy unregisters them.
// PipelineManager clears bindings before rebuilds; bindless consumers replay registrations for live island slots.
static void RegisterCombinedSamplerBindings(Pipeline& rPipeline, const DescriptorInfo& rDescriptorInfo, int64_t iRegisterBinding)
{
	if (rDescriptorInfo.textureCrc != 0)
	{
		Texture* pTexture = &gpTextureManager->mTextureMap.at(rDescriptorInfo.textureCrc);
		gpTextureManager->mTextureDescriptors.RegisterTextureBinding({.crc = rDescriptorInfo.textureCrc, .pPipeline = &rPipeline, .iBinding = iRegisterBinding, .samplerFlags = rDescriptorInfo.flags, .pTexture = pTexture});
		rPipeline.mTextureCrcs.push_back(rDescriptorInfo.textureCrc);
	}
	else if (rDescriptorInfo.iCount == 1 && rDescriptorInfo.pTexture != nullptr)
	{
		gpTextureManager->mTextureDescriptors.RegisterTextureBinding({.crc = 0, .pPipeline = &rPipeline, .iBinding = iRegisterBinding, .samplerFlags = rDescriptorInfo.flags, .pTexture = rDescriptorInfo.pTexture});
	}
	else if (rDescriptorInfo.ppTextures != nullptr)
	{
		if (rDescriptorInfo.flags & DescriptorFlags::kBindlessArrayConsumer)
		{
			// IslandTerrainResidency supplies per-slot binding keys at first mint.
			// Register the consumer here; TextureDescriptors::RewriteSamplerDescriptors reads its live array.
			gpTextureManager->mTextureDescriptors.RegisterBindlessArrayConsumer(std::span<Texture*>(rDescriptorInfo.ppTextures, static_cast<size_t>(rDescriptorInfo.iCount)), &rPipeline, iRegisterBinding, rDescriptorInfo.flags);
		}
		else
		{
			// Register per-CRC entries for lazy texture loading and sampler updates. ppTextures is copied into TextureBinding.textures; arrays mutated
			// after pipeline creation require kBindlessArrayConsumer so sampler recreation reads live pointers rather than stale snapshots.
			for (int64_t k = 0; k < rDescriptorInfo.iCount; ++k)
			{
				common::crc_t arrayCrc = rDescriptorInfo.ppTextures[k]->mInfo.uiCrc;
				if (arrayCrc != 0 && gpTextureManager->mTextureMap.contains(arrayCrc))
				{
					gpTextureManager->mTextureDescriptors.RegisterTextureBinding({.crc = arrayCrc, .pPipeline = &rPipeline, .iBinding = iRegisterBinding, .samplerFlags = rDescriptorInfo.flags, .ppTextures = rDescriptorInfo.ppTextures, .iCount = rDescriptorInfo.iCount});
					rPipeline.mTextureCrcs.push_back(arrayCrc);
				}
			}
			// Register under CRC 0 for sampler recreation coverage (texture array is copied into TextureBinding)
			gpTextureManager->mTextureDescriptors.RegisterTextureBinding({.crc = 0, .pPipeline = &rPipeline, .iBinding = iRegisterBinding, .samplerFlags = rDescriptorInfo.flags, .ppTextures = rDescriptorInfo.ppTextures, .iCount = rDescriptorInfo.iCount});
		}
	}
}

static int64_t FilterWritesByShaderLayout(const Pipeline& rPipeline, std::span<VkWriteDescriptorSet> writeDescriptorSets)
{
	bool bCompute = rPipeline.mInfo.flags & PipelineFlags::kCompute;
	Shader* pFirstShader = rPipeline.mInfo.ppShaders[0];
	Shader* pSecondShader = bCompute ? nullptr : rPipeline.mInfo.ppShaders[1];

	int64_t iValidCount = 0;
	for (int64_t j = 0; j < std::ssize(writeDescriptorSets); ++j)
	{
		int64_t iBinding = static_cast<int64_t>(writeDescriptorSets[j].dstBinding);
		if (iBinding < common::ShaderHeader::kiMaxDescriptorSetLayoutBindings)
		{
			const VkDescriptorSetLayoutBinding& rVkFirstBinding = iBinding < pFirstShader->mInformation.pChunkHeader->shaderHeader.iDescriptorSetLayoutBindings ? pFirstShader->mInformation.pVkDescriptorBindings[iBinding] : Pipeline::kEmptyVkDescriptorSetLayoutBinding;
			const VkDescriptorSetLayoutBinding& rVkSecondBinding = pSecondShader != nullptr && iBinding < pSecondShader->mInformation.pChunkHeader->shaderHeader.iDescriptorSetLayoutBindings ? pSecondShader->mInformation.pVkDescriptorBindings[iBinding] : Pipeline::kEmptyVkDescriptorSetLayoutBinding;
			if (rVkFirstBinding.descriptorCount > 0 || rVkSecondBinding.descriptorCount > 0)
			{
				writeDescriptorSets[iValidCount++] = writeDescriptorSets[j];
			}
		}
	}
	return iValidCount;
}

static int64_t RouteWritesBySet(const PipelineInfo& rPipelineInfo, std::span<VkWriteDescriptorSet> writeDescriptorSets, VkDescriptorSet vkDestinationSet2, bool bHasExternalSet1)
{
	int64_t iValidCount = 0;
	for (int64_t j = 0; j < std::ssize(writeDescriptorSets); ++j)
	{
		int64_t iBinding = static_cast<int64_t>(writeDescriptorSets[j].dstBinding);
		int64_t iSet = Pipeline::ResolveBindingSetIndex(rPipelineInfo, iBinding);

		if (iSet == 2)
		{
			writeDescriptorSets[j].dstSet = vkDestinationSet2;
			writeDescriptorSets[iValidCount++] = writeDescriptorSets[j];
		}
		else if (iSet == 1 && !bHasExternalSet1)
		{
			writeDescriptorSets[iValidCount++] = writeDescriptorSets[j];
		}
		// Set 0 writes are dropped (handled by global descriptor set)
	}
	return iValidCount;
}

bool PipelineDescriptorWriter::BindingExistsInShaderLayout(const Pipeline& rPipeline, int64_t iBinding)
{
	if (iBinding < 0 || iBinding >= common::ShaderHeader::kiMaxDescriptorSetLayoutBindings)
	{
		return false;
	}
	const VkDescriptorSetLayoutBinding& rVkFirstBinding = iBinding < rPipeline.mInfo.ppShaders[0]->mInformation.pChunkHeader->shaderHeader.iDescriptorSetLayoutBindings ? rPipeline.mInfo.ppShaders[0]->mInformation.pVkDescriptorBindings[iBinding] : Pipeline::kEmptyVkDescriptorSetLayoutBinding;
	if (rPipeline.mInfo.flags & PipelineFlags::kCompute)
	{
		return rVkFirstBinding.descriptorCount > 0;
	}
	const VkDescriptorSetLayoutBinding& rVkSecondBinding = iBinding < rPipeline.mInfo.ppShaders[1]->mInformation.pChunkHeader->shaderHeader.iDescriptorSetLayoutBindings ? rPipeline.mInfo.ppShaders[1]->mInformation.pVkDescriptorBindings[iBinding] : Pipeline::kEmptyVkDescriptorSetLayoutBinding;
	return rVkFirstBinding.descriptorCount > 0 || rVkSecondBinding.descriptorCount > 0;
}

void PipelineDescriptorWriter::Write(Pipeline& rPipeline)
{
	bool bMultiSet = rPipeline.mInfo.flags & PipelineFlags::kMultiSet;
	bool bHasExternalSet0 = rPipeline.mExternalVkDescriptorSetLayout != VK_NULL_HANDLE;
	bool bHasExternalSet1 = rPipeline.mExternalSet1VkDescriptorSetLayout != VK_NULL_HANDLE;

	int64_t iPerCommandBuffer = rPipeline.mbPerCommandBuffer ? std::ssize(gpSwapchainManager->mFramebuffers) : 1;
	if (!bHasExternalSet1)
	{
		rPipeline.mVkDescriptorSets.resize(iPerCommandBuffer);
	}
	if (bMultiSet)
	{
		rPipeline.mVkDescriptorSetsSet2.resize(iPerCommandBuffer);
	}

	// Upper bound on the image-infos this pipeline's descriptors push into the per-Write() buffer below.
	// Safe over-estimate that reads only iCount: each Write branch pushes at most max(iCount, 4) per
	// descriptor (model = 4, combined/storage = iCount, standalone sampler = 1, buffer/texture = 0).
	// Scales with shaders::kiMaxIslands (4 bindless terrain arrays) without a hand-tuned constant.
	int64_t iMaxImageInfos = 0;
	for (const DescriptorInfo& rDescriptorInfo : rPipeline.mInfo.descriptorInfos)
	{
		if (rDescriptorInfo.flags & DescriptorFlags::kEmpty)
		{
			break;
		}
		iMaxImageInfos += std::max(rDescriptorInfo.iCount, kiModelDescriptorImageInfos);
	}

	for (int64_t j = 0; j < iPerCommandBuffer; ++j)
	{
		VkDescriptorPool vkDescriptorPool = gpDeviceManager->mVkDescriptorPool;

		// Allocate Set 1 descriptor set (or single set for compute pipelines)
		VkDescriptorSet vkDescriptorSet = VK_NULL_HANDLE;
		if (!bHasExternalSet1)
		{
			VkDescriptorSetAllocateInfo vkDescriptorSetAllocateInfo
			{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.pNext = nullptr,
				.descriptorPool = vkDescriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts = &rPipeline.mVkDescriptorSetLayout,
			};
			CHECK_VK(vkAllocateDescriptorSets(gpDeviceManager->mVkDevice, &vkDescriptorSetAllocateInfo, &rPipeline.mVkDescriptorSets.at(j)));
			VkName(VK_OBJECT_TYPE_DESCRIPTOR_SET, rPipeline.mVkDescriptorSets.at(j), std::format("{}{}", rPipeline.mInfo.name.data(), j).c_str());
			vkDescriptorSet = rPipeline.mVkDescriptorSets.at(j);
		}

		// Allocate Set 2 descriptor set (multi-set models only)
		VkDescriptorSet vkDestinationSet2 = VK_NULL_HANDLE;
		if (bMultiSet)
		{
			VkDescriptorSetAllocateInfo vkDescriptorSetAllocateInfoSet2
			{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.pNext = nullptr,
				.descriptorPool = vkDescriptorPool,
				.descriptorSetCount = 1,
				.pSetLayouts = &rPipeline.mSet2VkDescriptorSetLayout,
			};
			CHECK_VK(vkAllocateDescriptorSets(gpDeviceManager->mVkDevice, &vkDescriptorSetAllocateInfoSet2, &rPipeline.mVkDescriptorSetsSet2.at(j)));
			VkName(VK_OBJECT_TYPE_DESCRIPTOR_SET, rPipeline.mVkDescriptorSetsSet2.at(j), std::format("{}Set2{}", rPipeline.mInfo.name.data(), j).c_str());
			vkDestinationSet2 = rPipeline.mVkDescriptorSetsSet2.at(j);
		}

		VkWriteDescriptorSet pVkWriteDescriptorSets[common::ShaderHeader::kiMaxDescriptorSetLayoutBindings] {};
		VkDescriptorBufferInfo pVkDescriptorBufferInfos[common::ShaderHeader::kiMaxDescriptorSetLayoutBindings] {};
		// Write() runs during startup and recreation; image infos use the per-thread workbuffer.
		// Workbuffer::Grow commits its reservation in place, preserving pointers across nested material uploads.
		// Every image info consumed by Vulkan is fully written before the update, so zero-initialization is unnecessary.
		auto pVkDescriptorImageInfos = common::gpThreadLocal->mWorkbuffer.PushBuffer<VkDescriptorImageInfo*>(iMaxImageInfos * static_cast<int64_t>(sizeof(VkDescriptorImageInfo)));

		DescriptorWriteCursor cursor
		{
			.pVkWriteDescriptorSets = pVkWriteDescriptorSets,
			.iDescriptorCount = 0,
			.pVkImageInfos = pVkDescriptorImageInfos.mpData,
			.iImageInfoCount = 0,
			.iMaxImageInfos = iMaxImageInfos,
			.pVkBufferInfos = pVkDescriptorBufferInfos,
			.iBufferInfoCount = 0,
		};
		for (const DescriptorInfo& rDescriptorInfo : rPipeline.mInfo.descriptorInfos)
		{
			if (rDescriptorInfo.flags & DescriptorFlags::kEmpty)
			{
				break;
			}

			// Bindless array consumers require kCombinedSamplers for registration before IslandTerrainResidency acquires slots.
			ASSERT(!(rDescriptorInfo.flags & DescriptorFlags::kBindlessArrayConsumer) || (rDescriptorInfo.flags & DescriptorFlags::kCombinedSamplers));

			// Deferred registrations require the shader binding number, not the descriptor-write index (VUID-00316).
			int64_t iBinding = rDescriptorInfo.iExplicitBinding >= 0 ? rDescriptorInfo.iExplicitBinding : cursor.iDescriptorCount;
			int64_t iRegisterBinding = iBinding;

			VkWriteDescriptorSet vkWriteDescriptorSet
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = vkDescriptorSet,
				.dstBinding = static_cast<uint32_t>(iBinding),
				.dstArrayElement = 0,
				.pTexelBufferView = nullptr,
			};

			bool bSampler = rDescriptorInfo.flags & DescriptorFlags::kSamplerAny;
			if (rDescriptorInfo.flags & DescriptorFlags::kModel)
			{
				WriteModelDescriptor(rPipeline, rDescriptorInfo, j, vkWriteDescriptorSet, cursor);
			}
			else if (rDescriptorInfo.flags & DescriptorFlags::kUniformBuffer || rDescriptorInfo.flags & DescriptorFlags::kStorageBuffer || rDescriptorInfo.flags & DescriptorFlags::kPerCommandBufferUniformBuffers || rDescriptorInfo.flags & DescriptorFlags::kPerCommandBufferStorageBuffers || rDescriptorInfo.flags & DescriptorFlags::kGlobalLayoutUniformBuffers || rDescriptorInfo.flags & DescriptorFlags::kMainLayoutUniformBuffers)
			{
				WriteBufferDescriptor(rDescriptorInfo, j, vkWriteDescriptorSet, cursor);
			}
			else if (bSampler && !(rDescriptorInfo.flags & DescriptorFlags::kCombinedSamplers))
			{
				WriteStandaloneSampler(rPipeline, rDescriptorInfo, j, iBinding, iRegisterBinding, vkWriteDescriptorSet, cursor);
			}
			else if (rDescriptorInfo.flags & DescriptorFlags::kTextures)
			{
				vkWriteDescriptorSet.descriptorCount = static_cast<uint32_t>(std::ssize(gpTextureManager->mTextureDescriptors.mImageInfos));
				vkWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				vkWriteDescriptorSet.pImageInfo = gpTextureManager->mTextureDescriptors.mImageInfos.data();
				vkWriteDescriptorSet.pBufferInfo = nullptr;

				cursor.pVkWriteDescriptorSets[cursor.iDescriptorCount++] = vkWriteDescriptorSet;
				ASSERT(cursor.iDescriptorCount < common::ShaderHeader::kiMaxDescriptorSetLayoutBindings);
			}
			else if (rDescriptorInfo.flags & DescriptorFlags::kCombinedSamplers || rDescriptorInfo.flags & DescriptorFlags::kStorageImages)
			{
				WriteCombinedSamplers(rDescriptorInfo, vkWriteDescriptorSet, cursor);

				if (rDescriptorInfo.flags & DescriptorFlags::kCombinedSamplers && ShouldRegisterBinding(rPipeline, j, iBinding))
				{
					RegisterCombinedSamplerBindings(rPipeline, rDescriptorInfo, iRegisterBinding);
				}
			}
			else
			{
				ASSERT(false);
			}

		}

		// Reflected layouts can be sparse (e.g. GLTF shadow bindings 0, 1, 2, 15).
		cursor.iDescriptorCount = FilterWritesByShaderLayout(rPipeline, std::span<VkWriteDescriptorSet>(cursor.pVkWriteDescriptorSets, static_cast<size_t>(cursor.iDescriptorCount)));

		// Route writes by set index: drop Set 0 (global), keep Set 1 and Set 2
		if (bHasExternalSet0)
		{
			cursor.iDescriptorCount = RouteWritesBySet(rPipeline.mInfo, std::span<VkWriteDescriptorSet>(cursor.pVkWriteDescriptorSets, static_cast<size_t>(cursor.iDescriptorCount)), vkDestinationSet2, bHasExternalSet1);
		}

		vkUpdateDescriptorSets(gpDeviceManager->mVkDevice, static_cast<uint32_t>(cursor.iDescriptorCount), cursor.pVkWriteDescriptorSets, 0, nullptr);
	}
}

void PipelineDescriptorWriter::UpdateStorageBuffer(const Pipeline& rPipeline, int64_t iFramebuffer, int64_t iBinding, const Buffer* pBuffer)
{
	// Guards against deferred-update callers (TextureDescriptors::Rewrite*) passing a binding
	// that the per-pipeline layout doesn't declare (e.g. confusing sequential descriptor index
	// with the explicit binding number). Pops the debugger before Vulkan validation fires.
	ASSERT(BindingExistsInShaderLayout(rPipeline, iBinding));

	VkDescriptorBufferInfo vkDescriptorBufferInfo
	{
		.buffer = pBuffer->GetBuffer(),
		.offset = 0,
		.range = VK_WHOLE_SIZE,
	};

	VkWriteDescriptorSet vkWriteDescriptorSet
	{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.pNext = nullptr,
		.dstSet = rPipeline.mVkDescriptorSets.at(iFramebuffer),
		.dstBinding = static_cast<uint32_t>(iBinding),
		.dstArrayElement = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.pImageInfo = nullptr,
		.pBufferInfo = &vkDescriptorBufferInfo,
		.pTexelBufferView = nullptr,
	};

	vkUpdateDescriptorSets(gpDeviceManager->mVkDevice, 1, &vkWriteDescriptorSet, 0, nullptr);
}

void PipelineDescriptorWriter::UpdateImageDescriptor(const Pipeline& rPipeline, int64_t iBinding, VkSampler vkSampler, VkImageView vkImageView, VkImageLayout vkImageLayout, VkDescriptorType vkDescriptorType)
{
	ASSERT(PipelineDescriptorWriter::BindingExistsInShaderLayout(rPipeline, iBinding));

	for (const VkDescriptorSet& rVkDescriptorSet : rPipeline.mVkDescriptorSets)
	{
		VkDescriptorImageInfo vkDescriptorImageInfo
		{
			.sampler = vkSampler,
			.imageView = vkImageView,
			.imageLayout = vkImageLayout,
		};

		VkWriteDescriptorSet vkWriteDescriptorSet
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = rVkDescriptorSet,
			.dstBinding = static_cast<uint32_t>(iBinding),
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vkDescriptorType,
			.pImageInfo = &vkDescriptorImageInfo,
			.pBufferInfo = nullptr,
			.pTexelBufferView = nullptr,
		};

		vkUpdateDescriptorSets(gpDeviceManager->mVkDevice, 1, &vkWriteDescriptorSet, 0, nullptr);
	}
}

} // namespace engine

#endif // defined(BT_CLIENT)
