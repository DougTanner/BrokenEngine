#if defined(BT_CLIENT)

#include "LightingAccelerationStructure.h"

namespace engine
{

LightingAccelerationStructure::LightingAccelerationStructure(LightingAccelerationStructure&& rOther) noexcept
	: mVkAccelerationStructureKHR(std::exchange(rOther.mVkAccelerationStructureKHR, VK_NULL_HANDLE))
	, mStorage(std::move(rOther.mStorage))
	, mScratch(std::move(rOther.mScratch))
	, mScratchSizeVkDeviceSize(rOther.mScratchSizeVkDeviceSize)
	, mVkAccelerationStructureTypeKHR(rOther.mVkAccelerationStructureTypeKHR)
	, mVkBuildAccelerationStructureFlagsKHR(rOther.mVkBuildAccelerationStructureFlagsKHR)
{
}

LightingAccelerationStructure::~LightingAccelerationStructure()
{
	Destroy();
}

void LightingAccelerationStructure::Create(std::string_view name, VkAccelerationStructureTypeKHR vkType, std::span<const VkAccelerationStructureGeometryKHR> geometries, std::span<const uint32_t> primitiveCounts, VkBuildAccelerationStructureFlagsKHR vkFlags)
{
	Destroy();
	mVkAccelerationStructureTypeKHR = vkType;
	mVkBuildAccelerationStructureFlagsKHR = vkFlags;
	VkAccelerationStructureBuildGeometryInfoKHR vkBuildInfo
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
		.type = vkType,
		.flags = vkFlags,
		.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
		.geometryCount = static_cast<uint32_t>(geometries.size()),
		.pGeometries = geometries.data(),
	};
	VkAccelerationStructureBuildSizesInfoKHR vkSizes
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
	};
	vkGetAccelerationStructureBuildSizesKHR(gpDeviceManager->mVkDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &vkBuildInfo, primitiveCounts.data(), &vkSizes);
	mStorage.Create(
	{
		.name = name,
		.flags = {BufferFlags::kStorage, BufferFlags::kDeviceLocal, BufferFlags::kAccelerationStorage},
		.iDataSize = static_cast<int64_t>(vkSizes.accelerationStructureSize),
	});
	mScratchSizeVkDeviceSize = vkSizes.buildScratchSize;
	VkDeviceSize vkAlignment = gpInstanceManager->mVkPhysicalDeviceAccelerationStructurePropertiesKHR.minAccelerationStructureScratchOffsetAlignment;
	mScratch.Create(
	{
		.name = name,
		.flags = {BufferFlags::kStorage, BufferFlags::kDeviceLocal, BufferFlags::kDeviceAddress},
		.iDataSize = static_cast<int64_t>(mScratchSizeVkDeviceSize + vkAlignment),
		.eMemoryPlacement = BufferMemoryPlacement::kOrdinaryRam,
	});
	VkAccelerationStructureCreateInfoKHR vkCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
		.buffer = mStorage.GetBuffer(),
		.size = vkSizes.accelerationStructureSize,
		.type = vkType,
	};
	CHECK_VK(vkCreateAccelerationStructureKHR(gpDeviceManager->mVkDevice, &vkCreateInfo, nullptr, &mVkAccelerationStructureKHR));
	VkName(VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR, mVkAccelerationStructureKHR, name.data());
}

void LightingAccelerationStructure::Destroy() noexcept
{
	if (mVkAccelerationStructureKHR != VK_NULL_HANDLE)
	{
		vkDestroyAccelerationStructureKHR(gpDeviceManager->mVkDevice, mVkAccelerationStructureKHR, nullptr);
		mVkAccelerationStructureKHR = VK_NULL_HANDLE;
	}
	mScratch.Destroy();
	mStorage.Destroy();
}

void LightingAccelerationStructure::RecordBuild(VkCommandBuffer vkCommandBuffer, std::span<const VkAccelerationStructureGeometryKHR> geometries, std::span<const VkAccelerationStructureBuildRangeInfoKHR> ranges)
{
	VkDeviceAddress vkAlignment = gpInstanceManager->mVkPhysicalDeviceAccelerationStructurePropertiesKHR.minAccelerationStructureScratchOffsetAlignment;
	VkDeviceAddress vkScratchAddress = mScratch.GetDeviceAddress();
	vkScratchAddress = (vkScratchAddress + vkAlignment - 1) & ~(vkAlignment - 1);
	VkAccelerationStructureBuildGeometryInfoKHR vkBuildInfo
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
		.type = mVkAccelerationStructureTypeKHR,
		.flags = mVkBuildAccelerationStructureFlagsKHR,
		.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
		.dstAccelerationStructure = mVkAccelerationStructureKHR,
		.geometryCount = static_cast<uint32_t>(geometries.size()),
		.pGeometries = geometries.data(),
		.scratchData = {.deviceAddress = vkScratchAddress},
	};
	const VkAccelerationStructureBuildRangeInfoKHR* pVkRanges = ranges.data();
	vkCmdBuildAccelerationStructuresKHR(vkCommandBuffer, 1, &vkBuildInfo, &pVkRanges);
}

VkDeviceAddress LightingAccelerationStructure::GetDeviceAddress() const
{
	VkAccelerationStructureDeviceAddressInfoKHR vkInfo
	{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
		.accelerationStructure = mVkAccelerationStructureKHR,
	};
	return vkGetAccelerationStructureDeviceAddressKHR(gpDeviceManager->mVkDevice, &vkInfo);
}

void LightingAccelerationStructure::RecordInputBarrier(VkCommandBuffer vkCommandBuffer)
{
	VkMemoryBarrier vkBarrier
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
	};
	vkCmdPipelineBarrier(vkCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &vkBarrier, 0, nullptr, 0, nullptr);
}

void LightingAccelerationStructure::RecordBuildBarrier(VkCommandBuffer vkCommandBuffer)
{
	VkMemoryBarrier vkBarrier
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
		.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
	};
	vkCmdPipelineBarrier(vkCommandBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &vkBarrier, 0, nullptr, 0, nullptr);
}

void LightingAccelerationStructure::RecordQueryBarrier(VkCommandBuffer vkCommandBuffer)
{
	VkMemoryBarrier vkBarrier
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
		.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
	};
	vkCmdPipelineBarrier(vkCommandBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &vkBarrier, 0, nullptr, 0, nullptr);
}

} // namespace engine

#endif // defined(BT_CLIENT)
