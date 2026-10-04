#pragma once

#if defined(BT_CLIENT)

namespace engine
{

struct TextureFileCacheHeader
{
	static constexpr int64_t kiMagic = 0xCACE'F11E;
	static constexpr int64_t kiVersion = 2;

	int64_t iMagic = 0;
	int64_t iVersion = 0;
	VkFormat vkFormat = VK_FORMAT_UNDEFINED;
	int64_t iWidth = 0;
	int64_t iHeight = 0;
	int64_t iMipmapLevels = 0;
	int64_t iArrayLayers = 0;
	int64_t iDataSize = 0;
	common::crc_t sourceCrc = 0;  // CRC of source texture used to generate this cache
};

class TextureCache
{
public:

	void GeneratePhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionLookupTable();

	bool TryLoadCachedTexture(const std::filesystem::path& rCachePath, Texture& rTexture, common::crc_t sourceCrc = 0);
	void SaveTextureToCache(const std::filesystem::path& rCachePath, const Texture& rTexture, common::crc_t sourceCrc = 0);

	// vkCurrentLayout is the source image's steady-state layout, used verbatim as the pre-copy barrier oldLayout and
	// the post-copy restore newLayout (swapchain: VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; render targets: their parked layout
	// via ToVkImageLayout). bFromSwapchain still selects the barrier stage/access masks.
	static void CopyImageToHostMemory(VkImage vkSourceImage, VkExtent3D vkExtent, VkFormat vkFormat, uint32_t uiMipmapLevels, uint32_t uiArrayLayers, bool bFromSwapchain, VkImageLayout vkCurrentLayout, std::vector<std::byte>& rOutputData);

	int64_t miPhysicallyBasedRenderingCubeMipmapCount = 0;
	Texture mPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionLookupTableTexture;
};

} // namespace engine

#endif // defined(BT_CLIENT)
