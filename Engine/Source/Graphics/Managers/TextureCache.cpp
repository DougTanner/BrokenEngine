#if defined(BT_CLIENT)

#include "TextureCache.h"

#include "Data/Data.h"

namespace engine
{

void TextureCache::CopyImageToHostMemory(VkImage vkSourceImage, VkExtent3D vkExtent, VkFormat vkFormat, uint32_t uiMipmapLevels, uint32_t uiArrayLayers, bool bFromSwapchain, VkImageLayout vkCurrentLayout, std::vector<std::byte>& rOutputData)
{
	// Heap: rOutputData.resize + staging-buffer creation. Main-loop-reachable per frame via the kbScreenshots trigger
	// (Graphics::RenderMainPresentAcquire -> Screenshot::SaveScreenshot -> here) with tracking live; rOutputData
	// is std::move'd into the async save lambda so it cannot use the workbuffer.
	ScopedSuppressAllocationTracking suppress;

	VkPipelineStageFlags vkSourceStage = bFromSwapchain ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	VkPipelineStageFlags vkDestinationStage = bFromSwapchain ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	VkAccessFlags vkSourceAccess = bFromSwapchain ? 0 : VK_ACCESS_SHADER_READ_BIT;
	VkAccessFlags vkDestinationAccess = bFromSwapchain ? 0 : VK_ACCESS_SHADER_READ_BIT;

	int64_t iTotalSize = common::ComputeImageByteSize(vkFormat, vkExtent.width, vkExtent.height, uiMipmapLevels, uiArrayLayers, 1);

	rOutputData.resize(iTotalSize);

	StagingBuffer stagingBuffer("ImageCopyStaging", iTotalSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

	OneShotCommandBuffer oneShotCommandBuffer;

	VkImageMemoryBarrier vkImageMemoryBarrier
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = vkSourceAccess,
		.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
		.oldLayout = vkCurrentLayout,
		.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = vkSourceImage,
		.subresourceRange =
		{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = uiMipmapLevels,
			.baseArrayLayer = 0,
			.layerCount = uiArrayLayers,
		},
	};
	vkCmdPipelineBarrier(oneShotCommandBuffer.mVkCommandBuffer, vkSourceStage, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &vkImageMemoryBarrier);

	VkDeviceSize vkOffset = 0;

	for (uint32_t i = 0; i < uiArrayLayers; ++i)
	{
		int64_t iMipmapWidth = vkExtent.width;
		int64_t iMipmapHeight = vkExtent.height;
		for (uint32_t j = 0; j < uiMipmapLevels; ++j)
		{
			VkBufferImageCopy vkBufferImageCopy
			{
				.bufferOffset = vkOffset,
				.imageSubresource =
				{
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.mipLevel = j,
					.baseArrayLayer = i,
					.layerCount = 1,
				},
				.imageExtent =
				{
					.width = static_cast<uint32_t>(iMipmapWidth),
					.height = static_cast<uint32_t>(iMipmapHeight),
					.depth = 1,
				},
			};

			vkCmdCopyImageToBuffer(oneShotCommandBuffer.mVkCommandBuffer, vkSourceImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer.vkStagingBuffer, 1, &vkBufferImageCopy);

			vkOffset += common::SizeInBytes(vkFormat, iMipmapWidth, iMipmapHeight);
			iMipmapWidth = std::max(iMipmapWidth / 2, 1ll);
			iMipmapHeight = std::max(iMipmapHeight / 2, 1ll);
		}
	}

	vkImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	vkImageMemoryBarrier.newLayout = vkCurrentLayout;
	vkImageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	vkImageMemoryBarrier.dstAccessMask = vkDestinationAccess;
	vkCmdPipelineBarrier(oneShotCommandBuffer.mVkCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, vkDestinationStage, 0, 0, nullptr, 0, nullptr, 1, &vkImageMemoryBarrier);

	oneShotCommandBuffer.Execute();

	// VMA keeps pMappedData valid for the mapped staging allocation's lifetime.
#pragma warning(suppress: 6387)
	std::memcpy(rOutputData.data(), stagingBuffer.vmaAllocationInfo.pMappedData, iTotalSize);
}

void TextureCache::GeneratePhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionLookupTable()
{
	if constexpr (kbRandomlyInvalidatePhysicallyBasedRenderingCubemapCache)
	{
		common::RandomEngine randomEngine(static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
		if (common::Random(10u, randomEngine) == 0)
		{
			LOG(kGraphics, kDebug, "Randomly invalidating GLTF BRDF LUT cache");
			gpFileManager->RemoveFile({FileFlags::kAppDataDirectory}, "BrdfLut.cache");
		}
	}

	VkFormat vkFormat = VK_FORMAT_R16G16_SFLOAT;
	int64_t iSize = 512;

	if (std::filesystem::exists(gpFileManager->GetFilePath({FileFlags::kAppDataDirectory}, "BrdfLut.cache")))
	{
		TextureInfo textureInfo
		{
			.textureFlags = {},
			.name = "PbrLutBrdf",
			.vkImageCreateFlags = {},
			.vkFormat = vkFormat,
			.vkExtent3D = VkExtent3D {.width = static_cast<uint32_t>(iSize), .height = static_cast<uint32_t>(iSize), .depth = 1},
			.uiMipLevels = 1,
			.uiArrayLayers = 1,
			.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
			.vkImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
			.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
			.eTextureLayout = TextureLayout::kShaderReadOnly,
		};
		mPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionLookupTableTexture.Create(textureInfo);

		if (TryLoadCachedTexture("BrdfLut.cache", mPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionLookupTableTexture))
		{
			return;
		}
	}

	mPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionLookupTableTexture.Create(
	{
		.textureFlags = {TextureFlags::kRenderPass},
		.name = "LutBrdf",
		.vkImageCreateFlags = 0,
		.vkFormat = vkFormat,
		.vkExtent3D = VkExtent3D {.width = 512, .height = 512, .depth = 1},
		.uiMipLevels = 1,
		.uiArrayLayers = 1,
		.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
		.vkImageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
		.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
		.vkRenderPassAttachmentLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.vkRenderPassInitialImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.vkRenderPassFinalImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.eTextureLayout = TextureLayout::kColorAttachment,
	});

	Pipeline pipeline(
	{
		.name = "PbrCubemap",
		.flags = {PipelineFlags::kRenderTarget},
		.ppShaders = {&gpPipelineManager->mShaders.at(data::kShadersModelModelGenBrdfLutvertCrc), &gpPipelineManager->mShaders.at(data::kShadersModelModelGenBrdfLutfragCrc)},
		.pVertexBuffer = &gpBufferManager->mQuadsVertexBuffer,
		.vkTargetRenderPass = mPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionLookupTableTexture.mVkRenderPass,
		.vkExtent3D = mPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionLookupTableTexture.mInfo.vkExtent3D,
		.descriptorInfos =
		{
		},
	});

	// Scoped so the render OneShotCommandBuffer is destroyed (releasing sbInUse) before SaveTextureToCache's
	// CopyImageToHostMemory constructs its own — overlapping lifetimes trip the shared-pool assert.
	{
		OneShotCommandBuffer oneShotCommandBuffer;

		mPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionLookupTableTexture.RecordBeginRenderPass(oneShotCommandBuffer.mVkCommandBuffer);
		pipeline.RecordDraw(0, oneShotCommandBuffer.mVkCommandBuffer, 1, 0);
		vkCmdEndRenderPass(oneShotCommandBuffer.mVkCommandBuffer);

		oneShotCommandBuffer.Execute();
	}

	SaveTextureToCache("BrdfLut.cache", mPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionLookupTableTexture);
}

bool TextureCache::TryLoadCachedTexture(const std::filesystem::path& rCachePath, Texture& rTexture, common::crc_t sourceCrc)
{
	// Heap: the cached-payload vector below runs from GeneratePhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionLookupTable in the PipelineManager ctor, which also fires on pipeline-tier recreate (settings change / device loss) with the main-loop tracker armed. Mirrors SaveTextureToCache's CopyImageToHostMemory suppression.
	ScopedSuppressAllocationTracking suppress;

	if (!std::filesystem::exists(gpFileManager->GetFilePath({FileFlags::kAppDataDirectory}, rCachePath)))
	{
		return false;
	}

	std::fstream fileStream = gpFileManager->OpenFile({FileFlags::kAppDataDirectory, FileFlags::kRead}, rCachePath);
	if (!fileStream.is_open())
	{
		return false;
	}

	TextureFileCacheHeader header {};
	fileStream.read(reinterpret_cast<char*>(&header), sizeof(TextureFileCacheHeader));

	if (!fileStream || header.iMagic != TextureFileCacheHeader::kiMagic || header.iVersion != TextureFileCacheHeader::kiVersion || header.vkFormat != rTexture.mInfo.vkFormat || header.iWidth != rTexture.mInfo.vkExtent3D.width || header.iHeight != rTexture.mInfo.vkExtent3D.height || header.iMipmapLevels != rTexture.mInfo.uiMipLevels || header.iArrayLayers != rTexture.mInfo.uiArrayLayers || (sourceCrc != 0 && header.sourceCrc != sourceCrc))
	{
		fileStream.close();
		LOG(kGraphics, kWarning, "Invalid cache file {} (header validation failed), regenerating", rCachePath.string());
		return false;
	}

	// The on-disk iDataSize is opaque (cache file is a trust boundary); validate it against the size computed from the already-validated dims/format before trusting it. A too-small value would overread in the upload memcpy below; a negative value would blow up the std::vector ctor.
	int64_t iExpectedDataSize = common::ComputeImageByteSize(rTexture.mInfo.vkFormat, rTexture.mInfo.vkExtent3D.width, rTexture.mInfo.vkExtent3D.height, rTexture.mInfo.uiMipLevels, rTexture.mInfo.uiArrayLayers, 1);
	if (header.iDataSize != iExpectedDataSize)
	{
		fileStream.close();
		LOG(kGraphics, kWarning, "Invalid cache file {} (iDataSize {} != expected {}), regenerating", rCachePath.string(), header.iDataSize, iExpectedDataSize);
		return false;
	}

	std::vector<std::byte> data(header.iDataSize);
	fileStream.read(reinterpret_cast<char*>(data.data()), header.iDataSize);
	if (!fileStream)
	{
		fileStream.close();
		LOG(kGraphics, kWarning, "Invalid cache file {} (truncated payload), regenerating", rCachePath.string());
		return false;
	}
	fileStream.close();

	// Update texture with cached data. Copy data.size() (== the validated iDataSize) rather than the staging span size so the read can never exceed the buffer we own (the two are equal for the depth-1 textures the cache holds).
	rTexture.UpdateData([&data](std::span<std::byte> destinationData, [[maybe_unused]] int64_t iPosition)
	{
		std::memcpy(destinationData.data(), data.data(), data.size());
	});

	LOG(kLoading, kDebug, "Loaded cached texture from {}", rCachePath.string());
	return true;
}

void TextureCache::SaveTextureToCache(const std::filesystem::path& rCachePath, const Texture& rTexture, common::crc_t sourceCrc)
{
	TextureFileCacheHeader header {};
	header.iMagic = TextureFileCacheHeader::kiMagic;
	header.iVersion = TextureFileCacheHeader::kiVersion;
	header.vkFormat = rTexture.mInfo.vkFormat;
	header.iWidth = rTexture.mInfo.vkExtent3D.width;
	header.iHeight = rTexture.mInfo.vkExtent3D.height;
	header.iMipmapLevels = rTexture.mInfo.uiMipLevels;
	header.iArrayLayers = rTexture.mInfo.uiArrayLayers;
	header.sourceCrc = sourceCrc;

	std::vector<std::byte> data;
	CopyImageToHostMemory(rTexture.mVkImage, rTexture.mInfo.vkExtent3D, rTexture.mInfo.vkFormat, rTexture.mInfo.uiMipLevels, rTexture.mInfo.uiArrayLayers, false, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, data);

	header.iDataSize = static_cast<int64_t>(data.size());

	if (gpFileManager->WriteFileAtomically({FileFlags::kAppDataDirectory, FileFlags::kWrite}, rCachePath, [&](std::fstream& rStream)
	{
		common::Write(rStream, header);
		common::Write(rStream, data);
	}))
	{
		LOG(kGraphics, kDebug, "Saved texture cache to {}", rCachePath.string());
	}
}

} // namespace engine

#endif // BT_CLIENT
