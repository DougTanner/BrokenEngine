#if defined(BT_CLIENT)

#include "TextureDescriptors.h"

#include "Graphics/Objects/PipelineDescriptorWriter.h"
#include "TextureManager.h"

namespace engine
{

TextureDescriptors::TextureDescriptors(TextureManager& rTextureManager)
: mrTextureManager(rTextureManager)
{
}

TextureDescriptors::ScopedBindlessWriteEpoch::ScopedBindlessWriteEpoch(TextureDescriptors& rTextureDescriptors, int64_t iFramebuffer)
: mrTextureDescriptors(rTextureDescriptors)
{
	ASSERT(!mrTextureDescriptors.mbBindlessWriteEpoch);
	mrTextureDescriptors.mbBindlessWriteEpoch = true;
	mrTextureDescriptors.miBindlessWriteFramebuffer = iFramebuffer;
}

TextureDescriptors::ScopedBindlessWriteEpoch::~ScopedBindlessWriteEpoch()
{
	ASSERT(mrTextureDescriptors.mbBindlessWriteEpoch);
	mrTextureDescriptors.mbBindlessWriteEpoch = false;
}

void TextureDescriptors::Create()
{
	mLightingFallback.Create({.name = "LightingFallback", .vkFormat = VK_FORMAT_R32G32B32A32_SFLOAT, .vkExtent3D = {1, 1, 1}, .vkImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, .vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT}, [](std::span<std::byte> bytes, [[maybe_unused]] int64_t iPosition) { std::memset(bytes.data(), 0, bytes.size()); });
	mLightingControls = std::vector<Buffer>(static_cast<size_t>(std::ssize(gpSwapchainManager->mFramebuffers)));
	for (Buffer& rControl : mLightingControls)
	{
		rControl.Create({.name = "LightingControl", .flags = {BufferFlags::kStorage, BufferFlags::kHostVisible}, .iDataSize = 16});
		std::memset(rControl.mpMappedMemory, 0, 16);
	}
	// Set 0 is shared by graphics and compute pipelines.
	VkDescriptorSetLayoutBinding pVkBindings[]
	{
		{.binding = shaders::kiGlobalBindingGlobalUniform, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT},
		{.binding = shaders::kiGlobalBindingMainUniform, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT},
		{.binding = shaders::kiGlobalBindingSamplerRepeatModelData, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT},
		{.binding = shaders::kiGlobalBindingSamplerRepeat, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT},
		{.binding = shaders::kiGlobalBindingBindlessTextures, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = static_cast<uint32_t>(std::ssize(mImageInfos)), .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT},
		{.binding = shaders::kiGlobalBindingSamplerClamp, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT},
		{.binding = shaders::kiGlobalBindingLightingReference, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
		{.binding = shaders::kiGlobalBindingLightingControl, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
	};

	VkDescriptorBindingFlags pVkBindingFlags[]
	{
		0,
		0,
		0,
		0,
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
		0,
		VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
		VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
	};

	VkDescriptorSetLayoutBindingFlagsCreateInfo vkBindingFlagsCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
		.pNext = nullptr,
		.bindingCount = static_cast<uint32_t>(std::size(pVkBindingFlags)),
		.pBindingFlags = pVkBindingFlags,
	};

	VkDescriptorSetLayoutCreateInfo vkLayoutCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = &vkBindingFlagsCreateInfo,
		.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
		.bindingCount = static_cast<uint32_t>(std::size(pVkBindings)),
		.pBindings = pVkBindings,
	};

	CHECK_VK(vkCreateDescriptorSetLayout(gpDeviceManager->mVkDevice, &vkLayoutCreateInfo, nullptr, &mGlobalVkDescriptorSetLayout));
	VkName(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, mGlobalVkDescriptorSetLayout, "GlobalSet0");

	int64_t iFramebufferCount = std::ssize(gpSwapchainManager->mFramebuffers);
	mGlobalDescriptorSets.resize(iFramebufferCount);
	for (int64_t i = 0; i < iFramebufferCount; ++i)
	{
		VkDescriptorSetAllocateInfo vkAllocateInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = gpDeviceManager->mVkDescriptorPool,
			.descriptorSetCount = 1,
			.pSetLayouts = &mGlobalVkDescriptorSetLayout,
		};
		CHECK_VK(vkAllocateDescriptorSets(gpDeviceManager->mVkDevice, &vkAllocateInfo, &mGlobalDescriptorSets.at(i)));
		VkName(VK_OBJECT_TYPE_DESCRIPTOR_SET, mGlobalDescriptorSets.at(i), std::format("GlobalSet0{}", i).c_str());
	}
	mAppliedPublicationCursors.assign(static_cast<size_t>(iFramebufferCount), 0);

	WriteGlobalDescriptorSets();
}

void TextureDescriptors::Destroy()
{
	if (mGlobalVkDescriptorSetLayout != VK_NULL_HANDLE)
	{
		vkFreeDescriptorSets(gpDeviceManager->mVkDevice, gpDeviceManager->mVkDescriptorPool, static_cast<uint32_t>(std::ssize(mGlobalDescriptorSets)), mGlobalDescriptorSets.data());
		vkDestroyDescriptorSetLayout(gpDeviceManager->mVkDevice, mGlobalVkDescriptorSetLayout, nullptr);
		mGlobalVkDescriptorSetLayout = VK_NULL_HANDLE;
		mGlobalDescriptorSets.clear();
		mLightingControls.clear();
		mLightingFallback.Destroy();
		// Create rewrites the new Global sets from live state, and every tier that destroys them also rebuilds the pipelines.
		mPendingPublications.clear();
		mAppliedPublicationCursors.clear();
	}
}

void TextureDescriptors::WriteGlobalDescriptorSets()
{
	for (int64_t i = 0; i < std::ssize(mGlobalDescriptorSets); ++i)
	{
		VkDescriptorBufferInfo vkGlobalBufferInfo {.buffer = gpBufferManager->mGlobalLayoutUniformBuffers.at(i).GetBuffer(), .offset = 0, .range = VK_WHOLE_SIZE};
		VkDescriptorBufferInfo vkMainBufferInfo {.buffer = gpBufferManager->mMainLayoutUniformBuffers.at(i).GetBuffer(), .offset = 0, .range = VK_WHOLE_SIZE};
		VkDescriptorImageInfo vkSamplerRepeatModelDataInfo {.sampler = mrTextureManager.mpSamplersVkSampler[TextureManager::kSamplerSlotRepeatModelData]};
		VkDescriptorImageInfo vkSamplerRepeatInfo {.sampler = mrTextureManager.mpSamplersVkSampler[TextureManager::kSamplerSlotRepeat]};
		VkDescriptorImageInfo vkSamplerClampInfo {.sampler = mrTextureManager.mpSamplersVkSampler[TextureManager::kSamplerSlotClamp]};

		VkWriteDescriptorSet pVkWrites[]
		{
			{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = mGlobalDescriptorSets.at(i), .dstBinding = shaders::kiGlobalBindingGlobalUniform, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &vkGlobalBufferInfo},
			{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = mGlobalDescriptorSets.at(i), .dstBinding = shaders::kiGlobalBindingMainUniform, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &vkMainBufferInfo},
			{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = mGlobalDescriptorSets.at(i), .dstBinding = shaders::kiGlobalBindingSamplerRepeatModelData, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER, .pImageInfo = &vkSamplerRepeatModelDataInfo},
			{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = mGlobalDescriptorSets.at(i), .dstBinding = shaders::kiGlobalBindingSamplerRepeat, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER, .pImageInfo = &vkSamplerRepeatInfo},
			{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = mGlobalDescriptorSets.at(i), .dstBinding = shaders::kiGlobalBindingBindlessTextures, .descriptorCount = static_cast<uint32_t>(std::ssize(mImageInfos)), .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .pImageInfo = mImageInfos.data()},
			{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = mGlobalDescriptorSets.at(i), .dstBinding = shaders::kiGlobalBindingSamplerClamp, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER, .pImageInfo = &vkSamplerClampInfo},
		};

		vkUpdateDescriptorSets(gpDeviceManager->mVkDevice, static_cast<uint32_t>(std::size(pVkWrites)), pVkWrites, 0, nullptr);
		SetLightingReference(i, nullptr, false);
	}
}

void TextureDescriptors::SetLightingReference(int64_t iFramebuffer, const Texture* pTexture, bool bEnabled)
{
	*reinterpret_cast<uint32_t*>(mLightingControls.at(iFramebuffer).mpMappedMemory) = bEnabled ? 1ui32 : 0ui32;
	VkDescriptorImageInfo vkDescriptorImageInfo {.sampler = mrTextureManager.mpSamplersVkSampler[TextureManager::kSamplerSlotClamp], .imageView = (pTexture != nullptr ? pTexture : &mLightingFallback)->mVkImageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
	VkDescriptorBufferInfo vkDescriptorBufferInfo {.buffer = mLightingControls.at(iFramebuffer).GetBuffer(), .range = 16};
	VkWriteDescriptorSet pVkWriteDescriptorSets[]
	{
		{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = mGlobalDescriptorSets.at(iFramebuffer), .dstBinding = shaders::kiGlobalBindingLightingReference, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &vkDescriptorImageInfo},
		{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = mGlobalDescriptorSets.at(iFramebuffer), .dstBinding = shaders::kiGlobalBindingLightingControl, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &vkDescriptorBufferInfo},
	};
	vkUpdateDescriptorSets(gpDeviceManager->mVkDevice, static_cast<uint32_t>(std::size(pVkWriteDescriptorSets)), pVkWriteDescriptorSets, 0, nullptr);
}

void TextureDescriptors::AppendPublication(common::crc_t bindingKey, int64_t iSlot)
{
	// Heap: the list persists until every framebuffer applies the entry; appended inside RenderGlobal.
	ScopedSuppressAllocationTracking suppress;
	mPendingPublications.push_back({.bindingKey = bindingKey, .iSlot = iSlot});
}

void TextureDescriptors::PublishFramebuffer(int64_t iFramebuffer)
{
	AssertSetsWritable(iFramebuffer);
	RenderTargetTextures& rTargets = mrTextureManager.mRenderTargetTextures;
	int64_t& riCursor = mAppliedPublicationCursors.at(iFramebuffer);
	for (; riCursor < std::ssize(mPendingPublications); ++riCursor)
	{
		const PendingPublication& rEntry = mPendingPublications.at(riCursor);
		if (rEntry.iSlot >= 0)
		{
			WriteArrayElementFromLive(rTargets.mElevationTextures.data(), rEntry.iSlot, iFramebuffer);
			WriteArrayElementFromLive(rTargets.mColorTextures.data(), rEntry.iSlot, iFramebuffer);
			WriteArrayElementFromLive(rTargets.mNormalsTextures.data(), rEntry.iSlot, iFramebuffer);
			WriteArrayElementFromLive(rTargets.mAmbientOcclusionTextures.data(), rEntry.iSlot, iFramebuffer);
			WriteArrayElementFromLive(rTargets.mMasksTextures.data(), rEntry.iSlot, iFramebuffer);
			continue;
		}

		auto itIndex = mImageInfosMap.find(rEntry.bindingKey);
		if (itIndex != mImageInfosMap.end())
		{
			VkWriteDescriptorSet vkWriteDescriptorSet
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = mGlobalDescriptorSets.at(iFramebuffer),
				.dstBinding = shaders::kiGlobalBindingBindlessTextures,
				.dstArrayElement = static_cast<uint32_t>(itIndex->second),
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.pImageInfo = &mImageInfos.at(itIndex->second),
				.pBufferInfo = nullptr,
				.pTexelBufferView = nullptr,
			};
			vkUpdateDescriptorSets(gpDeviceManager->mVkDevice, 1, &vkWriteDescriptorSet, 0, nullptr);
		}

		// Eviction unregisters a key's bindings, so its replay writes only the Set 0 element above.
		auto itBindings = mTextureBindings.find(rEntry.bindingKey);
		if (itBindings != mTextureBindings.end())
		{
			auto itTexture = mrTextureManager.mTextureMap.find(rEntry.bindingKey);
			const Texture* pKeyTexture = itTexture != mrTextureManager.mTextureMap.end() ? &itTexture->second : nullptr;
			for (const TextureBinding& rBinding : itBindings->second)
			{
				WriteBindingToSet(rBinding, iFramebuffer, pKeyTexture);
			}
		}
	}

	int64_t iAppliedByAll = std::ranges::min(mAppliedPublicationCursors);
	mPendingPublications.erase(mPendingPublications.begin(), mPendingPublications.begin() + iAppliedByAll);
	for (int64_t& riAppliedCursor : mAppliedPublicationCursors)
	{
		riAppliedCursor -= iAppliedByAll;
	}
}

void TextureDescriptors::PublishAllFramebuffers()
{
	AssertSetsWritable(kiAllFramebuffers);
	for (int64_t i = 0; i < std::ssize(mAppliedPublicationCursors); ++i)
	{
		PublishFramebuffer(i);
	}
}

bool TextureDescriptors::HasUnappliedPublications(int64_t iFramebuffer) const
{
	return mAppliedPublicationCursors.at(iFramebuffer) < std::ssize(mPendingPublications);
}

void TextureDescriptors::AssertSetsWritable(int64_t iFramebuffer) const
{
	ASSERT(mbBindlessWriteEpoch && (miBindlessWriteFramebuffer == kiAllFramebuffers || miBindlessWriteFramebuffer == iFramebuffer));
}

void TextureDescriptors::InitializeIslandSlots()
{
	for (int64_t i = 0; i < shaders::kiMaxIslands; ++i)
	{
		mrTextureManager.mRenderTargetTextures.mElevationTextures.at(i) = &mrTextureManager.mIslandPlaceholderElevation;
		mrTextureManager.mRenderTargetTextures.mColorTextures.at(i) = &mrTextureManager.mIslandPlaceholderColor;
		mrTextureManager.mRenderTargetTextures.mNormalsTextures.at(i) = &mrTextureManager.mIslandPlaceholderNormals;
		mrTextureManager.mRenderTargetTextures.mAmbientOcclusionTextures.at(i) = &mrTextureManager.mIslandPlaceholderAmbientOcclusion;
		mrTextureManager.mRenderTargetTextures.mMasksTextures.at(i) = &mrTextureManager.mIslandPlaceholderMasks;
	}
}

void TextureDescriptors::WriteBindingToSet(const TextureBinding& rBinding, int64_t iSet, const Texture* pKeyTexture)
{
	// A multi-set model's inner pipelines borrow pipeline 0's Set 1, which pipeline 0's own registration writes.
	if (rBinding.pPipeline->mVkDescriptorSets.empty())
	{
		return;
	}
	VkSampler vkSampler = mrTextureManager.GetSampler(rBinding.samplerFlags);
	VkDescriptorSet vkDescriptorSet = rBinding.pPipeline->mVkDescriptorSets.at(iSet);

	// Single-element write (per-island-slot bindings): touch only iArrayIndex so other slots'
	// descriptors are not clobbered by stale snapshot pointers.
	if (rBinding.iArrayIndex >= 0)
	{
		WriteImageElement(vkDescriptorSet, rBinding.iBinding, rBinding.iArrayIndex, vkSampler, rBinding.pTexture);
	}
	else if (!rBinding.textures.empty())
	{
		if (pKeyTexture == nullptr)
		{
			WriteFullArrayDescriptors(*rBinding.pPipeline, iSet, rBinding.iBinding, rBinding.textures, vkSampler);
			return;
		}
		for (int64_t i = 0; i < std::ssize(rBinding.textures); ++i)
		{
			if (rBinding.textures.at(i) == pKeyTexture)
			{
				WriteImageElement(vkDescriptorSet, rBinding.iBinding, i, vkSampler, pKeyTexture);
			}
		}
	}
	else if (rBinding.pTexture->mVkImageView != VK_NULL_HANDLE)
	{
		PipelineDescriptorWriter::UpdateImageDescriptorInSet(*rBinding.pPipeline, iSet, rBinding.iBinding, vkSampler, rBinding.pTexture->mVkImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	}
}

void TextureDescriptors::SnapshotBindingGenerations(TextureBinding& rBinding)
{
	if (rBinding.iArrayIndex >= 0)
	{
		Texture* pTexture = rBinding.pTexture;
		rBinding.iTextureGeneration = pTexture != nullptr ? pTexture->miGeneration : 0;
		rBinding.bTextureUsesPlaceholder = pTexture == nullptr || pTexture->mVkImageView == VK_NULL_HANDLE;
		return;
	}

	if (!rBinding.textures.empty())
	{
		int64_t iTextureCount = std::ssize(rBinding.textures);
		rBinding.iTextureGenerations.resize(iTextureCount);
		for (int64_t i = 0; i < iTextureCount; ++i)
		{
			rBinding.iTextureGenerations.at(i) = rBinding.textures.at(i) != nullptr ? rBinding.textures.at(i)->miGeneration : 0;
		}
		SynchronizeFullArrayBindingGenerations(rBinding);
		return;
	}

	if (rBinding.pTexture->mVkImageView != VK_NULL_HANDLE)
	{
		rBinding.iTextureGeneration = rBinding.pTexture->miGeneration;
	}
}

void TextureDescriptors::SynchronizeFullArrayBindingGenerations(const TextureBinding& rBinding)
{
	// Full-array registrations are duplicated once per member CRC (and under CRC 0) so any lazy
	// member can trigger the one shared descriptor write. Keep every distinct duplicate's snapshot aligned
	// with that write; per-island records own one array element and are deliberately excluded.
	ASSERT(rBinding.iArrayIndex < 0);
	ASSERT(!rBinding.textures.empty());
	for (auto& [rCrc, rBindings] : mTextureBindings)
	{
		for (TextureBinding& rOtherBinding : rBindings)
		{
			if (&rOtherBinding == &rBinding || rOtherBinding.pPipeline != rBinding.pPipeline || rOtherBinding.iBinding != rBinding.iBinding
			 || rOtherBinding.iArrayIndex >= 0 || rOtherBinding.textures != rBinding.textures)
			{
				continue;
			}
			ASSERT(std::ssize(rOtherBinding.iTextureGenerations) == std::ssize(rBinding.iTextureGenerations));
			std::copy(rBinding.iTextureGenerations.begin(), rBinding.iTextureGenerations.end(), rOtherBinding.iTextureGenerations.begin());
		}
	}
}

void TextureDescriptors::WriteImageElement(VkDescriptorSet vkDescriptorSet, int64_t iBinding, int64_t iElement, VkSampler vkSampler, const Texture* pTexture)
{
	// Null-view guard alongside the null-pointer guard: a slot mid-reload after eviction has a live
	// Texture whose view is destroyed until AdoptTransferredImage re-attaches it (see the mirrored
	// fallback in PipelineDescriptorWriter's WriteCombinedSamplers).
	VkDescriptorImageInfo vkImageInfo
	{
		.sampler = vkSampler,
		.imageView = pTexture != nullptr && pTexture->mVkImageView != VK_NULL_HANDLE ? pTexture->mVkImageView : mrTextureManager.mWhiteTexture.mVkImageView,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};
	VkWriteDescriptorSet vkWriteDescriptorSet
	{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.pNext = nullptr,
		.dstSet = vkDescriptorSet,
		.dstBinding = static_cast<uint32_t>(iBinding),
		.dstArrayElement = static_cast<uint32_t>(iElement),
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.pImageInfo = &vkImageInfo,
		.pBufferInfo = nullptr,
		.pTexelBufferView = nullptr,
	};
	vkUpdateDescriptorSets(gpDeviceManager->mVkDevice, 1, &vkWriteDescriptorSet, 0, nullptr);
}

void TextureDescriptors::WriteFullArrayDescriptors(const Pipeline& rPipeline, int64_t iSet, int64_t iBinding, std::span<Texture* const> textures, VkSampler vkSampler)
{
	auto pImageInfos = common::gpThreadLocal->mWorkbuffer.PushBuffer<VkDescriptorImageInfo*>(std::ssize(textures) * static_cast<int64_t>(sizeof(VkDescriptorImageInfo)));
	for (int64_t i = 0; i < std::ssize(textures); ++i)
	{
		pImageInfos.mpData[i].sampler = vkSampler;
		// Null-view guard: mid-reload slot (see WriteImageElement)
		pImageInfos.mpData[i].imageView = textures[i] != nullptr && textures[i]->mVkImageView != VK_NULL_HANDLE ? textures[i]->mVkImageView : mrTextureManager.mWhiteTexture.mVkImageView;
		pImageInfos.mpData[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	VkWriteDescriptorSet vkWriteDescriptorSet
	{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.pNext = nullptr,
		.dstSet = rPipeline.mVkDescriptorSets.at(iSet),
		.dstBinding = static_cast<uint32_t>(iBinding),
		.dstArrayElement = 0,
		.descriptorCount = static_cast<uint32_t>(std::ssize(textures)),
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.pImageInfo = pImageInfos.mpData,
		.pBufferInfo = nullptr,
		.pTexelBufferView = nullptr,
	};
	vkUpdateDescriptorSets(gpDeviceManager->mVkDevice, 1, &vkWriteDescriptorSet, 0, nullptr);
}

void TextureDescriptors::WriteArrayElementFromLive(Texture* const* ppArray, int64_t iIndex, int64_t iSet)
{
	// find() + ASSERT (not operator[]): the array must already be registered as a bindless consumer at
	// pipeline-create — same invariant guard as RegisterIslandSlotBindings.
	auto it = mBindlessArrayConsumers.find(ppArray);
	ASSERT(it != mBindlessArrayConsumers.end());

	for (const BindlessArrayConsumer& rConsumer : it->second)
	{
		WriteImageElement(rConsumer.pPipeline->mVkDescriptorSets.at(iSet), rConsumer.iBinding, iIndex, mrTextureManager.GetSampler(rConsumer.samplerFlags), ppArray[iIndex]);
	}
}

void TextureDescriptors::AssertBindlessWriteEpoch() const
{
	ASSERT(mbBindlessWriteEpoch);
}

void TextureDescriptors::RegisterBindlessArrayConsumer(std::span<Texture*> textures, Pipeline* pPipeline, int64_t iBinding, DescriptorFlags_t samplerFlags)
{
	ASSERT(PipelineDescriptorWriter::BindingExistsInShaderLayout(*pPipeline, iBinding));
	mBindlessArrayConsumers.try_emplace(textures.data()).first->second.push_back({.pPipeline = pPipeline, .iBinding = iBinding, .samplerFlags = samplerFlags, .iCount = std::ssize(textures)});

	// PipelineManager rebuilds preserve live island slots but clear pipeline registrations. Re-register owned elements so in-flight lazy adoption reaches the rebuilt Set 1 descriptors.
	RenderTargetTextures& rTargets = mrTextureManager.mRenderTargetTextures;
	for (const auto& [iSlot, rSlot] : mIslandSlots)
	{
		common::crc_t bindingKey = 0;
		if (textures.data() == rTargets.mElevationTextures.data())
		{
			bindingKey = rSlot.islandCrc;
		}
		else if (textures.data() == rTargets.mColorTextures.data())
		{
			bindingKey = rSlot.pTextureCrcs[0];
		}
		else if (textures.data() == rTargets.mNormalsTextures.data())
		{
			bindingKey = rSlot.pTextureCrcs[1];
		}
		else if (textures.data() == rTargets.mAmbientOcclusionTextures.data())
		{
			bindingKey = rSlot.pTextureCrcs[2];
		}
		else if (textures.data() == rTargets.mMasksTextures.data())
		{
			bindingKey = rSlot.pTextureCrcs[3];
		}
		else
		{
			continue;
		}
		RegisterTextureBinding({.crc = bindingKey, .pPipeline = pPipeline, .iBinding = iBinding, .samplerFlags = samplerFlags, .ppTextures = textures.data(), .iCount = std::ssize(textures), .iArrayIndex = iSlot});
	}
}

void TextureDescriptors::RegisterIslandSlotBindings(common::crc_t bindingKey, Texture** ppTextures, int64_t iSlot)
{
	auto it = mBindlessArrayConsumers.find(ppTextures);
	ASSERT(it != mBindlessArrayConsumers.end());
	for (const BindlessArrayConsumer& rConsumer : it->second)
	{
		RegisterTextureBinding({.crc = bindingKey, .pPipeline = rConsumer.pPipeline, .iBinding = rConsumer.iBinding, .samplerFlags = rConsumer.samplerFlags, .ppTextures = ppTextures, .iCount = rConsumer.iCount, .iArrayIndex = iSlot});
	}
}

void TextureDescriptors::MintIslandSlot(int64_t iSlot, common::crc_t islandCrc, Texture& rElevationTexture, const common::crc_t (&textureCrcs)[4])
{
	RenderTargetTextures& rTargets = mrTextureManager.mRenderTargetTextures;
	// Elevation remains at the slot-0 placeholder until all four chunk-backed channels are ready.
	// Keep the real target in IslandSlot so sampler/pipeline recreation cannot expose it early.
	rTargets.mElevationTextures.at(iSlot) = rTargets.mElevationTextures.at(0);

	auto AssignChannel = [&](int64_t iChannel, std::vector<Texture*>& rTextures)
	{
		rTextures.at(iSlot) = &mrTextureManager.mTextureMap.at(textureCrcs[iChannel]);
	};
	AssignChannel(0, rTargets.mColorTextures);
	AssignChannel(1, rTargets.mNormalsTextures);
	AssignChannel(2, rTargets.mAmbientOcclusionTextures);
	AssignChannel(3, rTargets.mMasksTextures);

	// Heap: first-mint adds slot metadata and five binding-key vectors while RenderGlobal may be allocation tracked.
	ScopedSuppressAllocationTracking suppress;
	IslandSlot islandSlot;
	islandSlot.islandCrc = islandCrc;
	islandSlot.pElevationTexture = &rElevationTexture;
	std::copy(std::begin(textureCrcs), std::end(textureCrcs), std::begin(islandSlot.pTextureCrcs));
	mIslandSlots.emplace(iSlot, islandSlot);
	RegisterIslandSlotBindings(islandCrc, rTargets.mElevationTextures.data(), iSlot);
	RegisterIslandSlotBindings(textureCrcs[0], rTargets.mColorTextures.data(), iSlot);
	RegisterIslandSlotBindings(textureCrcs[1], rTargets.mNormalsTextures.data(), iSlot);
	RegisterIslandSlotBindings(textureCrcs[2], rTargets.mAmbientOcclusionTextures.data(), iSlot);
	RegisterIslandSlotBindings(textureCrcs[3], rTargets.mMasksTextures.data(), iSlot);
}

void TextureDescriptors::EvictIslandSlot(int64_t iSlot, common::crc_t islandCrc, const common::crc_t (&textureCrcs)[4])
{
	AssertBindlessWriteEpoch();
	auto it = mIslandSlots.find(iSlot);
	RenderTargetTextures& rTargets = mrTextureManager.mRenderTargetTextures;
	rTargets.mElevationTextures.at(iSlot) = rTargets.mElevationTextures.at(0);
	rTargets.mColorTextures.at(iSlot) = rTargets.mColorTextures.at(0);
	rTargets.mNormalsTextures.at(iSlot) = rTargets.mNormalsTextures.at(0);
	rTargets.mAmbientOcclusionTextures.at(iSlot) = rTargets.mAmbientOcclusionTextures.at(0);
	rTargets.mMasksTextures.at(iSlot) = rTargets.mMasksTextures.at(0);

	for (common::crc_t textureCrc : textureCrcs)
	{
		mImageInfos.at(mImageInfosMap.at(textureCrc)).imageView = mrTextureManager.mWhiteTexture.mVkImageView;
		AppendPublication(textureCrc, -1);
	}
	AppendPublication(0, iSlot);

	ScopedSuppressAllocationTracking suppress;
	UnregisterBindingsForKey(islandCrc);
	for (common::crc_t textureCrc : textureCrcs)
	{
		UnregisterBindingsForKey(textureCrc);
	}
	mIslandSlots.erase(it);
}

void TextureDescriptors::RestoreIslandSlot(common::crc_t islandCrc)
{
	AssertBindlessWriteEpoch();
	for (const auto& [iSlot, rSlot] : mIslandSlots)
	{
		if (rSlot.islandCrc != islandCrc)
		{
			continue;
		}

		auto it = mTextureBindings.find(islandCrc);
		if (it == mTextureBindings.end())
		{
			return;
		}
		mrTextureManager.mRenderTargetTextures.mElevationTextures.at(iSlot) = rSlot.pElevationTexture;
		for (TextureBinding& rBinding : it->second)
		{
			ASSERT(rBinding.iArrayIndex >= 0);
			rBinding.pTexture = rSlot.pElevationTexture;
			SnapshotBindingGenerations(rBinding);
		}
		AppendPublication(islandCrc, -1);
		return;
	}
	ASSERT(false);
}

void TextureDescriptors::RegisterTextureBinding(const TextureBindingInfo& rInfo)
{
	// Validate binding existence at pipeline creation to prevent VUID-00316 during sampler recreation.
	ASSERT(PipelineDescriptorWriter::BindingExistsInShaderLayout(*rInfo.pPipeline, rInfo.iBinding));

	std::vector<Texture*> textures;
	std::vector<int64_t> iTextureGenerations;
	Texture* pTexture = rInfo.pTexture;
	if (rInfo.iArrayIndex >= 0)
	{
		ASSERT(rInfo.ppTextures != nullptr);
		pTexture = rInfo.ppTextures[rInfo.iArrayIndex];
	}
	else if (rInfo.ppTextures != nullptr)
	{
		textures.assign(rInfo.ppTextures, rInfo.ppTextures + rInfo.iCount);
		iTextureGenerations.resize(rInfo.iCount);
		for (int64_t i = 0; i < rInfo.iCount; ++i)
		{
			iTextureGenerations.at(i) = rInfo.ppTextures[i] != nullptr ? rInfo.ppTextures[i]->miGeneration : 0;
		}
	}
	int64_t iTextureGeneration = pTexture != nullptr ? pTexture->miGeneration : 0;
	bool bTextureUsesPlaceholder = rInfo.iArrayIndex >= 0 && (pTexture == nullptr || pTexture->mVkImageView == VK_NULL_HANDLE);
	mTextureBindings.try_emplace(rInfo.crc).first->second.push_back({.pPipeline = rInfo.pPipeline, .iBinding = rInfo.iBinding, .samplerFlags = rInfo.samplerFlags, .pTexture = pTexture, .textures = std::move(textures), .iTextureGeneration = iTextureGeneration, .iTextureGenerations = std::move(iTextureGenerations), .bTextureUsesPlaceholder = bTextureUsesPlaceholder, .iArrayIndex = rInfo.iArrayIndex});
}

void TextureDescriptors::RegisterStandaloneSamplerBinding(Pipeline* pPipeline, int64_t iBinding, DescriptorFlags_t samplerFlags)
{
	ASSERT(PipelineDescriptorWriter::BindingExistsInShaderLayout(*pPipeline, iBinding));

	mStandaloneSamplerBindings.push_back({.pPipeline = pPipeline, .iBinding = iBinding, .samplerFlags = samplerFlags});
}

void TextureDescriptors::UnregisterPipeline(const Pipeline* pPipeline)
{
	for (auto it = mTextureBindings.begin(); it != mTextureBindings.end();)
	{
		std::erase_if(it->second, [pPipeline](const TextureBinding& rBinding)
		{
			return rBinding.pPipeline == pPipeline;
		});
		if (it->second.empty())
		{
			it = mTextureBindings.erase(it);
		}
		else
		{
			++it;
		}
	}

	for (auto it = mBindlessArrayConsumers.begin(); it != mBindlessArrayConsumers.end();)
	{
		std::erase_if(it->second, [pPipeline](const BindlessArrayConsumer& rConsumer)
		{
			return rConsumer.pPipeline == pPipeline;
		});
		if (it->second.empty())
		{
			it = mBindlessArrayConsumers.erase(it);
		}
		else
		{
			++it;
		}
	}

	std::erase_if(mStandaloneSamplerBindings, [pPipeline](const StandaloneSamplerBinding& rBinding)
	{
		return rBinding.pPipeline == pPipeline;
	});
}

void TextureDescriptors::UpdateDescriptorsForTexture(common::crc_t crc)
{
	AssertBindlessWriteEpoch();
	Texture& rTexture = mrTextureManager.mTextureMap.at(crc);

	auto it = mTextureBindings.find(crc);
	if (it != mTextureBindings.end())
	{
		for (TextureBinding& rBinding : it->second)
		{
			SnapshotBindingGenerations(rBinding);
		}
	}

	// CrcToIndex is lock-free; render calls require worker Spawn calls to have joined.
	ASSERT(common::gpThreadLocal != nullptr && !common::gpThreadLocal->mbInFrameTick);
	mImageInfos.at(CrcToIndex(crc)).imageView = rTexture.mVkImageView;
	AppendPublication(crc, -1);
}

void TextureDescriptors::UnregisterBindingsForKey(common::crc_t bindingKey)
{
	// Heap: unordered_map::erase deallocates the bucket's vector<TextureBinding>; called from
	// IslandTerrainResidency::EvictionSweep inside RenderGlobal.
	ScopedSuppressAllocationTracking suppress;
	mTextureBindings.erase(bindingKey);
}

void TextureDescriptors::RewriteSamplerDescriptors()
{
	AssertSetsWritable(kiAllFramebuffers);
	for (const StandaloneSamplerBinding& rBinding : mStandaloneSamplerBindings)
	{
		VkSampler vkSampler = mrTextureManager.GetSampler(rBinding.samplerFlags);
		PipelineDescriptorWriter::UpdateImageDescriptor(*rBinding.pPipeline, rBinding.iBinding, vkSampler, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_SAMPLER);
	}

	for ([[maybe_unused]] auto& [rCrc, rBindings] : mTextureBindings)
	{
		for (TextureBinding& rBinding : rBindings)
		{
			for (int64_t i = 0; i < std::ssize(rBinding.pPipeline->mVkDescriptorSets); ++i)
			{
				WriteBindingToSet(rBinding, i, nullptr);
			}
			SnapshotBindingGenerations(rBinding);
		}
	}

	// IslandTerrainResidency::AcquireTextureSlot mutates live array entries after pipeline creation, so sampler rewrites must use the consumer map's array pointer to avoid stale placeholders.
	// Per-slot TextureBinding entries in the loop above resolve to the same live Texture* and also receive a write.
	for (const auto& [ppLiveArray, rConsumers] : mBindlessArrayConsumers)
	{
		for (const BindlessArrayConsumer& rConsumer : rConsumers)
		{
			VkSampler vkSampler = mrTextureManager.GetSampler(rConsumer.samplerFlags);
			for (int64_t i = 0; i < std::ssize(rConsumer.pPipeline->mVkDescriptorSets); ++i)
			{
				WriteFullArrayDescriptors(*rConsumer.pPipeline, i, rConsumer.iBinding, std::span<Texture* const>(ppLiveArray, static_cast<size_t>(rConsumer.iCount)), vkSampler);
			}
		}
	}
}

void TextureDescriptors::VerifyAllDescriptorGenerations() const
{
	for (const auto& [rCrc, rBindings] : mTextureBindings)
	{
		for (const TextureBinding& rBinding : rBindings)
		{
			if (rBinding.pTexture != nullptr && rBinding.pTexture->miGeneration != 0
			 && ((rBinding.bTextureUsesPlaceholder && rBinding.pTexture->mVkImageView != VK_NULL_HANDLE) || (!rBinding.bTextureUsesPlaceholder && (rBinding.pTexture->miGeneration != rBinding.iTextureGeneration || rBinding.pTexture->mVkImage == VK_NULL_HANDLE))))
			{
				LOG(kGraphics, kError, "Descriptor staleness: pipeline={} binding={} crc={} texture={} snapshotGen={} currentGen={} vkImage={}", rBinding.pPipeline->mInfo.name, rBinding.iBinding, rCrc, reinterpret_cast<uintptr_t>(rBinding.pTexture), rBinding.iTextureGeneration, rBinding.pTexture->miGeneration, reinterpret_cast<uintptr_t>(rBinding.pTexture->mVkImage));
				DEBUG_BREAK();
			}

			ASSERT(std::ssize(rBinding.iTextureGenerations) == std::ssize(rBinding.textures));
			for (int64_t i = 0; i < std::ssize(rBinding.textures); ++i)
			{
				Texture* pTexture = rBinding.textures.at(i);
				if (pTexture != nullptr && pTexture->miGeneration != 0
				 && (pTexture->miGeneration != rBinding.iTextureGenerations.at(i) || pTexture->mVkImage == VK_NULL_HANDLE))
				{
					LOG(kGraphics, kError, "Descriptor staleness (array): pipeline={} binding={} crc={} slot={} texture={} snapshotGen={} currentGen={} vkImage={}", rBinding.pPipeline->mInfo.name, rBinding.iBinding, rCrc, i, reinterpret_cast<uintptr_t>(pTexture), rBinding.iTextureGenerations.at(i), pTexture->miGeneration, reinterpret_cast<uintptr_t>(pTexture->mVkImage));
					DEBUG_BREAK();
				}
			}
		}
	}
}

void TextureDescriptors::ClearTextureBindings()
{
	// Pipeline recreation invalidates only raw Pipeline* registrations. Slot metadata survives so
	// RegisterBindlessArrayConsumer can rebuild per-element records for lazy channels still loading.
	mTextureBindings.clear();
	mBindlessArrayConsumers.clear();
	mStandaloneSamplerBindings.clear();
}

int64_t TextureDescriptors::CrcToIndex(common::crc_t crc)
{
	// Phase exclusion protects mImageInfosMap and miNextTextureIndex: ParticleManager::Spawn serializes worker writes with mSpawnMutex during
	// RunFrameTick dispatch, which fully joins before render writes. UpdateDescriptorsForTexture assert !mbInFrameTick.
	auto it = mImageInfosMap.find(crc);
	if (it != mImageInfosMap.end())
	{
		return it->second;
	}

	// Map insertion may allocate; permanent texture-array CRC indices need heap storage because workbuffer storage is frame-scoped.
	ScopedSuppressAllocationTracking suppress;

	int64_t iIndex = miNextTextureIndex++;
	ASSERT(iIndex < std::ssize(mImageInfos));
	mImageInfosMap.emplace(crc, iIndex);
	return iIndex;
}

} // namespace engine

#endif // BT_CLIENT
