#if defined(BT_CLIENT)

#include "Texture.h"

namespace engine
{

using enum TextureFlags;
using enum TextureLayout;

struct LayoutMapping
{
	VkImageLayout vkImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkAccessFlags vkAccessFlags = 0;
	VkPipelineStageFlags vkPipelineStageFlags = 0;
};

constexpr LayoutMapping kLayoutMappings[]
{
	// kUndefined
	{.vkImageLayout = VK_IMAGE_LAYOUT_UNDEFINED, .vkAccessFlags = VK_ACCESS_NONE_KHR, .vkPipelineStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT},
	// kColorAttachment
	{.vkImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, .vkAccessFlags = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, .vkPipelineStageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
	// kComputeReadOnly
	{.vkImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, .vkAccessFlags = VK_ACCESS_SHADER_READ_BIT, .vkPipelineStageFlags = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT},
	// kComputeReadWrite
	{.vkImageLayout = VK_IMAGE_LAYOUT_GENERAL, .vkAccessFlags = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, .vkPipelineStageFlags = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT},
	// kFragmentShaderReadOnly
	{.vkImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, .vkAccessFlags = VK_ACCESS_SHADER_READ_BIT, .vkPipelineStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT},
	// kGeneral
	{.vkImageLayout = VK_IMAGE_LAYOUT_GENERAL, .vkAccessFlags = VK_ACCESS_MEMORY_READ_BIT, .vkPipelineStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT},
	// kShaderReadOnly
	{.vkImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, .vkAccessFlags = VK_ACCESS_SHADER_READ_BIT, .vkPipelineStageFlags = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT},
	// kTransferDestination
	{.vkImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, .vkAccessFlags = VK_ACCESS_TRANSFER_WRITE_BIT, .vkPipelineStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT},
	// kTransferSource
	{.vkImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, .vkAccessFlags = VK_ACCESS_TRANSFER_READ_BIT, .vkPipelineStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT},
};
static_assert(std::size(kLayoutMappings) == static_cast<size_t>(TextureLayout::kTransferSource) + 1);

VkImageLayout ToVkImageLayout(TextureLayout eLayout)
{
	return kLayoutMappings[static_cast<int>(eLayout)].vkImageLayout;
}

static void CreateImageView(VkImage vkImage, const TextureInfo& rInfo, bool bCheckRenderPass, VkImageView& rVkImageView)
{
	VkComponentMapping vkComponentMapping {.r = VK_COMPONENT_SWIZZLE_R, .g = VK_COMPONENT_SWIZZLE_G, .b = VK_COMPONENT_SWIZZLE_B, .a = VK_COMPONENT_SWIZZLE_A};
	if ((!bCheckRenderPass || !(rInfo.textureFlags & kRenderPass)) && rInfo.vkFormat == VK_FORMAT_BC4_UNORM_BLOCK)
	{
		vkComponentMapping = {.r = VK_COMPONENT_SWIZZLE_R, .g = VK_COMPONENT_SWIZZLE_R, .b = VK_COMPONENT_SWIZZLE_R, .a = VK_COMPONENT_SWIZZLE_R};
	}
	VkImageViewCreateInfo vkImageViewCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.image = vkImage,
		.viewType = rInfo.vkImageViewType,
		.format = rInfo.vkFormat,
		.components = vkComponentMapping,
		.subresourceRange = {.aspectMask = rInfo.vkImageAspectFlags, .baseMipLevel = 0, .levelCount = static_cast<uint32_t>(rInfo.iMipLevels), .baseArrayLayer = 0, .layerCount = static_cast<uint32_t>(rInfo.iArrayLayers)},
	};
	CHECK_VK(vkCreateImageView(gpDeviceManager->mVkDevice, &vkImageViewCreateInfo, nullptr, &rVkImageView));
	VkName(VK_OBJECT_TYPE_IMAGE_VIEW, rVkImageView, rInfo.name.data());
}

void Texture::RecordBeginRenderPass(VkCommandBuffer vkCommandBuffer, VkRenderPass vkRenderPass, VkFramebuffer vkFramebuffer, VkExtent2D vkExtent2D, VkClearColorValue vkClearColorValue, RenderPassFlags_t renderPassFlags)
{
	VkClearValue pVkClearValues[] =
	{
		{.color = vkClearColorValue},
		{.depthStencil = {.depth = kfMaxDepth, .stencil = 0}},
		{.color = {.float32 = {0.0f, 0.0f, 0.0f, 0.0f}}},
	};

	int64_t iAttachmentCount = 1;
	if (renderPassFlags & RenderPassFlags::kDepth)
	{
		++iAttachmentCount;
	}
	if (renderPassFlags & RenderPassFlags::kMultisampling)
	{
		++iAttachmentCount;
	}

	bool bClear = renderPassFlags & RenderPassFlags::kClear;
	VkRenderPassBeginInfo vkRenderPassBeginInfo
	{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.pNext = nullptr,
		.renderPass = vkRenderPass,
		.framebuffer = vkFramebuffer,
		.renderArea =
		{
			.offset = {.x = 0, .y = 0},
			.extent = vkExtent2D,
		},
		.clearValueCount = bClear ? static_cast<uint32_t>(iAttachmentCount) : 0,
		.pClearValues = bClear ? pVkClearValues : nullptr,
	};
	vkCmdBeginRenderPass(vkCommandBuffer, &vkRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

Texture::Texture(const TextureInfo& rInfo, const std::function<void(std::span<std::byte>, int64_t)>& rDataFunction)
{
	Create(rInfo, rDataFunction);
}

Texture::~Texture()
{
	Destroy();
}

void Texture::InitDeferred(const TextureInfo& rInfo, VkImageView vkPlaceholderImageView)
{
	mInfo = rInfo;
	mVkImageView = vkPlaceholderImageView;
	// mVkImage stays VK_NULL_HANDLE - no GPU allocation
	// Destroy() early-returns when mVkImage == VK_NULL_HANDLE, so the borrowed placeholder view is never freed
}

void Texture::AdoptTransferredImage(VkImage& rVkImage, VmaAllocation& rVmaAllocation)
{
	Destroy();

	mVkImage = std::exchange(rVkImage, VK_NULL_HANDLE);
	mVmaAllocation = std::exchange(rVmaAllocation, VK_NULL_HANDLE);

	CreateImageView(mVkImage, mInfo, false, mVkImageView);
	++miGeneration;
}

void Texture::RecordAcquireBarrier(VkCommandBuffer vkCommandBuffer)
{
	bool bQueueFamilyOwnershipTransferOptional = gpDeviceManager->mCapabilities & DeviceCapabilityFlags::kTransferQueueFamilyOwnershipTransferOptional;

	VkImageMemoryBarrier vkImageMemoryBarrier
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.pNext = nullptr,
		.srcAccessMask = 0,
		.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
		.oldLayout = bQueueFamilyOwnershipTransferOptional ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.srcQueueFamilyIndex = bQueueFamilyOwnershipTransferOptional ? VK_QUEUE_FAMILY_IGNORED : static_cast<uint32_t>(gpInstanceManager->miTransferQueueFamilyIndex),
		.dstQueueFamilyIndex = bQueueFamilyOwnershipTransferOptional ? VK_QUEUE_FAMILY_IGNORED : static_cast<uint32_t>(gpInstanceManager->miGraphicsQueueFamilyIndex),
		.image = mVkImage,
		.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = static_cast<uint32_t>(mInfo.iMipLevels), .baseArrayLayer = 0, .layerCount = static_cast<uint32_t>(mInfo.iArrayLayers)},
	};
	vkCmdPipelineBarrier(vkCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &vkImageMemoryBarrier);
}

void Texture::Create(const TextureInfo& rInfo, const std::function<void(std::span<std::byte>, int64_t)>& rDataFunction)
{
	Destroy();

	mInfo = rInfo;

	VkImageCreateInfo vkImageCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = mInfo.vkImageCreateFlags,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = mInfo.vkFormat,
		.extent = mInfo.vkExtent3D,
		.mipLevels = static_cast<uint32_t>(mInfo.iMipLevels),
		.arrayLayers = static_cast<uint32_t>(mInfo.iArrayLayers),
		.samples = mInfo.vkSampleCountFlagBits,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = mInfo.vkImageUsageFlags,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};
	VmaAllocationCreateInfo vmaAllocationCreateInfo {};
	vmaAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

	if (mInfo.textureFlags & kRenderPass)
	{
		// Force dedicated allocations for render targets with at least 32 MiB of base-level array data; VMA chooses allocations otherwise.
		static constexpr int64_t kiLargeSizeThreshold = 32 * 1'024 * 1'024;
		if (common::SizeInBytes(mInfo.vkFormat, mInfo.vkExtent3D.width, mInfo.vkExtent3D.height) * mInfo.iArrayLayers >= kiLargeSizeThreshold)
		{
			vmaAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
		}
	}

	CHECK_VK(vmaCreateImage(gpDeviceManager->mpAllocator, &vkImageCreateInfo, &vmaAllocationCreateInfo, &mVkImage, &mVmaAllocation, nullptr));
	VkName(VK_OBJECT_TYPE_IMAGE, mVkImage, mInfo.name.data());

	CreateImageView(mVkImage, mInfo, true, mVkImageView);
	++miGeneration;

	if (rDataFunction != nullptr)
	{
		UploadImageData(rDataFunction, kUndefined, mInfo.eTextureLayout);
	}

	if (mInfo.textureFlags & kRenderPass)
	{
		CreateRenderTarget();
	}

	// Transition to final layout only if no rDataFunction was provided (rDataFunction case handles this above)
	if (rDataFunction == nullptr && mInfo.eTextureLayout != kUndefined)
	{
		OneShotCommandBuffer oneShotCommandBuffer;
		TransitionImageLayout(oneShotCommandBuffer.mVkCommandBuffer, kUndefined, mInfo.eTextureLayout);
		oneShotCommandBuffer.Execute();
	}
}

void Texture::UpdateData(const std::function<void(std::span<std::byte>, int64_t)>& rDataFunction)
{
	ASSERT(mVkImage != VK_NULL_HANDLE);
	UploadImageData(rDataFunction, kShaderReadOnly, kShaderReadOnly);
}

void Texture::UploadImageData(const std::function<void(std::span<std::byte>, int64_t)>& rDataFunction, TextureLayout eOldLayout, TextureLayout eFinalLayout)
{
	int64_t iSize = common::ComputeImageByteSize(mInfo.vkFormat, mInfo.vkExtent3D.width, mInfo.vkExtent3D.height, mInfo.iMipLevels, mInfo.iArrayLayers, mInfo.vkExtent3D.depth);

	StagingBuffer stagingBuffer(mInfo.name, iSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	rDataFunction(std::span<std::byte>(static_cast<std::byte*>(stagingBuffer.vmaAllocationInfo.pMappedData), static_cast<size_t>(iSize)), 0);

	OneShotCommandBuffer oneShotCommandBuffer;

	TransitionImageLayout(oneShotCommandBuffer.mVkCommandBuffer, eOldLayout, kTransferDestination);

	int64_t iOffset = 0;
	for (int64_t i = 0; i < mInfo.iArrayLayers; ++i)
	{
		int64_t iWidth = mInfo.vkExtent3D.width;
		int64_t iHeight = mInfo.vkExtent3D.height;
		for (int64_t j = 0; j < mInfo.iMipLevels; ++j)
		{
			VkBufferImageCopy vkBufferImageCopy {};
			vkBufferImageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			vkBufferImageCopy.imageSubresource.mipLevel = static_cast<uint32_t>(j);
			vkBufferImageCopy.imageSubresource.baseArrayLayer = static_cast<uint32_t>(i);
			vkBufferImageCopy.imageSubresource.layerCount = 1;
			vkBufferImageCopy.imageExtent.width = static_cast<uint32_t>(iWidth);
			vkBufferImageCopy.imageExtent.height = static_cast<uint32_t>(iHeight);
			vkBufferImageCopy.imageExtent.depth = 1;
			vkBufferImageCopy.bufferOffset = static_cast<VkDeviceSize>(iOffset);

			iOffset += common::SizeInBytes(mInfo.vkFormat, iWidth, iHeight);
			iWidth = std::max(1i64, iWidth / 2);
			iHeight = std::max(1i64, iHeight / 2);

			vkCmdCopyBufferToImage(oneShotCommandBuffer.mVkCommandBuffer, stagingBuffer.vkStagingBuffer, mVkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &vkBufferImageCopy);
		}
	}

	if (eFinalLayout != kUndefined)
	{
		TransitionImageLayout(oneShotCommandBuffer.mVkCommandBuffer, kTransferDestination, eFinalLayout);
	}

	oneShotCommandBuffer.Execute();
}

void Texture::CreateRenderTarget()
{
	VkAttachmentDescription pVkAttachmentDescriptions[]
	{
		VkAttachmentDescription
		{
			.flags = 0,
			.format = mInfo.vkFormat,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = mInfo.vkRenderPassAttachmentLoadOp,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = mInfo.vkRenderPassInitialImageLayout,
			.finalLayout = mInfo.vkRenderPassFinalImageLayout,
		},
	};
	VkAttachmentReference vkAttachmentReference
	{
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};
	VkSubpassDescription vkSubpassDescription
	{
		.flags = 0,
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.inputAttachmentCount = 0,
		.pInputAttachments = nullptr,
		.colorAttachmentCount = 1,
		.pColorAttachments = &vkAttachmentReference,
		.pResolveAttachments = nullptr,
		.pDepthStencilAttachment = nullptr,
		.preserveAttachmentCount = 0,
		.pPreserveAttachments = nullptr,
	};
	// This VK_SUBPASS_EXTERNAL subpass dependency, combined with mInfo.vkRenderPassFinalImageLayout, will insert an implicit pipeline barrier at the end of the render pass
	VkSubpassDependency vkSubpassDependency
	{
		.srcSubpass = 0,
		.dstSubpass = VK_SUBPASS_EXTERNAL,
		.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstStageMask = mInfo.vkRenderPassDestinationStageMask,
		.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.dstAccessMask = mInfo.vkRenderPassDestinationAccessMask,
		.dependencyFlags = 0,
	};
	VkRenderPassCreateInfo vkRenderPassCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.attachmentCount = 1,
		.pAttachments = pVkAttachmentDescriptions,
		.subpassCount = 1,
		.pSubpasses = &vkSubpassDescription,
		.dependencyCount = 1,
		.pDependencies = &vkSubpassDependency,
	};
	CHECK_VK(vkCreateRenderPass(gpDeviceManager->mVkDevice, &vkRenderPassCreateInfo, nullptr, &mVkRenderPass));
	VkName(VK_OBJECT_TYPE_RENDER_PASS, mVkRenderPass, mInfo.name.data());
	VkImageView pVkImageViews[] {mVkImageView};
	VkFramebufferCreateInfo vkFramebufferCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.renderPass = mVkRenderPass,
		.attachmentCount = 1,
		.pAttachments = pVkImageViews,
		.width = mInfo.vkExtent3D.width,
		.height = mInfo.vkExtent3D.height,
		.layers = 1,
	};
	CHECK_VK(vkCreateFramebuffer(gpDeviceManager->mVkDevice, &vkFramebufferCreateInfo, nullptr, &mVkFramebuffer));
	VkName(VK_OBJECT_TYPE_FRAMEBUFFER, mVkFramebuffer, mInfo.name.data());
}

void Texture::Destroy() noexcept
{
	if (mVkImage == VK_NULL_HANDLE)
	{
		return;
	}

	vkDestroyImageView(gpDeviceManager->mVkDevice, mVkImageView, nullptr);
	mVkImageView = VK_NULL_HANDLE;

	vmaDestroyImage(gpDeviceManager->mpAllocator, mVkImage, mVmaAllocation);
	mVkImage = VK_NULL_HANDLE;
	mVmaAllocation = VK_NULL_HANDLE;

	if (mVkRenderPass != VK_NULL_HANDLE)
	{
		vkDestroyFramebuffer(gpDeviceManager->mVkDevice, mVkFramebuffer, nullptr);
		mVkFramebuffer = VK_NULL_HANDLE;
		vkDestroyRenderPass(gpDeviceManager->mVkDevice, mVkRenderPass, nullptr);
		mVkRenderPass = VK_NULL_HANDLE;
	}
}

void Texture::TransitionImageLayout(VkCommandBuffer vkCommandBuffer, TextureLayout eOldLayout, TextureLayout eNewLayout)
{
	const LayoutMapping& rSource = kLayoutMappings[static_cast<int>(eOldLayout)];
	const LayoutMapping& rDestination = kLayoutMappings[static_cast<int>(eNewLayout)];

	VkImageMemoryBarrier vkImageMemoryBarrier
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.pNext = nullptr,
		.srcAccessMask = rSource.vkAccessFlags,
		.dstAccessMask = rDestination.vkAccessFlags,
		.oldLayout = rSource.vkImageLayout,
		.newLayout = rDestination.vkImageLayout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = mVkImage,
		.subresourceRange =
		{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = static_cast<uint32_t>(mInfo.iMipLevels),
			.baseArrayLayer = 0,
			.layerCount = static_cast<uint32_t>(mInfo.iArrayLayers),
		},
	};

	vkCmdPipelineBarrier(vkCommandBuffer, rSource.vkPipelineStageFlags, rDestination.vkPipelineStageFlags, 0, 0, nullptr, 0, nullptr, 1, &vkImageMemoryBarrier);
}

void Texture::RecordCopyImageFrom(VkCommandBuffer vkCommandBuffer, const Texture& rSource)
{
	VkImageCopy vkImageCopy
	{
		.srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
		.srcOffset = {.x = 0, .y = 0, .z = 0},
		.dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
		.dstOffset = {.x = 0, .y = 0, .z = 0},
		.extent = mInfo.vkExtent3D,
	};
	vkCmdCopyImage(vkCommandBuffer, rSource.mVkImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mVkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &vkImageCopy);
}

void Texture::RecordBeginRenderPass(VkCommandBuffer vkCommandBuffer)
{
	RenderPassFlags_t renderPassFlags;
	if (mInfo.vkRenderPassAttachmentLoadOp == VK_ATTACHMENT_LOAD_OP_CLEAR)
	{
		renderPassFlags.Set(RenderPassFlags::kClear);
	}
	RecordBeginRenderPass(vkCommandBuffer, mVkRenderPass, mVkFramebuffer, {.width = mInfo.vkExtent3D.width, .height = mInfo.vkExtent3D.height}, mInfo.vkRenderPassClearColorValue, renderPassFlags);
}

} // namespace engine

#endif // defined(BT_CLIENT)
