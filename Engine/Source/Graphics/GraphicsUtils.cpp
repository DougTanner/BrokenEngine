#if defined(BT_CLIENT)

#include "GraphicsUtils.h"

#include "Graphics/EngineCamera.h"
#include "Ui/LightingWrappersBase.h"
#include "Ui/WrapperBase.h"

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
		// std::max, not plain assign: a same-frame kSurface escalation must never downgrade to kSwapchain.
		gpGraphics->meDestroyType = std::max(DestroyType::kSwapchain, gpGraphics->meDestroyType);
		return;
	}

	if (vkResult == VK_ERROR_SURFACE_LOST_KHR)
	{
		gpGraphics->meDestroyType = DestroyType::kSurface;
		return;
	}

	if (vkResult == VK_ERROR_DEVICE_LOST)
	{
		throw std::system_error(VkErrorCode(vkResult), exceptionAllocation.mpData);
	}

	DEBUG_BREAK();
	throw std::runtime_error(exceptionAllocation.mpData);
}

void SetVkObjectName([[maybe_unused]] VkObjectType vkObjectType, [[maybe_unused]] uint64_t uiHandle, [[maybe_unused]] std::string_view name)
{
	if constexpr (kbVulkanDebugLayers)
	{
		if (vkSetDebugUtilsObjectNameEXT != nullptr)
		{
			common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
			// string_VkObjectType returns "Unhandled VkObjectType" for unrecognized types (no "VK_OBJECT_TYPE_"
			// prefix); strip the prefix only when present, else the fixed skip mis-truncates the fallback into a garbage tail.
			const char* pcTypeName = string_VkObjectType(vkObjectType);
			int64_t iPrefixLength = static_cast<int64_t>(std::char_traits<char>::length("VK_OBJECT_TYPE_"));
			const char* pcPrefix = std::char_traits<char>::compare(pcTypeName, "VK_OBJECT_TYPE_", static_cast<size_t>(iPrefixLength)) == 0 ? pcTypeName + iPrefixLength : pcTypeName;
			common::ScopedWorkbufferArena innerArena = rWorkbuffer.Push();
			rWorkbuffer.Append(pcPrefix);
			rWorkbuffer.Append(" ");
			rWorkbuffer.Append(name);

			// Heap: caching debug names in mDebugNames can allocate string storage and unordered_set nodes.
			ScopedSuppressAllocationTracking suppress;

			auto [it, bInserted] = gpGraphics->mDebugNames.emplace(rWorkbuffer.View());
			VkDebugUtilsObjectNameInfoEXT vkDebugUtilsObjectNameInfoEXT =
			{
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
				.pNext = nullptr,
				.objectType = vkObjectType,
				.objectHandle = uiHandle,
				.pObjectName = it->c_str(),
			};
			vkSetDebugUtilsObjectNameEXT(gpDeviceManager->mVkDevice, &vkDebugUtilsObjectNameInfoEXT);
		}
	}
}

bool IsPointVisible(XMVECTOR vecPosition, XMFLOAT4A& rf4OutPosition)
{
	XMStoreFloat4A(&rf4OutPosition, vecPosition);
	return rf4OutPosition.x >= engine::gpCamera->mf4RenderVisibleArea.x && rf4OutPosition.x <= engine::gpCamera->mf4RenderVisibleArea.z && rf4OutPosition.y <= engine::gpCamera->mf4RenderVisibleArea.y && rf4OutPosition.y >= engine::gpCamera->mf4RenderVisibleArea.w;
}

XMVECTOR ProjectToBaseHeight(XMVECTOR vecLocalPosition, const RenderBasis& rBasis)
{
	// The elevation query is answered in the emitter's own cell, from the coordinate the basis carries; the
	// projection toward the eye is the conversion point, so the rebase happens exactly once here.
	float fElevation = gpIslandTerrain->GlobalElevation(rBasis.coordinate, vecLocalPosition);
	return common::ToBaseHeight(Rebase(rBasis, vecLocalPosition), engine::gpCamera->mVecEyePosition, std::max(fElevation, gBaseHeight.mfCurrent));
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

float MinLightingDepositSize()
{
	// At settled camera heights, eight constant-density lighting texels prevent sub-texel light flicker.
	// Lighting-area and texture headroom factors cancel, so texel world size uses the base DetailTextureSize.
	// Ceil inflates the floor by a sub-texel amount.
	auto [iLightingTextureX, iLightingTextureY] = TextureManager::DetailTextureSize(gLightingDepositTextureMultiplier.mfCurrent);
	float fTexelSizeX = std::ceil(engine::gpCamera->mf4RenderVisibleArea.z - engine::gpCamera->mf4RenderVisibleArea.x) / static_cast<float>(iLightingTextureX);
	float fTexelSizeY = std::ceil(engine::gpCamera->mf4RenderVisibleArea.y - engine::gpCamera->mf4RenderVisibleArea.w) / static_cast<float>(iLightingTextureY);
	return std::max(fTexelSizeX, fTexelSizeY) * 8.0f;
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
