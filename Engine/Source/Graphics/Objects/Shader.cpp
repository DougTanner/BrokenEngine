#if defined(BT_CLIENT)

#include "Shader.h"

namespace engine
{

Shader::Shader(const ShaderInfo& rInformation, const std::byte* pData)
{
	Create(rInformation, pData);
}

Shader::~Shader()
{
	Destroy();
}

void Shader::Create(const ShaderInfo& rInformation, const std::byte* pData)
{
	Destroy();

	mInformation = rInformation;

	VkShaderModuleCreateInfo vkShaderModuleCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.codeSize = static_cast<size_t>(mInformation.iSpirvSize),
		.pCode = reinterpret_cast<const uint32_t*>(pData),
	};

	ASSERT(*reinterpret_cast<const uint32_t*>(pData) == common::ShaderHeader::kiSpirvMagic);

	CHECK_VK(vkCreateShaderModule(gpDeviceManager->mVkDevice, &vkShaderModuleCreateInfo, nullptr, &mVkShaderModule));
	VkName(VK_OBJECT_TYPE_SHADER_MODULE, mVkShaderModule, mInformation.pChunkHeader->pcPath);
}

void Shader::Destroy() noexcept
{
	if (mVkShaderModule != VK_NULL_HANDLE)
	{
		vkDestroyShaderModule(gpDeviceManager->mVkDevice, mVkShaderModule, nullptr);
		mVkShaderModule = VK_NULL_HANDLE;
	}
}

} // namespace engine

#endif // defined(BT_CLIENT)
