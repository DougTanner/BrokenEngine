#pragma once

#if defined(BT_CLIENT)

#include "Frame/GridCoord.h"

namespace engine
{

std::error_code VkErrorCode(VkResult vkResult) noexcept;
void CheckVkFailed(VkResult vkResult, std::string_view expression, std::source_location location);

void SetVkObjectName(VkObjectType vkObjectType, uint64_t uiHandle, std::string_view name);

template <typename T>
inline void VkName([[maybe_unused]] VkObjectType vkObjectType, [[maybe_unused]] T handle, [[maybe_unused]] std::string_view name)
{
	if constexpr (kbVulkanDebugLayers)
	{
		SetVkObjectName(vkObjectType, reinterpret_cast<uint64_t>(handle), name);
	}
}

inline void CheckVk(VkResult vkResult, std::string_view expression, std::source_location location = std::source_location::current())
{
	if (vkResult != VK_SUCCESS) [[unlikely]]
	{
		CheckVkFailed(vkResult, expression, location);
	}
}

inline constexpr uint32_t TileCount(uint32_t uiCount)
{
	return (uiCount + shaders::kiComputeTileSize - 1) / shaders::kiComputeTileSize;
}

// Shared rendering helpers for collections. Each renderer rebases its cell-local positions once, at its single
// position-to-GPU point, so only ProjectToBaseHeight takes a basis: it reaches the cross-cell GlobalElevation
// query, which needs the cell identity as well as the offset.
// vecPosition is already in the camera cell's frame.
bool IsPointVisible(XMVECTOR vecPosition, XMFLOAT4A& rf4OutPosition);
// vecLocalPosition is local to rBasis.coordinate; the returned position is in the camera cell's frame.
XMVECTOR ProjectToBaseHeight(XMVECTOR vecLocalPosition, const RenderBasis& rBasis);
// rf4Position is ProjectToBaseHeight's output, so it is already in the camera cell's frame.
void BuildAxisAlignedQuad(shaders::AxisAlignedQuadLayout& rLayout, const XMFLOAT4A& rf4Position, float fArea, const XMFLOAT4A& rf4Parameters, uint32_t uiColor);
float MinLightingDepositSize();

bool SupportsStorageImage(VkFormat vkFormat);
bool SupportsColorAttachmentBlend(VkFormat vkFormat);

} // namespace engine

#define CHECK_VK(a) \
do \
{ \
	VkResult vkResultMacro = a; \
	if (vkResultMacro != VK_SUCCESS) [[unlikely]] \
	{ \
		CheckVk(vkResultMacro, #a); \
	} \
	_Analysis_assume_(vkResultMacro == VK_SUCCESS); \
} while (false)

#endif // defined(BT_CLIENT)
