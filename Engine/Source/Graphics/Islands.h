#pragma once

#if defined(BT_CLIENT)

#include "Frame/IslandChainPlacement.h"

namespace engine
{

struct Cell;
struct GridCoord;

// Global SSBO arena capacity: every subscribed coordinate slot plus the local unconfirmed cell can contribute
// at most kiMaximumIslandsPerCell placements. Per-template runs are packed contiguously into this shared arena,
// with mesh-visible placements first. Resident allocation is kiMaxFramebuffers × kiMaxActivePlacements ×
// sizeof(AxisAlignedQuadLayout): with kiCoordinateSlots = 16 and kiMaximumIslandsPerCell = 107 this is 436,560 bytes
// (426.33 KiB), independent of the number of island templates.
inline constexpr int64_t kiMaxActivePlacements = (game::NetworkSessionContract::kiCoordinateSlots + 1) * kiMaximumIslandsPerCell;
inline constexpr int64_t kiIslandMeshArenaBytes = 64i64 * 1'024i64 * 1'024i64;

// SSBO and indirect buffers have one instance per framebuffer index, sized by kiMaxFramebuffers. UpdateActiveIslands runs before
// RenderGlobal and writes only the re-acquired instance: its prior frame has presented and completed its GPU read, while this frame has not
// submitted. All instances are allocated at boot and indexed by gpSwapchainManager->miFramebufferIndex, so they survive swapchain
// recreation.

class Islands
{
public:

	Islands();
	~Islands();

	bool AllocateMeshRanges(int64_t iIndexSize, int64_t iVertexSize, VmaVirtualAllocation& rIndexAllocation, int64_t& riIndexOffset, VmaVirtualAllocation& rVertexAllocation, int64_t& riVertexOffset);
	void FreeMeshRanges(VmaVirtualAllocation vmaIndexAllocation, VmaVirtualAllocation vmaVertexAllocation);
	void UploadMesh(int64_t iIndexOffset, std::span<const std::byte> indexData, int64_t iVertexOffset, std::span<const std::byte> vertexData);
	void WriteMeshIndirect(int64_t iTemplate, int64_t iIndexOffset, int64_t iVertexOffset, int64_t iIndexCount);

	void UpdateActiveIslands(const std::unordered_map<GridCoord, Cell>& rCells, std::span<const GridCoord> activeCoordinates);

	Buffer mIslandMeshArena;
	int64_t miMeshArenaCapacityGeneration = 0;

	// One SSBO per framebuffer index. Placements occupy contiguous per-template runs packed into the shared
	// arena by UpdateActiveIslands; each run starts at that template's per-frame firstInstance, with its
	// mesh-visible prefix before the offscreen remainder. Inactive slots stay zero-width so the vertex shader
	// emits degenerate triangles (GPU-culled).
	// Bound via kPerCommandBufferStorageBuffers so an
	// in-flight frame's GPU read never races the host rewrite of the instance the current frame consumes.
	std::array<Buffer, kiMaxFramebuffers> mIslandsStorageBuffers;

	// One per-template VkDrawIndexedIndirectCommand buffer per framebuffer index. Mesh residency rewrites
	// indexCount / firstIndex / vertexOffset in every instance; firstInstance and instanceCount are rewritten
	// per frame by UpdateActiveIslands in the acquired framebuffer's instance only.
	// Allocated manually (Buffer wrapper has no INDIRECT_BUFFER_BIT path); mirrors Pipeline's
	// mIndirectVkBuffer pattern in SetupIndirectBuffer (Engine/Source/Graphics/Objects/PipelineCreator.cpp).
	std::array<VkBuffer, kiMaxFramebuffers> mIslandsIndirectVkBuffers {};
	std::array<VmaAllocation, kiMaxFramebuffers> mIslandsIndirectVmaAllocations {};
	std::array<VkDrawIndexedIndirectCommand*, kiMaxFramebuffers> mppIslandsIndirectMappedVkDrawIndexedIndirectCommands {};

	int64_t miTemplateCount = 0;  // Cached gpIslandTerrain->mIslandCrcsSorted.size() (fixed at boot).

	// Per-framebuffer total placement count written the last time that framebuffer index was populated. The
	// current run always starts at arena slot 0, so UpdateActiveIslands clears only the tail from the current
	// total through this previous total after rewriting, instead of clearing the whole reserved arena every
	// frame. Values are zero-initialized to match the ctor's baseline full memset.
	std::array<int64_t, kiMaxFramebuffers> mLastWrittenCounts {};

private:

	VmaVirtualBlock mIslandMeshVirtualBlock = VK_NULL_HANDLE;
};

inline Islands* gpIslands = nullptr;

} // namespace engine

#endif // BT_CLIENT
