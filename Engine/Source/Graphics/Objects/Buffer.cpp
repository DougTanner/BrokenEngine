#if defined(BT_CLIENT)

#include "Buffer.h"

namespace engine
{

using enum BufferFlags;

void Buffer::CreateBuffer([[maybe_unused]] std::string_view name, int64_t iDeviceSize, VkBufferUsageFlags vkBufferUsageFlags, VkMemoryPropertyFlags vkMemoryPropertyFlags, VkBuffer& rVkBuffer, VmaAllocation& rVmaAllocation, VmaAllocationInfo* pVmaAllocationInfo, BufferMemoryPlacement eMemoryPlacement)
{
	int64_t iRoundedDeviceSize = common::RoundUp(iDeviceSize, static_cast<int64_t>(gpInstanceManager->mVkPhysicalDeviceProperties.limits.nonCoherentAtomSize));

	VkBufferCreateInfo vkBufferCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.size = static_cast<VkDeviceSize>(iRoundedDeviceSize),
		.usage = vkBufferUsageFlags,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
	};

	const VkPhysicalDeviceMemoryProperties& rVkPhysicalDeviceMemoryProperties = gpInstanceManager->mVkPhysicalDeviceMemoryProperties;
	uint32_t uiOrdinaryRamTypes = 0;
	for (uint32_t i = 0; i < rVkPhysicalDeviceMemoryProperties.memoryTypeCount; ++i)
	{
		const VkMemoryType& rVkMemoryType = rVkPhysicalDeviceMemoryProperties.memoryTypes[i];
		if ((rVkMemoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0 && (rVkPhysicalDeviceMemoryProperties.memoryHeaps[rVkMemoryType.heapIndex].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) == 0)
		{
			uiOrdinaryRamTypes |= 1ui32 << i;
		}
	}

	// Only ordinary RAM placement and diagnosed OOM recovery relax DEVICE_LOCAL; every other requested property remains required.
	// RAM allocations are coherent and persistently mapped because host consumers have no flush/invalidate path.
	VkMemoryPropertyFlags vkRamRequiredFlags = (vkMemoryPropertyFlags & ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	auto MakeAllocationInfo = [vkBufferUsageFlags](VkMemoryPropertyFlags vkRequiredFlags, uint32_t uiAllowedTypes)
	{
		VmaAllocationCreateInfo info
		{
			.flags = 0,
			.usage = VMA_MEMORY_USAGE_AUTO,
			.requiredFlags = vkRequiredFlags,
			.memoryTypeBits = uiAllowedTypes,
		};
		if ((vkRequiredFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0)
		{
			bool bIsReadbackBuffer = (vkBufferUsageFlags & VK_BUFFER_USAGE_TRANSFER_DST_BIT) != 0 && (vkBufferUsageFlags & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) == 0;
			info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | (bIsReadbackBuffer ? VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT : VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
		}
		return info;
	};
	VmaAllocationCreateInfo vmaRamAllocationCreateInfo = MakeAllocationInfo(vkRamRequiredFlags, uiOrdinaryRamTypes);
	VmaAllocationCreateInfo vmaAllocationCreateInfo = eMemoryPlacement == BufferMemoryPlacement::kOrdinaryRam ? vmaRamAllocationCreateInfo : MakeAllocationInfo(vkMemoryPropertyFlags, 0);

	VmaAllocationInfo vmaAllocationInfo {};
	// TEMP START memory allocation observation
	int64_t iMemoryThread = GetCurrentThreadId();
	LOG(kTemp, kInfo, "MEMORY CREATE_BEGIN thread={} site=Buffer.cpp owner={} destination={}", iMemoryThread, name, static_cast<const void*>(&rVkBuffer));
	LOG(kTemp, kInfo, "MEMORY BUFFER_OWNER thread={} requested={} rounded={} terrainValidationObserver={} addedOperandObserver={}", iMemoryThread, iDeviceSize, iRoundedDeviceSize, name == "LightingTerrainWriteCounts" || name == "LightingTerrainOriginalXYZ" || name == "LightingTerrainBaseFilteredZ" || name == "LightingTerrainEvidence" || name == "LightingTerrainEvidenceReadback" || name == "LightingTerrainSavedR16" || name == "LightingTerrainValidationSummary" || name == "LightingTerrainValidationTiles" || name == "LightingTerrainSummaryReadback" || name == "LightingTerrainTileReadback", name == "LightingTerrainBaseFilteredZ" || name == "LightingTerrainEvidence" || name == "LightingTerrainEvidenceReadback");
	LOG(kTemp, kInfo, "MEMORY BUFFER_REQUEST thread={} flags={} size={} usage={} sharing={} queueCount={} pNext={}", iMemoryThread, vkBufferCreateInfo.flags, vkBufferCreateInfo.size, vkBufferCreateInfo.usage, static_cast<int32_t>(vkBufferCreateInfo.sharingMode), vkBufferCreateInfo.queueFamilyIndexCount, vkBufferCreateInfo.pNext);
	LOG(kTemp, kInfo, "MEMORY VMA_REQUEST thread={} flags={} usage={} required={} preferred={} typeBits={} pool={}", iMemoryThread, vmaAllocationCreateInfo.flags, static_cast<int32_t>(vmaAllocationCreateInfo.usage), vmaAllocationCreateInfo.requiredFlags, vmaAllocationCreateInfo.preferredFlags, vmaAllocationCreateInfo.memoryTypeBits, reinterpret_cast<uint64_t>(vmaAllocationCreateInfo.pool));
	// TEMP END memory allocation observation
	VkBuffer vkBuffer = VK_NULL_HANDLE;
	VmaAllocation vmaAllocation = VK_NULL_HANDLE;
	VkResult vkMemoryCreateResult = VK_ERROR_FEATURE_NOT_PRESENT;
	// VMA treats zero memoryTypeBits as unrestricted, so an unavailable RAM heap must not reach VMA.
	if (eMemoryPlacement != BufferMemoryPlacement::kOrdinaryRam || uiOrdinaryRamTypes != 0)
	{
		vkMemoryCreateResult = vmaCreateBuffer(gpDeviceManager->mpAllocator, &vkBufferCreateInfo, &vmaAllocationCreateInfo, &vkBuffer, &vmaAllocation, &vmaAllocationInfo);
	}
	if (eMemoryPlacement == BufferMemoryPlacement::kDefault && (vkMemoryPropertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0 && vkMemoryCreateResult == VK_ERROR_OUT_OF_DEVICE_MEMORY)
	{
		LOG(kGraphics, kWarning, "Buffer {} device-local allocation exhausted: size={} usage={} required={} result={}; ordinary RAM allowed types={}", name, iRoundedDeviceSize, vkBufferUsageFlags, vkMemoryPropertyFlags, static_cast<int32_t>(vkMemoryCreateResult), uiOrdinaryRamTypes);
		DEBUG_BREAK();
		if (uiOrdinaryRamTypes != 0)
		{
			// VMA destroys failed-create resources and clears its outputs before this single recovery attempt.
			vkMemoryCreateResult = vmaCreateBuffer(gpDeviceManager->mpAllocator, &vkBufferCreateInfo, &vmaRamAllocationCreateInfo, &vkBuffer, &vmaAllocation, &vmaAllocationInfo);
			if (vkMemoryCreateResult == VK_SUCCESS)
			{
				const VkMemoryType& rVkMemoryType = rVkPhysicalDeviceMemoryProperties.memoryTypes[vmaAllocationInfo.memoryType];
				LOG(kGraphics, kWarning, "Buffer {} ordinary RAM recovery result={} type={} heap={} heapFlags={} properties={} allocated={} mapped={}", name, static_cast<int32_t>(vkMemoryCreateResult), vmaAllocationInfo.memoryType, rVkMemoryType.heapIndex, rVkPhysicalDeviceMemoryProperties.memoryHeaps[rVkMemoryType.heapIndex].flags, rVkMemoryType.propertyFlags, vmaAllocationInfo.size, vmaAllocationInfo.pMappedData != nullptr);
			}
			else
			{
				LOG(kGraphics, kWarning, "Buffer {} ordinary RAM recovery result={}", name, static_cast<int32_t>(vkMemoryCreateResult));
			}
		}
	}
	// TEMP START memory allocation observation
	LOG(kTemp, kInfo, "MEMORY CREATE_END thread={} site=Buffer.cpp destination={} result={}", iMemoryThread, static_cast<const void*>(&rVkBuffer), static_cast<int32_t>(vkMemoryCreateResult));
	if (vkMemoryCreateResult == VK_SUCCESS)
	{
		LOG(kTemp, kInfo, "MEMORY CREATE_ALLOCATION thread={} destination={} resource={} allocation={} type={} memory={} offset={} size={}", iMemoryThread, static_cast<const void*>(&rVkBuffer), reinterpret_cast<uint64_t>(vkBuffer), reinterpret_cast<uint64_t>(vmaAllocation), vmaAllocationInfo.memoryType, reinterpret_cast<uint64_t>(vmaAllocationInfo.deviceMemory), vmaAllocationInfo.offset, vmaAllocationInfo.size);
	}
	// TEMP END memory allocation observation
	CHECK_VK(vkMemoryCreateResult);
	rVkBuffer = vkBuffer;
	rVmaAllocation = vmaAllocation;
	VkName(VK_OBJECT_TYPE_BUFFER, rVkBuffer, name.data());

	// VMA_ALLOCATION_CREATE_MAPPED_BIT populates pMappedData.
	if (pVmaAllocationInfo != nullptr)
	{
		*pVmaAllocationInfo = vmaAllocationInfo;
	}
}

std::unique_ptr<StagingBuffer> Buffer::RecordUploadToDeviceLocal(VkCommandBuffer vkCommandBuffer, VkBuffer vkDeviceLocalBuffer, std::span<const DeviceLocalBufferUpload> uploads)
{
	ASSERT(vkDeviceLocalBuffer != VK_NULL_HANDLE);
	ASSERT(!uploads.empty());

	int64_t iStagingSize = 0;
	for (const DeviceLocalBufferUpload& rUpload : uploads)
	{
		ASSERT(rUpload.pData != nullptr);
		ASSERT(rUpload.iSize > 0);
		iStagingSize += rUpload.iSize;
	}

	std::unique_ptr<StagingBuffer> pStagingBuffer = std::make_unique<StagingBuffer>("BufferUpload", iStagingSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	common::ScopedWorkbufferArena scopedWorkbufferArena = common::gpThreadLocal->mWorkbuffer.Push();
	auto copiesAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<VkBufferCopy*>(uploads.size() * sizeof(VkBufferCopy));
	int64_t iStagingOffset = 0;
	for (int64_t i = 0; i < std::ssize(uploads); ++i)
	{
		const DeviceLocalBufferUpload& rUpload = uploads[static_cast<size_t>(i)];
		std::memcpy(static_cast<std::byte*>(pStagingBuffer->vmaAllocationInfo.pMappedData) + iStagingOffset, rUpload.pData, static_cast<size_t>(rUpload.iSize));
		copiesAllocation.mpData[i] =
		{
			.srcOffset = static_cast<VkDeviceSize>(iStagingOffset),
			.dstOffset = static_cast<VkDeviceSize>(rUpload.iDestinationOffset),
			.size = static_cast<VkDeviceSize>(rUpload.iSize),
		};
		iStagingOffset += rUpload.iSize;
	}

	vkCmdCopyBuffer(vkCommandBuffer, pStagingBuffer->vkStagingBuffer, vkDeviceLocalBuffer, static_cast<uint32_t>(uploads.size()), copiesAllocation.mpData);

	VkBufferMemoryBarrier vkBufferMemoryBarrier
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		.pNext = nullptr,
		.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.buffer = vkDeviceLocalBuffer,
		.offset = 0,
		.size = VK_WHOLE_SIZE,
	};
	vkCmdPipelineBarrier(vkCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &vkBufferMemoryBarrier, 0, nullptr);

	return pStagingBuffer;
}

StagingBuffer::StagingBuffer(std::string_view name, int64_t iDeviceSize, VkBufferUsageFlags vkBufferUsageFlags)
{
	Buffer::CreateBuffer(name, iDeviceSize, vkBufferUsageFlags, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vkStagingBuffer, vmaAllocation, &vmaAllocationInfo, BufferMemoryPlacement::kOrdinaryRam);
}

StagingBuffer::~StagingBuffer()
{
	vmaDestroyBuffer(gpDeviceManager->mpAllocator, vkStagingBuffer, vmaAllocation);
}

void Buffer::RecordBarriers(VkCommandBuffer vkCommandBuffer, std::span<const BarrierInfo> barriers)
{
	common::ScopedWorkbufferArena scopedWorkbufferArena = common::gpThreadLocal->mWorkbuffer.Push();
	VkPipelineStageFlags vkCombinedDestinationStage = 0;

	for (const BarrierInfo& rBarrier : barriers)
	{
		VkAccessFlags vkDestinationAccessMask = VK_ACCESS_NONE_KHR;
		VkPipelineStageFlags vkDestinationStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		switch (rBarrier.eDestination)
		{
			case BufferBarrier::kComputeRead:
				vkDestinationAccessMask = VK_ACCESS_SHADER_READ_BIT;
				vkDestinationStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
				break;

			case BufferBarrier::kStorageBufferRead:
				vkDestinationAccessMask = VK_ACCESS_SHADER_READ_BIT;
				vkDestinationStageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				break;

			case BufferBarrier::kShaderIndirectRead:
				vkDestinationAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
				vkDestinationStageMask = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
				break;
		}

		vkCombinedDestinationStage |= vkDestinationStageMask;

		common::gpThreadLocal->mWorkbuffer.PushBack<VkBufferMemoryBarrier>(
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			.dstAccessMask = vkDestinationAccessMask,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.buffer = rBarrier.vkBarrierBuffer,
			.offset = 0,
			.size = VK_WHOLE_SIZE,
		});
	}

	auto bufferBarriers = common::gpThreadLocal->mWorkbuffer.Span<VkBufferMemoryBarrier>();
	vkCmdPipelineBarrier(vkCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, vkCombinedDestinationStage, 0, 0, nullptr, static_cast<uint32_t>(bufferBarriers.size()), bufferBarriers.data(), 0, nullptr);
}

Buffer::Buffer(const BufferInfo& rInfo, const std::function<void(void*)>& rDataFunction)
{
	Create(rInfo, rDataFunction);
}

Buffer::Buffer(Buffer&& rOther) noexcept
	: mInfo(std::exchange(rOther.mInfo, {}))
	, mHostVisibleVkBuffer(std::exchange(rOther.mHostVisibleVkBuffer, VK_NULL_HANDLE))
	, mHostVisibleVmaAllocation(std::exchange(rOther.mHostVisibleVmaAllocation, VK_NULL_HANDLE))
	, mpMappedMemory(std::exchange(rOther.mpMappedMemory, nullptr))
	, mDeviceLocalVkBuffer(std::exchange(rOther.mDeviceLocalVkBuffer, VK_NULL_HANDLE))
	, mDeviceLocalVmaAllocation(std::exchange(rOther.mDeviceLocalVmaAllocation, VK_NULL_HANDLE))
{
}

Buffer& Buffer::operator=(Buffer&& rOther) noexcept
{
	if (this != &rOther)
	{
		Destroy();

		mInfo = std::exchange(rOther.mInfo, {});
		mHostVisibleVkBuffer = std::exchange(rOther.mHostVisibleVkBuffer, VK_NULL_HANDLE);
		mHostVisibleVmaAllocation = std::exchange(rOther.mHostVisibleVmaAllocation, VK_NULL_HANDLE);
		mpMappedMemory = std::exchange(rOther.mpMappedMemory, nullptr);
		mDeviceLocalVkBuffer = std::exchange(rOther.mDeviceLocalVkBuffer, VK_NULL_HANDLE);
		mDeviceLocalVmaAllocation = std::exchange(rOther.mDeviceLocalVmaAllocation, VK_NULL_HANDLE);
	}

	return *this;
}

Buffer::~Buffer()
{
	Destroy();
}

void Buffer::Create(const BufferInfo& rInfo, const std::function<void(void*)>& rDataFunction)
{
	Destroy();

	mInfo = rInfo;
	VkBufferUsageFlags vkAdditionalUsage = 0;
	if (mInfo.flags & kDeviceAddress || mInfo.flags & kAccelerationInput || mInfo.flags & kAccelerationStorage)
	{
		vkAdditionalUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	}
	if (mInfo.flags & kAccelerationInput)
	{
		vkAdditionalUsage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	}
	if (mInfo.flags & kAccelerationStorage)
	{
		vkAdditionalUsage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
	}
	ASSERT(mInfo.name.size() > 0);
	if (!(mInfo.flags & kUniform || mInfo.flags & kStorage))
	{
		ASSERT(mInfo.iVertexStride != 0);
	}

	if (mInfo.flags & kUniform || mInfo.flags & kStorage)
	{
		VkBufferUsageFlags vkBufferUsageFlagBits = (mInfo.flags & kUniform ? VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT : VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) | vkAdditionalUsage;
		if (mInfo.flags & kHostVisible || mInfo.flags & kCopyToDeviceLocalEveryFrame)
		{
			VmaAllocationInfo vmaAllocationInfo {};
			if (mInfo.flags & kHostVisible)
			{
				Buffer::CreateBuffer(mInfo.name, mInfo.iDataSize, vkBufferUsageFlagBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, mHostVisibleVkBuffer, mHostVisibleVmaAllocation, &vmaAllocationInfo, mInfo.eMemoryPlacement);
			}
			else
			{
				Buffer::CreateBuffer(mInfo.name, mInfo.iDataSize, vkBufferUsageFlagBits | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, mHostVisibleVkBuffer, mHostVisibleVmaAllocation, &vmaAllocationInfo, BufferMemoryPlacement::kOrdinaryRam);
			}

			// Use VMA's pre-mapped pointer (VMA_ALLOCATION_CREATE_MAPPED_BIT auto-maps the memory)
			mpMappedMemory = static_cast<char*>(vmaAllocationInfo.pMappedData);

			if (rDataFunction != nullptr)
			{
				rDataFunction(mpMappedMemory);
			}
		}

		if (mInfo.flags & kDeviceLocal || mInfo.flags & kCopyToDeviceLocalEveryFrame)
		{
			Buffer::CreateBuffer(mInfo.name, mInfo.iDataSize, vkBufferUsageFlagBits | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mDeviceLocalVkBuffer, mDeviceLocalVmaAllocation, nullptr, mInfo.eMemoryPlacement);
		}
	}
	else
	{
		ASSERT(mInfo.flags & kIndexVertex);
		Buffer::CreateBuffer(mInfo.name, mInfo.iDataSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | vkAdditionalUsage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mDeviceLocalVkBuffer, mDeviceLocalVmaAllocation, nullptr, mInfo.eMemoryPlacement);
	}

	if (mInfo.flags & kDeviceLocal && rDataFunction != nullptr)
	{
		StagingBuffer stagingBuffer(mInfo.name, mInfo.iDataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

		rDataFunction(stagingBuffer.vmaAllocationInfo.pMappedData);

		OneShotCommandBuffer oneShotCommandBuffer;
		VkBufferCopy vkBufferCopy
		{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = static_cast<VkDeviceSize>(mInfo.iDataSize),
		};
		vkCmdCopyBuffer(oneShotCommandBuffer.mVkCommandBuffer, stagingBuffer.vkStagingBuffer, mDeviceLocalVkBuffer, 1, &vkBufferCopy);
		oneShotCommandBuffer.Execute();
	}
}

void Buffer::Destroy() noexcept
{
	if (mHostVisibleVkBuffer != VK_NULL_HANDLE)
	{
		mpMappedMemory = nullptr;
		vmaDestroyBuffer(gpDeviceManager->mpAllocator, mHostVisibleVkBuffer, mHostVisibleVmaAllocation);
		mHostVisibleVkBuffer = VK_NULL_HANDLE;
		mHostVisibleVmaAllocation = VK_NULL_HANDLE;
	}

	if (mDeviceLocalVkBuffer != VK_NULL_HANDLE)
	{
		vmaDestroyBuffer(gpDeviceManager->mpAllocator, mDeviceLocalVkBuffer, mDeviceLocalVmaAllocation);
		mDeviceLocalVkBuffer = VK_NULL_HANDLE;
		mDeviceLocalVmaAllocation = VK_NULL_HANDLE;
	}
}

VkBuffer Buffer::GetBuffer() const
{
	return mInfo.flags & kHostVisible ? mHostVisibleVkBuffer : mDeviceLocalVkBuffer;
}

VkDeviceAddress Buffer::GetDeviceAddress() const
{
	VkBufferDeviceAddressInfo vkInfo
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.buffer = GetBuffer(),
	};
	return vkGetBufferDeviceAddress(gpDeviceManager->mVkDevice, &vkInfo);
}

void Buffer::RecordBindVertexBuffer(VkCommandBuffer vkCommandBuffer)
{
	ASSERT(!(mInfo.flags & kUniform));

	vkCmdBindIndexBuffer(vkCommandBuffer, mDeviceLocalVkBuffer, 0, mInfo.vkIndexType);
	int64_t iIndexSize = mInfo.vkIndexType == VK_INDEX_TYPE_UINT16 ? sizeof(uint16_t) : sizeof(uint32_t);
	int64_t iVerticesOffset = common::ModelHeader::VerticesOffset(mInfo.iCount, iIndexSize);
	VkDeviceSize vkVerticesOffset = static_cast<VkDeviceSize>(iVerticesOffset);
	vkCmdBindVertexBuffers(vkCommandBuffer, 0, 1, &mDeviceLocalVkBuffer, &vkVerticesOffset);
}

void Buffer::RecordCopy(VkCommandBuffer vkCommandBuffer)
{
	ASSERT((mInfo.flags & kUniform || mInfo.flags & kStorage) && mInfo.flags & kCopyToDeviceLocalEveryFrame);

	static constexpr VkPipelineStageFlags kVkShaderStageFlags = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

	// Pre-copy barrier: Wait for shader reads to complete before transfer write
	VkBufferMemoryBarrier vkBufferMemoryBarrier
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		.pNext = nullptr,
		.srcAccessMask = VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
		.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.buffer = mDeviceLocalVkBuffer,
		.offset = 0,
		.size = static_cast<VkDeviceSize>(mInfo.iDataSize),
	};
	vkCmdPipelineBarrier(vkCommandBuffer, kVkShaderStageFlags, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &vkBufferMemoryBarrier, 0, nullptr);

	VkBufferCopy vkBufferCopy
	{
		.srcOffset = 0,
		.dstOffset = 0,
		.size = static_cast<VkDeviceSize>(mInfo.iDataSize),
	};
	vkCmdCopyBuffer(vkCommandBuffer, mHostVisibleVkBuffer, mDeviceLocalVkBuffer, 1, &vkBufferCopy);

	// Post-copy barrier: Transfer write complete before shader reads
	vkBufferMemoryBarrier =
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
		.pNext = nullptr,
		.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.buffer = mDeviceLocalVkBuffer,
		.offset = 0,
		.size = static_cast<VkDeviceSize>(mInfo.iDataSize),
	};
	vkCmdPipelineBarrier(vkCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, kVkShaderStageFlags, 0, 0, nullptr, 1, &vkBufferMemoryBarrier, 0, nullptr);
}

} // namespace engine

#endif // defined(BT_CLIENT)
