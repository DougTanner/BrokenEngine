#pragma once

#if defined(BT_CLIENT)

namespace engine
{

enum class BufferFlags : uint64_t
{
	kIndexVertex                 = 0x01,
	kUniform                     = 0x02,
	kStorage                     = 0x04,

	kDeviceLocal                 = 0x08,
	kHostVisible                 = 0x10,

	kCopyToDeviceLocalEveryFrame = 0x20,
	kDeviceAddress              = 0x40,
	kAccelerationInput          = 0x80,
	kAccelerationStorage        = 0x100,
};
using BufferFlags_t = common::Flags<BufferFlags>;

enum class BufferBarrier
{
	kComputeRead,
	kStorageBufferRead,
	kShaderIndirectRead,
};

struct BarrierInfo
{
	BufferBarrier eDestination = BufferBarrier::kComputeRead;
	VkBuffer vkBarrierBuffer = VK_NULL_HANDLE;
};

enum class BufferMemoryPlacement
{
	kDefault,
	kOrdinaryRam,
};

struct BufferInfo
{
	std::string_view name;
	BufferFlags_t flags;

	int64_t iCount = 0;
	VkIndexType vkIndexType = VK_INDEX_TYPE_NONE_KHR;
	int64_t iVertexStride = 0;
	int64_t iDataSize = 0;
	int64_t iElementSize = 0;
	BufferMemoryPlacement eMemoryPlacement = BufferMemoryPlacement::kDefault;
};

struct StagingBuffer;

struct DeviceLocalBufferUpload
{
	const void* pData = nullptr;
	int64_t iDestinationOffset = 0;
	int64_t iSize = 0;
};

class Buffer
{
public:

	static void CreateBuffer(std::string_view name, int64_t iDeviceSize, VkBufferUsageFlags vkBufferUsageFlags, VkMemoryPropertyFlags vkMemoryPropertyFlags, VkBuffer& rVkBuffer, VmaAllocation& rVmaAllocation, VmaAllocationInfo* pVmaAllocationInfo = nullptr, BufferMemoryPlacement eMemoryPlacement = BufferMemoryPlacement::kDefault);
	// Records the staged copies plus a transfer-to-vertex-input barrier (index/vertex buffer destinations) into vkCommandBuffer.
	// The caller keeps the returned staging buffer alive until that command buffer's execution completes.
	static std::unique_ptr<StagingBuffer> RecordUploadToDeviceLocal(VkCommandBuffer vkCommandBuffer, VkBuffer vkDeviceLocalBuffer, std::span<const DeviceLocalBufferUpload> uploads);
	static void RecordBarriers(VkCommandBuffer vkCommandBuffer, std::span<const BarrierInfo> barriers);

	Buffer() = default;
	Buffer(const Buffer&) = delete;
	Buffer& operator=(const Buffer&) = delete;
	Buffer(Buffer&& rOther) noexcept;
	Buffer& operator=(Buffer&& rOther) noexcept;
	Buffer(const BufferInfo& rInfo, const std::function<void(void*)>& rDataFunction = nullptr);
	~Buffer();

	void Create(const BufferInfo& rInfo, const std::function<void(void*)>& rDataFunction = nullptr);
	void Destroy() noexcept;

	VkBuffer GetBuffer() const;
	VkDeviceAddress GetDeviceAddress() const;

	void RecordBindVertexBuffer(VkCommandBuffer vkCommandBuffer);
	void RecordCopy(VkCommandBuffer vkCommandBuffer);

	BufferInfo mInfo {};

	VkBuffer mHostVisibleVkBuffer = VK_NULL_HANDLE;
	VmaAllocation mHostVisibleVmaAllocation = VK_NULL_HANDLE;
	char* mpMappedMemory = nullptr;

	VkBuffer mDeviceLocalVkBuffer = VK_NULL_HANDLE;
	VmaAllocation mDeviceLocalVmaAllocation = VK_NULL_HANDLE;
};

struct StagingBuffer
{
	StagingBuffer(std::string_view name, int64_t iDeviceSize, VkBufferUsageFlags vkBufferUsageFlags);
	StagingBuffer(const StagingBuffer&) = delete;
	StagingBuffer& operator=(const StagingBuffer&) = delete;
	~StagingBuffer();

	VkBuffer vkStagingBuffer = VK_NULL_HANDLE;
	VmaAllocation vmaAllocation = VK_NULL_HANDLE;
	VmaAllocationInfo vmaAllocationInfo {};
};

} // namespace engine

#endif // defined(BT_CLIENT)
