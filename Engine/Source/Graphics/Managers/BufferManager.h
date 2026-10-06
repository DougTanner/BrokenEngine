#pragma once

#if defined(BT_CLIENT)

namespace engine
{

// Max swapchain framebuffer count. Sizes the per-command-buffer skinning arrays below and Islands'
// triple-buffered SSBO/indirect arrays; guarded by ASSERTs where the live framebuffer count is read.
inline constexpr int64_t kiMaxFramebuffers = 4;

enum DynamicBufferType
{
	kBufferMain,
	kBufferVisibleLights,
	kBufferWindDeposit,

	kBufferTypeCount,
};

class BufferManager
{
public:

	BufferManager();
	~BufferManager();

	void DestroySwapchainDependentBuffers();
	void CreateSwapchainDependentBuffers();

	void CreateWaterMesh();

	Buffer* CreateDynamicBuffer(common::crc_t crc, DynamicBufferType eType, std::string_view name, int64_t iElementSize);
	void ResizeDynamicBuffer(common::crc_t crc, DynamicBufferType eType, std::string_view name, int64_t iNewSize, int64_t iFramebuffer);
	Buffer* ResizeDynamicBufferIfNeeded(common::crc_t crc, DynamicBufferType eType, std::string_view name, int64_t iLayoutSize, int64_t iCapacity, int64_t iCommandBuffer);

	template<typename T>
	struct DynamicStorageBufferResult
	{
		T* pData = nullptr;
		int64_t iCapacity = 0;
	};

	template<typename T>
	DynamicStorageBufferResult<T> GetDynamicStorageBuffer(common::crc_t crc, DynamicBufferType eType, int64_t iCommandBuffer)
	{
		static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
		Buffer& rBuffer = mDynamicStorageBuffers[eType].at(crc).at(iCommandBuffer);
		// CreateDynamicBuffer() stores its size argument as iElementSize; pass sizeof(T) so ResizeDynamicBuffer preserves the element size.
		ASSERT(static_cast<int64_t>(sizeof(T)) == rBuffer.mInfo.iElementSize);
		return
		{
			.pData = reinterpret_cast<T*>(rBuffer.mpMappedMemory),
			.iCapacity = rBuffer.mInfo.iDataSize / rBuffer.mInfo.iElementSize,
		};
	}

	std::unordered_map<common::crc_t, Buffer> mModelMap;

	std::vector<Buffer> mGlobalLayoutUniformBuffers;
	std::vector<Buffer> mMainLayoutUniformBuffers;

	std::vector<Buffer> mUiRectangleStorageBuffers;

	// Smoke hierarchical dispatch buffers (per-texture occupancy, shared active tile list)
	VkBuffer mSmokeOccupancyVkBuffers[2] {};
	VmaAllocation mSmokeOccupancyVmaAllocations[2] {};
	int64_t miSmokeOccupancyBufferSize = 0;
	VkBuffer mSmokeActiveTileVkBuffer = VK_NULL_HANDLE;
	VmaAllocation mSmokeActiveTileVmaAllocation = VK_NULL_HANDLE;
	int64_t miSmokeActiveTileBufferSize = 0;
	void CreateSmokeHierarchicalBuffers();
	void DestroySmokeHierarchicalBuffers();

	// Wind hierarchical dispatch buffers (two pairs: A for TextureOne, B for TextureTwo)
	VkBuffer mWindOccupancyVkBuffers[2] = {};
	VmaAllocation mWindOccupancyVmaAllocations[2] = {};
	int64_t miWindOccupancyBufferSize = 0;
	VkBuffer mWindActiveTileVkBuffers[2] = {};
	VmaAllocation mWindActiveTileVmaAllocations[2] = {};
	int64_t miWindActiveTileBufferSize = 0;
	void CreateWindHierarchicalBuffers();
	void DestroyWindHierarchicalBuffers();

	Buffer mQuadsVertexBuffer;

	// Each water LOD halves each quad dimension, clamped to one; nominal eye-distance boundaries are kfMinimumEyeHeight * 4^k.
	// Camera applies hysteresis and uses the selected LOD for visible-area snapping.
	// All LODs share mWaterMeshBuffer; indirect firstIndex/indexCount/vertexOffset select the LOD without re-recording commands.
	// Terrain draws IslandTemplate's Gaea2 Mesher meshes in CommandBufferRecordMain.
	static constexpr int64_t kiVisibleAreaLodCount = 4;
	struct VisibleAreaMeshLod
	{
		int64_t iIndexOffset = 0;   // First index for this LOD inside the concat index region
		int64_t iIndexCount = 0;
		int64_t iVertexOffset = 0;  // Vertex base added by vkCmdDrawIndexedIndirect's vertexOffset
		int64_t iQuadCountX = 0;    // For visible-area snap math
		int64_t iQuadCountY = 0;
	};
	VisibleAreaMeshLod mWaterMeshLods[kiVisibleAreaLodCount] {};
	Buffer mWaterMeshBuffer;

	Buffer mDebugBoxVertexBuffer;
	Buffer mDebugSphereVertexBuffer;
	Buffer mDebugCircleVertexBuffer;
	Buffer mDebugLineVertexBuffer;

	std::vector<Buffer> mLongParticlesSpawnStorageBuffers;
	Buffer mLongParticlesStorageBuffer;

	std::vector<Buffer> mSquareParticlesSpawnStorageBuffers;
	Buffer mSquareParticlesStorageBuffer;

	std::vector<Buffer> mMeshDataStorageBuffers;
	std::vector<Buffer> mJointMatrixStorageBuffers;

	int64_t AllocateMeshData(int64_t iCommandBuffer, int64_t iCount);
	int64_t AllocateJointMatrices(int64_t iCommandBuffer, int64_t iCount);
	void ResetSkinningAllocations(int64_t iCommandBuffer);

	std::unordered_map<common::crc_t, std::vector<Buffer>> mDynamicStorageBuffers[kBufferTypeCount];
	std::optional<Buffer> mPreviousBuffer;

	void InitializePerCommandBufferBuffers(int64_t iCommandBufferCount);

private:

	void CreateDebugMeshBuffers();

	void GrowMeshDataBuffer(int64_t iCommandBuffer, int64_t iValidCount);
	void GrowJointMatrixBuffer(int64_t iCommandBuffer, int64_t iValidCount);

	int64_t miMeshDataOffset[kiMaxFramebuffers] {};
	int64_t miJointMatrixOffset[kiMaxFramebuffers] {};
	int64_t miMeshDataCapacity[kiMaxFramebuffers] {};
	int64_t miJointMatrixCapacity[kiMaxFramebuffers] {};
	// Skinning buffers remain parked until the next ResetSkinningAllocations call;
	// a second grow in the frame overwrites its framebuffer's slot.
	std::optional<Buffer> mPreviousMeshDataBuffer[kiMaxFramebuffers];
	std::optional<Buffer> mPreviousJointMatrixBuffer[kiMaxFramebuffers];
};

inline BufferManager* gpBufferManager = nullptr;

} // namespace engine

#endif // defined(BT_CLIENT)
