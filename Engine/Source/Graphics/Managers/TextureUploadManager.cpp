#if defined(BT_CLIENT)

#include "TextureUploadManager.h"

#include "File/PackChunks.h"

namespace engine
{

TextureUploadManager::TextureUploadManager()
: common::Singleton<TextureUploadManager>(gpTextureUploadManager)
{
}

void TextureUploadManager::InitializeTransferResources()
{
	RethrowException();

	mbShutdown = false;
	// Reset the drain handshake with mbShutdown so a recreated upload thread starts clean.
	mbDrainRequested = false;
	mbDrained = false;
	mbThreadExited.store(false, std::memory_order_release);

	VkCommandPoolCreateInfo vkCommandPoolCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = static_cast<uint32_t>(gpInstanceManager->miTransferQueueFamilyIndex),
	};
	CHECK_VK(vkCreateCommandPool(gpDeviceManager->mVkDevice, &vkCommandPoolCreateInfo, nullptr, &mTransferVkCommandPool));
	VkName(VK_OBJECT_TYPE_COMMAND_POOL, mTransferVkCommandPool, "Transfer");

	VkCommandBufferAllocateInfo vkCommandBufferAllocateInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext = nullptr,
		.commandPool = mTransferVkCommandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1,
	};
	CHECK_VK(vkAllocateCommandBuffers(gpDeviceManager->mVkDevice, &vkCommandBufferAllocateInfo, &mTransferVkCommandBuffer));

	VkFenceCreateInfo vkFenceCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT,
	};
	CHECK_VK(vkCreateFence(gpDeviceManager->mVkDevice, &vkFenceCreateInfo, nullptr, &mTransferVkFence));
	VkName(VK_OBJECT_TYPE_FENCE, mTransferVkFence, "Transfer");

	VmaAllocationInfo stagingVmaAllocationInfo {};
	Buffer::CreateBuffer("TransferStaging", kiByteBudgetPerFrame, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, mStagingVkBuffer, mStagingVmaAllocation, &stagingVmaAllocationInfo);
	miStagingSize = kiByteBudgetPerFrame;
	mpStagingMappedData = stagingVmaAllocationInfo.pMappedData;
}

void TextureUploadManager::DestroyTransferResources()
{
	if (mTransferVkCommandPool == VK_NULL_HANDLE)
	{
		return;
	}

	// Join upload thread first. Drain any pending permit before the release: a WaitIdle that returned early via
	//   mbThreadExited (device-loss or fatal-error thread-exit path) can leave its probe permit pending, and an unguarded release
	//   would push the binary_semaphore past its max of 1 (UB). Same drain-then-release idiom as WaitIdle.
	mbShutdown = true;
	std::ignore = mFrameSignal.try_acquire();
	mFrameSignal.release();
	if (mUploadThread.joinable())
	{
		mUploadThread.join();
	}

	ResetUploadProgress();

	{
		std::unique_lock lock(mUploadMutex);
		mUploadQueue = {};
	}

	if (mTransferVkFence != VK_NULL_HANDLE)
	{
		// Teardown drain intentionally ignores this Vulkan result because CHECK_VK could throw before destruction completes.
		vkWaitForFences(gpDeviceManager->mVkDevice, 1, &mTransferVkFence, VK_TRUE, kFenceTimeoutNanoseconds.count());
	}

	if (mStagingVkBuffer != VK_NULL_HANDLE)
	{
		vmaDestroyBuffer(gpDeviceManager->mpAllocator, mStagingVkBuffer, mStagingVmaAllocation);
		mStagingVkBuffer = VK_NULL_HANDLE;
		mStagingVmaAllocation = VK_NULL_HANDLE;
		miStagingSize = 0;
	}

	// Clean up any GPU-uploaded texture images that were not adopted by TextureManager
	for (const auto& [rCrc, rLazyChunk] : gpFileManager->mpPackChunks->mLazyChunkMap)
	{
		if (rLazyChunk.vkUploadImage != VK_NULL_HANDLE)
		{
			LazyChunk& rMutableChunk = gpFileManager->mpPackChunks->mLazyChunkMap.at(rCrc);
			vmaDestroyImage(gpDeviceManager->mpAllocator, rMutableChunk.vkUploadImage, rMutableChunk.vmaAllocation);
			rMutableChunk.vkUploadImage = VK_NULL_HANDLE;
			rMutableChunk.vmaAllocation = VK_NULL_HANDLE;
		}
	}

	vkDestroyFence(gpDeviceManager->mVkDevice, mTransferVkFence, nullptr);
	mTransferVkFence = VK_NULL_HANDLE;

	vkDestroyCommandPool(gpDeviceManager->mVkDevice, mTransferVkCommandPool, nullptr);
	mTransferVkCommandPool = VK_NULL_HANDLE;
}

void TextureUploadManager::ResetUploadProgress()
{
	mCurrentCrc = 0;
	miCurrentLayer = 0;
	muiCurrentMip = 0;
	miCurrentMipY = 0;
	miCurrentDataOffset = 0;
}

void TextureUploadManager::StartThread()
{
	if (mUploadThread.joinable())
	{
		return;
	}
	mUploadThread = std::thread(common::ThreadLocal::Entry(&TextureUploadManager::UploadThread, common::kiMinWorkbufferSize, common::kThreadTextureUpload), this);
}

void TextureUploadManager::RequestUpload(common::crc_t crc, LoadPriority ePriority)
{
	{
		std::unique_lock lock(mUploadMutex);

		// Heap: priority_queue insertion may allocate. Items must persist until the upload thread pops them,
		//   so a workbuffer (frame-scoped) can't own them, and the queue grows/shrinks unpredictably
		ScopedSuppressAllocationTracking suppress;

		mUploadQueue.push({.crc = crc, .ePriority = ePriority});
	}
}

void TextureUploadManager::WaitIdle()
{
	// No upload thread can acknowledge a drain after shutdown/exit or before startup. First-boot Create calls Destroy before TextureManager
	// starts the thread, while mbShutdown is false. This thread is the sole starter/joiner, so joinable() is main-thread-only state.
	if (mbShutdown || mbThreadExited || !mUploadThread.joinable())
	{
		return;
	}

	// UploadThread consumes mFrameSignal before taking mWorkMutex, so locking alone cannot prevent a pending submit from racing teardown's vkDeviceWaitIdle.
	// Each iteration holds mWorkMutex through submission. A drain probe waits for an active iteration and makes the next iteration acknowledge without submitting.
	// Binary-semaphore frame signals resume uploads; the condition variable is used only for teardown.
	{
		std::unique_lock lock(mWorkMutex);
		mbDrained = false;
		mbDrainRequested = true;
	}

	// Ensure exactly one wake is pending (binary_semaphore max count is 1) so a parked thread observes the
	//   request; try_acquire first so the release cannot exceed the max.
	std::ignore = mFrameSignal.try_acquire();
	mFrameSignal.release();

	std::unique_lock lock(mWorkMutex);
	mIdleConditionVariable.wait(lock, [this] { return mbDrained || mbThreadExited; });
	mbDrainRequested = false;
}

void TextureUploadManager::SignalFrame()
{
	// Drain before releasing so the binary semaphore cannot exceed its maximum count of 1 while the upload thread is mid-iteration.
	// A dropped wake is posted again by the next frame signal.
	std::ignore = mFrameSignal.try_acquire();
	mFrameSignal.release();
}

void TextureUploadManager::RethrowException()
{
	// Healthy per-frame polling stays lock-free. The acquire pairs with UploadThread's release-store, so a
	// fatal exit publishes mException before this thread takes the mutex and consumes the mailbox.
	if (!mbThreadExited.load(std::memory_order_acquire)) [[likely]]
	{
		return;
	}

	std::unique_lock lock(mWorkMutex);
	if (mException != nullptr) [[unlikely]]
	{
		std::exception_ptr exception = std::exchange(mException, nullptr);
		std::rethrow_exception(exception);
	}
}

void TextureUploadManager::UploadThread()
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

	while (!mbShutdown)
	{
		// Wait for the main thread to signal one chunk this frame
		mFrameSignal.acquire();
		if (mbShutdown)
		{
			// mWorkMutex is not yet held here (workLock is taken below), so lock it to set the exit flag and
			//   notify under the lock -- a WaitIdle parked on mIdleConditionVariable then sees no lost wakeup.
			{
				std::unique_lock exitLock(mWorkMutex);
				mbThreadExited.store(true, std::memory_order_release);
				mIdleConditionVariable.notify_all();
			}
			break;
		}

		std::unique_lock workLock(mWorkMutex);

		// WaitIdle() drain probe: any real iteration in flight when WaitIdle took mWorkMutex already finished
		//   its submit (we hold mWorkMutex across each iteration), so ack from this quiescent point and re-park
		//   without submitting. Queued requests persist and resume on the next frame's signal.
		if (mbDrainRequested)
		{
			mbDrainRequested = false;
			mbDrained = true;
			mIdleConditionVariable.notify_all();
			// Swallow the pending probe permit so it can't drive one more acquire() -> vkQueueSubmit after
			//   WaitIdle returns (the stray permit would race teardown's vkDeviceWaitIdle). Same idiom as
			//   TextureManager::WaitForTextures; a real frame signal re-posts next frame by design.
			std::ignore = mFrameSignal.try_acquire();
			continue;
		}

		try
		{
			if (!DequeueNextUpload())
			{
				continue;
			}

			LazyChunk& rLazyChunk = gpFileManager->mpPackChunks->mLazyChunkMap.at(mCurrentCrc);
			ValidateTextureDimensions(rLazyChunk);

			if (HandleUploadEarlyOut(rLazyChunk))
			{
				continue;
			}

			// Wait for previous submission (fence starts signaled, so first wait is free)
			CHECK_VK(vkWaitForFences(gpDeviceManager->mVkDevice, 1, &mTransferVkFence, VK_TRUE, kFenceTimeoutNanoseconds.count()));

			bool bFirstChunk = (miCurrentDataOffset == 0);
			bool bCubemap = rLazyChunk.header.flags & common::ChunkFlags::kCubemap;
			VkFormat vkFormat = rLazyChunk.header.textureHeader.vkFormat;
			bool bCompressed = (vkFormat == VK_FORMAT_BC4_UNORM_BLOCK || vkFormat == VK_FORMAT_BC5_UNORM_BLOCK || vkFormat == VK_FORMAT_BC7_UNORM_BLOCK);
			ChunkDimensions dimensions
			{
				.bCubemap = bCubemap,
				.vkFormat = vkFormat,
				.iBlockHeight = bCompressed ? 4i64 : 1i64,
				.iArrayLayers = bCubemap ? 6i64 : 1i64,
				.iMipLevels = rLazyChunk.header.textureHeader.iMipLevels,
				.uiBaseWidth = static_cast<uint32_t>(rLazyChunk.header.textureHeader.iTextureWidth),
				.uiBaseHeight = static_cast<uint32_t>(rLazyChunk.header.textureHeader.iTextureHeight),
			};

			if (bFirstChunk)
			{
				CreateTransferImage(rLazyChunk, dimensions);
			}

			CHECK_VK(vkResetCommandBuffer(mTransferVkCommandBuffer, 0));
			VkCommandBufferBeginInfo vkCommandBufferBeginInfo
			{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.pNext = nullptr,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
				.pInheritanceInfo = nullptr,
			};
			CHECK_VK(vkBeginCommandBuffer(mTransferVkCommandBuffer, &vkCommandBufferBeginInfo));

			VkImageMemoryBarrier vkImageMemoryBarrier
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = 0,
				.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = rLazyChunk.vkUploadImage,
				.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = static_cast<uint32_t>(dimensions.iMipLevels), .baseArrayLayer = 0, .layerCount = static_cast<uint32_t>(dimensions.iArrayLayers)},
			};
			if (bFirstChunk)
			{
				vkCmdPipelineBarrier(mTransferVkCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &vkImageMemoryBarrier);
			}

			RecordStagingCopies(rLazyChunk, dimensions);

			bool bDone = (miCurrentLayer >= dimensions.iArrayLayers);

			SubmitChunkUpload(rLazyChunk, vkImageMemoryBarrier, bDone);
		}
		catch (const std::system_error& rException)
		{
			if (rException.code() != VkErrorCode(VK_ERROR_DEVICE_LOST))
			{
				mException = std::current_exception();
				mbThreadExited.store(true, std::memory_order_release);
				mIdleConditionVariable.notify_all();
				break;
			}

			// Device lost during upload -- DestroyTransferResources will clean up GPU resources.
			LazyChunk& rLazyChunk = gpFileManager->mpPackChunks->mLazyChunkMap.at(mCurrentCrc);
			rLazyChunk.eState.value.store(ChunkState::kDiskLoaded, std::memory_order_release);
			miPendingAdoptions.fetch_add(1, std::memory_order_relaxed); // kUploading -> kDiskLoaded: arm the pending-adoption counter
			mCurrentCrc = 0;
			// workLock already holds the non-recursive mWorkMutex; locking it again would self-deadlock.
			// Publish the exit and notify the drain waiter before leaving the thread, since it cannot acknowledge another probe.
			mbThreadExited.store(true, std::memory_order_release);
			mIdleConditionVariable.notify_all();
			break;
		}
		catch (...)
		{
			// Unexpected failures are process-fatal. Keep the chunk kUploading and preserve any unadopted image;
			// Graphics teardown waits for the device before DestroyTransferResources destroys that image.
			mException = std::current_exception();
			mbThreadExited.store(true, std::memory_order_release);
			mIdleConditionVariable.notify_all();
			break;
		}
	}
}

bool TextureUploadManager::DequeueNextUpload()
{
	// An empty queue leaves the upload thread parked until another frame signal.
	if (mCurrentCrc == 0)
	{
		std::unique_lock lock(mUploadMutex);
		if (mUploadQueue.empty())
		{
			return false;
		}

		mCurrentCrc = mUploadQueue.top().crc;
		mUploadQueue.pop();
	}

	return true;
}

bool TextureUploadManager::HandleUploadEarlyOut(LazyChunk& rLazyChunk)
{
	ASSERT(mTransferVkCommandPool != VK_NULL_HANDLE);

	// Early out: same queue (concurrent vkQueueSubmit is not thread-safe)
	if (gpDeviceManager->mTransferVkQueue == gpDeviceManager->mGraphicsVkQueue)
	{
		rLazyChunk.eState.value.store(ChunkState::kDiskLoaded, std::memory_order_release);
		miPendingAdoptions.fetch_add(1, std::memory_order_relaxed); // kUploading -> kDiskLoaded: arm the pending-adoption counter
		gpFileManager->mpPackChunks->mLoader.NotifyChunkCompletion();
		mCurrentCrc = 0;
		return true;
	}

	return false;
}

void TextureUploadManager::ValidateTextureDimensions(const LazyChunk& rLazyChunk)
{
	static constexpr int64_t kiMaxTextureDimension = 16'384;
	static constexpr int64_t kiMaxMipLevels = 15;

	const common::TextureHeader& rTextureHeader = rLazyChunk.header.textureHeader;
	ASSERT(rTextureHeader.iTextureWidth > 0);
	ASSERT(rTextureHeader.iTextureHeight > 0);
	ASSERT(rTextureHeader.iMipLevels > 0);
	ASSERT(rTextureHeader.iTextureWidth <= kiMaxTextureDimension);
	ASSERT(rTextureHeader.iTextureHeight <= kiMaxTextureDimension);
	ASSERT(rTextureHeader.iMipLevels <= kiMaxMipLevels);

	uint64_t uiMaxDimension = static_cast<uint64_t>(std::max(rTextureHeader.iTextureWidth, rTextureHeader.iTextureHeight));
	ASSERT(rTextureHeader.iMipLevels <= static_cast<int64_t>(std::bit_width(uiMaxDimension)));

	bool bCubemap = rLazyChunk.header.flags & common::ChunkFlags::kCubemap;
	VkImageCreateFlags vkImageCreateFlags = bCubemap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : static_cast<VkImageCreateFlags>(0);
	VkImageFormatProperties vkImageFormatProperties {};
	VkResult vkResult = vkGetPhysicalDeviceImageFormatProperties(gpInstanceManager->mVkPhysicalDevice, rTextureHeader.vkFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, vkImageCreateFlags, &vkImageFormatProperties);
	ASSERT(vkResult == VK_SUCCESS);
	ASSERT(rTextureHeader.iTextureWidth <= static_cast<int64_t>(vkImageFormatProperties.maxExtent.width));
	ASSERT(rTextureHeader.iTextureHeight <= static_cast<int64_t>(vkImageFormatProperties.maxExtent.height));
	ASSERT(rTextureHeader.iMipLevels <= static_cast<int64_t>(vkImageFormatProperties.maxMipLevels));

	if (bCubemap)
	{
		ASSERT(rTextureHeader.iTextureWidth == rTextureHeader.iTextureHeight);
	}

	int64_t iArrayLayers = bCubemap ? 6 : 1;
	int64_t iExpectedBytes = common::ComputeImageByteSize(rTextureHeader.vkFormat, rTextureHeader.iTextureWidth, rTextureHeader.iTextureHeight, rTextureHeader.iMipLevels, iArrayLayers, 1);
	ASSERT(iExpectedBytes > 0 && iExpectedBytes <= rLazyChunk.iDataSize);
}

void TextureUploadManager::CreateTransferImage(LazyChunk& rLazyChunk, const ChunkDimensions& rDimensions)
{
	VkImageCreateInfo vkImageCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = rDimensions.bCubemap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : static_cast<VkImageCreateFlags>(0),
		.imageType = VK_IMAGE_TYPE_2D,
		.format = rDimensions.vkFormat,
		.extent = VkExtent3D {.width = rDimensions.uiBaseWidth, .height = rDimensions.uiBaseHeight, .depth = 1},
		.mipLevels = static_cast<uint32_t>(rDimensions.iMipLevels),
		.arrayLayers = static_cast<uint32_t>(rDimensions.iArrayLayers),
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};
	VmaAllocationCreateInfo vmaAllocationCreateInfo {};
	vmaAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
	CHECK_VK(vmaCreateImage(gpDeviceManager->mpAllocator, &vkImageCreateInfo, &vmaAllocationCreateInfo, &rLazyChunk.vkUploadImage, &rLazyChunk.vmaAllocation, nullptr));
}

void TextureUploadManager::RecordStagingCopies(const LazyChunk& rLazyChunk, const ChunkDimensions& rDimensions)
{
	const std::byte* pData = rLazyChunk.pData;
	int64_t iStagingUsed = 0;

	while (iStagingUsed < miStagingSize && miCurrentLayer < rDimensions.iArrayLayers)
	{
		int64_t iMipWidth = std::max(rDimensions.uiBaseWidth >> muiCurrentMip, 1ui32);
		int64_t iMipHeight = std::max(rDimensions.uiBaseHeight >> muiCurrentMip, 1ui32);
		int64_t iRemainingHeight = iMipHeight - miCurrentMipY;
		int64_t iRemainingMipBytes = common::SizeInBytes(rDimensions.vkFormat, iMipWidth, iRemainingHeight);
		int64_t iRemainingStaging = miStagingSize - iStagingUsed;

		if (iRemainingMipBytes <= iRemainingStaging)
		{
			std::memcpy(static_cast<std::byte*>(mpStagingMappedData) + iStagingUsed, pData + miCurrentDataOffset, iRemainingMipBytes);

			VkBufferImageCopy vkBufferImageCopy {};
			vkBufferImageCopy.bufferOffset = static_cast<VkDeviceSize>(iStagingUsed);
			vkBufferImageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			vkBufferImageCopy.imageSubresource.mipLevel = muiCurrentMip;
			vkBufferImageCopy.imageSubresource.baseArrayLayer = static_cast<uint32_t>(miCurrentLayer);
			vkBufferImageCopy.imageSubresource.layerCount = 1;
			vkBufferImageCopy.imageOffset = {.x = 0, .y = static_cast<int32_t>(miCurrentMipY), .z = 0};
			vkBufferImageCopy.imageExtent = {.width = static_cast<uint32_t>(iMipWidth), .height = static_cast<uint32_t>(iRemainingHeight), .depth = 1};
			vkCmdCopyBufferToImage(mTransferVkCommandBuffer, mStagingVkBuffer, rLazyChunk.vkUploadImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &vkBufferImageCopy);

			iStagingUsed += iRemainingMipBytes;
			miCurrentDataOffset += iRemainingMipBytes;
			miCurrentMipY = 0;
			++muiCurrentMip;
			if (muiCurrentMip >= rDimensions.iMipLevels)
			{
				muiCurrentMip = 0;
				++miCurrentLayer;
			}
		}
		else
		{
			// Partial mip: chunk height must satisfy VUID-vkCmdCopyBufferToImage-imageOffset-07738.
			// The Vulkan validator interprets minImageTransferGranularity as BLOCK-relative for
			// compressed formats, so the chunk's BLOCK extent (chunk_pixels / blockHeight) must
			// be a multiple of granularity.height. For uncompressed (blockHeight == 1) this
			// degenerates to `granularity.height` pixels per chunk; for BCn (blockHeight == 4)
			// it scales up to `granularity.height * 4 = 64` pixels per chunk on hardware that
			// reports granularity.height == 16.
			int64_t iChunkHeight = std::max(1i64, static_cast<int64_t>(gpInstanceManager->mTransferImageGranularityVkExtent3D.height)) * rDimensions.iBlockHeight;
			int64_t iBytesPerChunk = common::SizeInBytes(rDimensions.vkFormat, iMipWidth, iChunkHeight);
			int64_t iChunksThatFit = iRemainingStaging / iBytesPerChunk;
			if (iChunksThatFit == 0)
			{
				break;
			}

			int64_t iCopyHeight = iChunksThatFit * iChunkHeight;
			int64_t iCopyBytes = common::SizeInBytes(rDimensions.vkFormat, iMipWidth, iCopyHeight);

			std::memcpy(static_cast<std::byte*>(mpStagingMappedData) + iStagingUsed, pData + miCurrentDataOffset, iCopyBytes);

			VkBufferImageCopy vkBufferImageCopy {};
			vkBufferImageCopy.bufferOffset = static_cast<VkDeviceSize>(iStagingUsed);
			vkBufferImageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			vkBufferImageCopy.imageSubresource.mipLevel = muiCurrentMip;
			vkBufferImageCopy.imageSubresource.baseArrayLayer = static_cast<uint32_t>(miCurrentLayer);
			vkBufferImageCopy.imageSubresource.layerCount = 1;
			vkBufferImageCopy.imageOffset = {.x = 0, .y = static_cast<int32_t>(miCurrentMipY), .z = 0};
			vkBufferImageCopy.imageExtent = {.width = static_cast<uint32_t>(iMipWidth), .height = static_cast<uint32_t>(iCopyHeight), .depth = 1};
			vkCmdCopyBufferToImage(mTransferVkCommandBuffer, mStagingVkBuffer, rLazyChunk.vkUploadImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &vkBufferImageCopy);

			miCurrentDataOffset += iCopyBytes;
			miCurrentMipY += iCopyHeight;
			break; // staging full
		}
	}
}

void TextureUploadManager::SubmitChunkUpload(LazyChunk& rLazyChunk, VkImageMemoryBarrier& rVkImageMemoryBarrier, bool bDone)
{
	if (bDone)
	{
		bool bSeparateTransferFamily = gpInstanceManager->miTransferQueueFamilyIndex != gpInstanceManager->miGraphicsQueueFamilyIndex;
		if (bSeparateTransferFamily)
		{
			if (gpDeviceManager->mCapabilities & DeviceCapabilityFlags::kTransferQueueFamilyOwnershipTransferOptional)
			{
				// QFOT optional (VK_KHR_maintenance9): no ownership transfer needed, transition layout directly
				rVkImageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				rVkImageMemoryBarrier.dstAccessMask = 0;
				rVkImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				rVkImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				rVkImageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				rVkImageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			}
			else
			{
				// Queue family release barrier (transfer -> graphics)
				rVkImageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				rVkImageMemoryBarrier.dstAccessMask = 0;
				rVkImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				rVkImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				rVkImageMemoryBarrier.srcQueueFamilyIndex = static_cast<uint32_t>(gpInstanceManager->miTransferQueueFamilyIndex);
				rVkImageMemoryBarrier.dstQueueFamilyIndex = static_cast<uint32_t>(gpInstanceManager->miGraphicsQueueFamilyIndex);
			}
			vkCmdPipelineBarrier(mTransferVkCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &rVkImageMemoryBarrier);
		}
		else
		{
			rVkImageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			rVkImageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			rVkImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			rVkImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			rVkImageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			rVkImageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			vkCmdPipelineBarrier(mTransferVkCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &rVkImageMemoryBarrier);
		}
	}

	CHECK_VK(vkEndCommandBuffer(mTransferVkCommandBuffer));

	VkSubmitInfo vkSubmitInfo
	{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext = nullptr,
		.waitSemaphoreCount = 0,
		.pWaitSemaphores = nullptr,
		.pWaitDstStageMask = nullptr,
		.commandBufferCount = 1,
		.pCommandBuffers = &mTransferVkCommandBuffer,
		.signalSemaphoreCount = 0,
		.pSignalSemaphores = nullptr,
	};
	CHECK_VK(vkResetFences(gpDeviceManager->mVkDevice, 1, &mTransferVkFence));
	CHECK_VK(vkQueueSubmit(gpDeviceManager->mTransferVkQueue, 1, &vkSubmitInfo, mTransferVkFence));

	if (bDone)
	{
		// Wait for GPU to finish before signaling completion
		CHECK_VK(vkWaitForFences(gpDeviceManager->mVkDevice, 1, &mTransferVkFence, VK_TRUE, kFenceTimeoutNanoseconds.count()));

		rLazyChunk.eState.value.store(ChunkState::kGpuUploadComplete, std::memory_order_release);
		miPendingAdoptions.fetch_add(1, std::memory_order_relaxed); // kUploading -> kGpuUploadComplete: arm the pending-adoption counter
		gpFileManager->mpPackChunks->mLoader.NotifyChunkCompletion();

		ResetUploadProgress();
	}
}

} // namespace engine

#endif // defined(BT_CLIENT)
