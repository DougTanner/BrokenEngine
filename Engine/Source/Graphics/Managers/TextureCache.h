#pragma once

#if defined(BT_CLIENT)

namespace engine
{

struct TextureFileCacheHeader
{
	static constexpr int64_t kiVersion = 3;

	VkFormat vkFormat = VK_FORMAT_UNDEFINED;
	int64_t iWidth = 0;
	int64_t iHeight = 0;
	int64_t iMipmapLevels = 0;
	int64_t iArrayLayers = 0;
};

class TextureCache
{
public:

	void GeneratePhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionLookupTable();

	bool TryLoadCachedTexture(const std::filesystem::path& rCachePath, const TextureInfo& rTextureInfo, Texture& rTexture);
	void SaveTextureToCache(const std::filesystem::path& rCachePath, const Texture& rTexture);

	// vkCurrentLayout is the source image's steady-state layout, used verbatim as the pre-copy barrier oldLayout and
	// the post-copy restore newLayout (swapchain: VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; render targets: their parked layout
	// via ToVkImageLayout). bFromSwapchain still selects the barrier stage/access masks.
	static void CopyImageToHostMemory(VkImage vkSourceImage, VkExtent3D vkExtent, VkFormat vkFormat, int64_t iMipmapLevels, int64_t iArrayLayers, bool bFromSwapchain, VkImageLayout vkCurrentLayout, std::vector<std::byte>& rOutputData);

	int64_t miPhysicallyBasedRenderingCubeMipmapCount = 0;
	Texture mPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionLookupTableTexture;
};

} // namespace engine

#endif // defined(BT_CLIENT)
