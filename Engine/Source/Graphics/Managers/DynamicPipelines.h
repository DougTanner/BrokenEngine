#pragma once

#if defined(BT_CLIENT)

namespace engine
{

struct ModelPipelineSpec
{
	common::crc_t sceneCrc = 0;
	PipelineInfo pipelineInfo {};
	bool bIsPipelineShadow = false;
};

enum DynamicPipelineType
{
	kDynamicPipelineLighting,
	kDynamicPipelineAxisAlignedLighting,
	kDynamicPipelineVisibleLights,
	kDynamicPipelineBillboards,
	kDynamicPipelineSmokeAxisAligned,
	kDynamicPipelineSmoke,
	kDynamicPipelineWindDepositA,
	kDynamicPipelineWindDepositB,
	kDynamicPipelineWindDepositAxisAlignedA,
	kDynamicPipelineWindDepositAxisAlignedB,
	kDynamicPipelineHexShields,
	kDynamicPipelineHexShieldsLighting,

	kDynamicPipelineCount,
};

enum DynamicModelPipelineType
{
	kDynamicModelPipelineModel,
	kDynamicModelPipelineModelShadow,

	kDynamicModelPipelineCount,
};

class DynamicPipelines
{
public:

	explicit DynamicPipelines(std::unordered_map<common::crc_t, Shader>& rShaders);

	ModelPipeline* CreateModelPipeline(const ModelPipelineSpec& rModelPipelineSpec);
	void CreateModelPipeline(common::crc_t crc, std::string_view name, common::crc_t sceneCrc, Buffer* pStorageBuffers);
	void CreateModelPipelineShadow(common::crc_t crc, std::string_view name, common::crc_t sceneCrc, Buffer* pStorageBuffers);
	void CreatePipelineVisibleLights(common::crc_t crc, std::string_view name, Buffer* pStorageBuffers);
	void CreatePipelineBillboards(common::crc_t crc, std::string_view name, int64_t iBufferSize);
	void CreatePipelineHexShields(common::crc_t crc, std::string_view name, int64_t iBufferSize);
	void CreatePipelineHexShieldsLighting(common::crc_t crc, std::string_view name);
	void UpdateAllModelPipelineDescriptors(int64_t iCommandBuffer, int64_t iBinding, Buffer* pBuffer);

	std::vector<std::unique_ptr<Pipeline>> mPipelines;
	std::vector<std::unique_ptr<ModelPipeline>> mModelPipelines;
	std::unordered_map<common::crc_t, Pipeline*> mPipelineMaps[kDynamicPipelineCount];
	std::unordered_map<common::crc_t, ModelPipeline*> mModelPipelineMaps[kDynamicModelPipelineCount];
	std::unordered_map<common::crc_t, std::string> mShadowPipelineNames; // Owns shadow pipeline name strings

	void CreateAreaLightingPipeline(DynamicPipelineType eType, common::crc_t crc, std::string_view name, int64_t iBufferSize, common::crc_t vertexShaderCrc, common::crc_t fragmentShaderCrc, DescriptorFlags eSamplerFlag);
	void CreateDepositPipeline(DynamicPipelineType eType, common::crc_t crc, std::string_view name, common::crc_t vertexShaderCrc, common::crc_t fragmentShaderCrc, const Texture& rTargetTexture, const DescriptorInfo& rTextureDescriptor, VkBuffer* pVkOccupancyBuffer, int64_t iBufferSize);

private:

	void AddPipeline(DynamicPipelineType eType, common::crc_t crc, const PipelineInfo& rPipelineInfo);

	std::unordered_map<common::crc_t, Shader>& mrShaders;
};

} // namespace engine

#endif // defined(BT_CLIENT)
