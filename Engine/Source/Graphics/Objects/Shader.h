#pragma once

#if defined(BT_CLIENT)

namespace engine
{

struct ShaderInfo
{
	common::ChunkHeader* pChunkHeader = nullptr;
	const VkDescriptorSetLayoutBinding* pVkDescriptorBindings = nullptr;
	const uint32_t* puiDescriptorSetIndices = nullptr;
	const VkVertexInputAttributeDescription* pVkVertexAttributes = nullptr;
	int64_t iSpirvSize = 0;
};

class Shader
{
public:

	Shader() = default;
	Shader(const Shader&) = delete;
	Shader& operator=(const Shader&) = delete;
	Shader(const ShaderInfo& rInformation, const std::byte* pData);
	~Shader();

	void Create(const ShaderInfo& rInformation, const std::byte* pData);
	void Destroy() noexcept;

	ShaderInfo mInformation {};

	VkShaderModule mVkShaderModule = VK_NULL_HANDLE;
};

} // namespace engine

#endif // defined(BT_CLIENT)
