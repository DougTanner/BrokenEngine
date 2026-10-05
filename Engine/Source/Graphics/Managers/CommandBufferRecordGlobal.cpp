#if defined(BT_CLIENT)

#include "CommandBufferRecordGlobal.h"

#include "CommandBufferManager.h"

#include "Profile/ProfileManager.h"

namespace engine
{

void CommandBufferRecordGlobal::Record(int64_t iFramebuffer)
{
	gpTextureManager->mTextureDescriptors.VerifyAllDescriptorGenerations();

	CommandBuffers& rCommandBuffers = gpCommandBufferManager->mPerFramebufferCommandBuffers.at(iFramebuffer);
	Pipeline* pPipelines = gpPipelineManager->mpPipelines;
	int64_t iCommandBuffer = iFramebuffer;

	VkCommandBufferBeginInfo vkCommandBufferBeginInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = 0,
		.pInheritanceInfo = nullptr,
	};

	VkCommandBuffer vkCommandBuffer = rCommandBuffers.mGlobalVkCommandBuffer;
	CHECK_VK(vkBeginCommandBuffer(vkCommandBuffer, &vkCommandBufferBeginInfo));
	gpProfileManager->ResetQueryPools(iCommandBuffer, vkCommandBuffer, kGpuTimerGlobal, kGpuTimerMain);

	gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerGlobal);

	gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerGlobalUniformCopy);
	gpBufferManager->mGlobalLayoutUniformBuffers.at(iCommandBuffer).RecordCopy(vkCommandBuffer);
	gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerGlobalUniformCopy);

	RecordShadowPasses(vkCommandBuffer, iCommandBuffer, pPipelines);

	RecordTerrainPasses(vkCommandBuffer, iCommandBuffer, pPipelines);

	int64_t iWindWidth = gpTextureManager->mRenderTargetTextures.mWindTextureOne.mInfo.vkExtent3D.width;
	int64_t iWindHeight = gpTextureManager->mRenderTargetTextures.mWindTextureOne.mInfo.vkExtent3D.height;
	int64_t iWindTilesX = TileCount(iWindWidth);
	int64_t iWindTilesY = TileCount(iWindHeight);

	int64_t iSmokeMaximumWidth = std::max(gpTextureManager->mRenderTargetTextures.mSmokeTextureOne.mInfo.vkExtent3D.width, gpTextureManager->mRenderTargetTextures.mSmokeTextureTwo.mInfo.vkExtent3D.width);
	int64_t iSmokeMaximumHeight = std::max(gpTextureManager->mRenderTargetTextures.mSmokeTextureOne.mInfo.vkExtent3D.height, gpTextureManager->mRenderTargetTextures.mSmokeTextureTwo.mInfo.vkExtent3D.height);
	int64_t iSmokeTilesX = TileCount(iSmokeMaximumWidth);
	int64_t iSmokeTilesY = TileCount(iSmokeMaximumHeight);

	gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerSpread);
	RecordWindSpreadPipeline(vkCommandBuffer, iCommandBuffer, iWindTilesX, iWindTilesY, pPipelines);
	RecordSmokeSpreadPipeline(vkCommandBuffer, iCommandBuffer, iSmokeTilesX, iSmokeTilesY, pPipelines);
	gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerSpread);

	gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerParticles);

	gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerParticlesSpawn);
	pPipelines[kPipelineLongParticlesSpawn].RecordCompute(iCommandBuffer, vkCommandBuffer, 1);
	pPipelines[kPipelineSquareParticlesSpawn].RecordCompute(iCommandBuffer, vkCommandBuffer, 1);
	gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerParticlesSpawn);

	RecordParticleUpdatePasses(vkCommandBuffer, iCommandBuffer, pPipelines);

	gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerParticles);

	gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerGlobal);

	CHECK_VK(vkEndCommandBuffer(vkCommandBuffer));
}

void CommandBufferRecordGlobal::RecordShadowPasses(VkCommandBuffer vkCommandBuffer, int64_t iCommandBuffer, Pipeline* pPipelines)
{
	gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerShadow);
	// Record-time group counts cover the whole shadow texture: every extent change destroys and re-records
	// these command buffers (Graphics recreation is at least the command-buffer destroy tier).
	int64_t iShadowTilesX = TileCount(gpTextureManager->mRenderTargetTextures.mShadowTexture.mInfo.vkExtent3D.width);
	int64_t iShadowTilesY = TileCount(gpTextureManager->mRenderTargetTextures.mShadowTexture.mInfo.vkExtent3D.height);
	gpTextureManager->mRenderTargetTextures.mShadowElevationTexture.RecordBeginRenderPass(vkCommandBuffer);
	// Fixed-count prepass consumes the shared global placement arena populated by
	// Islands::UpdateActiveIslands; cleared slots beyond the written total remain degenerate.
	pPipelines[kPipelineShadowElevation].RecordDraw(iCommandBuffer, vkCommandBuffer, kiMaxActivePlacements, 0, {1.0f, 0.0f, 0.0f, 0.0f});
	vkCmdEndRenderPass(vkCommandBuffer);
	pPipelines[kPipelineShadow].RecordCompute(iCommandBuffer, vkCommandBuffer, iShadowTilesX, iShadowTilesY);
	gpTextureManager->mRenderTargetTextures.mShadowTexture.TransitionImageLayout(vkCommandBuffer, TextureLayout::kComputeReadWrite, TextureLayout::kComputeReadOnly);
	gpTextureManager->mRenderTargetTextures.mShadowBlurIntermediateTexture.TransitionImageLayout(vkCommandBuffer, TextureLayout::kShaderReadOnly, TextureLayout::kComputeReadWrite);
	pPipelines[kPipelineShadowBlurH].RecordCompute(iCommandBuffer, vkCommandBuffer, iShadowTilesX, iShadowTilesY);
	gpTextureManager->mRenderTargetTextures.mShadowBlurIntermediateTexture.TransitionImageLayout(vkCommandBuffer, TextureLayout::kComputeReadWrite, TextureLayout::kComputeReadOnly);
	gpTextureManager->mRenderTargetTextures.mShadowBlurTexture.TransitionImageLayout(vkCommandBuffer, TextureLayout::kShaderReadOnly, TextureLayout::kComputeReadWrite);
	pPipelines[kPipelineShadowBlurV].RecordCompute(iCommandBuffer, vkCommandBuffer, iShadowTilesX, iShadowTilesY);
	// Same-layout barrier: ShadowTemporal reads and writes ShadowBlur in place after BlurV. Its history
	// sampler remains read-only, then ShadowHistoryCopy refreshes the distinct storage image after another barrier.
	gpTextureManager->mRenderTargetTextures.mShadowBlurTexture.TransitionImageLayout(vkCommandBuffer, TextureLayout::kComputeReadWrite, TextureLayout::kComputeReadWrite);
	gpTextureManager->mRenderTargetTextures.mShadowHistoryTexture.TransitionImageLayout(vkCommandBuffer, TextureLayout::kShaderReadOnly, TextureLayout::kComputeReadOnly);
	pPipelines[kPipelineShadowTemporal].RecordCompute(iCommandBuffer, vkCommandBuffer, iShadowTilesX, iShadowTilesY);
	gpTextureManager->mRenderTargetTextures.mShadowBlurTexture.TransitionImageLayout(vkCommandBuffer, TextureLayout::kComputeReadWrite, TextureLayout::kComputeReadWrite);
	gpTextureManager->mRenderTargetTextures.mShadowHistoryTexture.TransitionImageLayout(vkCommandBuffer, TextureLayout::kComputeReadOnly, TextureLayout::kComputeReadWrite);
	pPipelines[kPipelineShadowHistoryCopy].RecordCompute(iCommandBuffer, vkCommandBuffer, iShadowTilesX, iShadowTilesY);
	gpTextureManager->mRenderTargetTextures.mShadowBlurTexture.TransitionImageLayout(vkCommandBuffer, TextureLayout::kComputeReadWrite, TextureLayout::kShaderReadOnly);
	gpTextureManager->mRenderTargetTextures.mShadowHistoryTexture.TransitionImageLayout(vkCommandBuffer, TextureLayout::kComputeReadWrite, TextureLayout::kShaderReadOnly);
	gpTextureManager->mRenderTargetTextures.mShadowTexture.TransitionImageLayout(vkCommandBuffer, TextureLayout::kComputeReadOnly, TextureLayout::kComputeReadWrite);
	gpTextureManager->mRenderTargetTextures.mShadowBlurIntermediateTexture.TransitionImageLayout(vkCommandBuffer, TextureLayout::kComputeReadOnly, TextureLayout::kShaderReadOnly);
	gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerShadow);
}

void CommandBufferRecordGlobal::RecordTerrainPasses(VkCommandBuffer vkCommandBuffer, int64_t iCommandBuffer, Pipeline* pPipelines)
{
	// The fixed-count prepass consumes the shared global placement arena populated by
	// UpdateActiveIslands; cleared slots beyond the written total are zero-width quads that
	// QuadsAxisAlignedVisibleArea.vert culls.

	gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerTerrainElevation);
	gpTextureManager->mRenderTargetTextures.mTerrainElevationTexture.RecordBeginRenderPass(vkCommandBuffer);
	pPipelines[kPipelineTerrainElevation].RecordDraw(iCommandBuffer, vkCommandBuffer, kiMaxActivePlacements, 0);
	vkCmdEndRenderPass(vkCommandBuffer);
	gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerTerrainElevation);
}

void CommandBufferRecordGlobal::RecordWindSpreadPipeline(VkCommandBuffer vkCommandBuffer, int64_t iCommandBuffer, int64_t iWindTilesX, int64_t iWindTilesY, Pipeline* pPipelines)
{
	// Persistent spread state must drain and advance and has no simple disabled-state latch, so disabled-state recording stays fixed.
	gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerWindSpread);

	int64_t iWindDilateGroups = (iWindTilesX * iWindTilesY + shaders::kiOccupancyDilateGroupSize - 1) / shaders::kiOccupancyDilateGroupSize;

	uint32_t pWindResetCommand[3] {0, 1, 1};
	vkCmdUpdateBuffer(vkCommandBuffer, gpBufferManager->mWindActiveTileVkBuffers[0], 0, sizeof(pWindResetCommand), pWindResetCommand);
	vkCmdUpdateBuffer(vkCommandBuffer, gpBufferManager->mWindActiveTileVkBuffers[1], 0, sizeof(pWindResetCommand), pWindResetCommand);

	// Barrier: transfer -> compute (both active tile buffers)
	VkBufferMemoryBarrier pVkWindActiveTileResetBarriers[]
	{
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.buffer = gpBufferManager->mWindActiveTileVkBuffers[0],
			.offset = 0,
			.size = VK_WHOLE_SIZE,
		},
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.buffer = gpBufferManager->mWindActiveTileVkBuffers[1],
			.offset = 0,
			.size = VK_WHOLE_SIZE,
		},
	};
	vkCmdPipelineBarrier(vkCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, static_cast<uint32_t>(std::size(pVkWindActiveTileResetBarriers)), pVkWindActiveTileResetBarriers, 0, nullptr);

	// Both dilates (read different occupancy buffers, write different active tile buffers)
	pPipelines[kPipelineWindOccupancyDilateB].RecordCompute(iCommandBuffer, vkCommandBuffer, iWindDilateGroups);
	pPipelines[kPipelineWindOccupancyDilateA].RecordCompute(iCommandBuffer, vkCommandBuffer, iWindDilateGroups);

	// Barrier: compute read/write -> transfer write (occupancy), compute read/write -> indirect+compute (active tiles)
	VkBufferMemoryBarrier pVkWindDilateBarriers[]
	{
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.buffer = gpBufferManager->mWindActiveTileVkBuffers[0],
			.offset = 0,
			.size = VK_WHOLE_SIZE,
		},
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.buffer = gpBufferManager->mWindActiveTileVkBuffers[1],
			.offset = 0,
			.size = VK_WHOLE_SIZE,
		},
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.buffer = gpBufferManager->mWindOccupancyVkBuffers[0],
			.offset = 0,
			.size = VK_WHOLE_SIZE,
		},
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.buffer = gpBufferManager->mWindOccupancyVkBuffers[1],
			.offset = 0,
			.size = VK_WHOLE_SIZE,
		},
	};
	vkCmdPipelineBarrier(vkCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, static_cast<uint32_t>(std::size(pVkWindDilateBarriers)), pVkWindDilateBarriers, 0, nullptr);

	vkCmdFillBuffer(vkCommandBuffer, gpBufferManager->mWindOccupancyVkBuffers[0], 0, static_cast<VkDeviceSize>(gpBufferManager->miWindOccupancyBufferSize), 0);
	vkCmdFillBuffer(vkCommandBuffer, gpBufferManager->mWindOccupancyVkBuffers[1], 0, static_cast<VkDeviceSize>(gpBufferManager->miWindOccupancyBufferSize), 0);

	// Barrier: transfer -> compute (both occupancy buffers)
	VkBufferMemoryBarrier pVkWindOccupancyClearBarriers[]
	{
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.buffer = gpBufferManager->mWindOccupancyVkBuffers[0],
			.offset = 0,
			.size = VK_WHOLE_SIZE,
		},
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.buffer = gpBufferManager->mWindOccupancyVkBuffers[1],
			.offset = 0,
			.size = VK_WHOLE_SIZE,
		},
	};
	vkCmdPipelineBarrier(vkCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, static_cast<uint32_t>(std::size(pVkWindOccupancyClearBarriers)), pVkWindOccupancyClearBarriers, 0, nullptr);

	// SpreadB: dispatch from ActiveTileB (reads TextureOne, writes TextureTwo)
	gpTextureManager->mRenderTargetTextures.mWindTextureTwo.TransitionImageLayout(vkCommandBuffer, TextureLayout::kShaderReadOnly, TextureLayout::kComputeReadWrite);
	pPipelines[kPipelineWindSpreadComputeB].RecordComputeIndirectFrom(iCommandBuffer, vkCommandBuffer, gpBufferManager->mWindActiveTileVkBuffers[1], 0);
	gpTextureManager->mRenderTargetTextures.mWindTextureTwo.TransitionImageLayout(vkCommandBuffer, TextureLayout::kComputeReadWrite, TextureLayout::kShaderReadOnly);

	// SpreadA: dispatch from ActiveTileA (reads TextureTwo, writes TextureOne)
	gpTextureManager->mRenderTargetTextures.mWindTextureOne.TransitionImageLayout(vkCommandBuffer, TextureLayout::kShaderReadOnly, TextureLayout::kComputeReadWrite);
	pPipelines[kPipelineWindSpreadComputeA].RecordComputeIndirectFrom(iCommandBuffer, vkCommandBuffer, gpBufferManager->mWindActiveTileVkBuffers[0], 0);
	gpTextureManager->mRenderTargetTextures.mWindTextureOne.TransitionImageLayout(vkCommandBuffer, TextureLayout::kComputeReadWrite, TextureLayout::kShaderReadOnly);

	gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerWindSpread);
}

void CommandBufferRecordGlobal::RecordSmokeSpreadHalf(VkCommandBuffer vkCommandBuffer, int64_t iCommandBuffer, int64_t iDilateGroups, Pipeline& rDilatePipeline, VkBuffer vkOutputOccupancyBuffer, Texture& rSmokeTexture, Pipeline& rSpreadPipeline)
{
	uint32_t pResetCommand[3] {0, 1, 1};
	vkCmdUpdateBuffer(vkCommandBuffer, gpBufferManager->mSmokeActiveTileVkBuffer, 0, sizeof(pResetCommand), pResetCommand);

	// Barrier: transfer write → compute read for active tile buffer
	VkBufferMemoryBarrier vkActiveTileResetBarrier
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		.pNext = nullptr,
		.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.buffer = gpBufferManager->mSmokeActiveTileVkBuffer,
		.offset = 0,
		.size = VK_WHOLE_SIZE,
	};
	vkCmdPipelineBarrier(vkCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &vkActiveTileResetBarrier, 0, nullptr);

	// Dilate: read input and output occupancy, write active tile list
	rDilatePipeline.RecordCompute(iCommandBuffer, vkCommandBuffer, iDilateGroups);

	// Barrier: dilate compute → indirect read + compute read (active tile), output occupancy read → transfer write
	VkBufferMemoryBarrier pVkDilateBarriers[]
	{
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.buffer = gpBufferManager->mSmokeActiveTileVkBuffer,
			.offset = 0,
			.size = VK_WHOLE_SIZE,
		},
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.buffer = vkOutputOccupancyBuffer,
			.offset = 0,
			.size = VK_WHOLE_SIZE,
		},
	};
	vkCmdPipelineBarrier(vkCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, static_cast<uint32_t>(std::size(pVkDilateBarriers)), pVkDilateBarriers, 0, nullptr);

	// Clear output occupancy after the dilate has consumed its stale-tile union term, then let spread re-mark it.
	vkCmdFillBuffer(vkCommandBuffer, vkOutputOccupancyBuffer, 0, static_cast<VkDeviceSize>(gpBufferManager->miSmokeOccupancyBufferSize), 0);
	VkBufferMemoryBarrier vkOccupancyClearBarrier
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		.pNext = nullptr,
		.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.buffer = vkOutputOccupancyBuffer,
		.offset = 0,
		.size = VK_WHOLE_SIZE,
	};
	vkCmdPipelineBarrier(vkCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &vkOccupancyClearBarrier, 0, nullptr);

	rSmokeTexture.TransitionImageLayout(vkCommandBuffer, TextureLayout::kShaderReadOnly, TextureLayout::kComputeReadWrite);
	rSpreadPipeline.RecordComputeIndirectFrom(iCommandBuffer, vkCommandBuffer, gpBufferManager->mSmokeActiveTileVkBuffer, 0);
	rSmokeTexture.TransitionImageLayout(vkCommandBuffer, TextureLayout::kComputeReadWrite, TextureLayout::kShaderReadOnly);
}

void CommandBufferRecordGlobal::RecordSmokeSpreadPipeline(VkCommandBuffer vkCommandBuffer, int64_t iCommandBuffer, int64_t iSmokeTilesX, int64_t iSmokeTilesY, Pipeline* pPipelines)
{
	// After the one-shot texture clear, spread must consume and drain stale occupancy, so disabled-state recording remains fixed.
	gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerSmokeSpread);

	int64_t iTotalTiles = iSmokeTilesX * iSmokeTilesY;
	int64_t iDilateGroups = (iTotalTiles + shaders::kiOccupancyDilateGroupSize - 1) / shaders::kiOccupancyDilateGroupSize;

	// SpreadB: dilate the previous frame's occupancy, then indirect-spread into TextureTwo.
	RecordSmokeSpreadHalf(vkCommandBuffer, iCommandBuffer, iDilateGroups, pPipelines[kPipelineSmokeOccupancyDilate], gpBufferManager->mSmokeOccupancyVkBuffers[1], gpTextureManager->mRenderTargetTextures.mSmokeTextureTwo, pPipelines[kPipelineSmokeSpreadComputeB]);

	// Barrier between halves: SpreadB compute writes (occupancy) → SpreadA dilate reads; SpreadB indirect+compute reads (active tile) → transfer write
	VkBufferMemoryBarrier pVkSpreadBBarriers[]
	{
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.buffer = gpBufferManager->mSmokeOccupancyVkBuffers[1],
			.offset = 0,
			.size = VK_WHOLE_SIZE,
		},
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
			.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.buffer = gpBufferManager->mSmokeActiveTileVkBuffer,
			.offset = 0,
			.size = VK_WHOLE_SIZE,
		},
	};
	vkCmdPipelineBarrier(vkCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, static_cast<uint32_t>(std::size(pVkSpreadBBarriers)), pVkSpreadBBarriers, 0, nullptr);

	// SpreadA: dilate with the scale-aware remap (current-area UV -> world -> previous-area UV, so zoomed smoke
	// whose remapped sample-tile lies more than ~2 tiles from the output tile stays in the active list), then
	// indirect-spread into TextureOne.
	RecordSmokeSpreadHalf(vkCommandBuffer, iCommandBuffer, iDilateGroups, pPipelines[kPipelineSmokeOccupancyDilateRemap], gpBufferManager->mSmokeOccupancyVkBuffers[0], gpTextureManager->mRenderTargetTextures.mSmokeTextureOne, pPipelines[kPipelineSmokeSpreadComputeA]);

	gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerSmokeSpread);
}

void CommandBufferRecordGlobal::RecordParticleUpdatePasses(VkCommandBuffer vkCommandBuffer, int64_t iCommandBuffer, Pipeline* pPipelines)
{
	BarrierInfo pBarriers[]
	{
		{.eSource = BufferBarrier::kComputeReadWrite, .eDestination = BufferBarrier::kComputeRead, .vkBarrierBuffer = gpBufferManager->mLongParticlesStorageBuffer.mDeviceLocalVkBuffer},
		{.eSource = BufferBarrier::kComputeReadWrite, .eDestination = BufferBarrier::kShaderIndirectRead, .vkBarrierBuffer = pPipelines[kPipelineLongParticlesUpdate].mIndirectVkBuffer},
		{.eSource = BufferBarrier::kComputeReadWrite, .eDestination = BufferBarrier::kShaderIndirectRead, .vkBarrierBuffer = pPipelines[kPipelineLongParticlesRender].mIndirectVkBuffer},
		{.eSource = BufferBarrier::kComputeReadWrite, .eDestination = BufferBarrier::kComputeRead, .vkBarrierBuffer = gpBufferManager->mSquareParticlesStorageBuffer.mDeviceLocalVkBuffer},
		{.eSource = BufferBarrier::kComputeReadWrite, .eDestination = BufferBarrier::kShaderIndirectRead, .vkBarrierBuffer = pPipelines[kPipelineSquareParticlesUpdate].mIndirectVkBuffer},
		{.eSource = BufferBarrier::kComputeReadWrite, .eDestination = BufferBarrier::kShaderIndirectRead, .vkBarrierBuffer = pPipelines[kPipelineSquareParticlesRender].mIndirectVkBuffer},
	};
	Buffer::RecordBarriers(vkCommandBuffer, pBarriers);

	gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerLongParticlesUpdate);
	pPipelines[kPipelineLongParticlesUpdate].RecordComputeIndirect(iCommandBuffer, vkCommandBuffer);
	BarrierInfo pLongParticleBarrier[]
	{
		{.eSource = BufferBarrier::kComputeReadWrite, .eDestination = BufferBarrier::kStorageBufferRead, .vkBarrierBuffer = gpBufferManager->mLongParticlesStorageBuffer.mDeviceLocalVkBuffer},
	};
	Buffer::RecordBarriers(vkCommandBuffer, pLongParticleBarrier);
	gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerLongParticlesUpdate);

	gpProfileManager->GpuStart(iCommandBuffer, vkCommandBuffer, kGpuTimerSquareParticlesUpdate);
	pPipelines[kPipelineSquareParticlesUpdate].RecordComputeIndirect(iCommandBuffer, vkCommandBuffer);
	BarrierInfo pSquareParticleBarrier[]
	{
		{.eSource = BufferBarrier::kComputeReadWrite, .eDestination = BufferBarrier::kStorageBufferRead, .vkBarrierBuffer = gpBufferManager->mSquareParticlesStorageBuffer.mDeviceLocalVkBuffer},
	};
	Buffer::RecordBarriers(vkCommandBuffer, pSquareParticleBarrier);
	gpProfileManager->GpuStop(iCommandBuffer, vkCommandBuffer, kGpuTimerSquareParticlesUpdate);
}

} // namespace engine

#endif // defined(BT_CLIENT)
