#if defined(BT_CLIENT)

#include "GraphicsUtils.h"

#include "Graphics/CameraBase.h"

namespace engine
{

std::error_code VkErrorCode(VkResult vkResult) noexcept
{
	class VulkanErrorCategory final : public std::error_category
	{
	public:

		const char* name() const noexcept override
		{
			return "Vulkan";
		}

		std::string message(int iValue) const override
		{
			return string_VkResult(static_cast<VkResult>(iValue));
		}
	};

	static const VulkanErrorCategory sCategory;
	return std::error_code(static_cast<int>(vkResult), sCategory);
}

void CheckVkFailed(VkResult vkResult, std::string_view expression, std::source_location location)
{
	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	const char* pcResult = string_VkResult(vkResult);
	LOG(kDefault, kError, "CheckVk failed: {} - \"{}\" at {}:{} in {}", pcResult, expression, location.file_name(), location.line(), location.function_name());

	auto exceptionAllocation = rWorkbuffer.PushBuffer<char*>(1'024);
	std::snprintf(exceptionAllocation.mpData, 1'023, "CheckVk failed: \"%.*s\" at %s:%u in %s\nVkResult: %s", static_cast<int>(expression.size()), expression.data(), location.file_name(), location.line(), location.function_name(), pcResult);

	if (vkResult == VK_ERROR_OUT_OF_DATE_KHR || vkResult == VK_SUBOPTIMAL_KHR)
	{
		gpGraphics->meDestroyType = std::max(DestroyType::kSwapchain, gpGraphics->meDestroyType);
		return;
	}

	if (vkResult == VK_ERROR_DEVICE_LOST || vkResult == VK_ERROR_SURFACE_LOST_KHR)
	{
		// Heap: std::system_error composes its message in std::string.
		ScopedSuppressAllocationTracking suppress;
		throw std::system_error(VkErrorCode(vkResult), exceptionAllocation.mpData);
	}

	DEBUG_BREAK();
	throw std::runtime_error(exceptionAllocation.mpData);
}

void SetVkObjectName(VkObjectType vkObjectType, uint64_t uiHandle, const char* pcName)
{
	if (vkSetDebugUtilsObjectNameEXT != nullptr)
	{
		VkDebugUtilsObjectNameInfoEXT vkDebugUtilsObjectNameInfoEXT =
		{
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			.pNext = nullptr,
			.objectType = vkObjectType,
			.objectHandle = uiHandle,
			.pObjectName = pcName,
		};
		vkSetDebugUtilsObjectNameEXT(gpDeviceManager->mVkDevice, &vkDebugUtilsObjectNameInfoEXT);
	}
}

bool IsPointVisible(XMVECTOR vecPosition, XMFLOAT4A& rf4OutPosition)
{
	XMStoreFloat4A(&rf4OutPosition, vecPosition);
	return rf4OutPosition.x >= engine::gpCamera->mf4RenderVisibleArea.x && rf4OutPosition.x <= engine::gpCamera->mf4RenderVisibleArea.z && rf4OutPosition.y <= engine::gpCamera->mf4RenderVisibleArea.y && rf4OutPosition.y >= engine::gpCamera->mf4RenderVisibleArea.w;
}

SmokePlaneWeights ComputeSmokePlaneWeights(float fZ)
{
	if (fZ < gpFlightPlanes->Lowest().fHeightMeters)
	{
		return {};
	}

	for (int64_t i = 0; i + 1 < gpFlightPlanes->Count(); ++i)
	{
		float fLowerHeight = gpFlightPlanes->Plane(i).fHeightMeters;
		float fUpperHeight = gpFlightPlanes->Plane(i + 1).fHeightMeters;
		if (fZ < fUpperHeight)
		{
			return {.iLowerPlane = i, .fUpperWeight = (fZ - fLowerHeight) / (fUpperHeight - fLowerHeight)};
		}
	}
	return {.iLowerPlane = gpFlightPlanes->Count() - 1, .fUpperWeight = 0.0f};
}

void BuildAxisAlignedQuad(shaders::AxisAlignedQuadLayout& rLayout, const XMFLOAT4A& rf4Position, float fArea, const XMFLOAT4A& rf4Parameters, uint32_t uiColor)
{
	rLayout.f4VertexRect = {rf4Position.x - fArea, rf4Position.y + fArea, 2.0f * fArea, -2.0f * fArea};
	rLayout.f4TextureRect = {0.0f, 0.0f, 1.0f, 1.0f};
	rLayout.f4Parameters = rf4Parameters;
	rLayout.fRotation = 0.0f; // non-island consumers render axis-aligned
	rLayout.uiTextureSlot = 0; // non-island consumers don't sample the island texture array
	rLayout.uiColor = uiColor;
}

bool SupportsStorageImage(VkFormat vkFormat)
{
	VkFormatProperties vkFormatProperties {};
	vkGetPhysicalDeviceFormatProperties(gpInstanceManager->mVkPhysicalDevice, vkFormat, &vkFormatProperties);
	return (vkFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) != 0;
}

bool SupportsColorAttachmentBlend(VkFormat vkFormat)
{
	VkFormatProperties vkFormatProperties {};
	vkGetPhysicalDeviceFormatProperties(gpInstanceManager->mVkPhysicalDevice, vkFormat, &vkFormatProperties);
	return (vkFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT) != 0;
}

} // namespace engine

#endif // BT_CLIENT
