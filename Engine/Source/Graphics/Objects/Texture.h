#pragma once

#if defined(BT_CLIENT)

namespace engine
{

inline constexpr float kfMinDepth = 0.0f;
inline constexpr float kfMaxDepth = 1.0f;

enum class TextureFlags : uint64_t
{
	kRenderPass = 0x02,
};
using TextureFlags_t = common::Flags<TextureFlags>;

enum class RenderPassFlags : uint8_t
{
	kDepth         = 0x01,
	kMultisampling = 0x02,
	kClear         = 0x04,
};
using RenderPassFlags_t = common::Flags<RenderPassFlags>;

enum class TextureLayout
{
	kUndefined,

	kColorAttachment,
	kComputeReadOnly,
	kComputeReadWrite,
	kFragmentShaderReadOnly,
	kGeneral,
	kShaderReadOnly,
	kTransferDestination,
	kTransferSource,
};

// Steady-state VkImageLayout a TextureLayout maps to (single-sources the kLayoutMappings table in Texture.cpp).
// Used by CopyImageToHostMemory callers to supply a readback source's parked layout (e.g. agent dump_render_target).
VkImageLayout ToVkImageLayout(TextureLayout eLayout);

struct TextureInfo
{
	TextureFlags_t textureFlags;
	std::string_view name;
	common::crc_t uiCrc = 0;

	// VkImageCreateInfo
	VkImageCreateFlags vkImageCreateFlags = 0;
	VkFormat vkFormat = VK_FORMAT_UNDEFINED;
	VkExtent3D vkExtent3D {};
	int64_t iMipLevels = 1;
	int64_t iArrayLayers = 1;
	VkSampleCountFlagBits vkSampleCountFlagBits = VK_SAMPLE_COUNT_1_BIT;
	VkImageUsageFlags vkImageUsageFlags = 0;

	// VkImageViewCreateInfo
	VkImageViewType vkImageViewType = VK_IMAGE_VIEW_TYPE_2D;
	VkImageAspectFlags vkImageAspectFlags = 0;

	// vkCreateRenderPass
	VkAttachmentLoadOp vkRenderPassAttachmentLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	VkImageLayout vkRenderPassInitialImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageLayout vkRenderPassFinalImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	// vkCmdBeginRenderPass
	VkClearColorValue vkRenderPassClearColorValue {};

	// TransitionImageLayout
	TextureLayout eTextureLayout = TextureLayout::kShaderReadOnly;

	// Subpass dependency masks (render targets only)
	VkPipelineStageFlags vkRenderPassDestinationStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	VkAccessFlags vkRenderPassDestinationAccessMask = VK_ACCESS_SHADER_READ_BIT;
};

class Texture
{
public:

	static void RecordBeginRenderPass(VkCommandBuffer vkCommandBuffer, VkRenderPass vkRenderPass, VkFramebuffer vkFramebuffer, VkExtent2D vkExtent2D, VkClearColorValue vkClearColorValue, RenderPassFlags_t renderPassFlags);

	Texture() = default;
	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;
	Texture(Texture&& rOther) noexcept
		: mInfo(std::move(rOther.mInfo))
		, mVmaAllocation(std::exchange(rOther.mVmaAllocation, VK_NULL_HANDLE))
		, mVkImage(std::exchange(rOther.mVkImage, VK_NULL_HANDLE))
		, mVkImageView(std::exchange(rOther.mVkImageView, VK_NULL_HANDLE))
		, miGeneration(rOther.miGeneration)
		, mVkRenderPass(std::exchange(rOther.mVkRenderPass, VK_NULL_HANDLE))
		, mVkFramebuffer(std::exchange(rOther.mVkFramebuffer, VK_NULL_HANDLE))
	{}
	Texture(const TextureInfo& rInfo, const std::function<void(std::span<std::byte>, int64_t)>& rDataFunction = nullptr);
	~Texture();

	void Create(const TextureInfo& rInfo, const std::function<void(std::span<std::byte>, int64_t)>& rDataFunction = nullptr);
	void InitDeferred(const TextureInfo& rInfo, VkImageView vkPlaceholderImageView);
	void AdoptTransferredImage(VkImage& rVkImage, VmaAllocation& rVmaAllocation);
	void RecordAcquireBarrier(VkCommandBuffer vkCommandBuffer);
	void UpdateData(const std::function<void(std::span<std::byte>, int64_t)>& rDataFunction);
	void Destroy() noexcept;

	void TransitionImageLayout(VkCommandBuffer vkCommandBuffer, TextureLayout eOldLayout, TextureLayout eNewLayout);
	void RecordCopyImageFrom(VkCommandBuffer vkCommandBuffer, const Texture& rSource); // Caller transitions source->kTransferSource, this->kTransferDestination first
	void RecordBeginRenderPass(VkCommandBuffer vkCommandBuffer);

	TextureInfo mInfo {};

private:

	void UploadImageData(const std::function<void(std::span<std::byte>, int64_t)>& rDataFunction, TextureLayout eOldLayout, TextureLayout eFinalLayout);
	void CreateRenderTarget();

public:

	VmaAllocation mVmaAllocation = VK_NULL_HANDLE;
	VkImage mVkImage = VK_NULL_HANDLE;
	VkImageView mVkImageView = VK_NULL_HANDLE;

	// Create and AdoptTransferredImage increment this generation; descriptor writes snapshot it.
	// TextureDescriptors::VerifyAllDescriptorGenerations detects stale resources Vulkan validation misses.
	int64_t miGeneration = 0;

	VkRenderPass mVkRenderPass = VK_NULL_HANDLE;
	VkFramebuffer mVkFramebuffer = VK_NULL_HANDLE;
};

} // namespace engine

#endif // defined(BT_CLIENT)
