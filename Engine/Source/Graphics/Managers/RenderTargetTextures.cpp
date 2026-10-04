#if defined(BT_CLIENT)

#include "RenderTargetTextures.h"

#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/ShadowWrappersBase.h"
#include "TextureManager.h"

namespace engine
{

void RenderTargetTextures::Create()
{
	CreateLightingTextures();
	CreateShadowTextures();
	CreateSmokeTextures();
	CreateWindTextures();
	CreateObjectShadowsTextures();
	CreateTerrainTextures();
	CreateWaterDisplacementTextures();
}

void RenderTargetTextures::CreateWaterDisplacementTextures()
{
	// Both water pipelines sample Gerstner displacement and normals computed by WaterDisplacement.comp.
	// The texel grid matches the LOD0 vertex grid built in BufferManager::CreateWaterMesh via WaterDetailTextureSize.
	auto [iWaterX, iWaterY] = gpTextureManager->WaterDetailTextureSize(gWaterShapeDetail.mfCurrent);
	mWaterDisplacementTexture.Create(
	{
		.textureFlags = {},
		.name = "WaterDisplacement",
		.vkImageCreateFlags = 0,
		.vkFormat = shaders::kVkFormatWaterDisplacement,
		.vkExtent3D = VkExtent3D {.width = static_cast<uint32_t>(iWaterX), .height = static_cast<uint32_t>(iWaterY), .depth = 1},
		.uiMipLevels = 1,
		.uiArrayLayers = 1,
		.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
		.vkImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // TRANSFER_SRC: agent dump_render_target readback
		.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
		.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
		.eTextureLayout = TextureLayout::kShaderReadOnly,
	});
	mWaterDisplacementNormalTexture.Create(
	{
		.textureFlags = {},
		.name = "WaterDisplacementNormal",
		.vkImageCreateFlags = 0,
		.vkFormat = shaders::kVkFormatWaterDisplacement,
		.vkExtent3D = VkExtent3D {.width = static_cast<uint32_t>(iWaterX), .height = static_cast<uint32_t>(iWaterY), .depth = 1},
		.uiMipLevels = 1,
		.uiArrayLayers = 1,
		.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
		.vkImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // TRANSFER_SRC: agent dump_render_target readback
		.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
		.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
		.eTextureLayout = TextureLayout::kShaderReadOnly,
	});
}

void RenderTargetTextures::CreateShadowTextures()
{
	// Fixed-world-size shadow texels: pre-size the texture to cover the visible area at the reference eye
	// height so the texel scale is constant across zoom (no pop/shimmer); closer zoom uses a centered
	// sub-window, farther crops. Clamp AFTER the multiply (DetailTextureSize clamps pre-multiply); cap the
	// width at (limit/3)*2 so the 1.5x-wide elevation texture still fits maxImageDimension2D. Force the
	// width even (below) so the 1.5x elevation width stays integer; dispatch ceil-divides so no block multiple.

	// Re-arm the temporal first-frame guard: mShadowHistoryTexture below is (re)created with undefined contents,
	// so PopulateShadowParameters must blend pure-current for one frame before reusing history. Mirrors gbSmokeClear
	// (and CreateWindTextures' creation-time hard clear).
	gbShadowTemporalReset = true;

	auto [iBaseX, iBaseY] = TextureManager::DetailTextureSize(gShadowRenderMultiplier.mfCurrent);
	int64_t iLimit = static_cast<int64_t>(gpInstanceManager->mVkPhysicalDeviceProperties.limits.maxImageDimension2D);
	int64_t iShadowTextureX = std::min(static_cast<int64_t>(std::lround(static_cast<float>(iBaseX) * engine::Camera::kfShadowHeadroomMultiplier)), (iLimit / 3) * 2);
	iShadowTextureX &= ~1i64;
	int64_t iShadowTextureY = std::min(static_cast<int64_t>(std::lround(static_cast<float>(iBaseY) * engine::Camera::kfShadowHeadroomMultiplier)), iLimit);
	LOG(kGraphics, kDebug, "iShadowTexture: {} x {}", iShadowTextureX, iShadowTextureY);
	mShadowElevationTexture.Create(
	{
		.textureFlags = {TextureFlags::kRenderPass},
		.name = "ShadowElevation",
		.vkImageCreateFlags = 0,
		.vkFormat = shaders::kVkFormatElevation,
		.vkExtent3D = VkExtent3D {.width = static_cast<uint32_t>(iShadowTextureX + iShadowTextureX / 2), .height = static_cast<uint32_t>(iShadowTextureY), .depth = 1},
		.uiMipLevels = 1,
		.uiArrayLayers = 1,
		.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
		.vkImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // TRANSFER_SRC: agent dump_render_target readback
		.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
		.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
		.vkRenderPassAttachmentLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.vkRenderPassFinalImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.vkRenderPassClearColorValue = {gpIslandTerrain->mfSeaFloorElevation, 0.0f, 0.0f, 1.0f},
		.eTextureLayout = TextureLayout::kShaderReadOnly,
		.vkRenderPassDestinationStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
	});
	mShadowTexture.Create(
	{
		.textureFlags = {},
		.name = "Shadow",
		.vkImageCreateFlags = 0,
		.vkFormat = shaders::kVkFormatShadow,
		.vkExtent3D = VkExtent3D {.width = static_cast<uint32_t>(iShadowTextureX), .height = static_cast<uint32_t>(iShadowTextureY), .depth = 1},
		.uiMipLevels = 1,
		.uiArrayLayers = 1,
		.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
		.vkImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // TRANSFER_SRC: agent dump_render_target readback
		.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
		.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
		.eTextureLayout = TextureLayout::kComputeReadWrite,
	});
	mShadowBlurTexture.Create(
	{
		.textureFlags = {},
		.name = "ShadowBlur",
		.vkImageCreateFlags = 0,
		.vkFormat = shaders::kVkFormatShadow,
		.vkExtent3D = VkExtent3D {.width = static_cast<uint32_t>(iShadowTextureX), .height = static_cast<uint32_t>(iShadowTextureY), .depth = 1},
		.uiMipLevels = 1,
		.uiArrayLayers = 1,
		.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
		.vkImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // TRANSFER_SRC: agent dump_render_target readback
		.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
		.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
		.eTextureLayout = TextureLayout::kShaderReadOnly,
	});
	mShadowBlurIntermediateTexture.Create(
	{
		.textureFlags = {},
		.name = "ShadowBlurIntermediate",
		.vkImageCreateFlags = 0,
		.vkFormat = shaders::kVkFormatShadow,
		.vkExtent3D = VkExtent3D {.width = static_cast<uint32_t>(iShadowTextureX), .height = static_cast<uint32_t>(iShadowTextureY), .depth = 1},
		.uiMipLevels = 1,
		.uiArrayLayers = 1,
		.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
		.vkImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // TRANSFER_SRC: agent dump_render_target readback
		.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
		.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
		.eTextureLayout = TextureLayout::kShaderReadOnly,
	});
	mShadowHistoryTexture.Create(
	{
		.textureFlags = {},
		.name = "ShadowHistory",
		.vkImageCreateFlags = 0,
		.vkFormat = shaders::kVkFormatShadow,
		.vkExtent3D = VkExtent3D {.width = static_cast<uint32_t>(iShadowTextureX), .height = static_cast<uint32_t>(iShadowTextureY), .depth = 1},
		.uiMipLevels = 1,
		.uiArrayLayers = 1,
		.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
		.vkImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // TRANSFER_SRC: agent dump_render_target readback
		.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
		.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
		.eTextureLayout = TextureLayout::kShaderReadOnly,
	});
}

void RenderTargetTextures::CreateSmokeTextures()
{
	gbSmokeClear = true;

	int64_t iGradientSize = 128;
	mSmokeGradientTexture.Create(
	{
		.textureFlags = {},
		.name = "SmokeTrailGradient",
		.vkImageCreateFlags = 0,
		.vkFormat = VK_FORMAT_R16_UNORM,
		.vkExtent3D = VkExtent3D {.width = static_cast<uint32_t>(iGradientSize), .height = static_cast<uint32_t>(iGradientSize), .depth = 1},
		.uiMipLevels = 1,
		.uiArrayLayers = 1,
		.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
		.vkImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // TRANSFER_SRC: agent dump_render_target readback
		.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
		.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
		.eTextureLayout = TextureLayout::kShaderReadOnly,
	},
	[&](std::span<std::byte> data, [[maybe_unused]] int64_t iPosition)
	{
		float fCenter = static_cast<float>(iGradientSize) * 0.5f;
		float fPower = gSmokeTrailPower.mfCurrent;
		float fAlpha = gSmokeTrailAlpha.mfCurrent;

		uint16_t* puiColor = reinterpret_cast<uint16_t*>(data.data());
		for (int64_t j = 0; j < iGradientSize; ++j)
		{
			for (int64_t i = 0; i < iGradientSize; ++i)
			{
				float fX = -fCenter + 0.5f + static_cast<float>(i);
				float fY = fCenter - 0.5f - static_cast<float>(j);
				float fDistance = std::pow(std::sqrt(fX * fX + fY * fY) / fCenter, fPower);
				float fIntensity = 1.0f - std::pow(fDistance, fAlpha) / (std::pow(fDistance, fAlpha) + std::pow((1.0f - fDistance), fAlpha));

				uint16_t uiRed = static_cast<uint16_t>(static_cast<float>(std::numeric_limits<uint16_t>::max()) * fIntensity);
				puiColor[j * iGradientSize + i] = uiRed;
			}
		}
	});

	TextureInfo smokeTextureInfo
	{
		.textureFlags = {TextureFlags::kRenderPass},
		.name = "SmokeOne",
		.vkImageCreateFlags = 0,
		.vkFormat = shaders::kVkFormatSmoke,
		.vkExtent3D = VkExtent3D {.width = static_cast<uint32_t>(SmokeSimulationPixels()), .height = static_cast<uint32_t>(SmokeSimulationPixelsY()), .depth = 1},
		.uiMipLevels = 1,
		.uiArrayLayers = 1,
		.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
		.vkImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // TRANSFER_SRC: agent dump_render_target readback (shared by SmokeOne/SmokeTwo)
		.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
		.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
		.vkRenderPassAttachmentLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
		.vkRenderPassInitialImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.vkRenderPassFinalImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.eTextureLayout = TextureLayout::kShaderReadOnly,
	};
	mSmokeTextureOne.Create(smokeTextureInfo);
	smokeTextureInfo.name = "SmokeTwo";
	smokeTextureInfo.vkRenderPassInitialImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	mSmokeTextureTwo.Create(smokeTextureInfo);

	// Smoke spread only rewrites occupancy-active tiles, so both ping-pong textures must start at zero.
	OneShotCommandBuffer oneShotCommandBuffer;
	VkClearColorValue vkSmokeClearColor {{0.0f, 0.0f, 0.0f, 0.0f}};
	VkImageSubresourceRange vkSmokeSubresource {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1};
	for (Texture* pSmokeTexture : {&mSmokeTextureOne, &mSmokeTextureTwo})
	{
		pSmokeTexture->TransitionImageLayout(oneShotCommandBuffer.mVkCommandBuffer, TextureLayout::kShaderReadOnly, TextureLayout::kTransferDestination);
		vkCmdClearColorImage(oneShotCommandBuffer.mVkCommandBuffer, pSmokeTexture->mVkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &vkSmokeClearColor, 1, &vkSmokeSubresource);
		pSmokeTexture->TransitionImageLayout(oneShotCommandBuffer.mVkCommandBuffer, TextureLayout::kTransferDestination, TextureLayout::kShaderReadOnly);
	}
	oneShotCommandBuffer.Execute();
}

void RenderTargetTextures::CreateWindTextures()
{
	TextureInfo windTextureInfo
	{
		.textureFlags = {TextureFlags::kRenderPass},
		.name = "WindOne",
		.vkImageCreateFlags = 0,
		.vkFormat = shaders::kVkFormatWind,
		.vkExtent3D = VkExtent3D {.width = static_cast<uint32_t>(SmokeSimulationPixels()), .height = static_cast<uint32_t>(SmokeSimulationPixelsY()), .depth = 1},
		.uiMipLevels = 1,
		.uiArrayLayers = 1,
		.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
		.vkImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // TRANSFER_SRC: agent dump_render_target readback
		.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
		.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
		.vkRenderPassAttachmentLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
		.vkRenderPassInitialImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.vkRenderPassFinalImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.eTextureLayout = TextureLayout::kShaderReadOnly,
	};
	mWindTextureOne.Create(windTextureInfo);

	mWindTextureTwo.Create(TextureInfo
	{
		.textureFlags = {TextureFlags::kRenderPass},
		.name = "WindTwo",
		.vkImageCreateFlags = 0,
		.vkFormat = shaders::kVkFormatWind,
		.vkExtent3D = windTextureInfo.vkExtent3D,
		.uiMipLevels = 1,
		.uiArrayLayers = 1,
		.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
		.vkImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // TRANSFER_SRC: agent dump_render_target readback
		.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
		.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
		.vkRenderPassAttachmentLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
		.vkRenderPassInitialImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.vkRenderPassFinalImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.eTextureLayout = TextureLayout::kShaderReadOnly,
	});

	// Occupancy describes these textures' contents, so it is replaced exactly when they are and sized from their new extent.
	gpBufferManager->CreateWindHierarchicalBuffers();

	// Wind spread writes only occupancy-active tiles, leaving inactive texels untouched for smoke to sample.
	// Clear both ping-pong textures once at creation so inactive texels contain zero.
	// The device is idle on the boot and recreation paths.
	OneShotCommandBuffer oneShotCommandBuffer;
	VkClearColorValue vkWindClearColor {{0.0f, 0.0f, 0.0f, 0.0f}};
	VkImageSubresourceRange vkWindSubresource {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1};
	for (Texture* pWindTexture : {&mWindTextureOne, &mWindTextureTwo})
	{
		pWindTexture->TransitionImageLayout(oneShotCommandBuffer.mVkCommandBuffer, TextureLayout::kShaderReadOnly, TextureLayout::kTransferDestination);
		vkCmdClearColorImage(oneShotCommandBuffer.mVkCommandBuffer, pWindTexture->mVkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &vkWindClearColor, 1, &vkWindSubresource);
		pWindTexture->TransitionImageLayout(oneShotCommandBuffer.mVkCommandBuffer, TextureLayout::kTransferDestination, TextureLayout::kShaderReadOnly);
	}
	VkBufferMemoryBarrier pVkWindOccupancyInitializationBarriers[2] {};
	for (int64_t i = 0; i < 2; ++i)
	{
		vkCmdFillBuffer(oneShotCommandBuffer.mVkCommandBuffer, gpBufferManager->mWindOccupancyVkBuffers[i], 0, gpBufferManager->mWindOccupancyBufferVkDeviceSize, 0);
		pVkWindOccupancyInitializationBarriers[i] =
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.buffer = gpBufferManager->mWindOccupancyVkBuffers[i],
			.offset = 0,
			.size = VK_WHOLE_SIZE,
		};
	}
	vkCmdPipelineBarrier(oneShotCommandBuffer.mVkCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, static_cast<uint32_t>(std::size(pVkWindOccupancyInitializationBarriers)), pVkWindOccupancyInitializationBarriers, 0, nullptr);
	oneShotCommandBuffer.Execute();
}

void RenderTargetTextures::CreateObjectShadowsTextures()
{
	auto [iObjectShadowsRenderTextureX, iObjectShadowsRenderTextureY] = TextureManager::DetailTextureSize(gObjectShadowsRenderMultiplier.mfCurrent);
	mObjectShadowsTexture.Create(
	{
		.textureFlags = {TextureFlags::kRenderPass},
		.name = "ObjectShadows",
		.vkImageCreateFlags = 0,
		.vkFormat = shaders::kVkFormatShadow,
		.vkExtent3D = VkExtent3D {.width = static_cast<uint32_t>(iObjectShadowsRenderTextureX), .height = static_cast<uint32_t>(iObjectShadowsRenderTextureY), .depth = 1},
		.uiMipLevels = 1,
		.uiArrayLayers = 1,
		.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
		.vkImageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // TRANSFER_SRC: agent dump_render_target readback
		.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
		.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
		.vkRenderPassAttachmentLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.vkRenderPassFinalImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.vkRenderPassClearColorValue = {1.0f, 0.0f, 0.0f, 0.0f},
		.eTextureLayout = TextureLayout::kShaderReadOnly,
		.vkRenderPassDestinationStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
	});

	auto [iObjectShadowsBlurTextureX, iObjectShadowsBlurTextureY] = TextureManager::DetailTextureSize(gObjectShadowsBlurMultiplier.mfCurrent);
	mObjectShadowsBlurTexture.Create(
	{
		.textureFlags = {},
		.name = "ObjectShadowsBlur",
		.vkImageCreateFlags = 0,
		.vkFormat = shaders::kVkFormatShadow,
		.vkExtent3D = VkExtent3D {.width = static_cast<uint32_t>(iObjectShadowsBlurTextureX), .height = static_cast<uint32_t>(iObjectShadowsBlurTextureY), .depth = 1},
		.uiMipLevels = 1,
		.uiArrayLayers = 1,
		.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
		.vkImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // TRANSFER_SRC: agent dump_render_target readback
		.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
		.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
		.eTextureLayout = TextureLayout::kShaderReadOnly,
	});
	mObjectShadowsBlurIntermediateTexture.Create(
	{
		.textureFlags = {},
		.name = "ObjectShadowsBlurIntermediate",
		.vkImageCreateFlags = 0,
		.vkFormat = shaders::kVkFormatShadow,
		.vkExtent3D = VkExtent3D {.width = static_cast<uint32_t>(iObjectShadowsBlurTextureX), .height = static_cast<uint32_t>(iObjectShadowsBlurTextureY), .depth = 1},
		.uiMipLevels = 1,
		.uiArrayLayers = 1,
		.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
		.vkImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // TRANSFER_SRC: agent dump_render_target readback
		.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
		.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
		.eTextureLayout = TextureLayout::kShaderReadOnly,
	});
}

void RenderTargetTextures::CreateTerrainTextures()
{
	if constexpr (kbDebugPrintf)
	{
		mLogTexture.Create(
		{
			.textureFlags = {TextureFlags::kRenderPass},
			.name = "Log",
			.vkImageCreateFlags = 0,
			.vkFormat = VK_FORMAT_R8G8B8A8_UNORM,
			.vkExtent3D = VkExtent3D {.width = 32, .height = 32, .depth = 1},
			.uiMipLevels = 1,
			.uiArrayLayers = 1,
			.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
			.vkImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // TRANSFER_SRC: agent dump_render_target readback
			.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
			.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
			.vkRenderPassAttachmentLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.vkRenderPassFinalImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.vkRenderPassClearColorValue = {0.0f, 0.0f, 0.0f, 1.0f},
			.eTextureLayout = TextureLayout::kShaderReadOnly,
		});
	}

	auto [iTerrainElevationTextureX, iTerrainElevationTextureY] = TextureManager::DetailTextureSize(gTerrainElevationTextureMultiplier.mfCurrent);
	mTerrainElevationTexture.Create(
	{
		.textureFlags = {TextureFlags::kRenderPass},
		.name = "Elevation",
		.vkImageCreateFlags = 0,
		.vkFormat = shaders::kVkFormatElevation,
		.vkExtent3D = VkExtent3D {.width = static_cast<uint32_t>(iTerrainElevationTextureX), .height = static_cast<uint32_t>(iTerrainElevationTextureY), .depth = 1},
		.uiMipLevels = 1,
		.uiArrayLayers = 1,
		.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
		.vkImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // TRANSFER_SRC: agent dump_render_target readback
		.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
		.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
		.vkRenderPassAttachmentLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.vkRenderPassFinalImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.vkRenderPassClearColorValue = {gpIslandTerrain->mfSeaFloorElevation, 0.0f, 0.0f, 1.0f},
		.eTextureLayout = TextureLayout::kShaderReadOnly,
	});
}

} // namespace engine

#endif // defined(BT_CLIENT)
