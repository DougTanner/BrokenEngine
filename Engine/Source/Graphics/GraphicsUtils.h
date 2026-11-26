#pragma once

#if defined(BT_CLIENT)

#include "Frame/GridCoord.h"

namespace engine
{

std::error_code VkErrorCode(VkResult vkResult) noexcept;
void CheckVkFailed(VkResult vkResult, std::string_view expression, std::source_location location);

void SetVkObjectName(VkObjectType vkObjectType, uint64_t uiHandle, const char* pcName);

template <typename T>
inline void VkName([[maybe_unused]] VkObjectType vkObjectType, [[maybe_unused]] T handle, [[maybe_unused]] const char* pcName)
{
	if constexpr (kbVulkanDebugLayers)
	{
		SetVkObjectName(vkObjectType, reinterpret_cast<uint64_t>(handle), pcName);
	}
}

inline void CheckVk(VkResult vkResult, std::string_view expression, std::source_location location = std::source_location::current())
{
	if (vkResult != VK_SUCCESS) [[unlikely]]
	{
		CheckVkFailed(vkResult, expression, location);
	}
}

inline constexpr int64_t TileCount(int64_t iCount)
{
	return (iCount + shaders::kiComputeTileSize - 1) / shaders::kiComputeTileSize;
}

// IsPointVisible takes camera-cell coordinates. ProjectToBaseHeight needs the emitter's cell identity for cross-cell
// elevation sampling before rebasing.
bool IsPointVisible(XMVECTOR vecPosition, XMFLOAT4A& rf4OutPosition);
// vecLocalPosition is local to rBasis.coordinate; the returned position is in the camera cell's frame, on the eye
// ray at the base height or the terrain below the position, whichever is higher.
XMVECTOR ProjectToBaseHeight(XMVECTOR vecLocalPosition, const RenderBasis& rBasis);
// rf4Position is the deposit position in the camera cell's frame, projected onto the base height or higher terrain.
void BuildAxisAlignedQuad(shaders::AxisAlignedQuadLayout& rLayout, const XMFLOAT4A& rf4Position, float fArea, const XMFLOAT4A& rf4Parameters, uint32_t uiColor);

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
