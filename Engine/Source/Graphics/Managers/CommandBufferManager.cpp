#if defined(BT_CLIENT)

#include "CommandBufferManager.h"

#include "CommandBufferRecordGlobal.h"
#include "CommandBufferRecordMain.h"

#include "Profile/ProfileRows.h"

namespace engine
{

CommandBufferManager::CommandBufferManager()
: common::Singleton<CommandBufferManager>(gpCommandBufferManager, this)
{
	ScopedBootTimer scopedBootTimer(kBootTimerCommandBufferManager);

	// Each drawing command binds a specific VkFramebuffer, so record one command buffer per swapchain image.
	mPerFramebufferCommandBuffers.reserve(static_cast<size_t>(std::ssize(gpSwapchainManager->mFramebuffers)));
	for (int64_t i = 0; i < std::ssize(gpSwapchainManager->mFramebuffers); ++i)
	{
		mPerFramebufferCommandBuffers.emplace_back(i);
	}

	VkSemaphoreCreateInfo vkSemaphoreCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
	};
	CHECK_VK(vkCreateSemaphore(gpDeviceManager->mVkDevice, &vkSemaphoreCreateInfo, nullptr, &mParticleSyncVkSemaphore));
}

CommandBufferManager::~CommandBufferManager()
{
	vkDestroySemaphore(gpDeviceManager->mVkDevice, mParticleSyncVkSemaphore, nullptr);
}

void CommandBufferManager::RecordCommandBuffers()
{
	ScopedBootTimer scopedBootTimer(kBootTimerRecordCommandBuffers);

	for (int64_t i = 0; i < std::ssize(mPerFramebufferCommandBuffers); ++i)
	{
		RecordCommandBuffer(i);
	}
}

void CommandBufferManager::RecordCommandBuffer(int64_t iFramebuffer)
{
	CommandBuffers& rCommandBuffers = mPerFramebufferCommandBuffers.at(iFramebuffer);

	// Guard: Command buffers are immutable after initial recording, only re-recorded when CommandBufferManager is destroyed/recreated (resize, device lost).
	if (rCommandBuffers.mFlags & CommandBufferFlags::kRecorded)
	{
		return;
	}
	rCommandBuffers.mFlags.Set(CommandBufferFlags::kRecorded);
	rCommandBuffers.mFlags.Set(CommandBufferFlags::kExecuted, false);

	LOG(kGraphics, kDebug, "Record command buffer: {}", iFramebuffer);

	CommandBufferRecordGlobal::Record(iFramebuffer);
	CommandBufferRecordMain::Record(iFramebuffer);
}

void CommandBufferManager::SubmitGlobalToQueue(int64_t iFramebufferIndex)
{
	CommandBuffers& rCommandBuffers = mPerFramebufferCommandBuffers.at(iFramebufferIndex);

	// Prepend the acquire command buffer when this frame recorded into it (QFOT acquires, recorded uploads).
	// kPendingAcquireBarriers and miAcquireFramebufferIndex are written on the main thread in TextureManager's
	// ProcessPendingTextures and EndAcquireCommandBuffer; under
	// kbRenderThread this read runs on the mSubmitGlobal worker, safe only because SubmitGlobalCommandBuffer's
	// mSubmitGlobal.Wake() edge published those writes first. Same family as CommandBuffers.h (mFlags/mVkFence).
	VkCommandBuffer pVkCommandBuffers[2] {};
	int64_t iCommandBufferCount = 0;
	if (gpTextureManager->mFlags & TextureManagerFlags::kPendingAcquireBarriers)
	{
		pVkCommandBuffers[iCommandBufferCount++] = gpTextureManager->mAcquireVkCommandBuffers.at(gpTextureManager->miAcquireFramebufferIndex);
	}
	pVkCommandBuffers[iCommandBufferCount++] = rCommandBuffers.mGlobalVkCommandBuffer;

	int64_t iWaitSemaphoreCount = 0;
	VkSemaphore pVkWaitSemaphores[1] {};
	VkPipelineStageFlags pVkWaitDestinationStageMask[1] {};
	if (mbParticleSemaphoreSignaled)
	{
		pVkWaitSemaphores[iWaitSemaphoreCount] = mParticleSyncVkSemaphore;
		pVkWaitDestinationStageMask[iWaitSemaphoreCount] = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		++iWaitSemaphoreCount;
	}

	VkSubmitInfo vkSubmitInfo
	{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext = nullptr,
		.waitSemaphoreCount = static_cast<uint32_t>(iWaitSemaphoreCount),
		.pWaitSemaphores = pVkWaitSemaphores,
		.pWaitDstStageMask = pVkWaitDestinationStageMask,
		.commandBufferCount = static_cast<uint32_t>(iCommandBufferCount),
		.pCommandBuffers = pVkCommandBuffers,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &rCommandBuffers.mGlobalFinishedVkSemaphore,
	};

	gpProfileManager->CpuStop(kCpuTimerAcquireToGlobal, {CpuStopFlags::kSmoothNow, CpuStopFlags::kCrossThread});

	gpProfileManager->CpuStart(kCpuTimerSubmitGlobal);
	CHECK_VK(vkQueueSubmit(gpDeviceManager->mGraphicsVkQueue, 1, &vkSubmitInfo, VK_NULL_HANDLE));
	gpProfileManager->CpuStop(kCpuTimerSubmitGlobal);

	rCommandBuffers.mFlags.Set(CommandBufferFlags::kExecuted);
}

void CommandBufferManager::SubmitGlobalCommandBuffer(int64_t iFramebufferIndex)
{
	if constexpr (kbRenderThread)
	{
		mSubmitGlobal.Wake([this, iFramebufferIndex]()
		{
			SubmitGlobalToQueue(iFramebufferIndex);
		});
	}
	else
	{
		SubmitGlobalToQueue(iFramebufferIndex);
	}
}

void CommandBufferManager::SubmitMainToQueue(int64_t iFramebufferIndex, bool bPreparationSubmitted)
{
	CommandBuffers& rCommandBuffers = mPerFramebufferCommandBuffers.at(iFramebufferIndex);
	VkPipelineStageFlags vkPreparationWaitStages = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo vkPreparationSubmit
	{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &rCommandBuffers.mGlobalFinishedVkSemaphore,
		.pWaitDstStageMask = &vkPreparationWaitStages,
		.commandBufferCount = 1,
		.pCommandBuffers = &rCommandBuffers.mPreparationVkCommandBuffer,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &rCommandBuffers.mPreparationFinishedVkSemaphore,
	};
	if (!bPreparationSubmitted)
	{
		CHECK_VK(vkQueueSubmit(gpDeviceManager->mGraphicsVkQueue, 1, &vkPreparationSubmit, VK_NULL_HANDLE));
	}

	VkSemaphore pVkSemaphores[]
	{
		rCommandBuffers.mPreparationFinishedVkSemaphore,
		gpSwapchainManager->mImageAvailableVkSemaphore,
	};
	VkPipelineStageFlags pVkPipelineStageFlags[]
	{
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
	};

	VkSemaphore pVkSignalSemaphores[]
	{
		rCommandBuffers.mMainFinishedVkSemaphore,
		mParticleSyncVkSemaphore,
	};

	VkSubmitInfo vkSubmitInfo
	{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext = nullptr,
		.waitSemaphoreCount = static_cast<uint32_t>(std::size(pVkSemaphores)),
		.pWaitSemaphores = pVkSemaphores,
		.pWaitDstStageMask = pVkPipelineStageFlags,
		.commandBufferCount = 1,
		.pCommandBuffers = &rCommandBuffers.mMainVkCommandBuffer,
		.signalSemaphoreCount = static_cast<uint32_t>(std::size(pVkSignalSemaphores)),
		.pSignalSemaphores = pVkSignalSemaphores,
	};
	gpProfileManager->CpuStart(kCpuTimerSubmitImage);
	// SubmitMainCommandBuffer resets mVkFence without signaling it; Graphics.cpp must follow it with
	// SubmitUiCommandBuffer so ImGuiManager::Submit signals the fence.
	CHECK_VK(vkResetFences(gpDeviceManager->mVkDevice, 1, &rCommandBuffers.mVkFence));
	CHECK_VK(vkQueueSubmit(gpDeviceManager->mGraphicsVkQueue, 1, &vkSubmitInfo, VK_NULL_HANDLE));
	gpProfileManager->CpuStop(kCpuTimerSubmitImage);

	// mParticleSyncVkSemaphore (signaled above via pVkSignalSemaphores) is waited by the NEXT frame's
	// SubmitGlobalToQueue, gated on mbParticleSemaphoreSignaled — a cross-frame, cross-method pairing that
	// holds only because a Main submit always precedes the next Global submit. Same "plain member published across
	// a PersistentWorker Wake/Wait edge" family as SwapchainManager::PresentToQueue (meDestroyType) and CommandBuffers.h
	// (mFlags/mVkFence).
	mbParticleSemaphoreSignaled = true;
}

void CommandBufferManager::SubmitMainCommandBuffer(int64_t iFramebufferIndex)
{
	CommandBuffers& rCommandBuffers = mPerFramebufferCommandBuffers.at(iFramebufferIndex);
	CHECK_VK(vkResetCommandBuffer(rCommandBuffers.mPreparationVkCommandBuffer, 0));
	VkCommandBufferBeginInfo vkBeginInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	CHECK_VK(vkBeginCommandBuffer(rCommandBuffers.mPreparationVkCommandBuffer, &vkBeginInfo));
	gpGraphics->mpLighting->RecordPreparation(rCommandBuffers.mPreparationVkCommandBuffer);
	CHECK_VK(vkEndCommandBuffer(rCommandBuffers.mPreparationVkCommandBuffer));
	bool bPreparationSubmitted = gpGraphics->mpLighting->CapturePreparation(rCommandBuffers.mPreparationVkCommandBuffer, rCommandBuffers.mGlobalFinishedVkSemaphore, rCommandBuffers.mPreparationFinishedVkSemaphore);

	if constexpr (kbRenderThread)
	{
		mSubmitMain.Wake([this, iFramebufferIndex, bPreparationSubmitted]()
		{
			mSubmitGlobal.Wait();
			SubmitMainToQueue(iFramebufferIndex, bPreparationSubmitted);
		});
	}
	else
	{
		SubmitMainToQueue(iFramebufferIndex, bPreparationSubmitted);
	}
}

void CommandBufferManager::SubmitUiCommandBuffer(int64_t iFramebufferIndex)
{
	mSubmitMain.Wait();

	gpImGuiManager->Submit(iFramebufferIndex);
}

} // namespace engine

#endif // defined(BT_CLIENT)
