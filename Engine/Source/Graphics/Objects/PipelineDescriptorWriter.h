#pragma once

#if defined(BT_CLIENT)

namespace engine
{

class Pipeline;
class Buffer;

struct PipelineDescriptorWriter
{
	// Returns true if iBinding has a non-zero descriptorCount in either shader's reflected layout.
	// Exposed so deferred-update callers (TextureDescriptors::Register*) can validate at register time.
	static bool BindingExistsInShaderLayout(const Pipeline& rPipeline, int64_t iBinding);

	static void Write(Pipeline& rPipeline);
	// Writes Set 1 copy iDescriptorSet only (the framebuffer index for a per-framebuffer pipeline).
	static void UpdateImageDescriptorInSet(const Pipeline& rPipeline, int64_t iDescriptorSet, int64_t iBinding, VkSampler vkSampler, VkImageView vkImageView, VkImageLayout vkImageLayout, VkDescriptorType vkDescriptorType);
	static void UpdateStorageBuffer(const Pipeline& rPipeline, int64_t iFramebuffer, int64_t iBinding, const Buffer* pBuffer);
};

} // namespace engine

#endif // defined(BT_CLIENT)
