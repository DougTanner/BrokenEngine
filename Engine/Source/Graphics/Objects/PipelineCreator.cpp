#if defined(BT_CLIENT)

#include "PipelineCreator.h"

#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/WrapperBase.h"
#include "Pipeline.h"

namespace engine
{

constexpr float kfDepthBiasConstantFactor = -3.0f;
constexpr float kfDepthBiasSlopeFactor = -3.0f;
constexpr int64_t kiMaxColorAttachments = 6;

// Push-constant range size for a pipeline's layout: the per-pipeline override when set, else the default
// 16-byte PushConstantsLayout. It must match the shader's declared push-constant block.
static int64_t ResolvePushConstantBytes(const Pipeline& rPipeline)
{
	return rPipeline.mInfo.iPushConstantBytes > 0 ? rPipeline.mInfo.iPushConstantBytes : static_cast<int64_t>(sizeof(shaders::PushConstantsLayout));
}

// Configures update-after-bind for storage buffer bindings in dynamic pipelines
static void ConfigureUpdateAfterBind(VkDescriptorSetLayoutCreateInfo& rVkLayoutCreateInfo, std::span<const VkDescriptorSetLayoutBinding> bindings, std::span<VkDescriptorBindingFlags> bindingFlags, VkDescriptorSetLayoutBindingFlagsCreateInfo& rVkBindingFlagsCreateInfo, bool bUpdateAfterBind)
{
	rVkBindingFlagsCreateInfo = VkDescriptorSetLayoutBindingFlagsCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
		.pNext = nullptr,
		.bindingCount = static_cast<uint32_t>(std::ssize(bindings)),
		.pBindingFlags = bindingFlags.data(),
	};

	if (bUpdateAfterBind)
	{
		for (int64_t i = 0; i < std::ssize(bindings); ++i)
		{
			if (bindings[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
			 || bindings[i].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
			 || bindings[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
			{
				bindingFlags[i] = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

				// PARTIALLY_BOUND permits array elements that the shader does not dynamically access to be invalid.
				if (bindings[i].descriptorCount > 1)
				{
					bindingFlags[i] |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
				}
			}
		}
		rVkLayoutCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
		rVkLayoutCreateInfo.pNext = &rVkBindingFlagsCreateInfo;
	}
	else
	{
		rVkLayoutCreateInfo.flags = 0;
		rVkLayoutCreateInfo.pNext = nullptr;
	}
}

static void CreateSingleSetPipelineLayout(VkDescriptorSetLayoutCreateInfo& rVkLayoutCreateInfo, std::span<const VkDescriptorSetLayoutBinding> bindings, Pipeline& rPipeline, VkPipelineLayoutCreateInfo& rVkPipelineLayoutCreateInfo, VkShaderStageFlags vkPushConstantStageFlags)
{
	rVkLayoutCreateInfo.bindingCount = static_cast<uint32_t>(std::ssize(bindings));
	rVkLayoutCreateInfo.pBindings = bindings.data();

	VkDescriptorBindingFlags pVkBindingFlags[common::ShaderHeader::kiMaxDescriptorSetLayoutBindings] {};
	VkDescriptorSetLayoutBindingFlagsCreateInfo vkBindingFlagsCreateInfo {};
	ConfigureUpdateAfterBind(rVkLayoutCreateInfo, bindings, pVkBindingFlags, vkBindingFlagsCreateInfo, rPipeline.mInfo.flags & PipelineFlags::kUpdateAfterBind);

	CHECK_VK(vkCreateDescriptorSetLayout(gpDeviceManager->mVkDevice, &rVkLayoutCreateInfo, nullptr, &rPipeline.mVkDescriptorSetLayout));
	VkName(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, rPipeline.mVkDescriptorSetLayout, rPipeline.mInfo.name.data());

	rVkPipelineLayoutCreateInfo.pSetLayouts = &rPipeline.mVkDescriptorSetLayout;
	rVkPipelineLayoutCreateInfo.pushConstantRangeCount = rPipeline.mInfo.flags & PipelineFlags::kPushConstants ? 1 : 0;
	VkPushConstantRange vkPushConstantRange {};
	vkPushConstantRange.stageFlags = vkPushConstantStageFlags;
	vkPushConstantRange.offset = 0;
	vkPushConstantRange.size = static_cast<uint32_t>(ResolvePushConstantBytes(rPipeline));
	rVkPipelineLayoutCreateInfo.pPushConstantRanges = rPipeline.mInfo.flags & PipelineFlags::kPushConstants ? &vkPushConstantRange : nullptr;
	CHECK_VK(vkCreatePipelineLayout(gpDeviceManager->mVkDevice, &rVkPipelineLayoutCreateInfo, nullptr, &rPipeline.mVkPipelineLayout));
	VkName(VK_OBJECT_TYPE_PIPELINE_LAYOUT, rPipeline.mVkPipelineLayout, rPipeline.mInfo.name.data());
}

// Allocate iSlotCount * iElementSize host-visible/coherent bytes and assert those properties. Zero every indirect-command byte so no slot
// executes before CPU data is written; return the persistent mapping. Graphics draw-indirect and compute dispatch-indirect share this path
// with their respective command types. Device-local draw/dispatch paths remain GPU-written and unmapped.
static void* CreateHostVisibleIndirectBuffer(Pipeline& rPipeline, int64_t iSlotCount, int64_t iElementSize)
{
	VmaAllocationInfo vmaAllocationInfo {};
	Buffer::CreateBuffer(rPipeline.mInfo.name, iSlotCount * iElementSize, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, rPipeline.mIndirectVkBuffer, rPipeline.mIndirectVmaAllocation, &vmaAllocationInfo);

	VkMemoryPropertyFlags vkMemoryPropertyFlags = 0;
	vmaGetAllocationMemoryProperties(gpDeviceManager->mpAllocator, rPipeline.mIndirectVmaAllocation, &vkMemoryPropertyFlags);
	ASSERT((vkMemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0);
	ASSERT((vkMemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0);

	ASSERT(vmaAllocationInfo.pMappedData != nullptr);

	std::memset(vmaAllocationInfo.pMappedData, 0, static_cast<size_t>(iSlotCount * iElementSize));
	return vmaAllocationInfo.pMappedData;
}

static void SetupIndirectBuffer(Pipeline& rPipeline, int64_t iCommandBufferCount)
{
	if (rPipeline.mInfo.flags & PipelineFlags::kIndirectHostVisible)
	{
		rPipeline.miIndirectSlotCount = iCommandBufferCount;
		rPipeline.mpIndirectVkDrawIndexedIndirectCommand = static_cast<VkDrawIndexedIndirectCommand*>(CreateHostVisibleIndirectBuffer(rPipeline, iCommandBufferCount, sizeof(VkDrawIndexedIndirectCommand)));
	}
	else if (rPipeline.mInfo.flags & PipelineFlags::kIndirectDeviceLocal)
	{
		rPipeline.miIndirectSlotCount = iCommandBufferCount;
		Buffer::CreateBuffer(rPipeline.mInfo.name, iCommandBufferCount * static_cast<int64_t>(sizeof(VkDrawIndexedIndirectCommand)), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, rPipeline.mIndirectVkBuffer, rPipeline.mIndirectVmaAllocation);
	}
}

static int64_t MergeReflectedBindings(const Shader& rVertexShader, const Shader& rFragmentShader, VkDescriptorSetLayoutBinding* pVkDescriptorSetLayoutBindings)
{
	int64_t iSourceCount = std::max(rVertexShader.mInformation.pChunkHeader->shaderHeader.iDescriptorSetLayoutBindings, rFragmentShader.mInformation.pChunkHeader->shaderHeader.iDescriptorSetLayoutBindings);
	int64_t iDescriptorCount = 0;
	for (int64_t i = 0; i < iSourceCount; ++i)
	{
		const VkDescriptorSetLayoutBinding& rVkVertexBinding = i < rVertexShader.mInformation.pChunkHeader->shaderHeader.iDescriptorSetLayoutBindings ? rVertexShader.mInformation.pVkDescriptorBindings[i] : Pipeline::kEmptyVkDescriptorSetLayoutBinding;
		const VkDescriptorSetLayoutBinding& rVkFragmentBinding = i < rFragmentShader.mInformation.pChunkHeader->shaderHeader.iDescriptorSetLayoutBindings ? rFragmentShader.mInformation.pVkDescriptorBindings[i] : Pipeline::kEmptyVkDescriptorSetLayoutBinding;

		// Skip empty gap entries - they would all have binding=0 causing duplicates
		if (rVkVertexBinding.descriptorCount == 0 && rVkFragmentBinding.descriptorCount == 0)
		{
			continue;
		}

		ASSERT(iDescriptorCount < common::ShaderHeader::kiMaxDescriptorSetLayoutBindings);
		pVkDescriptorSetLayoutBindings[iDescriptorCount].binding = rVkVertexBinding.binding | rVkFragmentBinding.binding;
		if (rVkVertexBinding.descriptorCount > 0 && rVkFragmentBinding.descriptorCount > 0)
		{
			ASSERT(rVkVertexBinding.descriptorType == rVkFragmentBinding.descriptorType);
		}
		pVkDescriptorSetLayoutBindings[iDescriptorCount].descriptorType = rVkVertexBinding.descriptorCount > 0 ? rVkVertexBinding.descriptorType : rVkFragmentBinding.descriptorType;
		int64_t iBindingDescriptorCount = std::max(rVkVertexBinding.descriptorCount, rVkFragmentBinding.descriptorCount);
		// Runtime-sized arrays exported with UINT32_MAX sentinel; replace with actual texture array size
		if (iBindingDescriptorCount == UINT32_MAX)
		{
			iBindingDescriptorCount = std::ssize(gpTextureManager->mTextureDescriptors.mImageInfos);
		}
		pVkDescriptorSetLayoutBindings[iDescriptorCount].descriptorCount = static_cast<uint32_t>(iBindingDescriptorCount);
		pVkDescriptorSetLayoutBindings[iDescriptorCount].stageFlags = rVkVertexBinding.stageFlags | rVkFragmentBinding.stageFlags;
		pVkDescriptorSetLayoutBindings[iDescriptorCount].pImmutableSamplers = rVkVertexBinding.pImmutableSamplers != nullptr ? rVkVertexBinding.pImmutableSamplers : rVkFragmentBinding.pImmutableSamplers;
		++iDescriptorCount;
	}
	return iDescriptorCount;
}

// CreateDescriptorSetLayouts stage 2: partition the merged bindings into the per-pipeline Set 1 / Set 2
// arrays by shader-reflected set index. Set 0 bindings are handled by the global descriptor set and dropped.
static void SplitBindingsBySet(const Pipeline& rPipeline, std::span<const VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings, VkDescriptorSetLayoutBinding* pVkSet1Bindings, int64_t& riSet1Count, VkDescriptorSetLayoutBinding* pVkSet2Bindings, int64_t& riSet2Count)
{
	riSet1Count = 0;
	riSet2Count = 0;
	for (const VkDescriptorSetLayoutBinding& rVkBinding : descriptorSetLayoutBindings)
	{
		int64_t iBinding = rVkBinding.binding;
		int64_t iSet = Pipeline::ResolveBindingSetIndex(rPipeline.mInfo, iBinding);

		if (iSet == 1)
		{
			pVkSet1Bindings[riSet1Count++] = rVkBinding;
		}
		else if (iSet == 2)
		{
			pVkSet2Bindings[riSet2Count++] = rVkBinding;
		}
	}
}

// CreateDescriptorSetLayouts stage 3: create the per-pipeline Set 1 (+ optional Set 2) descriptor set
// layouts and assemble the [global Set 0, Set 1, optional Set 2] pipeline layout.
static void CreateMultiSetPipelineLayout(Pipeline& rPipeline, VkDescriptorSetLayoutCreateInfo& rVkLayoutCreateInfo, VkPipelineLayoutCreateInfo& rVkPipelineLayoutCreateInfo, const VkPushConstantRange& rVkPushConstantRange, std::span<const VkDescriptorSetLayoutBinding> set1Bindings, std::span<const VkDescriptorSetLayoutBinding> set2Bindings)
{
	// Create Set 1 layout (unless using external layout from first ModelPipeline material)
	if (rPipeline.mExternalSet1VkDescriptorSetLayout == VK_NULL_HANDLE)
	{
		rVkLayoutCreateInfo.bindingCount = static_cast<uint32_t>(std::ssize(set1Bindings));
		rVkLayoutCreateInfo.pBindings = set1Bindings.data();
		VkDescriptorBindingFlags pVkBindingFlagsSet1[common::ShaderHeader::kiMaxDescriptorSetLayoutBindings] {};
		VkDescriptorSetLayoutBindingFlagsCreateInfo vkBindingFlagsCreateInfoSet1 {};
		ConfigureUpdateAfterBind(rVkLayoutCreateInfo, set1Bindings, pVkBindingFlagsSet1, vkBindingFlagsCreateInfoSet1, rPipeline.mInfo.flags & PipelineFlags::kUpdateAfterBind);
		CHECK_VK(vkCreateDescriptorSetLayout(gpDeviceManager->mVkDevice, &rVkLayoutCreateInfo, nullptr, &rPipeline.mVkDescriptorSetLayout));
		VkName(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, rPipeline.mVkDescriptorSetLayout, rPipeline.mInfo.name.data());
	}

	// Create Set 2 layout (kMultiSet models only)
	if (rPipeline.mInfo.flags & PipelineFlags::kMultiSet)
	{
		rVkLayoutCreateInfo.bindingCount = static_cast<uint32_t>(std::ssize(set2Bindings));
		rVkLayoutCreateInfo.pBindings = set2Bindings.data();
		VkDescriptorBindingFlags pVkBindingFlagsSet2[common::ShaderHeader::kiMaxDescriptorSetLayoutBindings] {};
		VkDescriptorSetLayoutBindingFlagsCreateInfo vkBindingFlagsCreateInfoSet2 {};
		ConfigureUpdateAfterBind(rVkLayoutCreateInfo, set2Bindings, pVkBindingFlagsSet2, vkBindingFlagsCreateInfoSet2, rPipeline.mInfo.flags & PipelineFlags::kUpdateAfterBind);
		CHECK_VK(vkCreateDescriptorSetLayout(gpDeviceManager->mVkDevice, &rVkLayoutCreateInfo, nullptr, &rPipeline.mSet2VkDescriptorSetLayout));
		VkName(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, rPipeline.mSet2VkDescriptorSetLayout, rPipeline.mInfo.name.data());
	}

	VkDescriptorSetLayout pVkSetLayouts[3] =
	{
		rPipeline.mExternalVkDescriptorSetLayout,
		rPipeline.mExternalSet1VkDescriptorSetLayout != VK_NULL_HANDLE ? rPipeline.mExternalSet1VkDescriptorSetLayout : rPipeline.mVkDescriptorSetLayout,
		rPipeline.mSet2VkDescriptorSetLayout,
	};
	int64_t iSetCount = (rPipeline.mInfo.flags & PipelineFlags::kMultiSet) ? 3 : 2;
	rVkPipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(iSetCount);
	rVkPipelineLayoutCreateInfo.pSetLayouts = pVkSetLayouts;
	rVkPipelineLayoutCreateInfo.pushConstantRangeCount = rPipeline.mInfo.flags & PipelineFlags::kPushConstants ? 1 : 0;
	rVkPipelineLayoutCreateInfo.pPushConstantRanges = rPipeline.mInfo.flags & PipelineFlags::kPushConstants ? &rVkPushConstantRange : nullptr;
	CHECK_VK(vkCreatePipelineLayout(gpDeviceManager->mVkDevice, &rVkPipelineLayoutCreateInfo, nullptr, &rPipeline.mVkPipelineLayout));
	VkName(VK_OBJECT_TYPE_PIPELINE_LAYOUT, rPipeline.mVkPipelineLayout, rPipeline.mInfo.name.data());
}

static void CreateDescriptorSetLayouts(Pipeline& rPipeline, VkDescriptorSetLayoutCreateInfo& rVkLayoutCreateInfo, VkPipelineLayoutCreateInfo& rVkPipelineLayoutCreateInfo)
{
	VkDescriptorSetLayoutBinding pVkDescriptorSetLayoutBindings[common::ShaderHeader::kiMaxDescriptorSetLayoutBindings] {};
	int64_t iDescriptorCount = MergeReflectedBindings(*rPipeline.mInfo.ppShaders[0], *rPipeline.mInfo.ppShaders[1], pVkDescriptorSetLayoutBindings);

	VkPushConstantRange vkPushConstantRange {};
	vkPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	vkPushConstantRange.offset = 0;
	vkPushConstantRange.size = static_cast<uint32_t>(ResolvePushConstantBytes(rPipeline));

	if (rPipeline.mExternalVkDescriptorSetLayout != VK_NULL_HANDLE)
	{
		// Graphics pipeline with global Set 0: split bindings by shader reflection set index
		VkDescriptorSetLayoutBinding pVkSet1Bindings[common::ShaderHeader::kiMaxDescriptorSetLayoutBindings] {};
		VkDescriptorSetLayoutBinding pVkSet2Bindings[common::ShaderHeader::kiMaxDescriptorSetLayoutBindings] {};
		int64_t iSet1Count = 0;
		int64_t iSet2Count = 0;
		SplitBindingsBySet(rPipeline, std::span<const VkDescriptorSetLayoutBinding>(pVkDescriptorSetLayoutBindings, iDescriptorCount), pVkSet1Bindings, iSet1Count, pVkSet2Bindings, iSet2Count);
		CreateMultiSetPipelineLayout(rPipeline, rVkLayoutCreateInfo, rVkPipelineLayoutCreateInfo, vkPushConstantRange, std::span<const VkDescriptorSetLayoutBinding>(pVkSet1Bindings, iSet1Count), std::span<const VkDescriptorSetLayoutBinding>(pVkSet2Bindings, iSet2Count));
	}
	else
	{
		CreateSingleSetPipelineLayout(rVkLayoutCreateInfo, std::span<const VkDescriptorSetLayoutBinding>(pVkDescriptorSetLayoutBindings, iDescriptorCount), rPipeline, rVkPipelineLayoutCreateInfo, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	}
}

// Owns the Vk*StateCreateInfo structs (and the sub-structs / arrays they point at) for one graphics
// pipeline so they outlive the vkCreateGraphicsPipelines call; the Configure* stages populate it in place.
struct GraphicsPipelineState
{
	VkVertexInputBindingDescription vkVertexInputBindingDescription {};
	VkPipelineVertexInputStateCreateInfo vkPipelineVertexInputStateCreateInfo {};
	VkPipelineInputAssemblyStateCreateInfo vkPipelineInputAssemblyStateCreateInfo {};
	VkViewport vkViewport {};
	VkRect2D vkScissorRect2D {};
	VkPipelineViewportStateCreateInfo vkPipelineViewportStateCreateInfo {};
	VkPipelineRasterizationLineStateCreateInfoEXT vkPipelineRasterizationLineStateCreateInfoEXT {};
	VkPipelineRasterizationStateCreateInfo vkPipelineRasterizationStateCreateInfo {};
	VkPipelineMultisampleStateCreateInfo vkPipelineMultisampleStateCreateInfo {};
	VkPipelineDepthStencilStateCreateInfo vkPipelineDepthStencilStateCreateInfo {};
	VkPipelineColorBlendAttachmentState vkPipelineColorBlendAttachmentState {};
	VkPipelineColorBlendStateCreateInfo vkPipelineColorBlendStateCreateInfo {};
	VkPipelineColorBlendAttachmentState pVkMultipleRenderTargetBlendStates[kiMaxColorAttachments] {};
};

// Builds the vertex-input state: stride + attribute descriptions come from shader reflection (or are
// zeroed for stride-0 fullscreen passes); a bound vertex buffer asserts a matching reflected stride.
static void ConfigureVertexInput(GraphicsPipelineState& rState, const Pipeline& rPipeline)
{
	rState.vkVertexInputBindingDescription = VkVertexInputBindingDescription
	{
		.binding = 0,
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};

	rState.vkPipelineVertexInputStateCreateInfo = VkPipelineVertexInputStateCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &rState.vkVertexInputBindingDescription,
	};

	Shader* pVertexShader = rPipeline.mInfo.ppShaders[0];
	if (rPipeline.mInfo.pVertexBuffer != nullptr)
	{
		ASSERT(pVertexShader->mInformation.pChunkHeader->shaderHeader.iVertexInputStride == rPipeline.mInfo.pVertexBuffer->mInfo.iVertexStride);
		rState.vkVertexInputBindingDescription.stride = static_cast<uint32_t>(pVertexShader->mInformation.pChunkHeader->shaderHeader.iVertexInputStride);
		rState.vkPipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(pVertexShader->mInformation.pChunkHeader->shaderHeader.iVertexInputAttributeDescriptions);
		rState.vkPipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = pVertexShader->mInformation.pVkVertexAttributes;
	}
	else if (pVertexShader->mInformation.pChunkHeader->shaderHeader.iVertexInputStride > 0)
	{
		// Per-draw vertex buffer binding (e.g., per-island terrain meshes): no single canonical
		// vertex buffer is owned by the pipeline. Stride and attributes come from shader reflection;
		// the caller binds the actual buffer via vkCmdBindVertexBuffers at draw time.
		rState.vkVertexInputBindingDescription.stride = static_cast<uint32_t>(pVertexShader->mInformation.pChunkHeader->shaderHeader.iVertexInputStride);
		rState.vkPipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(pVertexShader->mInformation.pChunkHeader->shaderHeader.iVertexInputAttributeDescriptions);
		rState.vkPipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = pVertexShader->mInformation.pVkVertexAttributes;
	}
	else
	{
		rState.vkPipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
		rState.vkPipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr;
		rState.vkPipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
		rState.vkPipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr;
	}
}

static void ConfigureViewportScissor(GraphicsPipelineState& rState, const Pipeline& rPipeline)
{
	rState.vkViewport = VkViewport
	{
		.x = 0.0f,
		.y = 0.0f,
		.minDepth = kfMinDepth,
		.maxDepth = kfMaxDepth,
	};

	rState.vkScissorRect2D = VkRect2D
	{
		.offset = VkOffset2D {.x = 0, .y = 0},
	};

	rState.vkPipelineViewportStateCreateInfo = VkPipelineViewportStateCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.viewportCount = 1,
		.pViewports = &rState.vkViewport,
		.scissorCount = 1,
		.pScissors = &rState.vkScissorRect2D,
	};

	VkExtent2D vkExtent2D
	{
		.width = rPipeline.mInfo.flags & PipelineFlags::kRenderTarget ? rPipeline.mInfo.vkExtent3D.width : gpGraphics->mFramebufferVkExtent2D.width,
		.height = rPipeline.mInfo.flags & PipelineFlags::kRenderTarget ? rPipeline.mInfo.vkExtent3D.height : gpGraphics->mFramebufferVkExtent2D.height,
	};

	// Negative viewport height (VK_KHR_maintenance1) flips the Vulkan Y axis to match DirectX convention
	rState.vkViewport.x = 0.0f;
	rState.vkViewport.y = static_cast<float>(vkExtent2D.height);
	rState.vkViewport.width = static_cast<float>(vkExtent2D.width);
	rState.vkViewport.height = -static_cast<float>(vkExtent2D.height);

	rState.vkScissorRect2D.extent = vkExtent2D;
}

static void ConfigureRasterization(GraphicsPipelineState& rState, const Pipeline& rPipeline)
{
	rState.vkPipelineRasterizationLineStateCreateInfoEXT = VkPipelineRasterizationLineStateCreateInfoEXT
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT,
		.pNext = nullptr,
		.lineRasterizationMode = VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_EXT,
		.stippledLineEnable = VK_FALSE,
		.lineStippleFactor = 0,
		.lineStipplePattern = 0,
	};

	bool bSmoothLines = (rPipeline.mInfo.flags & PipelineFlags::kLineList) && (gpDeviceManager->mCapabilities & DeviceCapabilityFlags::kSmoothLinesEnabled);
	bool bWideLines = (rPipeline.mInfo.flags & PipelineFlags::kLineList) && (gpDeviceManager->mCapabilities & DeviceCapabilityFlags::kWideLinesEnabled);

	rState.vkPipelineRasterizationStateCreateInfo = VkPipelineRasterizationStateCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.pNext = bSmoothLines ? &rState.vkPipelineRasterizationLineStateCreateInfoEXT : nullptr,
		.flags = 0,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, // Negative viewport height reverses winding.
		.lineWidth = bWideLines ? 2.0f : 1.0f,
	};

	if constexpr (kbVulkanWireframe)
	{
		bool bWireframe = gWireframe.Get<bool>();
		if (rPipeline.mInfo.flags & PipelineFlags::kRenderTarget || rPipeline.mInfo.flags & PipelineFlags::kNoWireframe)
		{
			bWireframe = false;
		}
		rState.vkPipelineRasterizationStateCreateInfo.polygonMode = bWireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
	}
	else
	{
		rState.vkPipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	}
	rState.vkPipelineRasterizationStateCreateInfo.cullMode = rPipeline.mInfo.flags & PipelineFlags::kCullBack ? VK_CULL_MODE_BACK_BIT : (rPipeline.mInfo.flags & PipelineFlags::kCullFront ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_NONE);
	if (rPipeline.mInfo.flags & PipelineFlags::kDepthBias)
	{
		rState.vkPipelineRasterizationStateCreateInfo.depthBiasEnable = VK_TRUE;
		rState.vkPipelineRasterizationStateCreateInfo.depthBiasConstantFactor = kfDepthBiasConstantFactor;
		rState.vkPipelineRasterizationStateCreateInfo.depthBiasClamp = 0.0f;
		rState.vkPipelineRasterizationStateCreateInfo.depthBiasSlopeFactor = kfDepthBiasSlopeFactor;
	}
	else
	{
		rState.vkPipelineRasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
		rState.vkPipelineRasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
		rState.vkPipelineRasterizationStateCreateInfo.depthBiasClamp = 0.0f;
		rState.vkPipelineRasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
	}
}

// Builds the multisample state from the global multisampling / sample-shading settings (render targets
// are always single-sample).
static void ConfigureMultisampling(GraphicsPipelineState& rState, const Pipeline& rPipeline)
{
	rState.vkPipelineMultisampleStateCreateInfo = VkPipelineMultisampleStateCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.rasterizationSamples = rPipeline.mInfo.flags & PipelineFlags::kRenderTarget ? VK_SAMPLE_COUNT_1_BIT : (gMultisampling.Get<bool>() ? gSampleCount.Get<VkSampleCountFlagBits>() : VK_SAMPLE_COUNT_1_BIT),
		.sampleShadingEnable = (rPipeline.mInfo.flags & PipelineFlags::kSampleShading && gSampleShading.Get<bool>()) ? VK_TRUE : VK_FALSE,
		.minSampleShading = gMinimumSampleShading.mfCurrent,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable = VK_FALSE,
	};
}

// Builds the color-blend state: per-attachment blend factors from the blend-mode flags, then replicates
// the attachment across all MRT targets (the lighting pass auto-upgrades a single attachment to 3).
static void ConfigureBlendState(GraphicsPipelineState& rState, const Pipeline& rPipeline)
{
	rState.vkPipelineColorBlendAttachmentState = VkPipelineColorBlendAttachmentState
	{
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
	};
	if (rPipeline.mInfo.flags & PipelineFlags::kNoColorWrite)
	{
		rState.vkPipelineColorBlendAttachmentState.colorWriteMask = 0;
	}

	rState.vkPipelineColorBlendStateCreateInfo = VkPipelineColorBlendStateCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.logicOpEnable = VK_FALSE,
		.logicOp = VK_LOGIC_OP_COPY,
		.attachmentCount = 1,
		.pAttachments = &rState.vkPipelineColorBlendAttachmentState,
		.blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
	};

	rState.vkPipelineColorBlendAttachmentState.blendEnable = (rPipeline.mInfo.flags & PipelineFlags::kAlphaBlend || rPipeline.mInfo.flags & PipelineFlags::kAdd || rPipeline.mInfo.flags & PipelineFlags::kAddAlpha || rPipeline.mInfo.flags & PipelineFlags::kMax) ? VK_TRUE : VK_FALSE;
	rState.vkPipelineColorBlendAttachmentState.colorBlendOp = rPipeline.mInfo.flags & PipelineFlags::kMax ? VK_BLEND_OP_MAX : VK_BLEND_OP_ADD;
	rState.vkPipelineColorBlendAttachmentState.alphaBlendOp = rPipeline.mInfo.flags & PipelineFlags::kMax ? VK_BLEND_OP_MAX : VK_BLEND_OP_ADD;
	if (rPipeline.mInfo.flags & PipelineFlags::kAlphaBlend)
	{
		rState.vkPipelineColorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		rState.vkPipelineColorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		rState.vkPipelineColorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		rState.vkPipelineColorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	}
	else if (rPipeline.mInfo.flags & PipelineFlags::kAddAlpha)
	{
		rState.vkPipelineColorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		rState.vkPipelineColorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
		rState.vkPipelineColorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		rState.vkPipelineColorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	}
	else
	{
		rState.vkPipelineColorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		rState.vkPipelineColorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
		rState.vkPipelineColorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		rState.vkPipelineColorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	}

	int64_t iColorAttachmentCount = rPipeline.mInfo.iColorAttachmentCount;
	if (iColorAttachmentCount == 1 && rPipeline.mInfo.vkTargetRenderPass == gpTextureManager->mRenderTargetTextures.mLightingVkRenderPass)
	{
		iColorAttachmentCount = 3;
	}
	ASSERT(iColorAttachmentCount <= kiMaxColorAttachments);
	if (iColorAttachmentCount > 1)
	{
		for (int64_t i = 0; i < iColorAttachmentCount; ++i)
		{
			rState.pVkMultipleRenderTargetBlendStates[i] = rState.vkPipelineColorBlendAttachmentState;
		}
		rState.vkPipelineColorBlendStateCreateInfo.attachmentCount = static_cast<uint32_t>(iColorAttachmentCount);
		rState.vkPipelineColorBlendStateCreateInfo.pAttachments = rState.pVkMultipleRenderTargetBlendStates;
	}
	else
	{
		rState.vkPipelineColorBlendStateCreateInfo.attachmentCount = 1;
		rState.vkPipelineColorBlendStateCreateInfo.pAttachments = &rState.vkPipelineColorBlendAttachmentState;
	}
}

void PipelineCreator::CreateGraphicsPipeline(Pipeline& rPipeline)
{
	ASSERT(rPipeline.mInfo.name.size() > 0);

	// Layout-creation scratch + shader stages stay orchestrator-local; the per-pipeline state structs the
	// create info points at live in the GraphicsPipelineState aggregate below.
	VkDescriptorSetLayoutCreateInfo vkUniformTextureDescriptorSetLayoutCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
	};

	VkPipelineLayoutCreateInfo vkPipelineLayoutCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.setLayoutCount = 1,
	};

	VkPipelineShaderStageCreateInfo pVkPipelineShaderStageCreateInfos[]
	{
		VkPipelineShaderStageCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.pName = "main",
			.pSpecializationInfo = nullptr,
		},
		VkPipelineShaderStageCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.pName = "main",
			.pSpecializationInfo = nullptr,
		},
	};

	GraphicsPipelineState state {};
	state.vkPipelineInputAssemblyStateCreateInfo = VkPipelineInputAssemblyStateCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.topology = rPipeline.mInfo.flags & PipelineFlags::kLineList ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = VK_FALSE,
	};
	state.vkPipelineDepthStencilStateCreateInfo = VkPipelineDepthStencilStateCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.depthCompareOp = VK_COMPARE_OP_LESS,
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable = VK_FALSE,
		.front = {},
		.back = {},
		.minDepthBounds = 0.0f,
		.maxDepthBounds = 0.0f,
	};

	ConfigureVertexInput(state, rPipeline);
	ConfigureViewportScissor(state, rPipeline);
	ConfigureRasterization(state, rPipeline);
	ConfigureMultisampling(state, rPipeline);
	ConfigureBlendState(state, rPipeline);

	state.vkPipelineDepthStencilStateCreateInfo.depthTestEnable = rPipeline.mInfo.flags & PipelineFlags::kDepthTest ? VK_TRUE : VK_FALSE;
	state.vkPipelineDepthStencilStateCreateInfo.depthWriteEnable = rPipeline.mInfo.flags & PipelineFlags::kDepthWrite ? VK_TRUE : VK_FALSE;

	VkGraphicsPipelineCreateInfo vkGraphicsPipelineCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.stageCount = 2,
		.pStages = pVkPipelineShaderStageCreateInfos,
		.pVertexInputState = &state.vkPipelineVertexInputStateCreateInfo,
		.pInputAssemblyState = &state.vkPipelineInputAssemblyStateCreateInfo,
		.pViewportState = &state.vkPipelineViewportStateCreateInfo,
		.pRasterizationState = &state.vkPipelineRasterizationStateCreateInfo,
		.pMultisampleState = &state.vkPipelineMultisampleStateCreateInfo,
		.pDepthStencilState = &state.vkPipelineDepthStencilStateCreateInfo,
		.pColorBlendState = &state.vkPipelineColorBlendStateCreateInfo,
		.pDynamicState = nullptr,
		.basePipelineHandle = VK_NULL_HANDLE,
		.basePipelineIndex = -1,
	};

	// Use max of actual count and 3 to handle swapchain recreation scenarios
	int64_t iFramebufferCount = std::ssize(gpSwapchainManager->mFramebuffers);
	int64_t iCommandBufferCount = std::max(iFramebufferCount, 3i64);
	SetupIndirectBuffer(rPipeline, iCommandBufferCount);

	CreateDescriptorSetLayouts(rPipeline, vkUniformTextureDescriptorSetLayoutCreateInfo, vkPipelineLayoutCreateInfo);

	// Setup pipeline (modules + layout/renderPass resolved after CreateDescriptorSetLayouts)
	pVkPipelineShaderStageCreateInfos[0].module = rPipeline.mInfo.ppShaders[0]->mVkShaderModule;
	pVkPipelineShaderStageCreateInfos[1].module = rPipeline.mInfo.ppShaders[1]->mVkShaderModule;

	vkGraphicsPipelineCreateInfo.layout = rPipeline.mVkPipelineLayout;
	// Default (non-kRenderTarget) scene pipelines render into the F16 HDR intermediate; the resolve pass
	// (itself kRenderTarget with vkTargetRenderPass = mVkRenderPass) writes the swapchain.
	vkGraphicsPipelineCreateInfo.renderPass = rPipeline.mInfo.flags & PipelineFlags::kRenderTarget ? rPipeline.mInfo.vkTargetRenderPass : gpSwapchainManager->mHdrVkRenderPass;

	CHECK_VK(vkCreateGraphicsPipelines(gpDeviceManager->mVkDevice, gpDeviceManager->mVkPipelineCache, 1, &vkGraphicsPipelineCreateInfo, nullptr, &rPipeline.mVkPipeline));
	VkName(VK_OBJECT_TYPE_PIPELINE, rPipeline.mVkPipeline, rPipeline.mInfo.name.data());
}

void PipelineCreator::CreateComputePipeline(Pipeline& rPipeline)
{
	VkDescriptorSetLayoutCreateInfo vkUniformTextureDescriptorSetLayoutCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
	};

	VkPipelineLayoutCreateInfo vkPipelineLayoutCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.setLayoutCount = 1,
	};

	if (rPipeline.mInfo.flags & PipelineFlags::kIndirectHostVisible)
	{
		// Per-framebuffer host-visible dispatch buffer: the CPU writes the active-region workgroup dims each
		// frame (WriteIndirectComputeBuffer), and RecordComputeIndirect reads slot iCommandBuffer — so size it
		// like the graphics indirect path (SetupIndirectBuffer), not the single-slot device-local branch below.
		int64_t iFramebufferCount = std::ssize(gpSwapchainManager->mFramebuffers);
		int64_t iCommandBufferCount = std::max(iFramebufferCount, 3i64);
		rPipeline.miIndirectSlotCount = iCommandBufferCount;
		rPipeline.mpIndirectComputeVkDispatchIndirectCommand = static_cast<VkDispatchIndirectCommand*>(CreateHostVisibleIndirectBuffer(rPipeline, iCommandBufferCount, sizeof(VkDispatchIndirectCommand)));
	}
	else if (rPipeline.mInfo.flags & PipelineFlags::kIndirectDeviceLocal)
	{
		// Single-slot dispatch buffer (always read at offset 0); bounds the RecordComputeIndirect index assert
		rPipeline.miIndirectSlotCount = 1;
		Buffer::CreateBuffer(rPipeline.mInfo.name, sizeof(VkDispatchIndirectCommand), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, rPipeline.mIndirectVkBuffer, rPipeline.mIndirectVmaAllocation);
	}

	ASSERT(!(rPipeline.mInfo.flags & PipelineFlags::kMultiSet)); // Set 2 / multi-material is graphics-only

	Shader* pComputeShader = rPipeline.mInfo.ppShaders[0];

	// Filter out empty entries to avoid duplicate binding 0 errors from zero-initialized gaps
	VkDescriptorSetLayoutBinding pVkDescriptorSetLayoutBindings[common::ShaderHeader::kiMaxDescriptorSetLayoutBindings] {};
	int64_t iSourceCount = pComputeShader->mInformation.pChunkHeader->shaderHeader.iDescriptorSetLayoutBindings;
	int64_t iDescriptorCount = 0;
	for (int64_t i = 0; i < iSourceCount; ++i)
	{
		const VkDescriptorSetLayoutBinding& rVkBinding = pComputeShader->mInformation.pVkDescriptorBindings[i];
		if (rVkBinding.descriptorCount == 0)
		{
			continue;
		}
		ASSERT(iDescriptorCount < common::ShaderHeader::kiMaxDescriptorSetLayoutBindings);
		pVkDescriptorSetLayoutBindings[iDescriptorCount++] = rVkBinding;
	}

	// Compute uses global Set 0 and per-pipeline Set 1 when an external layout and Set 1 bindings exist.
	// Otherwise all reflected bindings use a standalone set, with the external layout cleared for binder and writer.
	VkDescriptorSetLayoutBinding pVkSet1Bindings[common::ShaderHeader::kiMaxDescriptorSetLayoutBindings] {};
	int64_t iSet1Count = 0;
	for (int64_t i = 0; i < iDescriptorCount; ++i)
	{
		int64_t iBinding = pVkDescriptorSetLayoutBindings[i].binding;
		int64_t iSet = Pipeline::ResolveBindingSetIndex(rPipeline.mInfo, iBinding);
		if (iSet == 1)
		{
			pVkSet1Bindings[iSet1Count++] = pVkDescriptorSetLayoutBindings[i];
		}
	}

	if (rPipeline.mExternalVkDescriptorSetLayout != VK_NULL_HANDLE && iSet1Count > 0)
	{
		vkUniformTextureDescriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(iSet1Count);
		vkUniformTextureDescriptorSetLayoutCreateInfo.pBindings = pVkSet1Bindings;
		VkDescriptorBindingFlags pVkBindingFlagsSet1[common::ShaderHeader::kiMaxDescriptorSetLayoutBindings] {};
		VkDescriptorSetLayoutBindingFlagsCreateInfo vkBindingFlagsCreateInfoSet1 {};
		ConfigureUpdateAfterBind(vkUniformTextureDescriptorSetLayoutCreateInfo, std::span<const VkDescriptorSetLayoutBinding>(pVkSet1Bindings, iSet1Count), pVkBindingFlagsSet1, vkBindingFlagsCreateInfoSet1, rPipeline.mInfo.flags & PipelineFlags::kUpdateAfterBind);
		CHECK_VK(vkCreateDescriptorSetLayout(gpDeviceManager->mVkDevice, &vkUniformTextureDescriptorSetLayoutCreateInfo, nullptr, &rPipeline.mVkDescriptorSetLayout));
		VkName(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, rPipeline.mVkDescriptorSetLayout, rPipeline.mInfo.name.data());

		VkDescriptorSetLayout pVkSetLayouts[2] =
		{
			rPipeline.mExternalVkDescriptorSetLayout,
			rPipeline.mVkDescriptorSetLayout,
		};
		VkPushConstantRange vkPushConstantRange {};
		vkPushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		vkPushConstantRange.offset = 0;
		vkPushConstantRange.size = static_cast<uint32_t>(ResolvePushConstantBytes(rPipeline));
		vkPipelineLayoutCreateInfo.setLayoutCount = 2;
		vkPipelineLayoutCreateInfo.pSetLayouts = pVkSetLayouts;
		vkPipelineLayoutCreateInfo.pushConstantRangeCount = rPipeline.mInfo.flags & PipelineFlags::kPushConstants ? 1 : 0;
		vkPipelineLayoutCreateInfo.pPushConstantRanges = rPipeline.mInfo.flags & PipelineFlags::kPushConstants ? &vkPushConstantRange : nullptr;
		CHECK_VK(vkCreatePipelineLayout(gpDeviceManager->mVkDevice, &vkPipelineLayoutCreateInfo, nullptr, &rPipeline.mVkPipelineLayout));
		VkName(VK_OBJECT_TYPE_PIPELINE_LAYOUT, rPipeline.mVkPipelineLayout, rPipeline.mInfo.name.data());
	}
	else
	{
		// Standalone compute, including when global Set 0 is unavailable: shader set-0 bindings form the per-pipeline set. Clear the
		// external-layout pointer so binder and writer treat it as standalone.
		rPipeline.mExternalVkDescriptorSetLayout = VK_NULL_HANDLE;
		CreateSingleSetPipelineLayout(vkUniformTextureDescriptorSetLayoutCreateInfo, std::span<const VkDescriptorSetLayoutBinding>(pVkDescriptorSetLayoutBindings, iDescriptorCount), rPipeline, vkPipelineLayoutCreateInfo, VK_SHADER_STAGE_COMPUTE_BIT);
	}

	VkComputePipelineCreateInfo vkComputePipelineCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.stage =
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_COMPUTE_BIT,
			.module = pComputeShader->mVkShaderModule,
			.pName = "main",
		},
		.layout = rPipeline.mVkPipelineLayout,
	};
	CHECK_VK(vkCreateComputePipelines(gpDeviceManager->mVkDevice, gpDeviceManager->mVkPipelineCache, 1, &vkComputePipelineCreateInfo, nullptr, &rPipeline.mVkPipeline));
	VkName(VK_OBJECT_TYPE_PIPELINE, rPipeline.mVkPipeline, rPipeline.mInfo.name.data());
}

} // namespace engine

#endif // defined(BT_CLIENT)
