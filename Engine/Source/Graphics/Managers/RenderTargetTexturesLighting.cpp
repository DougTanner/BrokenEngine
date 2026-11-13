#if defined(BT_CLIENT)

#include "RenderTargetTextures.h"

#include "Ui/LightingWrappersBase.h"
#include "TextureManager.h"

namespace engine
{

// TextureInfo keeps the name view and Vulkan object naming reads its data(), so per-layer names need static storage.
constexpr std::string_view kppCombineNames[shaders::kiLightingLayers][3] {{"CombineRed0", "CombineGreen0", "CombineBlue0"}, {"CombineRed1", "CombineGreen1", "CombineBlue1"}};
constexpr std::string_view kpAmbientCombineNames[shaders::kiLightingLayers] {"CombineAmbient0", "CombineAmbient1"};
constexpr std::string_view kppHistoryNames[shaders::kiLightingLayers][3] {{"LightingHistoryRed0", "LightingHistoryGreen0", "LightingHistoryBlue0"}, {"LightingHistoryRed1", "LightingHistoryGreen1", "LightingHistoryBlue1"}};
constexpr std::string_view kpAmbientHistoryNames[shaders::kiLightingLayers] {"LightingHistoryAmbient0", "LightingHistoryAmbient1"};

void RenderTargetTextures::DestroyLightingTextures()
{
	miDebugTextureCount = 0;

	for (int64_t i = 0; i < shaders::kiLightingLayers; ++i)
	{
		for (int64_t j = 0; j < 3; ++j)
		{
			mpCombineTextures[i][j].Destroy();
			mpLightingHistoryTextures[i][j].Destroy();
		}
		mpAmbientCombineTextures[i].Destroy();
		mpAmbientHistoryTextures[i].Destroy();
	}

	for (int64_t i = 0; i < shaders::kiMaxSpreadPasses; ++i)
	{
		for (int64_t j = 0; j < 3; ++j)
		{
			mpSpreadTextures[i][j].Destroy();
			mpSpreadOnlyTextures[i][j].Destroy();
		}

		if (mpSpreadVkFramebuffers[i] != VK_NULL_HANDLE)
		{
			vkDestroyFramebuffer(gpDeviceManager->mVkDevice, mpSpreadVkFramebuffers[i], nullptr);
			mpSpreadVkFramebuffers[i] = VK_NULL_HANDLE;
		}
	}

	if (mSpreadVkRenderPass != VK_NULL_HANDLE)
	{
		vkDestroyRenderPass(gpDeviceManager->mVkDevice, mSpreadVkRenderPass, nullptr);
		mSpreadVkRenderPass = VK_NULL_HANDLE;
	}

	if (mLightingVkFramebuffer != VK_NULL_HANDLE)
	{
		vkDestroyFramebuffer(gpDeviceManager->mVkDevice, mLightingVkFramebuffer, nullptr);
		mLightingVkFramebuffer = VK_NULL_HANDLE;
		vkDestroyRenderPass(gpDeviceManager->mVkDevice, mLightingVkRenderPass, nullptr);
		mLightingVkRenderPass = VK_NULL_HANDLE;
	}
}

void RenderTargetTextures::CreateLightingTextures()
{
	DestroyLightingTextures();

	// Lighting layer k is flight plane k's layer, and every lighting reader assumes exactly two planes.
	ASSERT(gpFlightPlanes->Count() == shaders::kiLightingLayers);

	// This (re)create rebuilds the history textures (below) with undefined contents; re-arm the temporal first-frame
	// guard so PopulateLightingParameters blends pure-current and re-seeds history next frame (mirror of CreateShadowTextures).
	gbLightingTemporalReset = true;

	auto [iLightingTextureX, iLightingTextureY] = TextureManager::ExtraAreaDetailTextureSize(gLightingDepositTextureMultiplier.mfCurrent);
	// Create 3 lighting textures without individual render passes
	TextureInfo lightingTextureInfo
	{
		.textureFlags = {},
		.name = "RedLighting",
		.vkImageCreateFlags = 0,
		.vkFormat = shaders::kVkFormatLighting,
		.vkExtent3D = VkExtent3D {.width = static_cast<uint32_t>(iLightingTextureX), .height = static_cast<uint32_t>(iLightingTextureY), .depth = 1},
		.iMipLevels = 1,
		.iArrayLayers = 1,
		.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
		.vkImageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // TRANSFER_SRC: agent dump_render_target readback (shared by all 3 lighting textures)
		.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
		.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
		.eTextureLayout = TextureLayout::kShaderReadOnly,
	};
	mpLightingTextures[0].Create(lightingTextureInfo);
	lightingTextureInfo.name = "GreenLighting";
	mpLightingTextures[1].Create(lightingTextureInfo);
	lightingTextureInfo.name = "BlueLighting";
	mpLightingTextures[2].Create(lightingTextureInfo);

	VkAttachmentDescription pVkAttachmentDescriptions[3] {};
	for (VkAttachmentDescription& rVkAttachmentDescription : pVkAttachmentDescriptions)
	{
		rVkAttachmentDescription = VkAttachmentDescription
		{
			.flags = 0,
			.format = shaders::kVkFormatLighting,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};
	}
	VkAttachmentReference pVkAttachmentReferences[3]
	{
		{.attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
		{.attachment = 1, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
		{.attachment = 2, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
	};
	VkSubpassDescription vkSubpassDescription
	{
		.flags = 0,
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.inputAttachmentCount = 0,
		.pInputAttachments = nullptr,
		.colorAttachmentCount = 3,
		.pColorAttachments = pVkAttachmentReferences,
		.pResolveAttachments = nullptr,
		.pDepthStencilAttachment = nullptr,
		.preserveAttachmentCount = 0,
		.pPreserveAttachments = nullptr,
	};
	// The UNDEFINED transition and clear must wait for the previous frame's shader reads of these shared textures: the implicit
	// external dependency the spec supplies is TOP_OF_PIPE/0 and orders nothing, and frames overlap up to the swapchain image count.
	VkSubpassDependency pVkSubpassDependencies[2]
	{
		{
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = 0, // Write-after-read needs execution ordering only
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dependencyFlags = 0,
		},
		{
			.srcSubpass = 0,
			.dstSubpass = VK_SUBPASS_EXTERNAL,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dependencyFlags = 0,
		},
	};
	VkRenderPassCreateInfo vkRenderPassCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.attachmentCount = static_cast<uint32_t>(std::size(pVkAttachmentDescriptions)),
		.pAttachments = pVkAttachmentDescriptions,
		.subpassCount = 1,
		.pSubpasses = &vkSubpassDescription,
		.dependencyCount = static_cast<uint32_t>(std::size(pVkSubpassDependencies)),
		.pDependencies = pVkSubpassDependencies,
	};
	CHECK_VK(vkCreateRenderPass(gpDeviceManager->mVkDevice, &vkRenderPassCreateInfo, nullptr, &mLightingVkRenderPass));
	VkName(VK_OBJECT_TYPE_RENDER_PASS, mLightingVkRenderPass, "LightingMRT");

	VkImageView pVkImageViews[3] {mpLightingTextures[0].mVkImageView, mpLightingTextures[1].mVkImageView, mpLightingTextures[2].mVkImageView};
	VkFramebufferCreateInfo vkFramebufferCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.renderPass = mLightingVkRenderPass,
		.attachmentCount = static_cast<uint32_t>(std::size(pVkImageViews)),
		.pAttachments = pVkImageViews,
		.width = static_cast<uint32_t>(iLightingTextureX),
		.height = static_cast<uint32_t>(iLightingTextureY),
		.layers = 1,
	};
	CHECK_VK(vkCreateFramebuffer(gpDeviceManager->mVkDevice, &vkFramebufferCreateInfo, nullptr, &mLightingVkFramebuffer));
	VkName(VK_OBJECT_TYPE_FRAMEBUFFER, mLightingVkFramebuffer, "LightingMRT");

	float fSpreadMultiplierStart = gSpreadTextureMultiplierStart.mfCurrent;
	float fSpreadMultiplierEnd = gSpreadTextureMultiplierEnd.mfCurrent;
	int64_t iPassCount = static_cast<int64_t>(gSpreadPassCount.mfCurrent);
	static constexpr std::string_view kpColorNames[3] {"Red", "Green", "Blue"};
	for (int64_t i = 0; i < shaders::kiMaxSpreadPasses; ++i)
	{
		float fInterpolationFraction = (iPassCount > 1) ? static_cast<float>(i) / static_cast<float>(iPassCount - 1) : 0.0f;
		float fMultiplier = fSpreadMultiplierStart + fInterpolationFraction * (fSpreadMultiplierEnd - fSpreadMultiplierStart);
		auto [iPassX, iPassY] = TextureManager::ExtraAreaDetailTextureSize(fMultiplier);
		for (int64_t j = 0; j < 3; ++j)
		{
			std::string spreadName = std::format("Spread{}_{}", kpColorNames[j], i);
			mpSpreadTextures[i][j].Create(TextureInfo
			{
				.textureFlags = {},
				.name = spreadName,
				.vkImageCreateFlags = 0,
				.vkFormat = shaders::kVkFormatLighting,
				.vkExtent3D = VkExtent3D {.width = static_cast<uint32_t>(iPassX), .height = static_cast<uint32_t>(iPassY), .depth = 1},
				.iMipLevels = 1,
				.iArrayLayers = 1,
				.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
				.vkImageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // TRANSFER_SRC: agent dump_render_target readback
				.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
				.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
				.eTextureLayout = TextureLayout::kShaderReadOnly,
			});
			std::string spreadOnlyName = std::format("SpreadOnly{}_{}", kpColorNames[j], i);
			mpSpreadOnlyTextures[i][j].Create(TextureInfo
			{
				.textureFlags = {},
				.name = spreadOnlyName,
				.vkImageCreateFlags = 0,
				.vkFormat = shaders::kVkFormatLighting,
				.vkExtent3D = VkExtent3D {.width = static_cast<uint32_t>(iPassX), .height = static_cast<uint32_t>(iPassY), .depth = 1},
				.iMipLevels = 1,
				.iArrayLayers = 1,
				.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
				.vkImageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // TRANSFER_SRC: agent dump_render_target readback
				.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
				.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
				.eTextureLayout = TextureLayout::kShaderReadOnly,
			});
		}
	}

	// Locations 0–2: accumulated (fed to next spread pass). Locations 3–5: spread-only (read by combine).
	VkAttachmentDescription pVkSpreadAttachmentDescriptions[6] {};
	VkAttachmentReference pVkSpreadAttachmentReferences[6] {};
	for (int64_t i = 0; i < 6; ++i)
	{
		pVkSpreadAttachmentDescriptions[i] = VkAttachmentDescription
		{
			.flags = 0,
			.format = shaders::kVkFormatLighting,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};
		pVkSpreadAttachmentReferences[i] = {.attachment = static_cast<uint32_t>(i), .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	}
	VkSubpassDescription vkSpreadSubpassDescription
	{
		.flags = 0,
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.inputAttachmentCount = 0,
		.pInputAttachments = nullptr,
		.colorAttachmentCount = 6,
		.pColorAttachments = pVkSpreadAttachmentReferences,
		.pResolveAttachments = nullptr,
		.pDepthStencilAttachment = nullptr,
		.preserveAttachmentCount = 0,
		.pPreserveAttachments = nullptr,
	};
	// The UNDEFINED transition and clear must wait for the previous frame's shader reads of these shared textures: the implicit
	// external dependency the spec supplies is TOP_OF_PIPE/0 and orders nothing, and frames overlap up to the swapchain image count.
	VkSubpassDependency pVkSpreadSubpassDependencies[2]
	{
		{
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = 0, // Write-after-read needs execution ordering only
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dependencyFlags = 0,
		},
		{
			.srcSubpass = 0,
			.dstSubpass = VK_SUBPASS_EXTERNAL,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dependencyFlags = 0,
		},
	};
	VkRenderPassCreateInfo vkSpreadRenderPassCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.attachmentCount = static_cast<uint32_t>(std::size(pVkSpreadAttachmentDescriptions)),
		.pAttachments = pVkSpreadAttachmentDescriptions,
		.subpassCount = 1,
		.pSubpasses = &vkSpreadSubpassDescription,
		.dependencyCount = static_cast<uint32_t>(std::size(pVkSpreadSubpassDependencies)),
		.pDependencies = pVkSpreadSubpassDependencies,
	};
	CHECK_VK(vkCreateRenderPass(gpDeviceManager->mVkDevice, &vkSpreadRenderPassCreateInfo, nullptr, &mSpreadVkRenderPass));
	VkName(VK_OBJECT_TYPE_RENDER_PASS, mSpreadVkRenderPass, "SpreadMRT");

	for (int64_t i = 0; i < shaders::kiMaxSpreadPasses; ++i)
	{
		VkImageView pVkSpreadImageViews[6]
		{
			mpSpreadTextures[i][0].mVkImageView,
			mpSpreadTextures[i][1].mVkImageView,
			mpSpreadTextures[i][2].mVkImageView,
			mpSpreadOnlyTextures[i][0].mVkImageView,
			mpSpreadOnlyTextures[i][1].mVkImageView,
			mpSpreadOnlyTextures[i][2].mVkImageView,
		};
		VkFramebufferCreateInfo vkSpreadFramebufferCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.renderPass = mSpreadVkRenderPass,
			.attachmentCount = static_cast<uint32_t>(std::size(pVkSpreadImageViews)),
			.pAttachments = pVkSpreadImageViews,
			.width = mpSpreadTextures[i][0].mInfo.vkExtent3D.width,
			.height = mpSpreadTextures[i][0].mInfo.vkExtent3D.height,
			.layers = 1,
		};
		CHECK_VK(vkCreateFramebuffer(gpDeviceManager->mVkDevice, &vkSpreadFramebufferCreateInfo, nullptr, &mpSpreadVkFramebuffers[i]));
		VkName(VK_OBJECT_TYPE_FRAMEBUFFER, mpSpreadVkFramebuffers[i], std::format("SpreadMRT_{}", i));
	}

	// Create combine textures (UNORM tone-mapped output, sized to max of start/end). LightingTemporal.comp blends the
	// last-refresh history into these in place; LightingHistoryCopy.comp publishes the result to history.
	auto [iCombineX, iCombineY] = TextureManager::ExtraAreaDetailTextureSize(std::max(fSpreadMultiplierStart, fSpreadMultiplierEnd));
	// TRANSFER_DST: the creation clear below.
	TextureInfo combineTextureInfo
	{
		.textureFlags = {},
		.name = kppCombineNames[0][0],
		.vkImageCreateFlags = 0,
		.vkFormat = shaders::kVkFormatCombine,
		.vkExtent3D = VkExtent3D {.width = static_cast<uint32_t>(iCombineX), .height = static_cast<uint32_t>(iCombineY), .depth = 1},
		.iMipLevels = 1,
		.iArrayLayers = 1,
		.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
		.vkImageUsageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
		.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
		.eTextureLayout = TextureLayout::kShaderReadOnly,
	};

	// History textures: last-refresh combine outputs reprojected + EMA-blended by LightingTemporal.comp.
	// Clones of the combine textures (same bumped extent, RGBA8 UNORM); SAMPLED (read as history), STORAGE
	// (descriptor-set compatibility with combine bindings) so LightingHistoryCopy.comp can refresh them in-place.
	TextureInfo historyTextureInfo
	{
		.textureFlags = {},
		.name = kppHistoryNames[0][0],
		.vkImageCreateFlags = 0,
		.vkFormat = shaders::kVkFormatCombine,
		.vkExtent3D = VkExtent3D {.width = static_cast<uint32_t>(iCombineX), .height = static_cast<uint32_t>(iCombineY), .depth = 1},
		.iMipLevels = 1,
		.iArrayLayers = 1,
		.vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT,
		.vkImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // TRANSFER_SRC: agent dump_render_target readback
		.vkImageViewType = VK_IMAGE_VIEW_TYPE_2D,
		.vkImageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
		.eTextureLayout = TextureLayout::kShaderReadOnly,
	};

	for (int64_t i = 0; i < shaders::kiLightingLayers; ++i)
	{
		for (int64_t j = 0; j < 3; ++j)
		{
			combineTextureInfo.name = kppCombineNames[i][j];
			mpCombineTextures[i][j].Create(combineTextureInfo);
			historyTextureInfo.name = kppHistoryNames[i][j];
			mpLightingHistoryTextures[i][j].Create(historyTextureInfo);
		}
		combineTextureInfo.name = kpAmbientCombineNames[i];
		mpAmbientCombineTextures[i].Create(combineTextureInfo);
		historyTextureInfo.name = kpAmbientHistoryNames[i];
		mpAmbientHistoryTextures[i].Create(historyTextureInfo);
	}

	// A reset refreshes one layer per frame, so consumers read the other layer's combine outputs a frame before its first
	// refresh writes them. History needs no clear: a reset's first refresh blends pure current and then overwrites it.
	OneShotCommandBuffer oneShotCommandBuffer;
	VkClearColorValue vkCombineClearColor {{0.0f, 0.0f, 0.0f, 0.0f}};
	VkImageSubresourceRange vkCombineSubresource {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1};
	for (int64_t i = 0; i < shaders::kiLightingLayers; ++i)
	{
		for (Texture* pCombineTexture : {&mpCombineTextures[i][0], &mpCombineTextures[i][1], &mpCombineTextures[i][2], &mpAmbientCombineTextures[i]})
		{
			pCombineTexture->TransitionImageLayout(oneShotCommandBuffer.mVkCommandBuffer, TextureLayout::kShaderReadOnly, TextureLayout::kTransferDestination);
			vkCmdClearColorImage(oneShotCommandBuffer.mVkCommandBuffer, pCombineTexture->mVkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &vkCombineClearColor, 1, &vkCombineSubresource);
			pCombineTexture->TransitionImageLayout(oneShotCommandBuffer.mVkCommandBuffer, TextureLayout::kTransferDestination, TextureLayout::kShaderReadOnly);
		}
	}
	oneShotCommandBuffer.Execute();

	// Final output points to combine textures (tone-mapped UNORM)
	for (int64_t i = 0; i < shaders::kiLightingLayers; ++i)
	{
		for (int64_t j = 0; j < 3; ++j)
		{
			mppLightingFinalTextures[i * 3 + j] = &mpCombineTextures[i][j];
		}
		mppAmbientCombineTextures[i] = &mpAmbientCombineTextures[i];
	}
	for (int64_t i = 0; i < 3; ++i)
	{
		mppLightingDepositTextures[i] = &mpLightingTextures[i];
	}

	RegisterDebugTextures(iPassCount);
}

void RenderTargetTextures::RegisterDebugTextures(int64_t iPassCount)
{
	// Terrain.frag samples color/normal/AO directly from bindless per-island textures.
	static constexpr int64_t kiTerrainDebugSlotCount = 1;
	mppDebugTextures[0] = &mTerrainElevationTexture;
	mpiDebugTextureFormats[0] = shaders::kiDebugTextureFormatTerrainElevation;

	mppDebugTextures[kiTerrainDebugSlotCount + 0] = &mpLightingTextures[0];
	mpiDebugTextureFormats[kiTerrainDebugSlotCount + 0] = shaders::kiDebugTextureFormatFloat16LinearVisibleArea;
	mppDebugTextures[kiTerrainDebugSlotCount + 1] = &mpLightingTextures[0];
	mpiDebugTextureFormats[kiTerrainDebugSlotCount + 1] = shaders::kiDebugTextureFormatFloat16DepositDirectionCombined;
	for (int64_t i = 0; i < iPassCount; ++i)
	{
		mppDebugTextures[kiTerrainDebugSlotCount + 2 + i] = &mpSpreadTextures[i][0];
		mppDebugTexturesB[kiTerrainDebugSlotCount + 2 + i] = &mpSpreadTextures[i][1];
		mppDebugTexturesC[kiTerrainDebugSlotCount + 2 + i] = &mpSpreadTextures[i][2];
		mpiDebugTextureFormats[kiTerrainDebugSlotCount + 2 + i] = shaders::kiDebugTextureFormatFloat16SpreadDirectionCombined;
	}
	mppDebugTextures[kiTerrainDebugSlotCount + 2 + iPassCount] = &mpCombineTextures[0][0];
	mpiDebugTextureFormats[kiTerrainDebugSlotCount + 2 + iPassCount] = shaders::kiDebugTextureFormatUnormLightingDirectional;
	miDebugTextureCount = kiTerrainDebugSlotCount + 3 + iPassCount;

	// Fill unused B/C slots with primary texture so descriptor writes remain valid
	for (int64_t i = 0; i < shaders::kiMaxDebugTextures; ++i)
	{
		if (mppDebugTexturesB[i] == nullptr)
		{
			mppDebugTexturesB[i] = mppDebugTextures[i] != nullptr ? mppDebugTextures[i] : &mpLightingTextures[0];
		}
		if (mppDebugTexturesC[i] == nullptr)
		{
			mppDebugTexturesC[i] = mppDebugTextures[i] != nullptr ? mppDebugTextures[i] : &mpLightingTextures[0];
		}
		if (mppDebugTextures[i] == nullptr)
		{
			mppDebugTextures[i] = &mpLightingTextures[0];
		}
	}

	if (gDebugTextureIndex.mfCurrent >= static_cast<float>(miDebugTextureCount))
	{
		gDebugTextureIndex.Set(static_cast<float>(miDebugTextureCount - 1));
	}
}

} // namespace engine

#endif // defined(BT_CLIENT)
