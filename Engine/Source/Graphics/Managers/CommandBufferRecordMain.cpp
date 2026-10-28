#if defined(BT_CLIENT)

#include "CommandBufferRecordMain.h"

#include "Frame/IslandTerrain.h"
#include "Graphics/Islands.h"
#include "Ui/GraphicsQualityWrappersBase.h"
#include "Ui/GraphicsSettingsWrappersBase.h"
#include "CommandBufferManager.h"

#include "Profile/ProfileRows.h"
#include "Game.h"

namespace engine
{

void CommandBufferRecordMain::Record(int64_t iFramebuffer)
{
	CommandBuffers& rCommandBuffers = gpCommandBufferManager->mPerFramebufferCommandBuffers.at(iFramebuffer);
	int64_t iCommandBuffer = iFramebuffer;

	VkCommandBufferBeginInfo vkCommandBufferBeginInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = 0,
		.pInheritanceInfo = nullptr,
	};

	VkCommandBuffer vkCommandBuffer = rCommandBuffers.mMainVkCommandBuffer;
	CHECK_VK(vkBeginCommandBuffer(vkCommandBuffer, &vkCommandBufferBeginInfo));

	if constexpr (kbDebugPrintf)
	{
		gpTextureManager->mRenderTargetTextures.mLogTexture.RecordBeginRenderPass(vkCommandBuffer);
		gpPipelineManager->mpPipelines[kPipelineLog].RecordDraw(iCommandBuffer, vkCommandBuffer, 1, 0, {static_cast<float>(iCommandBuffer), 0.0f, 0.0f, 0.0f});
		vkCmdEndRenderPass(vkCommandBuffer);
	}

	gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerMain);

	gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerImage);
	RecordImageRenderPass(vkCommandBuffer, iCommandBuffer, iFramebuffer);
	RecordHighDynamicRangeResolve(vkCommandBuffer, iCommandBuffer, iFramebuffer);

	gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerImage);

	CHECK_VK(vkEndCommandBuffer(vkCommandBuffer));
}

void CommandBufferRecordMain::RecordEnvironment(VkCommandBuffer vkCommandBuffer, int64_t iCommandBuffer)
{
	RecordSmokeEmit(vkCommandBuffer, iCommandBuffer);
	RecordWindDeposits(vkCommandBuffer, iCommandBuffer);
	RecordObjectShadows(vkCommandBuffer, iCommandBuffer);
	RecordObjectShadowsBlur(vkCommandBuffer, iCommandBuffer);
	RecordWaterFoam(vkCommandBuffer, iCommandBuffer);
}

void CommandBufferRecordMain::RecordSmokeEmit(VkCommandBuffer vkCommandBuffer, int64_t iCommandBuffer)
{
	gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerSmokeEmit);

	for (int64_t i = 0; i < gpFlightPlanes->Count(); ++i)
	{
		Texture& rSmokeTextureOne = gpTextureManager->mRenderTargetTextures.mSmokeTexturesOne[i];
		Texture& rSmokeTextureTwo = gpTextureManager->mRenderTargetTextures.mSmokeTexturesTwo[i];

		// Force-clear TextureTwo on the Smoke enabled/recreate edge. Global spread runs before Main, so any stale
		// occupancy describing this cleared texture is consumed and drained by the next frame's spread. The shared
		// clear pipelines draw in every plane's render passes, which stay compatible with plane 0's.
		rSmokeTextureTwo.RecordBeginRenderPass(vkCommandBuffer);
		gpPipelineManager->mpPipelines[kPipelineSmokeClearB].RecordDrawIndirect(iCommandBuffer, vkCommandBuffer, {0.0f, 0.0f, 0.0f, 0.0f});
		vkCmdEndRenderPass(vkCommandBuffer);

		rSmokeTextureOne.TransitionImageLayout(vkCommandBuffer, TextureLayout::kShaderReadOnly, TextureLayout::kColorAttachment);
		rSmokeTextureOne.RecordBeginRenderPass(vkCommandBuffer);
		gpPipelineManager->mpPipelines[kPipelineSmokeClearA].RecordDrawIndirect(iCommandBuffer, vkCommandBuffer, {0.0f, 0.0f, 0.0f, 0.0f});
		// The deposit maps hold one pipeline per (collection, plane); each targets its own plane's render pass. Push
		// constant x 2 selects the smoke area and y the plane, so each plane deposits through its own published area.
		XMFLOAT4 f4DepositPushConstants {2.0f, static_cast<float>(i), 0.0f, 0.0f};
		for (const auto& [rCrc, pPipeline] : gpPipelineManager->mDynamicPipelines.mpPipelineMaps[kDynamicPipelineSmokeAxisAligned])
		{
			if (pPipeline->mInfo.vkTargetRenderPass == rSmokeTextureOne.mVkRenderPass)
			{
				pPipeline->RecordDrawIndirect(iCommandBuffer, vkCommandBuffer, f4DepositPushConstants);
			}
		}
		for (const auto& [rCrc, pPipeline] : gpPipelineManager->mDynamicPipelines.mpPipelineMaps[kDynamicPipelineSmoke])
		{
			if (pPipeline->mInfo.vkTargetRenderPass == rSmokeTextureOne.mVkRenderPass)
			{
				pPipeline->RecordDrawIndirect(iCommandBuffer, vkCommandBuffer, f4DepositPushConstants);
			}
		}
		vkCmdEndRenderPass(vkCommandBuffer);
	}
	gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerSmokeEmit);
}

void CommandBufferRecordMain::RecordWindDeposits(VkCommandBuffer vkCommandBuffer, int64_t iCommandBuffer)
{
	// Wind-deposit commands stay fixed even when disabled; their disabled-state cost is low.
	gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerWindDeposit);
	gpTextureManager->mRenderTargetTextures.mWindTextureOne.TransitionImageLayout(vkCommandBuffer, TextureLayout::kShaderReadOnly, TextureLayout::kColorAttachment);
	gpTextureManager->mRenderTargetTextures.mWindTextureOne.RecordBeginRenderPass(vkCommandBuffer);
	for (const auto& [rCrc, pPipeline] : gpPipelineManager->mDynamicPipelines.mpPipelineMaps[kDynamicPipelineWindDepositA])
	{
		pPipeline->RecordDrawIndirect(iCommandBuffer, vkCommandBuffer, {2.0f, 0.0f, 0.0f, 0.0f});
	}
	for (const auto& [rCrc, pPipeline] : gpPipelineManager->mDynamicPipelines.mpPipelineMaps[kDynamicPipelineWindDepositAxisAlignedA])
	{
		pPipeline->RecordDrawIndirect(iCommandBuffer, vkCommandBuffer, {2.0f, 0.0f, 0.0f, 0.0f});
	}
	vkCmdEndRenderPass(vkCommandBuffer);

	gpTextureManager->mRenderTargetTextures.mWindTextureTwo.TransitionImageLayout(vkCommandBuffer, TextureLayout::kShaderReadOnly, TextureLayout::kColorAttachment);
	gpTextureManager->mRenderTargetTextures.mWindTextureTwo.RecordBeginRenderPass(vkCommandBuffer);
	for (const auto& [rCrc, pPipeline] : gpPipelineManager->mDynamicPipelines.mpPipelineMaps[kDynamicPipelineWindDepositB])
	{
		pPipeline->RecordDrawIndirect(iCommandBuffer, vkCommandBuffer, {2.0f, 0.0f, 0.0f, 0.0f});
	}
	for (const auto& [rCrc, pPipeline] : gpPipelineManager->mDynamicPipelines.mpPipelineMaps[kDynamicPipelineWindDepositAxisAlignedB])
	{
		pPipeline->RecordDrawIndirect(iCommandBuffer, vkCommandBuffer, {2.0f, 0.0f, 0.0f, 0.0f});
	}
	vkCmdEndRenderPass(vkCommandBuffer);
	gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerWindDeposit);
}

void CommandBufferRecordMain::RecordObjectShadows(VkCommandBuffer vkCommandBuffer, int64_t iCommandBuffer)
{
	// Object-shadow commands stay fixed across the continuous day cycle, which has no explicit feature toggle; their cost is low.
	gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerObjectShadows);
	Texture::RecordBeginRenderPass(vkCommandBuffer, gpTextureManager->mRenderTargetTextures.mObjectShadowsTexture.mVkRenderPass, gpTextureManager->mRenderTargetTextures.mObjectShadowsTexture.mVkFramebuffer, {.width = gpTextureManager->mRenderTargetTextures.mObjectShadowsTexture.mInfo.vkExtent3D.width, .height = gpTextureManager->mRenderTargetTextures.mObjectShadowsTexture.mInfo.vkExtent3D.height}, gpTextureManager->mRenderTargetTextures.mObjectShadowsTexture.mInfo.vkRenderPassClearColorValue, RenderPassFlags_t {RenderPassFlags::kClear});
	for (const auto& [rCrc, pPipeline] : gpPipelineManager->mDynamicPipelines.mpModelPipelineMaps[kDynamicModelPipelineModelShadow])
	{
		pPipeline->RecordDrawIndirect(iCommandBuffer, vkCommandBuffer, {0.0f, 2.0f, 0.0f, 0.0f}, ModelDrawPass::kOpaque);
	}
	vkCmdEndRenderPass(vkCommandBuffer);
	gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerObjectShadows);
}

void CommandBufferRecordMain::RecordObjectShadowsBlur(VkCommandBuffer vkCommandBuffer, int64_t iCommandBuffer)
{
	gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerObjectShadowsBlur);
	int64_t iObjectShadowsBlurWidth = gpTextureManager->mRenderTargetTextures.mObjectShadowsBlurTexture.mInfo.vkExtent3D.width;
	int64_t iObjectShadowsBlurHeight = gpTextureManager->mRenderTargetTextures.mObjectShadowsBlurTexture.mInfo.vkExtent3D.height;
	gpTextureManager->mRenderTargetTextures.mObjectShadowsBlurIntermediateTexture.TransitionImageLayout(vkCommandBuffer, TextureLayout::kShaderReadOnly, TextureLayout::kComputeReadWrite);
	gpPipelineManager->mpPipelines[kPipelineObjectShadowsBlurH].RecordCompute(iCommandBuffer, vkCommandBuffer, TileCount(iObjectShadowsBlurWidth), TileCount(iObjectShadowsBlurHeight));
	gpTextureManager->mRenderTargetTextures.mObjectShadowsBlurIntermediateTexture.TransitionImageLayout(vkCommandBuffer, TextureLayout::kComputeReadWrite, TextureLayout::kComputeReadOnly);
	gpTextureManager->mRenderTargetTextures.mObjectShadowsBlurTexture.TransitionImageLayout(vkCommandBuffer, TextureLayout::kShaderReadOnly, TextureLayout::kComputeReadWrite);
	gpPipelineManager->mpPipelines[kPipelineObjectShadowsBlurV].RecordCompute(iCommandBuffer, vkCommandBuffer, TileCount(iObjectShadowsBlurWidth), TileCount(iObjectShadowsBlurHeight));
	gpTextureManager->mRenderTargetTextures.mObjectShadowsBlurTexture.TransitionImageLayout(vkCommandBuffer, TextureLayout::kComputeReadWrite, TextureLayout::kShaderReadOnly);
	gpTextureManager->mRenderTargetTextures.mObjectShadowsBlurIntermediateTexture.TransitionImageLayout(vkCommandBuffer, TextureLayout::kComputeReadOnly, TextureLayout::kShaderReadOnly);
	gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerObjectShadowsBlur);
}

void CommandBufferRecordMain::RecordWaterDisplacement(VkCommandBuffer vkCommandBuffer, int64_t iCommandBuffer)
{
	// RGBA16F Gerstner displacement and Jacobian normals let Water.vert fetch each result per vertex without summing the low and medium wave bands.
	// MainUniforms writes indirect dispatch dimensions for the active LOD region; the shader bounds-checks iWaterActiveQuad*.
	// Preparation waits for Global before sampling elevation and signals Main before water rendering.
	gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerWaterDisplacement);
	gpTextureManager->mRenderTargetTextures.mWaterDisplacementTexture.TransitionImageLayout(vkCommandBuffer, TextureLayout::kShaderReadOnly, TextureLayout::kComputeReadWrite);
	gpTextureManager->mRenderTargetTextures.mWaterDisplacementNormalTexture.TransitionImageLayout(vkCommandBuffer, TextureLayout::kShaderReadOnly, TextureLayout::kComputeReadWrite);
	gpPipelineManager->mpPipelines[kPipelineWaterDisplacement].RecordComputeIndirect(iCommandBuffer, vkCommandBuffer);
	gpTextureManager->mRenderTargetTextures.mWaterDisplacementTexture.TransitionImageLayout(vkCommandBuffer, TextureLayout::kComputeReadWrite, TextureLayout::kShaderReadOnly);
	gpTextureManager->mRenderTargetTextures.mWaterDisplacementNormalTexture.TransitionImageLayout(vkCommandBuffer, TextureLayout::kComputeReadWrite, TextureLayout::kShaderReadOnly);
	gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerWaterDisplacement);
}

void CommandBufferRecordMain::RecordWaterFoam(VkCommandBuffer vkCommandBuffer, int64_t iCommandBuffer)
{
	gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerWaterFoam);
	bool bActive = gWaterFoamSystemEnabled.Get<bool>() && gWaterLevel.Get<int64_t>() > static_cast<int64_t>(GraphicsQualityLevel::kLow);
	gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerWaterFoamCompute);
	if (bActive)
	{
		Texture& rCurrent = gpTextureManager->mRenderTargetTextures.mWaterFoamCurrentTexture;
		rCurrent.TransitionImageLayout(vkCommandBuffer, TextureLayout::kShaderReadOnly, TextureLayout::kComputeReadWrite);
		gpPipelineManager->mpPipelines[kPipelineWaterFoam].RecordCompute(iCommandBuffer, vkCommandBuffer, TileCount(shaders::kiWaterFoamWidth), TileCount(shaders::kiWaterFoamHeight));
		rCurrent.TransitionImageLayout(vkCommandBuffer, TextureLayout::kComputeReadWrite, TextureLayout::kTransferSource);
	}
	gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerWaterFoamCompute);

	gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerWaterFoamHistory);
	if (bActive)
	{
		Texture& rCurrent = gpTextureManager->mRenderTargetTextures.mWaterFoamCurrentTexture;
		Texture& rHistory = gpTextureManager->mRenderTargetTextures.mWaterFoamHistoryTexture;
		auto TransitionHistory = [&](int64_t iMip, int64_t iCount, VkImageLayout vkOldLayout, VkImageLayout vkNewLayout, VkAccessFlags vkSourceAccess, VkAccessFlags vkDestinationAccess, VkPipelineStageFlags vkSourceStage, VkPipelineStageFlags vkDestinationStage)
		{
			VkImageMemoryBarrier vkBarrier
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = vkSourceAccess,
				.dstAccessMask = vkDestinationAccess,
				.oldLayout = vkOldLayout,
				.newLayout = vkNewLayout,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = rHistory.mVkImage,
				.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = static_cast<uint32_t>(iMip), .levelCount = static_cast<uint32_t>(iCount), .baseArrayLayer = 0, .layerCount = 1},
			};
			vkCmdPipelineBarrier(vkCommandBuffer, vkSourceStage, vkDestinationStage, 0, 0, nullptr, 0, nullptr, 1, &vkBarrier);
		};
		static constexpr VkPipelineStageFlags kVkHistoryReadStages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		TransitionHistory(0, shaders::kiWaterFoamMipLevels, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, kVkHistoryReadStages, VK_PIPELINE_STAGE_TRANSFER_BIT);
		VkImageCopy vkCopy
		{
			.srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
			.srcOffset = {},
			.dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
			.dstOffset = {},
			.extent = {.width = shaders::kiWaterFoamWidth, .height = shaders::kiWaterFoamHeight, .depth = 1},
		};
		vkCmdCopyImage(vkCommandBuffer, rCurrent.mVkImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, rHistory.mVkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &vkCopy);
		int64_t iWidth = shaders::kiWaterFoamWidth;
		int64_t iHeight = shaders::kiWaterFoamHeight;
		for (int64_t i = 1; i < shaders::kiWaterFoamMipLevels; ++i)
		{
			TransitionHistory(i - 1, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
			int64_t iNextWidth = std::max(1i64, iWidth / 2);
			int64_t iNextHeight = std::max(1i64, iHeight / 2);
			VkImageBlit vkBlit
			{
				.srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = static_cast<uint32_t>(i - 1), .baseArrayLayer = 0, .layerCount = 1},
				.srcOffsets =
				{
					{
						.x = 0,
						.y = 0,
						.z = 0,
					},
					{
						.x = static_cast<int32_t>(iWidth),
						.y = static_cast<int32_t>(iHeight),
						.z = 1,
					},
				},
				.dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = static_cast<uint32_t>(i), .baseArrayLayer = 0, .layerCount = 1},
				.dstOffsets =
				{
					{
						.x = 0,
						.y = 0,
						.z = 0,
					},
					{
						.x = static_cast<int32_t>(iNextWidth),
						.y = static_cast<int32_t>(iNextHeight),
						.z = 1,
					},
				},
			};
			vkCmdBlitImage(vkCommandBuffer, rHistory.mVkImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, rHistory.mVkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &vkBlit, VK_FILTER_LINEAR);
			TransitionHistory(i - 1, 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, kVkHistoryReadStages);
			iWidth = iNextWidth;
			iHeight = iNextHeight;
		}
		TransitionHistory(shaders::kiWaterFoamMipLevels - 1, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, kVkHistoryReadStages);
		rCurrent.TransitionImageLayout(vkCommandBuffer, TextureLayout::kTransferSource, TextureLayout::kShaderReadOnly);
	}
	gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerWaterFoamHistory);
	gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerWaterFoam);
}

void CommandBufferRecordMain::RecordImageRenderPass(VkCommandBuffer vkCommandBuffer, int64_t iCommandBuffer, int64_t iFramebuffer)
{
	Pipeline* pPipelines = gpPipelineManager->mpPipelines;
	{
		RenderPassFlags_t renderPassFlags {RenderPassFlags::kDepth};
		renderPassFlags.Set(RenderPassFlags::kClear);
		if (gMultisampling.Get<bool>())
		{
			renderPassFlags.Set(RenderPassFlags::kMultisampling);
		}
		Texture::RecordBeginRenderPass(vkCommandBuffer, gpSwapchainManager->mHdrVkRenderPass, gpSwapchainManager->mHdrVkFramebuffer, gpGraphics->mFramebufferVkExtent2D, game::kVkHdrClearColor, renderPassFlags);
	}

	// UI depth pre-pass: depth-only quads at ImGui window positions (instanceCount=0 when disabled)
	auto RecordUiDepth = [&]()
	{
		Pipeline& rUiPrepass = pPipelines[kPipelineUiDepthPrepass];
		vkCmdBindPipeline(vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rUiPrepass.mVkPipeline);
		VkDescriptorSet pVkUiPrepassSets[2] = {gpTextureManager->mTextureDescriptors.mGlobalDescriptorSets.at(iFramebuffer), rUiPrepass.mVkDescriptorSets.at(iFramebuffer)};
		vkCmdBindDescriptorSets(vkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rUiPrepass.mVkPipelineLayout, 0, 2, pVkUiPrepassSets, 0, nullptr);
		vkCmdDrawIndirect(vkCommandBuffer, gpImGuiManager->mUiPrepassIndirectVkBuffer, static_cast<VkDeviceSize>(iFramebuffer) * sizeof(VkDrawIndirectCommand), 1, sizeof(VkDrawIndirectCommand));
	};
	gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerUiDepth);
	RecordUiDepth();
	gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerUiDepth);

	auto RecordOpaqueModels = [&]()
	{
		gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerObjects);
		for (const auto& [rCrc, pPipeline] : gpPipelineManager->mDynamicPipelines.mpModelPipelineMaps[kDynamicModelPipelineModel])
		{
			pPipeline->RecordDrawIndirect(iCommandBuffer, vkCommandBuffer, {0.0f, 0.0f, 0.0f, 0.0f}, ModelDrawPass::kOpaque);
		}
		gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerObjects);
	};

	bool bDebugTextureMode = false;
	if constexpr (kbDebugInput)
	{
		if (gDebugTexture.Get<bool>())
		{
			pPipelines[kPipelineDebugTexture].RecordDraw(iCommandBuffer, vkCommandBuffer, 1, 0);
			bDebugTextureMode = true;
		}
	}

	if (!bDebugTextureMode)
	{
		if (!(gpGraphics->mFlags & GraphicsFlags::kDrawModelsOverWorld))
		{
			RecordOpaqueModels();
		}

		gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerTerrain);
		// Per-template terrain draws: one indirect draw per IslandTemplate (count fixed at boot
		// from gpIslandTerrain->mIslandCrcsSorted). The stable arena is bound once; each acquired
		// framebuffer's indirect record supplies its template's arena offset and mesh-visible count.
		// Inactive templates have instanceCount=0 → zero instances rendered. Subscription changes update
		// only those per-frame records, so the command buffer remains record-once.
		pPipelines[kPipelineTerrain].RecordBindPipelineAndDescriptors(iCommandBuffer, vkCommandBuffer);
		vkCmdBindIndexBuffer(vkCommandBuffer, gpIslands->mIslandMeshArena.mDeviceLocalVkBuffer, 0, VK_INDEX_TYPE_UINT32);
		VkDeviceSize vkVertexOffset = 0;
		vkCmdBindVertexBuffers(vkCommandBuffer, 0, 1, &gpIslands->mIslandMeshArena.mDeviceLocalVkBuffer, &vkVertexOffset);
		for (int64_t i = 0; i < gpIslands->miTemplateCount; ++i)
		{
			vkCmdDrawIndexedIndirect(vkCommandBuffer, gpIslands->mIslandsIndirectVkBuffers.at(iFramebuffer), static_cast<VkDeviceSize>(i) * sizeof(VkDrawIndexedIndirectCommand), 1, sizeof(VkDrawIndexedIndirectCommand));
		}
		gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerTerrain);

		gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerWater);
		pPipelines[kPipelineWater].RecordDrawIndirect(iCommandBuffer, vkCommandBuffer, {0.0f, 0.0f, 0.0f, 0.0f});
		gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerWater);
		gpGraphics->mpLighting->RecordFixtureDraws(vkCommandBuffer, iFramebuffer);

		gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerHexShields);
		for (const auto& [rCrc, pPipeline] : gpPipelineManager->mDynamicPipelines.mpPipelineMaps[kDynamicPipelineHexShields])
		{
			pPipeline->RecordDrawIndirect(iCommandBuffer, vkCommandBuffer, {0.0f, 0.0f, 0.0f, 0.0f});
		}
		gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerHexShields);

		if (gpGraphics->mFlags & GraphicsFlags::kDrawModelsOverWorld)
		{
			VkClearAttachment vkClearAttachment
			{
				.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				.clearValue = {.depthStencil = {.depth = kfMaxDepth}},
			};
			VkClearRect vkClearRect
			{
				.rect = {.extent = gpGraphics->mFramebufferVkExtent2D},
				.baseArrayLayer = 0,
				.layerCount = 1,
			};
			vkCmdClearAttachments(vkCommandBuffer, 1, &vkClearAttachment, 1, &vkClearRect);
			RecordUiDepth();
			RecordOpaqueModels();
		}

		gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerTransparentObjects);
		// Transparent model pass (after all opaque geometry and water for correct blending)
		for (const auto& [rCrc, pPipeline] : gpPipelineManager->mDynamicPipelines.mpModelPipelineMaps[kDynamicModelPipelineModel])
		{
			if (pPipeline->mFlags & ModelPipelineFlags::kHasTransparentMaterials)
			{
				pPipeline->RecordDrawIndirect(iCommandBuffer, vkCommandBuffer, {0.0f, 0.0f, 0.0f, 0.0f}, ModelDrawPass::kTransparent);
			}
		}
		gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerTransparentObjects);

		gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerLongParticlesRender);
		pPipelines[kPipelineLongParticlesRender].RecordDrawIndirect(iCommandBuffer, vkCommandBuffer);
		gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerLongParticlesRender);

		gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerSquareParticlesRender);
		pPipelines[kPipelineSquareParticlesRender].RecordDrawIndirect(iCommandBuffer, vkCommandBuffer);
		gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerSquareParticlesRender);

		gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerVisibleLights);
		for (const auto& [rCrc, pPipeline] : gpPipelineManager->mDynamicPipelines.mpPipelineMaps[kDynamicPipelineVisibleLights])
		{
			pPipeline->RecordDrawIndirect(iCommandBuffer, vkCommandBuffer);
		}
		gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerVisibleLights);

		gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerBillboards);
		for (const auto& [rCrc, pPipeline] : gpPipelineManager->mDynamicPipelines.mpPipelineMaps[kDynamicPipelineBillboards])
		{
			pPipeline->RecordDrawIndirect(iCommandBuffer, vkCommandBuffer);
		}
		gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerBillboards);

		if constexpr (kbDebugRender)
		{
			pPipelines[kPipelineDebugBox].RecordDrawIndirect(iCommandBuffer, vkCommandBuffer);
			pPipelines[kPipelineDebugSphere].RecordDrawIndirect(iCommandBuffer, vkCommandBuffer);
			pPipelines[kPipelineDebugCircle].RecordDrawIndirect(iCommandBuffer, vkCommandBuffer);
			pPipelines[kPipelineDebugLine].RecordDrawIndirect(iCommandBuffer, vkCommandBuffer);
		}
	}

	vkCmdEndRenderPass(vkCommandBuffer);
}

void CommandBufferRecordMain::RecordHighDynamicRangeResolve(VkCommandBuffer vkCommandBuffer, int64_t iCommandBuffer, int64_t iFramebuffer)
{
	// HDR resolve: tone-map + color-grade the F16 scene intermediate into the swapchain. Single DONT_CARE
	// color attachment (fully overwritten by the fullscreen quad) — empty flags, no clear.
	gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerHdrResolve);
	Texture::RecordBeginRenderPass(vkCommandBuffer, gpSwapchainManager->mVkRenderPass, gpSwapchainManager->mFramebuffers.at(iFramebuffer).vkPresentFramebuffer, gpGraphics->mFramebufferVkExtent2D, VkClearColorValue {}, RenderPassFlags_t {});
	gpPipelineManager->mpPipelines[kPipelineHdrResolve].RecordDraw(iCommandBuffer, vkCommandBuffer, 1, 0);
	vkCmdEndRenderPass(vkCommandBuffer);
	gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerHdrResolve);
}

} // namespace engine

#endif // defined(BT_CLIENT)
