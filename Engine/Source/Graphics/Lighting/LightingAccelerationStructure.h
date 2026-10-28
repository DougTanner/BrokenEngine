#pragma once

#if defined(BT_CLIENT)

#include "Graphics/Objects/Buffer.h"

namespace engine
{

class LightingAccelerationStructure
{
public:
	LightingAccelerationStructure() = default;
	LightingAccelerationStructure(const LightingAccelerationStructure&) = delete;
	LightingAccelerationStructure& operator=(const LightingAccelerationStructure&) = delete;
	LightingAccelerationStructure(LightingAccelerationStructure&& rOther) noexcept;
	~LightingAccelerationStructure();

	void Create(std::string_view name, VkAccelerationStructureTypeKHR vkType, std::span<const VkAccelerationStructureGeometryKHR> geometries, std::span<const uint32_t> primitiveCounts, VkBuildAccelerationStructureFlagsKHR vkFlags);
	void Destroy() noexcept;
	void RecordBuild(VkCommandBuffer vkCommandBuffer, std::span<const VkAccelerationStructureGeometryKHR> geometries, std::span<const VkAccelerationStructureBuildRangeInfoKHR> ranges);
	VkDeviceAddress GetDeviceAddress() const;
	static void RecordBuildBarrier(VkCommandBuffer vkCommandBuffer);
	static void RecordInputBarrier(VkCommandBuffer vkCommandBuffer);
	static void RecordQueryBarrier(VkCommandBuffer vkCommandBuffer);

	VkAccelerationStructureKHR mVkAccelerationStructureKHR = VK_NULL_HANDLE;
	Buffer mStorage;
	Buffer mScratch;
	VkDeviceSize mScratchSizeVkDeviceSize = 0;
	VkAccelerationStructureTypeKHR mVkAccelerationStructureTypeKHR = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	VkBuildAccelerationStructureFlagsKHR mVkBuildAccelerationStructureFlagsKHR = 0;
};

} // namespace engine

#endif // defined(BT_CLIENT)
